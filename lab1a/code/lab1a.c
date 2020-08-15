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

const int BUFFER_SIZE = 50;
static int shellFlag = 0;

#pragma mark Exit with message
void exitWithError(const char *msg) {
  fprintf(stderr, "%s\r\n", msg);
  exit(1);
}
void exitWithErrorReason(const char *msg) {
  fprintf(stderr, "%s: %s\r\n", msg, strerror(errno));
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
  char *name;

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

#pragma mark Program setup
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
    {"shell", no_argument, &shellFlag, 1},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
    switch (opt) {
      case 0:
        break;
      default: {
        exitWithError("Usage: lab1a [--shell]");
      }
    }
  }
}

#pragma mark Main I/O
void simpleIO() {
  char buf[BUFFER_SIZE];
  while (1) {
    int bytes = read(STDIN_FILENO, buf, BUFFER_SIZE);
    if (bytes == -1)
      exitWithErrorReason("Failed to read from stdin");
    for (int i = 0; i < bytes; i++) {
      if (buf[i] == '\004') { // C-d
        if (write(STDOUT_FILENO, "^D", 2) == -1)
          exitWithErrorReason("Failed to write to stdout");
        return;
      } else if (buf[i] == '\003') { // C-c
        if (write(STDOUT_FILENO, "^C", 2) == -1)
          exitWithErrorReason("Failed to write to stdout");
      } else if (buf[i] == '\012' || buf[i] == '\015') { // \n or \r
        if (write(STDOUT_FILENO, "\r\n", 2) == -1)
          exitWithErrorReason("Failed to write to stdout");
      } else {
        if (write(STDOUT_FILENO, buf + i, 1) == -1)
          exitWithErrorReason("Failed to write to stdout");
      }
    }
  }
}

void shellIO(int fdToShell, int fdFromShell, int childPid) {
  char buf[BUFFER_SIZE];
  char shellBuf[BUFFER_SIZE];

  // setup poll struct
  struct pollfd fds[2];
  int timeout_msecs = 0;
  fds[0].fd = STDIN_FILENO;
  fds[1].fd = fdFromShell;
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
          if (buf[i] == '\004') { // C-d
            if (write(STDOUT_FILENO, "^D", 2) == -1)
              exitWithErrorReason("Failed to write to stdout");
            if (close(fdToShell) == -1)
              exitWithErrorReason("Failed to close write end of shell pipe");
            break;
          } else if (buf[i] == '\012' || buf[i] == '\015') { // \n or \r
            if (write(STDOUT_FILENO, "\r\n", 2) == -1)
              exitWithErrorReason("Failed to write to stdout");
            if (write(fdToShell, "\n", 1) == -1)
              exitWithErrorReason("Failed to write to shell pipe");
          } else if (buf[i] == '\003') { // ^C
            if (write(STDOUT_FILENO, "^C", 2) == -1)
              exitWithErrorReason("Failed to write to stdout");
            if (kill(childPid, SIGINT) == -1)
              exitWithErrorReason("Failed to cause SIGINT in shell");
          } else {
            if (write(STDOUT_FILENO, buf + i, 1) == -1)
              exitWithErrorReason("Failed to write to stdout");
            if (write(fdToShell, buf + i, 1) == -1)
              exitWithErrorReason("Failed to write to shell pipe");
          }
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
          exit(0);
        }
        for (int i = 0; i < bytes; i++) {
          if (shellBuf[i] == '\012') { // \n
            if (write(STDOUT_FILENO, "\r\n", 2) == -1)
              exitWithErrorReason("Failed to write to stdout");
          } else {
            if (write(STDOUT_FILENO, shellBuf + i, 1) == -1)
              exitWithErrorReason("Failed to write to stdout");
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  parseArguments(argc, argv);

  int fdToShell = -1;
  int fdFromShell = -1;
  int childPid = -1;
  if (shellFlag)
    forkChildShell(&fdToShell, &fdFromShell, &childPid);
  // set to noncanonical input
  setInputMode();

  if (shellFlag)
    shellIO(fdToShell, fdFromShell, childPid);
  else
    simpleIO();
}
