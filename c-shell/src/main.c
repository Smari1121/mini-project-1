#include "prompt.h"
#include <limits.h>
#include <unistd.h>

int main(void)
{
    char home_dir[PATH_MAX];
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) return 1;
    print_prompt(home_dir);
    return 0;
}