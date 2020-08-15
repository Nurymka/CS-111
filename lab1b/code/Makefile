# NAME: Nurym Kudaibergen
# EMAIL: nurim98@gmail.com
# ID: 404648263

OPTIMIZE = -O2
DEBUG = -g3

CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lz

BUILDFLAGS = $(OPTIMIZE) $(FLAGS) $(LIBS)
DEBUGFLAGS = $(DEBUG) $(FLAGS) $(LIBS)

default: lab1b-server lab1b-client

lab1b-server: lab1b-server.c
	$(CC) $(BUILDFLAGS) $< -o $@

lab1b-client: lab1b-client.c
	$(CC) $(BUILDFLAGS) $< -o $@

dist:
	tar -czvf lab1b-404648263.tar.gz README Makefile lab1b-client.c lab1b-server.c

clean:
	rm -f client-debug server-debug lab1b-server lab1b-client lab1b-404648263.tar.gz

# DEBUG

debug: client-debug server-debug

client-debug: lab1b-client.c
	$(CC) $(DEBUGFLAGS) $< -o $@

server-debug: lab1b-server.c
	$(CC) $(DEBUGFLAGS) $< -o $@
