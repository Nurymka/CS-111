# NAME: Nurym Kudaibergen
# ID: 404648263
# EMAIL: nurim98@gmail.com
CC = gcc
OPTIMIZER = -O2
CFLAGS = -Wall -Wextra
LIBS = -lmraa -lm

# BUILDFLAGS = $(OPTIMIZER) $(CFLAGS) $(LIBS)
BUILDFLAGS = $(CFLAGS) $(LIBS)

default: build

build: lab4b.c
	$(CC) $(BUILDFLAGS) lab4b.c -o lab4b

check: build
	./smoketest.sh

dist: build
	tar -czvf lab4b-404648263.tar.gz lab4b.c Makefile README smoketest.sh

clean:
	rm -f lab4b-404648263.tar.gz lab4b
