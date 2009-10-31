#define _PTHREAD_ATTR_T
#include <pthread_internals.h>

#include "Baton.hpp"

void *Routine(void *);

extern "C" void Start(Baton *baton) {
    struct _pthread self;
    baton->_pthread_set_self(&self);

    pthread_t thread;
    baton->pthread_create(&thread, NULL, &Routine, baton);

    void *result;
    baton->pthread_join(thread, &result);

    baton->thread_terminate(baton->mach_thread_self());
}

void *Routine(void *arg) {
    Baton *baton(reinterpret_cast<Baton *>(arg));
    void *handle(baton->dlopen(baton->library, RTLD_LAZY | RTLD_LOCAL));
    void (*HandleServer)(pid_t) = reinterpret_cast<void (*)(pid_t)>(baton->dlsym(handle, "CYHandleServer"));
    HandleServer(baton->pid);
    return arg;
}
