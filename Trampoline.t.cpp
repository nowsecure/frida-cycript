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

static void *Routine(void *arg) {
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

static void *Thread(void *arg) {
    Baton *baton(reinterpret_cast<Baton *>(arg));

    int (*pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    dlset(baton, pthread_create, "pthread_create");

    pthread_t thread;
    pthread_create(&thread, NULL, &Routine, baton);

    int (*pthread_join)(pthread_t, void **);
    dlset(baton, pthread_join, "pthread_join");

    void *result;
    pthread_join(thread, &result);

    return NULL;
}

extern "C" void Start(Baton *baton) {
    struct _pthread self;
    baton->_pthread_start(&self, NULL, &Thread, baton, 8 * 1024, 0);
}
