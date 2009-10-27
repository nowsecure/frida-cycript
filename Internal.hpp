/* Cycript - Error.hppution Server and Disassembler
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

#ifndef CYCRIPT_INTERNAL_HPP
#define CYCRIPT_INTERNAL_HPP

#include "Pooling.hpp"

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include <sig/parse.hpp>
#include <sig/ffi_type.hpp>

void Structor_(apr_pool_t *pool, const char *name, const char *types, sig::Type *&type);

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
        if (pool != NULL)
            pool_ = pool;

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
    JSContextRef context_;
    JSObjectRef owner_;

  public:
    CYOwned(void *value, JSContextRef context, JSObjectRef owner) :
        CYValue(value),
        context_(context),
        owner_(owner)
    {
        if (owner_ != NULL)
            JSValueProtect(context_, owner_);
    }

    virtual ~CYOwned() {
        if (owner_ != NULL)
            JSValueUnprotect(context_, owner_);
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
    JSContextRef context_;
    JSObjectRef function_;

    Closure_privateData(JSContextRef context, JSObjectRef function, const char *type) :
        cy::Functor(type, NULL),
        context_(context),
        function_(function)
    {
        JSValueProtect(context_, function_);
    }

    virtual ~Closure_privateData() {
        JSValueUnprotect(context_, function_);
    }
};

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const char *type, void (*callback)(ffi_cif *, void *, void **, void *));

#endif/*CYCRIPT_INTERNAL_HPP*/
