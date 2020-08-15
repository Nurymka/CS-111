#include <assert.h>
#include <stdlib.h>
#include "SortedList.h"


int main(int argc, char *argv[]) {
  // head
  SortedList_t head;
  head.prev = &head;
  head.next = &head;
  head.key = NULL;

  SortedListElement_t elem4;
  elem4.key = "4";

  SortedListElement_t elem6;
  elem6.key = "6";

  SortedListElement_t elem2;
  elem2.key = "2";

  SortedListElement_t elem1;
  elem1.key = "1";

  SortedListElement_t elem7;
  elem7.key = "7";

  assert(SortedList_length(&head) == 0);
  SortedList_insert(&head, &elem4);
  SortedList_insert(&head, &elem6);
  SortedList_insert(&head, &elem2);
  SortedList_insert(&head, &elem1);
  SortedList_insert(&head, &elem7);
  assert(SortedList_length(&head) == 5);
  assert(SortedList_lookup(&head, "1") == &elem1);
  assert(SortedList_lookup(&head, "6") == &elem6);
  SortedList_delete(&elem7);
  SortedList_delete(&elem1);
  assert(SortedList_lookup(&head, "1") == NULL);
  assert(SortedList_lookup(&head, "7") == NULL);


  return 0;
}
