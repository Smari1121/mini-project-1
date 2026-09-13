#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

void handler(int sig) {}

int main() {
    pid_t p = fork();
    if (p == 0) {
        setpgid(0, 0);
        execlp("sleep", "sleep", "2", NULL);
        exit(1);
    }
    setpgid(p, p);
    kill(-p, SIGTSTP);
    
    int st;
    waitpid(p, &st, WUNTRACED);
    printf("Stopped by %d\n", WSTOPSIG(st));

    tcsetpgrp(0, p);
    kill(-p, SIGCONT);
    
    while(1) {
        pid_t r = waitpid(p, &st, WUNTRACED | WCONTINUED);
        if (r > 0) {
            if (WIFSTOPPED(st)) {
                printf("Stopped AGAIN by %d\n", WSTOPSIG(st));
                break;
            } else if (WIFCONTINUED(st)) {
                printf("Continued\n");
            } else if (WIFEXITED(st)) {
                printf("Exited %d\n", WEXITSTATUS(st));
                break;
            } else if (WIFSIGNALED(st)) {
                printf("Signaled %d\n", WTERMSIG(st));
                break;
            }
        }
    }
    tcsetpgrp(0, getpgrp());
    return 0;
}
