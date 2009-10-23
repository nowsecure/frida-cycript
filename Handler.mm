/* Cycript - Remote Execution Server and Disassembler
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

#include "substrate.h"
#include "cycript.hpp"

#include "Pooling.hpp"
#include "Parser.hpp"

#include "Cycript.tab.hh"

#include <Foundation/Foundation.h>
#include <apr_thread_proc.h>
#include <unistd.h>
#include <sstream>

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
    execute->data_ = CYExecute(execute->pool_, data);
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

    void Handle() { _pooled
        CYClient_ *client = [[[CYClient_ alloc] init] autorelease];

        for (;;) {
            size_t size;
            if (!CYRecvAll(socket_, &size, sizeof(size)))
                return;

            CYPool pool;
            char *data(new(pool) char[size + 1]);
            if (!CYRecvAll(socket_, data, size))
                return;
            data[size] = '\0';

            CYDriver driver("");
            cy::parser parser(driver);

            driver.data_ = data;
            driver.size_ = size;

            const char *json;
            if (parser.parse() != 0 || !driver.errors_.empty()) {
                json = NULL;
                size = _not(size_t);
            } else {
                CYContext context(driver.pool_);
                driver.program_->Replace(context);
                std::ostringstream str;
                CYOutput out(str);
                out << *driver.program_;
                std::string code(str.str());
                CYExecute_ execute = {pool, code.c_str()};
                [client performSelectorOnMainThread:@selector(execute:) withObject:[NSValue valueWithPointer:&execute] waitUntilDone:YES];
                json = execute.data_;
                size = json == NULL ? _not(size_t) : strlen(json);
            }

            if (!CYSendAll(socket_, &size, sizeof(size)))
                return;
            if (json != NULL)
                if (!CYSendAll(socket_, json, size))
                    return;
        }
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
