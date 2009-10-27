/* Cycript - Remove Execution Server and Disassembler
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

#include <substrate.h>

#include "cycript.hpp"
#include "Pooling.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <apr_thread_proc.h>

#include <fcntl.h>
#include <unistd.h>

#import <CoreFoundation/CFLogUtilities.h>

void CYThrow(const char *format, ...) {
    CYPool pool;

    va_list args;
    va_start (args, format);
    const char *message(apr_pvsprintf(pool, format, args));
    va_end (args);

    fprintf(stderr, "%s\n", message);
    throw std::string(message);
}

struct CYServer :
    CYData
{
    int socket_;
};

apr_status_t CYPoolDLClose_(void *data) {
    // XXX: this is an interesting idea
    /* void *handle(reinterpret_cast<void *>(data));
    dlclose(handle); */
    return APR_SUCCESS;
}

static void * APR_THREAD_FUNC Cyrver(apr_thread_t *thread, void *data) {
    CYServer *server(reinterpret_cast<CYServer *>(data));

    for (;;) {
        int socket(_syscall(accept(server->socket_, NULL, NULL)));

        if (void *handle = dlopen("/usr/lib/libcycript.dylib", RTLD_LAZY | RTLD_LOCAL)) {
            apr_pool_t *pool;
            _aprcall(apr_pool_create(&pool, NULL));

            apr_pool_cleanup_register(pool, handle, &CYPoolDLClose_, &apr_pool_cleanup_null);

            if (void (*CYHandleClient_)(apr_pool_t *, int) = reinterpret_cast<void (*)(apr_pool_t *, int)>(dlsym(handle, "CYHandleClient")))
                (*CYHandleClient_)(pool, socket);
            else
                apr_pool_destroy(pool);
        } else
            CFLog(kCFLogLevelError, CFSTR("CY:Error: cannot load: %s"), dlerror());
    }

    delete server;
    return NULL;
}

static void Unlink() {
    pid_t pid(getpid());
    char path[104];
    sprintf(path, "/tmp/.s.cy.%u", pid);
    unlink(path);
}

MSInitialize {
    _aprcall(apr_initialize());

    CYServer *server(new CYServer());
    server->socket_ = _syscall(socket(PF_UNIX, SOCK_STREAM, 0));

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;

    pid_t pid(getpid());
    sprintf(address.sun_path, "/tmp/.s.cy.%u", pid);

    try {
        _syscall(bind(server->socket_, reinterpret_cast<sockaddr *>(&address), SUN_LEN(&address)));
        atexit(&Unlink);
        _syscall(listen(server->socket_, 0));

        apr_threadattr_t *attr;
        _aprcall(apr_threadattr_create(&attr, server->pool_));

        apr_thread_t *thread;
        _aprcall(apr_thread_create(&thread, attr, &Cyrver, server, server->pool_));
    } catch (...) {
        CFLog(kCFLogLevelError, CFSTR("CY:Error: cannot bind unix domain socket"));
    }
}
