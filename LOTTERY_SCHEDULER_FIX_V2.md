# Lottery Scheduler Fix V2 - Proper CPU Time Measurement

## Root Cause Analysis

After disabling dynamic ticket adjustment, the tests still showed incorrect results. The deeper issue was **how CPU time was being measured**.

### Original Problem

The test was measuring **elapsed wall-clock time** (uptime delta), not **actual CPU time received**:

```c
int start = uptime();
// do work...
int elapsed = uptime() - start;  // WRONG: wall-clock time, not CPU time
```

**Why this failed:**
- All processes ran for the same wall-clock duration (e.g., 400 ticks)
- They all reported ~400 ticks regardless of how much CPU they actually got
- A process with 1 ticket and a process with 100 tickets would both report 400 ticks
- The lottery scheduler was working, but we weren't measuring the right thing!

### What We Actually Need to Measure

**CPU time** = number of timer ticks the process was actually RUNNING (not waiting/sleeping)

In a lottery scheduler:
- Process with more tickets gets scheduled more often
- Gets more timer ticks while in RUNNING state
- Should accumulate more CPU time

## Solution Implemented

### 1. Added `runticks` Counter to Process Structure

**File: proc.h**
```c
struct proc {
  // ... existing fields ...
  int runticks;  // Number of timer ticks this process has been running
};
```

### 2. Initialize `runticks` in Process Allocation

**File: proc.c (allocproc function)**
```c
p->runticks = 0;
```

### 3. Increment `runticks` on Every Timer Interrupt

**File: trap.c (timer interrupt handler)**
```c
if(myproc() && myproc()->state == RUNNING &&
   tf->trapno == T_IRQ0+IRQ_TIMER){
  myproc()->runticks++;  // Count timer ticks while running
  myproc()->preempted = 1;
  yield();
}
```

**Key insight:** This counts ONLY the timer ticks when the process is actively running, which is exactly what we need to measure lottery scheduler fairness.

### 4. Added `getrunticks()` System Call

**Files modified:**
- `sysproc.c`: Added `sys_getrunticks()` handler
- `syscall.h`: Added `#define SYS_getrunticks 26`
- `syscall.c`: Added extern declaration and dispatch table entry
- `usys.S`: Added `SYSCALL(getrunticks)` stub
- `user.h`: Added `int getrunticks(void);` declaration

**Usage:**
```c
int cpu_time = getrunticks();  // Returns number of timer ticks while running
```

### 5. Updated Test to Use `getrunticks()`

**File: schedtest.c**
```c
void spawn_cpu(int tickets, int pipefd[2]) {
  // ... setup ...
  
  int start = uptime();
  // Run for 400 ticks of wall-clock time
  while(uptime() - start < 400){
    // Do CPU work
    volatile int x = 0;
    for (int j = 0; j < 100000; j++) x++;
  }
  
  // Get ACTUAL CPU time received (not wall-clock time)
  int cpu_time = getrunticks();
  
  // Report cpu_time, which reflects lottery scheduler fairness
}
```

## Expected Results After Fix

Now the tests will show **actual CPU time** proportional to tickets:

### Experiment 1 (Equal Tickets - Fairness)
- 8 processes, 10 tickets each
- Total: 80 tickets
- **Expected**: Each gets ~12.5% of total CPU time (±2-3% variance)
- If total CPU time is 400 ticks, each should get ~50 ticks

### Experiment 2 (Weighted Tickets - Priority)
- 1 process with 80 tickets, 7 with 10 tickets
- Total: 150 tickets
- **Expected**:
  - 80-ticket process: ~53% CPU time (80/150)
  - Each 10-ticket process: ~6.7% CPU time (10/150)
- If total is 400 ticks:
  - 80-ticket process: ~213 ticks
  - Each 10-ticket process: ~27 ticks

### Experiment 3 (CPU vs IO vs Yield)
- All have 10 tickets
- **Expected**:
  - CPU processes: High runticks (they're always RUNNABLE)
  - IO processes: Low runticks (they sleep, not RUNNABLE)
  - Yield processes: Medium runticks (they yield but come back quickly)

### Experiment 4 (Starvation Test)
- 1 process with 1 ticket, 7 with 100 tickets
- Total: 701 tickets
- **Expected**:
  - 1-ticket process: ~0.14% CPU time (1/701)
  - Each 100-ticket process: ~14.3% CPU time (100/701)
- If total is 400 ticks:
  - 1-ticket process: ~0.6 ticks (should still run, no starvation!)
  - Each 100-ticket process: ~57 ticks

## Key Changes Summary

| File | Change | Purpose |
|------|--------|---------|
| `proc.h` | Added `int runticks` field | Track actual CPU time per process |
| `proc.c` | Initialize `runticks = 0` in allocproc | Start counter at zero |
| `trap.c` | Increment `runticks` on timer interrupt | Count CPU time |
| `sysproc.c` | Added `sys_getrunticks()` | System call handler |
| `syscall.h` | Added `#define SYS_getrunticks 26` | Syscall number |
| `syscall.c` | Added extern + dispatch entry | Register syscall |
| `usys.S` | Added `SYSCALL(getrunticks)` | User-space stub |
| `user.h` | Added `int getrunticks(void);` | User-space declaration |
| `schedtest.c` | Use `getrunticks()` instead of uptime delta | Measure actual CPU time |

## Rebuild and Test

```bash
make clean
make
make qemu-nox
# In QEMU:
schedtest
```

## Why This Fix Works

1. **Accurate Measurement**: `runticks` counts only timer ticks when process is RUNNING
2. **Proportional to Tickets**: More tickets → scheduled more often → more runticks
3. **Independent of Work**: Doesn't matter how much work the process does, only how much CPU time it gets
4. **Fair Comparison**: All processes compete for the same 400 ticks of wall-clock time, but get different amounts of CPU time based on their tickets

This is the correct way to test a lottery scheduler!
