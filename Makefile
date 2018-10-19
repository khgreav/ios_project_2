CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lpthread

all:
	$(CC) $(CFLAGS) proj2.c -o proj2 $(LFLAGS)

clean:
	rm proj2
	rm proj2.out
