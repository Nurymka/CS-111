// NAME: Nurym Kudaibergen
// EMAIL: nurim98@gmail.com
// ID: 404648263

#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "SortedList.h"

int opt_yield;

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
  SortedListElement_t *cur = list->next;

  while(cur != list && strcmp(element->key, cur->key) > 0) // stops at first node w/ key equal or larger
    cur = cur->next;

  if (opt_yield & INSERT_YIELD)
    sched_yield();
  element->next = cur;
  element->prev = cur->prev;
  cur->prev->next = element;
  cur->prev = element;
}

int SortedList_delete(SortedListElement_t *element) {
  if (element == NULL)
    return 1;

  SortedListElement_t *right = element->next;
  SortedListElement_t *left = element->prev;
  if (right->prev != element || left->next != element)
    return 1;

  if (opt_yield & DELETE_YIELD)
    sched_yield();
  right->prev = left;
  left->next = right;

  free(element);
  return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  SortedListElement_t *cur = list->next;
  if (cur == list)
    return NULL;

  while (cur != list) {
    if (strcmp(cur->key, key) == 0)
      return cur;
    if (opt_yield & LOOKUP_YIELD)
      sched_yield();
    cur = cur->next;
  }

  return NULL;
}

int SortedList_length(SortedList_t *list) {
  int count = 0;
  SortedListElement_t *cur = list->next;
  if (cur == list)
    return 0;

  while (cur != list) {
    count++;
    if (cur->next->prev != cur->prev->next)
      return -1;
    if (opt_yield & LOOKUP_YIELD)
      sched_yield();
    cur = cur->next;
  }

  return count;
}
