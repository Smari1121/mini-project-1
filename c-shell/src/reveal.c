#include "reveal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>

static int cmpstringp(const void *p1, const void *p2) {
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

static void reveal_dir(const char *base_path, const char *prefix, bool show_hidden, bool recursive) {
    DIR *d = opendir(base_path);
    if (!d) return;

    struct dirent *dir;
    char **entries = NULL;
    int count = 0;
    int cap = 0;

    while ((dir = readdir(d)) != NULL) {
        if (!show_hidden && dir->d_name[0] == '.') continue;
        if (recursive && (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)) {
            continue;
        }

        if (count >= cap) {
            cap = cap ? cap * 2 : 32;
            entries = realloc(entries, cap * sizeof(char *));
        }
        entries[count++] = strdup(dir->d_name);
    }
    closedir(d);

    if (count == 0) {
        free(entries);
        return;
    }

    qsort(entries, count, sizeof(char *), cmpstringp);

    for (int i = 0; i < count; i++) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entries[i]);

        struct stat st;
        int is_dir = 0;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            is_dir = 1;
        }

        if (recursive && is_dir) {
            printf("%s%s/\n", prefix, entries[i]);
            char next_prefix[PATH_MAX];
            snprintf(next_prefix, sizeof(next_prefix), "%s%s/", prefix, entries[i]);
            reveal_dir(full_path, next_prefix, show_hidden, recursive);
        } else {
            printf("%s%s\n", prefix, entries[i]);
        }
        
        free(entries[i]);
    }
    free(entries);
}

int execute_reveal(char **args, int arg_count, shell_state_t *state) {
    bool show_hidden = false;
    bool recursive = false;
    const char *target = NULL;
    int target_count = 0;

    for (int i = 0; i < arg_count; i++) {
        if (args[i][0] == '-') {
            if (args[i][1] == '\0') {
                target = args[i];
                target_count++;
            } else {
                for (int j = 1; args[i][j] != '\0'; j++) {
                    if (args[i][j] == 'a') show_hidden = true;
                    else if (args[i][j] == 't') recursive = true;
                    else {
                        fprintf(stderr, "reveal: invalid syntax\n");
                        return 1;
                    }
                }
            }
        } else {
            target = args[i];
            target_count++;
        }
    }

    if (target_count > 1) {
        fprintf(stderr, "reveal: invalid syntax\n");
        return 1;
    }

    char resolved_path[PATH_MAX];

    if (target == NULL) {
        if (getcwd(resolved_path, sizeof(resolved_path)) == NULL) return 1;
    } else if (strcmp(target, "~") == 0) {
        strncpy(resolved_path, state->home_dir, sizeof(resolved_path));
    } else if (strcmp(target, ".") == 0) {
        if (getcwd(resolved_path, sizeof(resolved_path)) == NULL) return 1;
    } else if (strcmp(target, "..") == 0) {
        strncpy(resolved_path, target, sizeof(resolved_path));
    } else if (strcmp(target, "-") == 0) {
        if (state->prev_dir[0] == '\0') {
            fprintf(stderr, "reveal: no such directory\n");
            return 1;
        }
        strncpy(resolved_path, state->prev_dir, sizeof(resolved_path));
    } else if (target[0] == '~') {
        snprintf(resolved_path, sizeof(resolved_path), "%s%s", state->home_dir, target + 1);
    } else {
        strncpy(resolved_path, target, sizeof(resolved_path));
    }

    DIR *d = opendir(resolved_path);
    if (!d) {
        fprintf(stderr, "reveal: no such directory\n");
        return 1;
    }
    closedir(d);

    reveal_dir(resolved_path, "", show_hidden, recursive);
    
    return 0;
}
