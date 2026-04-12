#include "types.h"
#include "stat.h"
#include "user.h"

#define NPROC 8
#define WORKLOAD 2000000

int
main(void)
{
  int tickets[NPROC] = {1, 2, 3, 4, 5, 6, 7, 8};
  int pipes[NPROC][2];
  int i;

  printf(1, "\n--- Scheduling Distribution Test ---\n");
  printf(1, "Spawning %d processes with starting tickets: 1 2 3 4 5 6 7 8\n\n", NPROC);

  for(i = 0; i < NPROC; i++){
    pipe(pipes[i]);

    int pid = fork();
    if(pid == 0){
      close(pipes[i][0]);

      int mypid = getpid();
      int start_tickets = tickets[i];
      settickets(mypid, start_tickets);

      int start = uptime();

      // busy loop — CPU hog, will lose tickets due to dynamic adjustment
      volatile int x = 0;
      int j;
      for(j = 0; j < WORKLOAD; j++)
        x++;

      int end = uptime();
      int elapsed = end - start;

      // how many times were we scheduled (rough count via ticks)
      // report: pid, starting tickets, elapsed ticks
      // note: tickets will have dropped by now due to preemption penalty
      write(pipes[i][1], &mypid,         sizeof(mypid));
      write(pipes[i][1], &start_tickets, sizeof(start_tickets));
      write(pipes[i][1], &elapsed,       sizeof(elapsed));

      close(pipes[i][1]);
      exit();
    } else {
      close(pipes[i][1]);
    }
  }

  // collect and print results from all children
  printf(1, "PID   Start Tickets   Ticks Used   Note\n");
  printf(1, "---   ------------   ----------   ----\n");

  int total_ticks = 0;
  int rpid[NPROC], rtickets[NPROC], relapsed[NPROC];

  for(i = 0; i < NPROC; i++){
    read(pipes[i][0], &rpid[i],     sizeof(rpid[i]));
    read(pipes[i][0], &rtickets[i], sizeof(rtickets[i]));
    read(pipes[i][0], &relapsed[i], sizeof(relapsed[i]));
    close(pipes[i][0]);
    total_ticks += relapsed[i];
    wait();
  }

  for(i = 0; i < NPROC; i++){
    // expected share roughly proportional to starting tickets
    // total starting tickets = 1+2+...+8 = 36
    printf(1, "%d     %d              %d",
           rpid[i], rtickets[i], relapsed[i]);

    // simple label to show relative share
    if(rtickets[i] <= 2)
      printf(1, "            low priority\n");
    else if(rtickets[i] <= 5)
      printf(1, "            mid priority\n");
    else
      printf(1, "            high priority\n");
  }

  printf(1, "\nTotal ticks across all processes: %d\n", total_ticks);
  printf(1, "Expected: higher ticket processes finish faster (fewer ticks needed).\n");
  printf(1, "Dynamic adjustment: all busy-loop processes lose tickets over time.\n");
  exit();
}
