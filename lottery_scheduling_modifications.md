# Lottery Scheduling — File Modifications in xv6

## Overview

Default xv6 uses Round-Robin scheduling — every runnable process gets equal CPU time in order. Lottery scheduling replaces this with a probabilistic approach where each process holds tickets, and a random draw determines who runs next. More tickets = higher chance of getting the CPU.

---

## Files Modified

### 1. proc.h

**Status:** Modified

Added one field to `struct proc`:

```c
int tickets;
```

Added function declaration:

```c
int settickets(int pid, int tickets);
```

**Purpose:** Every process now carries a ticket count. The declaration tells the rest of the kernel that `settickets()` exists in `proc.c`.

---

### 2. proc.c

**Status:** Modified — 4 changes

#### Change 1 — Default tickets in `allocproc()`

```c
found:
  p->state = EMBRYO;
  p->tickets = 10;   // ADDED
  p->pid = nextpid++;
```

**Purpose:** Every new process starts with 10 tickets by default, so all processes begin equal.

---

#### Change 2 — `rand_range()` function added

```c
int rand_range(int max) {
  static unsigned int z1 = 12345, z2 = 45678, z3 = 78901, z4 = 23456;
  unsigned int b;
  b  = ((z1 << 6) ^ z1) >> 13;
  z1 = ((z1 & 4294967294U) << 18) ^ b;
  // ... more bit manipulation ...
  return (z1 ^ z2 ^ z3 ^ z4) % max;
}
```

**Purpose:** The kernel has no standard library, so no built-in `rand()`. This is a custom pseudo-random number generator to support the lottery draw.

---

#### Change 3 — `scheduler()` replaced (Round-Robin → Lottery)

Original Round-Robin:
```c
// just loop through all processes, run each one in order
for(p = ...) {
  if(p->state != RUNNABLE) continue;
  p->state = RUNNING;
  swtch(...);
}
```

New Lottery Scheduler:
```c
// Step 1: count total tickets of all RUNNABLE processes
int total_tickets = 0;
for(p = ...) {
  if(p->state == RUNNABLE)
    total_tickets += p->tickets;
}

// Step 2: pick a random winning ticket number
long winner = random_at_most(total_tickets - 1);

// Step 3: walk the list, accumulate, find the winner
int counter = 0;
for(p = ...) {
  if(p->state != RUNNABLE) continue;
  counter += p->tickets;
  if(counter > winner) {
    // this process won, run it
    p->state = RUNNING;
    swtch(...);
    break;
  }
}
```

**Purpose:** Core of lottery scheduling. Instead of equal turns, each process wins proportional to its ticket count.

Example: 3 processes with 10, 20, 30 tickets. Total = 60. Random number = 25.
- Process A: counter = 10, 10 > 25? No
- Process B: counter = 30, 30 > 25? Yes → Process B wins (had 33% chance)

---

#### Change 4 — `settickets()` kernel function added

```c
int settickets(int pid, int tickets) {
  if(tickets <= 0) return -1;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      p->tickets = tickets;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
```

**Purpose:** Kernel-side function that modifies a process's ticket count by searching the process table by PID.

---

### 3. sysproc.c

**Status:** Modified

Added system call handler at the bottom:

```c
int sys_settickets(void) {
  int n;
  if(argint(0, &n) < 0)   // safely read argument from user stack
    return -1;
  myproc()->tickets = n;   // set tickets for current process
  return 0;
}
```

**Purpose:** Bridge between user space and kernel. When a user program calls `settickets(10)`, the kernel routes it here. `argint(0, &n)` safely reads the argument. `myproc()` refers to the currently running process.

---

### 4. syscall.h

**Status:** Modified

Added:

```c
#define SYS_settickets 22
```

**Purpose:** Every syscall needs a unique ID number. The user program puts this number in the `%eax` register before triggering the interrupt. The kernel reads it to know which syscall was requested.

---

### 5. syscall.c

**Status:** Modified — 2 additions

Added extern declaration:

```c
extern int sys_settickets(void);
```

Added to dispatch table:

```c
[SYS_settickets] sys_settickets,
```

**Purpose:** The extern tells the compiler this function exists in another file (sysproc.c). The dispatch table entry means when syscall number 22 fires, call `sys_settickets()`.

> Note: `sys_settickets` appears twice in the dispatch table — that is a bug but does not break anything since both entries point to the same function.

---

### 6. usys.S

**Status:** Modified

Added:

```asm
SYSCALL(settickets)
```

**Purpose:** Generates the user-space assembly stub for `settickets()`. When called by a user program, it:
1. Puts `22` (SYS_settickets) into `%eax`
2. Fires `INT T_SYSCALL` to enter kernel mode
3. Returns the result

Without this, user programs cannot call `settickets()` at all.

---

### 7. user.h

**Status:** Modified

Added:

```c
int settickets(int, int);
```

**Purpose:** Declaration that lets user programs include and call `settickets()`. Without this, the compiler would not know the function exists or what arguments it takes.

---

### 8. trap.c

**Status:** NOT modified

Important existing code:

```c
if(myproc() && myproc()->state == RUNNING &&
   tf->trapno == T_IRQ0+IRQ_TIMER)
  yield();
```

**Purpose:** Every timer tick, this forces the running process to yield the CPU, which lets the scheduler run the lottery again. This is what makes lottery scheduling work repeatedly. This behavior already existed and was not changed.

---

## New Files Added

### 9. rand.c

```c
static unsigned short lfsr = 0xACE1u;

unsigned short rand(void) {
  // LFSR (Linear Feedback Shift Register) algorithm
  // generates pseudo-random 16-bit numbers
}

long random_at_most(long max) {
  // returns random number between 0 and max inclusive
  // uses rejection sampling to avoid modulo bias
}
```

**Purpose:** Custom random number generator for the kernel. The scheduler calls `random_at_most(total_tickets - 1)` to pick the winning ticket number.

---

### 10. rand.h

```c
#ifndef RAND_H
#define RAND_H

long random_at_most(long max);

#endif
```

**Purpose:** Header file so other kernel files (like proc.c) can include and use `random_at_most()`.

---

### 11. testlottery.c

```c
int main(void) {
  int pid = getpid();
  settickets(pid, 10);        // give self 10 tickets

  int child = fork();
  if(child == 0) {
    settickets(getpid(), 2);  // child gets 2 tickets
    // run busy loop
    exit();
  } else {
    // parent runs busy loop with 10 tickets
    wait();
  }
  exit();
}
```

**Purpose:** Test program to verify the scheduler works. Creates two processes with different ticket counts (10 vs 2) to observe proportional CPU allocation.

---

## Complete Flow

```
user calls settickets(pid, 10)
        |
usys.S stub → puts 22 in %eax → INT instruction
        |
syscall.c dispatch table → calls sys_settickets()
        |
sysproc.c sys_settickets() → sets myproc()->tickets = 10
        |
timer fires → trap.c → yield() called
        |
scheduler() in proc.c runs
        |
counts total tickets → picks random number → finds winner → runs it
```

---

## Summary Table

| File | Status | What Changed |
|---|---|---|
| proc.h | Modified | Added `int tickets` field + `settickets` declaration |
| proc.c | Modified | Default tickets in allocproc, new lottery scheduler, settickets function, rand_range |
| sysproc.c | Modified | Added `sys_settickets` handler |
| syscall.h | Modified | Added `#define SYS_settickets 22` |
| syscall.c | Modified | Added extern + dispatch table entry |
| usys.S | Modified | Added `SYSCALL(settickets)` |
| user.h | Modified | Added `settickets` declaration |
| trap.c | Not modified | Already handles timer-based yielding |
| rand.c | New file | Random number generator for lottery |
| rand.h | New file | Header for rand.c |
| testlottery.c | New file | Test program |

---

## What Is Missing (Professor's Requirement)

The professor asked for dynamic ticket adjustment based on process behavior:

| Behavior | Expected Action |
|---|---|
| Process uses full time slice (timer forces yield) | Lose tickets — it is CPU-hungry |
| Process yields voluntarily before slice ends | Gain tickets — it is being cooperative |

This is **not implemented**. Currently `yield()` is called in both cases with no way to distinguish between them. A flag like `int preempted` in `struct proc` would be needed to tell them apart, then adjust tickets accordingly in `trap.c` and `yield()`.
