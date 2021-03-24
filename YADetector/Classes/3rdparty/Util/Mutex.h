#ifndef YAD_UTILS_MUTEX_H
#define YAD_UTILS_MUTEX_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#if !defined(_WIN32)
# include <pthread.h>
#endif

// ---------------------------------------------------------------------------
namespace YAD {
// ---------------------------------------------------------------------------

/*
 * NOTE: This class is for code that builds on Win32.  Its usage is
 * deprecated for code which doesn't build for Win32.  New code which
 * doesn't build for Win32 should use std::mutex and std::lock_guard instead.
 *
 * Simple mutex class.  The implementation is system-dependent.
 *
 * The mutex must be unlocked by the thread that locked it.  They are not
 * recursive, i.e. the same thread can't lock it multiple times.
 */
class Mutex {
public:
    enum {
        PRIVATE = 0,
        SHARED = 1
    };

                Mutex();
                Mutex(const char* name);
                Mutex(int type, const char* name = NULL);
                ~Mutex();

    // lock or unlock the mutex
    int         lock();
    void        unlock();

    // lock if possible; returns 0 on success, error otherwise
    int         tryLock();

    // Manages the mutex automatically. It'll be locked when Autolock is
    // constructed and released when Autolock goes out of scope.
    class Autolock {
    public:
        inline Autolock(Mutex& mutex) : mLock(mutex)  { mLock.lock(); }
        inline Autolock(Mutex* mutex) : mLock(*mutex) { mLock.lock(); }
        inline ~Autolock() { mLock.unlock(); }
    private:
        Mutex& mLock;
    };

private:
    friend class Condition;

    // A mutex cannot be copied
                Mutex(const Mutex&);
    Mutex&      operator = (const Mutex&);

#if !defined(_WIN32)
    pthread_mutex_t mMutex;
#else
    void    _init();
    void*   mState;
#endif
};

// ---------------------------------------------------------------------------

#if !defined(_WIN32)

inline Mutex::Mutex() {
    pthread_mutex_init(&mMutex, NULL);
}
inline Mutex::Mutex(__attribute__((unused)) const char* name) {
    pthread_mutex_init(&mMutex, NULL);
}
inline Mutex::Mutex(int type, __attribute__((unused)) const char* name) {
    if (type == SHARED) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mMutex, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(&mMutex, NULL);
    }
}
inline Mutex::~Mutex() {
    pthread_mutex_destroy(&mMutex);
}
inline int Mutex::lock() {
    return -pthread_mutex_lock(&mMutex);
}
inline void Mutex::unlock() {
    pthread_mutex_unlock(&mMutex);
}
inline int Mutex::tryLock() {
    return -pthread_mutex_trylock(&mMutex);
}

#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------

/*
 * Automatic mutex.  Declare one of these at the top of a function.
 * When the function returns, it will go out of scope, and release the
 * mutex.
 */

typedef Mutex::Autolock AutoMutex;

// ---------------------------------------------------------------------------
}; // namespace YAD
// ---------------------------------------------------------------------------

#endif // YAD_UTILS_MUTEX_H
