/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include "cycript.hpp"

#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "Driver.hpp"
#include "JavaScript.hpp"
#include "Syntax.hpp"
#include "Pooling.hpp"

struct CYExecute_ {
    CYPool &pool_;
    const char * volatile data_;
    pthread_mutex_t mutex_;
    pthread_cond_t condition_;
};

void CYPerform(void *arg) {
    CYExecute_ *execute(reinterpret_cast<CYExecute_ *>(arg));

    const char *data(execute->data_);
    execute->data_ = NULL;
    execute->data_ = CYExecute(CYGetJSContext(), execute->pool_, CYUTF8String(data));

    pthread_mutex_lock(&execute->mutex_);
    pthread_cond_signal(&execute->condition_);
    pthread_mutex_unlock(&execute->mutex_);
}

struct CYClient :
    CYData
{
    int socket_;
    pthread_t thread_;

    CYClient(int socket) :
        socket_(socket)
    {
    }

    ~CYClient() {
        _syscall(close(socket_));
    }

    void Handle() {
        bool dispatch;
#ifdef __APPLE__
        CFRunLoopRef loop(CFRunLoopGetMain());
        if (CFStringRef mode = CFRunLoopCopyCurrentMode(loop)) {
            dispatch = true;
            CFRelease(mode);
        } else
#endif
            dispatch = false;

        for (;;) {
            uint32_t size;
            if (!CYRecvAll(socket_, &size, sizeof(size)))
                return;

            CYLocalPool pool;
            char *data(new(pool) char[size + 1]);
            if (!CYRecvAll(socket_, data, size))
                return;
            data[size] = '\0';

            std::string code(data, size);
            CYExecute_ execute = {pool, code.c_str()};

            pthread_mutex_init(&execute.mutex_, NULL);
            pthread_cond_init(&execute.condition_, NULL);

            if (!dispatch)
                CYPerform(&execute);
#ifdef __APPLE__
            else {
                CFRunLoopSourceContext context;
                memset(&context, 0, sizeof(context));
                context.version = 0;
                context.info = &execute;
                context.perform = &CYPerform;

                CFRunLoopSourceRef source(CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context));

                pthread_mutex_lock(&execute.mutex_);

                CFRunLoopAddSource(loop, source, kCFRunLoopCommonModes);
                CFRunLoopSourceSignal(source);

                CFRunLoopWakeUp(loop);
                pthread_cond_wait(&execute.condition_, &execute.mutex_);
                pthread_mutex_unlock(&execute.mutex_);

                CFRunLoopRemoveSource(loop, source, kCFRunLoopCommonModes);
                CFRelease(source);
            }
#endif

            pthread_cond_destroy(&execute.condition_);
            pthread_mutex_destroy(&execute.mutex_);

            const char *json(execute.data_);
            size = json == NULL ? _not(uint32_t) : strlen(json);

            if (!CYSendAll(socket_, &size, sizeof(size)))
                return;
            if (json != NULL)
                if (!CYSendAll(socket_, json, size))
                    return;
        }
    }
};

static void *OnClient(void *data) {
    CYClient *client(reinterpret_cast<CYClient *>(data));
    client->Handle();
    delete client;
    return NULL;
}

void CYHandleClient(int socket) {
    // XXX: this leaks memory... really?
    CYPool *pool(new CYPool());
    CYClient *client(new(*pool) CYClient(socket));
    _assert(pthread_create(&client->thread_, NULL, &OnClient, client) == 0);
}

static void CYHandleSocket(const char *path) {
    int socket(_syscall(::socket(PF_UNIX, SOCK_STREAM, 0)));

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);

    _syscall(connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)));

    CYInitializeDynamic();
    CYHandleClient(socket);
}

_extern void CYHandleServer(pid_t pid) { try {
    char path[1024];
    sprintf(path, "/tmp/.s.cy.%u", pid);
    CYHandleSocket(path);
} catch (const CYException &error) {
    CYPool pool;
    fprintf(stderr, "%s\n", error.PoolCString(pool));
} }

_extern char *MSmain0(int argc, char *argv[]) { try {
    _assert(argc == 2);
    CYHandleSocket(argv[1]);

    static void *handle(NULL);
    if (handle == NULL) {
        Dl_info info;
        _assert(dladdr(reinterpret_cast<void *>(&MSmain0), &info) != 0);
#ifdef __ANDROID__
        handle = dlopen(info.dli_fname, 0);
#else
        handle = dlopen(info.dli_fname, RTLD_NOLOAD);
#endif
    }

    return NULL;
} catch (const CYException &error) {
    CYPool pool;
    return strdup(error.PoolCString(pool));
} }

struct CYServer {
    pthread_t thread_;
    uint16_t port_;
    int socket_;

    CYServer(uint16_t port) :
        port_(port),
        socket_(-1)
    {
    }

    ~CYServer() {
        if (socket_ != -1)
            _syscall(close(socket_));
    }

    void Listen() {
        socket_ = _syscall(::socket(PF_INET, SOCK_STREAM, 0)); try {
            int value;
            _syscall(::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &(value = 1), sizeof(value)));

            sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port_);
            _syscall(::bind(socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address)));

            _syscall(::listen(socket_, -1));

            for (;;) {
                socklen_t length(sizeof(address));
                int socket(_syscall(::accept(socket_, reinterpret_cast<sockaddr *>(&address), &length)));
                CYHandleClient(socket);
            }
        } catch (const CYException &error) {
            CYPool pool;
            fprintf(stderr, "%s\n", error.PoolCString(pool));
        }
    }
};

static void *OnServer(void *data) {
    CYServer *server(reinterpret_cast<CYServer *>(data));
    server->Listen();
    delete server;
    return NULL;
}

_extern void CYListenServer(short port) {
    CYInitializeDynamic();

    CYServer *server(new CYServer(port));
    _assert(pthread_create(&server->thread_, NULL, &OnServer, server) == 0);
}
