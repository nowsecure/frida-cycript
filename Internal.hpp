/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2013  Jay Freeman (saurik)
*/

/* GNU General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
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

#include "Pooling.hpp"

JSGlobalContextRef CYGetJSContext(JSContextRef context);
void Structor_(CYPool &pool, sig::Type *&type);

JSObjectRef CYMakeType(JSContextRef context, const char *type);
JSObjectRef CYMakeType(JSContextRef context, sig::Type *type);

extern JSClassRef Functor_;

struct Type_privateData :
    CYData
{
    static JSClassRef Class_;

    ffi_type *ffi_;
    sig::Type *type_;

    void Set(sig::Type *type) {
        type_ = new(*pool_) sig::Type;
        sig::Copy(*pool_, *type_, *type);
    }

    Type_privateData(CYPool &pool, const char *type) :
        CYData(pool),
        ffi_(NULL)
    {
        sig::Signature signature;
        sig::Parse(*pool_, &signature, type, &Structor_);
        type_ = signature.elements[0].type;
    }

    Type_privateData(const char *type) :
        ffi_(NULL)
    {
        sig::Signature signature;
        sig::Parse(*pool_, &signature, type, &Structor_);
        type_ = signature.elements[0].type;
    }

    Type_privateData(sig::Type *type) :
        ffi_(NULL)
    {
        // XXX: just in case I messed up migrating
        _assert(type != NULL);
        Set(type);
    }

    Type_privateData(sig::Type *type, ffi_type *ffi) {
        ffi_ = new(*pool_) ffi_type;
        sig::Copy(*pool_, *ffi_, *ffi);
        Set(type);
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
            sig::sig_ffi_cif(*pool_, &sig::ObjectiveC, &signature, &cif);

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

struct CYOwned :
    CYValue
{
  private:
    JSGlobalContextRef context_;
    JSObjectRef owner_;

  public:
    CYOwned(void *value, JSContextRef context, JSObjectRef owner) :
        CYValue(value),
        context_(CYGetJSContext(context)),
        owner_(owner)
    {
        //XXX:JSGlobalContextRetain(context_);
        if (owner_ != NULL)
            JSValueProtect(context_, owner_);
    }

    virtual ~CYOwned() {
        if (owner_ != NULL)
            JSValueUnprotect(context_, owner_);
        //XXX:JSGlobalContextRelease(context_);
    }

    JSObjectRef GetOwner() const {
        return owner_;
    }
};

namespace cy {
struct Functor :
    CYValue
{
  private:
    void set() {
        sig::sig_ffi_cif(*pool_, &sig::ObjectiveC, &signature_, &cif_);
    }

  public:
    sig::Signature signature_;
    ffi_cif cif_;

    Functor(const sig::Signature &signature, void (*value)()) :
        CYValue(reinterpret_cast<void *>(value))
    {
        sig::Copy(*pool_, signature_, signature);
        set();
    }

    Functor(const char *encoding, void (*value)()) :
        CYValue(reinterpret_cast<void *>(value))
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

    Closure_privateData(JSContextRef context, JSObjectRef function, const sig::Signature &signature) :
        cy::Functor(signature, NULL),
        context_(CYGetJSContext(context)),
        function_(function)
    {
        //XXX:JSGlobalContextRetain(context_);
        JSValueProtect(context_, function_);
    }

    virtual ~Closure_privateData() {
        JSValueUnprotect(context_, function_);
        //XXX:JSGlobalContextRelease(context_);
    }
};

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const sig::Signature &signature, void (*callback)(ffi_cif *, void *, void **, void *));
void CYExecuteClosure(ffi_cif *cif, void *result, void **arguments, void *arg, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef));

#endif/*CYCRIPT_INTERNAL_HPP*/
