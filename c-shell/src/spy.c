#include "spy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

static const char *get_file_type(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) return NULL;

    if (S_ISREG(st.st_mode)) return "REG";
    if (S_ISDIR(st.st_mode)) return "DIR";
    if (S_ISCHR(st.st_mode)) return "CHR";
    if (S_ISBLK(st.st_mode)) return "BLK";
    if (S_ISFIFO(st.st_mode)) return "FIFO";
    if (S_ISSOCK(st.st_mode)) return "SOCK";
    return "UNKNOWN";
}

void execute_spy(char **args, int arg_count) {
    if (arg_count > 1) {
        printf("spy: invalid syntax\n");
        return;
    }

    pid_t target_pid;
    if (arg_count == 0) {
        target_pid = getpid();
    } else {
        target_pid = atoi(args[0]);
    }

    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d", target_pid);
    struct stat st;
    if (stat(proc_path, &st) == -1) {
        printf("spy: no such process\n");
        return;
    }

    printf("%-5s %-8s %-8s %s\n", "PID", "FD", "TYPE", "PATH");

    char link_path[512];
    char target[PATH_MAX];
    ssize_t len;

    // cwd
    snprintf(link_path, sizeof(link_path), "/proc/%d/cwd", target_pid);
    if ((len = readlink(link_path, target, sizeof(target) - 1)) != -1) {
        target[len] = '\0';
        const char *type = get_file_type(target);
        if (type) printf("%-5d %-8s %-8s %s\n", target_pid, "cwd", type, target);
    }

    // txt (executable)
    snprintf(link_path, sizeof(link_path), "/proc/%d/exe", target_pid);
    if ((len = readlink(link_path, target, sizeof(target) - 1)) != -1) {
        target[len] = '\0';
        const char *type = get_file_type(target);
        if (type) printf("%-5d %-8s %-8s %s\n", target_pid, "txt", type, target);
    }

    // mem (memory-mapped files, each unique path printed only once)
    snprintf(link_path, sizeof(link_path), "/proc/%d/maps", target_pid);
    FILE *maps = fopen(link_path, "r");
    if (maps) {
        char line[1024];
        char *seen_paths[4096];
        int seen_count = 0;
        while (fgets(line, sizeof(line), maps)) {
            char map_path[PATH_MAX];
            if (sscanf(line, "%*s %*s %*s %*s %*s %s", map_path) == 1) {
                if (map_path[0] == '/') {
                    int already_seen = 0;
                    for (int i = 0; i < seen_count; i++) {
                        if (strcmp(seen_paths[i], map_path) == 0) {
                            already_seen = 1;
                            break;
                        }
                    }
                    if (!already_seen) {
                        const char *type = get_file_type(map_path);
                        if (type) printf("%-5d %-8s %-8s %s\n", target_pid, "mem", type, map_path);
                        if (seen_count < 4096) {
                            seen_paths[seen_count++] = strdup(map_path);
                        }
                    }
                }
            }
        }
        for (int i = 0; i < seen_count; i++) {
            free(seen_paths[i]);
        }
        fclose(maps);
    }

    // numeric file descriptors (sorted)
    snprintf(link_path, sizeof(link_path), "/proc/%d/fd", target_pid);
    DIR *dir = opendir(link_path);
    if (dir) {
        int fd_nums[4096];
        int fd_count = 0;
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            fd_nums[fd_count++] = atoi(ent->d_name);
        }
        closedir(dir);

        // sort numerically
        for (int i = 0; i < fd_count - 1; i++) {
            for (int j = i + 1; j < fd_count; j++) {
                if (fd_nums[i] > fd_nums[j]) {
                    int tmp = fd_nums[i];
                    fd_nums[i] = fd_nums[j];
                    fd_nums[j] = tmp;
                }
            }
        }

        for (int i = 0; i < fd_count; i++) {
            char fd_str[16];
            snprintf(fd_str, sizeof(fd_str), "%d", fd_nums[i]);
            char fd_link[512];
            snprintf(fd_link, sizeof(fd_link), "/proc/%d/fd/%s", target_pid, fd_str);
            if ((len = readlink(fd_link, target, sizeof(target) - 1)) != -1) {
                target[len] = '\0';
                const char *type = get_file_type(target);
                if (type) {
                    printf("%-5d %-8s %-8s %s\n", target_pid, fd_str, type, target);
                }
            }
        }
    }
}
