#!/usr/bin/python

import sys, math
from optparse import OptionParser

# import json
# print(json.dumps(allocated_blocks, indent=4))

def block_consistency_audit(lines):
    # parse superblock
    superblock_info = []
    for line in lines:
        if line.find("SUPERBLOCK") != -1:
            superblock_info = line.split(',')
            break
    tot_num_blocks = int(superblock_info[1])
    tot_num_inodes = int(superblock_info[2])
    block_size = int(superblock_info[3])
    inode_size = int(superblock_info[4])

    # helper functions
    def last_non_data_block_id(): # block number of the last block from inode table
        return 4 + int(math.ceil(tot_num_inodes / (block_size / inode_size)))

    def is_block_invalid(block_id):
        return not (block_id >= 0 and block_id <= (tot_num_blocks - 1))
    def is_block_reserved(block_id):
        return block_id >= 0 and block_id <= last_non_data_block_id()

    # print functions
    def str_block(block_id, inode_num, offset, level):
        level_str = ""
        if level == 1:
            level_str = "INDIRECT "
        elif level == 2:
            level_str = "DOUBLE INDIRECT "
        elif level == 3:
            level_str = "TRIPLE INDIRECT "

        return level_str + "BLOCK " + str(block_id) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset)
    def str_invalid_block(block_id, inode_num, offset, level):
        return "INVALID " + str_block(block_id, inode_num, offset, level) + "\n"
    def str_reserved_block(block_id, inode_num, offset, level):
        return "RESERVED " + str_block(block_id, inode_num, offset, level) + "\n"
    def str_unref_block(block_id):
        return "UNREFERENCED BLOCK " + str(block_id) + "\n"
    def str_allocated_and_free_block(block_id):
        return "ALLOCATED BLOCK " + str(block_id) + " ON FREELIST\n"
    def str_dup_block(block_id, inode_num, offset, level):
        return "DUPLICATE " + str_block(block_id, inode_num, offset, level) + "\n"

    ret_str = ""

    # create a free list
    free_list = set()
    for line in lines:
        if line.find("BFREE,") != -1:
            block_id = int(line.split(',')[1])
            free_list.add(block_id)

    # create allocated blocks dict
    allocated_blocks = {}


    def check_block_info_and_insert(block_id, inode_num, offset, level):
        ret = ""
        if is_block_invalid(block_id): # INVALID blocks
            ret += str_invalid_block(block_id, inode_num, offset, level)
        if is_block_reserved(block_id): # RESERVED blocks
            ret += str_reserved_block(block_id, inode_num, offset, level)
        if not is_block_invalid(block_id) and not is_block_reserved(block_id):
            if block_id in allocated_blocks:
                allocated_blocks[block_id].append((inode_num, offset, level))
            else:
                allocated_blocks[block_id] = [(inode_num, offset, level)]
        return ret

    # allocated inodes from direct pointers
    for line in lines:
        if line.find("INODE") != -1: # blocks from direct pointers
            inode_info = line.split(',')
            num_blocks_in_inode = int(inode_info[11])
            for i in range(12, 27): # 13 data pointers
                block_id = int(inode_info[i])
                if block_id == 0:
                    continue
                inode_num = int(inode_info[1])
                offset = 0
                level = 0
                if i == 24: # indirect pointer
                    offset = 12
                    level = 1
                elif i == 25: # double indirect pointer
                    offset = 268
                    level = 2
                elif i == 26: # triple indirect pointer
                    offset = 65804
                    level = 3
                ret_str += check_block_info_and_insert(block_id, inode_num, offset, level)
        if line.find("INDIRECT") != -1: # blocks from indirect pointers
            indir_info = line.split(',')
            block_id = int(indir_info[5])
            inode_num = int(indir_info[1])
            level = int(indir_info[2])
            offset = int(indir_info[3])
            ret_str += check_block_info_and_insert(block_id, inode_num, offset, level)

    # UNREFERENCED, ALLOCATED & IN FREELIST, DUPLICATE checks
    first_data_block_id = last_non_data_block_id() + 1
    for block_id in range(first_data_block_id, tot_num_blocks):
        if block_id not in allocated_blocks and block_id not in free_list: # UNREFERENCED block
            ret_str += str_unref_block(block_id)
        if block_id in allocated_blocks and block_id in free_list: # ALLOCATED & IN FREELIST
            ret_str += str_allocated_and_free_block(block_id)
        if block_id in allocated_blocks and len(allocated_blocks[block_id]) > 1:
            for block_info in allocated_blocks[block_id]:
                inode_num, offset, level = block_info
                ret_str += str_dup_block(block_id, inode_num, offset, level) # DUPLICATE

    return ret_str

def inode_consistency_audit(lines):
    ret_str = ""
    free_inodes = []
    allocated_inodes = []
    num_inodes = 0
    first_non_reserved_inode_num = 2

    # parsing
    for line in lines:
        if line.find("SUPERBLOCK") != -1:
            num_inodes = int(line.split(',')[2])
            first_non_reserved_inode_num = int(line.split(',')[7])
        if line.find("IFREE") != -1:
            free_inodes.append(int(line.split(',')[1]))
        if line.find("INODE") != -1:
            allocated_inodes.append(int(line.split(',')[1]))

    for inode_num in range(2, num_inodes + 1): # first two inodes are reserved
        if inode_num != 2 and inode_num < first_non_reserved_inode_num: # node 2 as special case
            continue
        if inode_num in allocated_inodes and inode_num in free_inodes:
            ret_str += "ALLOCATED INODE " + str(inode_num) + " ON FREELIST\n"
        if inode_num not in allocated_inodes and inode_num not in free_inodes:
            ret_str += "UNALLOCATED INODE " + str(inode_num) + " NOT ON FREELIST\n"

    return ret_str

def dir_consistency_audit(lines):
    ret_str = ""
    allocated_inodes = {} # { inode_num : link_count }
    dir_entries = {} # { ref_inode_num : [(entry_name, dir_inode_num)] }
    tot_num_inodes = 0

    # parse
    for line in lines:
        if line.find("SUPERBLOCK") != -1:
            superblock_info = line.split(',')
            tot_num_inodes = int(superblock_info[2])
        if line.find("INODE") != -1:
            inode_info = line.split(',')
            inode_num = int(inode_info[1])
            link_count = int(inode_info[6])
            allocated_inodes[inode_num] = link_count
        if line.find("DIRENT") != -1:
            dirent_info = line.split(',')
            ref_inode_num = int(dirent_info[3])
            entry_name = dirent_info[6].rstrip()
            dir_inode_num = int(dirent_info[1])
            if ref_inode_num in dir_entries:
                dir_entries[ref_inode_num].append((entry_name, dir_inode_num))
            else:
                dir_entries[ref_inode_num] = [(entry_name, dir_inode_num)]

    # check for real link count
    for inode_num in allocated_inodes:
        actual_link_count = len(dir_entries[inode_num]) if inode_num in dir_entries else 0
        reported_link_count = allocated_inodes[inode_num]
        if actual_link_count != reported_link_count: # LINKCOUNt inconsistency
            ret_str += "INODE " + str(inode_num) + " HAS " + str(actual_link_count) + " LINKS BUT LINKCOUNT IS " + str(reported_link_count) + "\n"


    def is_inode_invalid(inode_num):
        return inode_num < 1 or inode_num > tot_num_inodes
    def is_inode_unallocated(inode_num):
        return inode_num not in allocated_inodes

    # check for invalid, unallocated referenced inodes and '.', '..' pointers
    for ref_inode_num in dir_entries:
        for (entry_name, dir_inode_num) in dir_entries[ref_inode_num]:
            if is_inode_invalid(ref_inode_num): # INVALID INODE ref check
                ret_str += "DIRECTORY INODE " + str(dir_inode_num) + " NAME " + entry_name + " INVALID INODE " + str(ref_inode_num) + "\n"
            if not is_inode_invalid(ref_inode_num) and is_inode_unallocated(ref_inode_num): # UNALLOCATED INODE ref check
                ret_str += "DIRECTORY INODE " + str(dir_inode_num) + " NAME " + entry_name + " UNALLOCATED INODE " + str(ref_inode_num) + "\n"
                
            if entry_name == "'.'" and dir_inode_num != ref_inode_num: # '.' entry check
                ret_str += "DIRECTORY INODE " + str(dir_inode_num) + " NAME '.' LINK TO INODE " + ref_inode_num + " SHOULD BE " + dir_inode_num + "\n"

            if entry_name == "'..'" and dir_inode_num == 2 and dir_inode_num != ref_inode_num: # special case: in root (inode num 2), entry '..' should point to itself
                ret_str += "DIRECTORY INODE 2 NAME '..' LINK TO INODE " + str(ref_inode_num) + " SHOULD BE 2" + "\n"
            else: # usual case: '..' should point to parent of the dir containing this inode
                _, parent_inode_num = dir_entries[dir_inode_num][0]
                if entry_name == "'..'" and parent_inode_num != ref_inode_num:
                    ret_str += "DIRECTORY INODE " + str(dir_inode_num) + " NAME " + entry_name + " LINK TO INODE " + str(ref_inode_num) + " SHOULD BE " + str(parent_inode_num) + "\n"

    return ret_str


def audit_report(filename):
    # lines
    fd = open(filename)
    file_lines = fd.readlines()
    fd.close()

    ret = ""
    ret += block_consistency_audit(file_lines)
    ret += inode_consistency_audit(file_lines)
    ret += dir_consistency_audit(file_lines)

    return ret

def main():
    version_msg = "%prog 1.0"
    usage_msg = """%prog <filename.csv>
-- Analyzes file system image consistency from a report in <filename.csv>
"""
    parser = OptionParser(version=version_msg,
                          usage=usage_msg)
    opts, args = parser.parse_args(sys.argv[1:])

    if len(args) != 1:
        sys.stderr.write("Invalid number of arguments")
        exit(1)

    try:
        audit_output = audit_report(args[0])
        sys.stdout.write(audit_output)
        if audit_output: # there are inconsistencies
            exit(2)
        else:
            exit(0)
    except IOError as err:
        sys.stderr.write("I/O error: {0}\n".format(err.strerror))
        exit(1)

if __name__ == "__main__":
    main()
