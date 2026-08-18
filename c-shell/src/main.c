#include "prompt.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char home_dir[PATH_MAX];
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) return 1;

    char *line = NULL;
    size_t len = 0;

    while (1){
        print_prompt(home_dir);
        if (getline(&line, &len, stdin) == -1) break;
    }

    free(line);
    return 0;
}