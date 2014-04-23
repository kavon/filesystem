#ifndef __BITMAP_H
#define __BITMAP_H

#include "stdint.h"
#include "stdlib.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>


size_t createBitmap(uint64_t numBits, void* location, size_t size);

void setBit(uint64_t num, void *bitmap);

int checkBit(uint64_t num, void *bitmap);

void clearBit(uint64_t num, void *bitmap);

#endif /* __BITMAP_H */