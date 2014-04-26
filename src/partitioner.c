#include "partitioner.h"

// file pointer to beginning of this partition
static FILE *part;

// in-memory version of the partition directory
static void* partDir;


// little endian machines flip these Magic values.
const uint64_t ALLOCATED =  0xEDA70C110A;
const uint64_t FREE = 0xEEF4EEF4;

typedef struct directory {
	block_id alloc_block_id;
	block_id free_block_id;
	block_id root_dir_id;
	block_size_t partition_size;
} directory;

typedef struct block_header {
	uint64_t magic; // indicates allocated or free.
	uint64_t size; // size of the _contents_ of this block.
	block_id previous_id;
	block_id next_id;
	// between here and the end of the block is the contents of the block.
} block_header;

/*

size_t fread(void *buffer, size_t size, size_t count, FILE *stream);

buffer	 -	 pointer to the array where the read objects are stored
size	 -	 size of each object in bytes
count	 -	 the number of the objects to be read
stream	 -	 the stream to read

-------------

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream);

buffer	 -	 pointer to the first object object in the array to be written
size	 -	 size of each object
count	 -	 the number of the objects to be written

*/

// Byte offset from the beginning of the file
void readPartition(uint64_t offset, void *data, uint64_t numBytes) {
	rewind(part);
	// The amount of rewinding is likely excessive, but has nice guarentees.

	if(fseek(part, offset, SEEK_SET)) {
		fprintf(stderr, "Error: seeking within partition failed.\n");
		_exit(0xbabecafe);
	}

	if(fread(data, numBytes, 1, part) != 1) {
		fprintf(stderr, "Error: reading from partition at offset %llu failed.\n", offset);
		_exit(0xcafebabe);
	}
	
	rewind(part);
}

// Offset from the beginning of the file.
void writePartition(uint64_t offset, void *data, uint64_t numBytes) {
	rewind(part);
	// The amount of rewinding is likely excessive, but has nice guarentees.

	if(fseek(part, offset, SEEK_SET)) {
		fprintf(stderr, "Error: seeking within partition failed.\n");
		_exit(0xbabecafe);
	}
	
	if(fwrite(data, numBytes, 1, part) != 1) {
		fprintf(stderr, "Error: writing to partition failed.\n");
		_exit(0xcafebabe);
	}

	rewind(part);
}

// we identify the block physically adjacent s.t. it follows the specified block
block_id look_right(block_id blknum) {
	block_header bh;
	readPartition(blknum, &bh, sizeof(block_header));

	uint64_t next = blknum + bh.size + (bh.magic == ALLOCATED ? sizeof(block_header) : 0);
	if(next >= sizeof(directory) + ((directory*)partDir)->partition_size) {
		// this is the last block in the filesystem.
		return 0;
	}

	return next;
}

// we identify the block physically adjacent s.t. it precedes the specified block
block_id look_left(block_id blk) {
	// the very first possible block.
	uint64_t currentBlk = sizeof(directory);

	if(blk == 0) {
		fprintf(stderr, "ERROR: shouldn't be asking for a block left of the partition descriptor!\n");
		_exit(12345);
	}

	if(currentBlk == blk) {
		// you're the first block, there's nothing to the left.
		return 0;
	}

	uint64_t nextBlk = look_right(currentBlk);
	while(nextBlk != blk && nextBlk != 0) {
		currentBlk = nextBlk;
		nextBlk = look_right(currentBlk);
	}

	if(nextBlk == 0) {
		fprintf(stderr, "ERROR: something went wrong in look_left!\n");
		_exit(1337);
	}

	return currentBlk;
}

//////
//
// PUBLICLY ACCESSIBLE FUNCTIONS BELOW.
//
/////


/**
 * Creates a file represting our file system partition on
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
int initialize(char* filename, uint64_t numBytes) {
	if(access(filename, F_OK ) != -1) {
		// file already exists

		part = fopen(filename, "rb+");

		if(part == NULL) {
			fprintf(stderr, "Unable to open file!\n");
			_exit(2);
		}

		// make space for it in memory
		partDir = malloc(sizeof(directory));
		// copy directory to memory.
		readPartition(0, partDir, sizeof(directory));

	} else {
		part = fopen(filename, "w+"); // open for read/write, at beginning

		if(part == NULL) {
			fprintf(stderr, "Unable to open file!\n");
			_exit(2);
		}

		partDir = calloc(1, sizeof(directory));
		directory *dirPtr = (directory*)partDir;

		dirPtr->free_block_id = sizeof(directory); // we know the id's are just byte offsets, this is the first block.
		dirPtr->partition_size = numBytes;
		writePartition(0, dirPtr, sizeof(directory));


		block_header newBlock;
		newBlock.size = numBytes;
		newBlock.magic = FREE;
		newBlock.previous_id = 0;
		newBlock.next_id = 0;

		// now we need to fill the file in with enough space for the sectors to
		// have it simulate being a hard drive.
		uint8_t zeroed_sector = 0;

		fseek(part, sizeof(directory), SEEK_SET); // start at the end of the partition directory.		
		for(block_size_t i = 0; i < numBytes; i++) {
			if(fwrite(&zeroed_sector, 1, 1, part) != 1) {
				fprintf(stderr, "Unable to completely allocate the partition!!\n");
				return 1;
			}
		}
		rewind(part);

		// now we need to initialize the initial free block
		writePartition(dirPtr->free_block_id, &newBlock, sizeof(block_header));

		fflush(part);

	}

	return 0;
}

/**
 * Prints info about the state of the partition (descriptor block and free block table stats)
 * to the specified file descriptor.
 */
void printInfo(FILE *dest) {
	fprintf(dest, "\n\t* Partition Information *\n\nAllocated Space:\n");

	directory d = *((directory*)partDir);
	block_size_t totalBytes = 0;

	block_id i = d.alloc_block_id;
	uint64_t numBlocks = 0;
	block_header bh;
	while(i != 0) {
		readPartition(i, &bh, sizeof(block_header));
		fprintf(dest, "offset %llu: %llu bytes  (%llu usable)\n", i, bh.size + sizeof(block_header), bh.size);
		totalBytes += bh.size + sizeof(block_header);
		i = bh.next_id;
		numBlocks++;
	}

	fprintf(dest, "--> total allocated: %llu bytes  (%llu usable)\n\nFree Space:\n", totalBytes, totalBytes - (numBlocks * sizeof(block_header)));

	totalBytes = 0;
	numBlocks = 0;

	i = d.free_block_id;
	while(i != 0) {
		readPartition(i, &bh, sizeof(block_header));
		fprintf(dest, "offset %llu: %llu bytes\n", i, bh.size);
		totalBytes += bh.size;
		i = bh.next_id;
		numBlocks++;
	}

	fprintf(dest, "--> total free: %llu bytes\n\n", totalBytes);
}

/**
 * Resizes an already allocated block in this partition, potentially moving it,
 * so you should upate the pointer to the one that is returned.
 *
 * Passing 0 in for the blk is equivalent to calling allocate_block.
 * 
 */
block_id resize_block(block_id blk, block_size_t size) {

	if(blk == 0) {
		return allocate_block(size);
	}

	block_header head;
	readPartition(blk, &head, sizeof(block_header));

	block_id newBlk = allocate_block(size);
	if(newBlk == 0) {
		fprintf(stderr, "Error: request to resize block in partition failed, not enough space!");
		_exit(-1);
	}

	void* buf = malloc(size);
	if(buf == NULL) {
		fprintf(stderr, "The file you're moving is quite large, and this version of the partitioner only supports files that can wholly fit in the heap.\n");
		_exit(-1);
	}

	// will truncate the file if the request is smaller.
	
	load_block(blk, buf, size);

	save_block(newBlk, buf, size);

	free(buf);

	free_block(blk);

	return newBlk;
}

/**
 * Allocates a new block in the partition.
 */
block_id allocate_block(block_size_t request_size) {
	block_header current;
	block_id currentPosition;
	directory *dir = (directory*)partDir;

	// actual size of the block we'll be allocating must include space for the header.
	uint64_t size = request_size + sizeof(block_header);

	currentPosition = dir->free_block_id;
	readPartition(currentPosition, &current, sizeof(block_header));

	if(currentPosition == 0) {
		fprintf(stderr, "Error: Partition is full!!\n");
		_exit(2);
	}

	if(current.magic != FREE) {
		// Somebody done fucked up....
		fprintf(stderr, "Somehow, the free list contains an allocated block :O\n");
		_exit(1231231);

		// not sure if its worth putting these checks in everywhere because they're
		// only caused by programming errors internal to this code, and it "looks good to me" (tm)
	}
	
	// find the FIRST block that FITs.
	while(current.next_id != 0 && current.size < size) {
		currentPosition = current.next_id;
		readPartition(currentPosition, &current, sizeof(block_header));
	}

	if(current.size < size) {
		// then we must not have found a block large enough.
		fprintf(stderr, "Error: There are no free blocks large enough for a %llu byte request!\n", size);
		_exit(1);
	}

	// now we have a usable free block
	block_size_t oldSize = current.size;
	block_id oldNext = current.next_id;
	block_id oldPrev = current.previous_id;

	// If the residual free block is less than 512 bytes, we give the whole block to the request...
	// there's enough overhead to make that remaining amount of space almost useless anyway.

	// If we were a little more clever, we'd overallocate slightly if it meant realignment
	// for the block following this one.

	if(oldSize - size >= 512) {
		
		// carve off a piece of this block for allocation.

		current.size = request_size;

		block_header newFree;
		newFree.magic = FREE;
		newFree.next_id = oldNext;
		newFree.previous_id = oldPrev;
		newFree.size = oldSize - size;

		writePartition(currentPosition + size, &newFree, sizeof(block_header));

		// update previous and next blocks with this new block.
		block_header temp;

		// note this block in the partition directory
		if(oldPrev == 0) {
			dir->free_block_id = currentPosition + size;
			writePartition(0, dir, sizeof(directory));
		} else {
			readPartition(oldPrev, &temp, sizeof(block_header));
			temp.next_id = currentPosition + size;
			writePartition(oldPrev, &temp, sizeof(block_header));
		}

		if(oldNext != 0) {
			readPartition(oldNext, &temp, sizeof(block_header));
			temp.previous_id = currentPosition + size;
			writePartition(oldNext, &temp, sizeof(block_header));
		}

	} else {
		// We're taking the whole block, so, just link previous and next
		// with each other to remove this block from the list.

		block_header temp;

		current.size = oldSize - sizeof(block_header);

		// just note the next block in the partition directory
		if(oldPrev == 0) {
			dir->free_block_id = oldNext;
			writePartition(0, dir, sizeof(directory));
		} else {
			readPartition(oldPrev, &temp, sizeof(block_header));
			temp.next_id = oldNext;
			writePartition(oldPrev, &temp, sizeof(block_header));
		}
		
		if(oldNext != 0) {
			readPartition(oldNext, &temp, sizeof(block_header));
			temp.previous_id = oldPrev;
			writePartition(oldNext, &temp, sizeof(block_header));
		}
	}

	// now that free list is fixed, we need to add this newly allocated block
	// to the allocated list. there's no actual reason why the allocated
	// list needs to be sorted by id, it only matters for the free list in order
	// to support a simpler block coalescing system.

	current.magic = ALLOCATED;
	current.previous_id = 0;
	current.next_id = 0;

	if(dir->alloc_block_id != 0) {
		uint64_t currentFirstPos = dir->alloc_block_id;
		block_header currentFirst;

		readPartition(currentFirstPos, &currentFirst, sizeof(block_header));
		currentFirst.previous_id = currentPosition;
		writePartition(currentFirstPos, &currentFirst, sizeof(block_header));

		current.next_id = currentFirstPos;
	}

	dir->alloc_block_id = currentPosition;
	writePartition(0, dir, sizeof(directory));

	// and save the new header.
	writePartition(currentPosition, &current, sizeof(block_header));

	return currentPosition;

}

/**
 * Frees the given block.
 * Do not call free on an already freed block pls.
 * Coalesces adjacent blocks.
 */
void free_block(block_id blk) {
	block_id left_id;
	block_id right_id;
	block_id newFree_id;

	block_header leftHead, rightHead, currentHead;
	block_header newFree;
	
	readPartition(blk, &currentHead, sizeof(block_header));

	if(currentHead.magic != ALLOCATED) {
		fprintf(stderr, "You're trying to free a block that is not allocated!\n");
		_exit(31337);
	}

	// remove from allocated list

	left_id = currentHead.previous_id;
	right_id = currentHead.next_id;

	if(left_id != 0) {
		readPartition(left_id, &leftHead, sizeof(block_header));
	}

	if(right_id != 0) {
		readPartition(right_id, &rightHead, sizeof(block_header));
	}

	if(left_id == 0) {
		((directory*)partDir)->alloc_block_id = right_id;
		writePartition(0, partDir, sizeof(directory));

	} else {
		leftHead.next_id = right_id;
		writePartition(left_id, &leftHead, sizeof(block_header));
	}

	if(right_id != 0) {
		// update right_id too
		rightHead.previous_id = left_id;
		writePartition(right_id, &rightHead, sizeof(block_header));
	}


	// add it to the free list.
	left_id = look_left(blk);
	right_id = look_right(blk);

	newFree_id = blk;
	newFree.magic = FREE;
	newFree.size = currentHead.size + sizeof(block_header); // allocated blocks don't include header in its field

	if(left_id != 0) {
		readPartition(left_id, &leftHead, sizeof(block_header));
	}

	if(right_id != 0) {
		readPartition(right_id, &rightHead, sizeof(block_header));
	}

	//fprintf(stderr, "Looking around, I see: %llu (free? %i) <- %llu -> %llu (free? %i)\n", left_id, leftHead.magic == FREE, blk, right_id, rightHead.magic == FREE);

	// yeah... the below logic could be optimized, but it's 4am

	// ADJUST SIZE OF NEW FREE BLOCK
	if(leftHead.magic == FREE) {
		// we can extend left!
		newFree_id = left_id;
		// we add the _entire_ left block, we already account for header space from our old size.
		newFree.size += leftHead.size;
	}

	if(rightHead.magic == FREE) {
		// aww yiss, we can extend right 
		newFree.size += rightHead.size;
	}

	
	// By the property that the free list is _ordered_, by block_id, we
	// adjust the pointers very simply:
	if(leftHead.magic == FREE && rightHead.magic == FREE) {
		// hit the jackpot here... this block is between two free blocks.
		// we can just update the left one, and remove the right one.

		leftHead.size = newFree.size;
		leftHead.next_id = rightHead.next_id;
		writePartition(left_id, &leftHead, sizeof(block_header));

	} else if(leftHead.magic == FREE) {
		// just extend the left block

		leftHead.size = newFree.size;
		writePartition(left_id, &leftHead, sizeof(block_header));

	} else if(rightHead.magic == FREE) {
		// remove the right block, and and make this one bigger

		newFree.previous_id = rightHead.previous_id;
		newFree.next_id = rightHead.next_id;

		if(newFree.previous_id == 0) {
			((directory*)partDir)->free_block_id = newFree_id;
			writePartition(0, partDir, sizeof(directory));
		}

		writePartition(newFree_id, &newFree, sizeof(block_header));

	} else {
		// we're surrounded by allocated blocks, so we must traverse the
		// free list and find the right place for this block.

		block_header guyInfo;
		block_id guy = ((directory*)partDir)->free_block_id;
		readPartition(guy, &guyInfo, sizeof(block_header));

		newFree.previous_id = 0;
		newFree.next_id = 0;

		if(guy == 0) {
			// there are no free blocks, you're the first and only one.

			((directory*)partDir)->free_block_id = newFree_id;
			writePartition(0, partDir, sizeof(directory));

		} else {

			while(guy < newFree_id && guyInfo.next_id != 0) {
				guy = guyInfo.next_id;
				readPartition(guy, &guyInfo, sizeof(block_header));
			}

			if(guy < newFree_id) {
				// we go _after_ guy
				guyInfo.next_id = newFree_id;
				newFree.previous_id = guy;

				writePartition(guy, &guyInfo, sizeof(block_header));

			} else {
				// we go _before_ guy

				// need to let the block before guy know who's really next
				if(guyInfo.previous_id == 0) {
					// we're about to become the first free block.
					((directory*)partDir)->free_block_id = newFree_id;
					writePartition(0, partDir, sizeof(directory));
				} else {
					// this is a block we need to update.

					readPartition(guyInfo.previous_id, &leftHead, sizeof(block_header));
					leftHead.next_id = newFree_id;
					writePartition(guyInfo.previous_id, &leftHead, sizeof(block_header));
				}

				newFree.previous_id = guyInfo.previous_id;
				newFree.next_id = guy;

				// update guy itself
				guyInfo.previous_id = newFree_id;
				writePartition(guy, &guyInfo, sizeof(block_header));
			}
		}

		// gotta update newFree in all cases.
		writePartition(newFree_id, &newFree, sizeof(block_header));

	}	
}

/**
 * Copies at most numBytes bytes from this block into the specified pointer.
 * Use this to read the block, and you can modify and save it back.
 */
void load_block(block_id blk, void* destination, size_t numBytes) {
	//TODO: check that they're not reading past the end.
	readPartition(blk + sizeof(block_header), destination, numBytes);
}

/**
 * Overwrites the block's contents (starting at the beginning of the block) with
 * at most min(numBytes, block size) bytes, effectively saving it to the disk. 
 * 
 */
void save_block(block_id blk, void *source, size_t numBytes) {
	//TODO: check that they're not writing past the end.
	writePartition(blk + sizeof(block_header), source, numBytes);
}

/**
 * Saves the block_id represting the root directory in our filesystem structure
 * to the partition descriptor.
 */
void saveRootID(block_id id) {
	((directory*)partDir)->root_dir_id = id;
	 // no need to save the bitmap here anyway
	writePartition(0, partDir, sizeof(directory));
}

/**
 * Retrieves the block_id representing the root directory in this partition.
 * If the returned value is equal to 0, then there does not exist a root directory
 * that was saved to the descriptor.
 */
block_id getRootID() {
	return ((directory*)partDir)->root_dir_id;
}




