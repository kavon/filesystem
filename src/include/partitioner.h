#ifndef __PARTITION_H
#define __PARTITION_H

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

#include "bitmap.h"

typedef uint64_t block_id;
typedef uint64_t block_size_t;

/**
 * Creates the file represting our file system partition on
 * in the current OS's filesystem.
 *
 * If the file specified already exists, it is simply loaded in order
 * to allow for further interaction with the partition.
 *
 * If the file does not exist, one is created with at least the specified
 * number of bytes.
 *
 * Returns a non-zero value if an error occurs.
 *
 */
int initialize(char* filename, uint64_t numBytes);

/**
 * Retrieves the block_id representing the root directory in this partition.
 * If the returned value is equal to 0, then there does not exist a root directory
 * that was saved to the descriptor.
 */
block_id getRootID();

/**
 * Saves the block_id represting the root directory in our filesystem structure
 * to the partition descriptor.
 */
void saveRootID(block_id id);

/**
 * Prints info about the state of the partition (descriptor block and free block table stats)
 * to the specified file descriptor.
 */
void printInfo(FILE *dest);

/**
 * Resizes an already allocated block in this partition, potentially moving it,
 * so you should upate the pointer to the one that is returned.
 *
 * Passing NULL in for the ptr is equivalent to calling allocate_block.
 * 
 */
block_id resize_block(block_id blk, block_size_t size);

/**
 * Allocates a new block in the partition.
 */
block_id allocate_block(block_size_t size);

/**
 * Frees the given block.
 * Do not call free on an already freed block.
 */
void free_block(block_id blk);

/**
 * Copies at most numBytes bytes from this block into the specified pointer.
 * Use this to read the block, and you can modify and save it back.
 */
void load_block(block_id blk, void* destination, size_t numBytes);

/**
 * Overwrites the block's contents (starting at the beginning of the block) with
 * at most min(numBytes, block size) bytes, effectively saving it to the disk. 
 * 
 */
void save_block(block_id blk, void *source, size_t numBytes);

 
#endif /* __PARTITION_H */


