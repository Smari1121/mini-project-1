#include "lexer.h"
#include <stdlib.h>
#include <string.h>

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int is_special(char c)
{
    return c == '|' || c == '&' || c == '>' || c == '<' || c == ';';
}

void token_list_init(token_list_t *list)
{
    list->toks = NULL;
    list->count = 0;
    list->cap = 0;
}

void token_list_free(token_list_t *list)
{
    for (int i = 0; i < list->count; i++)
        free(list->toks[i].value);
    free(list->toks);
    list->toks = NULL;
    list->count = 0;
    list->cap = 0;
}

static void token_add(token_list_t *list, tok_type_t type, char *value)
{
    if (list->count >= list->cap){
        list->cap = list->cap ? list->cap * 2 : 8;
        list->toks = realloc(list->toks, list->cap * sizeof(token_t));
    }
    list->toks[list->count].type = type;
    list->toks[list->count].value = value;
    list->count++;
}

static int buf_push(char **buf, size_t *len, size_t *cap, char c)
{
    if (*len + 1 >= *cap){
        size_t nc = *cap ? *cap * 2 : 32;
        char *tmp = realloc(*buf, nc);
        if (!tmp) return -1;
        *buf = tmp;
        *cap = nc;
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
    return 0;
}

static int lex_dquote(const char **pp, char **word, size_t *wlen, size_t *wcap)
{
    const char *p = *pp + 1;
    while (*p && *p != '"'){
        if (*p == '\\'){
            p++;
            if (!*p) return -1;
            if (*p == '"' || *p == '\\')
                buf_push(word, wlen, wcap, *p);
            else {
                buf_push(word, wlen, wcap, '\\');
                buf_push(word, wlen, wcap, *p);
            }
            p++;
        } else {
            buf_push(word, wlen, wcap, *p);
            p++;
        }
    }
    if (*p != '"') return -1;
    *pp = p + 1;
    return 0;
}

static int lex_squote(const char **pp, char **word, size_t *wlen, size_t *wcap)
{
    const char *p = *pp + 1;
    while (*p && *p != '\''){
        buf_push(word, wlen, wcap, *p);
        p++;
    }
    if (*p != '\'') return -1;
    *pp = p + 1;
    return 0;
}

int lexer_tokenize(const char *input, token_list_t *list)
{
    const char *p = input;

    while (*p){
        while (*p && is_space(*p)) p++;
        if (!*p) break;

        if (*p == '|'){ token_add(list, TOK_PIPE, NULL); p++; continue; }
        if (*p == '&'){ token_add(list, TOK_AMP, NULL); p++; continue; }
        if (*p == ';'){ token_add(list, TOK_SEMI, NULL); p++; continue; }
        if (*p == '<'){ token_add(list, TOK_LT, NULL); p++; continue; }
        if (*p == '>'){
            if (p[1] == '>'){
                token_add(list, TOK_GTGT, NULL);
                p += 2;
            } else {
                token_add(list, TOK_GT, NULL);
                p++;
            }
            continue;
        }

        char *word = NULL;
        size_t wlen = 0, wcap = 0;

        while (*p && !is_space(*p) && !is_special(*p)){
            if (*p == '\\'){
                p++;
                if (!*p){ free(word); return -1; }
                buf_push(&word, &wlen, &wcap, *p);
                p++;
            } else if (*p == '"'){
                if (lex_dquote(&p, &word, &wlen, &wcap) != 0){
                    free(word);
                    return -1;
                }
            } else if (*p == '\''){
                if (lex_squote(&p, &word, &wlen, &wcap) != 0){
                    free(word);
                    return -1;
                }
            } else {
                buf_push(&word, &wlen, &wcap, *p);
                p++;
            }
        }

        if (!word) word = strdup("");
        token_add(list, TOK_WORD, word);
    }

    return 0;
}
