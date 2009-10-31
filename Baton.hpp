#include <dlfcn.h>
#include <mach/mach.h>

struct Baton {
    void (*_pthread_set_self)(pthread_t);
    int (*pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    int (*pthread_detach)(pthread_t);
    void *(*dlopen)(const char *, int);
    mach_port_t (*mach_thread_self)();
    kern_return_t (*thread_terminate)(thread_act_t);
    char library[];
};
