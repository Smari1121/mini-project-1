#!/usr/bin/env python3
"""
MLFQ Timeline Plot Generator for xv6

This script parses MLFQ_LOG lines from xv6 console output and generates
a scatter plot showing processes moving between priority queues over time.

Usage:
  1. Build and run xv6 with MLFQ:
       make clean && make qemu SCHEDULER=MLFQ
  2. Run 'schedulertest' inside xv6
  3. Copy/paste the entire console output into a file, e.g. mlfq_output.txt
  4. Run: python3 plot_mlfq.py mlfq_output.txt

  Alternatively, pipe directly:
    python3 plot_mlfq.py < mlfq_output.txt

The MLFQ_LOG lines are emitted by the kernel scheduler in proc.c:
  MLFQ_LOG <tick> <pid> <queue_level>
"""

import re
import sys
import matplotlib.pyplot as plt

def parse_log(filename=None):
    """Parse MLFQ_LOG lines from file or stdin."""
    data = []
    regex = re.compile(r'MLFQ_LOG\s+(\d+)\s+(\d+)\s+(\d+)')

    if filename:
        f = open(filename, 'r')
    else:
        f = sys.stdin

    for line in f:
        m = regex.search(line)
        if m:
            tick = int(m.group(1))
            pid = int(m.group(2))
            queue = int(m.group(3))
            data.append((tick, pid, queue))

    if filename:
        f.close()

    return data

def plot_mlfq(data, output='mlfq_plot.png', watermark='keshvi.agrawal'):
    """Generate the MLFQ timeline scatter plot."""
    if not data:
        print("No MLFQ_LOG data found! Make sure you ran with SCHEDULER=MLFQ")
        print("and the console output contains lines like: MLFQ_LOG 10 3 0")
        sys.exit(1)

    print(f"Collected {len(data)} data points.")

    # Group by PID
    pids = sorted(set(d[1] for d in data))
    colors = plt.cm.get_cmap('tab10', max(len(pids), 1))

    # Process type labels (matching schedulertest.c ordering)
    # PIDs 1=init, 2=sh, 3+=test processes
    labels = {}
    for i, pid in enumerate(pids):
        if i == 0:
            labels[pid] = f'PID {pid} (CPU-hog)'
        elif i == 1:
            labels[pid] = f'PID {pid} (CPU-hog)'
        elif i == 2:
            labels[pid] = f'PID {pid} (I/O-bound)'
        elif i == 3:
            labels[pid] = f'PID {pid} (Mixed)'
        elif i == 4:
            labels[pid] = f'PID {pid} (I/O-bound)'
        else:
            labels[pid] = f'PID {pid}'

    fig, ax = plt.subplots(figsize=(12, 6))

    for i, pid in enumerate(pids):
        pid_data = [d for d in data if d[1] == pid]
        ticks = [d[0] for d in pid_data]
        queues = [d[2] for d in pid_data]
        ax.scatter(ticks, queues, label=labels.get(pid, f'PID {pid}'),
                   color=colors(i), s=20, alpha=0.7, edgecolors='none')

    ax.set_yticks([0, 1, 2, 3])
    ax.set_yticklabels(['Queue 0\n(highest)', 'Queue 1', 'Queue 2', 'Queue 3\n(lowest)'])
    ax.set_xlabel('Time (ticks)', fontsize=12)
    ax.set_ylabel('Priority Queue', fontsize=12)
    ax.set_title('MLFQ Queue Assignment over Time', fontsize=14)
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(True, linestyle='--', alpha=0.4)
    ax.invert_yaxis()  # Queue 0 (highest priority) at top

    # Mark priority boost boundaries (every 48 ticks)
    max_tick = max(d[0] for d in data)
    for t in range(48, max_tick + 1, 48):
        ax.axvline(x=t, color='red', linestyle=':', alpha=0.3, linewidth=1)
    # Add a single label for boost lines
    ax.axvline(x=-100, color='red', linestyle=':', alpha=0.3, linewidth=1,
               label='Priority Boost (48 ticks)')

    # Re-draw legend to include boost line
    handles, lbls = ax.get_legend_handles_labels()
    ax.legend(handles, lbls, loc='upper right', fontsize=9)

    # Watermark
    ax.text(0.95, 0.02, watermark,
            ha='right', va='bottom',
            transform=ax.transAxes,
            fontsize=11, color='gray', alpha=0.6,
            style='italic')

    plt.tight_layout()
    plt.savefig(output, dpi=300, bbox_inches='tight')
    print(f"Saved plot to {output}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        data = parse_log(sys.argv[1])
    else:
        print("Reading from stdin... (paste output, then Ctrl+D)")
        data = parse_log()

    plot_mlfq(data)
