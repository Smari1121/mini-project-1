#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
int main() {
    pid_t p1 = fork();
    if (p1 == 0) {
        setpgid(0, 0);
        exit(0);
    }
    usleep(100000); 
    pid_t p2 = fork();
    if (p2 == 0) {
        int r = setpgid(0, p1);
        if (r < 0) perror("setpgid");
        else printf("setpgid success!\n");
        exit(0);
    }
    wait(NULL);
    wait(NULL);
    return 0;
}
