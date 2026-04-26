# Sample Output: testmailbox Execution

## Successful Execution (All Tests Pass)

### Terminal Output

```
xv6...
cpu1: starting 1
cpu0: starting 0
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
init: starting sh
$ testmailbox

=== MAILBOX IPC TEST SUITE ===

[TEST 1] Basic send/receive
  PASSED: got 'Hello from child!'

[TEST 2] Blocking recv (parent waits for child)
  PASSED: received after blocking

[TEST 3] Multiple channels (0, 1, 2)
  PASSED: all 3 channels received correctly

[TEST 4] Ping-pong (5 rounds)
  PASSED: 5 ping-pong rounds completed

[TEST 5] Invalid arguments
  PASSED: all bad args returned -1

[TEST 6] Max-size message (128 bytes)
  PASSED: received 128 bytes, all correct

=== ALL TESTS DONE ===
$ 
```

### Execution Timeline

```
Time    Event
----    -----
0.0s    User types "testmailbox" in xv6 shell
0.1s    Shell forks and execs testmailbox program
0.2s    testmailbox starts, prints header
0.3s    TEST 1 starts
0.4s    TEST 1 completes - PASSED
0.5s    TEST 2 starts
1.5s    TEST 2 completes - PASSED (includes 10-tick sleep)
1.6s    TEST 3 starts
1.7s    TEST 3 completes - PASSED
1.8s    TEST 4 starts
2.0s    TEST 4 completes - PASSED (5 rounds of ping-pong)
2.1s    TEST 5 starts
2.2s    TEST 5 completes - PASSED
2.3s    TEST 6 starts
2.4s    TEST 6 completes - PASSED
2.5s    Program prints footer and exits
2.6s    Shell prompt returns
```

---

## Detailed Test-by-Test Output

### Test 1: Basic Send/Receive

```
[TEST 1] Basic send/receive
```

**What happens internally:**
1. Parent process forks
2. Child process created (PID 4)
3. Parent calls `krecv(0, buf, 128)`
   - Mailbox 0 is empty
   - Parent blocks (sleeps)
4. Child calls `ksend(0, "Hello from child!", 18)`
   - Copies message to mailbox 0
   - Sets mailbox[0].full = 1
   - Calls wakeup(mailbox[0])
5. Parent wakes up
   - Copies message from mailbox 0
   - Returns n=18
6. Parent validates: strcmp(buf, "Hello from child!") == 0
7. Child exits
8. Parent waits for child

```
  PASSED: got 'Hello from child!'
```

**Kernel state during test:**
```
Before ksend():
  mailbox[0].full = 0
  mailbox[0].msglen = 0
  mailbox[0].msg = ""

After ksend():
  mailbox[0].full = 1
  mailbox[0].msglen = 18
  mailbox[0].msg = "Hello from child!"

After krecv():
  mailbox[0].full = 0
  mailbox[0].msglen = 0
  mailbox[0].msg = ""
```

---

### Test 2: Blocking Receive

```
[TEST 2] Blocking recv (parent waits for child)
```

**What happens internally:**
1. Parent forks
2. Child created (PID 5)
3. Parent calls `krecv(1, buf, 128)` IMMEDIATELY
   - Mailbox 1 is empty
   - Parent blocks (sleeps)
   - **Parent is now waiting...**
4. Child calls `sleep(10)`
   - Child sleeps for 10 timer ticks (~100ms)
   - **Both processes sleeping!**
5. Scheduler runs other processes
6. After 10 ticks, child wakes up
7. Child calls `ksend(1, "delayed msg", 12)`
   - Copies message to mailbox 1
   - Calls wakeup(mailbox[1])
8. Parent wakes up
   - Copies message from mailbox 1
   - Returns n=12
9. Parent validates message

```
  PASSED: received after blocking
```

**Timeline visualization:**
```
Time    Parent                  Child
----    ------                  -----
T0      fork()                  
T1      krecv(1) → blocks       sleep(10)
T2      [sleeping]              [sleeping]
T3      [sleeping]              [sleeping]
...     [sleeping]              [sleeping]
T10     [sleeping]              [wakes up]
T11     [sleeping]              ksend(1, "delayed msg")
T12     [woken up]              exit()
T13     validates message
T14     wait()
```

---

### Test 3: Multiple Channels

```
[TEST 3] Multiple channels (0, 1, 2)
```

**What happens internally:**
1. Parent forks
2. Child sends on 3 different channels:
   - `ksend(0, "chan0", 5)`
   - `ksend(1, "chan1", 5)`
   - `ksend(2, "chan2", 5)`
3. Child exits
4. Parent receives from each channel:
   - `krecv(0, buf, 128)` → "chan0"
   - `krecv(1, buf, 128)` → "chan1"
   - `krecv(2, buf, 128)` → "chan2"
5. Parent validates each message

```
  PASSED: all 3 channels received correctly
```

**Mailbox state after child sends all 3:**
```
mailbox[0]: full=1, msglen=5, msg="chan0"
mailbox[1]: full=1, msglen=5, msg="chan1"
mailbox[2]: full=1, msglen=5, msg="chan2"
mailbox[3]: full=0, msglen=0, msg=""
...
mailbox[15]: full=0, msglen=0, msg=""
```

**After parent receives from channel 0:**
```
mailbox[0]: full=0, msglen=0, msg=""        ← emptied
mailbox[1]: full=1, msglen=5, msg="chan1"   ← still full
mailbox[2]: full=1, msglen=5, msg="chan2"   ← still full
```

This proves channels are independent!

---

### Test 4: Ping-Pong

```
[TEST 4] Ping-pong (5 rounds)
```

**What happens internally:**

**Round 1:**
```
Parent: ksend(3, "ping", 5)     → mailbox[3] = "ping"
Child:  krecv(3, buf, 128)      → receives "ping"
Child:  ksend(4, "pong", 5)     → mailbox[4] = "pong"
Parent: krecv(4, buf, 128)      → receives "pong"
```

**Round 2:**
```
Parent: ksend(3, "ping", 5)     → mailbox[3] = "ping"
Child:  krecv(3, buf, 128)      → receives "ping"
Child:  ksend(4, "pong", 5)     → mailbox[4] = "pong"
Parent: krecv(4, buf, 128)      → receives "pong"
```

**Rounds 3, 4, 5:** Same pattern

```
  PASSED: 5 ping-pong rounds completed
```

**Channel usage:**
- Channel 3: Parent → Child (ping)
- Channel 4: Child → Parent (pong)

**Synchronization pattern:**
```
Parent          Mailbox[3]      Child           Mailbox[4]
------          ----------      -----           ----------
send ping  →    full=1     →    recv ping
recv pong  ←    full=0     ←    send pong  →    full=1
                                recv ping  ←    full=0
[repeat 5 times]
```

---

### Test 5: Invalid Arguments

```
[TEST 5] Invalid arguments
```

**What happens internally:**

**Test 1: Channel -1**
```c
ksend(-1, "x", 1)
  → enters ksend()
  → if(chan < 0 || chan >= NUM_MAILBOXES) return -1
  → returns -1 immediately (no kernel panic!)
```

**Test 2: Channel 16**
```c
ksend(16, "x", 1)
  → enters ksend()
  → if(chan < 0 || chan >= 16) return -1
  → returns -1 immediately
```

**Test 3: krecv with invalid channels**
```c
krecv(-1, buf, 128)  → returns -1
krecv(16, buf, 128)  → returns -1
```

```
  PASSED: all bad args returned -1
```

**What this proves:**
- Kernel validates inputs before accessing arrays
- No array out-of-bounds access
- No kernel panic or crash
- Graceful error handling

---

### Test 6: Max-Size Message

```
[TEST 6] Max-size message (128 bytes)
```

**What happens internally:**

**Child builds message:**
```c
char msg[128];
for(i = 0; i < 128; i++) msg[i] = 'A';
// msg = "AAAAAAAAAA..." (128 A's)
```

**Child sends:**
```c
ksend(5, msg, 128)
  → validates: len=128 <= MAILBOX_MSGSIZE (OK)
  → memmove(mailbox[5].msg, msg, 128)
  → mailbox[5].msglen = 128
  → mailbox[5].full = 1
```

**Parent receives:**
```c
krecv(5, buf, 128)
  → n = mailbox[5].msglen = 128
  → if(n > maxlen) n = maxlen  // 128 <= 128, no truncation
  → memmove(buf, mailbox[5].msg, 128)
  → returns 128
```

**Parent validates:**
```c
for(i = 0; i < 128; i++)
  if(buf[i] != 'A') ok = 0;
// All 128 bytes are 'A' → ok = 1
```

```
  PASSED: received 128 bytes, all correct
```

**Memory layout:**
```
mailbox[5].msg[0]   = 'A'
mailbox[5].msg[1]   = 'A'
mailbox[5].msg[2]   = 'A'
...
mailbox[5].msg[127] = 'A'
Total: 128 bytes (exactly MAILBOX_MSGSIZE)
```

---

## Failure Scenarios (What If Something Breaks?)

### Scenario 1: Blocking Doesn't Work

**Symptom:**
```
[TEST 2] Blocking recv (parent waits for child)
  FAILED: n=0 buf=''
```

**What happened:**
- Parent called `krecv(1)` but didn't block
- Returned immediately with empty buffer
- Child's message never received

**Root cause:**
```c
// In krecv(), this is broken:
while(!mb->full){
  // sleep(mb, &mb->lock);  ← COMMENTED OUT OR MISSING
}
```

---

### Scenario 2: Channel Isolation Broken

**Symptom:**
```
[TEST 3] Multiple channels (0, 1, 2)
  FAILED ch1: 'chan0'
  FAILED ch2: 'chan0'
```

**What happened:**
- All channels share the same buffer
- Receiving from channel 1 gets channel 0's message

**Root cause:**
```c
// In ksend(), this is broken:
mb = &mailboxes[0];  // ← ALWAYS USES CHANNEL 0!
// Should be:
mb = &mailboxes[chan];
```

---

### Scenario 3: Deadlock in Ping-Pong

**Symptom:**
```
[TEST 4] Ping-pong (5 rounds)
[hangs forever - no output]
```

**What happened:**
- Parent sent ping, waiting for pong
- Child received ping, sent pong, but parent never woke up
- Both processes stuck

**Root cause:**
```c
// In ksend(), this is broken:
// wakeup(mb);  ← COMMENTED OUT OR MISSING
```

---

### Scenario 4: Invalid Args Not Checked

**Symptom:**
```
[TEST 5] Invalid arguments
kernel panic: trap 14
```

**What happened:**
- `ksend(-1, "x", 1)` tried to access `mailboxes[-1]`
- Array out-of-bounds access
- Kernel page fault

**Root cause:**
```c
// In ksend(), validation missing:
// if(chan < 0 || chan >= NUM_MAILBOXES)
//   return -1;
mb = &mailboxes[chan];  // ← ACCESSES INVALID MEMORY!
```

---

### Scenario 5: Buffer Overflow

**Symptom:**
```
[TEST 6] Max-size message (128 bytes)
  FAILED: n=64 or data mismatch
```

**What happened:**
- Only 64 bytes copied instead of 128
- Or some bytes corrupted

**Root cause:**
```c
// In mailbox.h, wrong size:
#define MAILBOX_MSGSIZE 64  // ← SHOULD BE 128!
```

---

## Performance Characteristics

### Execution Time Breakdown

```
Test 1: ~100ms  (basic send/receive)
Test 2: ~1000ms (includes 10-tick sleep)
Test 3: ~200ms  (3 channels)
Test 4: ~300ms  (5 ping-pong rounds)
Test 5: ~50ms   (just validation, no IPC)
Test 6: ~150ms  (128-byte message)
-----------------------------
Total:  ~1800ms (1.8 seconds)
```

### Context Switches

```
Test 1: 2 switches (parent→child, child→parent)
Test 2: 3 switches (parent→child, child→parent, parent→wait)
Test 3: 4 switches (child sends 3, parent receives 3)
Test 4: 20 switches (5 rounds × 4 switches per round)
Test 5: 0 switches (no IPC)
Test 6: 2 switches (parent→child, child→parent)
-----------------------------
Total:  31 context switches
```

---

## Conclusion

The test suite provides comprehensive validation with clear output:

✅ **All tests pass** → Implementation is correct
❌ **Any test fails** → Specific component is broken

The output format makes it easy to:
- Identify which test failed
- Understand what went wrong
- Debug the specific issue

Sample successful run takes ~2 seconds and produces clean, readable output showing all 6 tests passing.
