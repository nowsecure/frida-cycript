#ifndef CYCRIPT_OBJECTIVEC_INTERNAL_HPP
#define CYCRIPT_OBJECTIVEC_INTERNAL_HPP

#include "Internal.hpp"

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
