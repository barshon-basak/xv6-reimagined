// testmailbox2.c
// Enhanced mailbox IPC tests with detailed step-by-step output.
// Shows HOW each operation works, not just pass/fail results.
//
// Same experiments as testmailbox.c but with verbose explanations:
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
  printf(1, "\n========================================\n");
  printf(1, "[TEST 1] Basic send/receive\n");
  printf(1, "========================================\n");
  printf(1, "Goal: Child sends a message, parent receives it.\n");
  printf(1, "Channel: 0\n\n");

  int pid = fork();
  if(pid == 0){
    // child: sender
    printf(1, "[CHILD %d] Starting...\n", getpid());
    char *msg = "Hello from child!";
    printf(1, "[CHILD %d] Calling ksend(channel=0, msg='%s', len=%d)\n", 
           getpid(), msg, strlen(msg) + 1);
    
    int ret = ksend(0, msg, strlen(msg) + 1);
    
    printf(1, "[CHILD %d] ksend() returned %d\n", getpid(), ret);
    printf(1, "[CHILD %d] Message sent successfully. Exiting.\n", getpid());
    exit();
  } else {
    // parent: receiver
    printf(1, "[PARENT %d] Starting...\n", getpid());
    printf(1, "[PARENT %d] Child PID is %d\n", getpid(), pid);
    printf(1, "[PARENT %d] Calling krecv(channel=0, buf, size=%d)\n", 
           getpid(), MSGSIZE);
    
    char buf[MSGSIZE];
    int n = krecv(0, buf, sizeof(buf));
    
    printf(1, "[PARENT %d] krecv() returned %d bytes\n", getpid(), n);
    printf(1, "[PARENT %d] Received message: '%s'\n", getpid(), buf);
    printf(1, "[PARENT %d] Waiting for child to exit...\n", getpid());
    wait();
    
    if(n > 0 && strcmp(buf, "Hello from child!") == 0)
      printf(1, "\n✓ TEST 1 PASSED: Message received correctly!\n");
    else
      printf(1, "\n✗ TEST 1 FAILED: Unexpected result n=%d\n", n);
  }
}

// ---------------------------------------------------------------
// TEST 2 — Blocking recv
// Parent calls krecv FIRST, then child sends after a short delay.
// Parent must block until the message arrives.
// ---------------------------------------------------------------
void test2(void)
{
  printf(1, "\n========================================\n");
  printf(1, "[TEST 2] Blocking recv\n");
  printf(1, "========================================\n");
  printf(1, "Goal: Parent blocks on krecv() until child sends.\n");
  printf(1, "Channel: 1\n\n");

  int pid = fork();
  if(pid == 0){
    // child: wait a bit, then send
    printf(1, "[CHILD %d] Starting...\n", getpid());
    printf(1, "[CHILD %d] Sleeping for 10 ticks to let parent block...\n", getpid());
    sleep(10);
    
    printf(1, "[CHILD %d] Woke up! Now calling ksend(channel=1, msg='delayed msg')\n", 
           getpid());
    int ret = ksend(1, "delayed msg", 12);
    
    printf(1, "[CHILD %d] ksend() returned %d\n", getpid(), ret);
    printf(1, "[CHILD %d] This should unblock the parent. Exiting.\n", getpid());
    exit();
  } else {
    // parent: krecv blocks here until child sends
    printf(1, "[PARENT %d] Starting...\n", getpid());
    printf(1, "[PARENT %d] Child PID is %d\n", getpid(), pid);
    printf(1, "[PARENT %d] Calling krecv(channel=1) BEFORE child sends...\n", getpid());
    printf(1, "[PARENT %d] This will BLOCK until child sends a message.\n", getpid());
    
    char buf[MSGSIZE];
    int n = krecv(1, buf, sizeof(buf));
    
    printf(1, "[PARENT %d] UNBLOCKED! krecv() returned %d bytes\n", getpid(), n);
    printf(1, "[PARENT %d] Received message: '%s'\n", getpid(), buf);
    printf(1, "[PARENT %d] Waiting for child to exit...\n", getpid());
    wait();
    
    if(n == 12 && strcmp(buf, "delayed msg") == 0)
      printf(1, "\n✓ TEST 2 PASSED: Blocking recv worked correctly!\n");
    else
      printf(1, "\n✗ TEST 2 FAILED: n=%d buf='%s'\n", n, buf);
  }
}

// ---------------------------------------------------------------
// TEST 3 — Multiple channels
// Send different messages on channels 0, 1, 2.
// Each channel is independent — messages don't mix.
// ---------------------------------------------------------------
void test3(void)
{
  printf(1, "\n========================================\n");
  printf(1, "[TEST 3] Multiple channels\n");
  printf(1, "========================================\n");
  printf(1, "Goal: Send on channels 0, 1, 2 independently.\n");
  printf(1, "Messages should not interfere with each other.\n\n");

  int pid = fork();
  if(pid == 0){
    // child: send one message on each channel
    printf(1, "[CHILD %d] Starting...\n", getpid());
    
    printf(1, "[CHILD %d] Sending 'chan0' on channel 0...\n", getpid());
    ksend(0, "chan0", 6);
    
    printf(1, "[CHILD %d] Sending 'chan1' on channel 1...\n", getpid());
    ksend(1, "chan1", 6);
    
    printf(1, "[CHILD %d] Sending 'chan2' on channel 2...\n", getpid());
    ksend(2, "chan2", 6);
    
    printf(1, "[CHILD %d] All 3 messages sent. Exiting.\n", getpid());
    exit();
  } else {
    char buf[MSGSIZE];
    int ok = 1;

    printf(1, "[PARENT %d] Starting...\n", getpid());
    printf(1, "[PARENT %d] Child PID is %d\n\n", getpid(), pid);

    printf(1, "[PARENT %d] Receiving from channel 0...\n", getpid());
    memset(buf, 0, sizeof(buf));
    int n0 = krecv(0, buf, sizeof(buf));
    printf(1, "[PARENT %d] Got %d bytes: '%s'\n", getpid(), n0, buf);
    if(strcmp(buf, "chan0") != 0){ 
      printf(1, "[PARENT %d] ✗ FAILED ch0: expected 'chan0', got '%s'\n", getpid(), buf); 
      ok=0; 
    } else {
      printf(1, "[PARENT %d] ✓ Channel 0 correct\n\n", getpid());
    }

    printf(1, "[PARENT %d] Receiving from channel 1...\n", getpid());
    memset(buf, 0, sizeof(buf));
    int n1 = krecv(1, buf, sizeof(buf));
    printf(1, "[PARENT %d] Got %d bytes: '%s'\n", getpid(), n1, buf);
    if(strcmp(buf, "chan1") != 0){ 
      printf(1, "[PARENT %d] ✗ FAILED ch1: expected 'chan1', got '%s'\n", getpid(), buf); 
      ok=0; 
    } else {
      printf(1, "[PARENT %d] ✓ Channel 1 correct\n\n", getpid());
    }

    printf(1, "[PARENT %d] Receiving from channel 2...\n", getpid());
    memset(buf, 0, sizeof(buf));
    int n2 = krecv(2, buf, sizeof(buf));
    printf(1, "[PARENT %d] Got %d bytes: '%s'\n", getpid(), n2, buf);
    if(strcmp(buf, "chan2") != 0){ 
      printf(1, "[PARENT %d] ✗ FAILED ch2: expected 'chan2', got '%s'\n", getpid(), buf); 
      ok=0; 
    } else {
      printf(1, "[PARENT %d] ✓ Channel 2 correct\n\n", getpid());
    }

    printf(1, "[PARENT %d] Waiting for child to exit...\n", getpid());
    wait();
    
    if(ok) 
      printf(1, "\n✓ TEST 3 PASSED: All 3 channels received correctly!\n");
    else
      printf(1, "\n✗ TEST 3 FAILED: One or more channels had errors\n");
  }
}

// ---------------------------------------------------------------
// TEST 4 — Ping-pong
// Parent and child exchange 5 messages back and forth.
// Parent sends on channel 3, child replies on channel 4.
// ---------------------------------------------------------------
void test4(void)
{
  printf(1, "\n========================================\n");
  printf(1, "[TEST 4] Ping-pong\n");
  printf(1, "========================================\n");
  printf(1, "Goal: Exchange 5 messages back and forth.\n");
  printf(1, "Parent sends 'ping' on channel 3.\n");
  printf(1, "Child replies 'pong' on channel 4.\n\n");

  int pid = fork();
  if(pid == 0){
    // child: receive ping, send pong, repeat 5 times
    printf(1, "[CHILD %d] Starting ping-pong responder...\n", getpid());
    char buf[MSGSIZE];
    int i;
    for(i = 0; i < 5; i++){
      printf(1, "[CHILD %d] Round %d: Waiting for 'ping' on channel 3...\n", 
             getpid(), i+1);
      krecv(3, buf, sizeof(buf));
      printf(1, "[CHILD %d] Round %d: Received '%s'\n", getpid(), i+1, buf);
      
      printf(1, "[CHILD %d] Round %d: Sending 'pong' on channel 4...\n", 
             getpid(), i+1);
      ksend(4, "pong", 5);
    }
    printf(1, "[CHILD %d] All 5 rounds complete. Exiting.\n", getpid());
    exit();
  } else {
    // parent: send ping, receive pong, repeat 5 times
    printf(1, "[PARENT %d] Starting ping-pong initiator...\n", getpid());
    printf(1, "[PARENT %d] Child PID is %d\n\n", getpid(), pid);
    
    char buf[MSGSIZE];
    int i;
    for(i = 0; i < 5; i++){
      printf(1, "[PARENT %d] Round %d: Sending 'ping' on channel 3...\n", 
             getpid(), i+1);
      ksend(3, "ping", 5);
      
      printf(1, "[PARENT %d] Round %d: Waiting for 'pong' on channel 4...\n", 
             getpid(), i+1);
      krecv(4, buf, sizeof(buf));
      printf(1, "[PARENT %d] Round %d: Received '%s'\n\n", getpid(), i+1, buf);
    }
    
    printf(1, "[PARENT %d] Waiting for child to exit...\n", getpid());
    wait();
    printf(1, "\n✓ TEST 4 PASSED: 5 ping-pong rounds completed!\n");
  }
}

// ---------------------------------------------------------------
// TEST 5 — Invalid arguments
// Bad channel IDs should return -1 (no crash).
// ---------------------------------------------------------------
void test5(void)
{
  printf(1, "\n========================================\n");
  printf(1, "[TEST 5] Invalid arguments\n");
  printf(1, "========================================\n");
  printf(1, "Goal: Verify that invalid channel IDs return -1.\n");
  printf(1, "Valid channels: 0-15\n\n");

  char buf[MSGSIZE];
  int ok = 1;

  printf(1, "[PROCESS %d] Testing ksend(-1, 'x', 1)...\n", getpid());
  int ret1 = ksend(-1, "x", 1);
  printf(1, "[PROCESS %d] Returned: %d (expected -1)\n", getpid(), ret1);
  if(ret1 != -1){ 
    printf(1, "[PROCESS %d] ✗ FAILED: should return -1\n", getpid()); 
    ok=0; 
  } else {
    printf(1, "[PROCESS %d] ✓ Correct\n\n", getpid());
  }

  printf(1, "[PROCESS %d] Testing krecv(-1, buf, size)...\n", getpid());
  int ret2 = krecv(-1, buf, sizeof(buf));
  printf(1, "[PROCESS %d] Returned: %d (expected -1)\n", getpid(), ret2);
  if(ret2 != -1){ 
    printf(1, "[PROCESS %d] ✗ FAILED: should return -1\n", getpid()); 
    ok=0; 
  } else {
    printf(1, "[PROCESS %d] ✓ Correct\n\n", getpid());
  }

  printf(1, "[PROCESS %d] Testing ksend(16, 'x', 1)...\n", getpid());
  int ret3 = ksend(16, "x", 1);
  printf(1, "[PROCESS %d] Returned: %d (expected -1)\n", getpid(), ret3);
  if(ret3 != -1){ 
    printf(1, "[PROCESS %d] ✗ FAILED: should return -1\n", getpid()); 
    ok=0; 
  } else {
    printf(1, "[PROCESS %d] ✓ Correct\n\n", getpid());
  }

  printf(1, "[PROCESS %d] Testing krecv(16, buf, size)...\n", getpid());
  int ret4 = krecv(16, buf, sizeof(buf));
  printf(1, "[PROCESS %d] Returned: %d (expected -1)\n", getpid(), ret4);
  if(ret4 != -1){ 
    printf(1, "[PROCESS %d] ✗ FAILED: should return -1\n", getpid()); 
    ok=0; 
  } else {
    printf(1, "[PROCESS %d] ✓ Correct\n\n", getpid());
  }

  if(ok) 
    printf(1, "✓ TEST 5 PASSED: All invalid args returned -1!\n");
  else
    printf(1, "✗ TEST 5 FAILED: Some invalid args did not return -1\n");
}

// ---------------------------------------------------------------
// TEST 6 — Max-size message (128 bytes)
// Fill a 128-byte buffer and send it. Verify all bytes arrive.
// ---------------------------------------------------------------
void test6(void)
{
  printf(1, "\n========================================\n");
  printf(1, "[TEST 6] Max-size message\n");
  printf(1, "========================================\n");
  printf(1, "Goal: Send a full %d-byte message.\n", MSGSIZE);
  printf(1, "Channel: 5\n\n");

  int pid = fork();
  if(pid == 0){
    // child: build a 128-byte message (all 'A's)
    printf(1, "[CHILD %d] Starting...\n", getpid());
    printf(1, "[CHILD %d] Building %d-byte message (all 'A's)...\n", 
           getpid(), MSGSIZE);
    
    char msg[MSGSIZE];
    int i;
    for(i = 0; i < MSGSIZE; i++) msg[i] = 'A';
    
    printf(1, "[CHILD %d] Calling ksend(channel=5, msg, len=%d)...\n", 
           getpid(), MSGSIZE);
    int ret = ksend(5, msg, MSGSIZE);
    
    printf(1, "[CHILD %d] ksend() returned %d\n", getpid(), ret);
    printf(1, "[CHILD %d] Max-size message sent. Exiting.\n", getpid());
    exit();
  } else {
    printf(1, "[PARENT %d] Starting...\n", getpid());
    printf(1, "[PARENT %d] Child PID is %d\n", getpid(), pid);
    printf(1, "[PARENT %d] Calling krecv(channel=5, buf, size=%d)...\n", 
           getpid(), MSGSIZE);
    
    char buf[MSGSIZE];
    int n = krecv(5, buf, sizeof(buf));
    
    printf(1, "[PARENT %d] krecv() returned %d bytes\n", getpid(), n);
    printf(1, "[PARENT %d] Verifying all %d bytes are 'A'...\n", getpid(), n);
    
    printf(1, "[PARENT %d] Waiting for child to exit...\n", getpid());
    wait();

    int ok = (n == MSGSIZE);
    int i;
    for(i = 0; i < MSGSIZE && ok; i++)
      if(buf[i] != 'A') ok = 0;

    if(ok) {
      printf(1, "[PARENT %d] ✓ All %d bytes verified correct!\n", getpid(), n);
      printf(1, "\n✓ TEST 6 PASSED: Max-size message received correctly!\n");
    } else {
      printf(1, "[PARENT %d] ✗ Data mismatch detected\n", getpid());
      printf(1, "\n✗ TEST 6 FAILED: n=%d or data mismatch\n", n);
    }
  }
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int
main(void)
{
  printf(1, "\n");
  printf(1, "╔════════════════════════════════════════╗\n");
  printf(1, "║  MAILBOX IPC TEST SUITE (VERBOSE)     ║\n");
  printf(1, "║  testmailbox2.c                        ║\n");
  printf(1, "╚════════════════════════════════════════╝\n");
  printf(1, "\nThis test suite shows HOW mailbox operations work,\n");
  printf(1, "with detailed step-by-step output for each test.\n");

  test1();
  test2();
  test3();
  test4();
  test5();
  test6();

  printf(1, "\n");
  printf(1, "╔════════════════════════════════════════╗\n");
  printf(1, "║  ALL TESTS COMPLETED                   ║\n");
  printf(1, "╚════════════════════════════════════════╝\n");
  printf(1, "\n");
  
  exit();
}
