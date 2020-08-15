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
#include "zlib.h"

// file
#include <fcntl.h>

// sockets
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

const char* HOST="localhost";
const int BUFFER_SIZE = 4096;
const int DEFLATED_SIZE = 1024;
static int logfd = -1;
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

#pragma mark Input mode
struct termios saved_attributes;

void resetInputMode(void) {
  int ret = tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
  if (ret == -1)
    exitWithErrorReason("Failed to save current terminal attributes");
}

void setInputMode(void) {
  struct termios tattr;
  // char *name;

  // stdin is terminal
  if (!isatty(STDIN_FILENO)) {
    exitWithError("Standard input doesn't point to a terminal");
  }

  // save for restoration
  int st0 = tcgetattr(STDIN_FILENO, &saved_attributes);
  if (st0 == -1)
    exitWithErrorReason("Failed to retrieve current terminal attributes");
  atexit(resetInputMode);

  // set attr to non-canonical mode
  int st1 = tcgetattr(STDIN_FILENO, &tattr);
  if (st1 == -1)
    exitWithErrorReason("Failed to retrieve current terminal attributes");

  tattr.c_iflag = ISTRIP;
  tattr.c_oflag = 0;
  tattr.c_lflag = 0;

  int st2 = tcsetattr(STDIN_FILENO, TCSANOW, &tattr);
  if (st2 == -1)
    exitWithErrorReason("Failed to save terminal attributes for non-canonical input mode");
}

#pragma mark Argument related
// returns an fd to the logfile
void setupLog(char *filename) {
  int fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0666);
  if (fd == -1)
    exitWithErrorReason("Failed to create/open a log file");

  logfd = fd;
}

void setupPort(char *strPort) {
  port = atoi(strPort);
  if (port <= 1024)
    exitWithError("Please use a port number greater than 1024");
}

void parseArguments(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"port", required_argument, 0, 'p'},
    {"log", required_argument, 0, 'l'},
    {"compress", no_argument, &compress_flag, 1},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "p:l:", long_options, NULL)) != -1) {
    switch (opt) {
      case 0:
        break;
      case 'l':
        setupLog(optarg);
        break;
      case 'p':
        setupPort(optarg);
        break;
      default: {
        exitWithError("Usage: lab1b-client --port=<number> [--log=<filename>] [--compress]");
      }
    }
  }

  if (port == -1)
    exitWithError("Port is required.\nUsage: lab1b-client --port=<number> [--log=<filename>] [--compress]");
}

#pragma mark Server setup
// returns a sockfd to the sever
int connectToServer() {
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    exitWithErrorReason("Failed to open a socket");

  server = gethostbyname(HOST);
  if (server == NULL)
    exitWithErrorReason("Couldn't locate the host with specified name");

  memset((char *) &serv_addr, '\0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    exitWithErrorReason("Failed to connect to server");

  return sockfd;
}

#pragma mark I/O
void serverIO(int sockfd) {
  unsigned char buf[BUFFER_SIZE];
  unsigned char serverBuf[BUFFER_SIZE];
  unsigned char compressionBuf[BUFFER_SIZE];

  // setup poll struct
  struct pollfd fds[2];
  int timeout_msecs = 0;
  fds[0].fd = STDIN_FILENO;
  fds[1].fd = sockfd;
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[1].events = POLLIN | POLLHUP | POLLERR;

  while (1) {
    int ret = poll(fds, 2, timeout_msecs);
    if (ret == -1)
      exitWithErrorReason("Failed to call poll()");
    if (ret > 0) {
      // read input from keyboard
      if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
        int bytes = read(STDIN_FILENO, buf, BUFFER_SIZE);
        if (bytes == -1)
          exitWithErrorReason("Failed to read from stdin");
        for (int i = 0; i < bytes; i++) {
          char* strStdout = buf + i;
          char strStdoutSize = 1;

          switch(buf[i]) {
            case '\004': // ^D
              strStdout = "^D";
              strStdoutSize = 2;
              break;
            case '\012': // \n
            case '\015': // \r
              strStdout = "\r\n";
              strStdoutSize = 2;
              break;
            case '\003': // ^C
              strStdout = "^C";
              strStdoutSize = 2;
              break;
            default:
              break;
          }

          if (write(STDOUT_FILENO, strStdout, strStdoutSize) == -1)
            exitWithErrorReason("Failed to write to stdout");
        }
        if (bytes > 0) {
          unsigned char *writeBuf = compress_flag ? compressionBuf : buf;
          if (compress_flag) {
            z_stream z_toServer;
            z_toServer.zalloc = Z_NULL;
            z_toServer.zfree = Z_NULL;
            z_toServer.opaque = Z_NULL;
            if (deflateInit(&z_toServer, Z_DEFAULT_COMPRESSION) != Z_OK)
              exitWithErrorOwnReason("Failed to initalize compression", z_toServer.msg);
            z_toServer.avail_in = bytes;
            z_toServer.next_in = buf;
            z_toServer.avail_out = BUFFER_SIZE;
            z_toServer.next_out = compressionBuf;
            do {
              deflate(&z_toServer, Z_SYNC_FLUSH);
            } while (z_toServer.avail_in > 0);
            bytes = BUFFER_SIZE - z_toServer.avail_out;
            deflateEnd(&z_toServer);
          }

          if (write(sockfd, writeBuf, bytes) == -1) {
            exitWithErrorReason("Failed to write to server via socket");
          }

          if (logfd != -1) {
            int ret = dprintf(logfd, "SENT %d bytes: ", bytes);
            ret |= write(logfd, writeBuf, bytes);
            ret |= dprintf(logfd, "\n");
            if (ret < 0)
              exitWithErrorReason("Failed to write to the log file");
          }
        }
      }

      // read input from server
      if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
        int bytes = read(sockfd, serverBuf, DEFLATED_SIZE);
        if (bytes == -1)
          exitWithErrorReason("Failed to read from server");
        if (bytes == 0) { // server pipe's write end has closed
          if (logfd != -1)
            close(logfd);
          exit(0);
        }
        if (logfd != -1) {
          int ret = dprintf(logfd, "RECEIVED %d bytes: ", bytes);
          ret |= write(logfd, serverBuf, bytes);
          ret |= dprintf(logfd, "\n");
          if (ret < 0)
            exitWithErrorReason("Failed to write to the log file");
        }
        unsigned char *readBuf = compress_flag ? compressionBuf : serverBuf;
        if (compress_flag) {
          z_stream z_fromServer;
          z_fromServer.zalloc = Z_NULL;
          z_fromServer.zfree = Z_NULL;
          z_fromServer.opaque = Z_NULL;
          if (inflateInit(&z_fromServer) != Z_OK)
            exitWithErrorOwnReason("Failed to intialize decompression", z_fromServer.msg);
          z_fromServer.avail_in = bytes;
          z_fromServer.next_in = serverBuf;
          z_fromServer.avail_out = BUFFER_SIZE;
          z_fromServer.next_out = compressionBuf;
          do {
            inflate(&z_fromServer, Z_SYNC_FLUSH);
          } while (z_fromServer.avail_in > 0);
          bytes = BUFFER_SIZE - z_fromServer.avail_out;
          inflateEnd(&z_fromServer);
        }
        for (int i = 0; i < bytes; i++) {
          char *strToStdout = readBuf + i;
          int strLen = 1;

          switch(readBuf[i]) {
            case '\012': // \n
            case '\015': // \r
              strToStdout = "\r\n";
              strLen = 2;
              break;
            default: break;
          }
          if (write(STDOUT_FILENO, strToStdout, strLen) == -1)
            exitWithErrorReason("Failed to write to stdout");
        }
      }
    }
  }
}

#pragma mark Main

int main(int argc, char *argv[]) {
  // set to noncanonical input
  parseArguments(argc, argv);
  setInputMode();

  int sockfd = connectToServer();
  serverIO(sockfd);
}
