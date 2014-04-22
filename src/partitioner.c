#include "partitioner.h"

// file pointer to beginning of this partition
static FILE *part;

// in-memory version of the partition directory
static void* partDir;

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
		printf("Unable to open file!\n");
		return 2;
	}

	if(fileAlreadyExists) {
		// need to know how big the partition directory is
		uint64_t size;
		rewind(part);
		fread(&size, 8, 1, part);
		// TODO: error check

		// make space for it in memory
		partDir = malloc(size);
		// copy directory to memory.
		rewind(part);
		fread(partDir, 1, size, part);
		rewind(part);

	} else {

		// we need to initialize a partition directory and write it out to the file.
		uint64_t numSectors = (numBytes / SECTOR_SIZE);
		if(numBytes % SECTOR_SIZE != 0) {
			numSectors += 1;
		}

		uint64_t size = sizeof(directory) + ((numSectors / 8) + 1) + 8;
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
				printf("Unable to completely allocate the partition!!\n");
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

}

/**
 * Allocates a new block in the partition.
 */
block_id allocate_block(block_size_t size) {

}

/**
 * Frees the given block.
 * Do not call free on an already freed block.
 */
void free_block(block_id blk) {

}

/**
 * Copies at most numBytes bytes from this block into the specified pointer.
 * Use this to read the block, and you can modify and save it back.
 */
void load_block(block_id blk, void* destination, size_t numBytes) {
	readPartition(blk, destination, numBytes);
}

/**
 * Overwrites the block's contents (starting at the beginning of the block) with
 * at most min(numBytes, block size) bytes, effectively saving it to the disk. 
 * 
 */
void save_block(block_id blk, void *source, size_t numBytes) {
	writePartition(blk, source, numBytes);
}




