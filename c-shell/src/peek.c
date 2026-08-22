#include "peek.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

static void process_stream(FILE *f, bool show_num, bool reverse, bool is_seekable, int fd) {
    if (!reverse) {
        char *line = NULL;
        size_t len = 0;
        int cur_num = 1;
        while (getline(&line, &len, f) != -1) {
            line[strcspn(line, "\n")] = '\0';
            line[strcspn(line, "\r")] = '\0';
            if (strlen(line) > 0) {
                if (show_num) printf("%d %s\n", cur_num++, line);
                else printf("%s\n", line);
            } else {
                printf("\n");
            }
        }
        free(line);
    } else {
        if (is_seekable) {
            off_t file_size = lseek(fd, 0, SEEK_END);
            int total_non_empty = 0;
            
            if (show_num) {
                lseek(fd, 0, SEEK_SET);
                char *line = NULL;
                size_t len = 0;
                FILE *tmp = fdopen(dup(fd), "r");
                if (tmp) {
                    while (getline(&line, &len, tmp) != -1) {
                        line[strcspn(line, "\n")] = '\0';
                        line[strcspn(line, "\r")] = '\0';
                        if (strlen(line) > 0) total_non_empty++;
                    }
                    free(line);
                    fclose(tmp);
                }
            }
            
            lseek(fd, 0, SEEK_END);
            off_t pos = file_size;
            char chunk[4096];
            char *pending = NULL;
            int is_first_chunk = 1;

            while (pos > 0) {
                off_t read_start = (pos > 4096) ? pos - 4096 : 0;
                size_t to_read = pos - read_start;
                
                lseek(fd, read_start, SEEK_SET);
                read(fd, chunk, to_read);
                
                if (is_first_chunk) {
                    if (to_read > 0 && chunk[to_read - 1] == '\n') {
                        to_read--;
                    }
                    is_first_chunk = 0;
                }
                
                int last_idx = to_read;
                for (int i = to_read - 1; i >= 0; i--) {
                    if (chunk[i] == '\n') {
                        int frag_len = last_idx - (i + 1);
                        char *line = malloc(frag_len + (pending ? strlen(pending) : 0) + 1);
                        line[0] = '\0';
                        if (frag_len > 0) strncat(line, &chunk[i + 1], frag_len);
                        if (pending) {
                            strcat(line, pending);
                            free(pending);
                            pending = NULL;
                        }
                        
                        if (strlen(line) > 0 && line[strlen(line)-1] == '\r') {
                            line[strlen(line)-1] = '\0';
                        }
                        
                        if (strlen(line) > 0) {
                            if (show_num) printf("%d %s\n", total_non_empty--, line);
                            else printf("%s\n", line);
                        } else {
                            printf("\n");
                        }
                        
                        free(line);
                        last_idx = i;
                    }
                }
                
                if (last_idx > 0) {
                    int frag_len = last_idx;
                    char *new_pending = malloc(frag_len + (pending ? strlen(pending) : 0) + 1);
                    new_pending[0] = '\0';
                    strncat(new_pending, chunk, frag_len);
                    if (pending) {
                        strcat(new_pending, pending);
                        free(pending);
                    }
                    pending = new_pending;
                }
                pos = read_start;
            }
            if (pending) {
                if (strlen(pending) > 0 && pending[strlen(pending)-1] == '\r') {
                    pending[strlen(pending)-1] = '\0';
                }
                if (strlen(pending) > 0) {
                    if (show_num) printf("%d %s\n", total_non_empty--, pending);
                    else printf("%s\n", pending);
                } else {
                    printf("\n");
                }
                free(pending);
            }
        } else {
            char **lines = NULL;
            int count = 0, cap = 0;
            char *line = NULL;
            size_t len = 0;
            while (getline(&line, &len, f) != -1) {
                line[strcspn(line, "\n")] = '\0';
                line[strcspn(line, "\r")] = '\0';
                if (count >= cap) {
                    cap = cap ? cap * 2 : 64;
                    lines = realloc(lines, cap * sizeof(char *));
                }
                lines[count++] = strdup(line);
            }
            free(line);
            
            int total_non_empty = 0;
            for (int i = 0; i < count; i++) {
                if (strlen(lines[i]) > 0) total_non_empty++;
            }
            
            for (int i = count - 1; i >= 0; i--) {
                if (strlen(lines[i]) > 0) {
                    if (show_num) printf("%d %s\n", total_non_empty--, lines[i]);
                    else printf("%s\n", lines[i]);
                } else {
                    printf("\n");
                }
                free(lines[i]);
            }
            free(lines);
        }
    }
}

int execute_peek(char **args, int arg_count, shell_state_t *state) {
    bool show_num = false;
    bool reverse = false;
    int first_file_idx = -1;
    
    for (int i = 0; i < arg_count; i++) {
        if (args[i][0] == '-' && args[i][1] != '\0') {
            for (int j = 1; args[i][j] != '\0'; j++) {
                if (args[i][j] == 'n') show_num = true;
                else if (args[i][j] == 'r') reverse = true;
                else {
                    fprintf(stderr, "peek: invalid syntax\n");
                    return 1;
                }
            }
        } else {
            first_file_idx = i;
            break;
        }
    }
    
    if (first_file_idx == -1) {
        process_stream(stdin, show_num, reverse, false, STDIN_FILENO);
    } else {
        for (int i = first_file_idx; i < arg_count; i++) {
            const char *file = args[i];
            if (strcmp(file, "-") == 0) {
                process_stream(stdin, show_num, reverse, false, STDIN_FILENO);
            } else {
                struct stat st;
                if (stat(file, &st) != 0) {
                    fprintf(stderr, "peek: no such file or directory\n");
                    continue;
                }
                if (S_ISDIR(st.st_mode)) {
                    fprintf(stderr, "peek: is a directory\n");
                    continue;
                }
                
                int fd = open(file, O_RDONLY);
                if (fd == -1) {
                    fprintf(stderr, "peek: no such file or directory\n");
                    continue;
                }
                
                FILE *f = fdopen(fd, "r");
                if (!f) {
                    close(fd);
                    continue;
                }
                process_stream(f, show_num, reverse, true, fd);
                fclose(f);
            }
        }
    }
    return 0;
}
