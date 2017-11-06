#ifndef MYCLOCK_H
#define MYCLOCK_H

#define NANOSECS_PER_SEC 1000000000  // 1 * 10^9 nanoseconds

/*
 * A simple simulated clock
 * ------------------------*/
struct my_clock {
  unsigned int secs;        // Amount of time in seconds
  unsigned int nanosecs;    // Amount of time in nanoseconds
};

#endif