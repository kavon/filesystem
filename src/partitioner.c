#include "partitioner.h"

// file pointer to beginning of this partition
static FILE *part;

// in-memory version of the partition directory
static void* partDir;
static uint64_t partDir_size;

// 4kb sectors
const uint64_t SECTOR_SIZE = 4096;

// little endian machines flip these Magic values.
const uint64_t ALLOCATED =  0xEDA70C110A;
const uint64_t FREE = 0xEEF4EEF4;

typedef struct directory {
	uint64_t size; // everything, including bitmap stuff.
	uint64_t alloc_block_id;
	uint64_t free_block_id;
	uint64_t num_sectors;
	uint64_t sector_size;
} directory;

typedef struct block_header {
	uint64_t magic; // indicates allocated or free.
	uint64_t size; // size of the _contents_ of this block.
	uint64_t previous_id;
	uint64_t next_id;
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

void readPartition(uint64_t offset, void *data, uint64_t numBytes) {
	rewind(part);
	fseek(part, offset, SEEK_SET);
	fread(data, numBytes, 1, part);
	rewind(part);
}

// Offset from the beginning of the file.
void writePartition(uint64_t offset, void *data, uint64_t numBytes) {
	rewind(part);
	if(fseek(part, offset, SEEK_SET)) {
		// TODO: catch error
	}
	
	if(fwrite(data, numBytes, 1, part) != 1) {
		//TODO: catch error
	}

	rewind(part);
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
	bool fileAlreadyExists = (access(filename, F_OK ) != -1);
	part = fopen(filename, "w+"); // open for read/write, at beginning

	if(part == NULL) {
		fprintf(stderr, "Unable to open file!\n");
		return 2;
	}

	if(fileAlreadyExists) {
		// need to know how big the partition directory is
		uint64_t size;
		readPartition(0, &size, 8);
		// TODO: error check

		partDir_size = size;

		// make space for it in memory
		partDir = malloc(partDir_size);
		// copy directory to memory.
		readPartition(0, partDir, size);

	} else {

		// we need to initialize a partition directory and write it out to the file.
		uint64_t numSectors = (numBytes / SECTOR_SIZE);
		if(numBytes % SECTOR_SIZE != 0) {
			numSectors += 1;
		}

		uint64_t size = sizeof(directory) + ((numSectors / 8) + 1) + 8;
		partDir_size = size;
		partDir = calloc(1, size);
		directory *dirPtr = (directory*)partDir;

		dirPtr->size = size;
		dirPtr->num_sectors = numSectors;
		dirPtr->sector_size = SECTOR_SIZE;

		void *bitmap = (((uint8_t*)partDir) + sizeof(directory));
		createBitmap(numSectors, bitmap, size - sizeof(directory));

		dirPtr->free_block_id = size; // we know the id's are just byte offsets, this is the first block.

		block_header newBlock;
		newBlock.size = (numSectors * SECTOR_SIZE) - sizeof(block_header);
		newBlock.magic = FREE;
		newBlock.previous_id = 0;
		newBlock.next_id = 0;

		writePartition(0, dirPtr, size);

		// now we need to fill the file in with enough space for the sectors to
		// have it simulate being a hard drive.
		uint8_t *zeroed_sector = calloc(1, SECTOR_SIZE);

		fseek(part, size, SEEK_SET); // start at the end of the partition directory.
		for(unsigned long i = 0; i < numSectors; i++) {			
			if(fwrite(zeroed_sector, SECTOR_SIZE, 1, part) != 1) {
				fprintf(stderr, "Unable to completely allocate the partition!!\n");
				return 1;
			}
		}
		rewind(part);
		free(zeroed_sector);

		// now we need to initialize the initial free block
		writePartition(dirPtr->free_block_id, &newBlock, sizeof(block_header));

	}

	return 0;
}

/**
 * Prints info about the state of the partition (descriptor block and free block table stats)
 * to the specified file descriptor.
 */
void printInfo(FILE *dest) {
	fprintf(dest, "Hello, the partition stats should be here.\n");
}

/**
 * Resizes an already allocated block in this partition, potentially moving it,
 * so you should upate the pointer to the one that is returned.
 *
 * Passing NULL in for the ptr is equivalent to calling allocate_block.
 * 
 */
block_id resize_block(block_id blk, block_size_t size) {
	return 0;
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
		//TODO: Somebody done fucked up....
		// not sure if its worth putting these checks in right now though.
	}
	

	while(current.size < size && current.next_id != 0) {
		currentPosition = current.next_id;
		readPartition(currentPosition, &current, sizeof(block_header));
	}

	if(current.size < size) {
		fprintf(stderr, "Error: Not enough space for a %llu byte block.\n", size);
		_exit(1);
	}

	// now we have a usable free block
	uint64_t oldSize = current.size;
	block_id oldNext = current.next_id;
	block_id oldPrev = current.previous_id;

	// if the residual free block is less than 512 bytes, we give the whole block to the request...
	// there's enough overhead to make that remaining amount of space almost useless anyway.

	if(oldSize - size >= 512) {
		// chop off a new free block.
		block_header newFree;
		newFree.magic = FREE;
		newFree.next_id = oldNext;
		newFree.previous_id = oldPrev;
		newFree.size = oldSize - size;

		writePartition(currentPosition + size, &newFree, sizeof(block_header));

		// update previous and next blocks with this new block.
		block_header temp;

		// just note this block in the partition directory
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
		// otherwise, just link previous and next.
		block_header temp;

		// just note this block in the partition directory
		if(oldPrev == 0) {
			dir->free_block_id = currentPosition + size;
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

	// now, we need to add this newly allocated block to the allocated list.
	// we keep these lists sorted by block id, so we must find the right position.

	current.magic = ALLOCATED;
	current.size = request_size;
	current.previous_id = 0;
	current.next_id = 0;

	
	uint64_t tempAllocPos = dir->alloc_block_id;

	if(tempAllocPos == 0) {

		// Nothing's allocated, just place this block id here.
		dir->alloc_block_id = currentPosition;
		writePartition(0, dir, sizeof(directory));

		// and save the new header.
		writePartition(currentPosition, &current, sizeof(block_header));

	} else {
		// find an appropraite place.
		block_header tempAlloc;
		readPartition(tempAllocPos, &tempAlloc, sizeof(block_header));
		while(tempAlloc.next_id != 0 && tempAlloc.next_id < currentPosition) {
			tempAllocPos = tempAlloc.next_id;
			readPartition(tempAllocPos, &tempAlloc, sizeof(block_header));
		}

		// stick this block after the current block.
		
		// new previous
		block_id oldNext = tempAlloc.next_id;
		tempAlloc.next_id = currentPosition;
		writePartition(tempAllocPos, &tempAlloc, sizeof(block_header));

		// new next
		if(oldNext != 0) {
			readPartition(oldNext, &tempAlloc, sizeof(block_header));
			tempAlloc.previous_id = currentPosition;
			writePartition(oldNext, &tempAlloc, sizeof(block_header));
		}

		// new block in the middle.
		current.previous_id = tempAllocPos;
		current.next_id = oldNext;
		writePartition(currentPosition, &current, sizeof(block_header));

	}

	/*
	// finally, we need to mark off the newly used sectors in the bitmap.
	uint64_t startSector = (currentPosition - partDir_size) / SECTOR_SIZE;
	uint64_t endSector = ((currentPosition - partDir_size) + current.size - 1) / SECTOR_SIZE;
	void *bitmap = ((uint8_t*)partDir) + partDir_size;
	for(uint64_t i = startSector; i < endSector; i++) {
		setBit(i, bitmap);
	}
	// write partDir to disk.
	*/

	return currentPosition;

}

/**
 * Frees the given block.
 * Do not call free on an already freed block pls.
 */
void free_block(block_id blk) {

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




