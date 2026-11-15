CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g

demo: demo.c scheduler.c banker.c scheduler.h banker.h structs.h
	$(CC) $(CFLAGS) -o demo demo.c scheduler.c banker.c

clean:
	rm -f demo

.PHONY: clean
