#ifndef EXEC_H
#define EXEC_H

#include "lexer.h"
#include <limits.h>

typedef struct {
    char home_dir[PATH_MAX];
    char prev_dir[PATH_MAX];
} shell_state_t;

void execute_line(token_list_t *list, shell_state_t *state);

#endif
