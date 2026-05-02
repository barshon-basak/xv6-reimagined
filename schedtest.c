#include "types.h"
#include "stat.h"
#include "user.h"

#define NEXP_PROCS 8

// Result sent back from each child via pipe
struct result {
  int pid;
  int tickets;
  int ticks;       // number of times scheduled (or elapsed ticks)
  int kind;        // 0=cpu, 1=io, 2=yield
};

// ---------------------------------------------------------------
// Helper: print a table of results collected from 'n' children
// ---------------------------------------------------------------
void print_stats(char *title, struct result *res, int n)
{
  int i;
  int total_ticks = 0;
  int max_ticks   = 0;
  int min_ticks   = 999999;

  for (i = 0; i < n; i++) {
    total_ticks += res[i].ticks;
    if (res[i].ticks > max_ticks) max_ticks = res[i].ticks;
    if (res[i].ticks < min_ticks) min_ticks = res[i].ticks;
  }

  printf(1, "\n>> %s\n", title);
  printf(1, "\n");

  for (i = 0; i < n; i++) {
    char *kind  = (res[i].kind == 1) ? "io(sleep)" :
                  (res[i].kind == 2) ? "yield"     : "cpu(busy)";
    int share = (total_ticks > 0) ? (res[i].ticks * 100) / total_ticks : 0;

    printf(1, "  pid=%d  tickets=%d  ticks=%d  share=%d%%  type=%s\n",
           res[i].pid, res[i].tickets, res[i].ticks, share, kind);
  }

  printf(1, "\n");
  printf(1, "  total=%d  max=%d  min=%d\n", total_ticks, max_ticks, min_ticks);
  printf(1, "\n");
}

// ---------------------------------------------------------------
// Collect results from n pipes, wait for all children
// ---------------------------------------------------------------
void collect(int pipes[][2], struct result *res, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    read(pipes[i][0], &res[i], sizeof(struct result));
    close(pipes[i][0]);
    wait();
  }
}

// ---------------------------------------------------------------
// spawn_cpu: CPU-bound child
// Runs for a fixed duration, measuring actual CPU time received
// ---------------------------------------------------------------
void spawn_cpu(int tickets, int pipefd[2])
{
  int pid = fork();
  if (pid == 0) {
    close(pipefd[0]);
    int mypid = getpid();
    settickets(mypid, tickets);

    int start = uptime();
    
    // Run for approximately 800 ticks of wall-clock time (longer for better statistics)
    while(uptime() - start < 800){
      // Do some CPU work to keep busy
      volatile int x = 0;
      int j;
      for (j = 0; j < 100000; j++) x++;
    }
    
    // Get actual CPU time (timer ticks while running)
    int cpu_time = getrunticks();

    struct result r;
    r.pid     = mypid;
    r.tickets = tickets;
    r.ticks   = cpu_time;  // actual CPU time received
    r.kind    = 0;
    write(pipefd[1], &r, sizeof(r));
    close(pipefd[1]);
    exit();
  }
  close(pipefd[1]);
}

// ---------------------------------------------------------------
// spawn_io: IO-bound child (sleeps in a loop)
// ---------------------------------------------------------------
// spawn_io: IO-bound child (sleeps in a loop)
// ---------------------------------------------------------------
void spawn_io(int tickets, int pipefd[2])
{
  int pid = fork();
  if (pid == 0) {
    close(pipefd[0]);
    int mypid = getpid();
    settickets(mypid, tickets);

    int j;
    for (j = 0; j < 100; j++) sleep(5);
    
    // Get actual CPU time (should be very low since mostly sleeping)
    int cpu_time = getrunticks();

    struct result r;
    r.pid     = mypid;
    r.tickets = tickets;
    r.ticks   = cpu_time;
    r.kind    = 1;
    write(pipefd[1], &r, sizeof(r));
    close(pipefd[1]);
    exit();
  }
  close(pipefd[1]);
}

// ---------------------------------------------------------------
// spawn_yield: yield-based child
// ---------------------------------------------------------------
void spawn_yield(int tickets, int pipefd[2])
{
  int pid = fork();
  if (pid == 0) {
    close(pipefd[0]);
    int mypid = getpid();
    settickets(mypid, tickets);

    int j;
    for (j = 0; j < 2500; j++) yield();
    
    // Get actual CPU time (should be low since yielding frequently)
    int cpu_time = getrunticks();

    struct result r;
    r.pid     = mypid;
    r.tickets = tickets;
    r.ticks   = cpu_time;
    r.kind    = 2;
    write(pipefd[1], &r, sizeof(r));
    close(pipefd[1]);
    exit();
  }
  close(pipefd[1]);
}

// ---------------------------------------------------------------
// EXPERIMENT 1 — Equal Tickets (Fairness)
// 8 CPU-bound processes, 10 tickets each
// ---------------------------------------------------------------
void experiment1(void)
{
  int pipes[NEXP_PROCS][2];
  struct result res[NEXP_PROCS];
  int i;

  printf(1, "\n[ EXP 1 ] 8 cpu processes, 10 tickets each -> expect equal share\n");

  for (i = 0; i < NEXP_PROCS; i++) {
    pipe(pipes[i]);
    spawn_cpu(10, pipes[i]);
  }

  sleep(850);  // Wait for all processes to complete their 800-tick run
  collect(pipes, res, NEXP_PROCS);
  print_stats("EXPERIMENT 1: Equal Tickets (Fairness)", res, NEXP_PROCS);
}

// ---------------------------------------------------------------
// EXPERIMENT 2 — Different Tickets (Priority)
// 1 process with 80 tickets, 7 with 10 tickets
// ---------------------------------------------------------------
void experiment2(void)
{
  int pipes[NEXP_PROCS][2];
  struct result res[NEXP_PROCS];
  int i;

  printf(1, "\n[ EXP 2 ] 1 process=80 tickets, 7 processes=10 tickets -> expect pid[0] dominates\n");

  pipe(pipes[0]);
  spawn_cpu(80, pipes[0]);

  for (i = 1; i < NEXP_PROCS; i++) {
    pipe(pipes[i]);
    spawn_cpu(10, pipes[i]);
  }

  sleep(850);  // Wait for all processes to complete their 800-tick run
  collect(pipes, res, NEXP_PROCS);
  print_stats("EXPERIMENT 2: Weighted Tickets (Priority)", res, NEXP_PROCS);
}

// ---------------------------------------------------------------
// EXPERIMENT 3 — CPU vs IO vs Yield
// 3 cpu (10), 3 io (10), 2 yield (10)
// ---------------------------------------------------------------
void experiment3(void)
{
  int pipes[NEXP_PROCS][2];
  struct result res[NEXP_PROCS];
  int i;

  printf(1, "\n[ EXP 3 ] 3 cpu + 3 io + 2 yield, all 10 tickets -> io/yield use fewer ticks\n");

  for (i = 0; i < 3; i++) {
    pipe(pipes[i]);
    spawn_cpu(10, pipes[i]);
  }
  for (i = 3; i < 6; i++) {
    pipe(pipes[i]);
    spawn_io(10, pipes[i]);
  }
  for (i = 6; i < 8; i++) {
    pipe(pipes[i]);
    spawn_yield(10, pipes[i]);
  }

  sleep(600);
  collect(pipes, res, NEXP_PROCS);
  print_stats("EXPERIMENT 3: CPU vs IO vs Yield", res, NEXP_PROCS);
}
// ---------------------------------------------------------------
// EXPERIMENT 4 — Starvation Check
// 1 process with 1 ticket, 7 with 100 tickets
// ---------------------------------------------------------------
void experiment4(void)
{
  int pipes[NEXP_PROCS][2];
  struct result res[NEXP_PROCS];
  int i;

  printf(1, "\n[ EXP 4 ] 1 process=1 ticket, 7 processes=100 tickets -> check starvation\n");

  pipe(pipes[0]);
  spawn_cpu(1, pipes[0]);

  for (i = 1; i < NEXP_PROCS; i++) {
    pipe(pipes[i]);
    spawn_cpu(100, pipes[i]);
  }

  sleep(850);  // Wait for all processes to complete
  collect(pipes, res, NEXP_PROCS);
  print_stats("EXPERIMENT 4: Starvation Test", res, NEXP_PROCS);
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int
main(void)
{
  printf(1, "\n*** LOTTERY SCHEDULER TEST SUITE ***\n");
  printf(1, "4 experiments, 8 processes each\n");

  experiment1();
  experiment2();
  experiment3();
  experiment4();

  printf(1, "*** all tests finished ***\n");
  exit();
}
