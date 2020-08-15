// NAME: Nurym Kudaibergen
// EMAIL: nurim98@gmail.com
// ID: 404648263

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "SortedList.h"

#define SYNC_M 1
#define SYNC_S 2

const int RANDKEY_MAX_LEN = 10;

static int g_numthreads = 1;
static int g_numiterations = 1;
static int g_yield_mode = 0;
static int g_sync_mode = 0;
static SortedList_t *g_head = NULL;
static SortedListElement_t **g_elems = NULL;
static pthread_mutex_t g_mutex;
static int g_spin_lock;

// Error
void exitWithError(const char *msg, int code) {
  fprintf(stderr, "%s\n", msg);
  exit(code);
}
void exitWithErrorReason(const char *msg, int code) {
  perror(msg);
  exit(code);
}

// Setup
void parseSyncModes(char *syncStr) {
  if (syncStr[0] == 'm')
    g_sync_mode |= SYNC_M;
  else if (syncStr[0] == 's')
    g_sync_mode |= SYNC_S;
}

void parseYieldModes(char *yieldStr) {
  unsigned int i = 0;
  size_t len = strlen(yieldStr);
  for (; i < len; i++) {
    if (yieldStr[i] == 'i')
      g_yield_mode |= INSERT_YIELD;
    else if (yieldStr[i] == 'd')
      g_yield_mode |= DELETE_YIELD;
    else if (yieldStr[i] == 'l')
      g_yield_mode |= LOOKUP_YIELD;
  }
}

void parseArguments(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"threads", required_argument, 0, 't'},
    {"iterations", required_argument, 0, 'i'},
    {"yield", required_argument, 0, 'y'},
    {"sync", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "t:i:y:", long_options, NULL)) != -1) {
    switch(opt) {
      case 'y':
        parseYieldModes(optarg);
        break;
      case 's':
        parseSyncModes(optarg);
        break;
      case 't':
        g_numthreads = atoi(optarg);
        break;
      case 'i':
        g_numiterations = atoi(optarg);
        break;
      default:
        exitWithError("Usage: lab2_add [--threads=<number>] [--iterations=<number>] [--yield={i,d,l}]", 1);
    }
  }
}

// Misc
char *formattedYieldOpts() {
  char *str = malloc(5 * sizeof(char)); // idl and a null

  if (g_yield_mode == 0) {
    str = "none";
    return str;
  }

  int cur_idx = 0;
  if (g_yield_mode & INSERT_YIELD) {
    str[cur_idx] = 'i';
    cur_idx++;
  }
  if (g_yield_mode & DELETE_YIELD) {
    str[cur_idx] = 'd';
    cur_idx++;
  }
  if (g_yield_mode & LOOKUP_YIELD) {
    str[cur_idx] = 'l';
    cur_idx++;
  }
  str[cur_idx] = '\0';
  return str;
}

char *formattedSyncOpts() {
  char *str = malloc(5 * sizeof(char)); // idl and a null
  if (g_sync_mode == 0)
    str = "none";
  else if (g_sync_mode & SYNC_M)
    str = "m";
  else if (g_sync_mode & SYNC_S)
    str = "s";
  return str;
}

char *randStr(int maxlen) {
  int len = (rand() % maxlen) + 2; // another +1 for null char
  char *str = malloc(len * sizeof(char));
  int i = 0;
  for (; i < len - 1; i++)
    str[i] = (rand() % 95) + ' '; // printable ascii chars are from 32 (' ') to 127 (exclusive)
  str[len] = '\0';
  return str;
}

// Thread subroutines
void list_insert(SortedList_t* head, SortedListElement_t* elem) {
  if (g_sync_mode == 0) {
    SortedList_insert(head, elem);
  } else if (g_sync_mode & SYNC_M) {
    pthread_mutex_lock(&g_mutex);
    SortedList_insert(head, elem);
    pthread_mutex_unlock(&g_mutex);
  } else if (g_sync_mode & SYNC_S) {
    while(__sync_lock_test_and_set(&g_spin_lock, 1));
    SortedList_insert(head, elem);
    __sync_lock_release(&g_spin_lock);
  }
}

int list_length(SortedList_t* head) {
  int ret = 0;
  if (g_sync_mode == 0) {
    ret = SortedList_length(head);
  } else if (g_sync_mode & SYNC_M) {
    pthread_mutex_lock(&g_mutex);
    ret = SortedList_length(head);
    pthread_mutex_unlock(&g_mutex);
  } else if (g_sync_mode & SYNC_S) {
    while(__sync_lock_test_and_set(&g_spin_lock, 1));
    ret = SortedList_length(head);
    __sync_lock_release(&g_spin_lock);
  }
  return ret;
}

void __list_lookup_and_delete(SortedList_t *head, const char* key) {
  SortedListElement_t *foundElem = SortedList_lookup(head, key);
  if (foundElem == NULL)
    exitWithError("Synchronization error: can't find an element with key that was inserted before", 2);
  if (SortedList_delete(foundElem) == 1)
    exitWithError("Synchronization error: the list is corrupted (SortedList_delete)", 2);
}

void list_lookup_and_delete(SortedList_t *head, const char* key) {
  if (g_sync_mode == 0) {
    __list_lookup_and_delete(head, key);
  } else if (g_sync_mode & SYNC_M) {
    pthread_mutex_lock(&g_mutex);
    __list_lookup_and_delete(head, key);
    pthread_mutex_unlock(&g_mutex);
  } else if (g_sync_mode & SYNC_S) {
    while(__sync_lock_test_and_set(&g_spin_lock, 1));
    __list_lookup_and_delete(head, key);
    __sync_lock_release(&g_spin_lock);
  }
}

void *thread_list(void *args) {
  long int thread_idx = (long int)args;
  SortedListElement_t **elems = g_elems + (thread_idx * g_numiterations);

  int i = 0;
  for (; i < g_numiterations; i++) {
    SortedListElement_t *elem = elems[i];
    list_insert(g_head, elem);
  }

  if (list_length(g_head) == -1)
    exitWithError("Synchronization error: the list is corrupted (SortedList_length)", 2);

  int j = 0;
  for (; j < g_numiterations; j++) {
    SortedListElement_t *elem = elems[j];
    const char *elem_key = elem->key;
    list_lookup_and_delete(g_head, elem_key);
  }

  return NULL;
}

// Signal handlers

void on_segfault() {
  exitWithError("Segmentation fault: the corrupted list lead to an invalid pointer access", 2);
}

int main(int argc, char *argv[]) {
  parseArguments(argc, argv);

  // init list
  g_head = malloc(sizeof(SortedList_t));
  g_head->key = NULL;
  g_head->next = g_head;
  g_head->prev = g_head;

  // init all items
  g_elems = malloc(g_numthreads * g_numiterations * sizeof(SortedListElement_t *));

  int elem_i = 0;
  for (; elem_i < g_numthreads; elem_i++) {
    int elem_j = 0;
    for (; elem_j < g_numiterations; elem_j++) {
      SortedListElement_t *elem = malloc(sizeof(SortedListElement_t));
      elem->key = randStr(RANDKEY_MAX_LEN);
      g_elems[elem_i * g_numiterations + elem_j] = elem;
    }
  }

  // register signal handler
  signal(SIGSEGV, on_segfault);

  // get current timestamp
  struct timespec start_time;
  if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1)
    exitWithErrorReason("Failed to obtain timestamp at start", 1);

  // create specified number of threads
  pthread_t threads[g_numthreads];
  long int i = 0;
  for (; i < g_numthreads; i++) {
    if (pthread_create(&threads[i], NULL, (void *)thread_list, (void *)i) != 0)
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

  int list_len = SortedList_length(g_head);
  if (list_len != 0)
    exitWithError("Synchronization error: the list length is not zero at the end", 2);

  // calculate outputs
  long long ns_elapsedtime = (end_time.tv_sec - start_time.tv_sec) * 1000000000;
  ns_elapsedtime += (end_time.tv_nsec - start_time.tv_nsec);

  int numLists = 1;
  long long total_ops = g_numthreads * g_numiterations * 3;
  long long total_time = ns_elapsedtime;
  long long avg_per_op = (total_time / total_ops);
  char *yieldOpts = formattedYieldOpts();
  char *syncOpts = formattedSyncOpts();
  printf("list-%s-%s,%d,%d,%d,%lld,%lld,%lld\n", yieldOpts, syncOpts, g_numthreads, g_numiterations, numLists, total_ops, total_time, avg_per_op);

  return 0;
}
