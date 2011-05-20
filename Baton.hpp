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

#include <dlfcn.h>
#include <mach/mach.h>
#include <sys/types.h>

struct Baton {
    void (*__pthread_set_self)(pthread_t);

    int (*pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    int (*pthread_join)(pthread_t, void **);

    void *(*dlopen)(const char *, int);
    char *(*dlerror)();
    void *(*dlsym)(void *, const char *);

    mach_port_t (*mach_thread_self)();
    kern_return_t (*thread_terminate)(thread_act_t);

    pid_t pid;
    char library[];
};
