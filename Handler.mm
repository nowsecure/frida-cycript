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

#include "cycript.hpp"
#include "JavaScript.hpp"

#include "Pooling.hpp"
#include "Parser.hpp"

#include "Cycript.tab.hh"

#include <Foundation/Foundation.h>
#include <apr_thread_proc.h>
#include <unistd.h>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

struct CYExecute_ {
    apr_pool_t *pool_;
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
    execute->data_ = CYExecute(execute->pool_, CYUTF8String(data));
}

@end

struct CYClient :
    CYData
{
    int socket_;
    apr_thread_t *thread_;

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
            size_t size;
            if (!CYRecvAll(socket_, &size, sizeof(size)))
                return;

            CYLocalPool pool;
            char *data(new(pool) char[size + 1]);
            if (!CYRecvAll(socket_, data, size))
                return;
            data[size] = '\0';

            CYDriver driver;
            cy::parser parser(driver);

            driver.data_ = data;
            driver.size_ = size;

            const char *json;
            if (parser.parse() != 0 || !driver.errors_.empty()) {
                json = NULL;
                size = _not(size_t);
            } else {
                NSAutoreleasePool *ar = [[NSAutoreleasePool alloc] init];

                CYOptions options;
                CYContext context(options);
                driver.program_->Replace(context);
                std::ostringstream str;
                CYOutput out(str, options);
                out << *driver.program_;
                std::string code(str.str());
                CYExecute_ execute = {pool, code.c_str()};
                NSValue *value([NSValue valueWithPointer:&execute]);
                if (dispatch)
                    [client performSelectorOnMainThread:@selector(execute:) withObject:value waitUntilDone:YES];
                else
                    [client execute:value];
                json = execute.data_;
                size = json == NULL ? _not(size_t) : strlen(json);

                [ar release];
            }

            if (!CYSendAll(socket_, &size, sizeof(size)))
                return;
            if (json != NULL)
                if (!CYSendAll(socket_, json, size))
                    return;
        }

        [pool release];
    }
};

static void * APR_THREAD_FUNC OnClient(apr_thread_t *thread, void *data) {
    CYClient *client(reinterpret_cast<CYClient *>(data));
    client->Handle();
    delete client;
    return NULL;
}

extern "C" void CYHandleClient(apr_pool_t *pool, int socket) {
    CYClient *client(new(pool) CYClient(socket));
    apr_threadattr_t *attr;
    _aprcall(apr_threadattr_create(&attr, client->pool_));
    _aprcall(apr_thread_create(&client->thread_, attr, &OnClient, client, client->pool_));
}

extern "C" void CYHandleServer(pid_t pid) {
    CYInitializeDynamic();

    int socket(_syscall(::socket(PF_UNIX, SOCK_STREAM, 0))); try {
        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        sprintf(address.sun_path, "/tmp/.s.cy.%u", pid);

        _syscall(connect(socket, reinterpret_cast<sockaddr *>(&address), SUN_LEN(&address)));

        apr_pool_t *pool;
        apr_pool_create(&pool, NULL);

        CYHandleClient(pool, socket);
    } catch (const CYException &error) {
        CYPool pool;
        fprintf(stderr, "%s\n", error.PoolCString(pool));
    }
}
