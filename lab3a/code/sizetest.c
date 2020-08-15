#include <stdio.h>
#include "ext2_fs.h"

int main(int argc, char* argv[]) {
  struct ext2_dir_entry dirEntry;
  printf("dirEntry size: %d\n", sizeof(__uint32_t));
  return 0;
}
