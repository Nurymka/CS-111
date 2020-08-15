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
static int g_numlists = 1;
static SortedList_t **g_multilist = NULL; // contains pointers to a head of each sublist
static SortedListElement_t **g_elems = NULL; // contains all items
static long long *g_telapsedtime = NULL; // contains lock acquisition time per thread
static pthread_mutex_t *g_mutexes = NULL; // contains mutexes for each sublist
static int *g_spin_locks = NULL; // contains spinlocks for each sublist

// Error
void exitWithError(const char *msg, int code) {
  fprintf(stderr, "%s\n", msg);
  exit(code);
}
void exitWithErrorReason(const char *msg, int code) {
  perror(msg);
  exit(code);
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

long long avg_waitforlock() {
  if (g_sync_mode == 0)
    return 0;

  long long avg = 0;
  int i = 0;
  for (; i < g_numthreads; i++)
    avg += g_telapsedtime[i];
  avg = avg / (g_numthreads * g_numiterations * 3); // number of times lock() was called

  return avg;
}

int hash_func(char const *key) {
  if (key == NULL)
    exitWithError("Empty key given to a hash function", 2);

  return (key[0] + key[1] + key[2]) % g_numlists;
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
    {"lists", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "t:i:y:l:", long_options, NULL)) != -1) {
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
      case 'l':
        g_numlists = atoi(optarg);
        break;
      default:
        exitWithError("Usage: lab2_add [--threads=<number>] [--iterations=<number>] [--lists=<number>] [--yield={i,d,l}]", 1);
    }
  }
}

void initMultilist() {
  g_multilist = malloc(g_numlists * sizeof(SortedList_t *));
  int head_i = 0;
  for (; head_i < g_numlists; head_i++) {
    SortedList_t **cur_pointer_to_head = g_multilist + head_i;
    *cur_pointer_to_head = malloc(sizeof(SortedList_t));
    SortedList_t *cur_head = *cur_pointer_to_head;
    cur_head->key = NULL;
    cur_head->next = cur_head;
    cur_head->prev = cur_head;
  }
}

void initLocks() {
  if (g_sync_mode & SYNC_M) {
    g_mutexes = malloc(g_numlists * sizeof(pthread_mutex_t));
    int i = 0;
    for (; i < g_numlists; i++) {
      if (pthread_mutex_init(g_mutexes+i, NULL) != 0)
        exitWithErrorReason("Failed to initalize locks for sublists", 1);
    }
  } else if (g_sync_mode & SYNC_S) {
    g_spin_locks = malloc(g_numlists * sizeof(int));
    int i = 0;
    for (; i < g_numlists; i++)
      g_spin_locks[i] = 0;
  }
}

void initAllElements() {
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
}

void initPerThreadTimeArray() {
  g_telapsedtime = malloc(sizeof(long long) * g_numthreads);
  int time_i = 0;
  for (; time_i < g_numthreads; time_i++)
    g_telapsedtime[time_i] = 0;
}

// Thread subroutines
void redirect_lock(int sublist_idx, int t_idx) {
  if (g_sync_mode == 0)
    return;

  struct timespec start_time;
  struct timespec end_time;

  if (g_sync_mode & SYNC_M) {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    pthread_mutex_lock(&g_mutexes[sublist_idx]);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
  } else if (g_sync_mode & SYNC_S) {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    while(__sync_lock_test_and_set(&g_spin_locks[sublist_idx], 1));
    clock_gettime(CLOCK_MONOTONIC, &end_time);
  }

  // add elapsed time to total counter
  long long ns_elapsedtime = (end_time.tv_sec - start_time.tv_sec) * 1000000000;
  ns_elapsedtime += (end_time.tv_nsec - start_time.tv_nsec);
  g_telapsedtime[t_idx] += ns_elapsedtime;
}

void redirect_unlock(int sublist_idx) {
  if (g_sync_mode == 0)
    return;
  else if (g_sync_mode & SYNC_M)
    pthread_mutex_unlock(&g_mutexes[sublist_idx]);
  else if (g_sync_mode & SYNC_S)
    __sync_lock_release(&g_spin_locks[sublist_idx]);
}

void list_insert(int sublist_idx, SortedListElement_t* elem, int t_idx) {
  redirect_lock(sublist_idx, t_idx);
  SortedList_insert(g_multilist[sublist_idx], elem);
  redirect_unlock(sublist_idx);
}

int list_length(int t_idx) {
  int ret = 0;
  int i = 0;
  for (; i < g_numlists; i++) {
    redirect_lock(i, t_idx);
    ret += SortedList_length(g_multilist[i]);
    redirect_unlock(i);
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

void list_lookup_and_delete(int sublist_idx, const char* key, int t_idx) {
  redirect_lock(sublist_idx, t_idx);
  __list_lookup_and_delete(g_multilist[sublist_idx], key);
  redirect_unlock(sublist_idx);
}

void *thread_list(void *args) {
  int thread_idx = (long)args;
  SortedListElement_t **elems = g_elems + (thread_idx * g_numiterations);

  int i = 0;
  for (; i < g_numiterations; i++) {
    SortedListElement_t *elem = elems[i];
    int sublist_idx = hash_func(elem->key);
    list_insert(sublist_idx, elem, thread_idx);
  }

  if (list_length(thread_idx) == -1)
    exitWithError("Synchronization error: the list is corrupted (SortedList_length)", 2);

  int j = 0;
  for (; j < g_numiterations; j++) {
    SortedListElement_t *elem = elems[j];
    const char *elem_key = elem->key;
    int sublist_idx = hash_func(elem_key);
    list_lookup_and_delete(sublist_idx, elem_key, thread_idx);
  }

  return NULL;
}

// Signal handlers

void on_segfault() {
  exitWithError("Segmentation fault: the corrupted list lead to an invalid pointer access", 2);
}

int main(int argc, char *argv[]) {
  parseArguments(argc, argv);

  initMultilist();
  initLocks();
  initAllElements();
  initPerThreadTimeArray();

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

  int list_len = 0;
  int list_i = 0;
  // not calling the list_length() function, since it'll try to acquire locks and add extra wait time for per thread
  for (; list_i < g_numlists; list_i++)
    list_len += SortedList_length(g_multilist[list_i]);
  if (list_len != 0)
    exitWithError("Synchronization error: the list length is not zero at the end", 2);

  // calculate outputs
  long long ns_elapsedtime = (end_time.tv_sec - start_time.tv_sec) * 1000000000;
  ns_elapsedtime += (end_time.tv_nsec - start_time.tv_nsec);

  long long total_ops = g_numthreads * g_numiterations * 3;
  long long total_time = ns_elapsedtime;
  long long avg_per_op = (total_time / total_ops);
  long long avg_waitforlock_time = avg_waitforlock();

  char *yieldOpts = formattedYieldOpts();
  char *syncOpts = formattedSyncOpts();
  printf("list-%s-%s,%d,%d,%d,%lld,%lld,%lld,%lld\n", yieldOpts, syncOpts, g_numthreads, g_numiterations, g_numlists, total_ops, total_time, avg_per_op, avg_waitforlock_time);

  return 0;
}
