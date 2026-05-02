# Project Report Corrections Summary

## Overview
After reviewing the actual code implementation, several corrections were made to ensure the report accurately reflects what was actually implemented, rather than relying solely on the markdown documentation files.

---

## Key Corrections Made

### 1. Dynamic Ticket Adjustment - DISABLED ⚠️

**What the documentation said:**
- Dynamic adjustment was fully implemented and working
- Processes lose tickets when preempted, gain tickets when yielding voluntarily

**What the code actually shows:**
```c
// From proc.c - yield() function
// DYNAMIC TICKET ADJUSTMENT DISABLED FOR TESTING
// This was causing lottery scheduler tests to fail because
// ticket counts were being modified during execution.
```

**Correction:** The report now correctly states that:
- The infrastructure exists (the `preempted` flag is tracked)
- The actual adjustment logic is **commented out/disabled**
- This was done to ensure predictable test results
- The feature remains as future work

---

### 2. Random Number Generator - LFSR, not LCG

**What the documentation said:**
- Linear Congruential Generator (LCG) implementation
- Uses formula: `seed = seed * 1103515245 + 12345`

**What the code actually shows:**
```c
// From rand.c
static unsigned short lfsr = 0xACE1u;

unsigned short rand(void)
{
    unsigned short bit;
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}
```

**Correction:** The report now correctly describes:
- **Linear Feedback Shift Register (LFSR)** implementation
- Uses XOR operations on specific bits
- Includes `random_at_most()` function with rejection sampling for uniform distribution

---

### 3. System Call Numbers

**What was missing:**
- `getrunticks` system call number

**What the code shows:**
```c
// From syscall.h
#define SYS_settickets  22
#define SYS_yield       23
#define SYS_ksend       24
#define SYS_krecv       25
#define SYS_getrunticks 26  // ← This was missing in initial report
```

**Correction:** Added `SYS_getrunticks 26` to all relevant sections

---

### 4. Timer Interrupt Handler Implementation

**What the documentation said:**
- Timer interrupt sets `preempted` flag in a separate case statement
- Includes `lapiceoi()` call

**What the code actually shows:**
```c
// From trap.c
if(myproc() && myproc()->state == RUNNING &&
   tf->trapno == T_IRQ0+IRQ_TIMER){
  myproc()->runticks++;      // count timer ticks while running
  myproc()->preempted = 1;   // mark that timer kicked this process out
  yield();
}
```

**Correction:** The report now shows:
- Timer handling is in the main trap handler, not a separate case
- `runticks` is incremented for CPU time tracking
- `preempted` flag is set before yielding
- This is used by `getrunticks()` system call

---

### 5. Scheduler Algorithm Details

**What was unclear:**
- How the random number is generated and bounded

**What the code shows:**
```c
// From proc.c - scheduler()
if(total_tickets > 0){
  long winner = random_at_most(total_tickets - 1);  // ← Uses helper function
  int counter = 0;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE)
      continue;

    counter += p->tickets;

    if(counter > winner){  // this process won the lottery
      // ... context switch code ...
      break;
    }
  }
}
```

**Correction:** The report now correctly shows:
- Uses `random_at_most(total_tickets - 1)` instead of `rand() % total_tickets`
- Proper bounds checking (0 to total_tickets-1)
- Includes check for `total_tickets > 0` to avoid division by zero

---

### 6. getrunticks() System Call

**What was missing:**
- Implementation details of this system call

**What the code shows:**
```c
// From sysproc.c
int sys_getrunticks(void)
{
  return myproc()->runticks;
}
```

**Correction:** Added complete implementation details showing:
- Simple accessor function
- Returns `runticks` field from current process
- Used by test programs to measure actual CPU time received

---

### 7. User.h Function Declarations Order

**Minor correction:**
- Reordered function declarations to match actual file
- `getrunticks()` placement corrected

---

### 8. Conclusions Section Updates

**What changed:**
- Removed claims about "working dynamic adjustment"
- Added explanation of why dynamic adjustment was disabled
- Moved dynamic adjustment from "implemented features" to "future work"
- Updated "What Couldn't Be Completed" section to explain the adjustment issue

---

## Files Verified Against Code

✅ **proc.c** - Scheduler implementation, yield(), settickets()
✅ **proc.h** - Process structure fields
✅ **trap.c** - Timer interrupt handling
✅ **sysproc.c** - System call wrappers
✅ **syscall.h** - System call numbers
✅ **syscall.c** - System call dispatch table
✅ **user.h** - User-space declarations
✅ **usys.S** - Assembly stubs
✅ **rand.c** - Random number generator
✅ **rand.h** - Random function declarations
✅ **mailbox.c** - IPC implementation
✅ **mailbox.h** - Mailbox structure

---

## Accuracy Status

✅ **Lottery Scheduler:** Report now accurately reflects actual implementation
✅ **Mailbox IPC:** Report was already accurate
✅ **System Calls:** All system calls correctly documented
✅ **Random Number Generator:** Corrected to LFSR
✅ **Dynamic Adjustment:** Correctly marked as disabled/future work

---

## Report Integrity

The report now accurately reflects:
1. What was **actually implemented** in the code
2. What **works** vs. what is **infrastructure only**
3. What was **disabled** and why
4. What remains as **future work**

The report is now ready for submission with confidence that it matches the actual codebase.
