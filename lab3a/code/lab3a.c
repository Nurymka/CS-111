// NAME: Nurym Kudaibergen
// EMAIL: nurim98@gmail.com
// ID: 404648263

#include <stdio.h>
#include <stdlib.h>
// open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// pread()
#include <unistd.h>
// gmtime_r()
#include <time.h>
// memcpy()
#include <string.h>
// pow()
#include <math.h>

#include "ext2_fs.h"

// Error
void exitWithError(const char *msg, int code) {
  fprintf(stderr, "%s\n", msg);
  exit(code);
}
void exitWithErrorReason(const char *msg, int code) {
  perror(msg);
  exit(code);
}

// Subroutines

int g_blockSize = 0;
int g_fsfd = -1;

void parseDir(int blockId, int parentNodeNum, int blkIdx) {
  if (blockId == 0) // block 0 is reserved
    return;

  int dirBlockStart = g_blockSize * blockId;
  int dirBlockEnd = g_blockSize * (blockId + 1);
  int curOffset = 0;
  int logicalOffset = blkIdx * g_blockSize;

  while (curOffset < (dirBlockEnd - dirBlockStart)) {
    struct ext2_dir_entry dirEntry;
    if (pread(g_fsfd, &dirEntry, sizeof(dirEntry), dirBlockStart + curOffset) == -1)
      exitWithErrorReason("Failed to read data block", 2);

    if (dirEntry.inode == 0) {
      curOffset += dirEntry.rec_len;
      continue;
    }

    char entryName[dirEntry.name_len + 1];
    entryName[dirEntry.name_len] = '\0';
    memcpy(entryName, dirEntry.name, dirEntry.name_len);
    printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n", parentNodeNum, logicalOffset + curOffset, dirEntry.inode, dirEntry.rec_len, dirEntry.name_len, entryName);
    curOffset += dirEntry.rec_len;
  }
}

// level = {1 | 2 | 3}
void parseIndDir(int blockId, int level, int parentNodeNum, int blkIdx) {
  int indirRefStart = blockId * g_blockSize;
  int indirRefEnd = (blockId + 1) * g_blockSize;
  int curOffset = 0;
  char blockBuf[g_blockSize];
  if (pread(g_fsfd, blockBuf, g_blockSize, indirRefStart) == -1)
    exitWithErrorReason("Failed to read indirect refs block", 2);

  for (; curOffset < (indirRefEnd - indirRefStart); curOffset += sizeof(__u32)) { // block IDs are of size __u32
    int refdBlockId;
    memcpy(&refdBlockId, blockBuf + curOffset, sizeof(__u32));
    int idx = curOffset / sizeof(__u32);
    if (level == 1)
      parseDir(refdBlockId, parentNodeNum, blkIdx + idx);
    else
      parseIndDir(refdBlockId, level - 1, parentNodeNum, blkIdx);
  }
}

void parseIndRefs(int blockId, int level, int parentNodeNum, int logicalOffset) {
  int indirRefStart = blockId * g_blockSize;
  int indirRefEnd = (blockId + 1) * g_blockSize;
  int curOffset = 0;

  char blockBuf[g_blockSize];
  if (pread(g_fsfd, blockBuf, g_blockSize, indirRefStart) == -1)
    exitWithErrorReason("Failed to read indirect refs block", 2);

  int numBlockIdsInBlock = g_blockSize / sizeof(__u32);
  for (; curOffset < (indirRefEnd - indirRefStart); curOffset += sizeof(__u32)) { // block IDs are of size __u32
    int refdBlockId;
    memcpy(&refdBlockId, blockBuf + curOffset, sizeof(__u32));
    if (refdBlockId == 0)
      continue;

    int idx = curOffset / sizeof(__u32);
    int curLogicalOffset = logicalOffset + (idx * (int)pow(numBlockIdsInBlock, level - 1));

    printf("INDIRECT,%d,%d,%d,%d,%d\n",parentNodeNum,level,curLogicalOffset,blockId,refdBlockId);
    if (level > 1)
      parseIndRefs(refdBlockId, level - 1, parentNodeNum, curLogicalOffset);
  }
}

// Helper

char getFiletype(int mode) {
  if (S_ISLNK(mode)) // symbolic link
    return 's';
  else if (S_ISREG(mode)) // regular file
    return 'f';
  else if (S_ISDIR(mode)) // directory
    return 'd';
  else
    return '?';
}

char *getFormattedTime(time_t timestamp) {
  char *fmtTime = malloc(32);
  struct tm tm;
  gmtime_r(&timestamp, &tm);
  strftime(fmtTime, 32, "%m/%d/%y %H:%M:%S", &tm);
  return fmtTime;
}

// Main

int main(int argc, char *argv[]) {
  if (argc != 2) {
    exitWithError("Usage: ./lab3a <fsimage.img>", 1);
  }

  char *fspath = argv[1];
  if ((g_fsfd = open(fspath, O_RDONLY)) == -1)
    exitWithErrorReason("Failed to open the specified file system image", 1);

  // superblock summary
  struct ext2_super_block superblock;

  unsigned int superblock_offset = 1024;
  if (pread(g_fsfd, &superblock, sizeof(superblock), superblock_offset) == -1) // superblock starts at byte 1024
    exitWithErrorReason("Failed to read superblock", 2);
  int totalNumBlocks = superblock.s_blocks_count;
  int totalNumInodes = superblock.s_inodes_count;
  g_blockSize = 1024 << superblock.s_log_block_size; // http://www.nongnu.org/ext2-doc/ext2.html#S-LOG-BLOCK-SIZE
  int inodeSize = superblock.s_inode_size;
  int numBlocksPerGroup = superblock.s_blocks_per_group;
  int numInodesPerGroup = superblock.s_inodes_per_group;
  int firstInode = superblock.s_first_ino;
  printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", totalNumBlocks, totalNumInodes, g_blockSize, inodeSize, numBlocksPerGroup, numInodesPerGroup, firstInode);

  // group summary
  struct ext2_group_desc groupdesc;

  unsigned int groupdesc_offset = superblock_offset + sizeof(superblock); // block right after superblock = superblock offset + size of superblock
  if (pread(g_fsfd, &groupdesc, sizeof(groupdesc), groupdesc_offset) == -1)
    exitWithErrorReason("Failed to read the block group description", 2);
  int groupNum = 0;
  int numFreeBlocks = groupdesc.bg_free_blocks_count;
  int numFreeInodes = groupdesc.bg_free_inodes_count;
  int blockBitmap = groupdesc.bg_block_bitmap;
  int inodeBitmap = groupdesc.bg_inode_bitmap;
  int inodeTable = groupdesc.bg_inode_table;
  printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n", groupNum, totalNumBlocks, totalNumInodes, numFreeBlocks, numFreeInodes, blockBitmap, inodeBitmap, inodeTable);

  // free block entries & free inode entries
  unsigned int blockBitmap_offset = groupdesc_offset + g_blockSize; // next block after groupdesc_offset
  char blkBitmapBuf[g_blockSize];
  if (pread(g_fsfd, blkBitmapBuf, g_blockSize, blockBitmap_offset) == -1)
    exitWithErrorReason("Failed to read the block bitmap", 2);

  unsigned int inodeBitmap_offset = blockBitmap_offset + g_blockSize;
  char inodeBitmapBuf[g_blockSize];
  if (pread(g_fsfd, inodeBitmapBuf, g_blockSize, inodeBitmap_offset) == -1)
    exitWithErrorReason("Failed to read the inode bitmap", 2);


  int bb_i = 0;
  for (; bb_i < g_blockSize; bb_i++) {
    int bb_j = 0;
    for (; bb_j < 8; bb_j++) {
      if (!(blkBitmapBuf[bb_i] & (1 << bb_j))) {
        int blockNum = bb_i * 8 + bb_j + 1;
        printf("BFREE,%d\n", blockNum);
      }
    }
  }

  int allocInodeNums[numInodesPerGroup];
  int allocInodeCount = 0;

  int ib_i = 0;
  for (; ib_i < g_blockSize; ib_i++) {
    int ib_j = 0;
    for (; ib_j < 8; ib_j++) {
      int inodeNum = ib_i * 8 + ib_j + 1;
      if (inodeNum > numInodesPerGroup) // don't interpret more bits than inodes in the group
        break;

      if (!(inodeBitmapBuf[ib_i] & (1 << ib_j))) {
        printf("IFREE,%d\n", inodeNum);
      } else { // inode is allocated
        allocInodeNums[allocInodeCount] = inodeNum;
        allocInodeCount++;
      }
    }
  }

  // inode summary
  int inodeTable_offset = inodeBitmap_offset + g_blockSize;
  int nt_i = 0;
  for (; nt_i < allocInodeCount; nt_i++) {
    int inodeNum = allocInodeNums[nt_i];
    struct ext2_inode curInode;
    int curInode_offset = inodeTable_offset + (inodeNum - 1) * sizeof(curInode);
    if (pread(g_fsfd, &curInode, sizeof(curInode), curInode_offset) == -1)
      exitWithErrorReason("Failed to read an inode", 2);

    int linkCount = curInode.i_links_count;
    int mode = curInode.i_mode & 0xFFF;
    if (linkCount == 0 && mode == 0)
      continue;

    char filetype = getFiletype(curInode.i_mode);
    int owner = curInode.i_uid;
    int group = curInode.i_gid;
    char *timeCreate = getFormattedTime(curInode.i_ctime);
    char *timeModify = getFormattedTime(curInode.i_mtime);
    char *timeAccess = getFormattedTime(curInode.i_atime);
    int filesize = curInode.i_size;
    int inode_numBlocks = curInode.i_blocks;
    printf("INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", inodeNum, filetype, mode, owner, group, linkCount, timeCreate, timeModify, timeAccess, filesize, inode_numBlocks);

    int blkAddrIdx = 0;
    for (; blkAddrIdx < 15; blkAddrIdx++)
      printf(",%d", curInode.i_block[blkAddrIdx]);
    printf("\n");

    // directory summary
    if (filetype == 'd') {
      int i = 0;
      for (; i < 12; i++)// 12 - num of direct pointers
        parseDir(curInode.i_block[i], inodeNum, i);
      parseIndDir(curInode.i_block[12], 1, inodeNum, 12);
      parseIndDir(curInode.i_block[13], 2, inodeNum, 268);
      parseIndDir(curInode.i_block[14], 3, inodeNum, 65804);
    }

    if (filetype == 'f' || filetype == 'd') {
      parseIndRefs(curInode.i_block[12], 1, inodeNum, 12);
      parseIndRefs(curInode.i_block[13], 2, inodeNum, 268);
      parseIndRefs(curInode.i_block[14], 3, inodeNum, 65804);
    }
  }

  return 0;
}
