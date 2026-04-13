// mailbox.c
// Kernel-side implementation of mailbox-based IPC.
//
// There are NUM_MAILBOXES mailboxes, each identified by an integer ID.
// Each mailbox holds at most one message at a time.
//
// ksend() - sends a message; blocks if the mailbox is already full.
// krecv() - receives a message; blocks if the mailbox is empty.
//
// Synchronization uses spinlocks + sleep/wakeup (standard xv6 pattern).

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "mailbox.h"
#include "string.h"

// The global array of mailboxes, shared across all processes.
struct mailbox mailboxes[NUM_MAILBOXES];

// Called once at kernel startup to initialize all mailboxes.
void
mailboxinit(void)
{
  int i;
  for(i = 0; i < NUM_MAILBOXES; i++){
    initlock(&mailboxes[i].lock, "mailbox");
    mailboxes[i].full   = 0;
    mailboxes[i].msglen = 0;
  }
}

// ksend: send a message of 'len' bytes from 'buf' to mailbox 'chan'.
// Blocks (sleeps) if the mailbox is already full.
// Returns 0 on success, -1 on bad arguments.
int
ksend(int chan, const void *buf, int len)
{
  struct mailbox *mb;

  // Validate channel ID and message length.
  if(chan < 0 || chan >= NUM_MAILBOXES)
    return -1;
  if(len <= 0 || len > MAILBOX_MSGSIZE)
    return -1;

  mb = &mailboxes[chan];

  acquire(&mb->lock);

  // If the mailbox is full, sleep until the receiver empties it.
  while(mb->full){
    // sleep() releases the lock and puts this process to sleep.
    // It wakes up when wakeup() is called on the same channel address.
    sleep(mb, &mb->lock);
  }

  // Copy the message into the mailbox buffer.
  memmove(mb->msg, buf, len);
  mb->msglen = len;
  mb->full   = 1;

  // Wake up any process sleeping in krecv() waiting for a message.
  wakeup(mb);

  release(&mb->lock);
  return 0;
}

// krecv: receive a message from mailbox 'chan' into 'buf' (up to 'maxlen' bytes).
// Blocks (sleeps) if the mailbox is empty.
// Returns the number of bytes copied, or -1 on bad arguments.
int
krecv(int chan, void *buf, int maxlen)
{
  struct mailbox *mb;
  int n;

  // Validate channel ID and buffer size.
  if(chan < 0 || chan >= NUM_MAILBOXES)
    return -1;
  if(maxlen <= 0)
    return -1;

  mb = &mailboxes[chan];

  acquire(&mb->lock);

  // If the mailbox is empty, sleep until a sender puts a message in.
  while(!mb->full){
    sleep(mb, &mb->lock);
  }

  // Copy the message out (up to maxlen bytes).
  n = mb->msglen;
  if(n > maxlen)
    n = maxlen;
  memmove(buf, mb->msg, n);

  // Mark the mailbox as empty again.
  mb->full   = 0;
  mb->msglen = 0;

  // Wake up any process sleeping in ksend() waiting for space.
  wakeup(mb);

  release(&mb->lock);
  return n;
}
