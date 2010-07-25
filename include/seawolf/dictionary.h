/**
 * \file
 */
 
#ifndef __SEAWOLF_DICTIONARY_INCLUDE_H
#define __SEAWOLF_DICTIONARY_INCLUDE_H

#include "seawolf/list.h"

#include <stdint.h>
#include <pthread.h>

/**
 * Number of buckets to create by default
 * \private
 */
#define DICTIONARY_INITIAL_BUCKETS 16

/**
 * The maximum number of buckets to allow
 * \private
 */
#define DICTIONARY_MAXIMUM_BUCKETS 65536

/**
 * The average number items per bucket that must be reached for the dictionary
 * to be rebuilt with a larger bucket count
 * \private
 */
#define DICTIONARY_LOAD_FACTOR 8

/**
 * An item in a dictionary
 * \private
 */
struct Dictionary_Item_s {
    /**
     * Key
     * \private
     */
    void* k;

    /**
     * Key size/length
     * \private
     */
    size_t k_size;

    /**
     * Value
     * \private
     */
    void* v;

    /**
     * Pointer to next item in list
     * \private
     */
    struct Dictionary_Item_s* next;
};

typedef struct Dictionary_Item_s Dictionary_Item;

/**
 * Dictionary object
 */
typedef struct {
    /**
     * Dictionary buckets
     * \private
     */
    Dictionary_Item** buckets;

    /**
     * Number of buckets
     * \private
     */
    unsigned int bucket_count;

    /**
     * Number of items in the dictionary
     * \private
     */
    unsigned int item_count;

    /**
     * Dictionary mutex lock
     * \private
     */
    pthread_mutex_t lock;

    /**
     * New item conditional
     * \private
     */
    pthread_cond_t new_item;
} Dictionary;

typedef uint32_t hash_t;

hash_t Dictionary_hash(const void* s, size_t n);

Dictionary* Dictionary_new(void);
void Dictionary_destroy(Dictionary* dict);
List* Dictionary_getKeys(Dictionary* dict);

void Dictionary_setData(Dictionary* dict, const void* k, size_t k_size, void* v);
void Dictionary_setInt(Dictionary* dict, int i, void* v);
void Dictionary_set(Dictionary* dict, const char* k, void* v);

void* Dictionary_getData(Dictionary* dict, const void* k, size_t k_size);
void* Dictionary_getInt(Dictionary* dict, int k);
void* Dictionary_get(Dictionary* dict, const char* k);

void Dictionary_waitForData(Dictionary* dict, const void* k, size_t k_size);
void Dictionary_waitForInt(Dictionary* dict, int k);
void Dictionary_waitFor(Dictionary* dict, const char* k);

bool Dictionary_existsData(Dictionary* dict, const void* k, size_t k_size);
bool Dictionary_existsInt(Dictionary* dict, int k);
bool Dictionary_exists(Dictionary* dict, const char* k);

int Dictionary_removeData(Dictionary* dict, const void* k, size_t k_size);
int Dictionary_removeInt(Dictionary* dict, int k);
int Dictionary_remove(Dictionary* dict, const char* k);

#endif // #ifndef __SEAWOLF_DICTIONARY_INCLUDE_H
