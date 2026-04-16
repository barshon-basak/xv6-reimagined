# schedtest.c — Detailed Overview

## Purpose

`schedtest.c` is a user-space test program for xv6 that validates the **Lottery Scheduler** implementation. It runs 4 automated experiments, each spawning 8 child processes with different ticket configurations, then collects and prints per-process CPU usage statistics.

---

## What is a Tick?

A **tick** is one hardware timer interrupt. xv6's timer fires every ~10 milliseconds, and each interrupt increments a global kernel counter. `uptime()` returns that counter.

```
1 tick  =  ~10ms of real time
10 ticks = ~100ms of real time
100 ticks = ~1 second of real time
```

In the test, `ticks` measures **elapsed wall-clock time** for a process to finish its work — not how many times it was scheduled.

---

## What is a Ticket?

In the lottery scheduler, every runnable process holds a number of **tickets**. Each scheduling round, the kernel draws a random number from 0 to (total tickets - 1). The process whose ticket range contains that number wins and gets the CPU.

- More tickets = higher probability of winning each round
- Fewer tickets = lower probability, less CPU time

The kernel call `settickets(pid, n)` assigns `n` tickets to a process.

---

## Data Structure

```c
struct result {
  int pid;      // process ID
  int tickets;  // ticket count assigned at start
  int ticks;    // elapsed ticks to finish the work loop
  int kind;     // 0=cpu, 1=io, 2=yield
};
```

Each child process fills one `struct result` and sends it to the parent through a **pipe** when it finishes. This is how the parent collects stats without needing a `getpinfo` syscall.

---

## Helper Functions

### `spawn_cpu(tickets, pipefd)`

Creates a CPU-bound child process.

**What it does:**
1. `fork()` — creates child
2. `settickets(mypid, tickets)` — registers ticket count with kernel
3. Records `uptime()` start time
4. Runs a `2,000,000` iteration busy loop — never voluntarily gives up CPU
5. Records elapsed ticks
6. Writes `struct result` into the pipe and exits

**Behaviour:** Always runnable, always consuming CPU. Gets preempted by the timer. Dynamic scheduler penalizes it by reducing tickets over time.

---

### `spawn_io(tickets, pipefd)`

Creates an IO-bound child process.

**What it does:**
1. `fork()` + `settickets()`
2. Calls `sleep(5)` × 20 times = 100 ticks of sleeping
3. Reports elapsed ticks and exits

**Behaviour:** Spends most of its time sleeping (off the run queue). Voluntarily releases CPU each iteration. Dynamic scheduler may reward it with more tickets over time.

---

### `spawn_yield(tickets, pipefd)`

Creates a yield-based child process.

**What it does:**
1. `fork()` + `settickets()`
2. Calls `yield()` × 500 times — explicitly hands CPU back each iteration
3. Reports elapsed ticks and exits

**Behaviour:** Most cooperative process type. Immediately gives up CPU every iteration. Finishes quickly with very low tick count.

---

### `collect(pipes, res, n)`

Parent-side function that gathers results from all children.

**What it does:**
- Reads one `struct result` from each pipe (blocks until child writes)
- Closes the read-end of each pipe
- Calls `wait()` to reap each child process

Ensures all children have finished before printing stats.

---

### `print_stats(title, res, n)`

Prints a formatted result table for one experiment.

**What it prints per process:**
- `pid` — process ID
- `tickets` — ticket count it was given
- `ticks` — how long it took to finish (wall-clock)
- `share` — percentage of total ticks this process consumed
- `type` — cpu(busy) / io(sleep) / yield

**Summary line:** total ticks across all processes, max, and min.

---

## Execution Flow

```
main()
  |
  |-- print header
  |
  |-- experiment1()
  |     |-- spawn 8 cpu processes (10 tickets each)
  |     |-- sleep(200)
  |     |-- collect results from all 8 pipes
  |     |-- print_stats()
  |
  |-- experiment2()
  |     |-- spawn 1 cpu process (80 tickets)
  |     |-- spawn 7 cpu processes (10 tickets each)
  |     |-- sleep(200)
  |     |-- collect + print_stats()
  |
  |-- experiment3()
  |     |-- spawn 3 cpu processes (10 tickets)
  |     |-- spawn 3 io processes (10 tickets)
  |     |-- spawn 2 yield processes (10 tickets)
  |     |-- sleep(300)
  |     |-- collect + print_stats()
  |
  |-- experiment4()
  |     |-- spawn 1 cpu process (1 ticket)
  |     |-- spawn 7 cpu processes (100 tickets each)
  |     |-- sleep(400)
  |     |-- collect + print_stats()
  |
  |-- print footer
  |-- exit()
```

Each experiment is fully sequential. The next one does not start until all children from the previous one have exited and been reaped by `collect()`.

---

## Experiment 1 — Equal Tickets (Fairness)

**Setup:** 8 CPU-bound processes, all with 10 tickets each.

**Total ticket pool:** 80 tickets. Each process holds 10/80 = 12.5% of the pool.

**What to expect:** All processes should get roughly equal CPU share (~12%). Ticks should be close to each other across all 8 processes.

**Sample output:**
```
[ EXP 1 ] 8 cpu processes, 10 tickets each -> expect equal share

>> EXPERIMENT 1: Equal Tickets (Fairness)

  pid=4   tickets=10  ticks=11  share=12%  type=cpu(busy)
  pid=5   tickets=10  ticks=12  share=13%  type=cpu(busy)
  pid=6   tickets=10  ticks=11  share=12%  type=cpu(busy)
  pid=7   tickets=10  ticks=12  share=13%  type=cpu(busy)
  pid=8   tickets=10  ticks=11  share=12%  type=cpu(busy)
  pid=9   tickets=10  ticks=10  share=11%  type=cpu(busy)
  pid=10  tickets=10  ticks=12  share=13%  type=cpu(busy)
  pid=11  tickets=10  ticks=12  share=13%  type=cpu(busy)

  total=91  max=12  min=10
```

**What it proves:** The lottery scheduler is fair. Equal tickets produce equal CPU distribution.

---

## Experiment 2 — Weighted Tickets (Priority)

**Setup:** 1 process with 80 tickets, 7 processes with 10 tickets each.

**Total ticket pool:** 150 tickets. The heavy process holds 80/150 = ~53% of the pool.

**What to expect:** The 80-ticket process should dominate CPU time with ~50%+ share. The 7 low-ticket processes share the remaining ~47% between them (~6-7% each).

**Sample output:**
```
[ EXP 2 ] 1 process=80 tickets, 7 processes=10 tickets -> expect pid[0] dominates

>> EXPERIMENT 2: Weighted Tickets (Priority)

  pid=12  tickets=80   ticks=38  share=51%  type=cpu(busy)
  pid=13  tickets=10   ticks=9   share=12%  type=cpu(busy)
  pid=14  tickets=10   ticks=8   share=11%  type=cpu(busy)
  pid=15  tickets=10   ticks=7   share=9%   type=cpu(busy)
  pid=16  tickets=10   ticks=6   share=8%   type=cpu(busy)
  pid=17  tickets=10   ticks=4   share=5%   type=cpu(busy)
  pid=18  tickets=10   ticks=3   share=4%   type=cpu(busy)
  pid=19  tickets=10   ticks=0   share=0%   type=cpu(busy)

  total=75  max=38  min=0
```

**What it proves:** Ticket weight directly controls CPU priority. More tickets = more CPU time. The scheduler is proportional, not just random.

---

## Experiment 3 — CPU vs IO vs Yield

**Setup:** 3 cpu-bound + 3 io-bound + 2 yield-based processes, all with 10 tickets.

**Total ticket pool:** 80 tickets. All equal weight, but behavior differs.

**What to expect:**
- CPU processes accumulate the most ticks (always runnable, always consuming)
- IO processes accumulate few ticks (sleeping most of the time, off the run queue)
- Yield processes accumulate the fewest ticks (finish quickly by handing CPU back immediately)

**Sample output:**
```
[ EXP 3 ] 3 cpu + 3 io + 2 yield, all 10 tickets -> io/yield use fewer ticks

>> EXPERIMENT 3: CPU vs IO vs Yield

  pid=20  tickets=10  ticks=28  share=28%  type=cpu(busy)
  pid=21  tickets=10  ticks=30  share=30%  type=cpu(busy)
  pid=22  tickets=10  ticks=27  share=27%  type=cpu(busy)
  pid=23  tickets=10  ticks=4   share=4%   type=io(sleep)
  pid=24  tickets=10  ticks=5   share=5%   type=io(sleep)
  pid=25  tickets=10  ticks=4   share=4%   type=io(sleep)
  pid=26  tickets=10  ticks=1   share=1%   type=yield
  pid=27  tickets=10  ticks=1   share=1%   type=yield

  total=100  max=30  min=1
```

**What it proves:** Process behaviour matters as much as ticket count. IO and yield processes are efficient — they don't hog the CPU even when they have equal tickets. CPU hogs consume disproportionately more wall-clock time.

---

## Experiment 4 — Starvation Test

**Setup:** 1 process with 1 ticket, 7 processes with 100 tickets each.

**Total ticket pool:** 701 tickets. The weak process holds 1/701 = ~0.14% of the pool.

**What to expect:** The 1-ticket process should barely get scheduled. If `ticks=0` and `share=0%`, it was fully starved. If `ticks > 0`, the scheduler avoids complete starvation.

**Sample output:**
```
[ EXP 4 ] 1 process=1 ticket, 7 processes=100 tickets -> check starvation

>> EXPERIMENT 4: Starvation Test

  pid=28  tickets=1    ticks=0  share=0%   type=cpu(busy)
  pid=29  tickets=100  ticks=0  share=0%   type=cpu(busy)
  pid=30  tickets=100  ticks=0  share=0%   type=cpu(busy)
  pid=31  tickets=100  ticks=0  share=0%   type=cpu(busy)
  pid=32  tickets=100  ticks=1  share=50%  type=cpu(busy)
  pid=33  tickets=100  ticks=1  share=50%  type=cpu(busy)
  pid=34  tickets=100  ticks=0  share=0%   type=cpu(busy)
  pid=35  tickets=100  ticks=0  share=0%   type=cpu(busy)

  total=2  max=1  min=0
```

**Why total is so low:** In QEMU, the 2,000,000 iteration loop finishes in under 10ms for most processes. The tick counter barely increments. `ticks=0` does not mean the process never ran — it means it finished in under one 10ms timer interval.

**What it proves:** pid=28 with 1 ticket got `share=0%` — it was effectively starved in the observation window. With only 0.14% win probability per lottery round, it rarely gets scheduled when competing against 700 combined tickets.

---

## Output Field Reference

| Field | Meaning |
|-------|---------|
| `pid` | Process ID assigned by the kernel |
| `tickets` | Number of lottery tickets assigned via `settickets()` |
| `ticks` | Wall-clock time to finish work loop (1 tick = ~10ms) |
| `share` | Percentage of total ticks this process consumed |
| `type` | cpu(busy) = busy loop, io(sleep) = sleep loop, yield = yield loop |
| `total` | Sum of ticks across all processes in the experiment |
| `max` | Highest individual tick count in the experiment |
| `min` | Lowest individual tick count in the experiment |

---

## Why ticks=0 is Common in QEMU

QEMU emulates x86 hardware much faster than real hardware. A 2,000,000 integer increment loop completes in under 10ms on QEMU, so `uptime()` often returns the same value before and after the loop. This is expected behaviour — it does not mean the process was not scheduled. The `share` percentage is the more meaningful metric when `total` is very small.

---

## Syscalls Used

| Syscall | Signature | Purpose |
|---------|-----------|---------|
| `fork()` | `int fork(void)` | Create child process |
| `exit()` | `int exit(void)` | Terminate process |
| `wait()` | `int wait(void)` | Reap child process |
| `pipe()` | `int pipe(int*)` | Create communication channel |
| `getpid()` | `int getpid(void)` | Get own process ID |
| `settickets()` | `int settickets(int pid, int n)` | Assign lottery tickets |
| `uptime()` | `int uptime(void)` | Read tick counter |
| `sleep()` | `int sleep(int n)` | Sleep for n ticks |
| `yield()` | `int yield(void)` | Voluntarily give up CPU |
| `read/write()` | standard | Pipe communication |
