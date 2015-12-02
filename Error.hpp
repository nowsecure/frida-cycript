/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
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

#ifndef CYCRIPT_ERROR_HPP
#define CYCRIPT_ERROR_HPP

#include "Exception.hpp"
#include "Pooling.hpp"

#ifdef CY_EXECUTE
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

    virtual const char *PoolCString(CYPool &pool) const;
    virtual JSValueRef CastJSValue(JSContextRef context) const;
};
#endif

struct _visible CYPoolError :
    CYException
{
    CYPool pool_;
    const char *message_;

    CYPoolError(const CYPoolError &rhs);

    CYPoolError(const char *format, ...);
    CYPoolError(const char *format, va_list args);

    virtual const char *PoolCString(CYPool &pool) const;
#ifdef CY_EXECUTE
    virtual JSValueRef CastJSValue(JSContextRef context) const;
#endif
};

#endif/*CYCRIPT_ERROR_HPP*/
