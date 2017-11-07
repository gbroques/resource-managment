#include "myclock.h"

struct my_clock add_nanosecs_to_clock(struct my_clock clock, int nanosecs) {
  struct my_clock new_time;
  new_time.secs = clock.secs;
  new_time.nanosecs = clock.nanosecs + nanosecs;
  if (new_time.nanosecs >= NANOSECS_PER_SEC) {
    int secs = new_time.nanosecs / NANOSECS_PER_SEC;
    new_time.secs += secs;
    new_time.nanosecs -= secs * NANOSECS_PER_SEC;
  }
  return new_time;
}