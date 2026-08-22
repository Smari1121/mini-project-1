#include "exec.h"
#include "lexer.h"
#include <string.h>
#include "parser.h"
#include "prompt.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    shell_state_t state;
    memset(&state, 0, sizeof(state));
    if (getcwd(state.home_dir, sizeof(state.home_dir)) == NULL) return 1;

    char *line = NULL;
    size_t len = 0;

    while (1){
        print_prompt(state.home_dir);
        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1) break;
        if (nread > 0 && line[nread - 1] == '\n') line[--nread] = '\0';
        if (nread > 0 && line[nread - 1] == '\r') line[--nread] = '\0';

        token_list_t tokens;
        token_list_init(&tokens);

        if (lexer_tokenize(line, &tokens) != 0 || parser_validate(&tokens) != 0) {
            fprintf(stderr, "cshell: invalid syntax\n");
        } else {
            execute_line(&tokens, &state);
        }

        token_list_free(&tokens);
    }

    free(line);
    return 0;
}