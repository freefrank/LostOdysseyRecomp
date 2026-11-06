// pthread_rwlock_* for the Switch, wired in with --wrap. newlib's unlock always broadcasts a condition variable, and on
// libnx that is a kernel call with or without a waiter. SDL takes a read lock in nearly every call that checks an
// object, so polling one controller was about 5 ms of the game's main thread per frame in the first battle. Here the
// kernel is only called when a thread is waiting.
#include <pthread.h>
#include <switch.h>

typedef struct
{
    Mutex mutex;
    CondVar readers;
    CondVar writers;
    // Guarded by mutex.
    uint32_t readerCount : 14;
    uint32_t writerActive : 1;
    uint32_t readerWaiters : 8;
    uint32_t writerWaiters : 9;
} LiteRwLock;

_Static_assert(sizeof(LiteRwLock) <= sizeof(pthread_rwlock_t), "LiteRwLock must fit in pthread_rwlock_t");

int __wrap_pthread_rwlock_init(pthread_rwlock_t* lock, const pthread_rwlockattr_t* attributes)
{
    (void)attributes;
    *(LiteRwLock*)lock = (LiteRwLock){ 0 };
    return 0;
}

int __wrap_pthread_rwlock_destroy(pthread_rwlock_t* lock)
{
    (void)lock;
    return 0;
}

int __wrap_pthread_rwlock_rdlock(pthread_rwlock_t* handle)
{
    LiteRwLock* lock = (LiteRwLock*)handle;
    mutexLock(&lock->mutex);
    lock->readerWaiters++;
    while (lock->writerActive)
        condvarWait(&lock->readers, &lock->mutex);
    lock->readerWaiters--;
    lock->readerCount++;
    mutexUnlock(&lock->mutex);
    return 0;
}

int __wrap_pthread_rwlock_wrlock(pthread_rwlock_t* handle)
{
    LiteRwLock* lock = (LiteRwLock*)handle;
    mutexLock(&lock->mutex);
    lock->writerWaiters++;
    while (lock->writerActive || lock->readerCount)
        condvarWait(&lock->writers, &lock->mutex);
    lock->writerWaiters--;
    lock->writerActive = 1;
    mutexUnlock(&lock->mutex);
    return 0;
}

int __wrap_pthread_rwlock_unlock(pthread_rwlock_t* handle)
{
    LiteRwLock* lock = (LiteRwLock*)handle;
    mutexLock(&lock->mutex);
    if (lock->writerActive)
        lock->writerActive = 0;
    else if (lock->readerCount)
        lock->readerCount--;
    if (!lock->writerActive && !lock->readerCount)
    {
        if (lock->writerWaiters)
            condvarWakeOne(&lock->writers);
        if (lock->readerWaiters)
            condvarWakeAll(&lock->readers);
    }
    mutexUnlock(&lock->mutex);
    return 0;
}
