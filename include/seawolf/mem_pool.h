/**
 * \file
 */

#ifndef __SEAWOLF_MEM_POOL_INCLUDE_H
#define __SEAWOLF_MEM_POOL_INCLUDE_H

#include <pthread.h>

/**
 * \addtogroup MemPool
 * \{
 */

/**
 * \brief An allocation as returned to a caller
 *
 * A memory allocation. The caller can write to and sub-allocate this space
 * using MemPool_reserve, MemPool_write, and MemPool_strdup.
 */
struct MemPool_Alloc_s {
	/**
	 * The base address of this memory allocation
	 */
	void* base;

	/**
	 * The write index (in bytes) within the block
	 */
	uint16_t write_index;

	/**
	 * The size of the block
	 */
	size_t size;

	/**
	 * The index of the block this allocation was made from
	 */
	int block_index;

	/**
	 * True if this allocation has been directly malloced
	 */
	bool external;

	/**
	 * Pointer to the next free allocation descriptor if this descriptor is
	 * also free
	 */
	struct MemPool_Alloc_s* next_free;
};
typedef struct MemPool_Alloc_s MemPool_Alloc;

/**
 * \brief A memory block
 *
 * A memory block has space for it allocated using malloc. Allocations
 * (MemPool_Alloc's) are drawn from these blocks
 */
typedef struct {
	/**
	 * The base address of the block
	 */
	void* base;

	/**
	 * Each block has a unique index specifying its index within the block list
	 */
	uint16_t index;

	/**
	 * A bit-map of which chunks in the block are allocated to MemPool_Alloc's
	 */
	uint32_t alloc_map;

	/**
	 * A lock that must be acquire to modify/access the block
	 */
	pthread_mutex_t lock;

	/**
	 * The block is fully allocated (no free chunks)
	 */
	bool full;
} MemPool_Block;

/** \} */

void MemPool_init(void);
MemPool_Alloc* MemPool_alloc(void);
void* MemPool_write(MemPool_Alloc* alloc, const void* data, size_t size);
void* MemPool_strdup(MemPool_Alloc* alloc, const char* s);
void* MemPool_reserve(MemPool_Alloc* alloc, size_t size);
void MemPool_free(MemPool_Alloc* alloc);
void MemPool_close(void);

#endif // #ifndef __SEAWOLF_MEM_POOL_INCLUDE_H
