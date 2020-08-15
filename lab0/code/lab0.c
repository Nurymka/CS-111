// NAME: Nurym Kudaibergen
// EMAIL: nurim98@gmail.com
// ID: 404648263

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>

static int segfault_flag = 0;
static int catch_flag = 0;
const unsigned int BUFFER_SIZE = 200;
const char option_segfault[] = "segfault";
const char option_catch[] = "catch";
const char option_input[] = "input";
const char option_output[] = "output";

void reportErrorAndExit(const char *msg, const char *option, const char *filename, int code) {
  if (option != NULL)
    fprintf(stderr, "Problem with option: %s.\n", option);

  if (filename != NULL)
    fprintf(stderr, "%s (%s): %s\n", msg, filename, strerror(errno));
  else
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));

  exit(code);
}

void simpleErrorAndExit(const char *msg, int code) {
  fprintf(stderr, "%s\n", msg);
  exit(code);
}

void setStdinTo(const char *filename) {
  int ifd = open(filename, O_RDONLY);
  if (ifd == -1)
    reportErrorAndExit("Couldn't open the specified input file", option_input, filename, 2);
  if (ifd >= 0) {
    close(0);
    dup(ifd);
    close(ifd);
  }
}

void setStdoutTo(const char *filename) {
  int ofd = creat(filename, 0666);
  if (ofd == -1)
    reportErrorAndExit("Couldn't create the specified output file", option_output, filename, 3);
  if (ofd >= 0) {
    close(1);
    dup(ofd);
    close(ofd);
  }
}

void segfault() {
  char *null = NULL;
  *null = 'n';
}

void segfault_handler() {
  simpleErrorAndExit("Successfully caught the segmentation fault, exiting now.", 4);
}

int main(int argc, char *argv[]) {
  // parse arguments
  static struct option long_options[] = {
    {option_segfault, no_argument, &segfault_flag, 1},
    {option_catch, no_argument, &catch_flag, 1},
    {option_input, required_argument, 0, 'i'},
    {option_output, required_argument, 0, 'o'},
    {0, 0, 0, 0}
  };
  int opt, opt_idx;
  char *i_filename = NULL; char *o_filename = NULL;
  while ((opt = getopt_long(argc, argv, "i:o:", long_options, &opt_idx)) != -1) {
    switch (opt) {
      case 0:
        break;
      case 'i':
        i_filename = malloc(strlen(optarg));
        strcpy(i_filename, optarg);
        setStdinTo(optarg);
        break;
      case 'o':
        o_filename = malloc(strlen(optarg));
        strcpy(o_filename, optarg);
        setStdoutTo(optarg);
        break;
      default: {
        simpleErrorAndExit("Usage: lab0 [--input=<filename>] [--output=<filename>] [--segfault]", 1);
      }
    }
  }

  // check if flags were specified
  if (catch_flag)
    signal(SIGSEGV, segfault_handler);
  if (segfault_flag)
    segfault();

  // copies from the stdin to stdout
  char *start_str = malloc(BUFFER_SIZE); // contains the whole input string
  if (start_str == NULL)
    reportErrorAndExit("Can't allocate memory to store the input", NULL, NULL, 2);
  unsigned int size = 0;
  int read_bytes = -1;
  unsigned int cur_bufsize = BUFFER_SIZE;

  while ((read_bytes = read(0, start_str + size, BUFFER_SIZE)) > 0) {
    size += read_bytes;
    if (cur_bufsize - size <= BUFFER_SIZE) { // if there is less buffer space than read() reads
      cur_bufsize += BUFFER_SIZE;
      start_str = realloc(start_str, cur_bufsize);
      if (start_str == NULL)
        reportErrorAndExit("Can't allocate memory to store the input", NULL, NULL, 2);
    }
  }
  if (read_bytes == -1)
    reportErrorAndExit("Error while reading from the input", NULL, i_filename, 2);

  // write to stdout
  int write_bytes = write(1, start_str, size);
  if (write_bytes == -1)
    reportErrorAndExit("Error while writing to output", NULL, o_filename, 3);

  // free up memory
  free(start_str);
  exit(0);
}
