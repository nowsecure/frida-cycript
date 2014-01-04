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

#include <TargetConditionals.h>
#if defined(__arm__) || defined(__arm64__)
#undef TARGET_IPHONE_SIMULATOR
#define TARGET_IPHONE_SIMULATOR 1
#endif
#define _PTHREAD_ATTR_T
#include <pthread_internals.h>
#if defined(__arm__) || defined(__arm64__)
#undef TARGET_IPHONE_SIMULATOR
#endif

#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

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

static void $strlcpy(char *dst, const char *src, size_t size) {
    if (size == 0)
        return;
    size_t i(0);
    while (i != size - 1) {
        char value(src[i]);
        if (value == '\0')
            break;
        dst[i++] = value;
    } dst[i] = '\0';
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

static const mach_header_xx *Library(struct dyld_all_image_infos *infos, const char *name) {
    for (uint32_t i(0); i != infos->infoArrayCount; ++i) {
        const dyld_image_info &info(infos->infoArray[i]);
        const mach_header_xx *mach(reinterpret_cast<const mach_header_xx *>(info.imageLoadAddress));
        if (mach->magic != MH_MAGIC_XX)
            continue;

        const char *path(info.imageFilePath);
        forlc (dylib, mach, LC_ID_DYLIB, dylib_command)
            path = reinterpret_cast<const char *>(dylib) + dylib->dylib.name.offset;
        if ($strcmp(path, name) != 0)
            continue;

        return mach;
    }

    return NULL;
}

static void *Symbol(const mach_header_xx *mach, const char *name) {
    const struct symtab_command *stp(NULL);
    forlc (command, mach, LC_SYMTAB, struct symtab_command)
        stp = command;
    if (stp == NULL)
        return NULL;

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
        return NULL;

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

#ifdef __arm__
        if ((symbol->n_desc & N_ARM_THUMB_DEF) != 0)
            value |= 0x00000001;
#endif

        value += slide;
        return reinterpret_cast<void *>(value);
    }

    return NULL;
}

template <typename Type_>
static _finline void cyset(Type_ &function, const char *name, const mach_header_xx *mach) {
    function = reinterpret_cast<Type_>(Symbol(mach, name));
}

static _finline const mach_header_xx *Library(Baton *baton, const char *name) {
    struct dyld_all_image_infos *infos(reinterpret_cast<struct dyld_all_image_infos *>(baton->dyld));
    return Library(infos, name);
}

void *Routine(void *arg) {
    Baton *baton(reinterpret_cast<Baton *>(arg));

    const mach_header_xx *dyld(NULL);
    if (dyld == NULL)
        dyld = Library(baton, "/usr/lib/system/libdyld.dylib");
    if (dyld == NULL)
        dyld = Library(baton, "/usr/lib/libSystem.B.dylib");

    char *(*$dlerror)();
    cyset($dlerror, "_dlerror", dyld);

    void *(*$dlopen)(const char *, int);
    cyset($dlopen, "_dlopen", dyld);

    void *handle($dlopen(baton->library, RTLD_LAZY | RTLD_LOCAL));
    if (handle == NULL) {
        $strlcpy(baton->error, $dlerror(), sizeof(baton->error));
        return NULL;
    }

    void *(*$dlsym)(void *, const char *);
    cyset($dlsym, "_dlsym", dyld);

    void (*CYHandleServer)(pid_t);
    CYHandleServer = reinterpret_cast<void (*)(pid_t)>($dlsym(handle, "CYHandleServer"));
    if (CYHandleServer == NULL) {
        $strlcpy(baton->error, $dlerror(), sizeof(baton->error));
        return NULL;
    }

    CYHandleServer(baton->pid);
    return NULL;
}

extern "C" void Start(Baton *baton) {
    struct _pthread self;
    $bzero(&self, sizeof(self));

    const mach_header_xx *pthread(NULL);
    if (pthread == NULL)
        pthread = Library(baton, "/usr/lib/system/libsystem_pthread.dylib");
    if (pthread == NULL)
        pthread = Library(baton, "/usr/lib/system/libsystem_c.dylib");
    if (pthread == NULL)
        pthread = Library(baton, "/usr/lib/libSystem.B.dylib");

    void (*$__pthread_set_self)(pthread_t);
    cyset($__pthread_set_self, "___pthread_set_self", pthread);

    self.tsd[0] = &self;
    $__pthread_set_self(&self);

    int (*$pthread_attr_init)(pthread_attr_t *);
    cyset($pthread_attr_init, "_pthread_attr_init", pthread);

#if 0
    pthread_attr_t attr;
    $pthread_attr_init(&attr);

    int (*$pthread_attr_setdetachstate)(pthread_attr_t *, int);
    cyset($pthread_attr_setdetachstate, "_pthread_attr_setdetachstate", pthread);

    $pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#endif

    int (*$pthread_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    cyset($pthread_create, "_pthread_create", pthread);

    pthread_t thread;
    $pthread_create(&thread, NULL, &Routine, baton);

#if 0
    int (*$pthread_attr_destroy)(pthread_attr_t *);
    cyset($pthread_attr_destroy, "_pthread_attr_destroy", pthread);

    $pthread_attr_destroy(&attr);
#endif

#if defined(__arm__) || defined(__arm64__)
    uintptr_t tpid;
#if defined(__arm__)
    __asm__ ("mrc p15, 0, %0, c13, c0, 3\n" : "=r" (tpid));
#elif defined(__arm64__)
    __asm__ ("mrs %0, tpidrro_el0\n" : "=r" (tpid));
#else
#error XXX
#endif

    void **tsd;
    tsd = reinterpret_cast<void **>(tpid & ~3);
    if (tsd != NULL)
        tsd[0] = &self;
#else
    _pthread_setspecific_direct(0, &self);
#endif

    int (*$pthread_join)(pthread_t, void **);
    cyset($pthread_join, "_pthread_join", pthread);

    void *status;
    $pthread_join(thread, &status);

    const mach_header_xx *kernel(NULL);
    if (kernel == NULL)
        kernel = Library(baton, "/usr/lib/system/libsystem_kernel.dylib");
    if (kernel == NULL)
        kernel = Library(baton, "/usr/lib/libSystem.B.dylib");

    mach_port_t (*$mach_thread_self)();
    cyset($mach_thread_self, "_mach_thread_self", kernel);

    kern_return_t (*$thread_terminate)(thread_act_t);
    cyset($thread_terminate, "_thread_terminate", kernel);

    $thread_terminate($mach_thread_self());
}
