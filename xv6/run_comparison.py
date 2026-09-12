import pexpect
import re
import sys

schedulers = ['FIFO', 'RR', 'MLFQ']
results = {}

for sched in schedulers:
    print(f"\n--- Running {sched} ---")
    if sched == 'RR':
        cmd = 'make clean && make qemu'
    else:
        cmd = f'make clean && make qemu SCHEDULER={sched}'
        
    child = pexpect.spawn('bash', ['-c', cmd], encoding='utf-8', timeout=60)
    child.expect(r'\$ ')
    child.sendline('schedulertest')
    
    # METRICS PID 3: Turnaround=200 Wait=50 Response=10
    regex = re.compile(r'METRICS PID \d+: Turnaround=(\d+) Wait=(\d+) Response=(\d+)')
    
    t_tot, w_tot, r_tot = 0, 0, 0
    count = 0
    
    try:
        while True:
            idx = child.expect([r'METRICS PID', r'schedulertest: all done'], timeout=20)
            if idx == 1:
                break
            
            # Matched METRICS
            line = child.readline().strip()
            # print(f"Raw line: {line}")
            m = re.search(r'Turnaround=(\d+) Wait=(\d+) Response=(\d+)', line)
            if m:
                t = int(m.group(1))
                w = int(m.group(2))
                r = int(m.group(3))
                t_tot += t
                w_tot += w
                r_tot += r
                count += 1
                
    except pexpect.TIMEOUT:
        print("Timeout waiting for schedulertest to finish")
    finally:
        try:
            child.send('\x01x')
            child.close()
        except:
            pass
            
    if count > 0:
        results[sched] = {
            'turnaround': t_tot / count,
            'wait': w_tot / count,
            'response': r_tot / count
        }
    else:
        print(f"No metrics found for {sched}!")
        results[sched] = {'turnaround': 0, 'wait': 0, 'response': 0}

print("\n\n=== COMPARISON RESULTS ===")
print(f"{'Scheduler':<12} | {'Avg Turnaround':<15} | {'Avg Wait':<10} | {'Avg Response':<12}")
print("-" * 57)
for sched in schedulers:
    r = results[sched]
    print(f"{sched:<12} | {r['turnaround']:<15.2f} | {r['wait']:<10.2f} | {r['response']:<12.2f}")
