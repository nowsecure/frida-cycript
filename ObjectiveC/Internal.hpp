/* Cycript - Inlining/Optimizing JavaScript Compiler
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

#ifndef CYCRIPT_OBJECTIVEC_INTERNAL_HPP
#define CYCRIPT_OBJECTIVEC_INTERNAL_HPP

#include <Internal.hpp>
#include <objc/objc.h>

struct Selector_privateData :
    CYValue
{
    _finline Selector_privateData(SEL value) :
        CYValue(value)
    {
    }

    _finline SEL GetValue() const {
        return reinterpret_cast<SEL>(value_);
    }

    virtual Type_privateData *GetType() const;
};

struct Instance :
    CYValue
{
    enum Flags {
        None          = 0,
        Transient     = (1 << 0),
        Uninitialized = (1 << 1),
    };

    Flags flags_;

    _finline Instance(id value, Flags flags) :
        CYValue(value),
        flags_(flags)
    {
    }

    virtual ~Instance();

    static JSObjectRef Make(JSContextRef context, id object, Flags flags = None);

    _finline id GetValue() const {
        return reinterpret_cast<id>(value_);
    }

    _finline bool IsUninitialized() const {
        return (flags_ & Uninitialized) != 0;
    }

    virtual Type_privateData *GetType() const;
};

namespace cy {
struct Super :
    Instance
{
    Class class_;

    _finline Super(id value, Class _class) :
        Instance(value, Instance::Transient),
        class_(_class)
    {
    }

    static JSObjectRef Make(JSContextRef context, id object, Class _class);
}; }

struct Messages :
    CYValue
{
    _finline Messages(Class value) :
        CYValue(value)
    {
    }

    static JSObjectRef Make(JSContextRef context, Class _class, bool array = false);

    _finline Class GetValue() const {
        return reinterpret_cast<Class>(value_);
    }
};

struct Internal :
    CYOwned
{
    _finline Internal(id value, JSContextRef context, JSObjectRef owner) :
        CYOwned(value, context, owner)
    {
    }

    static JSObjectRef Make(JSContextRef context, id object, JSObjectRef owner);

    _finline id GetValue() const {
        return reinterpret_cast<id>(value_);
    }
};

#endif/*CYCRIPT_OBJECTIVEC_INTERNAL_HPP*/
