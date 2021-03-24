#ifndef YAD_UTILS_SINGLETON_H
#define YAD_UTILS_SINGLETON_H

#include <stdint.h>
#include <sys/types.h>
#include "Mutex.h"

namespace YAD {
// ---------------------------------------------------------------------------

template <typename TYPE>
class Singleton
{
public:
    static TYPE& getInstance() {
        Mutex::Autolock _l(sLock);
        TYPE* instance = sInstance;
        if (instance == 0) {
            instance = new TYPE();
            sInstance = instance;
        }
        return *instance;
    }

    static bool hasInstance() {
        Mutex::Autolock _l(sLock);
        return sInstance != 0;
    }
    
protected:
    ~Singleton() { };
    Singleton() { };

private:
    Singleton(const Singleton&);
    Singleton& operator = (const Singleton&);
    static Mutex sLock;
    static TYPE* sInstance;
};

/*
 * use YAD_SINGLETON_STATIC_INSTANCE(TYPE) in your implementation file
 * (eg: <TYPE>.cpp) to create the static instance of Singleton<>'s attributes,
 * and avoid to have a copy of them in each compilation units Singleton<TYPE>
 * is used.
 * NOTE: we use a version of Mutex ctor that takes a parameter, because
 * for some unknown reason using the default ctor doesn't emit the variable!
 */

#define YAD_SINGLETON_STATIC_INSTANCE(TYPE)                 \
    template<> ::YAD::Mutex  \
        (::YAD::Singleton< TYPE >::sLock)(::YAD::Mutex::PRIVATE);  \
    template<> TYPE* ::YAD::Singleton< TYPE >::sInstance(0);  \
    template class ::YAD::Singleton< TYPE >;

// ---------------------------------------------------------------------------
}; // namespace YAD

#endif // YAD_UTILS_SINGLETON_H

