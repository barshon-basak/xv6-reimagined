/*
 * schedtest2.c — How 8 processes share the CPU under lottery scheduling
 *
 * All 8 processes start with the SAME ticket count (10).
 * Each process has a different behavior pattern so we can observe
 * how the scheduler responds over time:
 *
 *   P0 — pure CPU hog (busy loop, never yields voluntarily)
 *   P1 — pure CPU hog (same, second instance for comparison)
 *   P2 — yield-heavy  (calls yield() frequently inside work)
 *   P3 — yield-heavy  (same, second instance)
 *   P4 — I/O-like     (sleep between bursts, simulates blocking I/O)
 *   P5 — I/O-like     (same, second instance)
 *   P6 — mixed        (CPU burst then yield then sleep, repeating)
 *   P7 — mixed        (same, second instance)
 *
 * Each child reports back:
 *   - its PID
 *   - starting tickets
 *   - elapsed ticks (wall time from start to finish)
 *   - number of work iterations completed
 *   - behavior label
 *
 * Parent collects all results and prints a comparison table.
 */

#include "types.h"
#include "stat.h"
#include "user.h"

#define NPROC         8
#define INIT_TICKETS  10

/* workload sizes — tune these so processes run long enough to observe */
#define CPU_ITERS     1500000   /* iterations for a CPU burst */
#define YIELD_ITERS   300000    /* iterations between each yield call */
#define YIELD_ROUNDS  5         /* how many yield rounds to do */
#define IO_BURST      200000    /* CPU work before each sleep */
#define IO_ROUNDS     6         /* how many sleep rounds */
#define IO_SLEEP_TICKS 2        /* ticks to sleep each round */
#define MIXED_ROUNDS  4         /* rounds of: burst → yield → sleep */
#define MIXED_BURST   200000    /* CPU work per mixed round */
#define MIXED_SLEEP   1         /* ticks to sleep in mixed round */

/* result struct sent through pipe */
struct result {
  int  pid;
  int  tickets;
  int  elapsed;     /* uptime ticks from start to finish */
  int  iters;       /* work units completed */
  char label[16];   /* behavior type */
};

/* ------------------------------------------------------------------ */
/* worker functions — each represents one behavior pattern             */
/* ------------------------------------------------------------------ */

void
do_cpu_hog(struct result *r)
{
  volatile int x = 0;
  int i;
  for(i = 0; i < CPU_ITERS; i++)
    x++;
  r->iters = CPU_ITERS;
  r->label[0]='C'; r->label[1]='P'; r->label[2]='U';
  r->label[3]='-'; r->label[4]='H'; r->label[5]='O';
  r->label[6]='G'; r->label[7]='\0';
}

void
do_yield_heavy(struct result *r)
{
  volatile int x = 0;
  int round, i;
  for(round = 0; round < YIELD_ROUNDS; round++){
    for(i = 0; i < YIELD_ITERS; i++)
      x++;
    yield();          /* voluntarily give up CPU after each burst */
  }
  r->iters = YIELD_ROUNDS;
  r->label[0]='Y'; r->label[1]='I'; r->label[2]='E';
  r->label[3]='L'; r->label[4]='D'; r->label[5]='\0';
}

void
do_io_like(struct result *r)
{
  volatile int x = 0;
  int round, i;
  for(round = 0; round < IO_ROUNDS; round++){
    for(i = 0; i < IO_BURST; i++)
      x++;
    sleep(IO_SLEEP_TICKS);   /* simulate blocking I/O */
  }
  r->iters = IO_ROUNDS;
  r->label[0]='I'; r->label[1]='/'; r->label[2]='O';
  r->label[3]='\0';
}

void
do_mixed(struct result *r)
{
  volatile int x = 0;
  int round, i;
  for(round = 0; round < MIXED_ROUNDS; round++){
    /* CPU burst */
    for(i = 0; i < MIXED_BURST; i++)
      x++;
    /* voluntary yield */
    yield();
    /* short sleep */
    sleep(MIXED_SLEEP);
  }
  r->iters = MIXED_ROUNDS;
  r->label[0]='M'; r->label[1]='I'; r->label[2]='X';
  r->label[3]='E'; r->label[4]='D'; r->label[5]='\0';
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
  int pipes[NPROC][2];
  int i;

  printf(1, "\n=== CPU Sharing Test: 8 Processes, Equal Start Tickets (%d) ===\n\n",
         INIT_TICKETS);
  printf(1, "Behavior map:\n");
  printf(1, "  P0, P1 -> CPU-HOG  (busy loop, never yields)\n");
  printf(1, "  P2, P3 -> YIELD    (yields every %d iters, %d rounds)\n",
         YIELD_ITERS, YIELD_ROUNDS);
  printf(1, "  P4, P5 -> I/O      (sleep(%d) every %d iters, %d rounds)\n",
         IO_SLEEP_TICKS, IO_BURST, IO_ROUNDS);
  printf(1, "  P6, P7 -> MIXED    (burst+yield+sleep, %d rounds)\n\n",
         MIXED_ROUNDS);

  /* spawn all 8 children */
  for(i = 0; i < NPROC; i++){
    pipe(pipes[i]);

    int pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }

    if(pid == 0){
      /* child */
      close(pipes[i][0]);

      int mypid = getpid();
      settickets(mypid, INIT_TICKETS);   /* all start equal */

      struct result r;
      r.pid     = mypid;
      r.tickets = INIT_TICKETS;

      int start = uptime();

      /* pick behavior based on slot */
      if(i == 0 || i == 1)  do_cpu_hog(&r);
      if(i == 2 || i == 3)  do_yield_heavy(&r);
      if(i == 4 || i == 5)  do_io_like(&r);
      if(i == 6 || i == 7)  do_mixed(&r);

      r.elapsed = uptime() - start;

      write(pipes[i][1], &r, sizeof(r));
      close(pipes[i][1]);
      exit();

    } else {
      /* parent — close write end */
      close(pipes[i][1]);
    }
  }

  /* collect results */
  struct result results[NPROC];
  for(i = 0; i < NPROC; i++){
    read(pipes[i][0], &results[i], sizeof(results[i]));
    close(pipes[i][0]);
    wait();
  }

  /* print table */
  printf(1, "PID    Behavior   Start-Tickets   Elapsed-Ticks   Iters\n");
  printf(1, "-----  ---------  -------------   -------------   -----\n");

  int total_ticks = 0;
  for(i = 0; i < NPROC; i++){
    struct result *r = &results[i];
    total_ticks += r->elapsed;
    printf(1, "%d      %-9s  %d               %d               %d\n",
           r->pid, r->label, r->tickets, r->elapsed, r->iters);
  }

  printf(1, "\n--- Summary ---\n");
  printf(1, "Total elapsed ticks (sum across all): %d\n", total_ticks);
  printf(1, "\nObservations to check:\n");
  printf(1, "  - CPU-HOG processes: should take the most ticks (no voluntary yield)\n");
  printf(1, "  - YIELD processes:   should finish faster (cooperative, may gain tickets)\n");
  printf(1, "  - I/O processes:     lowest elapsed ticks (sleeping = not competing)\n");
  printf(1, "  - MIXED processes:   middle ground between CPU and I/O\n");
  printf(1, "  - If dynamic ticket adjustment is ON: CPU-HOG tickets drop over time,\n");
  printf(1, "    YIELD/IO tickets rise, widening the gap in elapsed ticks.\n");

  exit();
}
