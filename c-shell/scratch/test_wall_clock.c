#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>

int main() {
    pid_t p = fork();
    if (p == 0) {
        execlp("sleep", "sleep", "5", NULL);
        exit(1);
    }
    sleep(1);
    kill(p, SIGTSTP);
    printf("Stopped, waiting 2 seconds...\n");
    sleep(2);
    printf("Resuming...\n");
    time_t t1 = time(NULL);
    kill(p, SIGCONT);
    waitpid(p, NULL, 0);
    time_t t2 = time(NULL);
    printf("Slept for %ld seconds after resume\n", t2 - t1);
    return 0;
}
