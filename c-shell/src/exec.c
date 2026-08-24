#include "exec.h"
#include "hop.h"
#include "peek.h"
#include "reveal.h"
#include "locate.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

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

static void execute_single_command(token_list_t *list, int start, int end, shell_state_t *state, int is_forked) {
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
            if (is_forked) exit(1);
            return;
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
                    exit(0);
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
        if (is_forked) exit(1);
        return;
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
                exit(0);
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

    if (arg_count > 0) {
        char *cmd_name = args[0];
        int skip_cwd = 0;
        if (cmd_name[0] == '%') {
            skip_cwd = 1;
            cmd_name++;
            args[0] = cmd_name;
        }
        
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
        }
        
        if (is_builtin) {
            if (!is_forked) {
                if (orig_stdin != -1) {
                    dup2(orig_stdin, STDIN_FILENO);
                    close(orig_stdin);
                }
                if (orig_stdout != -1) {
                    dup2(orig_stdout, STDOUT_FILENO);
                    close(orig_stdout);
                }
                if (feeder_pid != -1) {
                    int st;
                    waitpid(feeder_pid, &st, 0);
                }
                if (writer_pid != -1) {
                    int st;
                    waitpid(writer_pid, &st, 0);
                }
            } else {
                exit(0);
            }
        } else {
            char resolved_path[PATH_MAX];
            int found = 0;
            
            if (strchr(cmd_name, '/') != NULL) {
                struct stat st;
                if (access(cmd_name, X_OK) == 0 && stat(cmd_name, &st) == 0 && S_ISREG(st.st_mode)) {
                    strncpy(resolved_path, cmd_name, sizeof(resolved_path));
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
                if (is_forked) exit(1);
            } else {
                if (is_forked) {
                    execv(resolved_path, args);
                    perror("execv");
                    exit(1);
                } else {
                    pid_t ext_pid = fork();
                    if (ext_pid == 0) {
                        execv(resolved_path, args);
                        perror("execv");
                        exit(1);
                    } else {
                        int status;
                        waitpid(ext_pid, &status, 0);
                    }
                }
            }
            
            if (!is_forked) {
                if (orig_stdin != -1) {
                    dup2(orig_stdin, STDIN_FILENO);
                    close(orig_stdin);
                }
                if (orig_stdout != -1) {
                    dup2(orig_stdout, STDOUT_FILENO);
                    close(orig_stdout);
                }
                if (feeder_pid != -1) {
                    int st;
                    waitpid(feeder_pid, &st, 0);
                }
                if (writer_pid != -1) {
                    int st;
                    waitpid(writer_pid, &st, 0);
                }
            }
        }
    } else {
        if (!is_forked) {
            if (orig_stdin != -1) { dup2(orig_stdin, STDIN_FILENO); close(orig_stdin); }
            if (orig_stdout != -1) { dup2(orig_stdout, STDOUT_FILENO); close(orig_stdout); }
            if (feeder_pid != -1) { int st; waitpid(feeder_pid, &st, 0); }
            if (writer_pid != -1) { int st; waitpid(writer_pid, &st, 0); }
        } else {
            exit(0);
        }
    }
}

void execute_line(token_list_t *list, shell_state_t *state)
{
    if (list->count == 0) return;

    int start = 0;
    while (start < list->count) {
        int end = start;
        while (end < list->count && list->toks[end].type != TOK_SEMI && list->toks[end].type != TOK_AMP) {
            end++;
        }
        
        int num_cmds = 1;
        for (int i = start; i < end; i++) {
            if (list->toks[i].type == TOK_PIPE) {
                num_cmds++;
            }
        }
        
        if (num_cmds == 1) {
            execute_single_command(list, start, end, state, 0);
        } else {
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
            
            int pipes[256][2];
            pid_t pids[256];
            
            for (int i = 0; i < num_cmds - 1; i++) {
                if (pipe(pipes[i]) < 0) {
                    perror("pipe");
                }
            }
            
            for (int i = 0; i < num_cmds; i++) {
                pids[i] = fork();
                if (pids[i] == 0) {
                    if (i > 0) {
                        dup2(pipes[i-1][0], STDIN_FILENO);
                    }
                    if (i < num_cmds - 1) {
                        dup2(pipes[i][1], STDOUT_FILENO);
                    }
                    for (int j = 0; j < num_cmds - 1; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                    execute_single_command(list, cmd_starts[i], cmd_ends[i], state, 1);
                    exit(0);
                }
            }
            
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            for (int i = 0; i < num_cmds; i++) {
                int status;
                waitpid(pids[i], &status, 0);
            }
        }
        
        break;
    }
}
