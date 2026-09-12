#include "exec.h"
#include "hop.h"
#include "peek.h"
#include "reveal.h"
#include "locate.h"
#include "spy.h"
#include "snoop.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>

#define PROCESS_RUNNING 0
#define PROCESS_STOPPED 1
#define PROCESS_EXITED  2

typedef struct {
    pid_t pid;
    char name[256];
    int state;
    int exit_status;
    int notified;
} process_t;

typedef struct {
    int job_id;
    pid_t pgid;
    char cmd_line[1024];
    process_t procs[256];
    int num_procs;
    int active;
    int is_bg;
} job_t;

#define MAX_JOBS 1024
static job_t jobs[MAX_JOBS];
static int next_job_id = 1;
static volatile sig_atomic_t fg_running = 0;
static pid_t shell_pgid;
static int shell_terminal;
static int shell_is_interactive;

static pid_t timeout_pgid = 0;
static volatile sig_atomic_t job_timed_out = 0;

void sigalrm_handler(int sig) {
    (void)sig;
    if (timeout_pgid) {
        kill(-timeout_pgid, SIGTERM);
        job_timed_out = 1;
    }
}

static job_t *get_job(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].job_id == job_id) return &jobs[i];
    }
    return NULL;
}

static int job_is_stopped(job_t *job) {
    if (job->num_procs == 0) return 0;
    for (int i = 0; i < job->num_procs; i++) {
        if (job->procs[i].state == PROCESS_RUNNING) return 0;
        /* If any process has exited, this is not "stopped" — it's partially completed */
    }
    /* All procs are either STOPPED or EXITED. Check if at least one is STOPPED. */
    for (int i = 0; i < job->num_procs; i++) {
        if (job->procs[i].state == PROCESS_STOPPED) return 1;
    }
    return 0;
}

static int job_is_completed(job_t *job) {
    if (job->num_procs == 0) return 0;
    for (int i = 0; i < job->num_procs; i++) {
        if (job->procs[i].state != PROCESS_EXITED) return 0;
    }
    return 1;
}

static void update_process_status(pid_t pid, int status) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) continue;
        for (int j = 0; j < jobs[i].num_procs; j++) {
            if (jobs[i].procs[j].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    jobs[i].procs[j].state = PROCESS_EXITED;
                    jobs[i].procs[j].exit_status = status;
                } else if (WIFSTOPPED(status)) {
                    jobs[i].procs[j].state = PROCESS_STOPPED;
                } else if (WIFCONTINUED(status)) {
                    jobs[i].procs[j].state = PROCESS_RUNNING;
                }
                return;
            }
        }
    }
}

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        update_process_status(pid, status);
    }
    errno = saved_errno;
}

void init_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) jobs[i].active = 0;

    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);

        shell_pgid = getpid();
        setpgid(shell_pgid, shell_pgid);
        tcsetpgrp(shell_terminal, shell_pgid);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    struct sigaction sa_alrm;
    sa_alrm.sa_handler = sigalrm_handler;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alrm, NULL);
}

void cleanup_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) continue;
        if (job_is_completed(&jobs[i])) {
            /* Print exit notifications for bg job processes that haven't been reported yet */
            if (jobs[i].is_bg) {
                for (int j = 0; j < jobs[i].num_procs; j++) {
                    if (!jobs[i].procs[j].notified) {
                        int status = jobs[i].procs[j].exit_status;
                        int normal = WIFEXITED(status) && WEXITSTATUS(status) == 0;
                        printf("%s with pid %d exited %s\n",
                               jobs[i].procs[j].name, jobs[i].procs[j].pid,
                               normal ? "normally" : "abnormally");
                        jobs[i].procs[j].notified = 1;
                    }
                }
            }
            jobs[i].active = 0;
        }
    }
    fflush(stdout);
}

int check_stopped_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && job_is_stopped(&jobs[i])) return 1;
    }
    return 0;
}

void kill_all_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            kill(-jobs[i].pgid, SIGHUP);
        }
    }
}

static int add_job(int is_bg, const char *cmd_line) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            jobs[i].job_id = next_job_id++;
            jobs[i].pgid = 0;
            strncpy(jobs[i].cmd_line, cmd_line, sizeof(jobs[i].cmd_line) - 1);
            jobs[i].cmd_line[sizeof(jobs[i].cmd_line) - 1] = '\0';
            jobs[i].num_procs = 0;
            jobs[i].active = 1;
            jobs[i].is_bg = is_bg;
            return i;
        }
    }
    return -1;
}

static void add_process_to_job(int job_idx, pid_t pid, const char *name) {
    if (job_idx < 0 || job_idx >= MAX_JOBS || !jobs[job_idx].active) return;
    int pidx = jobs[job_idx].num_procs++;
    jobs[job_idx].procs[pidx].pid = pid;
    strncpy(jobs[job_idx].procs[pidx].name, name, sizeof(jobs[job_idx].procs[pidx].name) - 1);
    jobs[job_idx].procs[pidx].name[sizeof(jobs[job_idx].procs[pidx].name) - 1] = '\0';
    jobs[job_idx].procs[pidx].state = PROCESS_RUNNING;
    jobs[job_idx].procs[pidx].exit_status = 0;
    jobs[job_idx].procs[pidx].notified = 0;
}

/* Returns 0 if the job exited successfully, nonzero otherwise */
static int wait_for_job(int job_id) {
    job_t *job = get_job(job_id);
    if (!job) return -1;

    sigset_t empty;
    sigemptyset(&empty);
    while (!job_is_stopped(job) && !job_is_completed(job)) {
        sigsuspend(&empty);
        if (job_timed_out) break;
    }

    if (shell_is_interactive) {
        tcsetpgrp(shell_terminal, shell_pgid);
    }

    if (job_is_stopped(job)) {
        printf("[%d] + Stopped    %s\n", job->job_id, job->cmd_line);
        job->is_bg = 1; /* keep tracking it */
        return -1;
    } else if (job_is_completed(job)) {
        /* Check if the last process exited with failure */
        int last_status = job->procs[job->num_procs - 1].exit_status;
        int failed = 0;
        if (WIFEXITED(last_status) && WEXITSTATUS(last_status) != 0) failed = 1;
        if (WIFSIGNALED(last_status)) failed = 1;
        job->active = 0;
        return failed;
    }
    return 0;
}

static int join_path(char *destination, size_t destination_size, const char *directory, const char *name) {
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);

    if (directory_length > destination_size - 2 || name_length > destination_size - directory_length - 2) {
        return 0;
    }

    memcpy(destination, directory, directory_length);
    destination[directory_length] = '/';
    memcpy(destination + directory_length + 1, name, name_length + 1);
    return 1;
}

static void execute_activities(void) {
    /* Reap any pending children first so the list is current */
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        update_process_status(pid, status);
    }

    for (int id = 1; id < next_job_id; id++) {
        job_t *job = get_job(id);
        if (!job || !job->active) continue;
        /* Remove completed jobs silently */
        if (job_is_completed(job)) {
            job->active = 0;
            continue;
        }
        printf("[%d] pgid %d\n", job->job_id, job->pgid);
        for (int j = 0; j < job->num_procs; j++) {
            if (job->procs[j].state != PROCESS_EXITED) {
                printf("  %d %s %s\n", job->procs[j].pid, job->procs[j].name,
                       job->procs[j].state == PROCESS_STOPPED ? "Stopped" : "Running");
            }
        }
    }
}

static void execute_resume(char **args, int arg_count) {
    if (arg_count < 2) { printf("resume: invalid syntax\n"); return; }
    if (args[0][0] != '%') { printf("resume: invalid syntax\n"); return; }

    char *endptr;
    long jid = strtol(args[0] + 1, &endptr, 10);
    if (*endptr != '\0') { printf("resume: invalid syntax\n"); return; }

    job_t *job = get_job((int)jid);
    if (!job) { printf("resume: no such job\n"); return; }

    int is_fg = 0;
    int timeout = 0;
    if (strcmp(args[1], "fg") == 0) {
        is_fg = 1;
        if (arg_count == 4 && strcmp(args[2], "--timeout") == 0) {
            char *tend;
            long tv = strtol(args[3], &tend, 10);
            if (*tend != '\0' || tv <= 0) { printf("resume: invalid syntax\n"); return; }
            timeout = (int)tv;
        } else if (arg_count != 2) {
            printf("resume: invalid syntax\n"); return;
        }
    } else if (strcmp(args[1], "bg") == 0) {
        is_fg = 0;
        if (arg_count != 2) { printf("resume: invalid syntax\n"); return; }
    } else {
        printf("resume: invalid syntax\n"); return;
    }

    /* Send SIGCONT if stopped */
    if (job_is_stopped(job)) {
        kill(-job->pgid, SIGCONT);
        for (int i = 0; i < job->num_procs; i++) {
            if (job->procs[i].state == PROCESS_STOPPED)
                job->procs[i].state = PROCESS_RUNNING;
        }
    }

    if (is_fg) {
        printf("%s\n", job->cmd_line);
        job->is_bg = 0;
        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, job->pgid);
        }

        if (timeout > 0) {
            job_timed_out = 0;
            timeout_pgid = job->pgid;
            alarm(timeout);

            fg_running = 1;
            wait_for_job((int)jid);
            fg_running = 0;

            alarm(0);
            timeout_pgid = 0;
            if (job_timed_out) {
                printf("resume: job timed out\n");
                /* The job was SIGTERM'd; reclaim terminal */
                if (shell_is_interactive) {
                    tcsetpgrp(shell_terminal, shell_pgid);
                }
            }
        } else {
            fg_running = 1;
            wait_for_job((int)jid);
            fg_running = 0;
        }
    } else {
        job->is_bg = 1;
        printf("[%d] + Running    %s\n", job->job_id, job->cmd_line);
    }
}

static void execute_ping(char **args, int arg_count) {
    if (arg_count != 2) { printf("ping: invalid syntax\n"); return; }

    /* Validate signal_number first (requirement 3: validated before target is looked up) */
    char *endptr;
    long sig = strtol(args[1], &endptr, 10);
    if (*endptr != '\0' || sig < 0) {
        printf("ping: invalid syntax\n");
        return;
    }

    int real_sig = (int)(sig % 64);

    if (args[0][0] == '%') {
        /* Job target */
        char *jendptr;
        long jid = strtol(args[0] + 1, &jendptr, 10);
        if (*jendptr != '\0') {
            printf("ping: no such process found\n");
            return;
        }
        job_t *job = get_job((int)jid);
        if (!job) {
            printf("ping: no such process found\n");
            return;
        }
        if (kill(-job->pgid, real_sig) == 0) {
            printf("Sent signal %ld to %s\n", sig, args[0]);
        } else {
            printf("ping: no such process found\n");
        }
    } else {
        /* PID target */
        char *pendptr;
        long target_l = strtol(args[0], &pendptr, 10);
        if (*pendptr != '\0' || target_l <= 0) {
            printf("ping: no such process found\n");
            return;
        }
        pid_t target_pid = (pid_t)target_l;

        /* Check if this pid is tracked by our shell */
        int tracked = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (!jobs[i].active) continue;
            for (int j = 0; j < jobs[i].num_procs; j++) {
                if (jobs[i].procs[j].pid == target_pid && jobs[i].procs[j].state != PROCESS_EXITED) {
                    tracked = 1; break;
                }
            }
            if (tracked) break;
        }
        if (!tracked) {
            printf("ping: no such process found\n");
            return;
        }
        if (kill(target_pid, real_sig) == 0) {
            printf("Sent signal %ld to %s\n", sig, args[0]);
        } else {
            printf("ping: no such process found\n");
        }
    }
}

/* ---- execute_single_command ----
 * Used ONLY when is_forked=1 (inside a child process about to execv)
 * OR for builtins that run in the parent (is_forked=0).
 * For is_forked=0, this NEVER runs external commands — the caller handles that. */
static int execute_single_command(token_list_t *list, int start, int end, shell_state_t *state, int is_forked) {
    char *args[256];
    int arg_count = 0;
    char *input_files[256];
    int input_file_count = 0;
    char *output_files[256];
    int output_modes[256];
    int output_file_count = 0;

    for (int i = start; i < end; i++) {
        if (list->toks[i].type == TOK_LT) {
            i++;
            if (i < end && list->toks[i].type == TOK_WORD) {
                input_files[input_file_count++] = list->toks[i].value;
            }
        } else if (list->toks[i].type == TOK_GT || list->toks[i].type == TOK_GTGT) {
            int mode = list->toks[i].type;
            i++;
            if (i < end && list->toks[i].type == TOK_WORD) {
                output_files[output_file_count] = list->toks[i].value;
                output_modes[output_file_count++] = mode;
            }
        } else if (list->toks[i].type == TOK_WORD) {
            args[arg_count++] = list->toks[i].value;
        }
    }
    args[arg_count] = NULL;

    /* I/O redirection setup */
    int in_fd = -1;
    int pfd[2] = {-1, -1};
    pid_t feeder_pid = -1;

    if (input_file_count > 0) {
        int all_exist = 1;
        for (int k = 0; k < input_file_count; k++) {
            int fd = open(input_files[k], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "cshell: no such file or directory\n");
                all_exist = 0;
                break;
            }
            close(fd);
        }
        if (!all_exist) {
            if (is_forked) _exit(1);
            return 1;
        }

        if (input_file_count == 1) {
            in_fd = open(input_files[0], O_RDONLY);
        } else {
            if (pipe(pfd) == 0) {
                in_fd = pfd[0];
                feeder_pid = fork();
                if (feeder_pid == 0) {
                    close(pfd[0]);
                    for (int k = 0; k < input_file_count; k++) {
                        int fd = open(input_files[k], O_RDONLY);
                        if (fd >= 0) {
                            char buf[4096];
                            ssize_t n;
                            while ((n = read(fd, buf, sizeof(buf))) > 0) {
                                write(pfd[1], buf, n);
                            }
                            close(fd);
                        }
                    }
                    close(pfd[1]);
                    _exit(0);
                }
                close(pfd[1]);
            }
        }
    }

    int orig_stdin = -1;
    int orig_stdout = -1;
    int out_fds[256];
    int out_fd = -1;
    int out_pfd[2] = {-1, -1};
    pid_t writer_pid = -1;

    int output_all_good = 1;
    for (int k = 0; k < output_file_count; k++) {
        int flags = O_WRONLY | O_CREAT;
        if (output_modes[k] == TOK_GT) {
            flags |= O_TRUNC;
        } else {
            flags |= O_APPEND;
        }
        out_fds[k] = open(output_files[k], flags, 0644);
        if (out_fds[k] < 0) {
            for (int j = 0; j < k; j++) close(out_fds[j]);
            fprintf(stderr, "cshell: unable to create file for writing\n");
            output_all_good = 0;
            break;
        }
    }

    if (!output_all_good) {
        if (in_fd != -1) close(in_fd);
        if (feeder_pid != -1) { int st; waitpid(feeder_pid, &st, 0); }
        if (is_forked) _exit(1);
        return 1;
    }

    if (output_file_count == 1) {
        out_fd = out_fds[0];
    } else if (output_file_count > 1) {
        if (pipe(out_pfd) == 0) {
            out_fd = out_pfd[1];
            writer_pid = fork();
            if (writer_pid == 0) {
                if (in_fd != -1) close(in_fd);
                close(out_pfd[1]);
                char buf[4096];
                ssize_t n;
                while ((n = read(out_pfd[0], buf, sizeof(buf))) > 0) {
                    for (int k = 0; k < output_file_count; k++) {
                        write(out_fds[k], buf, n);
                    }
                }
                for (int k = 0; k < output_file_count; k++) close(out_fds[k]);
                close(out_pfd[0]);
                _exit(0);
            }
            close(out_pfd[0]);
            for (int k = 0; k < output_file_count; k++) close(out_fds[k]);
        }
    }

    if (in_fd != -1) {
        orig_stdin = dup(STDIN_FILENO);
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }
    if (out_fd != -1) {
        orig_stdout = dup(STDOUT_FILENO);
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
    }

    #define RESTORE_FDS() do { \
        if (orig_stdin != -1) { dup2(orig_stdin, STDIN_FILENO); close(orig_stdin); } \
        if (orig_stdout != -1) { dup2(orig_stdout, STDOUT_FILENO); close(orig_stdout); } \
        if (feeder_pid != -1) { int _st; waitpid(feeder_pid, &_st, 0); } \
        if (writer_pid != -1) { int _st; waitpid(writer_pid, &_st, 0); } \
    } while (0)

    if (arg_count == 0) {
        if (is_forked) _exit(0);
        RESTORE_FDS();
        return 1;
    }

    char *cmd_name = args[0];
    int skip_cwd = 0;
    if (cmd_name[0] == '%') {
        skip_cwd = 1;
        cmd_name++;
        args[0] = cmd_name;
    }

    /* Builtins */
    int is_builtin = 0;
    if (!skip_cwd && strcmp(cmd_name, "hop") == 0) {
        execute_hop(args + 1, arg_count - 1, state);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "reveal") == 0) {
        execute_reveal(args + 1, arg_count - 1, state);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "peek") == 0) {
        execute_peek(args + 1, arg_count - 1, state);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "locate") == 0) {
        execute_locate(args + 1, arg_count - 1, state);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "activities") == 0) {
        execute_activities();
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "resume") == 0) {
        execute_resume(args + 1, arg_count - 1);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "ping") == 0) {
        execute_ping(args + 1, arg_count - 1);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "spy") == 0) {
        execute_spy(args + 1, arg_count - 1);
        is_builtin = 1;
    } else if (!skip_cwd && strcmp(cmd_name, "snoop") == 0) {
        execute_snoop(args + 1, arg_count - 1);
        is_builtin = 1;
    }

    if (is_builtin) {
        if (is_forked) _exit(0);
        RESTORE_FDS();
        return 1;
    }

    /* Resolve external command */
    char resolved_path[PATH_MAX];
    int found = 0;

    if (strchr(cmd_name, '/') != NULL) {
        struct stat st;
        if (access(cmd_name, X_OK) == 0 && stat(cmd_name, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(resolved_path, cmd_name, sizeof(resolved_path));
            resolved_path[sizeof(resolved_path)-1] = '\0';
            found = 1;
        }
    } else {
        if (!skip_cwd) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                struct stat st;
                if (join_path(resolved_path, sizeof(resolved_path), cwd, cmd_name) &&
                    access(resolved_path, X_OK) == 0 && stat(resolved_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    found = 1;
                }
            }
        }

        if (!found) {
            const char *path_env = getenv("PATH");
            if (path_env) {
                char *path_copy = strdup(path_env);
                if (path_copy) {
                    char *dir = strtok(path_copy, ":");
                    while (dir != NULL) {
                        struct stat st;
                        if (join_path(resolved_path, sizeof(resolved_path), dir, cmd_name) &&
                            access(resolved_path, X_OK) == 0 && stat(resolved_path, &st) == 0 && S_ISREG(st.st_mode)) {
                            found = 1;
                            break;
                        }
                        dir = strtok(NULL, ":");
                    }
                    free(path_copy);
                }
            }
        }
    }

    if (!found) {
        fprintf(stderr, "cshell: command not found (%s)\n", cmd_name);
        if (is_forked) _exit(1);
        RESTORE_FDS();
        return 0;
    }

    if (is_forked) {
        /* In child: just exec */
        execv(resolved_path, args);
        perror("execv");
        _exit(1);
    }

    /* Should never reach here — non-forked external commands are handled in execute_line */
    RESTORE_FDS();
    return 1;
    #undef RESTORE_FDS
}

/* Helper: check if a command name refers to a builtin */
static int is_builtin_name(const char *name) {
    if (name[0] == '%') name++;
    return (strcmp(name, "hop") == 0 || strcmp(name, "reveal") == 0 ||
            strcmp(name, "peek") == 0 || strcmp(name, "locate") == 0 ||
            strcmp(name, "activities") == 0 || strcmp(name, "resume") == 0 ||
            strcmp(name, "ping") == 0 || strcmp(name, "spy") == 0 ||
            strcmp(name, "snoop") == 0);
}

/* Helper: extract first command name from a token range */
static const char *get_cmd_name(token_list_t *list, int start, int end) {
    for (int j = start; j < end; j++) {
        if (list->toks[j].type == TOK_WORD) {
            return list->toks[j].value;
        } else if (list->toks[j].type == TOK_LT || list->toks[j].type == TOK_GT || list->toks[j].type == TOK_GTGT) {
            j++; /* skip filename */
        }
    }
    return NULL;
}

void execute_line(token_list_t *list, shell_state_t *state)
{
    if (list->count == 0) return;

    int start = 0;
    while (start < list->count) {
        /* Find end of this command group (up to ; or &) */
        int end = start;
        while (end < list->count && list->toks[end].type != TOK_SEMI && list->toks[end].type != TOK_AMP) {
            end++;
        }

        int is_bg = 0;
        if (end < list->count && list->toks[end].type == TOK_AMP) {
            is_bg = 1;
        }

        /* Build the full command line string for job display */
        char full_cmd_line[1024] = "";
        for (int i = start; i < end; i++) {
            if (i > start) strncat(full_cmd_line, " ", sizeof(full_cmd_line) - strlen(full_cmd_line) - 1);
            if (list->toks[i].type == TOK_LT) strncat(full_cmd_line, "<", sizeof(full_cmd_line) - strlen(full_cmd_line) - 1);
            else if (list->toks[i].type == TOK_GT) strncat(full_cmd_line, ">", sizeof(full_cmd_line) - strlen(full_cmd_line) - 1);
            else if (list->toks[i].type == TOK_GTGT) strncat(full_cmd_line, ">>", sizeof(full_cmd_line) - strlen(full_cmd_line) - 1);
            else if (list->toks[i].type == TOK_PIPE) strncat(full_cmd_line, "|", sizeof(full_cmd_line) - strlen(full_cmd_line) - 1);
            else strncat(full_cmd_line, list->toks[i].value, sizeof(full_cmd_line) - strlen(full_cmd_line) - 1);
        }

        /* Count pipeline stages */
        int num_cmds = 1;
        for (int i = start; i < end; i++) {
            if (list->toks[i].type == TOK_PIPE) num_cmds++;
        }

        /* Split into individual commands at pipe boundaries */
        int cmd_starts[256];
        int cmd_ends[256];
        int c_idx = 0;
        cmd_starts[0] = start;
        for (int i = start; i < end; i++) {
            if (list->toks[i].type == TOK_PIPE) {
                cmd_ends[c_idx] = i;
                c_idx++;
                cmd_starts[c_idx] = i + 1;
            }
        }
        cmd_ends[c_idx] = end;

        /* Special case: single builtin command, not background → run in parent */
        if (num_cmds == 1 && !is_bg) {
            const char *cname = get_cmd_name(list, start, end);
            if (cname && is_builtin_name(cname)) {
                int success = execute_single_command(list, start, end, state, 0);
                if (!success) break; /* command not found → stop sequence */
                start = end + 1;
                continue;
            }
        }

        /* Create a job for this command group */
        int job_idx = add_job(is_bg, full_cmd_line);
        if (job_idx < 0) {
            fprintf(stderr, "cshell: too many background jobs\n");
            break;
        }

        /* Block SIGCHLD during fork setup */
        sigset_t mask, oldmask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &oldmask);

        /* Set up pipes */
        int pipes_arr[256][2];
        for (int i = 0; i < num_cmds - 1; i++) {
            if (pipe(pipes_arr[i]) < 0) {
                perror("pipe");
            }
        }

        /* Fork all pipeline stages */
        pid_t pids[256];
        for (int i = 0; i < num_cmds; i++) {
            pids[i] = fork();
            if (pids[i] == 0) {
                /* ---- CHILD ---- */
                sigprocmask(SIG_SETMASK, &oldmask, NULL);

                /* Restore default signal handling for job control signals */
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);

                /* Set up process group */
                pid_t mypid = getpid();
                if (i == 0) {
                    setpgid(mypid, mypid);
                } else {
                    setpgid(mypid, pids[0]);
                }

                if (!is_bg && shell_is_interactive) {
                    tcsetpgrp(shell_terminal, (i == 0) ? mypid : pids[0]);
                }

                /* Background jobs: redirect stdin from /dev/null for first command */
                if (is_bg && i == 0) {
                    int fd = open("/dev/null", O_RDONLY);
                    if (fd >= 0) {
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                }

                /* Pipe plumbing */
                if (i > 0) {
                    dup2(pipes_arr[i-1][0], STDIN_FILENO);
                }
                if (i < num_cmds - 1) {
                    dup2(pipes_arr[i][1], STDOUT_FILENO);
                }
                for (int j = 0; j < num_cmds - 1; j++) {
                    close(pipes_arr[j][0]);
                    close(pipes_arr[j][1]);
                }

                execute_single_command(list, cmd_starts[i], cmd_ends[i], state, 1);
                _exit(1);
            } else {
                /* ---- PARENT ---- */
                if (i == 0) {
                    jobs[job_idx].pgid = pids[0];
                    setpgid(pids[0], pids[0]);
                } else {
                    setpgid(pids[i], pids[0]);
                }

                /* Track process in job */
                const char *cname = get_cmd_name(list, cmd_starts[i], cmd_ends[i]);
                const char *display_name = cname ? cname : "unknown";
                if (display_name[0] == '%') display_name++;
                add_process_to_job(job_idx, pids[i], display_name);
            }
        }

        /* Close parent's pipe ends */
        for (int j = 0; j < num_cmds - 1; j++) {
            close(pipes_arr[j][0]);
            close(pipes_arr[j][1]);
        }

        if (is_bg) {
            /* Print job info and continue */
            printf("[%d] %d\n", jobs[job_idx].job_id, jobs[job_idx].pgid);
            fflush(stdout);
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
        } else {
            /* Give terminal to the foreground job */
            if (shell_is_interactive) {
                tcsetpgrp(shell_terminal, jobs[job_idx].pgid);
            }
            sigprocmask(SIG_SETMASK, &oldmask, NULL);

            fg_running = 1;
            int job_failed = wait_for_job(jobs[job_idx].job_id);
            fg_running = 0;
            if (job_failed) break; /* command failed → stop sequence */
        }

        start = end + 1;
    }
}
