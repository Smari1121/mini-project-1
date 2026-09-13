#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// CPU-bound: burns CPU for many ticks, will get demoted through queues
static void cpu_hog(int id, int bursts) {
  int pid = getpid();
  printf("schedulertest: cpu_hog id=%d pid=%d started\n", id, pid);
  for (int b = 0; b < bursts; b++) {
    for (volatile int i = 0; i < 5000000; i++)
      ;
  }
  printf("schedulertest: cpu_hog id=%d pid=%d done\n", id, pid);
  exit(0);
}

// I/O-bound: short CPU bursts followed by voluntary yields (sleep)
// Should stay at high priority queues
static void io_bound(int id, int iters, int burst_fraction) {
  int pid = getpid();
  printf("schedulertest: io_bound id=%d pid=%d started\n", id, pid);
  for (int i = 0; i < iters; i++) {
    for (volatile int j = 0; j < 5000000 / burst_fraction; j++)
      ;
    pause(1); // Voluntary yield — sleeps for 1 tick
  }
  printf("schedulertest: io_bound id=%d pid=%d done\n", id, pid);
  exit(0);
}

// Mixed: alternates between CPU bursts and I/O
static void mixed(int id, int rounds) {
  int pid = getpid();
  printf("schedulertest: mixed id=%d pid=%d started\n", id, pid);
  for (int r = 0; r < rounds; r++) {
    // CPU burst
    for (volatile int i = 0; i < 5000000; i++)
      ;
    // Then yield
    pause(2);
  }
  printf("schedulertest: mixed id=%d pid=%d done\n", id, pid);
  exit(0);
}

int main(void) {
  int n = 5;
  int start = uptime();

  printf("schedulertest: starting at tick %d\n", start);

  // Process 0: CPU-bound, long running — should get demoted to queue 3
  if (fork() == 0) cpu_hog(0, 200);

  // Process 1: CPU-bound, medium — should get demoted to queue 2-3
  if (fork() == 0) cpu_hog(1, 150);

  // Process 2: I/O-bound, frequent yields — should stay in queue 0
  if (fork() == 0) io_bound(2, 150, 100);

  // Process 3: Mixed behavior — should oscillate between queues
  if (fork() == 0) mixed(3, 50);

  // Process 4: I/O-bound, moderate — should stay in queue 0-1
  if (fork() == 0) io_bound(4, 100, 50);

  for (int i = 0; i < n; i++) {
    wait(0);
  }

  printf("schedulertest: all done in %d ticks\n", uptime() - start);
  exit(0);
}
