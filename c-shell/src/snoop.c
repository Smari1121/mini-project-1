#include "snoop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

#define MAX_SYSCALLS 512

static const char *syscall_names[] = {
    [0] = "read", [1] = "write", [2] = "open", [3] = "close", [4] = "stat", [5] = "fstat",
    [6] = "lstat", [7] = "poll", [8] = "lseek", [9] = "mmap", [10] = "mprotect", [11] = "munmap",
    [12] = "brk", [13] = "rt_sigaction", [14] = "rt_sigprocmask", [15] = "rt_sigreturn",
    [16] = "ioctl", [17] = "pread64", [18] = "pwrite64", [19] = "readv", [20] = "writev",
    [21] = "access", [22] = "pipe", [23] = "select", [24] = "sched_yield", [25] = "mremap",
    [26] = "msync", [27] = "mincore", [28] = "madvise", [29] = "shmget", [30] = "shmat",
    [31] = "shmctl", [32] = "dup", [33] = "dup2", [34] = "pause", [35] = "nanosleep",
    [36] = "getitimer", [37] = "alarm", [38] = "setitimer", [39] = "getpid", [40] = "sendfile",
    [41] = "socket", [42] = "connect", [43] = "accept", [44] = "sendto", [45] = "recvfrom",
    [46] = "sendmsg", [47] = "recvmsg", [48] = "shutdown", [49] = "bind", [50] = "listen",
    [51] = "getsockname", [52] = "getpeername", [53] = "socketpair", [54] = "setsockopt",
    [55] = "getsockopt", [56] = "clone", [57] = "fork", [58] = "vfork", [59] = "execve",
    [60] = "exit", [61] = "wait4", [62] = "kill", [63] = "uname", [64] = "semget",
    [65] = "semop", [66] = "semctl", [67] = "shmdt", [68] = "msgget", [69] = "msgsnd",
    [70] = "msgrcv", [71] = "msgctl", [72] = "fcntl", [73] = "flock", [74] = "fsync",
    [75] = "fdatasync", [76] = "truncate", [77] = "ftruncate", [78] = "getdents", [79] = "getcwd",
    [80] = "chdir", [81] = "fchdir", [82] = "rename", [83] = "mkdir", [84] = "rmdir",
    [85] = "creat", [86] = "link", [87] = "unlink", [88] = "symlink", [89] = "readlink",
    [90] = "chmod", [91] = "fchmod", [92] = "chown", [93] = "fchown", [94] = "lchown",
    [95] = "umask", [96] = "gettimeofday", [97] = "getrlimit", [98] = "getrusage",
    [99] = "sysinfo", [100] = "times", [101] = "ptrace", [102] = "getuid", [103] = "syslog",
    [104] = "getgid", [105] = "setuid", [106] = "setgid", [107] = "geteuid", [108] = "getegid",
    [109] = "setpgid", [110] = "getppid", [111] = "getpgrp", [112] = "setsid", [113] = "setreuid",
    [114] = "setregid", [115] = "getgroups", [116] = "setgroups", [117] = "setresuid",
    [118] = "getresuid", [119] = "setresgid", [120] = "getresgid", [121] = "getpgid",
    [122] = "setfsuid", [123] = "setfsgid", [124] = "getsid", [125] = "capget",
    [126] = "capset", [127] = "rt_sigpending", [128] = "rt_sigtimedwait",
    [129] = "rt_sigqueueinfo", [130] = "rt_sigsuspend", [131] = "sigaltstack", [132] = "utime",
    [133] = "mknod", [134] = "uselib", [135] = "personality", [136] = "ustat",
    [137] = "statfs", [138] = "fstatfs", [139] = "sysfs", [140] = "getpriority",
    [141] = "setpriority", [142] = "sched_setparam", [143] = "sched_getparam",
    [144] = "sched_setscheduler", [145] = "sched_getscheduler", [146] = "sched_get_priority_max",
    [147] = "sched_get_priority_min", [148] = "sched_rr_get_interval", [149] = "mlock",
    [150] = "munlock", [151] = "mlockall", [152] = "munlockall", [153] = "vhangup",
    [154] = "modify_ldt", [155] = "pivot_root", [156] = "_sysctl", [157] = "prctl",
    [158] = "arch_prctl", [159] = "adjtimex", [160] = "setrlimit", [161] = "chroot",
    [162] = "sync", [163] = "acct", [164] = "settimeofday", [165] = "mount",
    [166] = "umount2", [167] = "swapon", [168] = "swapoff", [169] = "reboot",
    [170] = "sethostname", [171] = "setdomainname", [172] = "iopl", [173] = "ioperm",
    [174] = "create_module", [175] = "init_module", [176] = "delete_module", [177] = "get_kernel_syms",
    [178] = "query_module", [179] = "quotactl", [180] = "nfsservctl", [181] = "getpmsg",
    [182] = "putpmsg", [183] = "afs_syscall", [184] = "tuxcall", [185] = "security",
    [186] = "gettid", [187] = "readahead", [188] = "setxattr", [189] = "lsetxattr",
    [190] = "fsetxattr", [191] = "getxattr", [192] = "lgetxattr", [193] = "fgetxattr",
    [194] = "listxattr", [195] = "llistxattr", [196] = "flistxattr", [197] = "removexattr",
    [198] = "lremovexattr", [199] = "fremovexattr", [200] = "tkill", [201] = "time",
    [202] = "futex", [203] = "sched_setaffinity", [204] = "sched_getaffinity", [205] = "set_thread_area",
    [206] = "io_setup", [207] = "io_destroy", [208] = "io_getevents", [209] = "io_submit",
    [210] = "io_cancel", [211] = "get_thread_area", [212] = "lookup_dcookie", [213] = "epoll_create",
    [214] = "epoll_ctl_old", [215] = "epoll_wait_old", [216] = "remap_file_pages", [217] = "getdents64",
    [218] = "set_tid_address", [219] = "restart_syscall", [220] = "semtimedop", [221] = "fadvise64",
    [222] = "timer_create", [223] = "timer_settime", [224] = "timer_gettime", [225] = "timer_getoverrun",
    [226] = "timer_delete", [227] = "clock_settime", [228] = "clock_gettime", [229] = "clock_getres",
    [230] = "clock_nanosleep", [231] = "exit_group", [232] = "epoll_wait", [233] = "epoll_ctl",
    [234] = "tgkill", [235] = "utimes", [236] = "vserver", [237] = "mbind",
    [238] = "set_mempolicy", [239] = "get_mempolicy", [240] = "mq_open", [241] = "mq_unlink",
    [242] = "mq_timedsend", [243] = "mq_timedreceive", [244] = "mq_notify", [245] = "mq_getsetattr",
    [246] = "kexec_load", [247] = "waitid", [248] = "add_key", [249] = "request_key",
    [250] = "keyctl", [251] = "ioprio_set", [252] = "ioprio_get", [253] = "inotify_init",
    [254] = "inotify_add_watch", [255] = "inotify_rm_watch", [256] = "migrate_pages", [257] = "openat",
    [258] = "mkdirat", [259] = "mknodat", [260] = "fchownat", [261] = "futimesat",
    [262] = "newfstatat", [263] = "unlinkat", [264] = "renameat", [265] = "linkat",
    [266] = "symlinkat", [267] = "readlinkat", [268] = "fchmodat", [269] = "faccessat",
    [270] = "pselect6", [271] = "ppoll", [272] = "unshare", [273] = "set_robust_list",
    [274] = "get_robust_list", [275] = "splice", [276] = "tee", [277] = "sync_file_range",
    [278] = "vmsplice", [279] = "move_pages", [280] = "utimensat", [281] = "epoll_pwait",
    [282] = "signalfd", [283] = "timerfd_create", [284] = "eventfd", [285] = "fallocate",
    [286] = "timerfd_settime", [287] = "timerfd_gettime", [288] = "accept4", [289] = "signalfd4",
    [290] = "eventfd2", [291] = "epoll_create1", [292] = "dup3", [293] = "pipe2",
    [294] = "inotify_init1", [295] = "preadv", [296] = "pwritev", [297] = "rt_tgsigqueueinfo",
    [298] = "perf_event_open", [299] = "recvmmsg", [300] = "fanotify_init", [301] = "fanotify_mark",
    [302] = "prlimit64", [303] = "name_to_handle_at", [304] = "open_by_handle_at", [305] = "clock_adjtime",
    [306] = "syncfs", [307] = "sendmmsg", [308] = "setns", [309] = "getcpu",
    [310] = "process_vm_readv", [311] = "process_vm_writev", [312] = "kcmp", [313] = "finit_module",
    [314] = "sched_setattr", [315] = "sched_getattr", [316] = "renameat2", [317] = "seccomp",
    [318] = "getrandom", [319] = "memfd_create", [320] = "kexec_file_load", [321] = "bpf",
    [322] = "execveat", [323] = "userfaultfd", [324] = "membarrier", [325] = "mlock2",
    [326] = "copy_file_range", [327] = "preadv2", [328] = "pwritev2", [329] = "pkey_mprotect",
    [330] = "pkey_alloc", [331] = "pkey_free", [332] = "statx",
    [333] = "io_pgetevents", [334] = "rseq",
    [424] = "pidfd_send_signal", [425] = "io_uring_setup", [426] = "io_uring_enter",
    [427] = "io_uring_register", [428] = "open_tree", [429] = "move_mount",
    [430] = "fsopen", [431] = "fsconfig", [432] = "fsmount", [433] = "fspick",
    [434] = "pidfd_open", [435] = "clone3",
    [436] = "close_range", [437] = "openat2", [438] = "pidfd_getfd", [439] = "faccessat2",
    [440] = "process_madvise", [441] = "epoll_pwait2",
    [442] = "mount_setattr", [443] = "quotactl_fd", [444] = "landlock_create_ruleset",
    [445] = "landlock_add_rule", [446] = "landlock_restrict_self",
    [447] = "memfd_secret", [448] = "process_mrelease", [449] = "futex_waitv",
    [450] = "set_mempolicy_home_node"
};

#define SYSCALL_TABLE_SIZE ((int)(sizeof(syscall_names)/sizeof(syscall_names[0])))

static const char *get_syscall_name(int sys_num) {
    static char buf[32];
    if (sys_num >= 0 && sys_num < SYSCALL_TABLE_SIZE && syscall_names[sys_num]) {
        return syscall_names[sys_num];
    }
    snprintf(buf, sizeof(buf), "syscall_%d", sys_num);
    return buf;
}

typedef struct {
    int num;
    int calls;
    double time;
    int first_seen;
} sys_stat_t;

static int compare_stats(const void *a, const void *b) {
    const sys_stat_t *sa = (const sys_stat_t *)a;
    const sys_stat_t *sb = (const sys_stat_t *)b;
    if (sa->calls != sb->calls) return sb->calls - sa->calls;
    return sa->first_seen - sb->first_seen;
}

void execute_snoop(char **args, int arg_count) {
    if (arg_count == 0) {
        printf("snoop: command not found\n");
        return;
    }

    // Block SIGCHLD so the shell's handler doesn't steal our waitpids
    sigset_t block_chld, old_mask;
    sigemptyset(&block_chld);
    sigaddset(&block_chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_chld, &old_mask);

    pid_t pid;
    int attached = 0;

    if (strcmp(args[0], "-p") == 0) {
        if (arg_count != 2) {
            printf("snoop: no such process\n");
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return;
        }
        pid = atoi(args[1]);
        if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
            printf("snoop: no such process\n");
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return;
        }
        attached = 1;
    } else {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            return;
        }
        if (pid == 0) {
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            execvp(args[0], args);
            fprintf(stderr, "snoop: command not found\n");
            _exit(1);
        }
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return;
    }

    // If the child exited immediately (e.g. exec failed), nothing to trace
    if (WIFEXITED(status)) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return;
    }

    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

    sys_stat_t stats[MAX_SYSCALLS];
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        stats[i].num = i;
        stats[i].calls = 0;
        stats[i].time = 0.0;
        stats[i].first_seen = 0;
    }

    struct timeval entry_time;
    int in_syscall = 0;
    int current_syscall = -1;
    int order = 0;

    while (1) {
        ptrace(PTRACE_SYSCALL, pid, 0, 0);
        if (waitpid(pid, &status, 0) < 0) break;

        if (WIFEXITED(status) || WIFSIGNALED(status)) break;

        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);

            if (sig == (SIGTRAP | 0x80)) {
                struct user_regs_struct regs;
                if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == 0) {
                    if (!in_syscall) {
                        current_syscall = regs.orig_rax;
                        gettimeofday(&entry_time, NULL);
                        in_syscall = 1;
                    } else {
                        struct timeval exit_time;
                        gettimeofday(&exit_time, NULL);
                        double elapsed = (exit_time.tv_sec - entry_time.tv_sec) +
                                         (exit_time.tv_usec - entry_time.tv_usec) / 1000000.0;

                        if (current_syscall >= 0 && current_syscall < MAX_SYSCALLS) {
                            stats[current_syscall].calls++;
                            stats[current_syscall].time += elapsed;
                            if (stats[current_syscall].first_seen == 0) {
                                stats[current_syscall].first_seen = ++order;
                            }
                        }
                        in_syscall = 0;
                    }
                }
            } else if (sig == SIGTRAP) {
                // exec stop or other ptrace event, continue tracing
            } else {
                // Forward the signal to the tracee
                ptrace(PTRACE_SYSCALL, pid, 0, sig);
                continue;
            }
        }
    }

    if (attached) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    sys_stat_t valid[MAX_SYSCALLS];
    int vcount = 0;
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        if (stats[i].calls > 0) {
            valid[vcount++] = stats[i];
        }
    }

    qsort(valid, vcount, sizeof(sys_stat_t), compare_stats);

    printf("%-20s %-10s %s\n", "syscall", "calls", "time");
    for (int i = 0; i < vcount; i++) {
        printf("%-20s %-10d %.3fs\n", get_syscall_name(valid[i].num), valid[i].calls, valid[i].time);
    }
}
