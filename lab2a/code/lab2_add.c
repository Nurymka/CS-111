// NAME: Nurym Kudaibergen
// EMAIL: nurim98@gmail.com
// ID: 404648263

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

// tag fields
#define YIELD 4
#define ADD_NONE 0
#define ADD_M 1
#define ADD_S 2
#define ADD_C 3

char* const TAG_FIELDS[] = { "add-none", "add-m", "add-s", "add-c", "add-yield-none", "add-yield-m", "add-yield-s", "add-yield-c" };

// global vars
static int g_numthreads = 1;
static int g_numiterations = 1;
static long long g_counter = 0;
static int g_opt_yield = 0;
static int g_add_mode = 0;
static pthread_mutex_t g_mutex;
static int g_spin_lock;

void exitWithError(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}
void exitWithErrorReason(const char *msg, int code) {
  perror(msg);
  exit(code);
}

void parseArguments(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"threads", required_argument, 0, 't'},
    {"iterations", required_argument, 0, 'i'},
    {"sync", required_argument, 0, 's'},
    {"yield", no_argument, &g_opt_yield, 1},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "t:i:s:", long_options, NULL)) != -1) {
    switch(opt) {
      case 0:
        break;
      case 's': {
        if (optarg[0] == 'm')
          g_add_mode |= ADD_M;
        else if (optarg[0] == 's')
          g_add_mode |= ADD_S;
        else if (optarg[0] == 'c')
          g_add_mode |= ADD_C;
        break;
      } case 't':
        g_numthreads = atoi(optarg);
        break;
      case 'i':
        g_numiterations = atoi(optarg);
        break;
      default:
        exitWithError("Usage: lab2_add [--threads=<number>] [--iterations=<number>]");
    }
  }
}

void cas_add(long long *pointer, long long value) {
  long long prev;
  long long sum;
  do {
    prev = *pointer;
    sum = prev + value;
    if (g_opt_yield)
      sched_yield();
  } while (__sync_val_compare_and_swap(pointer, prev, sum) != prev);
}

void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (g_opt_yield)
    sched_yield();
  *pointer = sum;
}

void mode_add(long long *pointer, long long value) {
  switch(g_add_mode) {
    case ADD_NONE: {
      add(pointer, value);
      break;
    }
    case ADD_M: {
      pthread_mutex_lock(&g_mutex);
      add(pointer, value);
      pthread_mutex_unlock(&g_mutex);
      break;
    }
    case ADD_S: {
      while(__sync_lock_test_and_set(&g_spin_lock, 1));
      add(pointer, value);
      __sync_lock_release(&g_spin_lock);
      break;
    }
    case ADD_C: {
      cas_add(pointer, value);
      break;
    }
    default:
      exitWithError("Invalid add mode, must be 0 <= g_add_mode <= 3");
  }
}

void thread_add() {
  // add +1
  int i = 0;
  for (; i < g_numiterations; i++) {
    mode_add(&g_counter, 1);
  }

  // add -1
  int j = 0;
  for (; j < g_numiterations; j++) {
    mode_add(&g_counter, -1);
  }
}

int main(int argc, char *argv[]) {
  // setup
  parseArguments(argc, argv);
  pthread_t threads[g_numthreads];

  // get current timestamp
  struct timespec start_time;
  if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1)
    exitWithErrorReason("Failed to obtain timestamp at start", 1);

  // create specified number of threads
  int i = 0;
  for (; i < g_numthreads; i++) {
    if (pthread_create(&threads[i], NULL, (void *)thread_add, NULL) != 0)
      exitWithErrorReason("Failed to create a thread", 2);
  }

  // join specified number of threads
  int j = 0;
  for (; j < g_numthreads; j++) {
    if (pthread_join(threads[j], NULL) != 0)
      exitWithErrorReason("Failed to wait for a thread", 2);
  }

  // get timestamp at end
  struct timespec end_time;
  if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1)
    exitWithErrorReason("Failed to obtain timestamp at end", 1);

  // calculate outputs
  long long ns_elapsedtime = (end_time.tv_sec - start_time.tv_sec) * 1000000000;
  ns_elapsedtime += (end_time.tv_nsec - start_time.tv_nsec);

  int test_idx = (g_opt_yield << 2) | g_add_mode;
  char *test_name = TAG_FIELDS[test_idx];
  long long total_ops = g_numthreads * g_numiterations * 2;
  long long total_time = ns_elapsedtime;
  long long avg_per_op = total_time / total_ops;

  printf("%s,%d,%d,%lld,%lld,%lld,%lld\n", test_name, g_numthreads, g_numiterations, total_ops, total_time, avg_per_op, g_counter);

  return 0;
}
