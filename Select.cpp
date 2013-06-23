/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2013  Jay Freeman (saurik)
*/

/* GNU General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include <cstring>

#include <stdint.h>

#include <dlfcn.h>
#include <syslog.h>
#include <unistd.h>

#include <mach-o/dyld.h>

extern "C" void CYHandleServer(pid_t pid) {
    Dl_info addr;
    if (dladdr(reinterpret_cast<void *>(&CYHandleServer), &addr) == 0)
        return;

    const char *fname(addr.dli_fname);
    size_t length(strlen(fname));

    const char *ext(strrchr(fname, '.'));
    size_t prefix(ext - fname);

    const char *addition;
#ifdef __APPLE__
    // XXX: THIS IS HORRIBLE OMG I NEED TO FIX THIS ASAP
    bool simulator(false);
    for (uint32_t i(0), e(_dyld_image_count()); i != e; ++i) {
        if (strstr(_dyld_get_image_name(i), "/SDKs/iPhoneSimulator") != NULL)
            simulator = true;
    }
    if (simulator)
        addition = "-sim";
    else
#endif
    // someone threw a fit about dangling #endif + else
    // the idea that this bothers someone gives me glee
    addition = "-sys";

    char library[length + 5];
    memcpy(library, fname, prefix);
    memcpy(library + prefix, addition, 4);
    memcpy(library + prefix + 4, fname + prefix, length - prefix);
    library[length + 4] = '\0';

    void *handle(dlopen(library, RTLD_LOCAL | RTLD_LAZY));
    if (handle == NULL) {
        syslog(LOG_ERR, "dlopen() -> %s", dlerror());
        return;
    }

    void *symbol(dlsym(handle, "CYHandleServer"));
    if (symbol == NULL)
        return;

    reinterpret_cast<void (*)(pid_t)>(symbol)(pid);
}
