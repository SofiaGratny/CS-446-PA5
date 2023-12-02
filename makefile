CC=gcc
CFLAGS=-Wall -g

all: myfs

myalloc.o: myalloc.c
	$(CC) $(CFLAGS) -c myfs.c

myalloc: myalloc.o
	$(CC) $(CFLAGS) -o myfs myfs.o

clean:
	rm -f *.o myfs