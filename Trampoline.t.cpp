/* Cycript - Inlining/Optimizing JavaScript Compiler
 * Copyright (C) 2009  Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
 *        Redistribution and use in source and binary
 * forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation
 *    and/or other materials provided with the
 *    distribution.
 * 3. The name of the author may not be used to endorse
 *    or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#define _PTHREAD_ATTR_T
#include <pthread_internals.h>

#include "Standard.hpp"
#include "Baton.hpp"

template <typename Type_>
static _finline void dlset(Baton *baton, Type_ &function, const char *name, void *handle = RTLD_DEFAULT) {
    function = reinterpret_cast<Type_>(baton->dlsym(handle, name));
    if (function == NULL)
        baton->dlerror();
}

void *Routine(void *arg) {
    Baton *baton(reinterpret_cast<Baton *>(arg));

    void *(*dlopen)(const char *, int);
    dlset(baton, dlopen, "dlopen");

    void *handle(dlopen(baton->library, RTLD_LAZY | RTLD_LOCAL));
    if (handle == NULL) {
        baton->dlerror();
        return NULL;
    }

    void (*CYHandleServer)(pid_t);
    dlset(baton, CYHandleServer, "CYHandleServer", handle);

    CYHandleServer(baton->pid);

    return NULL;
}

static void $bzero(void *data, size_t size) {
    char *bytes(reinterpret_cast<char *>(data));
    for (size_t i(0); i != size; ++i)
        bytes[i] = 0;
}

extern "C" void Start(Baton *baton) {
    struct _pthread self;
    $bzero(&self, sizeof(self));

    // this code comes from _pthread_set_self
    self.tsd[0] = &self;
    baton->__pthread_set_self(&self);

    int (*pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    dlset(baton, pthread_create, "pthread_create");

    pthread_t thread;
    baton->pthread_create(&thread, NULL, &Routine, baton);

    int (*pthread_join)(pthread_t, void **);
    dlset(baton, pthread_join, "pthread_join");

    void *result;
    baton->pthread_join(thread, &result);

    mach_port_t (*mach_thread_self)();
    dlset(baton, mach_thread_self, "mach_thread_self");

    kern_return_t (*thread_terminate)(thread_act_t);
    dlset(baton, thread_terminate, "thread_terminate");

    baton->thread_terminate(baton->mach_thread_self());
}
