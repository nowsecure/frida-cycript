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

#ifndef SIG_TYPES_H
#define SIG_TYPES_H

#include <cstdlib>
#include <stdint.h>

#include <JavaScriptCore/JSBase.h>

#ifdef HAVE_FFI_FFI_H
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

#include "Standard.hpp"

class CYPool;
struct CYType;
struct CYTypedParameter;

namespace sig {

#define JOC_TYPE_INOUT  (1 << 0)
#define JOC_TYPE_IN     (1 << 1)
#define JOC_TYPE_BYCOPY (1 << 2)
#define JOC_TYPE_OUT    (1 << 3)
#define JOC_TYPE_BYREF  (1 << 4)
#define JOC_TYPE_CONST  (1 << 5)
#define JOC_TYPE_ONEWAY (1 << 6)

struct Type {
    uint8_t flags;

    Type() :
        flags(0)
    {
    }

    template <typename Type_>
    _finline Type_ *Flag(Type_ *type) const {
        type->flags = flags;
        return type;
    }

    virtual Type *Copy(CYPool &pool, const char *rename = NULL) const = 0;
    virtual const char *GetName() const;

    virtual const char *Encode(CYPool &pool) const = 0;
    virtual CYType *Decode(CYPool &pool) const = 0;

    virtual ffi_type *GetFFI(CYPool &pool) const = 0;
    virtual void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const = 0;
    virtual JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize = false, JSObjectRef owner = NULL) const = 0;
};

template <typename Type_>
struct Primitive :
    Type
{
    Primitive *Copy(CYPool &pool, const char *name) const {
        return Flag(new(pool) Primitive());
    }

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Element {
    const char *name;
    Type *type;
    size_t offset;
};

struct Signature {
    Element *elements;
    size_t count;
};

struct Void :
    Type
{
    Void *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Unknown :
    Type
{
    Unknown *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct String :
    Type
{
    String() {
    }

    String(bool constant) {
        if (constant)
            flags |= JOC_TYPE_CONST;
    }

    String *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

#ifdef CY_OBJECTIVEC
struct Meta :
    Type
{
    Meta *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Selector :
    Type
{
    Selector *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};
#endif

struct Bits :
    Type
{
    size_t size;

    Bits(size_t size) :
        size(size)
    {
    }

    Bits *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Pointer :
    Type
{
    Type &type;

    Pointer(Type &type) :
        type(type)
    {
    }

    Pointer *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Array :
    Type
{
    Type &type;
    size_t size;

    Array(Type &type, size_t size = _not(size_t)) :
        type(type),
        size(size)
    {
    }

    Array *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

#ifdef CY_OBJECTIVEC
struct Object :
    Type
{
    const char *name;

    Object(const char *name = NULL) :
        name(name)
    {
    }

    Object *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};
#endif

struct Constant {
    const char *name;
    double value;
};

struct Enum :
    Type
{
    Type &type;
    unsigned count;
    const char *name;

    Constant *constants;

    Enum(Type &type, unsigned count, const char *name = NULL) :
        type(type),
        count(count),
        name(name),
        constants(NULL)
    {
    }

    Enum *Copy(CYPool &pool, const char *rename = NULL) const override;
    const char *GetName() const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Aggregate :
    Type
{
    bool overlap;
    const char *name;
    Signature signature;

    Aggregate(bool overlap, const char *name = NULL) :
        overlap(overlap),
        name(name)
    {
    }

    Aggregate *Copy(CYPool &pool, const char *rename = NULL) const override;
    const char *GetName() const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

struct Callable :
    Type
{
    Signature signature;

    CYType *Decode(CYPool &pool) const override;
    virtual CYType *Modify(CYPool &pool, CYType *result, CYTypedParameter *parameters) const = 0;
};

struct Function :
    Callable
{
    bool variadic;

    Function(bool variadic) :
        variadic(variadic)
    {
    }

    Function *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Modify(CYPool &pool, CYType *result, CYTypedParameter *parameters) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};

#ifdef CY_OBJECTIVEC
struct Block :
    Callable
{
    Block *Copy(CYPool &pool, const char *rename = NULL) const override;

    const char *Encode(CYPool &pool) const override;
    CYType *Decode(CYPool &pool) const override;
    CYType *Modify(CYPool &pool, CYType *result, CYTypedParameter *parameters) const override;

    ffi_type *GetFFI(CYPool &pool) const override;
    void PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const override;
    JSValueRef FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const override;
};
#endif

Type *joc_parse_type(char **name, char eos, bool variable, bool signature);
void joc_parse_signature(Signature *signature, char **name, char eos, bool variable);

}

#endif/*SIG_TYPES_H*/
