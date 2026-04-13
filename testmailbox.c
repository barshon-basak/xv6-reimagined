// testmailbox.c
// Tests the mailbox IPC system calls: ksend() and krecv().
//
// The parent process forks a child.
// The child sends a message on mailbox channel 0.
// The parent receives it and prints it.

#include "types.h"
#include "user.h"

int
main(void)
{
  int pid;
  char buf[128];
  int n;

  pid = fork();

  if(pid == 0){
    // --- Child: sender ---
    char *msg = "Hello from child!";
    printf(1, "child: sending message...\n");
    ksend(0, msg, strlen(msg) + 1);  // +1 to include null terminator
    printf(1, "child: message sent.\n");
    exit();
  } else {
    // --- Parent: receiver ---
    printf(1, "parent: waiting for message...\n");
    n = krecv(0, buf, sizeof(buf));
    printf(1, "parent: received %d bytes: '%s'\n", n, buf);
    wait();
    printf(1, "testmailbox: PASSED\n");
    exit();
  }
}
