// mailbox.h
// Defines the mailbox structure used for IPC between processes.

#define NUM_MAILBOXES  16   // total number of mailboxes available
#define MAILBOX_MSGSIZE 128 // max bytes per message

struct mailbox {
  struct spinlock lock;        // protects all fields below
  char   msg[MAILBOX_MSGSIZE]; // the stored message
  int    msglen;               // length of the stored message
  int    full;                 // 1 = mailbox has a message, 0 = empty
};
