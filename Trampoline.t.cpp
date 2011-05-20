/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2010  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
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

#define Framework(framework) \
    "/System/Library/Frameworks/" #framework ".framework/" #framework

void *Routine(void *arg) {
    Baton *baton(reinterpret_cast<Baton *>(arg));

    void *(*dlopen)(const char *, int);
    dlset(baton, dlopen, "dlopen");

    if (baton->dlsym(RTLD_DEFAULT, "JSEvaluateScript") == NULL)
        dlopen(Framework(JavaScriptCore), RTLD_GLOBAL | RTLD_LAZY);

    void *(*objc_getClass)(const char *);
    dlset(baton, objc_getClass, "objc_getClass");

    if (objc_getClass("WebUndefined") == NULL)
        dlopen(Framework(WebKit), RTLD_GLOBAL | RTLD_LAZY);

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
