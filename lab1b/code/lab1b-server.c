// NAME: Nurym Kudaibergen
// EMAIL: nurim98@gmail.com
// ID: 404648263

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>

// sockets
#include <sys/socket.h>
#include <netinet/in.h>

// zlib
#include "zlib.h"

const int BUFFER_SIZE = 4096;
const int DEFLATED_SIZE = 1024;
static int port = -1;
static int compress_flag = 0;

#pragma mark Exit with message
void exitWithError(const char *msg) {
  fprintf(stderr, "%s\r\n", msg);
  exit(1);
}
void exitWithErrorReason(const char *msg) {
  fprintf(stderr, "%s: %s\r\n", msg, strerror(errno));
  exit(1);
}
void exitWithErrorOwnReason(const char *msg, const char *reason) {
  fprintf(stderr, "%s: %s\r\n", msg, reason);
  exit(1);
}

#pragma mark Program setup
// returns sockfd of client
int setupServer() {
  socklen_t client_addr_len;
  int sockfd, newsockfd;

  struct sockaddr_in serv_addr, cli_addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    exitWithErrorReason("Failed to open a socket");

  memset((char *) &serv_addr, '\0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    exitWithErrorReason("Failed to bind socket to address");

  listen(sockfd, 5);

  client_addr_len = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_len);
  if (newsockfd < 0)
    exitWithErrorReason("Failed to establish a connection to the client");

  return newsockfd;
}

void forkChildShell(int *fdToShell, int *fdFromShell, int *childPid) {
  int toShell[2];
  int fromShell[2];
  pid_t child_pid = -1;

  if (pipe(toShell) == -1)
    exitWithErrorReason("Failed to create a pipe");
  if (pipe(fromShell) == -1)
    exitWithErrorReason("Failed to create a pipe");

  child_pid = fork();
  if (child_pid == -1)
    exitWithErrorReason("Failed to fork current process");

  if (child_pid > 0) { // for process
    // close unneeded pipes
    if (close(toShell[0]) == -1)
      exitWithErrorReason("Failed to close unneeded pipe in parent process");
    if (close(fromShell[1]) == -1)
      exitWithErrorReason("Failed to close unneeded pipe in parent process");
    *fdToShell = toShell[1];
    *fdFromShell = fromShell[0];
    *childPid = child_pid;
  } else if (child_pid == 0) { // for child
    // close unneeded pipes
    if (close(toShell[1]) == -1)
      exitWithErrorReason("Failed to close unneeded pipe in child process");
    if (close(fromShell[0]) == -1)
      exitWithErrorReason("Failed to close unneeded pipe in child process");

    // set stdin/out/err to pipes
    if (dup2(toShell[0], STDIN_FILENO) == -1)
      exitWithErrorReason("Failed to set stdin to a pipe from terminal process");
    if (dup2(fromShell[1], STDOUT_FILENO) == -1)
      exitWithErrorReason("Failed to set stdout to a pipe to the terminal process");
    if (dup2(fromShell[1], STDERR_FILENO) == -1)
      exitWithErrorReason("Failed to set stderr to a pipe to the terminal process");

    // close remaining pipes
    if (close(toShell[0]) == -1)
      exitWithErrorReason("Failed to close unneeded pipe in child process");
    if (close(fromShell[1]) == -1)
      exitWithErrorReason("Failed to close unneeded pipe in child process");

    // exec
    char *execvp_argv[2];
    char execvp_filename[] = "/bin/bash";
    execvp_argv[0] = execvp_filename;
    execvp_argv[1] = NULL;
    if (execvp(execvp_filename, execvp_argv) == -1)
      exitWithErrorReason("Failed to exec /bin/bash in child process");
  }
}

void parseArguments(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"port", required_argument, 0, 'p'},
    {"compress", no_argument, &compress_flag, 1},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "p:", long_options, NULL)) != -1) {
    switch (opt) {
      case 0:
        break;
      case 'p':
        port = atoi(optarg);
        if (port <= 1024)
          exitWithError("Please use a port number greater than 1024");
        break;
      default: {
        exitWithError("Usage: lab1b --port=<number> [--compress]");
      }
    }
  }

  if (port == -1)
    exitWithError("Port is required. Usage: lab1b --port=<number>");
}

#pragma mark I/O
void shellIO(int sockfd, int fdToShell, int fdFromShell, int childPid) {
  unsigned char socketBuf[BUFFER_SIZE];
  unsigned char shellBuf[BUFFER_SIZE];
  unsigned char compressionBuf[BUFFER_SIZE];

  // setup poll struct
  struct pollfd fds[2];
  int timeout_msecs = 0;
  fds[0].fd = sockfd;
  fds[1].fd = fdFromShell;
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[1].events = POLLIN | POLLHUP | POLLERR;

  while (1) {
    int ret = poll(fds, 2, timeout_msecs);
    if (ret == -1)
      exitWithErrorReason("Failed to call poll()");

    if (ret <= 0)
      continue;

    // read input from client
    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      int bytes = read(sockfd, socketBuf, DEFLATED_SIZE);
      if (bytes == -1)
        exitWithErrorReason("Failed to read from socket");
      unsigned char *readBuf = compress_flag ? compressionBuf : socketBuf;
      if (compress_flag && bytes > 0) {
        z_stream z_fromClient;
        z_fromClient.zalloc = Z_NULL;
        z_fromClient.zfree = Z_NULL;
        z_fromClient.opaque = Z_NULL;
        if (inflateInit(&z_fromClient) != Z_OK)
          exitWithErrorOwnReason("Failed to intialize decompression", z_fromClient.msg);
        z_fromClient.avail_in = bytes;
        z_fromClient.next_in = socketBuf;
        z_fromClient.avail_out = BUFFER_SIZE;
        z_fromClient.next_out = compressionBuf;
        do {
          inflate(&z_fromClient, Z_SYNC_FLUSH);
        } while (z_fromClient.avail_in > 0);
        bytes = BUFFER_SIZE - z_fromClient.avail_out;
        inflateEnd(&z_fromClient);
      }
      for (int i = 0; i < bytes; i++) {
        unsigned char *charToShell = readBuf + i;

        switch (readBuf[i]) {
          case '\004': // ^D
            if (close(fdToShell) == -1)
              exitWithErrorReason("Failed to close write end of shell pipe");
            break;
          case '\012': // \n
          case '\015': // \r
            *charToShell = '\n';
            if (write(fdToShell, charToShell, 1) == -1)
              exitWithErrorReason("Failed to write to shell pipe");
            break;
          case '\003': // ^C
            if (kill(childPid, SIGINT) == -1)
              exitWithErrorReason("Failed to cause SIGINT in shell");
            break;
          default:
            if (write(fdToShell, charToShell, 1) == -1)
              exitWithErrorReason("Failed to write to shell pipe");
            break;
        }
      }
      if (bytes == 0) { // EOF/error from client
        close(fdToShell);
      }
    }
    // read input from shell
    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
      int bytes = read(fdFromShell, shellBuf, BUFFER_SIZE);
      if (bytes == -1)
        exitWithErrorReason("Failed to read from child shell");
      if (bytes == 0) { // toShell pipe's write end has closed
        int status;
        waitpid(childPid, &status, 0);
        int sig = status & 0x7f;
        int st = (status & 0xff00) >> 8;
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", sig, st);
        shutdown(sockfd, 2);
        exit(0);
      }
      unsigned char *writeBuf = compress_flag ? compressionBuf : shellBuf;
      if (compress_flag) {
        z_stream z_toClient;
        z_toClient.zalloc = Z_NULL;
        z_toClient.zfree = Z_NULL;
        z_toClient.opaque = Z_NULL;
        if (deflateInit(&z_toClient, Z_DEFAULT_COMPRESSION) != Z_OK)
          exitWithErrorOwnReason("Failed to initalize compression", z_toClient.msg);
        z_toClient.avail_in = bytes;
        z_toClient.next_in = shellBuf;
        z_toClient.avail_out = BUFFER_SIZE;
        z_toClient.next_out = compressionBuf;
        do {
          deflate(&z_toClient, Z_SYNC_FLUSH);
        } while (z_toClient.avail_in > 0);
        bytes = BUFFER_SIZE - z_toClient.avail_out;
        deflateEnd(&z_toClient);
      }
      if (write(sockfd, writeBuf, bytes) == -1)
        exitWithErrorReason("Failed to write to socket");
    }
  }
}

int main(int argc, char *argv[]) {
  parseArguments(argc, argv);

  int sockfd = setupServer();

  int fdToShell = -1;
  int fdFromShell = -1;
  int childPid = -1;
  forkChildShell(&fdToShell, &fdFromShell, &childPid);

  shellIO(sockfd, fdToShell, fdFromShell, childPid);
}
