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

> **TODO**: Run `schedulertest` under the MLFQ build, capture the per-process tick/queue output, and produce a timeline plot (tick on X-axis, queue on Y-axis, one color per PID).

---

## 2.3.3 Comparison Results

> **TODO**: Run the same workload under default RR and compute average Turnaround Time, Waiting Time, and Response Time for RR vs MLFQ (and FIFO if available from Homework 2). Fill in the table below.

| Scheduler | Avg Turnaround | Avg Waiting | Avg Response |
|-----------|---------------|-------------|--------------|
| FIFO      | —             | —           | —            |
| Round Robin | —           | —           | —            |
| MLFQ      | —             | —           | —            |
