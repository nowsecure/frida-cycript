#ifndef CYCRIPT_ERROR_HPP
#define CYCRIPT_ERROR_HPP

#include "Pooling.hpp"
#include "Exception.hpp"

struct CYJSError :
    CYException
{
    JSContextRef context_;
    JSValueRef value_;

    CYJSError(JSContextRef context, JSValueRef value) :
        context_(context),
        value_(value)
    {
    }

    CYJSError(JSContextRef context, const char *format, ...);

    virtual const char *PoolCString(apr_pool_t *pool) const;
    virtual JSValueRef CastJSValue(JSContextRef context) const;
};

struct CYPoolError :
    CYException
{
    CYPool pool_;
    const char *message_;

    CYPoolError(const char *format, ...);
    CYPoolError(const char *format, va_list args);

    virtual const char *PoolCString(apr_pool_t *pool) const;
    virtual JSValueRef CastJSValue(JSContextRef context) const;
};

#endif/*CYCRIPT_ERROR_HPP*/
