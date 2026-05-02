# Lottery Scheduler Testing Guide

## Quick Start

```bash
make clean && make
make qemu-nox
# In QEMU:
schedtest
```

## What the Tests Measure

The tests now measure **actual CPU time** (runticks) each process receives, not wall-clock time.

- **runticks** = number of timer interrupts that occurred while the process was RUNNING
- This directly reflects how often the lottery scheduler picked that process
- More tickets → picked more often → more runticks

## Understanding the Results

### Experiment 1: Equal Tickets (Fairness Test)

**Setup:** 8 CPU-bound processes, each with 10 tickets

**What to look for:**
- All processes should get approximately equal CPU time
- Expected: ~12.5% each (100% / 8 processes)
- Acceptable variance: ±2-3% due to randomness

**Example good result:**
```
pid=4  tickets=10  ticks=52  share=13%  type=cpu(busy)
pid=5  tickets=10  ticks=48  share=12%  type=cpu(busy)
pid=6  tickets=10  ticks=51  share=12%  type=cpu(busy)
pid=7  tickets=10  ticks=49  share=12%  type=cpu(busy)
pid=8  tickets=10  ticks=50  share=12%  type=cpu(busy)
pid=9  tickets=10  ticks=53  share=13%  type=cpu(busy)
pid=10 tickets=10  ticks=47  share=11%  type=cpu(busy)
pid=11 tickets=10  ticks=50  share=12%  type=cpu(busy)
total=400  max=53  min=47
```

**Red flags:**
- One process getting >20% or <5%
- Max/min difference > 15 ticks
- Total ticks much less than 400 (processes finishing too early)

---

### Experiment 2: Weighted Tickets (Priority Test)

**Setup:** 1 process with 80 tickets, 7 processes with 10 tickets each

**What to look for:**
- The 80-ticket process should dominate CPU time
- Expected: 80-ticket process gets ~53% (80/150 total tickets)
- Expected: Each 10-ticket process gets ~6.7% (10/150 total tickets)

**Example good result:**
```
pid=12 tickets=80  ticks=213  share=53%  type=cpu(busy)
pid=13 tickets=10  ticks=27   share=6%   type=cpu(busy)
pid=14 tickets=10  ticks=26   share=6%   type=cpu(busy)
pid=15 tickets=10  ticks=28   share=7%   type=cpu(busy)
pid=16 tickets=10  ticks=27   share=6%   type=cpu(busy)
pid=17 tickets=10  ticks=26   share=6%   type=cpu(busy)
pid=18 tickets=10  ticks=27   share=6%   type=cpu(busy)
pid=19 tickets=10  ticks=26   share=6%   type=cpu(busy)
total=400  max=213  min=26
```

**Red flags:**
- 80-ticket process getting <40% or >65%
- Any 10-ticket process getting more than the 80-ticket process
- 10-ticket processes getting >10% each

---

### Experiment 3: CPU vs IO vs Yield

**Setup:** 3 CPU-bound, 3 IO-bound, 2 yield-based processes (all with 10 tickets)

**What to look for:**
- CPU processes: High runticks (always competing for CPU)
- IO processes: Very high ticks (500 each, mostly sleeping)
- Yield processes: Low runticks (voluntarily giving up CPU)

**Example good result:**
```
pid=20 tickets=10  ticks=120  share=7%   type=cpu(busy)
pid=21 tickets=10  ticks=125  share=7%   type=cpu(busy)
pid=22 tickets=10  ticks=118  share=7%   type=cpu(busy)
pid=23 tickets=10  ticks=500  share=32%  type=io(sleep)
pid=24 tickets=10  ticks=500  share=32%  type=io(sleep)
pid=25 tickets=10  ticks=500  share=32%  type=io(sleep)
pid=26 tickets=10  ticks=15   share=0%   type=yield
pid=27 tickets=10  ticks=16   share=1%   type=yield
total=1894  max=500  min=15
```

**Key insight:** IO processes show 500 ticks because that's wall-clock time spent sleeping. The CPU processes should split the actual CPU time roughly equally.

**Red flags:**
- CPU processes with vastly different runticks (>20% variance)
- Yield processes getting more runticks than CPU processes

---

### Experiment 4: Starvation Test

**Setup:** 1 process with 1 ticket, 7 processes with 100 tickets each

**What to look for:**
- The 1-ticket process should get SOME CPU time (no starvation)
- Expected: 1-ticket process gets ~0.14% (1/701 total tickets)
- Expected: Each 100-ticket process gets ~14.3% (100/701 total tickets)

**Example good result:**
```
pid=28 tickets=1   ticks=1    share=0%   type=cpu(busy)
pid=29 tickets=100 ticks=57   share=14%  type=cpu(busy)
pid=30 tickets=100 ticks=58   share=14%  type=cpu(busy)
pid=31 tickets=100 ticks=56   share=14%  type=cpu(busy)
pid=32 tickets=100 ticks=57   share=14%  type=cpu(busy)
pid=33 tickets=100 ticks=58   share=14%  type=cpu(busy)
pid=34 tickets=100 ticks=57   share=14%  type=cpu(busy)
pid=35 tickets=100 ticks=56   share=14%  type=cpu(busy)
total=400  max=58  min=1
```

**Key insight:** The 1-ticket process gets very little CPU time (maybe just 1-2 ticks), but it DOES run. This proves no starvation.

**Red flags:**
- 1-ticket process getting 0 ticks (starvation!)
- 1-ticket process getting >2% share
- 100-ticket processes not getting roughly equal shares

---

## Debugging Bad Results

### If all processes get equal time regardless of tickets:

**Problem:** Lottery scheduler not working
**Check:**
1. Is `random_at_most()` returning varied numbers?
2. Is the scheduler actually using the lottery algorithm?
3. Enable debug output in scheduler (uncomment cprintf lines in proc.c)

### If processes finish too quickly (total < 300 ticks):

**Problem:** CPU work loop too short
**Fix:** Increase loop iterations in spawn_cpu() in schedtest.c

### If results are wildly random (no pattern):

**Problem:** Random number generator might have issues
**Check:** The LFSR in rand.c should produce varied output

### If one process always dominates:

**Problem:** Possible bug in lottery selection logic
**Check:** The counter accumulation in scheduler() (proc.c line ~350)

---

## Advanced: Enabling Debug Output

To see every lottery draw, edit `proc.c` line ~355:

```c
// Change this:
// if(p->pid > 2){
//   round++;
//   cprintf("[round %d] total=%d winner=%d -> pid %d (%s) tickets=%d\n",
//           round, total_tickets, winner, p->pid, p->name, p->tickets);
// }

// To this:
if(p->pid > 2){
  round++;
  cprintf("[round %d] total=%d winner=%d -> pid %d (%s) tickets=%d\n",
          round, total_tickets, winner, p->pid, p->name, p->tickets);
}
```

This will print every scheduling decision. You'll see:
- Which process won each lottery draw
- The winning ticket number
- Total tickets in the pool

**Warning:** This produces A LOT of output. Use only for debugging specific issues.

---

## Statistical Notes

Lottery scheduling is **probabilistic**, not deterministic:
- Small variations (±2-3%) are normal and expected
- Run the test multiple times to see average behavior
- Larger sample sizes (more ticks) give more accurate results
- The law of large numbers ensures fairness over time

**Good variance:** Process with 10 tickets gets 11-14% instead of 12.5%
**Bad variance:** Process with 10 tickets gets 5% or 20%
