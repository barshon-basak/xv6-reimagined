# Mailbox IPC — Concept & Implementation Explained

## The Problem

xv6 already has **pipes** for communication between processes. But pipes are stream-based — you write bytes, you read bytes, there's no concept of a "message". They're also tied to file descriptors which adds complexity.

The goal here was to build something simpler and more message-oriented: a **mailbox**. Think of it like a physical mailbox — you drop a letter in, someone picks it up. Only one letter fits at a time.

---

## The Big Picture

```
Process A (sender)          Kernel Mailbox [chan 0]       Process B (receiver)
     |                            |                              |
     |--- ksend(0, msg, len) ---> | stores msg, marks full       |
     |    (blocks if full)        |                              |
     |                            | <--- krecv(0, buf, max) -----|
     |                            |      copies msg out          |
     |                            |      marks empty             |
     |    (wakes up if blocked)   |                              |
```

The kernel sits in the middle managing the mailboxes. Processes never share memory directly — everything goes through the kernel.

---

## How xv6 System Calls Work (the pattern)

Every time a user program calls something like `ksend(...)`, here's what actually happens:

```
User program calls ksend()
        ↓
usys.S  →  puts syscall number in register %eax, triggers INT instruction
        ↓
CPU switches to kernel mode
        ↓
syscall.c  →  looks up the number in a dispatch table, calls sys_ksend()
        ↓
sysproc.c  →  sys_ksend() extracts arguments from the user stack safely
        ↓
mailbox.c  →  the real logic runs (ksend / krecv)
        ↓
returns result back to user
```

This is the same pattern for every single syscall in xv6 — `fork`, `read`, `write`, all of them. We just followed the exact same steps.

---

## The Mailbox Structure

```c
struct mailbox {
  struct spinlock lock;   // only one process touches this at a time
  char   msg[128];        // the message bytes
  int    msglen;          // how many bytes the message is
  int    full;            // 0 = empty, 1 = has a message
};
```

There are 16 of these in a global array. Channel 0 means `mailboxes[0]`, channel 1 means `mailboxes[1]`, etc.

---

## The Synchronization — The Most Important Part

This is where the OS concepts live. The key question is: **what happens when a sender tries to send but the mailbox is already full?** Or a receiver tries to receive but it's empty?

We use xv6's `sleep()` and `wakeup()` — the same mechanism xv6 uses internally for pipes, disk I/O, etc.

**ksend logic:**
```
acquire lock
while mailbox is full:
    sleep()          ← process goes to sleep, lock is released
                     ← another process runs
                     ← eventually receiver calls krecv, empties it, calls wakeup()
                     ← this process wakes up, re-acquires lock, checks again
copy message in
mark full = 1
wakeup any sleeping receiver
release lock
```

**krecv logic:**
```
acquire lock
while mailbox is empty:
    sleep()          ← same idea, blocks until sender puts something in
copy message out
mark full = 0
wakeup any sleeping sender
release lock
```

The `while` loop (not `if`) is important — it's a standard pattern because a process can wake up spuriously, so you always re-check the condition.

---

## Why a Spinlock?

The mailbox is shared kernel data. If two processes try to send to the same mailbox at the same time (on different CPUs), they'd corrupt each other's data without a lock. The spinlock ensures only one process is inside the critical section at a time.

---

## The Files We Touched and Why

| File | Why |
|------|-----|
| `mailbox.h` | Define the struct — both mailbox.c and sysproc.c need to know the shape |
| `mailbox.c` | The actual logic — ksend, krecv, mailboxinit |
| `defs.h` | Tell the rest of the kernel "these functions exist" |
| `main.c` | Initialize the mailboxes at boot time |
| `syscall.h` | Assign numbers 24 and 25 to the new syscalls |
| `syscall.c` | Add them to the dispatch table so the kernel knows which function to call |
| `sysproc.c` | Thin wrappers that safely pull arguments from user space |
| `usys.S` | Assembly stubs — what the user program actually calls |
| `user.h` | Declare the functions so user programs can compile against them |
| `Makefile` | Tell the build system to compile mailbox.c and include the test program |

---

## Flow of a Single ksend Call End-to-End

```
testmailbox.c:
    ksend(0, "Hello", 6)
         ↓
usys.S:
    mov $24, %eax    ← SYS_ksend = 24
    int $64          ← trap into kernel
         ↓
syscall.c:
    syscalls[24]()   ← calls sys_ksend
         ↓
sysproc.c sys_ksend:
    argint(0, &chan)      ← reads chan=0 from user stack
    argint(2, &len)       ← reads len=6
    argptr(1, &buf, 6)    ← validates and gets pointer to "Hello"
    return ksend(0, buf, 6)
         ↓
mailbox.c ksend:
    acquire lock on mailboxes[0]
    mailbox is empty → don't sleep
    memmove(mb->msg, "Hello", 6)
    mb->full = 1
    wakeup(mb)       ← wakes up parent sleeping in krecv
    release lock
```

---

That's the complete picture. The concept is simple — a fixed-size kernel buffer with blocking on full/empty — and the implementation just follows xv6's existing patterns exactly.
