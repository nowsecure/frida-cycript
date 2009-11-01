/* Cycript - Error.hppution Server and Disassembler
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

#ifndef CYCRIPT_EXCEPTION_HPP
#define CYCRIPT_EXCEPTION_HPP

#include <JavaScriptCore/JSBase.h>

#include <apr_pools.h>
#include "Standard.hpp"

struct CYException {
    virtual ~CYException() {
    }

    virtual const char *PoolCString(apr_pool_t *pool) const = 0;
    virtual JSValueRef CastJSValue(JSContextRef context) const = 0;
};

void CYThrow(const char *format, ...) _noreturn;
void CYThrow(JSContextRef context, JSValueRef value);

#define CYTry \
    try
#define CYCatch \
    catch (const CYException &error) { \
        *exception = error.CastJSValue(context); \
        return NULL; \
    } catch (...) { \
        *exception = CYCastJSValue(context, "catch(...)"); \
        return NULL; \
    }

#define _assert(test, args...) do { \
    if (!(test)) \
        CYThrow("*** _assert(%s):%s(%u):%s [errno=%d]", #test, __FILE__, __LINE__, __FUNCTION__, errno); \
} while (false)

#define _trace() do { \
    fprintf(stderr, "_trace():%u\n", __LINE__); \
} while (false)

#define _syscall(expr) ({ \
    __typeof__(expr) _value; \
    do if ((long) (_value = (expr)) != -1) \
        break; \
    else switch (errno) { \
        case EINTR: \
            continue; \
        default: \
            _assert(false); \
    } while (true); \
    _value; \
})

#define _aprcall(expr) \
    do { \
        apr_status_t _aprstatus((expr)); \
        _assert(_aprstatus == APR_SUCCESS); \
    } while (false)

#define _krncall(expr) \
    do { \
        kern_return_t _krnstatus((expr)); \
        _assert(_krnstatus == KERN_SUCCESS); \
    } while (false)

#define _sqlcall(expr) ({ \
    __typeof__(expr) _value = (expr); \
    if (_value != 0 && (_value < 100 || _value >= 200)) \
        _assert(false, "_sqlcall(%u:%s): %s\n", _value, #expr, sqlite3_errmsg(database_)); \
    _value; \
})

#endif/*CYCRIPT_ERROR_HPP*/
