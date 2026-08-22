#include "exec.h"
#include "hop.h"
#include "peek.h"
#include "reveal.h"
#include "locate.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void execute_line(token_list_t *list, shell_state_t *state)
{
    if (list->count == 0) return;

    int start = 0;
    while (start < list->count) {
        int end = start;
        while (end < list->count && list->toks[end].type != TOK_SEMI) {
            end++;
        }
        
        char *args[256];
        int arg_count = 0;
        for (int i = start; i < end; i++) {
            if (list->toks[i].type == TOK_WORD) {
                args[arg_count++] = list->toks[i].value;
            }
        }
        args[arg_count] = NULL;
        
        if (arg_count > 0) {
            if (strcmp(args[0], "hop") == 0) {
                execute_hop(args + 1, arg_count - 1, state);
            } else if (strcmp(args[0], "reveal") == 0) {
                execute_reveal(args + 1, arg_count - 1, state);
            } else if (strcmp(args[0], "peek") == 0) {
                execute_peek(args + 1, arg_count - 1, state);
            } else if (strcmp(args[0], "locate") == 0) {
                execute_locate(args + 1, arg_count - 1, state);
            } else {
                fprintf(stderr, "cshell: %s: command not found\n", args[0]);
            }
        }
        
        start = end + 1;
    }
}
