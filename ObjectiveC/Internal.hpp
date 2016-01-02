/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
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

#ifndef CYCRIPT_OBJECTIVEC_INTERNAL_HPP
#define CYCRIPT_OBJECTIVEC_INTERNAL_HPP

#include <objc/objc.h>

#include "../Internal.hpp"

struct Selector_privateData :
    CYValue<Selector_privateData, SEL>
{
    _finline Selector_privateData(SEL value) :
        CYValue(value)
    {
    }
};

struct Instance :
    CYValue<Instance, id>
{
    enum Flags {
        None          = 0,
        Permanent     = (1 << 0),
        Uninitialized = (1 << 1),
    };

    Flags flags_;

    Instance(id value, Flags flags);
    virtual ~Instance();

    JSValueRef GetPrototype(JSContextRef context) const;

    static JSClassRef GetClass(id value, Flags flags);

    _finline bool IsUninitialized() const {
        return (flags_ & Uninitialized) != 0;
    }
};

namespace cy {
struct Super :
    CYValue<Super, id>
{
    Class class_;

    _finline Super(id value, Class _class) :
        CYValue(value),
        class_(_class)
    {
    }
}; }

struct Messages :
    CYValue<Messages, Class>
{
    _finline Messages(Class value) :
        CYValue(value)
    {
    }

    JSValueRef GetPrototype(JSContextRef context) const;
};

struct Interior :
    CYValue<Interior, id>
{
    CYProtect owner_;

    _finline Interior(id value, JSContextRef context, JSObjectRef owner) :
        CYValue(value),
        owner_(context, owner)
    {
    }
};

#endif/*CYCRIPT_OBJECTIVEC_INTERNAL_HPP*/
