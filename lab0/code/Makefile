# NAME: Nurym Kudaibergen
# EMAIL: nurim98@gmail.com
# ID: 404648263

OPTIMIZE = -O2
DEBUG = -g3

CC = gcc
FLAGS = -Wall -Wextra

BUILDFLAGS = $(OPTIMIZE) $(FLAGS)
DEBUGFLAGS = $(DEBUG) $(FLAGS)

default: lab0

debug: lab0.c
	$(CC) $(DEBUGFLAGS) lab0.c -o lab0_debug

lab0: lab0.c
	$(CC) $(BUILDFLAGS) lab0.c -o $@

check: lab0 lab0_test.sh
	./lab0_test.sh

dist: lab0.c lab0_test.sh backtrace.png breakpoint.png README Makefile
	tar -czvf lab0-404648263.tar.gz lab0.c lab0_test.sh backtrace.png breakpoint.png README Makefile

clean:
	rm -f lab0 lab0_output unwritable.txt unopenable.txt lab0-404648263.tar.gz
