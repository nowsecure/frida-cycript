#ifndef CYCRIPT_EXCEPTION_HPP
#define CYCRIPT_EXCEPTION_HPP

#include <JavaScriptCore/JSBase.h>

#include "Standard.hpp"

struct CYException {
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

#define _sqlcall(expr) ({ \
    __typeof__(expr) _value = (expr); \
    if (_value != 0 && (_value < 100 || _value >= 200)) \
        _assert(false, "_sqlcall(%u:%s): %s\n", _value, #expr, sqlite3_errmsg(database_)); \
    _value; \
})

#endif/*CYCRIPT_ERROR_HPP*/
