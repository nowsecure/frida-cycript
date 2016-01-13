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

#ifndef CYCRIPT_JAVASCRIPT_HPP
#define CYCRIPT_JAVASCRIPT_HPP

#include <set>
#include <string>

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
#include "Utility.hpp"

extern JSStringRef Array_s;
extern JSStringRef constructor_s;
extern JSStringRef cy_s;
extern JSStringRef cyi_s;
extern JSStringRef cyt_s;
extern JSStringRef cyt__s;
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

#ifndef __ANDROID__
void CYCancel();
#endif

void CYSetArgs(const char *argv0, const char *script, int argc, const char *argv[]);

bool CYCastBool(JSContextRef context, JSValueRef value);
double CYCastDouble(JSContextRef context, JSValueRef value);

bool CYIsEqual(JSContextRef context, JSValueRef lhs, JSValueRef rhs);
bool CYIsStrictEqual(JSContextRef context, JSValueRef lhs, JSValueRef rhs);

CYUTF16String CYCastUTF16String(JSStringRef value);
CYUTF8String CYPoolUTF8String(CYPool &pool, JSContextRef context, JSStringRef value);
const char *CYPoolCString(CYPool &pool, JSContextRef context, JSStringRef value);

bool CYHasProperty(JSContextRef context, JSObjectRef object, JSStringRef name);
JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, size_t index);
JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, JSStringRef name);

void CYSetProperty(JSContextRef context, JSObjectRef object, size_t index, JSValueRef value);
void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef value, JSPropertyAttributes attributes = kJSPropertyAttributeNone);
void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef (*callback)(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *), JSPropertyAttributes attributes = kJSPropertyAttributeNone);

JSObjectRef CYGetPrototype(JSContextRef context, JSObjectRef object);
void CYSetPrototype(JSContextRef context, JSObjectRef object, JSValueRef prototype);

JSValueRef CYGetCachedValue(JSContextRef context, JSStringRef name);
JSObjectRef CYGetCachedObject(JSContextRef context, JSStringRef name);

JSValueRef CYCastJSValue(JSContextRef context, bool value);
JSValueRef CYCastJSValue(JSContextRef context, double value);

JSValueRef CYCastJSValue(JSContextRef context, signed short int value);
JSValueRef CYCastJSValue(JSContextRef context, unsigned short int value);
JSValueRef CYCastJSValue(JSContextRef context, signed int value);
JSValueRef CYCastJSValue(JSContextRef context, unsigned int value);
JSValueRef CYCastJSValue(JSContextRef context, signed long int value);
JSValueRef CYCastJSValue(JSContextRef context, unsigned long int value);
JSValueRef CYCastJSValue(JSContextRef context, signed long long int value);
JSValueRef CYCastJSValue(JSContextRef context, unsigned long long int value);

JSValueRef CYCastJSValue(JSContextRef context, JSStringRef value);
JSValueRef CYCastJSValue(JSContextRef context, const char *value);

JSObjectRef CYCastJSObject(JSContextRef context, JSValueRef value);
JSValueRef CYJSUndefined(JSContextRef context);
JSValueRef CYJSNull(JSContextRef context);

void *CYCastPointerEx_(JSContextRef context, JSObjectRef value);

template <typename Type_>
_finline Type_ CYCastPointerEx(JSContextRef context, JSObjectRef value) {
    return reinterpret_cast<Type_>(CYCastPointerEx_(context, value));
}

void *CYCastPointer_(JSContextRef context, JSValueRef value, bool *guess = NULL);

template <typename Type_>
_finline Type_ CYCastPointer(JSContextRef context, JSValueRef value, bool *guess = NULL) {
    return reinterpret_cast<Type_>(CYCastPointer_(context, value, guess));
}

void CYCallFunction(CYPool &pool, JSContextRef context, ffi_cif *cif, void (*function)(), void *value, void **values);
JSValueRef CYCallFunction(CYPool &pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, bool variadic, const sig::Signature &signature, ffi_cif *cif, void (*function)());

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

    void *(*CastSymbol)(const char *);
};

struct CYRegisterHook {
    CYRegisterHook(CYHook *hook);
};

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, const sig::Type &type, ffi_type *ffi, JSObjectRef owner);

JSObjectRef CYMakeType(JSContextRef context, const sig::Type &type);

void CYFinalize(JSObjectRef object);

JSObjectRef CYObjectMakeArray(JSContextRef context, size_t length, const JSValueRef values[]);

size_t CYArrayLength(JSContextRef context, JSObjectRef array);
JSValueRef CYArrayGet(JSContextRef context, JSObjectRef array, size_t index);

void CYArrayPush(JSContextRef context, JSObjectRef array, size_t length, const JSValueRef arguments[]);
void CYArrayPush(JSContextRef context, JSObjectRef array, JSValueRef value);

bool CYGetOffset(CYPool &pool, JSContextRef context, JSStringRef value, ssize_t &index);

const char *CYPoolCString(CYPool &pool, JSContextRef context, JSValueRef value);

JSStringRef CYCopyJSString(const char *value);
JSStringRef CYCopyJSString(JSStringRef value);
JSStringRef CYCopyJSString(CYUTF8String value);
JSStringRef CYCopyJSString(const std::string &value);
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
    CYJSString() :
        string_(NULL)
    {
    }

    CYJSString(const CYJSString &rhs) :
        string_(CYCopyJSString(rhs.string_))
    {
    }

    template <typename ...Args_>
    CYJSString(Args_ &&... args) :
        string_(CYCopyJSString(cy::Forward<Args_>(args)...))
    {
    }

    CYJSString &operator =(const CYJSString &rhs) {
        Clear_();
        string_ = CYCopyJSString(rhs.string_);
        return *this;
    }

    CYJSString &operator =(CYJSString &&rhs) {
        std::swap(string_, rhs.string_);
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

template <size_t Size_>
class CYArrayBuilder {
  private:
    JSContextRef context_;
    JSObjectRef &array_;
    size_t size_;
    JSValueRef values_[Size_];

    void flush() {
        if (array_ == NULL)
            array_ = CYObjectMakeArray(context_, size_, values_);
        else
            CYArrayPush(context_, array_, size_, values_);
    }

  public:
    CYArrayBuilder(JSContextRef context, JSObjectRef &array) :
        context_(context),
        array_(array),
        size_(0)
    {
    }

    ~CYArrayBuilder() {
        flush();
    }

    void operator ()(JSValueRef value) {
        if (size_ == Size_) {
            flush();
            size_ = 0;
        }

        values_[size_++] = value;
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
