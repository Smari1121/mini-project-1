#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

void handler(int sig) {}

int main() {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    alarm(2);
    char *line = NULL;
    size_t len = 0;
    printf("Prompt> ");
    fflush(stdout);
    ssize_t n = getline(&line, &len, stdin);
    if (n == -1 && errno == EINTR) {
        printf("\nGot EINTR\n");
    } else {
        printf("Got input\n");
    }
    return 0;
}
