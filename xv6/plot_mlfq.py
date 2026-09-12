import pexpect
import re
import time
import sys
import matplotlib.pyplot as plt

print("Starting xv6 MLFQ... please wait.")
child = pexpect.spawn('bash', ['-c', 'make clean && make qemu SCHEDULER=MLFQ'], encoding='utf-8', timeout=60)
child.expect(r'\$ ')
print("xv6 booted. Running schedulertest...")

child.sendline('schedulertest')

data = []
done = False

# Regex to match procdump output line
# Example: 4 run    schedulertest | prio: 0, ticks: 0, boost in: 48, global_tick: 125
regex = re.compile(r'^(\d+)\s+(run|runble|sleep )\s+\w+\s+\|\s+prio:\s+(\d+).*global_tick:\s+(\d+)')

try:
    while not done:
        child.send('\x10') # Ctrl+P
        idx = child.expect([r'schedulertest: all done', r'\n'], timeout=5)
        
        if idx == 0:
            done = True
            break
            
        line = child.before.strip()
        m = regex.match(line)
        if m:
            pid = int(m.group(1))
            if pid > 2:  # Skip init and sh
                prio = int(m.group(3))
                tick = int(m.group(4))
                data.append((tick, pid, prio))
        
        # Don't spin too fast
        time.sleep(0.05)
        
except pexpect.TIMEOUT:
    print("Timeout waiting for schedulertest to finish")
    child.terminate(force=True)
except Exception as e:
    print(f"Error: {e}")

# Try to exit cleanly
try:
    child.send('\x01x') # Ctrl-a x to quit qemu
    child.close()
except:
    pass

if not data:
    print("No data collected!")
    sys.exit(1)

print(f"Collected {len(data)} data points. Generating plot...")

# Group by PID
pids = sorted(list(set([d[1] for d in data])))
colors = plt.cm.get_cmap('tab10', len(pids))

plt.figure(figsize=(10, 6))

for i, pid in enumerate(pids):
    pid_data = [d for d in data if d[1] == pid]
    ticks = [d[0] for d in pid_data]
    prios = [d[2] for d in pid_data]
    plt.scatter(ticks, prios, label=f'PID {pid}', color=colors(i), s=15, alpha=0.7)

plt.yticks([0, 1, 2, 3], ['Queue 0', 'Queue 1', 'Queue 2', 'Queue 3'])
plt.xlabel("Global Ticks")
plt.ylabel("Queue Priority")
plt.title("MLFQ Queue Selection over Time")
plt.legend()
plt.grid(True, linestyle='--', alpha=0.5)
plt.gca().invert_yaxis() # 0 is highest priority

# WATERMARK
plt.text(
    0.95, 0.95, "somsuta.gandhi",
    ha='right', va='top',
    transform=plt.gca().transAxes,
    fontsize=10, color="gray", alpha=0.7
)

plt.savefig('mlfq_plot.png', dpi=300, bbox_inches='tight')
print("Saved plot to mlfq_plot.png")
