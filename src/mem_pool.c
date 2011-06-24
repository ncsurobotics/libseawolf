/**
 * \file
 * \brief Fast memory pool backend for Comm
 */

#include "seawolf.h"
#include "seawolf/mem_pool.h"

#include <pthread.h>

/**
 * \defgroup MemPool Memory pool
 * \ingroup Utilities
 * \brief Fast, caching memory pool
 * \{
 *
 * This special purpose memory allocator was designed to make the memory
 * management of the Comm component and the hub more efficient. This is
 * accomplished by caching memory and using fixed-sized allocations which are
 * used for the entire life of a message from first allocation, to packing, and
 * finally sending.
 *
 * MemPool gets backing memory in "blocks" from the operating system. These are
 * 4096 bytes in size and are subdivided and allocated into 512 byte allocations
 * by MemPool. It is these 512 byte allocations that MemPool_alloc()
 * returns. They are returned as a pointer within a structure, but the caller is
 * never meant to directly access the contents of this structure. Instead, the
 * caller should use the space with calls to MemPool_reserve, MemPool_write, and
 * MemPool_strdup. These calls use the allocated 512 linearly, and additionally
 * each call returns a pointer to the beginning of the space it just used.
 *
 * For example, consider the following use case of building a message:
 *
 * \code
 * // Comm_Message_new gets an allocations with MemPool_alloc and stores it to
 * // message->alloc
 * Comm_Message* message = Comm_Message_new(2);
 * 
 * // Calls to MemPool_strdup copy the data and return a pointer to the
 * // beginning of the copy
 * message->components[0] = MemPool_strdup(message->alloc, "Hello, world.");
 * message->components[1] = MemPool_strdup(message->alloc, "Foo bar");
 *
 * // Will use the existing allocation to pack the message
 * Comm_sendMessage(message)
 *
 * // Internally calls MemPool_free(message->alloc) which returns all the memory
 * // used above
 * Comm_Message_destroy(message);
 * \endcode
 *
 * In the event that an allocation is completely utilized and needs more than
 * the default 512 bytes, it will automatically be moved to what is called an
 * external allocation. This simply means that the memory for this allocation
 * is allocated directly through malloc. It is an assumption of the use case
 * that an external allocation should rarely (if not never) be needed. If many
 * allocations are being converted to external allocations then the default
 * allocation size should be increased.
 */

/**
 * Standard allocation size. This value must evenly divide BLOCK_SIZE
 */
#define DEFAULT_ALLOCATION 512

/**
 * Size in bytes of backing blocks. These are allocated using malloc. There must
 * be no more than 32 allocations per block
 */
#define BLOCK_SIZE 4096

/**
 * Allocate 16 descriptors at a time
 */
#define DESCRIPTOR_POOL_GROW 16

/**
 * Align all allocations on a byte boundary of this alignment
 */
#define ALLOC_ALIGNMENT 2

/**
 * Value of mem when full
 */
#define FULL_MAP (1 << (BLOCK_SIZE / DEFAULT_ALLOCATION)) - 1

static List* blocks = NULL;
static List* descriptor_pool = NULL;
static MemPool_Alloc* free_descriptors = NULL;

static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;

static MemPool_Block* MemPool_allocNewBlock(void);
static MemPool_Block* MemPool_getBlockForAlloc(void);
static void MemPool_setDescriptorFree(MemPool_Alloc* descriptor);
static MemPool_Alloc* MemPool_getDescriptor(void);
static void MemPool_releaseChunk(MemPool_Alloc* alloc);

/**
 * \brief Initialize the MemPool component
 *
 * Initialize the MemPool component
 */
void MemPool_init(void) {
    descriptor_pool = List_new();
    blocks = List_new();
}

/**
 * \brief Close the MemPool component
 *
 * Close the MemPool component
 */
void MemPool_close(void) {
    MemPool_Alloc* descriptor_group;
    MemPool_Block* block;

    while ((block = List_remove(blocks, 0)) != NULL) {
        free(block->base);
        free(block);
    }
    List_destroy(blocks);

    while((descriptor_group = List_remove(descriptor_pool, 0)) != NULL) {
        free(descriptor_group);
    }
    List_destroy(descriptor_pool);
}

/**
 * Initialize a new MemPool_Block and store it into the blocks list
 */
static MemPool_Block* MemPool_allocNewBlock(void) {
    MemPool_Block* new_block = malloc(sizeof(MemPool_Block));
    new_block->base = malloc(BLOCK_SIZE);
    new_block->index = List_getSize(blocks);
    new_block->alloc_map = 0;
    new_block->full = false;
    pthread_mutex_init(&new_block->lock, NULL);
    pthread_mutex_lock(&new_block->lock);
    List_append(blocks, new_block);

    return new_block;
}

/**
 * Gets a block that has room for an allocation and returns it in a locked state
 */
static MemPool_Block* MemPool_getBlockForAlloc(void) {
    MemPool_Block* block = NULL;
    int num_blocks, i;

    pthread_mutex_lock(&pool_lock);
    num_blocks = List_getSize(blocks);
    for (i = 0; i < num_blocks; i++) {
        block = List_get(blocks, i);

        pthread_mutex_lock(&block->lock);
        if (block->full == false) {
            goto release_pool_lock;
        }

        pthread_mutex_unlock(&block->lock);
    }

    block = MemPool_allocNewBlock();

 release_pool_lock:
    pthread_mutex_unlock(&pool_lock);
    return block;
}

/**
 * Mark a previously used descriptor as free by storing it into the free
 * descriptors linked list
 */
static void MemPool_setDescriptorFree(MemPool_Alloc* descriptor) {
    descriptor->next_free = free_descriptors;
    free_descriptors = descriptor;
}

/**
 * Free an allocation (chunk) in a block so that it can be reallocated
 */
static void MemPool_releaseChunk(MemPool_Alloc* alloc) {
    MemPool_Block* block = List_get(blocks, alloc->block_index);

    pthread_mutex_lock(&block->lock);
    int chunk_index = (((uint8_t*) alloc->base) - ((uint8_t*) block->base)) / DEFAULT_ALLOCATION;
    block->alloc_map &= ~(1 << chunk_index);
    block->full = false;
    pthread_mutex_unlock(&block->lock);
}

/**
 * Get a descriptor for a memory allocation
 */
static MemPool_Alloc* MemPool_getDescriptor(void) {
    MemPool_Alloc* descriptors = NULL;

    pthread_mutex_lock(&pool_lock);
    /* Allocate new free allocation descriptors */
    if (free_descriptors == NULL) {
        descriptors = malloc(sizeof(MemPool_Alloc) * DESCRIPTOR_POOL_GROW);
        List_append(descriptor_pool, descriptors);

        for (int i = 0; i < DESCRIPTOR_POOL_GROW; i++) {
            MemPool_setDescriptorFree(descriptors + i);
        }
    }

    descriptors = free_descriptors;
    free_descriptors = descriptors->next_free;
    pthread_mutex_unlock(&pool_lock);

    return descriptors;
}

/**
 * \brief Get a new allocation
 *
 * Return a new allocation of fixed capacity that can be used with
 * MemPool_write, MemPool_reserve, and MemPool_strdup. When the allocation is no
 * longer needed, it should be passed to MemPool_free
 *
 * \return The new allocation object
 */
MemPool_Alloc* MemPool_alloc(void) {
    MemPool_Block* block = MemPool_getBlockForAlloc();
    uint32_t m;
    int i;

    /* Find free chunk */
    for (i = 0; i < (BLOCK_SIZE / DEFAULT_ALLOCATION); i++) {
        m = (1 << i);
        if ((block->alloc_map & m) == 0) {
            break;
        }
    }

    /* Mark block as allocated */
    block->alloc_map |= m;

    /* Allocating the last chunk; block is full */
    if (block->alloc_map == FULL_MAP) {
        block->full = true;
    }

    /* Release lock */
    pthread_mutex_unlock(&block->lock);

    MemPool_Alloc* alloc = MemPool_getDescriptor();

    alloc->base = ((uint8_t*) block->base) + (i * DEFAULT_ALLOCATION);
    alloc->write_index = 0;
    alloc->size = DEFAULT_ALLOCATION;
    alloc->block_index = block->index;
    alloc->external = false;

    return alloc;
}

/**
 * \brief Free an allocation
 *
 * Free an allocation previously returned by MemPool_alloc
 *
 * \param alloc The allocation to free
 */
void MemPool_free(MemPool_Alloc* alloc) {
    if (alloc->external) {
        free(alloc->base);
    } else {
        MemPool_releaseChunk(alloc);
    }

    pthread_mutex_lock(&pool_lock);
    MemPool_setDescriptorFree(alloc);
    pthread_mutex_unlock(&pool_lock);
}

/**
 * \brief Write to an allocation
 *
 * Copy data into the allocation and return a pointer to the beginning of the
 * written data. This is similar to a malloc followed by a memcpy.
 *
 * \param alloc The allocation to copy to
 * \param data A pointer to the data to copy
 * \param size The number of bytes to copy from data
 * \return A pointer to the beginning of the copied data
 */
void* MemPool_write(MemPool_Alloc* alloc, const void* data, size_t size) {
    void* p = MemPool_reserve(alloc, size);
    memcpy(p, data, size);

    return p;
}

/**
 * \brief Copy a string to the allocation
 *
 * Copy a null-terminated string to the allocation and return a pointer to it.
 * This call is equivalent to strdup, but for MemPool allocations
 *
 * \param alloc The allocation to copy to
 * \param s The null-terminated string to copy
 * \return A pointer to the copy
 */
void* MemPool_strdup(MemPool_Alloc* alloc, const char* s) {
    return MemPool_write(alloc, s, strlen(s) + 1);
}

/**
 * \brief Reserve space in an allocation
 *
 * Reserve space in the allocation that can be written to by the caller instead
 * of by one of MemPool_write or MemPool_strup. This call is therefore
 * analogous to malloc.
 *
 * \param alloc The allocation to reserve space in
 * \param size The number of bytes to reserve
 * \return A pointer to the reserved space
 */
void* MemPool_reserve(MemPool_Alloc* alloc, size_t size) {
    void* p;

    /* Round up to the nearest boundary to keep alignment if not already aligned */
    if(size % ALLOC_ALIGNMENT > 0) {
        size += (ALLOC_ALIGNMENT - (size % ALLOC_ALIGNMENT));
    }

    /* If there is space go ahead and write */
    if (alloc->write_index + size < alloc->size) {
        /* No more space needed */
    } else if (alloc->external) {
        alloc->base = realloc(alloc->base, alloc->write_index + size);

        /* No space but this is an internal allocation so we turn it into an
           external one */
    } else {
        void* temp = malloc(alloc->write_index + size);
        memcpy(temp, alloc->base, alloc->write_index);

        MemPool_releaseChunk(alloc);
        alloc->base = temp;
        alloc->external = true;
    }

    p = ((uint8_t*) alloc->base) + alloc->write_index;
    alloc->write_index += size;

    return p;
}

/** \} */
