CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O1

.PHONY: all test clean

all: ime_test

ime_test: ime_test.c ../nuklear.h
	$(CC) $(CFLAGS) -o $@ ime_test.c

test: ime_test
	./ime_test

clean:
	rm -f ime_test
