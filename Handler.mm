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

#include <Foundation/Foundation.h>
#include <pthread.h>
#include <unistd.h>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "cycript.hpp"

#include "JavaScript.hpp"
#include "Parser.hpp"
#include "Pooling.hpp"

#include "Cycript.tab.hh"
#include "Driver.hpp"

struct CYExecute_ {
    CYPool &pool_;
    const char * volatile data_;
};

// XXX: this is "tre lame"
@interface CYClient_ : NSObject {
}

- (void) execute:(NSValue *)value;

@end

@implementation CYClient_

- (void) execute:(NSValue *)value {
    CYExecute_ *execute(reinterpret_cast<CYExecute_ *>([value pointerValue]));
    const char *data(execute->data_);
    execute->data_ = NULL;
    execute->data_ = CYExecute(CYGetJSContext(), execute->pool_, CYUTF8String(data));
}

@end

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
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

        CYClient_ *client = [[[CYClient_ alloc] init] autorelease];

        bool dispatch;
        if (CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain())) {
            dispatch = true;
            CFRelease(mode);
        } else
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

            NSAutoreleasePool *ar = [[NSAutoreleasePool alloc] init];

            std::string code(data, size);
            CYExecute_ execute = {pool, code.c_str()};
            NSValue *value([NSValue valueWithPointer:&execute]);
            if (dispatch)
                [client performSelectorOnMainThread:@selector(execute:) withObject:value waitUntilDone:YES];
            else
                [client execute:value];

            const char *json(execute.data_);
            size = json == NULL ? _not(uint32_t) : strlen(json);

            [ar release];

            if (!CYSendAll(socket_, &size, sizeof(size)))
                return;
            if (json != NULL)
                if (!CYSendAll(socket_, json, size))
                    return;
        }

        [pool release];
    }
};

static void *OnClient(void *data) {
    CYClient *client(reinterpret_cast<CYClient *>(data));
    client->Handle();
    delete client;
    return NULL;
}

extern "C" void CYHandleClient(int socket) {
    // XXX: this leaks memory... really?
    CYPool *pool(new CYPool());
    CYClient *client(new(*pool) CYClient(socket));
    _assert(pthread_create(&client->thread_, NULL, &OnClient, client) == 0);
}

extern "C" void CYHandleServer(pid_t pid) {
    CYInitializeDynamic();

    int socket(_syscall(::socket(PF_UNIX, SOCK_STREAM, 0))); try {
        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        sprintf(address.sun_path, "/tmp/.s.cy.%u", pid);

        _syscall(connect(socket, reinterpret_cast<sockaddr *>(&address), SUN_LEN(&address)));
        CYHandleClient(socket);
    } catch (const CYException &error) {
        CYPool pool;
        fprintf(stderr, "%s\n", error.PoolCString(pool));
    }
}

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

extern "C" void CYListenServer(short port) {
    CYInitializeDynamic();

    CYServer *server(new CYServer(port));
    _assert(pthread_create(&server->thread_, NULL, &OnServer, server) == 0);
}
