#include "parser.h"

int parser_validate(const token_list_t *list)
{
    int i = 0;
    int n = list->count;

    if (n == 0) return 0;
    if (list->toks[0].type != TOK_WORD) return -1;
    i = 1;

    while (i < n){
        tok_type_t t = list->toks[i].type;
        if (t == TOK_WORD){
            i++;
        } else if (t == TOK_LT || t == TOK_GT || t == TOK_GTGT){
            i++;
            if (i >= n || list->toks[i].type != TOK_WORD) return -1;
            i++;
        } else if (t == TOK_PIPE || t == TOK_SEMI){
            i++;
            if (i >= n || list->toks[i].type != TOK_WORD) return -1;
            i++;
        } else if (t == TOK_AMP){
            i++;
            if (i >= n) return 0;
            if (list->toks[i].type != TOK_WORD) return -1;
            i++;
        } else {
            return -1;
        }
    }

    return 0;
}
