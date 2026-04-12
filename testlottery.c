#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  int pid = getpid();
  printf(1, "My PID: %d\n", pid);

  if(settickets(pid, 10) < 0){
    printf(1, "settickets failed\n");
    exit();
  }
  printf(1, "Set tickets to 10 for PID %d\n", pid);

  int child = fork();
  if(child == 0){
    settickets(getpid(), 2);
    printf(1, "Child PID %d has 2 tickets\n", getpid());
    volatile int x = 0;
    for(int i = 0; i < 1000000; i++) x++;
    printf(1, "Child done\n");
    exit();
  } else {
    printf(1, "Parent PID %d has 10 tickets\n", getpid());
    volatile int x = 0;
    for(int i = 0; i < 1000000; i++) x++;
    wait();
    printf(1, "Parent done\n");
  }
  exit();
}
