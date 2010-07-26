/**
 * \file
 * \brief Dictionary/hash map
 */

#include "seawolf.h"

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

static Dictionary_Item* Dictionary_getItem(Dictionary* dict, const void* k, size_t k_size);
static Dictionary_Item* Dictionary_Item_new(const void* k, size_t k_size, void* v);
static void Dictionary_Item_destroy(Dictionary_Item* item);

/**
 * \defgroup Dictionary Dictionary
 * \ingroup DataStructures
 * \brief Provides a hash map data structure, often known as a dictionary
 * \sa http://en.wikipedia.org/wiki/Hash_map
 * \{
 */

/**
 * \brief Hash a block of memory
 *
 * Return a hash code of the give memory space
 *
 * \param s Pointer to the beginning of the memory space
 * \param n Bytes to include in the hash
 * \return The hash of the given space
 */
hash_t Dictionary_hash(const void* s, size_t n) {
    hash_t hash = 5381;

    for(size_t i = 0; i < n; i++) {
        hash = ((hash << 5) + hash) + ((uint8_t*)s)[i];
    }

    return hash;
}

/**
 * \brief Create a new dictionary
 *
 * Return a new, empty dictionary
 *
 * \return New dictionary
 */
Dictionary* Dictionary_new(void) {
    Dictionary* dict = malloc(sizeof(Dictionary));
    if(dict == NULL) {
        return NULL;
    }

    dict->bucket_count = DICTIONARY_INITIAL_BUCKETS;
    dict->item_count = 0;
    
    dict->buckets = calloc(dict->bucket_count, sizeof(Dictionary_Item*));
    if(dict->buckets == NULL) {
        free(dict);
        return NULL;
    }

    pthread_mutex_init(&dict->lock, NULL);
    pthread_cond_init(&dict->new_item, NULL);

    return dict;
}

static void Dictionary_increaseBuckets(Dictionary* dict) {
    Dictionary_Item** new_buckets;
    Dictionary_Item* item;
    Dictionary_Item* next;
    unsigned int new_bucket_count = dict->bucket_count * 2;
    unsigned int bucket;
    
    new_buckets = calloc(new_bucket_count, sizeof(Dictionary_Item*));

    /* Move all items from the old buckets to the new buckets */
    for(int i = 0; i < dict->bucket_count; i++) {
        item = dict->buckets[i];
        while(item) {
            next = item->next;

            /* Move item */
            bucket = Dictionary_hash(item->k, item->k_size) % new_bucket_count;
            item->next = new_buckets[bucket];
            new_buckets[bucket] = item;

            /* Next item from the old bucket list */
            item = next;
        }
    }

    free(dict->buckets);
    dict->buckets = new_buckets;
    dict->bucket_count = new_bucket_count;
}

/**
 * \brief Set an element
 *
 * Set an element in the dictionary
 *
 * \param dict The dictionary to set for
 * \param k The generic key
 * \param k_size The generic key size
 * \param v The value to set
 */
void Dictionary_setData(Dictionary* dict, const void* k, size_t k_size, void* v) {
    Dictionary_Item* item_new;
    hash_t hash = Dictionary_hash(k, k_size);
    unsigned int bucket = hash % dict->bucket_count;
    Dictionary_Item* item;
    Dictionary_Item* last = NULL;

    pthread_mutex_lock(&dict->lock);

    item = dict->buckets[bucket];
    while(item) {
        if(k_size == item->k_size && memcmp(k, item->k, k_size) == 0) {
            /* Item already exists, update value */
            item->v = v;
            goto release_locks;
        }
        
        last = item;
        item = item->next;
    }

    /* Add item to list */
    item_new = Dictionary_Item_new(k, k_size, v);
    if(last == NULL) {
        dict->buckets[bucket] = item_new;
    } else {
        last->next = item_new;
    }

    dict->item_count++;

    /* Resize buckets if the load factor has been exceeded */
    if(dict->item_count / dict->bucket_count > DICTIONARY_LOAD_FACTOR &&
       dict->bucket_count < DICTIONARY_MAXIMUM_BUCKETS) {
        Dictionary_increaseBuckets(dict);
    }

 release_locks:
    pthread_cond_broadcast(&dict->new_item);
    pthread_mutex_unlock(&dict->lock);
}

/**
 * \brief Set an element
 *
 * Set an element in the dictionary
 *
 * \param dict The dictionary to set for
 * \param i The integer key
 * \param v The value to set
 */
void Dictionary_setInt(Dictionary* dict, int i, void* v) {
    Dictionary_setData(dict, &i, sizeof(int), v);
}

/**
 * \brief Set an element
 *
 * Set an element in the dictionary
 *
 * \param dict The dictionary to set for
 * \param k The string key
 * \param v The value to set
 */
void Dictionary_set(Dictionary* dict, const char* k, void* v) {
    Dictionary_setData(dict, k, strlen(k) + 1, v);
}

/**
 * \brief Retrieve an item object from a dictionary
 *
 * Get the item associated with the given key
 *
 * \param dict The dictionary to retrieve from
 * \param k The key to locate
 * \param k_size The size/length of the key
 * \return The Dictionary_Item associated with the key, or NULL if the key was
 * not found
 */
static Dictionary_Item* Dictionary_getItem(Dictionary* dict, const void* k, size_t k_size) {
    hash_t hash = Dictionary_hash(k, k_size);
    unsigned int bucket = hash % dict->bucket_count;
    Dictionary_Item* item;

    item = dict->buckets[bucket];
    while(item) {
        if(k_size == item->k_size && memcmp(k, item->k, k_size) == 0) {
            return item;
        }
        
        item = item->next;
    }

    return NULL;
}

/**
 * \brief Retrieve an element
 *
 * Retrieve an element from the dictionary
 *
 * \param dict The dictionary to retrieve from
 * \param k The generic key
 * \param k_size The generic key size
 * \return The value associated with the key or NULL if the key is not found
 */
void* Dictionary_getData(Dictionary* dict, const void* k, size_t k_size) {
    Dictionary_Item* item;

    pthread_mutex_lock(&dict->lock);
    item = Dictionary_getItem(dict, k, k_size);
    pthread_mutex_unlock(&dict->lock);

    if(item == NULL) {
        /* Invalid key */
        return NULL;
    }

    return item->v;
}

/**
 * \brief Retrieve an element
 *
 * Retrieve an element from the dictionary
 *
 * \param dict The dictionary to retrieve from
 * \param k The integer key
 * \return The value associated with the key or NULL if the key is not found
 */
void* Dictionary_getInt(Dictionary* dict, int k) {
    return Dictionary_getData(dict, &k, sizeof(int));
}

/**
 * \brief Retrieve an element
 *
 * Retrieve an element from the dictionary
 *
 * \param dict The dictionary to retrieve from
 * \param k The string key
 * \return The value associated with the key or NULL if the key is not found
 */
void* Dictionary_get(Dictionary* dict, const char* k) {
    return Dictionary_getData(dict, k, strlen(k) + 1);
}

/**
 * \brief Wait for a key to enter the dictionary
 *
 * Wait for a generic key to be in the dictionary
 *
 * \param dict The dictionary to wait on
 * \param k The generic key to wait on
 * \param k_size The generic key size
 */
void Dictionary_waitForData(Dictionary* dict, const void* k, size_t k_size) {
    pthread_mutex_lock(&dict->lock);
    while(Dictionary_getItem(dict, k, k_size) == NULL) {
        pthread_cond_wait(&dict->new_item, &dict->lock);
    }
    pthread_mutex_unlock(&dict->lock);
}

/**
 * \brief Wait for a key to enter the dictionary
 *
 * Wait for an integer key to be in the dictionary
 *
 * \param dict The dictionary to wait on
 * \param k The integer key to wait on
 */
void Dictionary_waitForInt(Dictionary* dict, int k) {
    Dictionary_waitForData(dict, &k, sizeof(int));
}

/**
 * \brief Wait for a key to enter the dictionary
 *
 * Wait for a string key to be in the dictionary
 *
 * \param dict The dictionary to wait on
 * \param k The string key to wait on
 */
void Dictionary_waitFor(Dictionary* dict, const char* k) {
    Dictionary_waitForData(dict, k, strlen(k) + 1);
}

/**
 * \brief Check if a key is in the dictionary
 *
 * Check if the generic key exists in the dictionary
 *
 * \param dict The dictionary to check
 * \param k The generic key
 * \param k_size The generic key length
 * \return true if the key exists, false otherwise
 */
bool Dictionary_existsData(Dictionary* dict, const void* k, size_t k_size) {
    /* If a Dictionary_Item does not exist for the key then return false */
    Dictionary_Item* item;
    
    pthread_mutex_lock(&dict->lock);
    item = Dictionary_getItem(dict, k, k_size);
    pthread_mutex_unlock(&dict->lock);

    if(item == NULL) {
        return false;
    }
    
    /* Key exists */
    return true;
}

/**
 * \brief Check if a key is in the dictionary
 *
 * Check if the integer key exists in the dictionary
 *
 * \param dict The dictionary to check
 * \param k The integer key
 * \return true if the key exists, false otherwise
 */
bool Dictionary_existsInt(Dictionary* dict, int k) {
    return Dictionary_existsData(dict, &k, sizeof(k));
}

/**
 * \brief Check if a key is in the dictionary
 *
 * Check if the string key exists in the dictionary
 *
 * \param dict The dictionary to check
 * \param k The string key
 * \return true if the key exists, false otherwise
 */
bool Dictionary_exists(Dictionary* dict, const char* k) {
    return Dictionary_existsData(dict, k, strlen(k) + 1);
}

/**
 * \brief Remove a dictionary element
 *
 * Remove an element from the dictionary associated with the generic key
 *
 * \param dict The dictionary to remove from
 * \param k The generic key
 * \param k_size Length of the key
 * \return -1 in the remove failed. 0 if successful
 */
int Dictionary_removeData(Dictionary* dict, const void* k, size_t k_size) {
    hash_t hash = Dictionary_hash(k, k_size);
    unsigned int bucket = hash % dict->bucket_count;
    Dictionary_Item* last = NULL;
    Dictionary_Item* item;
    int retval = -1;

    pthread_mutex_lock(&dict->lock);

    item = dict->buckets[bucket];
    while(item) {
        if(k_size == item->k_size && memcmp(k, item->k, k_size) == 0) {
            if(last) {
                last->next = item->next;
            } else {
                dict->buckets[bucket] = item->next;
            }

            Dictionary_Item_destroy(item);
            retval = 0;
            dict->item_count--;
            break;
        }
        
        last = item;
        item = item->next;
    }

    pthread_mutex_unlock(&dict->lock);
    return retval;
}

/**
 * \brief Remove a dictionary element
 *
 * Remove an element from the dictionary associated with the integer key
 *
 * \param dict The dictionary to remove from
 * \param k The integer key
 * \return -1 in the remove failed. 0 if successful
 */
int Dictionary_removeInt(Dictionary* dict, int k) {
    return Dictionary_removeData(dict, &k, sizeof(int));
}

/**
 * \brief Remove a dictionary element
 *
 * Remove an element from the dictionary associated with the string key
 *
 * \param dict The dictionary to remove from
 * \param k The string key
 * \return -1 in the remove failed. 0 if successful
 */
int Dictionary_remove(Dictionary* dict, const char* k) {
    return Dictionary_removeData(dict, k, strlen(k) + 1);
}

/**
 * \brief Get the list of keys
 *
 * Return pointers to all the keys in the dictionary. The keys should not be freed
 *
 * \param dict The dictionary to retrieve keys for
 * \return List of the keys
 */
List* Dictionary_getKeys(Dictionary* dict) {
    Dictionary_Item* item;
    List* keys = List_new();
    
    for(int i = 0; i < dict->bucket_count; i++) {
        item = dict->buckets[i];
        while(item) {
            List_append(keys, item->k);
            item = item->next;
        }
    }

    return keys;
}

/**
 * \brief Create a new dictionary item
 *
 * Create a new dictionary item
 *
 * \param k The item key
 * \param k_size The size/length of the item key
 * \param v The value to store in the item
 * \return A new item object
 */
static Dictionary_Item* Dictionary_Item_new(const void* k, size_t k_size, void* v) {
    Dictionary_Item* item = malloc(sizeof(Dictionary_Item));

    if(item == NULL) {
        return NULL;
    }

    item->k = malloc(k_size);
    item->k_size = k_size;
    item->v = v;
    item->next = NULL;

    memcpy(item->k, k, k_size);

    return item;
}

/**
 * \brief Destroy a dictionary item object
 *
 * Destroy a dictionary item object
 *
 * \param di The item to destroy
 */
static void Dictionary_Item_destroy(Dictionary_Item* item) {
    free(item->k);
    free(item);
}

/**
 * \brief Destroy a dictionary
 *
 * Free a dictionary
 *
 * \param dict The dictionary to free
 */
void Dictionary_destroy(Dictionary* dict) {
    Dictionary_Item* item;

    for(int i = 0; i < dict->bucket_count; i++) {
        while(dict->buckets[i]) {
            item = dict->buckets[i];
            dict->buckets[i] = item->next;
            Dictionary_Item_destroy(item);
        }
    }

    free(dict->buckets);
    free(dict);
}

/** \} */
