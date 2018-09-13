#include "os.h"
#include "common/common.h"
#include <climits>
#include <pthread.h>

using namespace std;

// 16 Megabytes
constexpr int REQUIRED_STACK_SIZE = 16 * 1024 * 1024;

void *Joinable::trampoline(void *ptr) {
    ENFORCE(ptr != nullptr);
    Joinable &self = *static_cast<Joinable *>(ptr);
    setCurrentThreadName(self.originalThreadName);
    self.realFunction();
    return ptr;
}

unique_ptr<Joinable> runInAThread(const string &threadName, function<void()> function) {
    // AFAIK this should all be:
    //    - defined behaviour
    //    - available on all posix systems

    unique_ptr<Joinable> res = make_unique<Joinable>();
    res->realFunction = move(function);
    res->originalThreadName = threadName;

    Joinable *joinablePTR = res.get();

    pthread_t &thread = res->handle;
    int err = 0;
    pthread_attr_t &attr = res->attr;
    size_t stackSize = 0;

    /*  Initialize the attribute */
    err = pthread_attr_init(&attr);
    if (err != 0) {
        sorbet::Error::raise("Failed to set create pthread_attr_t");
    };

    /* Get the default value */
    err = pthread_attr_getstacksize(&attr, &stackSize);
    if (err != 0) {
        sorbet::Error::raise("Failed to get stack size");
    }

    // This should be the default, but just to play it safe.

    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (err != 0) {
        sorbet::Error::raise("Failed make thread joinable");
    }

    // Mac default is too low
    if (stackSize < REQUIRED_STACK_SIZE) {
        err = pthread_attr_setstacksize(&attr, REQUIRED_STACK_SIZE);
        if (err != 0) {
            sorbet::Error::raise("Failed to set stack size");
        };
    }

    /*  Create the thread with our attribute */
    err = pthread_create(&thread, &attr, &Joinable::trampoline, joinablePTR);
    if (err != 0) {
        sorbet::Error::raise("Failed create thread");
    }
    return res;
}

bool stopInDebugger() {
    if (amIBeingDebugged()) {
        __asm__("int $3");
        return true;
    }
    return false;
}

bool setCurrentThreadName(const std::string &name) {
    const size_t maxLen = 16 - 1; // Pthreads limits it to 16 bytes including trailing '\0'
    auto truncatedName = name.substr(0, maxLen);
    auto retCode =
#ifdef __APPLE__
        ::pthread_setname_np(truncatedName.c_str());
#else
        ::pthread_setname_np(::pthread_self(), truncatedName.c_str());
#endif
    return retCode == 0;
}