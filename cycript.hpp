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

#ifndef CYCRIPT_HPP
#define CYCRIPT_HPP

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#endif

#include <JavaScriptCore/JavaScript.h>

#include <apr_pools.h>
#include <ffi.h>

#include <sig/types.hpp>

bool CYRecvAll_(int socket, uint8_t *data, size_t size);
bool CYSendAll_(int socket, const uint8_t *data, size_t size);

extern "C" void CYHandleClient(apr_pool_t *pool, int socket);

template <typename Type_>
bool CYRecvAll(int socket, Type_ *data, size_t size) {
    return CYRecvAll_(socket, reinterpret_cast<uint8_t *>(data), size);
}

template <typename Type_>
bool CYSendAll(int socket, const Type_ *data, size_t size) {
    return CYSendAll_(socket, reinterpret_cast<const uint8_t *>(data), size);
}

JSGlobalContextRef CYGetJSContext();
JSObjectRef CYGetGlobalObject(JSContextRef context);
const char *CYExecute(apr_pool_t *pool, const char *code);

void CYSetArgs(int argc, const char *argv[]);

#endif/*CYCRIPT_HPP*/
