#ifndef EXEC_H
#define EXEC_H

#include "lexer.h"
#include <limits.h>

typedef struct {
    char home_dir[PATH_MAX];
    char prev_dir[PATH_MAX];
} shell_state_t;

void execute_line(token_list_t *list, shell_state_t *state);
void init_jobs(void);
void cleanup_jobs(void);
int check_stopped_jobs(void);
void kill_all_jobs(void);
extern int ctrl_d_pressed;

#endif
