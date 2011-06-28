/**
 * \file
 */

#ifndef __SEAWOLF_SYNCH_INCLUDE_H
#define __SEAWOLF_SYNCH_INCLUDE_H

#include "pthread.h"

/**
 * \addtogroup Synch
 * \{
 */

/**
 * \brief Lock
 *
 * A locking primitive
 */
typedef pthread_mutex_t Lock;

/**
 * \brief Read/write lock
 *
 * A locking primitive optimized for reading/writing
 */
typedef pthread_rwlock_t RWLock;

/**
 * \brief Condition flag
 *
 * A flag has a value. A thread can set and clear a Flag, or perform a blocking
 * wait for it to be set
 */
typedef struct {
    /** 
     * The underlying value of the flag
     */
    bool value;

    /**
     * Locks access to the flag
     */
    pthread_mutex_t mutex;

    /**
     * Signals the flag condition
     */
    pthread_cond_t cond;
} Flag;

/** \} */

void Synch_init(void);

Lock* Lock_new(void);
Lock* Lock_newRecursiveLock(void);
void Lock_acquire(Lock* lock);
void Lock_release(Lock* lock);
void Lock_destroy(Lock* lock);

RWLock* RWLock_new(void);
void RWLock_acquireRead(RWLock* lock);
void RWLock_acquireWrite(RWLock* lock);
void RWLock_release(RWLock* lock);
void RWLock_destroy(RWLock* lock);

Flag* Flag_new(void);
void Flag_wait(Flag* flag);
void Flag_set(Flag* flag);
void Flag_clear(Flag* flag);
void Flag_destroy(Flag* flag);

#endif // #ifndef __SEAWOLF_SYNCH_INCLUDE_H
