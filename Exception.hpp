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

#ifndef CYCRIPT_EXCEPTION_HPP
#define CYCRIPT_EXCEPTION_HPP

#ifdef CY_EXECUTE
#include <JavaScriptCore/JSBase.h>
#endif

#include <apr_pools.h>
#include "Standard.hpp"

struct CYException {
    virtual ~CYException() {
    }

    virtual const char *PoolCString(apr_pool_t *pool) const = 0;
#ifdef CY_EXECUTE
    virtual JSValueRef CastJSValue(JSContextRef context) const = 0;
#endif
};

void CYThrow(const char *format, ...) _noreturn;

#ifdef CY_EXECUTE
void CYThrow(JSContextRef context, JSValueRef value);
#endif

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

// XXX: fix this: _ is not safe; this is /not/ Menes ;P
#undef _assert
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
