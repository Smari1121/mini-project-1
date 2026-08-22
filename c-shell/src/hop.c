#include "hop.h"
#include "frecency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

int execute_hop(char **args, int arg_count, shell_state_t *state)
{
    if (arg_count == 0) {
        char current[PATH_MAX];
        if (getcwd(current, sizeof(current)) != NULL) {
            if (chdir(state->home_dir) == 0) {
                strncpy(state->prev_dir, current, sizeof(state->prev_dir));
                frecency_update(state->home_dir);
            } else {
                fprintf(stderr, "hop: no such directory\n");
            }
        }
        return 0;
    }

    for (int i = 0; i < arg_count; i++) {
        const char *arg = args[i];
        char current[PATH_MAX];
        if (getcwd(current, sizeof(current)) == NULL) continue;

        if (strcmp(arg, "") == 0 || strcmp(arg, ".") == 0) {
            continue;
        }
        
        if (strcmp(arg, "-") == 0) {
            if (state->prev_dir[0] != '\0') {
                if (chdir(state->prev_dir) == 0) {
                    strncpy(state->prev_dir, current, sizeof(state->prev_dir));
                    char newcwd[PATH_MAX];
                    if (getcwd(newcwd, sizeof(newcwd)) != NULL) {
                        frecency_update(newcwd);
                    }
                } else {
                    fprintf(stderr, "hop: no such directory\n");
                }
            }
            continue;
        }

        char expanded[PATH_MAX];
        const char *target = arg;
        
        if (arg[0] == '~') {
            snprintf(expanded, sizeof(expanded), "%s%s", state->home_dir, arg + 1);
            target = expanded;
        }

        if (chdir(target) == 0) {
            strncpy(state->prev_dir, current, sizeof(state->prev_dir));
            char newcwd[PATH_MAX];
            if (getcwd(newcwd, sizeof(newcwd)) != NULL) {
                frecency_update(newcwd);
            }
        } else {
            char *best = frecency_lookup(arg);
            if (best) {
                if (chdir(best) == 0) {
                    strncpy(state->prev_dir, current, sizeof(state->prev_dir));
                    char newcwd[PATH_MAX];
                    if (getcwd(newcwd, sizeof(newcwd)) != NULL) {
                        frecency_update(newcwd);
                    }
                } else {
                    fprintf(stderr, "hop: no such directory\n");
                }
                free(best);
            } else {
                fprintf(stderr, "hop: no such directory\n");
            }
        }
    }
    return 0;
}
