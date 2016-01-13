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

#include "cycript.hpp"

#include <iostream>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <cmath>

#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <sqlite3.h>

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include "Bridge.hpp"
#include "Code.hpp"
#include "Decode.hpp"
#include "Error.hpp"
#include "Execute.hpp"
#include "Internal.hpp"
#include "JavaScript.hpp"
#include "Pooling.hpp"
#include "String.hpp"

const char *sqlite3_column_string(sqlite3_stmt *stmt, int n) {
    return reinterpret_cast<const char *>(sqlite3_column_text(stmt, n));
}

char *sqlite3_column_pooled(CYPool &pool, sqlite3_stmt *stmt, int n) {
    if (const char *value = sqlite3_column_string(stmt, n))
        return pool.strdup(value);
    else return NULL;
}

static std::vector<CYHook *> &GetHooks() {
    static std::vector<CYHook *> hooks;
    return hooks;
}

CYRegisterHook::CYRegisterHook(CYHook *hook) {
    GetHooks().push_back(hook);
}

/* JavaScript Properties {{{ */
bool CYHasProperty(JSContextRef context, JSObjectRef object, JSStringRef name) {
    return JSObjectHasProperty(context, object, name);
}

JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, size_t index) {
    return _jsccall(JSObjectGetPropertyAtIndex, context, object, index);
}

JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, JSStringRef name) {
    return _jsccall(JSObjectGetProperty, context, object, name);
}

void CYSetProperty(JSContextRef context, JSObjectRef object, size_t index, JSValueRef value) {
    _jsccall(JSObjectSetPropertyAtIndex, context, object, index, value);
}

void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef value, JSPropertyAttributes attributes) {
    _jsccall(JSObjectSetProperty, context, object, name, value, attributes);
}

void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef (*callback)(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *), JSPropertyAttributes attributes) {
    CYSetProperty(context, object, name, JSObjectMakeFunctionWithCallback(context, name, callback), attributes);
}

JSObjectRef CYGetPrototype(JSContextRef context, JSObjectRef object) {
    return CYCastJSObject(context, JSObjectGetPrototype(context, object));
}

void CYSetPrototype(JSContextRef context, JSObjectRef object, JSValueRef value) {
    _assert(!JSValueIsUndefined(context, value));
    JSObjectSetPrototype(context, object, value);
    _assert(CYIsStrictEqual(context, JSObjectGetPrototype(context, object), value));
}
/* }}} */
/* JavaScript Strings {{{ */
JSStringRef CYCopyJSString(const char *value) {
    return value == NULL ? NULL : JSStringCreateWithUTF8CString(value);
}

JSStringRef CYCopyJSString(JSStringRef value) {
    return value == NULL ? NULL : JSStringRetain(value);
}

JSStringRef CYCopyJSString(CYUTF8String value) {
    if (memchr(value.data, '\0', value.size) != NULL) {
        CYPool pool;
        return CYCopyJSString(CYPoolUTF16String(pool, value));
    } else if (value.data[value.size] != '\0') {
        CYPool pool;
        return CYCopyJSString(pool.strmemdup(value.data, value.size));
    } else {
        return CYCopyJSString(value.data);
    }
}

JSStringRef CYCopyJSString(const std::string &value) {
    return CYCopyJSString(CYUTF8String(value.c_str(), value.size()));
}

JSStringRef CYCopyJSString(CYUTF16String value) {
    return JSStringCreateWithCharacters(value.data, value.size);
}

JSStringRef CYCopyJSString(JSContextRef context, JSValueRef value) {
    if (JSValueIsNull(context, value))
        return NULL;
    return _jsccall(JSValueToStringCopy, context, value);
}

CYUTF16String CYCastUTF16String(JSStringRef value) {
    return CYUTF16String(JSStringGetCharactersPtr(value), JSStringGetLength(value));
}

const char *CYPoolCString(CYPool &pool, CYUTF8String utf8) {
    return pool.strndup(utf8.data, utf8.size);
}

CYUTF8String CYPoolUTF8String(CYPool &pool, CYUTF8String utf8) {
    return {pool.strndup(utf8.data, utf8.size), utf8.size};
}

_visible CYUTF8String CYPoolUTF8String(CYPool &pool, const std::string &value) {
    return {pool.strndup(value.data(), value.size()), value.size()};
}

CYUTF8String CYPoolUTF8String(CYPool &pool, JSContextRef context, JSStringRef value) {
    return CYPoolUTF8String(pool, CYCastUTF16String(value));
}

const char *CYPoolCString(CYPool &pool, JSContextRef context, JSStringRef value) {
    CYUTF8String utf8(CYPoolUTF8String(pool, context, value));
    _assert(memchr(utf8.data, '\0', utf8.size) == NULL);
    return utf8.data;
}

const char *CYPoolCString(CYPool &pool, JSContextRef context, JSValueRef value) {
    return JSValueIsNull(context, value) ? NULL : CYPoolCString(pool, context, CYJSString(context, value));
}
/* }}} */
/* Index Offsets {{{ */
size_t CYGetIndex(CYPool &pool, JSContextRef context, JSStringRef value) {
    return CYGetIndex(CYPoolUTF8String(pool, context, value));
}
/* }}} */

static JSObjectRef (*JSObjectMakeArray$)(JSContextRef, size_t, const JSValueRef[], JSValueRef *);

JSObjectRef CYObjectMakeArray(JSContextRef context, size_t length, const JSValueRef values[]) {
    if (JSObjectMakeArray$ != NULL)
        return _jsccall(*JSObjectMakeArray$, context, length, values);
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array")));
    bool wat(length == 1 && JSValueGetType(context, values[0]) == kJSTypeNumber);
    JSValueRef value(CYCallAsFunction(context, Array, NULL, wat ? 0 : length, values));
    JSObjectRef object(CYCastJSObject(context, value));
    if (wat) CYArrayPush(context, object, 1, values);
    return object;
}

static JSClassRef All_;
JSClassRef cy::Functor::Class_;
static JSClassRef Global_;

JSStringRef Array_s;
JSStringRef constructor_s;
JSStringRef cy_s;
JSStringRef cyi_s;
JSStringRef cyt_s;
JSStringRef cyt__s;
JSStringRef length_s;
JSStringRef message_s;
JSStringRef name_s;
JSStringRef pop_s;
JSStringRef prototype_s;
JSStringRef push_s;
JSStringRef splice_s;
JSStringRef toCYON_s;
JSStringRef toJSON_s;
JSStringRef toPointer_s;
JSStringRef toString_s;
JSStringRef weak_s;

static sqlite3 *database_;

static JSStringRef Result_;

void CYFinalize(JSObjectRef object) {
    CYData *internal(reinterpret_cast<CYData *>(JSObjectGetPrivate(object)));
    if (internal == NULL)
        return;
    _assert(internal->count_ != _not(unsigned));
    if (--internal->count_ == 0)
        delete internal;
}

sig::Type *Structor_(CYPool &pool, sig::Aggregate *aggregate) {
    //_assert(false);
    return aggregate;
}

struct Context :
    CYRoot
{
    JSGlobalContextRef context_;

    Context(JSGlobalContextRef context) :
        context_(context)
    {
    }
};

struct CArray :
    CYRoot
{
    void *value_;
    CYProtect owner_;
    Type_privateData *type_;
    size_t length_;

    CArray(void *value, size_t length, const sig::Type &type, ffi_type *ffi, JSContextRef context, JSObjectRef owner) :
        value_(value),
        owner_(context, owner),
        type_(new(*pool_) Type_privateData(type, ffi)),
        length_(length)
    {
    }
};

struct CString :
    CYRoot
{
    char *value_;
    CYProtect owner_;

    CString(char *value, JSContextRef context, JSObjectRef owner) :
        value_(value),
        owner_(context, owner)
    {
    }
};

struct Pointer :
    CYRoot
{
    void *value_;
    CYProtect owner_;
    Type_privateData *type_;

    Pointer(void *value, const sig::Type &type, JSContextRef context, JSObjectRef owner) :
        value_(value),
        owner_(context, owner),
        type_(new(*pool_) Type_privateData(type))
    {
    }

    Pointer(void *value, const char *encoding, JSContextRef context, JSObjectRef owner) :
        value_(value),
        owner_(context, owner),
        type_(new(*pool_) Type_privateData(encoding))
    {
    }
};

struct Struct_privateData :
    CYRoot
{
    void *value_;
    CYProtect owner_;
    Type_privateData *type_;

    Struct_privateData(void *value, const sig::Type &type, ffi_type *ffi, JSContextRef context, JSObjectRef owner) :
        value_(value),
        owner_(context, owner),
        type_(new(*pool_) Type_privateData(type, ffi))
    {
        if (owner == NULL) {
            size_t size(ffi->size);
            void *copy(pool_->malloc<void>(size, ffi->alignment));
            memcpy(copy, value_, size);
            value_ = copy;
        }
    }
};

static void *CYCastSymbol(const char *name) {
    for (CYHook *hook : GetHooks())
        if (hook->CastSymbol != NULL)
            if (void *value = (*hook->CastSymbol)(name))
                return value;
    return dlsym(RTLD_DEFAULT, name);
}

JSValueRef CYCastJSValue(JSContextRef context, bool value) {
    return JSValueMakeBoolean(context, value);
}

JSValueRef CYCastJSValue(JSContextRef context, double value) {
    return JSValueMakeNumber(context, value);
}

#define CYCastJSValue_(Type_) \
    JSValueRef CYCastJSValue(JSContextRef context, Type_ value) { \
        _assert(static_cast<Type_>(static_cast<double>(value)) == value); \
        return JSValueMakeNumber(context, static_cast<double>(value)); \
    }

CYCastJSValue_(long double)
CYCastJSValue_(signed short int)
CYCastJSValue_(unsigned short int)
CYCastJSValue_(signed int)
CYCastJSValue_(unsigned int)
CYCastJSValue_(signed long int)
CYCastJSValue_(unsigned long int)
CYCastJSValue_(signed long long int)
CYCastJSValue_(unsigned long long int)

#ifdef __SIZEOF_INT128__
CYCastJSValue_(signed __int128)
CYCastJSValue_(unsigned __int128)
#endif

JSValueRef CYJSUndefined(JSContextRef context) {
    return JSValueMakeUndefined(context);
}

double CYCastDouble(JSContextRef context, JSValueRef value) {
    return _jsccall(JSValueToNumber, context, value);
}

bool CYCastBool(JSContextRef context, JSValueRef value) {
    return JSValueToBoolean(context, value);
}

JSValueRef CYJSNull(JSContextRef context) {
    return JSValueMakeNull(context);
}

JSValueRef CYCastJSValue(JSContextRef context, JSStringRef value) {
    return value == NULL ? CYJSNull(context) : JSValueMakeString(context, value);
}

JSValueRef CYCastJSValue(JSContextRef context, const char *value) {
    return CYCastJSValue(context, CYJSString(value));
}

JSObjectRef CYCastJSObject(JSContextRef context, JSValueRef value) {
    return _jsccall(JSValueToObject, context, value);
}

JSValueRef CYCallAsFunction(JSContextRef context, JSObjectRef function, JSObjectRef _this, size_t count, const JSValueRef arguments[]) {
    return _jsccall(JSObjectCallAsFunction, context, function, _this, count, arguments);
}

bool CYIsCallable(JSContextRef context, JSValueRef value) {
    return value != NULL && JSValueIsObject(context, value) && JSObjectIsFunction(context, (JSObjectRef) value);
}

bool CYIsEqual(JSContextRef context, JSValueRef lhs, JSValueRef rhs) {
    return _jsccall(JSValueIsEqual, context, lhs, rhs);
}

bool CYIsStrictEqual(JSContextRef context, JSValueRef lhs, JSValueRef rhs) {
    return JSValueIsStrictEqual(context, lhs, rhs);
}

size_t CYArrayLength(JSContextRef context, JSObjectRef array) {
    return CYCastDouble(context, CYGetProperty(context, array, length_s));
}

JSValueRef CYArrayGet(JSContextRef context, JSObjectRef array, size_t index) {
    return _jsccall(JSObjectGetPropertyAtIndex, context, array, index);
}

void CYArrayPush(JSContextRef context, JSObjectRef array, size_t length, const JSValueRef arguments[]) {
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
    _jsccall(JSObjectCallAsFunction, context, CYCastJSObject(context, CYGetProperty(context, Array, push_s)), array, length, arguments);
}

void CYArrayPush(JSContextRef context, JSObjectRef array, JSValueRef value) {
    return CYArrayPush(context, array, 1, &value);
}

static JSValueRef System_print(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    FILE *file(stdout);

    if (count == 0)
        fputc('\n', file);
    else {
        CYPool pool;
        CYUTF8String string(CYPoolUTF8String(pool, context, CYJSString(context, arguments[0])));
        fwrite(string.data, string.size, 1, file);
    }

    fflush(file);
    return CYJSUndefined(context);
} CYCatch(NULL) }

static JSValueRef Global_print(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    FILE *file(stdout);
    CYPool pool;

    for (size_t i(0); i != count; ++i) {
        if (i != 0)
            fputc(' ', file);
        CYUTF8String string(CYPoolUTF8String(pool, context, CYJSString(context, arguments[i])));
        fwrite(string.data, string.size, 1, file);
    }

    fputc('\n', file);
    fflush(file);
    return CYJSUndefined(context);
} CYCatch(NULL) }

static void (*JSSynchronousGarbageCollectForDebugging$)(JSContextRef);

_visible void CYGarbageCollect(JSContextRef context) {
    (JSSynchronousGarbageCollectForDebugging$ ?: &JSGarbageCollect)(context);
}

static JSValueRef Cycript_compile_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    CYUTF8String before(CYPoolUTF8String(pool, context, CYJSString(context, arguments[0])));
    CYUTF8String after(CYPoolCode(pool, before));
    return CYCastJSValue(context, CYJSString(after));
} CYCatch_(NULL, "SyntaxError") }

static JSValueRef Cycript_gc_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYGarbageCollect(context);
    return CYJSUndefined(context);
} CYCatch(NULL) }

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSValueRef value, std::set<void *> &objects, JSValueRef *exception) { CYTry {
    switch (JSType type = JSValueGetType(context, value)) {
        case kJSTypeUndefined:
            return "undefined";
        case kJSTypeNull:
            return "null";
        case kJSTypeBoolean:
            return CYCastBool(context, value) ? "true" : "false";

        case kJSTypeNumber: {
            std::ostringstream str;
            CYNumerify(str, CYCastDouble(context, value));
            std::string value(str.str());
            return pool.strmemdup(value.c_str(), value.size());
        } break;

        case kJSTypeString: {
            std::ostringstream str;
            CYUTF8String string(CYPoolUTF8String(pool, context, CYJSString(context, value)));
            CYStringify(str, string.data, string.size, CYStringifyModeCycript);
            std::string value(str.str());
            return pool.strmemdup(value.c_str(), value.size());
        } break;

        case kJSTypeObject:
            return CYPoolCCYON(pool, context, (JSObjectRef) value, objects);
        default:
            throw CYJSError(context, "JSValueGetType() == 0x%x", type);
    }
} CYCatch(NULL) }

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSValueRef value, std::set<void *> &objects) {
    return _jsccall(CYPoolCCYON, pool, context, value, objects);
}

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSValueRef value, std::set<void *> *objects) {
    if (objects != NULL)
        return CYPoolCCYON(pool, context, value, *objects);
    else {
        std::set<void *> objects;
        return CYPoolCCYON(pool, context, value, objects);
    }
}

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSObjectRef object, std::set<void *> &objects) {
    JSValueRef toCYON(CYGetProperty(context, object, toCYON_s));
    if (CYIsCallable(context, toCYON)) {
        // XXX: this needs to be abstracted behind some kind of function
        JSValueRef arguments[1] = {CYCastJSValue(context, reinterpret_cast<uintptr_t>(&objects))};
        JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toCYON, object, 1, arguments));
        _assert(value != NULL);
        return CYPoolCString(pool, context, value);
    }

    JSValueRef toJSON(CYGetProperty(context, object, toJSON_s));
    if (CYIsCallable(context, toJSON)) {
        JSValueRef arguments[1] = {CYCastJSValue(context, CYJSString(""))};
        return _jsccall(CYPoolCCYON, pool, context, CYCallAsFunction(context, (JSObjectRef) toJSON, object, 1, arguments), objects);
    }

    if (JSObjectIsFunction(context, object)) {
        JSValueRef toString(CYGetProperty(context, object, toString_s));
        if (CYIsCallable(context, toString)) {
            JSValueRef arguments[1] = {CYCastJSValue(context, CYJSString(""))};
            JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toString, object, 1, arguments));
            _assert(value != NULL);
            return CYPoolCString(pool, context, value);
        }
    }

    _assert(objects.insert(object).second);

    std::ostringstream str;

    JSValueRef value(CYGetProperty(context, object, constructor_s));
    if (JSValueIsObject(context, value)) {
        JSObjectRef constructor(CYCastJSObject(context, value));
        JSValueRef theory(CYGetProperty(context, constructor, prototype_s));
        JSValueRef practice(JSObjectGetPrototype(context, object));

        if (CYIsStrictEqual(context, theory, practice)) {
            JSValueRef name(CYGetProperty(context, constructor, name_s));
            if (!JSValueIsUndefined(context, name)) {
                auto utf8(CYPoolUTF8String(pool, context, CYJSString(context, name)));
                if (utf8 != "Object")
                    str << "new" << ' ' << utf8;
            }
        }
    }

    str << '{';

    // XXX: this is, sadly, going to leak
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context, object));

    bool comma(false);

    for (size_t index(0), count(JSPropertyNameArrayGetCount(names)); index != count; ++index) {
        if (comma)
            str << ',';
        else
            comma = true;

        JSStringRef name(JSPropertyNameArrayGetNameAtIndex(names, index));
        CYUTF8String string(CYPoolUTF8String(pool, context, name));

        if (CYIsKey(string))
            str << string.data;
        else
            CYStringify(str, string.data, string.size, CYStringifyModeLegacy);

        str << ':';

        try {
            JSValueRef value(CYGetProperty(context, object, name));
            str << CYPoolCCYON(pool, context, value, objects);
        } catch (const CYException &error) {
            str << "@error";
        }
    }

    JSPropertyNameArrayRelease(names);

    str << '}';

    std::string string(str.str());
    return pool.strmemdup(string.c_str(), string.size());
}

std::set<void *> *CYCastObjects(JSContextRef context, JSObjectRef _this, size_t count, const JSValueRef arguments[]) {
    if (count == 0)
        return NULL;
    return CYCastPointer<std::set<void *> *>(context, arguments[0]);
}

static JSValueRef Array_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    std::set<void *> *objects(CYCastObjects(context, _this, count, arguments));
    // XXX: this is horribly inefficient
    std::set<void *> backup;
    if (objects == NULL)
        objects = &backup;

    CYPool pool;
    std::ostringstream str;

    str << '[';

    JSValueRef length(CYGetProperty(context, _this, length_s));
    bool comma(false);

    for (size_t index(0), count(CYCastDouble(context, length)); index != count; ++index) {
        if (comma)
            str << ',';
        else
            comma = true;

        try {
            JSValueRef value(CYGetProperty(context, _this, index));
            if (!JSValueIsUndefined(context, value))
                str << CYPoolCCYON(pool, context, value, *objects);
            else {
                str << ',';
                comma = false;
            }
        } catch (const CYException &error) {
            str << "@error";
        }
    }

    str << ']';

    std::string value(str.str());
    return CYCastJSValue(context, CYJSString(CYUTF8String(value.c_str(), value.size())));
} CYCatch(NULL) }

static JSValueRef String_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    std::ostringstream str;

    CYUTF8String string(CYPoolUTF8String(pool, context, CYJSString(context, _this)));
    CYStringify(str, string.data, string.size, CYStringifyModeCycript);

    std::string value(str.str());
    return CYCastJSValue(context, CYJSString(CYUTF8String(value.c_str(), value.size())));
} CYCatch(NULL) }

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, const sig::Type &type, ffi_type *ffi, JSObjectRef owner) {
    return CYPrivate<Pointer>::Make(context, pointer, type, context, owner);
}

static JSValueRef CYMakeFunctor(JSContextRef context, void (*function)(), bool variadic, const sig::Signature &signature) {
    if (function == NULL)
        return CYJSNull(context);
    return JSObjectMake(context, cy::Functor::Class_, new cy::Functor(function, variadic, signature));
}

// XXX: remove this, as it is really stupid
static JSObjectRef CYMakeFunctor(JSContextRef context, const char *symbol, const char *encoding) {
    void (*function)()(reinterpret_cast<void (*)()>(CYCastSymbol(symbol)));
    if (function == NULL)
        return NULL;

    cy::Functor *internal(new cy::Functor(function, encoding));
    ++internal->count_;
    return JSObjectMake(context, cy::Functor::Class_, internal);
}

bool CYGetOffset(CYPool &pool, JSContextRef context, JSStringRef value, ssize_t &index) {
    return CYGetOffset(CYPoolCString(pool, context, value), index);
}

// XXX: this is a horrible back added for CFType
void *CYCastPointerEx_(JSContextRef context, JSObjectRef value) {
    JSObjectRef object((JSObjectRef) value);
    if (JSValueIsObjectOfClass(context, value, CYPrivate<Pointer>::Class_)) {
        Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
        return internal->value_;
    }

    JSValueRef toPointer(CYGetProperty(context, object, toPointer_s));
    if (CYIsCallable(context, toPointer)) {
        JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toPointer, object, 0, NULL));
        _assert(value != NULL);
        return CYCastPointer_(context, value);
    }

    return NULL;
}

void *CYCastPointer_(JSContextRef context, JSValueRef value, bool *guess) {
    if (value == NULL)
        return NULL;
    else switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return NULL;
        case kJSTypeObject: {
            JSObjectRef object((JSObjectRef) value);
            if (JSValueIsObjectOfClass(context, value, CYPrivate<Pointer>::Class_)) {
                Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
                return internal->value_;
            }
            JSValueRef toPointer(CYGetProperty(context, object, toPointer_s));
            if (CYIsCallable(context, toPointer)) {
                JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toPointer, object, 0, NULL));
                _assert(value != NULL);
                return CYCastPointer_(context, value, guess);
            }
        } default:
            if (guess != NULL)
                *guess = true;
        case kJSTypeNumber:
            double number(CYCastDouble(context, value));
            if (!std::isnan(number))
                return reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<long long>(number)));
            if (guess == NULL)
                throw CYJSError(context, "cannot convert value to pointer");
            else {
                *guess = true;
                return NULL;
            }
    }
}

static JSValueRef FunctionAdapter_(JSContextRef context, size_t count, JSValueRef values[], JSObjectRef function);

namespace sig {

// XXX: this is somehow not quite a template :/

template <>
void Primitive<bool>::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    *reinterpret_cast<bool *>(data) = JSValueToBoolean(context, value);
}

#define CYPoolFFI_(Type_) \
template <> \
void Primitive<Type_>::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const { \
    *reinterpret_cast<Type_ *>(data) = CYCastDouble(context, value); \
}

CYPoolFFI_(wchar_t)
CYPoolFFI_(float)
CYPoolFFI_(double)
CYPoolFFI_(long double)

CYPoolFFI_(signed char)
CYPoolFFI_(signed int)
CYPoolFFI_(signed long int)
CYPoolFFI_(signed long long int)
CYPoolFFI_(signed short int)

CYPoolFFI_(unsigned char)
CYPoolFFI_(unsigned int)
CYPoolFFI_(unsigned long int)
CYPoolFFI_(unsigned long long int)
CYPoolFFI_(unsigned short int)

#ifdef __SIZEOF_INT128__
CYPoolFFI_(signed __int128)
CYPoolFFI_(unsigned __int128)
#endif

template <>
void Primitive<char>::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    if (JSValueGetType(context, value) != kJSTypeString)
        *reinterpret_cast<char *>(data) = CYCastDouble(context, value);
    else {
        CYJSString script(context, value);
        auto string(CYCastUTF16String(script));
        _assert(string.size == 1);
        _assert((string.data[0] & 0xff) == string.data[0]);
        *reinterpret_cast<char *>(data) = string.data[0];
    }
}

void Void::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    _assert(JSValueIsUndefined(context, value));
}

void Unknown::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    _assert(false);
}

void String::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    bool guess(false);
    *reinterpret_cast<const char **>(data) = CYCastPointer<const char *>(context, value, &guess);
    if (guess && pool != NULL)
        *reinterpret_cast<const char **>(data) = CYPoolCString(*pool, context, value);
}

void Bits::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    _assert(false);
}

static void CYArrayCopy(CYPool *pool, JSContextRef context, uint8_t *base, size_t length, const sig::Type &type, ffi_type *ffi, JSObjectRef object) {
    for (size_t index(0); index != length; ++index) {
        JSValueRef rhs(CYGetProperty(context, object, index));
        if (JSValueIsUndefined(context, rhs))
            throw CYJSError(context, "unable to extract array value");
        type.PoolFFI(pool, context, ffi, base, rhs);
        base += ffi->size;
    }
}

void Pointer::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    bool guess(false);
    *reinterpret_cast<void **>(data) = CYCastPointer<void *>(context, value, &guess);
    if (!guess || pool == NULL || !JSValueIsObject(context, value))
        return;

    JSObjectRef object(CYCastJSObject(context, value));

    if (sig::Function *function = dynamic_cast<sig::Function *>(&type)) {
        _assert(!function->variadic);
        auto internal(CYMakeFunctor_(context, object, function->signature, &FunctionAdapter_));
        // XXX: see notes in Library.cpp about needing to leak
        *reinterpret_cast<void (**)()>(data) = internal->value_;
    } else if (CYHasProperty(context, object, length_s)) {
        size_t length(CYArrayLength(context, object));
        ffi_type *element(type.GetFFI(*pool));
        size_t size(element->size * length);
        uint8_t *base(pool->malloc<uint8_t>(size, element->alignment));
        CYArrayCopy(pool, context, base, length, type, element, object);
        *reinterpret_cast<void **>(data) = base;
    }
}

void Array::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    if (size == 0)
        return;
    uint8_t *base(reinterpret_cast<uint8_t *>(data));
    CYArrayCopy(pool, context, base, size, type, ffi->elements[0], CYCastJSObject(context, value));
}

void Enum::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    return type.PoolFFI(pool, context, ffi, data, value);
}

void Aggregate::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    _assert(!overlap);
    _assert(signature.count != _not(size_t));

    size_t offset(0);
    uint8_t *base(reinterpret_cast<uint8_t *>(data));
    JSObjectRef aggregate(JSValueIsObject(context, value) ? (JSObjectRef) value : NULL);
    for (size_t index(0); index != signature.count; ++index) {
        sig::Element *element(&signature.elements[index]);
        ffi_type *field(ffi->elements[index]);

        JSValueRef rhs;
        if (aggregate == NULL)
            rhs = value;
        else {
            rhs = CYGetProperty(context, aggregate, index);
            if (JSValueIsUndefined(context, rhs)) {
                if (element->name != NULL)
                    rhs = CYGetProperty(context, aggregate, CYJSString(element->name));
                else
                    goto undefined;
                if (JSValueIsUndefined(context, rhs)) undefined:
                    throw CYJSError(context, "unable to extract structure value");
            }
        }

        element->type->PoolFFI(pool, context, field, base + offset, rhs);
        offset += field->size;
        CYAlign(offset, field->alignment);
    }
}

void Function::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    _assert(false);
}

// XXX: this code is getting worse, not better :/

#define CYFromFFI_(Type_) \
template <> \
JSValueRef Primitive<Type_>::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const { \
    JSValueRef value(CYCastJSValue(context, *reinterpret_cast<Type_ *>(data))); \
    JSObjectRef typed(_jsccall(JSObjectCallAsConstructor, context, CYGetCachedObject(context, CYJSString("Number")), 1, &value)); \
    CYSetProperty(context, typed, cyt__s, CYMakeType(context, *this), kJSPropertyAttributeDontEnum); \
    return typed; \
}

#define CYFromFFI_2(Type_) \
template <> \
JSValueRef Primitive<Type_>::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const { \
    return CYCastJSValue(context, *reinterpret_cast<Type_ *>(data)); \
}

CYFromFFI_(wchar_t)
CYFromFFI_(float)
CYFromFFI_(double)
CYFromFFI_(long double)

CYFromFFI_2(signed char)
CYFromFFI_2(signed int)
CYFromFFI_(signed long int)
CYFromFFI_(signed long long int)
CYFromFFI_2(signed short int)

CYFromFFI_2(unsigned char)
CYFromFFI_2(unsigned int)
CYFromFFI_(unsigned long int)
CYFromFFI_(unsigned long long int)
CYFromFFI_2(unsigned short int)

#ifdef __SIZEOF_INT128__
CYFromFFI_(signed __int128)
CYFromFFI_(unsigned __int128)
#endif

template <>
JSValueRef Primitive<bool>::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return CYCastJSValue(context, *reinterpret_cast<bool *>(data));
}

template <>
JSValueRef Primitive<char>::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    uint16_t string(uint8_t(*reinterpret_cast<char *>(data)));
    JSValueRef value(CYCastJSValue(context, CYJSString(CYUTF16String(&string, 1))));
    JSObjectRef typed(_jsccall(JSObjectCallAsConstructor, context, CYGetCachedObject(context, CYJSString("String")), 1, &value));
    CYSetProperty(context, typed, cyt_s, CYMakeType(context, sig::Primitive<char>()), kJSPropertyAttributeDontEnum);
    CYSetPrototype(context, typed, CYGetCachedValue(context, CYJSString("Character_prototype")));
    return typed;
}

JSValueRef Void::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return CYJSUndefined(context);
}

JSValueRef Unknown::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    _assert(false);
}

JSValueRef String::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    if (char *value = *reinterpret_cast<char **>(data))
        return CYPrivate<CString>::Make(context, value, context, owner);
    return CYJSNull(context);
}

JSValueRef Bits::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    _assert(false);
}

JSValueRef Pointer::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    if (void *value = *reinterpret_cast<void **>(data))
        return CYMakePointer(context, value, type, NULL, owner);
    return CYJSNull(context);
}

JSValueRef Array::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return CYPrivate<CArray>::Make(context, data, size, type, ffi->elements[0], context, owner);
}

JSValueRef Enum::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return type.FromFFI(context, ffi, data, initialize, owner);
}

JSValueRef Aggregate::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    _assert(!overlap);
    _assert(signature.count != _not(size_t));
    return CYPrivate<Struct_privateData>::Make(context, data, *this, ffi, context, owner);
}

JSValueRef Function::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return CYMakeFunctor(context, reinterpret_cast<void (*)()>(data), variadic, signature);
}

}

void CYExecuteClosure(ffi_cif *cif, void *result, void **arguments, void *arg) {
    Closure_privateData *internal(reinterpret_cast<Closure_privateData *>(arg));

    JSContextRef context(internal->function_);

    size_t count(internal->cif_.nargs);
    JSValueRef values[count];

    for (size_t index(0); index != count; ++index)
        values[index] = internal->signature_.elements[1 + index].type->FromFFI(context, internal->cif_.arg_types[index], arguments[index]);

    JSValueRef value(internal->adapter_(context, count, values, internal->function_));
    internal->signature_.elements[0].type->PoolFFI(NULL, context, internal->cif_.rtype, result, value);
}

static JSValueRef FunctionAdapter_(JSContextRef context, size_t count, JSValueRef values[], JSObjectRef function) {
    return CYCallAsFunction(context, function, NULL, count, values);
}

#if defined(__APPLE__) && (defined(__arm__) || defined(__arm64__))
static void CYFreeFunctor(void *data) {
    ffi_closure_free(data);
}
#else
static void CYFreeFunctor(void *data) {
    _syscall(munmap(data, sizeof(ffi_closure)));
}
#endif

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const sig::Signature &signature, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef)) {
    // XXX: in case of exceptions this will leak
    Closure_privateData *internal(new Closure_privateData(context, function, adapter, signature));

#if defined(__APPLE__) && (defined(__arm__) || defined(__arm64__))
    void *executable;
    ffi_closure *writable(reinterpret_cast<ffi_closure *>(ffi_closure_alloc(sizeof(ffi_closure), &executable)));

    ffi_status status(ffi_prep_closure_loc(writable, &internal->cif_, &CYExecuteClosure, internal, executable));
    _assert(status == FFI_OK);

    internal->pool_->atexit(&CYFreeFunctor, writable);
    internal->value_ = reinterpret_cast<void (*)()>(executable);
#else
    ffi_closure *closure((ffi_closure *) _syscall(mmap(
        NULL, sizeof(ffi_closure),
        PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
        -1, 0
    )));

    ffi_status status(ffi_prep_closure(closure, &internal->cif_, &CYExecuteClosure, internal));
    _assert(status == FFI_OK);

    _syscall(mprotect(closure, sizeof(*closure), PROT_READ | PROT_EXEC));

    internal->pool_->atexit(&CYFreeFunctor, closure);
    internal->value_ = reinterpret_cast<void (*)()>(closure);
#endif

    return internal;
}

static JSObjectRef CYMakeFunctor(JSContextRef context, JSObjectRef function, const sig::Signature &signature) {
    Closure_privateData *internal(CYMakeFunctor_(context, function, signature, &FunctionAdapter_));
    JSObjectRef object(JSObjectMake(context, cy::Functor::Class_, internal));
    // XXX: see above notes about needing to leak
    JSValueProtect(CYGetJSContext(context), object);
    return object;
}

JSValueRef CYGetCachedValue(JSContextRef context, JSStringRef name) {
    return CYGetProperty(context, CYCastJSObject(context, CYGetProperty(context, CYGetGlobalObject(context), cy_s)), name);
}

JSObjectRef CYGetCachedObject(JSContextRef context, JSStringRef name) {
    return CYCastJSObject(context, CYGetCachedValue(context, name));
}

static JSValueRef CYMakeFunctor(JSContextRef context, JSValueRef value, bool variadic, const sig::Signature &signature) {
    JSObjectRef Function(CYGetCachedObject(context, CYJSString("Function")));

    bool function(_jsccall(JSValueIsInstanceOfConstructor, context, value, Function));
    if (function) {
        JSObjectRef function(CYCastJSObject(context, value));
        return CYMakeFunctor(context, function, signature);
    } else {
        void (*function)()(CYCastPointer<void (*)()>(context, value));
        return CYMakeFunctor(context, function, variadic, signature);
    }
}

static JSValueRef CString_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    CString *internal(reinterpret_cast<CString *>(JSObjectGetPrivate(object)));

    ssize_t offset;
    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        offset = 0;
    else if (!CYGetOffset(pool, context, property, offset))
        return NULL;

    sig::Primitive<char> type;
    return type.FromFFI(context, type.GetFFI(pool), internal->value_ + offset, false, NULL);
} CYCatch(NULL) }

static bool CString_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    CString *internal(reinterpret_cast<CString *>(JSObjectGetPrivate(object)));

    ssize_t offset;
    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        offset = 0;
    else if (!CYGetOffset(pool, context, property, offset))
        return false;

    sig::Primitive<char> type;
    type.PoolFFI(NULL, context, type.GetFFI(pool), internal->value_ + offset, value);
    return true;
} CYCatch(false) }

static bool Index_(CYPool &pool, JSContextRef context, Struct_privateData *internal, JSStringRef property, ssize_t &index, uint8_t *&base) {
    Type_privateData *typical(internal->type_);
    sig::Aggregate *type(static_cast<sig::Aggregate *>(typical->type_));
    if (type == NULL)
        return false;

    const char *name(CYPoolCString(pool, context, property));
    size_t length(strlen(name));
    double number(CYCastDouble(name, length));

    size_t count(type->signature.count);

    if (std::isnan(number)) {
        if (property == NULL)
            return false;

        sig::Element *elements(type->signature.elements);

        for (size_t local(0); local != count; ++local) {
            sig::Element *element(&elements[local]);
            if (element->name != NULL && strcmp(name, element->name) == 0) {
                index = local;
                goto base;
            }
        }

        return false;
    } else {
        index = static_cast<ssize_t>(number);
        if (index != number || index < 0 || static_cast<size_t>(index) >= count)
            return false;
    }

  base:
    ffi_type **elements(typical->GetFFI()->elements);

    size_t offset(0);
    for (ssize_t local(0); local != index; ++local) {
        offset += elements[local]->size;
        CYAlign(offset, elements[local + 1]->alignment);
    }

    base = reinterpret_cast<uint8_t *>(internal->value_) + offset;
    return true;
}

static void *Offset_(CYPool &pool, JSContextRef context, JSStringRef property, void *data, ffi_type *ffi) {
    ssize_t offset;
    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        offset = 0;
    else if (!CYGetOffset(pool, context, property, offset))
        return NULL;
    return reinterpret_cast<uint8_t *>(data) + ffi->size * offset;
}

static JSValueRef Offset_getProperty(CYPool &pool, JSContextRef context, JSStringRef property, void *data, Type_privateData *typical, JSObjectRef owner) {
    ffi_type *ffi(typical->GetFFI());
    void *base(Offset_(pool, context, property, data, ffi));
    if (base == NULL)
        return NULL;
    return typical->type_->FromFFI(context, ffi, base, false, owner);
}

static bool Offset_setProperty(CYPool &pool, JSContextRef context, JSStringRef property, void *data, Type_privateData *typical, JSValueRef value) {
    ffi_type *ffi(typical->GetFFI());
    void *base(Offset_(pool, context, property, data, ffi));
    if (base == NULL)
        return false;

    typical->type_->PoolFFI(NULL, context, ffi, base, value);
    return true;
}

static JSValueRef CArray_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    CArray *internal(reinterpret_cast<CArray *>(JSObjectGetPrivate(object)));
    if (JSStringIsEqual(property, length_s))
        return CYCastJSValue(context, internal->length_);
    Type_privateData *typical(internal->type_);
    JSObjectRef owner(internal->owner_ ?: object);
    return Offset_getProperty(pool, context, property, internal->value_, typical, owner);
} CYCatch(NULL) }

static bool CArray_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);
    return Offset_setProperty(pool, context, property, internal->value_, typical, value);
} CYCatch(false) }

static JSValueRef Pointer_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));

    Type_privateData *typical(internal->type_);

    if (sig::Function *function = dynamic_cast<sig::Function *>(typical->type_)) {
        if (!JSStringIsEqualToUTF8CString(property, "$cyi"))
            return NULL;
        return CYMakeFunctor(context, reinterpret_cast<void (*)()>(internal->value_), function->variadic, function->signature);
    }

    JSObjectRef owner(internal->owner_ ?: object);
    return Offset_getProperty(pool, context, property, internal->value_, typical, owner);
} CYCatch(NULL) }

static bool Pointer_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);
    return Offset_setProperty(pool, context, property, internal->value_, typical, value);
} CYCatch(false) }

static JSValueRef Struct_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(_this)));
    Type_privateData *typical(internal->type_);
    return CYMakePointer(context, internal->value_, *typical->type_, typical->ffi_, _this);
} CYCatch(NULL) }

static JSValueRef Struct_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    return CYMakeType(context, *internal->type_->type_);
} CYCatch(NULL) }

static JSValueRef Struct_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);
    sig::Aggregate *type(static_cast<sig::Aggregate *>(typical->type_));

    ssize_t index;
    uint8_t *base;

    if (!Index_(pool, context, internal, property, index, base))
        return NULL;

    JSObjectRef owner(internal->owner_ ?: object);

    return type->signature.elements[index].type->FromFFI(context, typical->GetFFI()->elements[index], base, false, owner);
} CYCatch(NULL) }

static bool Struct_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);
    sig::Aggregate *type(static_cast<sig::Aggregate *>(typical->type_));

    ssize_t index;
    uint8_t *base;

    if (!Index_(pool, context, internal, property, index, base))
        return false;

    type->signature.elements[index].type->PoolFFI(NULL, context, typical->GetFFI()->elements[index], base, value);
    return true;
} CYCatch(false) }

static void Struct_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);
    sig::Aggregate *type(static_cast<sig::Aggregate *>(typical->type_));

    if (type == NULL)
        return;

    size_t count(type->signature.count);
    sig::Element *elements(type->signature.elements);

    char number[32];

    for (size_t index(0); index != count; ++index) {
        const char *name;
        name = elements[index].name;

        if (name == NULL) {
            sprintf(number, "%zu", index);
            name = number;
        }

        JSPropertyNameAccumulatorAddName(names, CYJSString(name));
    }
}

static sig::Void Void_;
static sig::Pointer PointerToVoid_(Void_);

static sig::Type *CYGetType(CYPool &pool, JSContextRef context, JSValueRef value) {
    if (JSValueIsNull(context, value))
        return &PointerToVoid_;
    JSObjectRef object(CYCastJSObject(context, value));
    JSValueRef check(CYGetProperty(context, object, cyt_s));
    if (JSValueIsUndefined(context, check))
        CYThrow("could not infer type of argument '%s'", CYPoolCString(pool, context, value));
    JSObjectRef type(CYCastJSObject(context, check));
    _assert(JSValueIsObjectOfClass(context, type, CYPrivate<Type_privateData>::Class_));
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(type)));
    return internal->type_;
}

void CYCallFunction(CYPool &pool, JSContextRef context, ffi_cif *cif, void (*function)(), void *value, void **values) {
    ffi_call(cif, function, value, values);
}

JSValueRef CYCallFunction(CYPool &pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, bool variadic, const sig::Signature &signature, ffi_cif *cif, void (*function)()) {
    size_t have(setups + count);
    size_t need(signature.count - 1);

    if (have < need)
        throw CYJSError(context, "insufficient number of arguments to ffi function");

    ffi_cif corrected;
    sig::Element *elements(signature.elements);

    if (have > need) {
        if (!variadic)
            throw CYJSError(context, "exorbitant number of arguments to ffi function");

        elements = new (pool) sig::Element[have + 1];
        memcpy(elements, signature.elements, sizeof(sig::Element) * (need + 1));

        for (size_t index(need); index != have; ++index) {
            sig::Element &element(elements[index + 1]);
            element.name = NULL;
            element.offset = _not(size_t);
            element.type = CYGetType(pool, context, arguments[index - setups]);
        }

        sig::Signature extended;
        extended.elements = elements;
        extended.count = have + 1;
        sig::sig_ffi_cif(pool, signature.count, extended, &corrected);
        cif = &corrected;
    }

    void *values[have];
    memcpy(values, setup, sizeof(void *) * setups);

    for (size_t index(setups); index != have; ++index) {
        sig::Element &element(elements[index + 1]);
        ffi_type *ffi(cif->arg_types[index]);
        values[index] = pool.malloc<uint8_t>(ffi->size, ffi->alignment);
        element.type->PoolFFI(&pool, context, ffi, values[index], arguments[index - setups]);
    }

    CYBuffer buffer(context);
    uint8_t *value(buffer->malloc<uint8_t>(std::max<size_t>(cif->rtype->size, sizeof(ffi_arg)), std::max<size_t>(cif->rtype->alignment, alignof(ffi_arg))));

    void (*call)(CYPool &, JSContextRef, ffi_cif *, void (*)(), void *, void **) = &CYCallFunction;
    // XXX: this only supports one hook, but it is a bad idea anyway
    for (CYHook *hook : GetHooks())
        if (hook->CallFunction != NULL)
            call = hook->CallFunction;

    call(pool, context, cif, function, value, values);
    return signature.elements[0].type->FromFFI(context, cif->rtype, value, initialize, buffer);
}

static JSValueRef Functor_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    return CYCallFunction(pool, context, 0, NULL, count, arguments, false, internal->variadic_, internal->signature_, &internal->cif_, internal->value_);
} CYCatch(NULL) }

static JSValueRef Pointer_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    if (dynamic_cast<sig::Function *>(internal->type_->type_) == NULL)
        throw CYJSError(context, "cannot call a pointer to non-function");
    JSObjectRef functor(CYCastJSObject(context, CYGetProperty(context, object, cyi_s)));
    return CYCallAsFunction(context, functor, _this, count, arguments);
} CYCatch(NULL) }

JSObjectRef CYMakeType(JSContextRef context, const sig::Type &type) {
    return CYPrivate<Type_privateData>::Make(context, type);
}

extern "C" bool CYBridgeHash(CYPool &pool, CYUTF8String name, const char *&code, unsigned &flags) {
    sqlite3_stmt *statement;

    _sqlcall(sqlite3_prepare(database_,
        "select "
            "\"cache\".\"code\", "
            "\"cache\".\"flags\" "
        "from \"cache\" "
        "where"
            " \"cache\".\"system\" & " CY_SYSTEM " == " CY_SYSTEM " and"
            " \"cache\".\"name\" = ?"
        " limit 1"
    , -1, &statement, NULL));

    _sqlcall(sqlite3_bind_text(statement, 1, name.data, name.size, SQLITE_STATIC));

    bool success;
    if (_sqlcall(sqlite3_step(statement)) == SQLITE_DONE)
        success = false;
    else {
        success = true;
        code = sqlite3_column_pooled(pool, statement, 0);
        flags = sqlite3_column_int(statement, 1);
    }

    _sqlcall(sqlite3_finalize(statement));
    return success;
}

static bool All_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    if (JSStringIsEqualToUTF8CString(property, "errno"))
        return true;

    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    for (size_t i(0), count(CYArrayLength(context, alls)); i != count; ++i)
        if (JSObjectRef space = CYCastJSObject(context, CYArrayGet(context, alls, count - i - 1)))
            if (CYHasProperty(context, space, property))
                return true;

    CYPool pool;
    const char *code;
    unsigned flags;
    if (CYBridgeHash(pool, CYPoolUTF8String(pool, context, property), code, flags))
        return true;

    return false;
}

static JSValueRef All_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    if (JSStringIsEqualToUTF8CString(property, "errno"))
        return CYCastJSValue(context, errno);

    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    for (size_t i(0), count(CYArrayLength(context, alls)); i != count; ++i)
        if (JSObjectRef space = CYCastJSObject(context, CYArrayGet(context, alls, count - i - 1)))
            if (JSValueRef value = CYGetProperty(context, space, property))
                if (!JSValueIsUndefined(context, value))
                    return value;

    CYPool pool;
    const char *code;
    unsigned flags;
    if (CYBridgeHash(pool, CYPoolUTF8String(pool, context, property), code, flags)) {
        CYUTF8String parsed;

        try {
            parsed = CYPoolCode(pool, code);
        } catch (const CYException &error) {
            CYThrow("%s", pool.strcat("error caching ", CYPoolCString(pool, context, property), ": ", error.PoolCString(pool), NULL));
        }

        JSObjectRef cache(CYGetCachedObject(context, CYJSString("cache")));

        JSObjectRef stub;
        if (flags == CYBridgeType) {
            stub = CYMakeType(context, sig::Void());
            CYSetProperty(context, cache, property, stub);
        } else
            stub = NULL;

        JSValueRef value(_jsccall(JSEvaluateScript, context, CYJSString(parsed), NULL, NULL, 0));

        switch (flags) {
            case CYBridgeVoid: {
            } break;

            case CYBridgeHold: {
                CYSetProperty(context, cache, property, value);
            } break;

            case CYBridgeType: {
                JSObjectRef swap(CYCastJSObject(context, value));
                void *source(JSObjectGetPrivate(swap));
                _assert(source != NULL);
                void *target(JSObjectGetPrivate(stub));
                _assert(JSObjectSetPrivate(swap, target));
                _assert(JSObjectSetPrivate(stub, source));
                value = stub;
            } break;
        }

        return value;
    }

    return NULL;
} CYCatch(NULL) }

static JSValueRef All_complete_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    _assert(count == 1 || count == 2);
    CYPool pool;
    CYUTF8String prefix(CYPoolUTF8String(pool, context, CYJSString(context, arguments[0])));

    JSObjectRef array(NULL);

    {
    CYArrayBuilder<1024> values(context, array);

    sqlite3_stmt *statement;

    if (prefix.size == 0)
        _sqlcall(sqlite3_prepare(database_,
            "select "
                "\"cache\".\"name\" "
            "from \"cache\" "
            "where"
                " \"cache\".\"system\" & " CY_SYSTEM " == " CY_SYSTEM
        , -1, &statement, NULL));
    else {
        _sqlcall(sqlite3_prepare(database_,
            "select "
                "\"cache\".\"name\" "
            "from \"cache\" "
            "where"
                " \"cache\".\"name\" >= ? and \"cache\".\"name\" < ? and "
                " \"cache\".\"system\" & " CY_SYSTEM " == " CY_SYSTEM
        , -1, &statement, NULL));

        _sqlcall(sqlite3_bind_text(statement, 1, prefix.data, prefix.size, SQLITE_STATIC));

        char *after(pool.strndup(prefix.data, prefix.size));
        ++after[prefix.size - 1];
        _sqlcall(sqlite3_bind_text(statement, 2, after, prefix.size, SQLITE_STATIC));
    }

    while (_sqlcall(sqlite3_step(statement)) != SQLITE_DONE)
        values(CYCastJSValue(context, CYJSString(sqlite3_column_string(statement, 0))));

    _sqlcall(sqlite3_finalize(statement));
    }

    return array;
} CYCatch(NULL) }

static void All_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    for (size_t i(0), count(CYArrayLength(context, alls)); i != count; ++i)
        if (JSObjectRef space = CYCastJSObject(context, CYArrayGet(context, alls, count - i - 1))) {
            JSPropertyNameArrayRef subset(JSObjectCopyPropertyNames(context, space));
            for (size_t index(0), count(JSPropertyNameArrayGetCount(subset)); index != count; ++index)
                JSPropertyNameAccumulatorAddName(names, JSPropertyNameArrayGetNameAtIndex(subset, index));
            JSPropertyNameArrayRelease(subset);
        }
}

static JSObjectRef CArray_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    _assert(false);
} CYCatchObject() }

static JSObjectRef CString_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    _assert(false);
} CYCatchObject() }

static JSObjectRef Pointer_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    _assert(false);
} CYCatchObject() }

static JSObjectRef Type_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;

    if (false) {
    } else if (count == 1) {
        switch (JSValueGetType(context, arguments[0])) {
            case kJSTypeString: {
                const char *encoding(CYPoolCString(pool, context, arguments[0]));
                sig::Signature signature;
                sig::Parse(pool, &signature, encoding, &Structor_);
                return CYMakeType(context, *signature.elements[0].type);
            } break;

            case kJSTypeObject: {
                // XXX: accept a set of enum constants and /guess/ at their size
                _assert(false);
            } break;

            default:
                throw CYJSError(context, "incorrect kind of argument to new Type");
        }
    } else if (count == 2) {
        JSObjectRef types(CYCastJSObject(context, arguments[0]));
        size_t count(CYArrayLength(context, types));

        JSObjectRef names(CYCastJSObject(context, arguments[1]));

        sig::Aggregate type(false);
        type.signature.elements = new(pool) sig::Element[count];
        type.signature.count = count;

        for (size_t i(0); i != count; ++i) {
            sig::Element &element(type.signature.elements[i]);
            element.offset = _not(size_t);

            JSValueRef name(CYArrayGet(context, names, i));
            if (JSValueIsUndefined(context, name))
                element.name = NULL;
            else
                element.name = CYPoolCString(pool, context, name);

            JSObjectRef object(CYCastJSObject(context, CYArrayGet(context, types, i)));
            _assert(JSValueIsObjectOfClass(context, object, CYPrivate<Type_privateData>::Class_));
            Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
            element.type = internal->type_;
            _assert(element.type != NULL);
        }

        return CYMakeType(context, type);
    } else {
        throw CYJSError(context, "incorrect number of arguments to Type constructor");
    }
} CYCatchObject() }

static JSValueRef Type_callAsFunction_$With(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], sig::Callable &type, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;

    type.signature.elements = new(pool) sig::Element[1 + count];
    type.signature.count = 1 + count;

    type.signature.elements[0].name = NULL;
    type.signature.elements[0].type = internal->type_;
    type.signature.elements[0].offset = _not(size_t);

    for (size_t i(0); i != count; ++i) {
        sig::Element &element(type.signature.elements[i + 1]);
        element.name = NULL;
        element.offset = _not(size_t);

        JSObjectRef object(CYCastJSObject(context, arguments[i]));
        _assert(JSValueIsObjectOfClass(context, object, CYPrivate<Type_privateData>::Class_));
        Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

        element.type = internal->type_;
    }

    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_arrayOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Type.arrayOf");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;
    size_t index(CYGetIndex(pool, context, CYJSString(context, arguments[0])));
    if (index == _not(size_t))
        throw CYJSError(context, "invalid array size used with Type.arrayOf");

    sig::Array type(*internal->type_, index);
    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_blockWith(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
#ifdef CY_OBJECTIVEC
    sig::Block type;
    return Type_callAsFunction_$With(context, object, _this, count, arguments, type, exception);
#else
    _assert(false);
#endif
}

static JSValueRef Type_callAsFunction_constant(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.constant");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;
    sig::Type *type(internal->type_->Copy(pool));
    type->flags |= JOC_TYPE_CONST;
    return CYMakeType(context, *type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_enumFor(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Type.enumFor");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;

    JSObjectRef constants(CYCastJSObject(context, arguments[0]));

    // XXX: this is, sadly, going to leak
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context, constants));

    size_t count(JSPropertyNameArrayGetCount(names));

    sig::Enum type(*internal->type_, count);
    type.constants = new(pool) sig::Constant[count];

    for (size_t index(0); index != count; ++index) {
        JSStringRef name(JSPropertyNameArrayGetNameAtIndex(names, index));
        JSValueRef value(CYGetProperty(context, constants, name));
        _assert(JSValueGetType(context, value) == kJSTypeNumber);
        CYUTF8String string(CYPoolUTF8String(pool, context, name));
        type.constants[index].name = string.data;
        type.constants[index].value = CYCastDouble(context, value);
    }

    JSPropertyNameArrayRelease(names);

    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_functionWith(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    bool variadic(count != 0 && JSValueIsNull(context, arguments[count - 1]));
    sig::Function type(variadic);
    return Type_callAsFunction_$With(context, object, _this, variadic ? count - 1 : count, arguments, type, exception);
}

static JSValueRef Type_callAsFunction_pointerTo(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.pointerTo");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    if (dynamic_cast<sig::Primitive<char> *>(internal->type_) != NULL)
        return CYMakeType(context, sig::String((internal->type_->flags & JOC_TYPE_CONST) != 0));
    else
        return CYMakeType(context, sig::Pointer(*internal->type_));
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_withName(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Type.withName");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;
    return CYMakeType(context, *internal->type_->Copy(pool, CYPoolCString(pool, context, arguments[0])));
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to type cast function");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

    if (sig::Function *function = dynamic_cast<sig::Function *>(internal->type_))
        return CYMakeFunctor(context, arguments[0], function->variadic, function->signature);

    CYBuffer buffer(context);

    sig::Type *type(internal->type_);
    ffi_type *ffi(internal->GetFFI());

    void *data;
    if (_this == NULL || CYIsStrictEqual(context, _this, CYGetGlobalObject(context)))
        data = buffer->malloc<void>(ffi->size, ffi->alignment);
    else {
        CYSetProperty(context, buffer, CYJSString("$cyo"), _this, kJSPropertyAttributeDontEnum);
        data = CYCastPointer<void *>(context, _this);
    }

    type->PoolFFI(buffer, context, ffi, data, arguments[0]);
    JSValueRef value(type->FromFFI(context, ffi, data, false, buffer));
    return value;
} CYCatch(NULL) }

static JSObjectRef Type_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count > 1)
        throw CYJSError(context, "incorrect number of arguments to Type allocator");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

    JSObjectRef pointer(CYMakePointer(context, NULL, *internal->type_, NULL, NULL));
    Pointer *value(reinterpret_cast<Pointer *>(JSObjectGetPrivate(pointer)));

    sig::Type *type(internal->type_);
    ffi_type *ffi(internal->GetFFI());
    value->value_ = value->pool_->malloc<void>(ffi->size, ffi->alignment);

    if (count == 0)
        memset(value->value_, 0, ffi->size);
    else
        type->PoolFFI(value->pool_, context, ffi, value->value_, arguments[0]);

    return pointer;
} CYCatchObject() }

// XXX: I don't even think the user should be allowed to do this
static JSObjectRef Functor_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to Functor constructor");
    CYPool pool;
    const char *encoding(CYPoolCString(pool, context, arguments[1]));
    sig::Signature signature;
    sig::Parse(pool, &signature, encoding, &Structor_);
    // XXX: this can try to return null, and I guess then it just fails
    return CYCastJSObject(context, CYMakeFunctor(context, arguments[0], false, signature));
} CYCatchObject() }

static JSValueRef CArray_callAsFunction_toPointer(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CArray *internal(reinterpret_cast<CArray *>(JSObjectGetPrivate(_this)));
    JSObjectRef owner(internal->owner_ ?: object);
    return CYMakePointer(context, internal->value_, *internal->type_->type_, NULL, owner);
} CYCatch(NULL) }

static JSValueRef CString_callAsFunction_toPointer(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CString *internal(reinterpret_cast<CString *>(JSObjectGetPrivate(_this)));
    JSObjectRef owner(internal->owner_ ?: object);
    return CYMakePointer(context, internal->value_, sig::Primitive<char>(), NULL, owner);
} CYCatch(NULL) }

static JSValueRef Functor_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(_this)));

    sig::Function type(internal->variadic_);
    sig::Copy(pool, type.signature, internal->signature_);

    return CYMakePointer(context, reinterpret_cast<void *>(internal->value_), type, NULL, NULL);
} CYCatch(NULL) }

static JSValueRef Pointer_callAsFunction_toPointer(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    return _this;
} CYCatch(NULL) }

static JSValueRef CArray_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CArray *internal(reinterpret_cast<CArray *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
} CYCatch(NULL) }

static JSValueRef Pointer_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
} CYCatch(NULL) }

static JSValueRef Functor_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
} CYCatch(NULL) }

static JSValueRef Functor_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(_this)));
    uint8_t *value(reinterpret_cast<uint8_t *>(internal->value_));
    _assert(value != NULL);

    CYLocalPool pool;

    sig::Function function(internal->variadic_);
    sig::Copy(pool, function.signature, internal->signature_);

    CYPropertyName *name(internal->GetName(pool));
    auto typed(CYDecodeType(pool, &function));

    std::ostringstream str;
    CYOptions options;
    CYOutput output(*str.rdbuf(), options);
    output.pretty_ = true;
    (new(pool) CYExternalExpression(new(pool) CYString("C"), typed, name))->Output(output, CYNoFlags);
    return CYCastJSValue(context, CYJSString(str.str()));
} CYCatch(NULL) }

CYPropertyName *cy::Functor::GetName(CYPool &pool) const {
    Dl_info info;
    if (dladdr(reinterpret_cast<void *>(value_), &info) == 0 || strcmp(info.dli_sname, "<redacted>") == 0)
        return new(pool) CYNumber(reinterpret_cast<uintptr_t>(value_));

    std::ostringstream str;
    str << info.dli_sname;
    off_t offset(reinterpret_cast<uint8_t *>(value_) - reinterpret_cast<uint8_t *>(info.dli_saddr));
    if (offset != 0)
        str << "+0x" << std::hex << offset;
    return new(pool) CYString(pool.strdup(str.str().c_str()));
}

static JSValueRef Pointer_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    std::set<void *> *objects(CYCastObjects(context, _this, count, arguments));

    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(_this)));

    try {
        JSValueRef value(CYGetProperty(context, _this, cyi_s));
        if (!JSValueIsUndefined(context, value)) {
            CYPool pool;
            return CYCastJSValue(context, pool.strcat("&", CYPoolCCYON(pool, context, value, objects), NULL));
        }
    } catch (const CYException &e) {
        // XXX: it might be interesting to include this error
    }

    CYLocalPool pool;
    std::ostringstream str;

    sig::Pointer type(*internal->type_->type_);

    CYOptions options;
    CYOutput output(*str.rdbuf(), options);
    (new(pool) CYTypeExpression(CYDecodeType(pool, &type)))->Output(output, CYNoFlags);

    str << "(" << internal->value_ << ")";
    std::string value(str.str());
    return CYCastJSValue(context, CYJSString(CYUTF8String(value.c_str(), value.size())));
} CYCatch(NULL) }

static JSValueRef CString_getProperty_length(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CString *internal(reinterpret_cast<CString *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, strlen(internal->value_));
} CYCatch(NULL) }

static JSValueRef CString_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    return CYMakeType(context, sig::String(true));
} CYCatch(NULL) }

static JSValueRef CArray_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CArray *internal(reinterpret_cast<CArray *>(JSObjectGetPrivate(object)));
    sig::Array type(*internal->type_->type_, internal->length_);
    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef Pointer_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    sig::Pointer type(*internal->type_->type_);
    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef CString_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CString *internal(reinterpret_cast<CString *>(JSObjectGetPrivate(_this)));
    const char *string(internal->value_);
    std::ostringstream str;
    if (string == NULL)
        str << "NULL";
    else {
        str << "&";
        CYStringify(str, string, strlen(string), CYStringifyModeNative);
    }
    std::string value(str.str());
    return CYCastJSValue(context, CYJSString(CYUTF8String(value.c_str(), value.size())));
} CYCatch(NULL) }

static JSValueRef CString_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CString *internal(reinterpret_cast<CString *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, internal->value_);
} CYCatch(NULL) }

static JSValueRef Functor_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    CYPool pool;
    sig::Function type(internal->variadic_);
    sig::Copy(pool, type.signature, internal->signature_);
    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef Type_getProperty_alignment(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->GetFFI()->alignment);
} CYCatch(NULL) }

static JSValueRef Type_getProperty_name(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->type_->GetName());
} CYCatch(NULL) }

static JSValueRef Type_getProperty_size(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->GetFFI()->size);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
    CYPool pool;
    const char *type(sig::Unparse(pool, internal->type_));
    return CYCastJSValue(context, CYJSString(type));
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
    CYLocalPool pool;
    std::stringbuf out;
    CYOptions options;
    CYOutput output(out, options);
    output.pretty_ = true;
    (new(pool) CYTypeExpression(CYDecodeType(pool, internal->type_->Copy(pool, ""))))->Output(output, CYNoFlags);
    return CYCastJSValue(context, CYJSString(out.str().c_str()));
} CYCatch(NULL) }

static JSStaticFunction All_staticFunctions[2] = {
    {"cy$complete", &All_complete_callAsFunction, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction CArray_staticFunctions[3] = {
    {"toPointer", &CArray_callAsFunction_toPointer, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &CArray_callAsFunction_valueOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue CArray_staticValues[2] = {
    {"$cyt", &CArray_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction CString_staticFunctions[5] = {
    {"toCYON", &CString_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toPointer", &CString_callAsFunction_toPointer, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &CString_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &CString_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue CString_staticValues[3] = {
    {"length", &CString_getProperty_length, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"$cyt", &CString_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Pointer_staticFunctions[4] = {
    {"toCYON", &Pointer_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toPointer", &Pointer_callAsFunction_toPointer, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &Pointer_callAsFunction_valueOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue Pointer_staticValues[2] = {
    {"$cyt", &Pointer_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Struct_staticFunctions[2] = {
    {"$cya", &Struct_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue Struct_staticValues[2] = {
    {"$cyt", &Struct_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Functor_staticFunctions[5] = {
    {"$cya", &Functor_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toCYON", &Functor_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toPointer", &Functor_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &Functor_callAsFunction_valueOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue Functor_staticValues[2] = {
    {"$cyt", &Functor_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticValue Type_staticValues[4] = {
    {"alignment", &Type_getProperty_alignment, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"name", &Type_getProperty_name, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"size", &Type_getProperty_size, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Type_staticFunctions[10] = {
    {"arrayOf", &Type_callAsFunction_arrayOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"blockWith", &Type_callAsFunction_blockWith, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"constant", &Type_callAsFunction_constant, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"enumFor", &Type_callAsFunction_enumFor, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"functionWith", &Type_callAsFunction_functionWith, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"pointerTo", &Type_callAsFunction_pointerTo, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"withName", &Type_callAsFunction_withName, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toCYON", &Type_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Type_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

_visible void CYSetArgs(const char *argv0, const char *script, int argc, const char *argv[]) {
    JSContextRef context(CYGetJSContext());
    JSValueRef args[argc + 2];
    for (int i(0); i != argc; ++i)
        args[i + 2] = CYCastJSValue(context, argv[i]);

    size_t offset;
    if (script == NULL)
        offset = 1;
    else {
        offset = 0;
        args[1] = CYCastJSValue(context, CYJSString(script));
    }

    args[offset] = CYCastJSValue(context, CYJSString(argv0));

    CYSetProperty(context, CYGetCachedObject(context, CYJSString("System")), CYJSString("args"), CYObjectMakeArray(context, argc, args + 2));
    CYSetProperty(context, CYGetCachedObject(context, CYJSString("process")), CYJSString("argv"), CYObjectMakeArray(context, argc + 2 - offset, args + offset));
}

JSObjectRef CYGetGlobalObject(JSContextRef context) {
    return JSContextGetGlobalObject(context);
}

// XXX: this is neither exceptin safe nor even terribly sane
class ExecutionHandle {
  private:
    JSContextRef context_;
    std::vector<void *> handles_;

  public:
    ExecutionHandle(JSContextRef context) :
        context_(context)
    {
        handles_.resize(GetHooks().size());
        for (size_t i(0); i != GetHooks().size(); ++i) {
            CYHook *hook(GetHooks()[i]);
            if (hook->ExecuteStart != NULL)
                handles_[i] = (*hook->ExecuteStart)(context_);
            else
                handles_[i] = NULL;
        }
    }

    ~ExecutionHandle() {
        for (size_t i(GetHooks().size()); i != 0; --i) {
            CYHook *hook(GetHooks()[i-1]);
            if (hook->ExecuteEnd != NULL)
                (*hook->ExecuteEnd)(context_, handles_[i-1]);
        }
    }
};

#ifndef __ANDROID__
static volatile bool cancel_;

static bool CYShouldTerminate(JSContextRef context, void *arg) {
    return cancel_;
}
#endif

_visible const char *CYExecute(JSContextRef context, CYPool &pool, CYUTF8String code) {
    ExecutionHandle handle(context);

#ifndef __ANDROID__
    cancel_ = false;
    if (&JSContextGroupSetExecutionTimeLimit != NULL)
        JSContextGroupSetExecutionTimeLimit(JSContextGetGroup(context), 0.5, &CYShouldTerminate, NULL);
#endif

    try {
        JSValueRef result(_jsccall(JSEvaluateScript, context, CYJSString(code), NULL, NULL, 0));
        if (JSValueIsUndefined(context, result))
            return NULL;

        std::set<void *> objects;
        const char *json(_jsccall(CYPoolCCYON, pool, context, result, objects));
        CYSetProperty(context, CYGetGlobalObject(context), Result_, result);

        return json;
    } catch (const CYException &error) {
        return pool.strcat("throw ", error.PoolCString(pool), NULL);
    }
}

#ifndef __ANDROID__
_visible void CYCancel() {
    cancel_ = true;
}
#endif

const char *CYPoolLibraryPath(CYPool &pool);

static bool initialized_ = false;

void CYInitializeDynamic() {
    if (!initialized_)
        initialized_ = true;
    else return;

    CYPool pool;
    const char *db(pool.strcat(CYPoolLibraryPath(pool), "/libcycript.db", NULL));
    _sqlcall(sqlite3_open_v2(db, &database_, SQLITE_OPEN_READONLY, NULL));

    JSObjectMakeArray$ = reinterpret_cast<JSObjectRef (*)(JSContextRef, size_t, const JSValueRef[], JSValueRef *)>(dlsym(RTLD_DEFAULT, "JSObjectMakeArray"));
    JSSynchronousGarbageCollectForDebugging$ = reinterpret_cast<void (*)(JSContextRef)>(dlsym(RTLD_DEFAULT, "JSSynchronousGarbageCollectForDebugging"));

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "All";
    definition.staticFunctions = All_staticFunctions;
    definition.hasProperty = &All_hasProperty;
    definition.getProperty = &All_getProperty;
    definition.getPropertyNames = &All_getPropertyNames;
    All_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Context";
    definition.finalize = &CYFinalize;
    CYPrivate<Context>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "CArray";
    definition.staticFunctions = CArray_staticFunctions;
    definition.staticValues = CArray_staticValues;
    definition.getProperty = &CArray_getProperty;
    definition.setProperty = &CArray_setProperty;
    definition.finalize = &CYFinalize;
    CYPrivate<CArray>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "CString";
    definition.staticFunctions = CString_staticFunctions;
    definition.staticValues = CString_staticValues;
    definition.getProperty = &CString_getProperty;
    definition.setProperty = &CString_setProperty;
    definition.finalize = &CYFinalize;
    CYPrivate<CString>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Functor";
    definition.staticFunctions = Functor_staticFunctions;
    definition.staticValues = Functor_staticValues;
    definition.callAsFunction = &Functor_callAsFunction;
    definition.finalize = &CYFinalize;
    cy::Functor::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Pointer";
    definition.staticFunctions = Pointer_staticFunctions;
    definition.staticValues = Pointer_staticValues;
    definition.callAsFunction = &Pointer_callAsFunction;
    definition.getProperty = &Pointer_getProperty;
    definition.setProperty = &Pointer_setProperty;
    definition.finalize = &CYFinalize;
    CYPrivate<Pointer>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Root";
    definition.finalize = &CYFinalize;
    CYPrivate<CYRoot>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Struct";
    definition.staticFunctions = Struct_staticFunctions;
    definition.staticValues = Struct_staticValues;
    definition.getProperty = &Struct_getProperty;
    definition.setProperty = &Struct_setProperty;
    definition.getPropertyNames = &Struct_getPropertyNames;
    definition.finalize = &CYFinalize;
    CYPrivate<Struct_privateData>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Type";
    definition.staticValues = Type_staticValues;
    definition.staticFunctions = Type_staticFunctions;
    definition.callAsFunction = &Type_callAsFunction;
    definition.callAsConstructor = &Type_callAsConstructor;
    definition.finalize = &CYFinalize;
    CYPrivate<Type_privateData>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Global";
    //definition.getProperty = &Global_getProperty;
    Global_ = JSClassCreate(&definition);

    Array_s = JSStringCreateWithUTF8CString("Array");
    constructor_s = JSStringCreateWithUTF8CString("constructor");
    cy_s = JSStringCreateWithUTF8CString("$cy");
    cyi_s = JSStringCreateWithUTF8CString("$cyi");
    cyt_s = JSStringCreateWithUTF8CString("$cyt");
    cyt__s = JSStringCreateWithUTF8CString("$cyt_");
    length_s = JSStringCreateWithUTF8CString("length");
    message_s = JSStringCreateWithUTF8CString("message");
    name_s = JSStringCreateWithUTF8CString("name");
    pop_s = JSStringCreateWithUTF8CString("pop");
    prototype_s = JSStringCreateWithUTF8CString("prototype");
    push_s = JSStringCreateWithUTF8CString("push");
    splice_s = JSStringCreateWithUTF8CString("splice");
    toCYON_s = JSStringCreateWithUTF8CString("toCYON");
    toJSON_s = JSStringCreateWithUTF8CString("toJSON");
    toPointer_s = JSStringCreateWithUTF8CString("toPointer");
    toString_s = JSStringCreateWithUTF8CString("toString");
    weak_s = JSStringCreateWithUTF8CString("weak");

    Result_ = JSStringCreateWithUTF8CString("_");

    for (CYHook *hook : GetHooks())
        if (hook->Initialize != NULL)
            (*hook->Initialize)();
}

void CYThrow(JSContextRef context, JSValueRef value) {
    if (value != NULL)
        throw CYJSError(context, value);
}

const char *CYJSError::PoolCString(CYPool &pool) const {
    std::set<void *> objects;
    // XXX: this used to be CYPoolCString
    return CYPoolCCYON(pool, context_, value_, objects);
}

JSValueRef CYJSError::CastJSValue(JSContextRef context, const char *name) const {
    // XXX: what if the context is different? or the name? I dunno. ("epic" :/)
    return value_;
}

JSValueRef CYCastJSError(JSContextRef context, const char *name, const char *message) {
    JSObjectRef Error(CYGetCachedObject(context, CYJSString(name)));
    JSValueRef arguments[1] = {CYCastJSValue(context, message)};
    return _jsccall(JSObjectCallAsConstructor, context, Error, 1, arguments);
}

JSValueRef CYPoolError::CastJSValue(JSContextRef context, const char *name) const {
    return CYCastJSError(context, name, message_);
}

CYJSError::CYJSError(JSContextRef context, const char *format, ...) {
    _assert(context != NULL);

    CYPool pool;

    va_list args;
    va_start(args, format);
    // XXX: there might be a beter way to think about this
    const char *message(pool.vsprintf(64, format, args));
    va_end(args);

    value_ = CYCastJSError(context, "Error", message);
}

JSGlobalContextRef CYGetJSContext(JSContextRef context) {
    return reinterpret_cast<Context *>(JSObjectGetPrivate(CYCastJSObject(context, CYGetProperty(context, CYGetGlobalObject(context), cy_s))))->context_;
}

#ifdef __ANDROID__
char *CYPoolLibraryPath_(CYPool &pool) {
    FILE *maps(fopen("/proc/self/maps", "r"));
    struct F { FILE *f; F(FILE *f) : f(f) {}
        ~F() { fclose(f); } } f(maps);

    size_t function(reinterpret_cast<size_t>(&CYPoolLibraryPath));

    for (;;) {
        size_t start; size_t end; char flags[8]; unsigned long long offset;
        int major; int minor; unsigned long long inode; char file[1024];
        int count(fscanf(maps, "%zx-%zx %7s %llx %x:%x %llu%[ ]%1024[^\n]\n",
            &start, &end, flags, &offset, &major, &minor, &inode, file, file));
        if (count < 8) break; else if (start <= function && function < end)
            return pool.strdup(file);
    }

    _assert(false);
}
#else
char *CYPoolLibraryPath_(CYPool &pool) {
    Dl_info addr;
    _assert(dladdr(reinterpret_cast<void *>(&CYPoolLibraryPath), &addr) != 0);
    return pool.strdup(addr.dli_fname);
}
#endif

const char *CYPoolLibraryPath(CYPool &pool) {
    char *lib(CYPoolLibraryPath_(pool));

    char *slash(strrchr(lib, '/'));
    if (slash == NULL)
        return ".";
    *slash = '\0';

    slash = strrchr(lib, '/');
    if (slash != NULL) {
        if (strcmp(slash, "/.libs") == 0)
            *slash = '\0';
    } else if (strcmp(lib, ".libs") == 0)
        return ".";

    return lib;
}

static JSValueRef require_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    _assert(count == 1);
    CYPool pool;

    CYUTF8String name(CYPoolUTF8String(pool, context, CYJSString(context, arguments[0])));
    if (memchr(name.data, '/', name.size) == NULL && (
#ifdef __APPLE__
        dlopen(pool.strcat("/System/Library/Frameworks/", name.data, ".framework/", name.data, NULL), RTLD_LAZY | RTLD_GLOBAL) != NULL ||
        dlopen(pool.strcat("/System/Library/PrivateFrameworks/", name.data, ".framework/", name.data, NULL), RTLD_LAZY | RTLD_GLOBAL) != NULL ||
#endif
    false))
        return CYJSUndefined(context);

    CYJSString path;
    CYUTF8String code;

    sqlite3_stmt *statement;

    _sqlcall(sqlite3_prepare(database_,
        "select "
            "\"module\".\"code\", "
            "\"module\".\"flags\" "
        "from \"module\" "
        "where"
            " \"module\".\"name\" = ?"
        " limit 1"
    , -1, &statement, NULL));

    _sqlcall(sqlite3_bind_text(statement, 1, name.data, name.size, SQLITE_STATIC));

    if (_sqlcall(sqlite3_step(statement)) != SQLITE_DONE) {
        code.data = static_cast<const char *>(sqlite3_column_blob(statement, 0));
        code.size = sqlite3_column_bytes(statement, 0);
        path = CYJSString(name);
        code = CYPoolUTF8String(pool, code);
    } else {
        JSObjectRef resolve(CYCastJSObject(context, CYGetProperty(context, object, CYJSString("resolve"))));
        path = CYJSString(context, CYCallAsFunction(context, resolve, NULL, 1, arguments));
    }

    _sqlcall(sqlite3_finalize(statement));

    CYJSString property("exports");

    JSObjectRef modules(CYGetCachedObject(context, CYJSString("modules")));
    JSValueRef cache(CYGetProperty(context, modules, path));

    JSValueRef result;
    if (!JSValueIsUndefined(context, cache)) {
        JSObjectRef module(CYCastJSObject(context, cache));
        result = CYGetProperty(context, module, property);
    } else {
        if (code.data == NULL) {
            code = CYPoolFileUTF8String(pool, CYPoolCString(pool, context, path));
            _assert(code.data != NULL);
        }

        size_t length(name.size);
        if (length >= 5 && strncmp(name.data + length - 5, ".json", 5) == 0) {
            JSObjectRef JSON(CYGetCachedObject(context, CYJSString("JSON")));
            JSObjectRef parse(CYCastJSObject(context, CYGetProperty(context, JSON, CYJSString("parse"))));
            JSValueRef arguments[1] = { CYCastJSValue(context, CYJSString(code)) };
            result = CYCallAsFunction(context, parse, JSON, 1, arguments);
        } else {
            JSObjectRef module(JSObjectMake(context, NULL, NULL));
            CYSetProperty(context, modules, path, module);

            JSObjectRef exports(JSObjectMake(context, NULL, NULL));
            CYSetProperty(context, module, property, exports);

            std::stringstream wrap;
            wrap << "(function (exports, require, module, __filename) { " << code << "\n});";
            code = CYPoolCode(pool, *wrap.rdbuf());

            JSValueRef value(_jsccall(JSEvaluateScript, context, CYJSString(code), NULL, NULL, 0));
            JSObjectRef function(CYCastJSObject(context, value));

            JSValueRef arguments[4] = { exports, object, module, CYCastJSValue(context, path) };
            CYCallAsFunction(context, function, NULL, 4, arguments);
            result = CYGetProperty(context, module, property);
        }
    }

    return result;
} CYCatch(NULL) }

static bool CYRunScript(JSGlobalContextRef context, const char *path) {
    CYPool pool;
    CYUTF8String code(CYPoolFileUTF8String(pool, pool.strcat(CYPoolLibraryPath(pool), path, NULL)));
    if (code.data == NULL)
        return false;

    code = CYPoolCode(pool, code);
    _jsccall(JSEvaluateScript, context, CYJSString(code), NULL, NULL, 0);
    return true;
}

extern "C" void CYDestroyWeak(JSWeakObjectMapRef weak, void *data) {
}

extern "C" void CYSetupContext(JSGlobalContextRef context) {
    CYInitializeDynamic();

    JSObjectRef global(CYGetGlobalObject(context));

    JSObjectRef cy(CYPrivate<Context>::Make(context, context));
    CYSetProperty(context, global, cy_s, cy, kJSPropertyAttributeDontEnum);

/* Cache Globals {{{ */
    JSObjectRef Array(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Array"))));
    CYSetProperty(context, cy, CYJSString("Array"), Array);

    JSObjectRef Array_prototype(CYCastJSObject(context, CYGetProperty(context, Array, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Array_prototype"), Array_prototype);

    JSObjectRef Boolean(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Boolean"))));
    CYSetProperty(context, cy, CYJSString("Boolean"), Boolean);

    JSObjectRef Boolean_prototype(CYCastJSObject(context, CYGetProperty(context, Boolean, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Boolean_prototype"), Boolean_prototype);

    JSObjectRef Error(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Error"))));
    CYSetProperty(context, cy, CYJSString("Error"), Error);

    JSObjectRef Function(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Function"))));
    CYSetProperty(context, cy, CYJSString("Function"), Function);

    JSObjectRef Function_prototype(CYCastJSObject(context, CYGetProperty(context, Function, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Function_prototype"), Function_prototype);

    JSObjectRef JSON(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("JSON"))));
    CYSetProperty(context, cy, CYJSString("JSON"), JSON);

    JSObjectRef Number(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Number"))));
    CYSetProperty(context, cy, CYJSString("Number"), Number);

    JSObjectRef Number_prototype(CYCastJSObject(context, CYGetProperty(context, Number, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Number_prototype"), Number_prototype);

    JSObjectRef Object(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Object"))));
    CYSetProperty(context, cy, CYJSString("Object"), Object);

    JSObjectRef Object_prototype(CYCastJSObject(context, CYGetProperty(context, Object, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Object_prototype"), Object_prototype);

    JSObjectRef String(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("String"))));
    CYSetProperty(context, cy, CYJSString("String"), String);

    JSObjectRef String_prototype(CYCastJSObject(context, CYGetProperty(context, String, prototype_s)));
    CYSetProperty(context, cy, CYJSString("String_prototype"), String_prototype);

    JSObjectRef SyntaxError(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("SyntaxError"))));
    CYSetProperty(context, cy, CYJSString("SyntaxError"), SyntaxError);
/* }}} */

    CYSetProperty(context, Array_prototype, toCYON_s, &Array_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);
    CYSetProperty(context, String_prototype, toCYON_s, &String_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);

    JSObjectRef cycript(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, global, CYJSString("Cycript"), cycript);
    CYSetProperty(context, cycript, CYJSString("compile"), &Cycript_compile_callAsFunction);
    CYSetProperty(context, cycript, CYJSString("gc"), &Cycript_gc_callAsFunction);

    JSObjectRef CArray(JSObjectMakeConstructor(context, CYPrivate<::CArray>::Class_, &CArray_new));
    CYSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, CArray, prototype_s)), Array_prototype);
    CYSetProperty(context, cycript, CYJSString("CArray"), CArray);

    JSObjectRef CString(JSObjectMakeConstructor(context, CYPrivate<::CString>::Class_, &CString_new));
    CYSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, CString, prototype_s)), String_prototype);
    CYSetProperty(context, cycript, CYJSString("CString"), CString);

    JSObjectRef Functor(JSObjectMakeConstructor(context, cy::Functor::Class_, &Functor_new));
    JSObjectRef Functor_prototype(CYCastJSObject(context, CYGetProperty(context, Functor, prototype_s)));
    CYSetPrototype(context, Functor_prototype, Function_prototype);
    CYSetProperty(context, cy, CYJSString("Functor_prototype"), Functor_prototype);
    CYSetProperty(context, cycript, CYJSString("Functor"), Functor);

    CYSetProperty(context, cycript, CYJSString("Pointer"), JSObjectMakeConstructor(context, CYPrivate<Pointer>::Class_, &Pointer_new));

    JSObjectRef Type(JSObjectMakeConstructor(context, CYPrivate<Type_privateData>::Class_, &Type_new));
    JSObjectRef Type_prototype(CYCastJSObject(context, CYGetProperty(context, Type, prototype_s)));
    CYSetPrototype(context, Type_prototype, Function_prototype);
    CYSetProperty(context, cy, CYJSString("Type_prototype"), Type_prototype);
    CYSetProperty(context, cycript, CYJSString("Type"), Type);

    JSObjectRef Character_prototype(JSObjectMake(context, NULL, NULL));
    CYSetPrototype(context, Character_prototype, String_prototype);
    CYSetProperty(context, cy, CYJSString("Character_prototype"), Character_prototype);
    CYSetProperty(context, Character_prototype, CYJSString("valueOf"), _jsccall(JSEvaluateScript, context, CYJSString("(function(){return this.charCodeAt(0);})"), NULL, NULL, 0));

    JSObjectRef modules(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cy, CYJSString("modules"), modules);

    JSObjectRef all(JSObjectMake(context, All_, NULL));
    CYSetProperty(context, cycript, CYJSString("all"), all);

    JSObjectRef cache(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cy, CYJSString("cache"), cache);
    CYSetPrototype(context, cache, all);

    JSObjectRef alls(_jsccall(JSObjectCallAsConstructor, context, Array, 0, NULL));
    CYSetProperty(context, cycript, CYJSString("alls"), alls);

    if (true) {
        JSObjectRef last(NULL), curr(global);

        goto next; for (JSValueRef next;;) {
            if (JSValueIsNull(context, next))
                break;
            last = curr;
            curr = CYCastJSObject(context, next);
          next:
            next = JSObjectGetPrototype(context, curr);
        }

        CYSetPrototype(context, last, cache);
    }

#ifdef __APPLE__
    if (&JSWeakObjectMapCreate != NULL) {
        JSWeakObjectMapRef weak(JSWeakObjectMapCreate(context, NULL, &CYDestroyWeak));
        CYSetProperty(context, cy, weak_s, CYCastJSValue(context, reinterpret_cast<uintptr_t>(weak)));
    }
#endif

    CYSetProperty(context, String_prototype, cyt_s, CYMakeType(context, sig::String(true)), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, cache, CYJSString("dlerror"), CYMakeFunctor(context, "dlerror", "*"), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("RTLD_DEFAULT"), CYCastJSValue(context, reinterpret_cast<intptr_t>(RTLD_DEFAULT)), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("dlsym"), CYMakeFunctor(context, "dlsym", "^v^v*"), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, cache, CYJSString("NULL"), CYJSNull(context), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, cache, CYJSString("bool"), CYMakeType(context, sig::Primitive<bool>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("char"), CYMakeType(context, sig::Primitive<char>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("schar"), CYMakeType(context, sig::Primitive<signed char>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("uchar"), CYMakeType(context, sig::Primitive<unsigned char>()), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, cache, CYJSString("short"), CYMakeType(context, sig::Primitive<short>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("int"), CYMakeType(context, sig::Primitive<int>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("long"), CYMakeType(context, sig::Primitive<long>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("longlong"), CYMakeType(context, sig::Primitive<long long>()), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, cache, CYJSString("ushort"), CYMakeType(context, sig::Primitive<unsigned short>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("uint"), CYMakeType(context, sig::Primitive<unsigned int>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("ulong"), CYMakeType(context, sig::Primitive<unsigned long>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("ulonglong"), CYMakeType(context, sig::Primitive<unsigned long long>()), kJSPropertyAttributeDontEnum);

#ifdef __SIZEOF_INT128__
    CYSetProperty(context, cache, CYJSString("int128"), CYMakeType(context, sig::Primitive<__int128>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("uint128"), CYMakeType(context, sig::Primitive<unsigned __int128>()), kJSPropertyAttributeDontEnum);
#endif

    CYSetProperty(context, cache, CYJSString("float"), CYMakeType(context, sig::Primitive<float>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("double"), CYMakeType(context, sig::Primitive<double>()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("longdouble"), CYMakeType(context, sig::Primitive<long double>()), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, global, CYJSString("require"), &require_callAsFunction, kJSPropertyAttributeDontEnum);

    JSObjectRef System(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, all, CYJSString("system"), System);
    System = CYCastJSObject(context, CYGetProperty(context, global, CYJSString("system")));
    CYSetProperty(context, cy, CYJSString("System"), System);

    JSObjectRef process(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, global, CYJSString("process"), process);
    CYSetProperty(context, cy, CYJSString("process"), process);

    CYSetProperty(context, System, CYJSString("args"), CYJSNull(context));
    CYSetProperty(context, System, CYJSString("print"), &System_print);

    CYSetProperty(context, global, CYJSString("global"), global);
    CYSetProperty(context, global, CYJSString("print"), &Global_print);

    for (CYHook *hook : GetHooks())
        if (hook->SetupContext != NULL)
            (*hook->SetupContext)(context);

    CYArrayPush(context, alls, cycript);

    CYRunScript(context, "/libcycript.cy");
}

static JSGlobalContextRef context_;

_visible JSGlobalContextRef CYGetJSContext() {
    CYInitializeDynamic();

    if (context_ == NULL) {
        context_ = JSGlobalContextCreate(Global_);
        CYSetupContext(context_);
    }

    return context_;
}

_visible void CYDestroyContext() {
    if (context_ == NULL)
        return;
    JSGlobalContextRelease(context_);
    context_ = NULL;
}
