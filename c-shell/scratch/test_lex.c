#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int buf_push(char **buf, size_t *len, size_t *cap, char c) {
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

static int lex_dquote(const char **pp, char **word, size_t *wlen, size_t *wcap) {
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

int main() {
    const char *input = "\"b\\na\\n\"";
    char *word = NULL;
    size_t wlen = 0, wcap = 0;
    const char *p = input;
    int res = lex_dquote(&p, &word, &wlen, &wcap);
    printf("res=%d word=%s\n", res, word);
    return 0;
}
