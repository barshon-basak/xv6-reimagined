# Lottery Scheduler Fix - Dynamic Ticket Adjustment Issue

## Problem Identified

The lottery scheduler tests were showing completely incorrect results:

### Test Results Before Fix:
- **Experiment 1** (Equal tickets): Processes with 10 tickets each got 2%-32% share (should be ~12.5% each)
- **Experiment 2** (Priority): Process with 80 tickets got only 6%, while 10-ticket processes got up to 33%
- **Experiment 4** (Starvation): Process with 1 ticket got same share as processes with 100 tickets

### Root Cause

The `yield()` function in `proc.c` had **dynamic ticket adjustment** logic that was modifying ticket counts during execution:

```c
if(p->preempted){
    // CPU-bound processes lose tickets when preempted by timer
    if(p->tickets > 1)
        p->tickets--;
} else {
    // IO-bound processes gain tickets when voluntarily yielding
    if(p->tickets < 100)
        p->tickets++;
}
```

**Why this broke the tests:**

1. **CPU-bound processes** (doing busy loops) were constantly being preempted by timer interrupts
   - Each preemption reduced their tickets by 1
   - A process starting with 80 tickets would quickly drop to 1 ticket
   - This explains why high-ticket processes got low CPU shares

2. **IO-bound and yield processes** were voluntarily giving up the CPU
   - Each voluntary yield increased their tickets by 1 (up to 100)
   - A process starting with 10 tickets would quickly reach 100 tickets
   - This explains why low-ticket processes got high CPU shares

3. **Result**: The carefully set ticket values (10, 80, 100) were being overridden by dynamic adjustment, making lottery scheduling tests meaningless

## Solution Applied

**Disabled dynamic ticket adjustment in `yield()` function** (proc.c line 405)

The adjustment logic has been commented out with a clear explanation. Now:
- Ticket counts remain exactly as set by `settickets()`
- CPU-bound processes keep their assigned tickets
- IO-bound processes keep their assigned tickets
- Lottery scheduler operates purely on the configured ticket values

## Expected Results After Fix

With dynamic adjustment disabled, you should now see:

### Experiment 1 (Equal Tickets - Fairness)
- 8 processes, 10 tickets each
- **Expected**: Each gets ~12.5% CPU share (±2-3% variance is normal)

### Experiment 2 (Weighted Tickets - Priority)
- 1 process with 80 tickets, 7 with 10 tickets
- Total: 150 tickets
- **Expected**: 
  - 80-ticket process: ~53% share (80/150)
  - Each 10-ticket process: ~6.7% share (10/150)

### Experiment 3 (CPU vs IO vs Yield)
- All have 10 tickets
- **Expected**: 
  - CPU processes will show high tick counts (they run continuously)
  - IO processes will show lower tick counts (they sleep)
  - Yield processes will show lower tick counts (they yield frequently)
  - When RUNNABLE, all should get equal lottery chances

### Experiment 4 (Starvation Test)
- 1 process with 1 ticket, 7 with 100 tickets
- Total: 701 tickets
- **Expected**:
  - 1-ticket process: ~0.14% share (1/701)
  - Each 100-ticket process: ~14.3% share (100/701)
  - The 1-ticket process should still run (no starvation), just very rarely

## Next Steps

1. **Rebuild the kernel**: `make clean && make`
2. **Run the tests**: `schedtest`
3. **Verify results** match expected distributions above

## Note on Dynamic Ticket Adjustment

The dynamic adjustment feature was an attempt to implement **adaptive scheduling** (rewarding IO-bound processes, penalizing CPU-bound ones). However:

- It interferes with testing the core lottery scheduler
- It makes ticket values unpredictable
- It's not part of the standard lottery scheduling algorithm

If you want this feature in the future, consider:
- Adding a per-process flag to enable/disable adjustment
- Using separate "base tickets" and "bonus tickets" 
- Only adjusting within a limited range (e.g., ±20% of base)
