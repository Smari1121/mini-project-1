#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

int main(int argc, char **argv) {
    pid_t pid = fork();
    if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[1], argv + 1);
        printf("command not found\n");
        exit(1);
    }
    
    int status;
    waitpid(pid, &status, 0);
    printf("Initial wait: WIFEXITED=%d WIFSTOPPED=%d WSTOPSIG=%d\n", 
           WIFEXITED(status), WIFSTOPPED(status), WIFSTOPPED(status) ? WSTOPSIG(status) : 0);
           
    if (WIFEXITED(status)) return 0;

    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
    
    while (1) {
        ptrace(PTRACE_SYSCALL, pid, 0, 0);
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            printf("Child exited\n");
            break;
        }
        if (WIFSIGNALED(status)) {
            printf("Child signaled\n");
            break;
        }
        
        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);
            if (sig == (SIGTRAP | 0x80)) {
                struct user_regs_struct regs;
                ptrace(PTRACE_GETREGS, pid, 0, &regs);
                printf("Syscall trap! orig_rax=%llu\n", regs.orig_rax);
            } else {
                printf("Other stop: sig=%d\n", sig);
            }
        }
    }
    return 0;
}
