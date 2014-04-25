filesystem
==========

A short group project for an Operating Systems course.

### Partition Structure

The partitioning code was designed such that it would be quick to implement and easy to verify correctness, rather than efficiency. This might change in the future.

##### Descriptor

The descriptor size is fixed at 32 bytes and is located at the beginning of the partition file, containing the following information:

Offset | Type |  Description
--- | --- | ---
0 |  `uint64_t` | first allocated block id
8 |  `uint64_t` | first free block id
16 | `uint64_t` | block containing the root directory of the filesystem
24 | `uint64_t` | size of the partition


##### Blocks

The block allocator had already been developed before it was realized that we are not respecting a disk sector alignment. We also wanted to reduce internal fragmentation introduced by allocating full sectors to small files. While it would be a more accurate model of disk design, for the sake of time, we have left that out for now. Thus, there is no data structure such as a bitmap marking free/allocated sectors in the partition descriptor.

The requests for space in the partition are carried out with a traditional dynamic memory allocation system. The allocated and free blocks are kept in a doubly linked list, allocations are fufilled with a first fit strategy, and adjacent free blocks are always coalesced. Here's the header used for each block:

Offset | Type | Description
--- | --- | ---
0 | `uint64_t` | `0xEDA70C110A` if the block is allocated, `0xEEF4EEF4` if free.
8 | `uint64_t` | size of this block. does not include header if allocated, but does if free
16 | `uint64_t` | previous block id, 0 if nothing precedes it.
24 | `uint64_t` | next block id, 0 if nothing follows.




