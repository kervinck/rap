
CC:=gcc-mp-4.9
CFLAGS:=-Wall -O3 -std=c99 -pedantic

all: rap test

rap: main.o rap.o assemble.o library.o cplus.o
	$(CC) -o $@ $^

test: rap test.rap
	./rap < test.rap

clean:
	rm -f *.o

# vi: noexpandtab
