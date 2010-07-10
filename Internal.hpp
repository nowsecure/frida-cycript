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

#ifndef CYCRIPT_INTERNAL_HPP
#define CYCRIPT_INTERNAL_HPP

#include "Pooling.hpp"

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include <sig/parse.hpp>
#include <sig/ffi_type.hpp>

JSGlobalContextRef CYGetJSContext(JSContextRef context);
void Structor_(apr_pool_t *pool, sig::Type *&type);

struct Type_privateData :
    CYData
{
    static JSClassRef Class_;

    ffi_type *ffi_;
    sig::Type *type_;

    void Set(sig::Type *type) {
        type_ = new(pool_) sig::Type;
        sig::Copy(pool_, *type_, *type);
    }

    Type_privateData(apr_pool_t *pool, const char *type) :
        ffi_(NULL)
    {
        _assert(pool != NULL);
        pool_ = pool;
        sig::Signature signature;
        sig::Parse(pool_, &signature, type, &Structor_);
        type_ = signature.elements[0].type;
    }

    Type_privateData(const char *type) :
        ffi_(NULL)
    {
        sig::Signature signature;
        sig::Parse(pool_, &signature, type, &Structor_);
        type_ = signature.elements[0].type;
    }

    Type_privateData(sig::Type *type) :
        ffi_(NULL)
    {
        if (type != NULL)
            Set(type);
    }

    Type_privateData(sig::Type *type, ffi_type *ffi) {
        ffi_ = new(pool_) ffi_type;
        sig::Copy(pool_, *ffi_, *ffi);
        Set(type);
    }

    ffi_type *GetFFI() {
        if (ffi_ == NULL) {
            ffi_ = new(pool_) ffi_type;

            sig::Element element;
            element.name = NULL;
            element.type = type_;
            element.offset = 0;

            sig::Signature signature;
            signature.elements = &element;
            signature.count = 1;

            ffi_cif cif;
            sig::sig_ffi_cif(pool_, &sig::ObjectiveC, &signature, &cif);
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
    sig::Signature signature_;
    ffi_cif cif_;

    Functor(const char *type, void (*value)()) :
        CYValue(reinterpret_cast<void *>(value))
    {
        sig::Parse(pool_, &signature_, type, &Structor_);
        sig::sig_ffi_cif(pool_, &sig::ObjectiveC, &signature_, &cif_);
    }

    void (*GetValue())() const {
        return reinterpret_cast<void (*)()>(value_);
    }

    static JSStaticFunction const * const StaticFunctions;
}; }

struct Closure_privateData :
    cy::Functor
{
    JSGlobalContextRef context_;
    JSObjectRef function_;

    Closure_privateData(JSContextRef context, JSObjectRef function, const char *type) :
        cy::Functor(type, NULL),
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

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const char *type, void (*callback)(ffi_cif *, void *, void **, void *));

#endif/*CYCRIPT_INTERNAL_HPP*/
