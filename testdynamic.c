#include "types.h"
#include "stat.h"
#include "user.h"

// this test checks if dynamic ticket adjustment works
// one process sleeps a lot (cooperative) and should gain tickets
// the other just loops forever (cpu hog) and should lose tickets

int
main(void)
{
  int child = fork();

  if(child == 0){
    // child sleeps repeatedly, so it yields voluntarily each time
    // we expect its tickets to go up over time
    int i;
    for(i = 0; i < 8; i++)
      sleep(2);
    printf(1, "child (sleeper) done\n");
    exit();
  } else {
    // parent just burns cpu, never voluntarily yields
    // we expect its tickets to drop over time
    volatile int x = 0;
    int i;
    for(i = 0; i < 3000000; i++)
      x++;
    wait();
    printf(1, "parent (cpu hog) done\n");
  }

  exit();
}
