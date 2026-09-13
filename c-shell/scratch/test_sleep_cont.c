#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t p = fork();
    if (p == 0) {
        setpgid(0, 0);
        execlp("sleep", "sleep", "10", NULL);
        exit(1);
    }
    setpgid(p, p);
    sleep(1);
    kill(-p, SIGTSTP);
    sleep(1);
    kill(-p, SIGCONT);
    
    int st;
    waitpid(p, &st, 0);
    if (WIFEXITED(st)) printf("Exited %d\n", WEXITSTATUS(st));
    if (WIFSIGNALED(st)) printf("Signaled %d\n", WTERMSIG(st));
    return 0;
}
