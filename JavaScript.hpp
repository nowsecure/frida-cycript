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

#ifndef CYCRIPT_JAVASCRIPT_HPP
#define CYCRIPT_JAVASCRIPT_HPP

#include <set>

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#ifdef HAVE_FFI_FFI_H
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

#include "Pooling.hpp"
#include "String.hpp"

extern JSStringRef Array_s;
extern JSStringRef cy_s;
extern JSStringRef cyi_s;
extern JSStringRef length_s;
extern JSStringRef message_s;
extern JSStringRef name_s;
extern JSStringRef pop_s;
extern JSStringRef prototype_s;
extern JSStringRef push_s;
extern JSStringRef splice_s;
extern JSStringRef toCYON_s;
extern JSStringRef toJSON_s;
extern JSStringRef toPointer_s;
extern JSStringRef toString_s;
extern JSStringRef weak_s;

void CYInitializeDynamic();
JSGlobalContextRef CYGetJSContext();
JSObjectRef CYGetGlobalObject(JSContextRef context);

extern "C" void CYSetupContext(JSGlobalContextRef context);
const char *CYExecute(JSContextRef context, CYPool &pool, CYUTF8String code);
void CYCancel();

void CYSetArgs(int argc, const char *argv[]);

bool CYCastBool(JSContextRef context, JSValueRef value);
double CYCastDouble(JSContextRef context, JSValueRef value);

bool CYIsEqual(JSContextRef context, JSValueRef lhs, JSValueRef rhs);
bool CYIsStrictEqual(JSContextRef context, JSValueRef lhs, JSValueRef rhs);

CYUTF8String CYPoolUTF8String(CYPool &pool, JSContextRef context, JSStringRef value);
const char *CYPoolCString(CYPool &pool, JSContextRef context, JSStringRef value);

bool CYHasProperty(JSContextRef context, JSObjectRef object, JSStringRef name);
JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, size_t index);
JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, JSStringRef name);

void CYSetProperty(JSContextRef context, JSObjectRef object, size_t index, JSValueRef value);
void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef value, JSPropertyAttributes attributes = kJSPropertyAttributeNone);
void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef (*callback)(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *), JSPropertyAttributes attributes = kJSPropertyAttributeNone);

void CYSetPrototype(JSContextRef context, JSObjectRef object, JSValueRef prototype);

JSValueRef CYGetCachedValue(JSContextRef context, JSStringRef name);
JSObjectRef CYGetCachedObject(JSContextRef context, JSStringRef name);

JSValueRef CYCastJSValue(JSContextRef context, bool value);
JSValueRef CYCastJSValue(JSContextRef context, double value);
JSValueRef CYCastJSValue(JSContextRef context, int value);
JSValueRef CYCastJSValue(JSContextRef context, unsigned int value);
JSValueRef CYCastJSValue(JSContextRef context, long int value);
JSValueRef CYCastJSValue(JSContextRef context, long unsigned int value);
JSValueRef CYCastJSValue(JSContextRef context, long long int value);
JSValueRef CYCastJSValue(JSContextRef context, long long unsigned int value);

JSValueRef CYCastJSValue(JSContextRef context, JSStringRef value);
JSValueRef CYCastJSValue(JSContextRef context, const char *value);

JSObjectRef CYCastJSObject(JSContextRef context, JSValueRef value);
JSValueRef CYJSUndefined(JSContextRef context);
JSValueRef CYJSNull(JSContextRef context);

void *CYCastPointer_(JSContextRef context, JSValueRef value, bool *guess = NULL);

template <typename Type_>
_finline Type_ CYCastPointer(JSContextRef context, JSValueRef value, bool *guess = NULL) {
    return reinterpret_cast<Type_>(CYCastPointer_(context, value, guess));
}

void CYPoolFFI(CYPool *pool, JSContextRef context, sig::Type *type, ffi_type *ffi, void *data, JSValueRef value);
JSValueRef CYFromFFI(JSContextRef context, sig::Type *type, ffi_type *ffi, void *data, bool initialize = false, JSObjectRef owner = NULL);

void CYCallFunction(CYPool &pool, JSContextRef context, ffi_cif *cif, void (*function)(), void *value, void **values);
JSValueRef CYCallFunction(CYPool &pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, sig::Signature *signature, ffi_cif *cif, void (*function)());

bool CYIsCallable(JSContextRef context, JSValueRef value);
JSValueRef CYCallAsFunction(JSContextRef context, JSObjectRef function, JSObjectRef _this, size_t count, const JSValueRef arguments[]);

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSObjectRef object, std::set<void *> &objects);
std::set<void *> *CYCastObjects(JSContextRef context, JSObjectRef _this, size_t count, const JSValueRef arguments[]);

struct CYHook {
    void *(*ExecuteStart)(JSContextRef);
    void (*ExecuteEnd)(JSContextRef, void *);

    void (*CallFunction)(CYPool &, JSContextRef, ffi_cif *, void (*)(), void *, void **);

    void (*Initialize)();
    void (*SetupContext)(JSContextRef);

    bool (*PoolFFI)(CYPool *, JSContextRef, sig::Type *, ffi_type *, void *, JSValueRef);
    JSValueRef (*FromFFI)(JSContextRef, sig::Type *, ffi_type *, void *, bool, JSObjectRef);

    void *(*CastSymbol)(const char *);
};

struct CYRegisterHook {
    CYRegisterHook(CYHook *hook);
};

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, size_t length, sig::Type *type, ffi_type *ffi, JSObjectRef owner);

JSObjectRef CYMakeType(JSContextRef context, const char *encoding);
JSObjectRef CYMakeType(JSContextRef context, sig::Type *type);
JSObjectRef CYMakeType(JSContextRef context, sig::Signature *signature);

void CYFinalize(JSObjectRef object);

size_t CYArrayLength(JSContextRef context, JSObjectRef array);
JSValueRef CYArrayGet(JSContextRef context, JSObjectRef array, size_t index);
void CYArrayPush(JSContextRef context, JSObjectRef array, JSValueRef value);

const char *CYPoolCString(CYPool &pool, JSContextRef context, JSValueRef value);

JSStringRef CYCopyJSString(const char *value);
JSStringRef CYCopyJSString(JSStringRef value);
JSStringRef CYCopyJSString(CYUTF8String value);
JSStringRef CYCopyJSString(CYUTF16String value);
JSStringRef CYCopyJSString(JSContextRef context, JSValueRef value);

void CYGarbageCollect(JSContextRef context);
void CYDestroyContext();

class CYJSString {
  private:
    JSStringRef string_;

    void Clear_() {
        if (string_ != NULL)
            JSStringRelease(string_);
    }

  public:
    CYJSString(const CYJSString &rhs) :
        string_(CYCopyJSString(rhs.string_))
    {
    }

    template <typename Arg0_>
    CYJSString(Arg0_ arg0) :
        string_(CYCopyJSString(arg0))
    {
    }

    template <typename Arg0_, typename Arg1_>
    CYJSString(Arg0_ arg0, Arg1_ arg1) :
        string_(CYCopyJSString(arg0, arg1))
    {
    }

    CYJSString &operator =(const CYJSString &rhs) {
        Clear_();
        string_ = CYCopyJSString(rhs.string_);
        return *this;
    }

    ~CYJSString() {
        Clear_();
    }

    void Clear() {
        Clear_();
        string_ = NULL;
    }

    operator JSStringRef() const {
        return string_;
    }
};

#ifdef __APPLE__
#define _weak __attribute__((__weak_import__));
#else
#define _weak
#endif

typedef struct OpaqueJSWeakObjectMap *JSWeakObjectMapRef;
typedef void (*JSWeakMapDestroyedCallback)(JSWeakObjectMapRef map, void *data);

extern "C" JSWeakObjectMapRef JSWeakObjectMapCreate(JSContextRef ctx, void *data, JSWeakMapDestroyedCallback destructor) _weak;
extern "C" void JSWeakObjectMapSet(JSContextRef ctx, JSWeakObjectMapRef map, void *key, JSObjectRef) _weak;
extern "C" JSObjectRef JSWeakObjectMapGet(JSContextRef ctx, JSWeakObjectMapRef map, void *key) _weak;
extern "C" bool JSWeakObjectMapClear(JSContextRef ctx, JSWeakObjectMapRef map, void *key, JSObjectRef object) _weak;
extern "C" void JSWeakObjectMapRemove(JSContextRef ctx, JSWeakObjectMapRef map, void* key) _weak;

typedef bool (*JSShouldTerminateCallback)(JSContextRef ctx, void *context);
extern "C" void JSContextGroupSetExecutionTimeLimit(JSContextGroupRef, double limit, JSShouldTerminateCallback, void *context) _weak;

#endif/*CYCRIPT_JAVASCRIPT_HPP*/
