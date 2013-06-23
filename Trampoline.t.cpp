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

#define _PTHREAD_ATTR_T
#include <pthread_internals.h>

#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>

extern "C" {
#include <mach-o/nlist.h>
}

#include "Standard.hpp"
#include "Baton.hpp"

static void $bzero(void *data, size_t size) {
    char *bytes(reinterpret_cast<char *>(data));
    for (size_t i(0); i != size; ++i)
        bytes[i] = 0;
}

static int $strcmp(const char *lhs, const char *rhs) {
    while (*lhs == *rhs) {
        if (*lhs == '\0')
            return 0;
        ++lhs, ++rhs;
    } return *lhs < *rhs ? -1 : 1;
}

#ifdef __LP64__
typedef struct mach_header_64 mach_header_xx;
typedef struct nlist_64 nlist_xx;
typedef struct segment_command_64 segment_command_xx;

static const uint32_t LC_SEGMENT_XX = LC_SEGMENT_64;
static const uint32_t MH_MAGIC_XX = MH_MAGIC_64;
#else
typedef struct mach_header mach_header_xx;
typedef struct nlist nlist_xx;
typedef struct segment_command segment_command_xx;

static const uint32_t LC_SEGMENT_XX = LC_SEGMENT;
static const uint32_t MH_MAGIC_XX = MH_MAGIC;
#endif

#define forlc(command, mach, lc, type) \
    if (const struct load_command *load_commands = reinterpret_cast<const struct load_command *>(mach + 1)) \
        if (const struct load_command *lcp = load_commands) \
            for (uint32_t i(0); i != mach->ncmds; ++i, lcp = reinterpret_cast<const struct load_command *>(reinterpret_cast<const uint8_t *>(lcp) + lcp->cmdsize)) \
                if ( \
                    lcp->cmdsize % sizeof(long) != 0 || lcp->cmdsize <= 0 || \
                    reinterpret_cast<const uint8_t *>(lcp) + lcp->cmdsize > reinterpret_cast<const uint8_t *>(load_commands) + mach->sizeofcmds \
                ) \
                    return NULL; \
                else if (lcp->cmd != lc) \
                    continue; \
                else if (lcp->cmdsize < sizeof(type)) \
                    return NULL; \
                else if (const type *command = reinterpret_cast<const type *>(lcp))

static void *Symbol(struct dyld_all_image_infos *infos, const char *library, const char *name) {
for (uint32_t i(0); i != infos->infoArrayCount; ++i) {
    const dyld_image_info &info(infos->infoArray[i]);
    const mach_header_xx *mach(reinterpret_cast<const mach_header_xx *>(info.imageLoadAddress));
    if (mach->magic != MH_MAGIC_XX)
        continue;

    const char *path(info.imageFilePath);
    forlc (dylib, mach, LC_ID_DYLIB, dylib_command)
        path = reinterpret_cast<const char *>(dylib) + dylib->dylib.name.offset;
    if ($strcmp(path, library) != 0)
        continue;

    const struct symtab_command *stp(NULL);
    forlc (command, mach, LC_SYMTAB, struct symtab_command)
        stp = command;
    if (stp == NULL)
        continue;

    size_t slide(_not(size_t));
    const nlist_xx *symbols(NULL);
    const char *strings(NULL);

    forlc (segment, mach, LC_SEGMENT_XX, segment_command_xx) {
        if (segment->fileoff == 0)
            slide = reinterpret_cast<size_t>(mach) - segment->vmaddr;
        if (stp->symoff >= segment->fileoff && stp->symoff < segment->fileoff + segment->filesize)
            symbols = reinterpret_cast<const nlist_xx *>(stp->symoff - segment->fileoff + segment->vmaddr + slide);
        if (stp->stroff >= segment->fileoff && stp->stroff < segment->fileoff + segment->filesize)
            strings = reinterpret_cast<const char *>(stp->stroff - segment->fileoff + segment->vmaddr + slide);
    }

    if (slide == _not(size_t) || symbols == NULL || strings == NULL)
        continue;

    for (size_t i(0); i != stp->nsyms; ++i) {
        const nlist_xx *symbol(&symbols[i]);
        if (symbol->n_un.n_strx == 0 || (symbol->n_type & N_STAB) != 0)
            continue;

        const char *nambuf(strings + symbol->n_un.n_strx);
        if ($strcmp(name, nambuf) != 0)
            continue;

        uintptr_t value(symbol->n_value);
        if (value == 0)
            continue;

        value += slide;
        return reinterpret_cast<void *>(value);
    }
} return NULL; }

struct Dynamic {
    char *(*dlerror)();
    void *(*dlsym)(void *, const char *);
};

template <typename Type_>
static _finline void dlset(Dynamic *dynamic, Type_ &function, const char *name, void *handle = RTLD_DEFAULT) {
    function = reinterpret_cast<Type_>(dynamic->dlsym(handle, name));
    if (function == NULL)
        dynamic->dlerror();
}

template <typename Type_>
static _finline void cyset(Baton *baton, Type_ &function, const char *name, const char *library) {
    struct dyld_all_image_infos *infos(reinterpret_cast<struct dyld_all_image_infos *>(baton->dyld));
    function = reinterpret_cast<Type_>(Symbol(infos, library, name));
}

void *Routine(void *arg) {
    Baton *baton(reinterpret_cast<Baton *>(arg));

    Dynamic dynamic;
    cyset(baton, dynamic.dlerror, "_dlerror", "/usr/lib/system/libdyld.dylib");
    cyset(baton, dynamic.dlsym, "_dlsym", "/usr/lib/system/libdyld.dylib");

    int (*pthread_detach)(pthread_t);
    dlset(&dynamic, pthread_detach, "pthread_detach");

    pthread_t (*pthread_self)();
    dlset(&dynamic, pthread_self, "pthread_self");

    pthread_detach(pthread_self());

    void *(*dlopen)(const char *, int);
    dlset(&dynamic, dlopen, "dlopen");

    void *handle(dlopen(baton->library, RTLD_LAZY | RTLD_LOCAL));
    if (handle == NULL) {
        dynamic.dlerror();
        return NULL;
    }

    void (*CYHandleServer)(pid_t);
    dlset(&dynamic, CYHandleServer, "CYHandleServer", handle);
    if (CYHandleServer == NULL) {
        dynamic.dlerror();
        return NULL;
    }

    CYHandleServer(baton->pid);
    return NULL;
}

extern "C" void Start(Baton *baton) {
    struct _pthread self;
    $bzero(&self, sizeof(self));

    void (*$__pthread_set_self)(pthread_t);
    cyset(baton, $__pthread_set_self, "___pthread_set_self", "/usr/lib/system/libsystem_c.dylib");

    self.tsd[0] = &self;
    $__pthread_set_self(&self);

    int (*$pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    cyset(baton, $pthread_create, "_pthread_create", "/usr/lib/system/libsystem_c.dylib");

    pthread_t thread;
    $pthread_create(&thread, NULL, &Routine, baton);

    mach_port_t (*$mach_thread_self)();
    cyset(baton, $mach_thread_self, "_mach_thread_self", "/usr/lib/system/libsystem_kernel.dylib");

    kern_return_t (*$thread_terminate)(thread_act_t);
    cyset(baton, $thread_terminate, "_thread_terminate", "/usr/lib/system/libsystem_kernel.dylib");

    $thread_terminate($mach_thread_self());
}
