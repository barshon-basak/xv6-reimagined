// testmailbox.c
// Tests for the mailbox IPC system calls: ksend() and krecv().
//
// Tests:
//   1. Basic send/receive          - child sends, parent receives
//   2. Blocking recv               - parent calls krecv before child sends
//   3. Multiple channels           - send on channels 0, 1, 2 independently
//   4. Ping-pong                   - back-and-forth exchange on two channels
//   5. Invalid arguments           - bad channel IDs return -1
//   6. Max-size message            - send exactly 128 bytes (MAILBOX_MSGSIZE)

#include "types.h"
#include "user.h"

#define MSGSIZE 128   // must match MAILBOX_MSGSIZE in mailbox.h

// ---------------------------------------------------------------
// TEST 1 — Basic send/receive
// Child sends "Hello from child!", parent receives and checks it.
// ---------------------------------------------------------------
void test1(void)
{
  printf(1, "\n[TEST 1] Basic send/receive\n");

  int pid = fork();
  if(pid == 0){
    // child: sender
    char *msg = "Hello from child!";
    ksend(0, msg, strlen(msg) + 1);
    exit();
  } else {
    // parent: receiver
    char buf[MSGSIZE];
    int n = krecv(0, buf, sizeof(buf));
    wait();
    if(n > 0 && strcmp(buf, "Hello from child!") == 0)
      printf(1, "  PASSED: got '%s'\n", buf);
    else
      printf(1, "  FAILED: unexpected result n=%d\n", n);
  }
}

// ---------------------------------------------------------------
// TEST 2 — Blocking recv
// Parent calls krecv FIRST, then child sends after a short delay.
// Parent must block until the message arrives.
// ---------------------------------------------------------------
void test2(void)
{
  printf(1, "\n[TEST 2] Blocking recv (parent waits for child)\n");

  int pid = fork();
  if(pid == 0){
    // child: wait a bit, then send
    sleep(10);
    ksend(1, "delayed msg", 12);
    exit();
  } else {
    // parent: krecv blocks here until child sends
    char buf[MSGSIZE];
    int n = krecv(1, buf, sizeof(buf));
    wait();
    if(n == 12 && strcmp(buf, "delayed msg") == 0)
      printf(1, "  PASSED: received after blocking\n");
    else
      printf(1, "  FAILED: n=%d buf='%s'\n", n, buf);
  }
}

// ---------------------------------------------------------------
// TEST 3 — Multiple channels
// Send different messages on channels 0, 1, 2.
// Each channel is independent — messages don't mix.
// ---------------------------------------------------------------
void test3(void)
{
  printf(1, "\n[TEST 3] Multiple channels (0, 1, 2)\n");

  int pid = fork();
  if(pid == 0){
    // child: send one message on each channel
    ksend(0, "chan0", 6);
    ksend(1, "chan1", 6);
    ksend(2, "chan2", 6);
    exit();
  } else {
    char buf[MSGSIZE];
    int ok = 1;

    memset(buf, 0, sizeof(buf));
    krecv(0, buf, sizeof(buf));
    if(strcmp(buf, "chan0") != 0){ printf(1, "  FAILED ch0: '%s'\n", buf); ok=0; }

    memset(buf, 0, sizeof(buf));
    krecv(1, buf, sizeof(buf));
    if(strcmp(buf, "chan1") != 0){ printf(1, "  FAILED ch1: '%s'\n", buf); ok=0; }

    memset(buf, 0, sizeof(buf));
    krecv(2, buf, sizeof(buf));
    if(strcmp(buf, "chan2") != 0){ printf(1, "  FAILED ch2: '%s'\n", buf); ok=0; }

    wait();
    if(ok) printf(1, "  PASSED: all 3 channels received correctly\n");
  }
}

// ---------------------------------------------------------------
// TEST 4 — Ping-pong
// Parent and child exchange 5 messages back and forth.
// Parent sends on channel 3, child replies on channel 4.
// ---------------------------------------------------------------
void test4(void)
{
  printf(1, "\n[TEST 4] Ping-pong (5 rounds)\n");

  int pid = fork();
  if(pid == 0){
    // child: receive ping, send pong, repeat 5 times
    char buf[MSGSIZE];
    int i;
    for(i = 0; i < 5; i++){
      krecv(3, buf, sizeof(buf));
      ksend(4, "pong", 5);
    }
    exit();
  } else {
    // parent: send ping, receive pong, repeat 5 times
    char buf[MSGSIZE];
    int i;
    for(i = 0; i < 5; i++){
      ksend(3, "ping", 5);
      krecv(4, buf, sizeof(buf));
    }
    wait();
    printf(1, "  PASSED: 5 ping-pong rounds completed\n");
  }
}

// ---------------------------------------------------------------
// TEST 5 — Invalid arguments
// Bad channel IDs should return -1 (no crash).
// ---------------------------------------------------------------
void test5(void)
{
  printf(1, "\n[TEST 5] Invalid arguments\n");

  char buf[MSGSIZE];
  int ok = 1;

  // channel -1 is out of range
  if(ksend(-1, "x", 1) != -1){ printf(1, "  FAILED: ksend(-1) should return -1\n"); ok=0; }
  if(krecv(-1, buf, sizeof(buf)) != -1){ printf(1, "  FAILED: krecv(-1) should return -1\n"); ok=0; }

  // channel 16 is out of range (valid range is 0-15)
  if(ksend(16, "x", 1) != -1){ printf(1, "  FAILED: ksend(16) should return -1\n"); ok=0; }
  if(krecv(16, buf, sizeof(buf)) != -1){ printf(1, "  FAILED: krecv(16) should return -1\n"); ok=0; }

  if(ok) printf(1, "  PASSED: all bad args returned -1\n");
}

// ---------------------------------------------------------------
// TEST 6 — Max-size message (128 bytes)
// Fill a 128-byte buffer and send it. Verify all bytes arrive.
// ---------------------------------------------------------------
void test6(void)
{
  printf(1, "\n[TEST 6] Max-size message (%d bytes)\n", MSGSIZE);

  int pid = fork();
  if(pid == 0){
    // child: build a 128-byte message (all 'A's)
    char msg[MSGSIZE];
    int i;
    for(i = 0; i < MSGSIZE; i++) msg[i] = 'A';
    ksend(5, msg, MSGSIZE);
    exit();
  } else {
    char buf[MSGSIZE];
    int n = krecv(5, buf, sizeof(buf));
    wait();

    int ok = (n == MSGSIZE);
    int i;
    for(i = 0; i < MSGSIZE && ok; i++)
      if(buf[i] != 'A') ok = 0;

    if(ok) printf(1, "  PASSED: received %d bytes, all correct\n", n);
    else   printf(1, "  FAILED: n=%d or data mismatch\n", n);
  }
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int
main(void)
{
  printf(1, "\n=== MAILBOX IPC TEST SUITE ===\n");

  test1();
  test2();
  test3();
  test4();
  test5();
  test6();

  printf(1, "\n=== ALL TESTS DONE ===\n");
  exit();
}
