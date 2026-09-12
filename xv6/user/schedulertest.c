#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define CPU_BURST 5000000

static void cpu_hog(int id, int bursts) {
  int pid = getpid();
  for (int b = 0; b < bursts; b++) {
    for (volatile int i = 0; i < CPU_BURST; i++)
      ;
    // Just burning CPU
  }
  printf("schedulertest: cpu hog id=%d pid=%d done\n", id, pid);
  exit(0);
}

static void io_bound(int id, int iters, int burst_fraction) {
  int pid = getpid();
  for (int i = 0; i < iters; i++) {
    for (volatile int j = 0; j < CPU_BURST / burst_fraction; j++)
      ;
    pause(1); // Voluntary yield
  }
  printf("schedulertest: io bound id=%d pid=%d done\n", id, pid);
  exit(0);
}

int main(void) {
  int pids[5];
  int start = uptime();

  printf("schedulertest: starting at tick %d\n", start);

  // CPU bound, long bursts
  if ((pids[0] = fork()) == 0) cpu_hog(0, 10);
  if ((pids[1] = fork()) == 0) cpu_hog(1, 10);

  // I/O bound, frequent yields
  if ((pids[2] = fork()) == 0) io_bound(2, 50, 100);
  
  // Mixed
  if ((pids[3] = fork()) == 0) io_bound(3, 20, 10);
  if ((pids[4] = fork()) == 0) io_bound(4, 20, 10);

  for (int i = 0; i < 5; i++) {
    wait(0);
  }

  printf("schedulertest: all done in %d ticks\n", uptime() - start);
  exit(0);
}
