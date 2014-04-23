#include "bitmap.h"

/**
 * Constructs and returns a new bitmap, initialized to all 0's.
 * The first 8 bytes represents the size of the bitmap.
 */
size_t createBitmap(uint64_t numBits, void* location, size_t size) {
	uint64_t numBitsRequested = numBits;

	if (numBits % 8 != 0) {
		numBits = (numBits / 8) + 1;
	} else {
		numBits /= 8;
	}

	// need space for the size.
	numBits += 8;

	if (size < numBits) {
		fprintf(stderr, "ERROR: bitmap location too small.\n");
	}

	memset(location, 0, numBits);

	*((uint64_t*)location) = numBitsRequested;

	return numBits;
}

/**
 * Sets bit #num to 1 in the given bitmap
 * in a thread-safe manner. Does do bounds checking.
 */
void setBit(uint64_t num, void *bitmap) {
	uint8_t *b = ((uint8_t*)bitmap) + 8;

	if(num > *((uint64_t*)bitmap)) {
		fprintf(stderr, "ERROR: bounds check fail on setBit in bitmap.\n");
		_exit(-1);
	}

	// outer is obviously the number of bytes to offset to
	// find the right byte, and then inner finds the correct
	// bit in that byte.

	uint64_t outer = num / 8;
	uint64_t inner = num - (outer * 8);

	// OR that byte with a byte that has the specific bit set.
	__sync_or_and_fetch(b + outer, ((uint8_t)1) << inner);
} 

/**
 * Returns non-zero iff bit #num is 1 in the given bitmap.
 * Does do bounds checking.
 */
int checkBit(uint64_t num, void *bitmap) {
	uint8_t *b = ((uint8_t*)bitmap) + 8;

	if(num > *((uint64_t*)bitmap)) {
		fprintf(stderr, "ERROR: bounds check fail on checkBit in bitmap.\n");
		_exit(-1);
	}

	uint64_t outer = num / 8;
	uint64_t inner = num - (outer * 8);

	// ORing with zero just to fetch the value using the atomic built-in.
	uint8_t val = __sync_or_and_fetch(b + outer, ((uint8_t)0));
	return (val & (((uint8_t)1) << inner)) > 0;
}

/**
 * Clears bit #num, aka sets it to 0, in the given bitmap
 * in a thread-safe manner. Does do bounds checking.
 */
void clearBit(uint64_t num, void *bitmap) {
	uint8_t *b = ((uint8_t*)bitmap) + 8;

	if(num > *((uint64_t*)bitmap)) {
		fprintf(stderr, "ERROR: bounds check fail on clearBit in bitmap.\n");
		_exit(-1);
	}

	uint64_t outer = num / 8;
	uint64_t inner = num - (outer * 8);

	// AND that byte with a byte with all 1's except the bit we're
	// clearing.
	__sync_and_and_fetch(b + outer, ~(((uint8_t)1) << inner) );
}


