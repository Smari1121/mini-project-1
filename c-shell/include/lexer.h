#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_AMP,
    TOK_SEMI,
    TOK_LT,
    TOK_GT,
    TOK_GTGT
} tok_type_t;

typedef struct {
    tok_type_t type;
    char *value;
} token_t;

typedef struct {
    token_t *toks;
    int count;
    int cap;
} token_list_t;

void token_list_init(token_list_t *list);
void token_list_free(token_list_t *list);
int lexer_tokenize(const char *input, token_list_t *list);

#endif
