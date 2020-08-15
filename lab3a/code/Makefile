# NAME: Nurym Kudaibergen
# EMAIL: nurim98@gmail.com
# ID: 404648263

OPTIMIZE = -O2

CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lm

BUILDFLAGS = $(OPTIMIZE) $(CFLAGS) $(LIBS)

default:
	$(CC) $(BUILDFLAGS) lab3a.c -o lab3a

dist: default
	tar -czvf lab3a-404648263.tar.gz README Makefile ext2_fs.h lab3a.c

clean:
	rm -f lab3a-404648263.tar.gz lab3a
