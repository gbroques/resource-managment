CC = gcc
CFLAGS = -g -Wall -I.
EXECS = oss user
DEPS = ossshm.c sem.c myclock.c

all: $(EXECS)

oss: $(DEPS)

user: $(DEPS)

clean:
	rm -f *.o $(EXECS)
