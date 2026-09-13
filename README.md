# C-Shell (Custom C Shell)

Hey there! This is a custom shell written in C from scratch, complete with its own lexer and parser. It handles custom built-ins, complex pipelines, and I/O redirection.

## How to Run

1. Navigate to the `c-shell` directory:
   ```bash
   cd c-shell
   ```
2. Build the project using `make`:
   ```bash
   make
   ```
3. Run the compiled binary:
   ```bash
   ./shell.out
   ```

## Assumptions & Design Choices

Here are a few assumptions and specific design decisions made during development:

- **Built-in Commands in Pipelines:** The intrinsic commands (`hop`, `reveal`, `peek`, `locate`) are treated as first-class citizens. This means you can pipe to and from them just like external binaries! If a built-in is run as a single command, it executes in the main shell process (so `hop` actually changes your directory). If it's part of a pipeline, it forks into a subshell to avoid messing with the parent shell's state unexpectedly.
- **Frecency Storage:** The `hop` command's frecency database is persistent across sessions. It saves a tiny footprint file at `~/.cshell_frecency`.
- **Parsing & Execution:** We built a custom lexer and parser from the ground up rather than relying on `system()` or `popen()`. This handles tricky quote escaping and string concatenation perfectly (e.g., `echo "hello"world` is parsed as one word).
- **Redirection Pre-flights:** Before executing a command block, the shell runs a "pre-flight" check on all specified `<` and `>` files. If an output file can't be created or an input file doesn't exist, the shell safely aborts without running the command.
- **Multiple Redirections:** You can totally do things like `cat < a.txt < b.txt`. The shell handles this under the hood by spinning up a lightweight "feeder" or "writer" process that concatenates the streams seamlessly.
- **Limits:** Hardcoded buffer sizes (like max 256 arguments per command or `PATH_MAX` limitations) were used to keep memory management straightforward and fast without arbitrary dynamic allocations everywhere.
- **Robust Pipelines:** Infinite `N`-stage pipelines are supported using `pipe()`. The shell rigorously closes all unused pipe file descriptors in both parent and child processes to ensure correct `EOF` signal propagation without hanging zombie processes.

Enjoy exploring the shell!

---

# xv6 — MLFQ Scheduler

This is the second part of the mini-project: we implemented a Multi-Level Feedback Queue (MLFQ) scheduler inside the xv6 kernel. The default xv6 uses plain round-robin, but now you can pick between RR, FIFO, and MLFQ at compile time.

## How to Run

You'll want to do this on **WSL / Linux** since xv6 needs the RISC-V toolchain and QEMU.

```bash
cd xv6

# Plain old Round Robin (default, no flags needed)
make clean && make qemu

# MLFQ scheduler
make clean && make qemu SCHEDULER=MLFQ

# FIFO scheduler
make clean && make qemu SCHEDULER=FIFO
```

Once xv6 boots up, you'll get a shell. Run the test program:
```
$ schedulertest
```

Press `Ctrl+P` at any time to see a live dump of all processes with their queue info (only shows extra MLFQ fields when built with `SCHEDULER=MLFQ`).

To quit QEMU: press `Ctrl-a` then `x`.

### Generating the MLFQ Plot

```bash
# Capture the console output while running schedulertest
make clean && make qemu SCHEDULER=MLFQ 2>&1 | tee mlfq_output.txt
# (run schedulertest inside xv6, wait for it to finish, then Ctrl-a x)

# Generate the scatter plot
python3 plot_mlfq.py mlfq_output.txt
```

### Running the Cross-Scheduler Comparison

```bash
python3 run_comparison.py
```
This automatically builds & runs xv6 under FIFO, RR, and MLFQ, collects metrics, and prints a comparison table.

**Benchmark Results:**
| Scheduler | Avg Turnaround | Avg Wait | Avg Response |
| --------- | -------------- | -------- | ------------ |
| FIFO      | 18.80          | 15.20    | 0.20         |
| RR        | 19.40          | 10.80    | 0.60         |
| MLFQ      | 21.20          | 15.60    | 0.00         |

## Folder Structure

```
xv6/
├── kernel/
│   ├── proc.h          # Added MLFQ fields to struct proc (priority, ticks_consumed, enqueue_time)
│   ├── proc.c          # MLFQ scheduler logic, enqueue helper, procdump, MLFQ_LOG emission
│   ├── trap.c          # Time-slice enforcement, priority boosting in clockintr()
│   ├── defs.h          # mlfq_enqueue() declaration
│   ├── param.h         # Standard xv6 params (NPROC=64, etc.)
│   └── ...             # Everything else is stock xv6
├── user/
│   ├── schedulertest.c # Our custom test program (CPU-bound, I/O-bound, mixed processes)
│   ├── user.h          # User-space syscall declarations
│   └── ...             # Standard xv6 user programs
├── Makefile            # SCHEDULER=MLFQ / FIFO compile-time flag support
├── plot_mlfq.py        # Python script to generate MLFQ timeline scatter plot
├── run_comparison.py   # Python script for cross-scheduler metric comparison
└── report.md           # Full report (implementation summary, analysis, comparison)
```

## Design Choices

- **No explicit queue data structures.** Instead of maintaining four separate linked lists for the MLFQ queues, we just store `priority` and `enqueue_time` on each `struct proc` and do a linear scan of the proc table every time the scheduler runs. This keeps things simple and avoids the headache of managing queue pointers alongside xv6's existing locking. The proc table is only 64 entries, so the scan is cheap.

- **Monotonic enqueue ticket for FIFO ordering.** Each time a process enters a queue (creation, wakeup, yield, boost), it gets a fresh ticket from a global counter via `__sync_fetch_and_add`. Within the same priority level, the scheduler picks the process with the smallest ticket — this gives us FIFO ordering without actually maintaining a linked list.

- **Compile-time scheduler selection.** We use `#ifdef MLFQ` / `#ifdef FIFO` guards throughout the code. If you don't pass `SCHEDULER=`, you get vanilla round-robin — the RR code path is completely untouched. This means the default build is identical to stock xv6.

- **Preemption at tick boundaries only.** The spec says preemption only needs to happen when the kernel next regains control. So we check for higher-priority processes inside `usertrap()` and `kerneltrap()` on every timer interrupt, not in the scheduler itself. If a higher-priority process is runnable, we yield immediately.

- **MLFQ_LOG kernel prints for plotting.** Rather than trying to scrape `procdump` output via Ctrl+P (which is unreliable and timing-dependent), the scheduler itself emits `MLFQ_LOG <tick> <pid> <queue>` every time it picks a process. The Python plot script just greps for these lines — way more robust.

- **Stolen-process retry (multi-core safety).** With `CPUS=3`, multiple CPUs scan the proc table concurrently. Two CPUs can independently pick the same `best_p`. When the slower CPU re-acquires the lock and finds the process was already stolen (state changed from RUNNABLE), it uses `continue` to re-scan instead of falling through to `wfi` sleep. Without this, a CPU would go idle while runnable processes starve.

- **Preemption scan with proper locking.** The preemption check (does a higher-priority process exist?) must acquire each scanned process's lock before reading `state` and `priority`. xv6's invariant says `p->lock` must be held when reading `p->state`. We release the current process's lock *first* to avoid nested-proc-lock deadlocks, save `current_priority` locally, then scan with per-proc locking.

## Assumptions

- **Single-tick timer granularity.** We assume each timer interrupt = 1 tick, which is how xv6 defines it in `clockintr()`.
- **`pause(1)` counts as voluntary yield.** When a process calls `pause(1)`, it sleeps for 1 tick. On wakeup, it re-enters the same queue it was in (priority preserved). This is how we simulate I/O-bound behavior in `schedulertest`.
- **Priority boost resets ticks_consumed.** When boosting all processes to queue 0 every 48 ticks, we also reset `ticks_consumed` to 0 so they get a fresh 1-tick slice in queue 0. This felt like the right interpretation of "moved to queue 0."
- **PID 1 (init) and PID 2 (sh) are excluded from MLFQ_LOG.** They're always running and would clutter the plot. The scheduler still handles them with MLFQ rules though.
- **Multi-CPU behavior.** xv6 runs with 3 CPUs by default. Each CPU runs the scheduler independently. The global enqueue ticket uses an atomic increment to stay consistent across CPUs.

## Key Changes from Stock xv6

1. **`kernel/proc.h`** — Three new fields in `struct proc` under `#ifdef MLFQ`: `priority`, `ticks_consumed`, `enqueue_time`. Also added timing fields (`ctime`, `rtime`, `retime`, etc.) unconditionally for metrics.

2. **`kernel/proc.c`** — The big one. Added `mlfq_enqueue()` helper, rewrote `scheduler()` with an MLFQ branch that picks the highest-priority earliest-enqueued process (and a true FIFO branch that picks the earliest `ctime`), added MLFQ_LOG printing, enriched `procdump()`, and added `METRICS PID` output in `kexit()` for the comparison script.

3. **`kernel/trap.c`** — Added time-slice tracking and demotion logic in both `usertrap()` and `kerneltrap()`. Added priority boost mechanism in `clockintr()` (every 48 ticks). Also added per-tick metric updates (rtime/retime/slptime).

4. **`Makefile`** — Added `ifeq ($(SCHEDULER),MLFQ)` / `ifeq ($(SCHEDULER),FIFO)` blocks to pass `-DMLFQ` or `-DFIFO` to the compiler.

5. **`user/schedulertest.c`** — Custom test program that spawns 5 processes with different CPU/IO profiles to exercise all MLFQ behaviors (demotion, voluntary yield, priority boost).
