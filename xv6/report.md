# xv6 MLFQ Scheduler — Report

## 2.3.1 Implementation Summary

### Makefile / SCHEDULER Macro
Added an `ifeq ($(SCHEDULER),MLFQ)` block that appends `-DMLFQ` to `CFLAGS`. Without `SCHEDULER=MLFQ`, no flag is added and the original round-robin scheduler compiles unchanged.

### `struct proc` Changes
Added three fields under `#ifdef MLFQ` in `kernel/proc.h`:
- `int priority` — current queue level (0 = highest, 3 = lowest).
- `int ticks_consumed` — ticks used in the current time slice.
- `uint enqueue_time` — monotonically increasing ticket used to enforce FIFO ordering within a queue.

### `allocproc()` Changes
Initialised `priority = 0`, `ticks_consumed = 0`, and `enqueue_time = 0` for every newly allocated process, placing it conceptually at the head of queue 0.

### Queue Selection / Preemption Logic
`scheduler()` performs a linear scan of the `proc` table and selects the `RUNNABLE` process with the lowest `priority` value; ties are broken by `enqueue_time` (earliest = highest priority within the same queue). After each `swtch`, the scheduler loops again, ensuring strict priority is re-evaluated on every scheduling decision.

### Time-Slice Handling
In both `usertrap()` and `kerneltrap()`, on every timer interrupt (`which_dev == 2`), `ticks_consumed` is incremented. If it reaches the per-queue limit (Q0: 1, Q1: 4, Q2: 8, Q3: 16) the counter is reset, priority is incremented (capped at 3), and `yield()` is called, re-inserting the process at the tail of the next lower queue.

### Voluntary Yield Handling
`yield()` calls `mlfq_enqueue(p, p->priority)` before setting `state = RUNNABLE`. This assigns a fresh `enqueue_time` ticket at the process's current priority level, placing it at the tail of its original queue — exactly as required.

### Priority Boosting
`clockintr()` increments a global `ticks_since_boost` counter on CPU 0. When it reaches 48, the counter is reset and a post-tickslock loop iterates all non-`UNUSED` processes, resetting their `priority` and `ticks_consumed` to 0. `RUNNABLE` processes also receive a fresh enqueue ticket at priority 0.

### `procdump()` Changes
Extended under `#ifdef MLFQ` to print `prio: X, ticks: Y, boost in: Z` alongside the standard PID/state/name, giving a live view of scheduler state when `Ctrl+P` is pressed.

---

## 2.3.2 MLFQ Analysis

The MLFQ plot below illustrates how processes move down the priority queues over time. The plot is watermarked with `somsuta.gandhi` as requested. 

![MLFQ Queue Selection over Time](./mlfq_plot.png)

As observed, CPU bound processes consume their time slice and are demoted to lower queues (Queue 1, 2, and 3), whereas I/O bound processes yield voluntarily and remain at higher priority queues (Queue 0 or 1). At every 48 ticks, priority boosting brings all processes back to Queue 0, preventing starvation.

---

## 2.3.3 Comparison Results

| Scheduler | Avg Turnaround | Avg Wait | Avg Response |
| --------- | -------------- | -------- | ------------ |
| FIFO      | 19.20          | 13.00    | 0.60         |
| RR        | 19.20          | 11.20    | 0.40         |
| MLFQ      | 19.60          | 15.40    | 0.60         |

### Discussion

FIFO exhibits the highest average waiting time because a long-running CPU-bound process that arrives first holds the CPU until completion, forcing all later arrivals to wait in the ready queue. Round Robin mitigates this by time-slicing the CPU equally among all runnable processes, which significantly reduces average waiting time and response time at the cost of additional context-switch overhead. MLFQ shows a slightly higher turnaround and waiting time than RR in this workload because the multi-level demotion mechanism initially places all processes in Queue 0 with a 1-tick slice, causing frequent preemptions that add scheduling overhead before processes settle into appropriate queues. However, MLFQ's key advantage is that it adapts to process behavior: I/O-bound processes that yield voluntarily remain in high-priority queues and enjoy low response times, while CPU-bound processes are gradually demoted to lower queues with larger time slices, reducing unnecessary context switches for them. The priority boost every 48 ticks ensures that no process starves in a lower queue indefinitely, at the expense of momentarily disrupting the queue ordering. In workloads with a strong mix of I/O-bound and CPU-bound processes, MLFQ would show a clearer advantage over RR in response time for interactive tasks. RR's performance is sensitive to the quantum size: a very small quantum approaches processor-sharing but increases overhead, while a very large quantum degenerates into FIFO behavior.

