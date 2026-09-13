#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

int main() {
    signal(SIGTTIN, SIG_IGN);
    pid_t p = fork();
    if (p == 0) {
        setpgid(0, 0);
        tcsetpgrp(0, getpid());
        _exit(0);
    }
    sleep(1);
    char *line = NULL;
    size_t len = 0;
    ssize_t n = getline(&line, &len, stdin);
    if (n == -1) {
        perror("getline");
    } else {
        printf("Got input\n");
    }
    return 0;
}
