CC = gcc
CFLAGS = -g -Wall -I.
EXECS = oss user

all: $(EXECS)

oss: ossshm.c

user: ossshm.c

clean:
	rm -f *.o $(EXECS)
