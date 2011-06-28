/**
 * \file
 * \brief Standard set of easy to use synchronization primitives
 */

#include "seawolf.h"

#include <pthread.h>

/**
 * \defgroup Synch Synchronization primitives
 * \ingroup Multitasking
 * \brief Set of common synchronization structures with easy to use interfaces
 * \{
 */

/**
 * \cond Synch_Internal
 * \internal
 */

/**
 * Recursive lock attribute
 */
static pthread_mutexattr_t recursive_lock;

/**
 * \endcond Synch_Internal
 */

/**
 * \brief Initialize the synchronization module
 *
 * Initialize the synchronization module
 */
void Synch_init(void) {
    pthread_mutexattr_init(&recursive_lock);
    pthread_mutexattr_settype(&recursive_lock, PTHREAD_MUTEX_RECURSIVE);
}

/**
 * \brief Create a new lock
 *
 * Creates a new, non-recursive lock
 *
 * \return A new lock
 */
Lock* Lock_new(void) {
    Lock* lock = malloc(sizeof(Lock));
    pthread_mutex_init(lock, NULL);

    return lock;
}

/**
 * \brief Create a new recursive lock
 *
 * Create a new recursive lock. A recursive lock can be acquired multiple times
 * by the same thread.
 *
 * \return A new lock
 */
Lock* Lock_newRecursiveLock(void) {
    Lock* lock = malloc(sizeof(Lock));
    pthread_mutex_init(lock, &recursive_lock);

    return lock;
}

/**
 * \brief Acquire a lock
 *
 * Acquires a lock. If the lock is already held then this call will block until
 * the lock becomes available
 *
 * \param lock The lock to acquire
 */
void Lock_acquire(Lock* lock) {
    pthread_mutex_lock(lock);
}

/**
 * \brief Release the lock
 *
 * Release the previously acquired lock
 *
 * \param lock The lock to release
 */
void Lock_release(Lock* lock) {
    pthread_mutex_unlock(lock);
}

/**
 * \brief Free a lock
 *
 * Destroy a lock and free associated memory
 *
 * \param lock The lock to destroy
 */
void Lock_destroy(Lock* lock) {
    free(lock);
}

/**
 * \brief Create a new read/write lock
 *
 * Create a new read/write lock
 *
 * \return A new read/write lock
 */
RWLock* RWLock_new(void) {
    RWLock* lock = malloc(sizeof(RWLock));
    pthread_rwlock_init(lock, NULL);

    return lock;
}

/**
 * \brief Acquire a read lock
 *
 * Acquires a read lock. This call will block until a read lock becomes
 * available. A read lock is available once no write lock is held. Multiple
 * threads can all hold read locks
 *
 * \param lock The read lock to acquire
 */
void RWLock_acquireRead(RWLock* lock) {
    pthread_rwlock_rdlock(lock);
}

/**
 * \brief Acquire a write lock
 *
 * Acquires a write lock. This call will block until the write lock becomes
 * available. A write lock is available once no other thread holds either a
 * write or read lock on this lock
 *
 * \param lock The lock to acquire a read lock on
 */
void RWLock_acquireWrite(RWLock* lock) {
    pthread_rwlock_wrlock(lock);
}

/**
 * \brief Release the lock
 *
 * Release the previously acquired read or write lock
 *
 * \param lock The lock to release
 */
void RWLock_release(RWLock* lock) {
    pthread_rwlock_unlock(lock);
}

/**
 * \brief Free a read/write lock
 *
 * Destroy a read/write lock and free associated memory
 *
 * \param lock The lock to destroy
 */
void RWLock_destroy(RWLock* lock) {
    free(lock);
}

/**
 * \brief Create a new flag
 *
 * Create a new flag. A flag has a value (initialy false). The flag can be set
 * and cleared or the flag can be waited upon.
 *
 * \return A new flag
 */
Flag* Flag_new(void) {
    Flag* flag = malloc(sizeof(Flag));

    flag->value = false;
    pthread_mutex_init(&flag->mutex, NULL);
    pthread_cond_init(&flag->cond, NULL);

    return flag;
}

/**
 * \brief Wait for a flag to be set
 *
 * Wait for the flag to be set
 *
 * \param flag The flag to wait on
 */
void Flag_wait(Flag* flag) {
    pthread_mutex_lock(&flag->mutex);
    while(flag->value == false) {
        pthread_cond_wait(&flag->cond, &flag->mutex);
    }
    pthread_mutex_unlock(&flag->mutex);
}

/**
 * \brief Set the flag
 *
 * Set the flags value to true. This will cause all threads waiting on the flag
 * to wake up
 *
 * \param flag The flag to set
 */
 
void Flag_set(Flag* flag) {
    pthread_mutex_lock(&flag->mutex);
    flag->value = true;
    pthread_cond_broadcast(&flag->cond);
    pthread_mutex_unlock(&flag->mutex);
}

/**
 * \brief Clear the flag
 *
 * Set the value of the flag to false
 *
 * \param flag The flag to clear
 */
void Flag_clear(Flag* flag) {
    pthread_mutex_lock(&flag->mutex);
    flag->value = false;
    pthread_mutex_unlock(&flag->mutex);
}

/**
 * \brief Free a flag
 *
 * Destroy a flag and free associated memory
 *
 * \param flag The flag to destroy
 */
void Flag_destroy(Flag* flag) {
    free(flag);
}

/** \} */
