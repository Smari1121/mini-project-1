#include "prompt.h"
#include <pwd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void print_prompt(const char *home_dir)
{
    char cwd[PATH_MAX];
    char hostname[HOST_NAME_MAX + 1];

    if (getcwd(cwd, sizeof(cwd)) == NULL) return;
    if (gethostname(hostname, sizeof(hostname)) != 0) return;
    hostname[HOST_NAME_MAX] = '\0';

    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL) return;

    size_t home_len = strlen(home_dir);
    if (strcmp(cwd, home_dir) == 0){
        printf("<%s@%s:~> ", pw->pw_name, hostname);
    } else if (strncmp(cwd, home_dir, home_len) == 0 && cwd[home_len] == '/'){
        printf("<%s@%s:~%s> ", pw->pw_name, hostname, cwd + home_len);
    } else {
        printf("<%s@%s:%s> ", pw->pw_name, hostname, cwd);
    }
    fflush(stdout);
}