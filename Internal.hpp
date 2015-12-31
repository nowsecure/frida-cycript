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

#ifndef CYCRIPT_INTERNAL_HPP
#define CYCRIPT_INTERNAL_HPP

#include <sig/parse.hpp>
#include <sig/ffi_type.hpp>

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include "JavaScript.hpp"
#include "Pooling.hpp"
#include "Utility.hpp"

JSGlobalContextRef CYGetJSContext(JSContextRef context);
sig::Type *Structor_(CYPool &pool, sig::Aggregate *aggregate);

extern JSClassRef Functor_;

struct Type_privateData :
    CYData
{
    static JSClassRef Class_;

    ffi_type *ffi_;
    sig::Type *type_;

    Type_privateData(const char *type) :
        ffi_(NULL)
    {
        sig::Signature signature;
        sig::Parse(*pool_, &signature, type, &Structor_);
        type_ = signature.elements[0].type;
    }

    Type_privateData(const sig::Type &type, ffi_type *ffi = NULL) :
        type_(type.Copy(*pool_))
    {

        if (ffi == NULL)
            ffi_ = NULL;
        else {
            ffi_ = new(*pool_) ffi_type;
            sig::Copy(*pool_, *ffi_, *ffi);
        }
    }

    ffi_type *GetFFI() {
        if (ffi_ == NULL) {
            sig::Element element;
            element.name = NULL;
            element.type = type_;
            element.offset = 0;

            sig::Signature signature;
            signature.elements = &element;
            signature.count = 1;

            ffi_cif cif;
            sig::sig_ffi_cif(*pool_, false, signature, &cif);

            ffi_ = new(*pool_) ffi_type;
            *ffi_ = *cif.rtype;
        }

        return ffi_;
    }
};

struct CYValue :
    CYData
{
    void *value_;

    CYValue() {
    }

    CYValue(const void *value) :
        value_(const_cast<void *>(value))
    {
    }

    CYValue(const CYValue &rhs) :
        value_(rhs.value_)
    {
    }

    virtual Type_privateData *GetType() const {
        return NULL;
    }
};

template <typename Internal_, typename Value_>
struct CYValue_ :
    CYValue
{
    static JSClassRef Class_;
    static Type_privateData *Type_;

    using CYValue::CYValue;

    _finline Value_ GetValue() const {
        return reinterpret_cast<Value_>(value_);
    }

    virtual Type_privateData *GetType() const {
        return Type_;
    }

    _finline JSValueRef GetPrototype(JSContextRef context) const {
        return NULL;
    }

    template <typename... Args_>
    _finline static JSClassRef GetClass(Args_ &&... args) {
        return Class_;
    }

    template <typename... Args_>
    static JSObjectRef Make(JSContextRef context, Args_ &&... args) {
        Internal_ *internal(new Internal_(cy::Forward<Args_>(args)...));
        JSObjectRef object(JSObjectMake(context, Internal_::GetClass(cy::Forward<Args_>(args)...), internal));
        if (JSValueRef prototype = internal->GetPrototype(context))
            CYSetPrototype(context, object, prototype);
        return object;
    }
};

template <typename Internal_, typename Value_>
JSClassRef CYValue_<Internal_, Value_>::Class_;

template <typename Internal_, typename Value_>
Type_privateData *CYValue_<Internal_, Value_>::Type_;

struct CYProtect {
  private:
    JSGlobalContextRef context_;
    JSObjectRef object_;

  public:
    CYProtect(JSContextRef context, JSObjectRef object) :
        context_(CYGetJSContext(context)),
        object_(object)
    {
        //XXX:JSGlobalContextRetain(context_);
        if (object_ != NULL)
            JSValueProtect(context_, object_);
    }

    ~CYProtect() {
        if (object_ != NULL)
            JSValueUnprotect(context_, object_);
        //XXX:JSGlobalContextRelease(context_);
    }

    operator JSObjectRef() const {
        return object_;
    }
};

namespace cy {
struct Functor :
    CYValue
{
  private:
    void set() {
        sig::sig_ffi_cif(*pool_, variadic_ ? signature_.count : 0, signature_, &cif_);
    }

  public:
    bool variadic_;
    sig::Signature signature_;
    ffi_cif cif_;

    Functor(void (*value)(), bool variadic, const sig::Signature &signature) :
        CYValue(reinterpret_cast<void *>(value)),
        variadic_(variadic)
    {
        sig::Copy(*pool_, signature_, signature);
        set();
    }

    Functor(void (*value)(), const char *encoding) :
        CYValue(reinterpret_cast<void *>(value)),
        variadic_(false)
    {
        sig::Parse(*pool_, &signature_, encoding, &Structor_);
        set();
    }

    void (*GetValue() const)() {
        return reinterpret_cast<void (*)()>(value_);
    }

    static JSStaticFunction const * const StaticFunctions;
    static JSStaticValue const * const StaticValues;
}; }

struct Closure_privateData :
    cy::Functor
{
    JSGlobalContextRef context_;
    JSObjectRef function_;
    JSValueRef (*adapter_)(JSContextRef, size_t, JSValueRef[], JSObjectRef);

    Closure_privateData(JSContextRef context, JSObjectRef function, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef), const sig::Signature &signature) :
        cy::Functor(NULL, false, signature),
        context_(CYGetJSContext(context)),
        function_(function),
        adapter_(adapter)
    {
        //XXX:JSGlobalContextRetain(context_);
        JSValueProtect(context_, function_);
    }

    virtual ~Closure_privateData() {
        JSValueUnprotect(context_, function_);
        //XXX:JSGlobalContextRelease(context_);
    }
};

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const sig::Signature &signature, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef));
void CYExecuteClosure(ffi_cif *cif, void *result, void **arguments, void *arg, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef));

#endif/*CYCRIPT_INTERNAL_HPP*/
