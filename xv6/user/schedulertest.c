#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROCS 4
#define CPU_BURST 200000

static void cpu_hog(int id, int bursts) {
  int pid = getpid();
  int start = uptime();
  for (int b = 0; b < bursts; b++) {
    for (volatile int i = 0; i < CPU_BURST; i++)
      ;
    printf("pid=%d id=%d burst=%d tick=%d\n", pid, id, b, uptime());
  }
  int end = uptime();
  printf("pid=%d id=%d done turnaround=%d\n", pid, id, end - start);
  exit(0);
}

static void io_sim(int id, int iters) {
  int pid = getpid();
  int start = uptime();
  for (int i = 0; i < iters; i++) {
    for (volatile int j = 0; j < CPU_BURST / 8; j++)
      ;
    pause(1);
    printf("pid=%d id=%d iter=%d tick=%d\n", pid, id, i, uptime());
  }
  int end = uptime();
  printf("pid=%d id=%d done turnaround=%d\n", pid, id, end - start);
  exit(0);
}

int main(void) {
  int pids[NPROCS];
  int start = uptime();

  printf("schedulertest: starting %d processes at tick=%d\n", NPROCS, start);

  for (int i = 0; i < NPROCS; i++) {
    int pid = fork();
    if (pid == 0) {
      if (i < 2)
        cpu_hog(i, 8);
      else
        io_sim(i, 8);
    }
    pids[i] = pid;
  }

  for (int i = 0; i < NPROCS; i++) {
    int status;
    wait(&status);
  }

  int end = uptime();
  printf("schedulertest: all done, total ticks=%d\n", end - start);
  exit(0);
}
