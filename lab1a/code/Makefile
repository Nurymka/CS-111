OPTIMIZE = -O2
DEBUG = -g3

CC = gcc
CFLAGS = -Wall -Wextra

BUILDFLAGS = $(OPTIMIZE) $(FLAGS)
DEBUGFLAGS = $(DEBUG) $(FLAGS)

default: lab1a

debug: lab1a.c
	$(CC) $(DEBUGFLAGS) lab1a.c -o lab1a_debug

lab1a: lab1a.c
	$(CC) $(BUILDFLAGS) lab1a.c -o $@

dist: lab1a.c README Makefile
	tar -cvzf lab1a-404648263.tar.gz lab1a.c README Makefile

clean:
	rm -f lab1a_debug lab1a lab1a-404648263.tar.gz
