# Project Report: Implementing Lottery Scheduler and Mailbox IPC in xv6

## 1. Project Goals

### Features Implemented

This project implements two major enhancements to the xv6 operating system:

1. **Lottery Scheduler**: A proportional-share CPU scheduling algorithm where processes receive CPU time proportional to their ticket allocation, replacing xv6's default round-robin scheduler.

2. **Mailbox IPC**: A message-oriented inter-process communication mechanism providing atomic message passing through 16 dedicated channels (0-15), each supporting messages up to 128 bytes.

### Default xv6 Implementation Strategy

**Process Scheduling:** xv6 uses a simple round-robin scheduler that treats all runnable processes equally, iterating through the process table and giving each `RUNNABLE` process an equal time slice. This provides fairness but no mechanism for prioritization or proportional resource allocation.

**Inter-Process Communication:** xv6's default IPC uses pipes—byte-stream oriented channels integrated into the file system layer. Pipes require file descriptor allocation, file table management, and complex circular buffer logic (512-byte buffer). They provide no message boundaries, requiring applications to implement their own framing protocols.

### Improvements to xv6

**Lottery Scheduler:** Transforms xv6 into a priority-aware system supporting proportional-share scheduling. Processes with more tickets receive proportionally more CPU time while maintaining fairness through randomization. The implementation includes infrastructure for dynamic ticket adjustment (currently disabled for testing stability).

**Mailbox IPC:** Introduces lightweight, message-oriented IPC bypassing the file system layer. Guarantees atomic message delivery with simple binary state (full/empty) and blocking semantics. Accessed via integer channel IDs rather than file descriptors, reducing overhead and simplifying multiplexed communication.

---

## 2. Modifications

### Implementation Challenges

The main challenges included understanding xv6's scheduling infrastructure and context switching mechanisms, implementing correct sleep/wakeup synchronization to avoid lost wakeup problems, and safely transferring data between user and kernel space using xv6's validation functions.

### System Calls

**settickets(int pid, int tickets)** - Assigns lottery tickets to a process. Returns 0 on success, -1 on failure.

**yield(void)** - Voluntarily relinquishes CPU. Returns 0 on success.

**ksend(int channel, const void *msg, int len)** - Sends a message on a mailbox channel (0-15). Blocks if full. Returns 0 on success, -1 on failure.

**krecv(int channel, void *buf, int maxlen)** - Receives a message from a mailbox channel. Blocks if empty. Returns bytes received or -1 on failure.

**getrunticks(void)** - Returns cumulative CPU time (timer ticks) for the calling process.

### Kernel Modifications

#### File: proc.h

Extended `struct proc` with scheduler fields:

```c
int tickets;      // Number of lottery tickets (default: 10)
int preempted;    // 1 if preempted by timer, 0 if voluntary yield
int runticks;     // Cumulative CPU time in ticks
```

#### File: proc.c

**Modified allocproc():** Initializes `tickets=10`, `preempted=0`, `runticks=0` for new processes.

**Replaced scheduler():** Implemented lottery-based scheduling:
1. Count total tickets across all `RUNNABLE` processes
2. Generate random winning ticket using `random_at_most(total_tickets - 1)`
3. Iterate through processes, accumulating ticket ranges until finding winner
4. Context switch to winning process

```c
if(total_tickets > 0){
  long winner = random_at_most(total_tickets - 1);
  int counter = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE) continue;
    counter += p->tickets;
    if(counter > winner){
      // Context switch to this process
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();
      c->proc = 0;
      break;
    }
  }
}
```

**Added settickets():** Validates PID and ticket count, searches process table, updates `p->tickets` atomically under `ptable.lock`.

**Modified yield():** Dynamic adjustment logic is disabled (commented out) to ensure predictable test results. Infrastructure remains for future enhancement.

#### File: trap.c

Modified timer interrupt handler to track CPU time and preemption:

```c
if(myproc() && myproc()->state == RUNNING &&
   tf->trapno == T_IRQ0+IRQ_TIMER){
  myproc()->runticks++;      // Track CPU time
  myproc()->preempted = 1;   // Mark as preempted
  yield();
}
```

#### File: rand.c (New)

Implemented Linear Feedback Shift Register (LFSR) for random number generation:

```c
static unsigned short lfsr = 0xACE1u;

unsigned short rand(void) {
    unsigned short bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}

long random_at_most(long max) {
    if (max <= 0) return 0;
    unsigned long num_bins = (unsigned long)max + 1;
    unsigned long result;
    do {
        result = rand();
    } while (result >= (65536UL - (65536UL % num_bins)));
    return (long)(result % num_bins);
}
```

Uses rejection sampling for uniform distribution.

#### File: mailbox.h (New)

```c
#define NUM_MAILBOXES  16
#define MAILBOX_MSGSIZE 128

struct mailbox {
  struct spinlock lock;
  char   msg[MAILBOX_MSGSIZE];
  int    msglen;
  int    full;  // 1 = has message, 0 = empty
};
```

#### File: mailbox.c (New)

**mailboxinit():** Initializes all 16 mailboxes at boot, setting `full=0` and initializing locks.

**ksend():** Validates arguments, acquires lock, blocks while mailbox is full (`while(mb->full) sleep(mb, &mb->lock)`), copies message, sets `full=1`, wakes receivers, releases lock.

**krecv():** Validates arguments, acquires lock, blocks while mailbox is empty (`while(!mb->full) sleep(mb, &mb->lock)`), copies message out, sets `full=0`, wakes senders, releases lock.

#### File: sysproc.c

Added system call wrappers that extract arguments using `argint()` and `argptr()` (which validates user pointers) and call kernel functions:

```c
int sys_settickets(void) {
  int pid, n;
  if(argint(0, &pid) < 0 || argint(1, &n) < 0) return -1;
  return settickets(pid, n);
}

int sys_ksend(void) {
  int chan, len;
  char *buf;
  if(argint(0, &chan) < 0 || argint(2, &len) < 0) return -1;
  if(argptr(1, &buf, len) < 0) return -1;
  return ksend(chan, buf, len);
}

int sys_krecv(void) {
  int chan, maxlen;
  char *buf;
  if(argint(0, &chan) < 0 || argint(2, &maxlen) < 0) return -1;
  if(argptr(1, &buf, maxlen) < 0) return -1;
  return krecv(chan, buf, maxlen);
}

int sys_getrunticks(void) {
  return myproc()->runticks;
}
```

#### System Call Registration

**syscall.h:** Added `#define SYS_settickets 22`, `SYS_yield 23`, `SYS_ksend 24`, `SYS_krecv 25`, `SYS_getrunticks 26`

**syscall.c:** Added extern declarations and dispatch table entries for all five system calls.

**user.h:** Added function declarations for user-space API.

**usys.S:** Added assembly stubs (`SYSCALL(settickets)`, etc.) that place syscall number in `%eax` and trigger `int $T_SYSCALL`.

#### Test Programs

**schedtest.c:** Implements 4 experiments with 8 processes each, using pipes to collect performance data:
- Experiment 1: Equal tickets (10 each) - validates fairness
- Experiment 2: Weighted tickets (1×80, 7×10) - validates priority
- Experiment 3: Mixed workloads (3 CPU, 3 I/O, 2 yield) - validates behavior differences
- Experiment 4: Starvation test (1×1, 7×100) - validates no complete starvation

**testmailbox.c:** Implements 6 tests validating IPC functionality:
- Test 1: Basic send/receive
- Test 2: Blocking receive (receiver waits for sender)
- Test 3: Multiple independent channels
- Test 4: Bidirectional ping-pong communication
- Test 5: Invalid argument error handling
- Test 6: Maximum message size (128 bytes)

---

## 3. Evaluation

### Lottery Scheduler Results

**Experiment 1: Equal Tickets (Fairness)**
- Configuration: 8 CPU-bound processes, 10 tickets each
- Result: All processes received 12-13% CPU share (expected: 12.5% each)
- Conclusion: Excellent fairness with minimal variance

**Experiment 2: Weighted Tickets (Priority)**
- Configuration: 1 process with 80 tickets, 7 with 10 tickets each
- Result: High-ticket process received 54% (expected: 53.3%)
- Conclusion: Proportional sharing works correctly

**Experiment 3: CPU vs I/O vs Yield**
- Configuration: 3 CPU-bound, 3 I/O-bound, 2 yield-based (all 10 tickets)
- Result: CPU processes consumed 31-32% each, I/O processes 2% each, yield processes <1%
- Conclusion: Process behavior matters; I/O and yield processes naturally use less CPU

**Experiment 4: Starvation Test**
- Configuration: 1 process with 1 ticket, 7 with 100 tickets each
- Result: Low-ticket process received 0.3% (expected: 0.14%)
- Conclusion: Severely disadvantaged but not completely starved

### Mailbox IPC Results

All 6 tests passed successfully:
- ✅ Basic send/receive works correctly
- ✅ Blocking semantics function properly
- ✅ Channels are independent (no message mixing)
- ✅ Bidirectional communication works
- ✅ Error handling returns -1 for invalid inputs
- ✅ Maximum message size (128 bytes) handled correctly

### Comparison: Default vs Modified xv6

| Metric | Round-Robin | Lottery Scheduler |
|--------|-------------|-------------------|
| Fairness | Equal time slices | Proportional to tickets |
| Priority Support | None | Ticket-based |
| Starvation Prevention | Guaranteed | Probabilistic |
| Overhead | Minimal | Low |

| Metric | Pipes | Mailboxes |
|--------|-------|-----------|
| Data Model | Byte stream | Discrete messages |
| Message Boundaries | None | Guaranteed |
| Access Method | File descriptors | Channel IDs (0-15) |
| Buffer Size | 512 bytes (circular) | 128 bytes (fixed) |
| Overhead | High (file system) | Low (direct) |

---

## 4. Conclusions

Both implementations successfully enhance xv6 with modern OS features. The lottery scheduler provides proportional-share CPU allocation suitable for priority-aware workloads. The mailbox IPC system offers lightweight, message-oriented communication that simplifies inter-process coordination.

### Limitations and Future Improvements

**Lottery Scheduler:**
1. Dynamic ticket adjustment infrastructure exists but is disabled—re-enabling with proper tuning could automatically adapt to workload characteristics
2. Could integrate with Multi-Level Feedback Queue (MLFQ) for better interactive/batch workload handling
3. Ticket transfer for priority inheritance would solve priority inversion
4. CPU affinity tracking would improve cache locality

**Mailbox IPC:**
1. Variable message sizes with queuing would increase flexibility
2. Non-blocking operations (ksend_nb/krecv_nb) would enable polling patterns
3. Select/poll support for waiting on multiple channels
4. Message priorities and broadcast channels

### What Couldn't Be Completed

1. **Dynamic Ticket Adjustment:** Infrastructure implemented but logic disabled due to unpredictable test results. Ticket counts were changing during execution, making it difficult to verify proportional sharing.
2. **Comprehensive Performance Benchmarking:** Detailed performance metrics (context switch overhead, IPC latency) not completed.
3. **Multi-Core Optimization:** No per-CPU run queues or work stealing implemented.

### Group Member Contributions

**Member 1: [Your Name]**
Implemented lottery scheduler core algorithm (scheduler(), settickets()), random number generator (rand.c), timer interrupt modifications (trap.c), and comprehensive test suite (schedtest.c) with performance measurement infrastructure.

**Member 2: [Team Member 2 Name]**
Implemented mailbox IPC system (mailbox.c, mailbox.h), including ksend/krecv functions, mailbox initialization, system call wrappers (sysproc.c), and IPC test suite (testmailbox.c).

**Member 3: [Team Member 3 Name]**
Handled system call registration across all files (syscall.h, syscall.c, user.h, usys.S), managed Makefile modifications, created comprehensive documentation, and conducted code review and debugging.

---

**End of Report**
