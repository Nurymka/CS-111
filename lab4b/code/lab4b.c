// NAME: Nurym Kudaibergen
// ID: 404648263
// EMAIL: nurim98@gmail.com
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <mraa.h>

// scale modes
#define SCALE_FAHRENHEIT 0
#define SCALE_CELCIUS 1

// define ports
#define AIO_TEMPERATURE 1 // matches A0
#define DIO_BUTTON 62 // matches GPIO_51

const int BUFFER_SIZE = 512;
const int BUFFER_THRESHOLD = 400; // after reading BUFFER_THRESHOLD bytes into buffer, it gets cleaned up
const int READ_SIZE = 32;
const char *g_usage_str = "Usage: [--period=<number>] [--log=<filename>] [--scale={C, F}]";

static int g_sampling_interval = 1;
static int g_scale = SCALE_FAHRENHEIT;
static FILE* g_logfile = NULL;
static mraa_aio_context g_tempsensor;
static mraa_gpio_context g_button;

// Error
void exitWithError(const char *msg, int code) {
  fprintf(stderr, "%s\n", msg);
  exit(code);
}
void exitWithErrorReason(const char *msg, int code) {
  perror(msg);
  exit(code);
}

// End

void cleanup() {
  if (g_logfile != NULL) {
    if (fclose(g_logfile) != 0)
      exitWithErrorReason("Failed to close the log file", 1);
  }
}

// Setup

void parseScale(const char *scale) {
  if (strcmp(scale, "F") == 0)
    g_scale = SCALE_FAHRENHEIT;
  else if (strcmp(scale, "C") == 0)
    g_scale = SCALE_CELCIUS;
  else
    exitWithError(g_usage_str, 1);
}

void parseArguments(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"period", required_argument, 0, 'p'},
    {"scale", required_argument, 0, 's'},
    {"log", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };

  int opt;
  while((opt = getopt_long(argc, argv, "p:s:l:", long_options, NULL)) != -1) {
    switch(opt) {
      case 'p':
        g_sampling_interval = atoi(optarg);
        break;
      case 's':
        parseScale(optarg);
        break;
      case 'l':
        g_logfile = fopen(optarg, "w");
        if (g_logfile == NULL)
          exitWithErrorReason("Failed to open/create the log file", 1);
        break;
      default:
        exitWithError(g_usage_str, 1);
    }
  }
}

void parseCmd(char *cmdStart, int cmdLen, int *isStopped, int *shutdown) {
  int isInvalid = 0;

  if (cmdLen == 3 && strncmp("OFF", cmdStart, cmdLen) == 0) {
    *shutdown = 1;
  } else if (cmdLen == 7 && strncmp("SCALE=", cmdStart, 6) == 0) {
    char scale = cmdStart[6];
    if (scale == 'F')
      g_scale = SCALE_FAHRENHEIT;
    else if (scale == 'C')
      g_scale = SCALE_CELCIUS;
  } else if (cmdLen >= 8 && strncmp("PERIOD=", cmdStart, 7) == 0) {
    char *sec_str = strtok(cmdStart + 7, "\n");
    g_sampling_interval = atoi(sec_str);
  } else if (cmdLen == 4 && strncmp("STOP", cmdStart, cmdLen) == 0) {
    *isStopped = 1;
  } else if (cmdLen == 5 && strncmp("START", cmdStart, cmdLen) == 0) {
    *isStopped = 0;
  } else if (cmdLen > 4 && strncmp("LOG ", cmdStart, 4) == 0) {
    // char *log_str = strtok(cmdStart + 5, "\n");
    // will be used in 4C
  } else {
    isInvalid = 1;
    fprintf(stdout, "Error: invalid command\n");
  }

  if (!isInvalid && g_logfile != NULL) {
    fwrite(cmdStart, sizeof(char), cmdLen, g_logfile);
    fwrite("\n", sizeof(char), 1, g_logfile);
  }
}

// I/O

void printShutdown(char *outbuf, struct timeval *clock) {
  struct tm *now = localtime(&(clock->tv_sec));
  sprintf(outbuf, "%02d:%02d:%02d SHUTDOWN\n",
    now->tm_hour, now->tm_min, now->tm_sec);
  fputs(outbuf, stdout);
  if (g_logfile != NULL)
    fputs(outbuf, g_logfile);
}

float getTemperature(int scale_mode) {
  int reading = mraa_aio_read(g_tempsensor);

  int B = 4275;
  float R0 = 100000.0;
  float R = 1023.0/((float)reading) - 1.0;
  R = R0*R;
  float C = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;

  return scale_mode == SCALE_FAHRENHEIT ? (C * 9)/5 + 32 : C;
}

void sampleTemperature(char *outbuf, time_t *nextDue, struct timeval *clock) {
  // get current time
  gettimeofday(clock, 0);

  // the time for sampling has come
  if (clock->tv_sec >= *nextDue) {
      float temp = getTemperature(g_scale);
      int t = temp * 10;

      struct tm *now = localtime(&(clock->tv_sec));
      sprintf(outbuf, "%02d:%02d:%02d %d.%1d\n",
        now->tm_hour, now->tm_min, now->tm_sec,
        t/10, t%10);
      fputs(outbuf, stdout);
      if (g_logfile != NULL)
        fputs(outbuf, g_logfile);

      *nextDue = clock->tv_sec + g_sampling_interval;
  }
}

void tryCleanupBuffer(char *cmdBuf, int *cmdOffset, int *readOffset) {
  int remnantsLen = *readOffset - *cmdOffset;
  memmove(cmdBuf, cmdBuf + *cmdOffset, remnantsLen);
  memset(cmdBuf + remnantsLen, '\0', BUFFER_SIZE - remnantsLen);
  *cmdOffset = 0;
  *readOffset = remnantsLen;
}

void IO() {
  // init I/O
  g_tempsensor = mraa_aio_init(AIO_TEMPERATURE);
  g_button = mraa_gpio_init(DIO_BUTTON);
  mraa_gpio_dir(g_button, MRAA_GPIO_IN);

  // temperature vars
  char outbuf[BUFFER_SIZE];
  struct timeval clock;
  time_t nextDue = 0;

  // stdin cmd vars
  struct pollfd pollStdin = { 0, POLLIN, 0 };
  char cmdbuf[BUFFER_SIZE];
  int cmdOffset = 0;
  int readOffset = 0;

  // loop vars
  int shutdown = 0;
  int isStopped = 0;

  // main loop
  while (!shutdown) {
    if (!isStopped)
      sampleTemperature(outbuf, &nextDue, &clock);

    pollStdin.revents = 0;
    int ret = poll(&pollStdin, 1, 0);
    if (ret >= 1) {
      int bytes = read(STDIN_FILENO, cmdbuf + readOffset, READ_SIZE);
      if (bytes == -1)
        exitWithErrorReason("Failed to read from stdin", 1);
      int i = 0;

      for (; i < bytes; i++) {
        if (*(cmdbuf + readOffset + i) == '\n') {
          char *cmdStart = cmdbuf + cmdOffset;
          int cmdLen = readOffset + i - cmdOffset;
          parseCmd(cmdStart, cmdLen, &isStopped, &shutdown);
          if (shutdown)
            break;
          cmdOffset = readOffset + i + 1; //  right after '\n'
        }
      }
      readOffset += bytes;
      if (readOffset >= BUFFER_THRESHOLD)
        tryCleanupBuffer(cmdbuf, &cmdOffset, &readOffset);
    }

    if (mraa_gpio_read(g_button))
      shutdown = 1;
  }

  printShutdown(outbuf, &clock);

  // shutdown
  mraa_aio_close(g_tempsensor);
  mraa_gpio_close(g_button);
}

// Main

int main(int argc, char *argv[]) {
  parseArguments(argc, argv);
  IO();
  cleanup();
  return 0;
}
