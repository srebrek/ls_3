CC=gcc
CFLAGS=-std=gnu99 -Wall -fsanitize=address,undefined
LDFLAGS=-fsanitize=address,undefined
LDLIBS=-lpthread -lm

main: main.c circular_buffer.c circular_buffer.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o main main.c circular_buffer.c $(LDLIBS)

all: main

.PHONY: clean all

clean:
	rm -f main