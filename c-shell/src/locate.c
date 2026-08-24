#include "locate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

int execute_locate(char **args, int arg_count, shell_state_t *state) {
    if (arg_count == 0) {
        fprintf(stderr, "locate: invalid syntax\n");
        return 1;
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) return 1;

    for (int i = 0; i < arg_count; i++) {
        const char *cmd = args[i];
        int found = 0;

        char full_path[PATH_MAX * 3];
        snprintf(full_path, sizeof(full_path), "%s/%s", cwd, cmd);
        
        struct stat st;
        if (access(full_path, X_OK) == 0 && stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            printf("%s\n", full_path);
            found = 1;
        }

        const char *path_env = getenv("PATH");
        if (path_env) {
            char *path_copy = strdup(path_env);
            if (path_copy) {
                char *dir = strtok(path_copy, ":");
                while (dir != NULL) {
                    if (dir[0] == '/') {
                        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
                    } else {
                        snprintf(full_path, sizeof(full_path), "%s/%s/%s", cwd, dir, cmd);
                    }
                    
                    if (access(full_path, X_OK) == 0 && stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                        printf("%s\n", full_path);
                        found = 1;
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_copy);
            }
        }

        if (!found) {
            fprintf(stderr, "locate: command not found (%s)\n", cmd);
        }
    }

    return 0;
}
