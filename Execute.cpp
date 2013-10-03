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

#include "Internal.hpp"

#include <dlfcn.h>

#include "cycript.hpp"

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include "Pooling.hpp"
#include "Execute.hpp"

#include <sys/mman.h>

#include <iostream>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "Parser.hpp"

#include "Error.hpp"
#include "JavaScript.hpp"
#include "String.hpp"

struct CYHooks *hooks_;

/* JavaScript Properties {{{ */
JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, size_t index) {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetPropertyAtIndex(context, object, index, &exception));
    CYThrow(context, exception);
    return value;
}

JSValueRef CYGetProperty(JSContextRef context, JSObjectRef object, JSStringRef name) {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetProperty(context, object, name, &exception));
    CYThrow(context, exception);
    return value;
}

void CYSetProperty(JSContextRef context, JSObjectRef object, size_t index, JSValueRef value) {
    JSValueRef exception(NULL);
    JSObjectSetPropertyAtIndex(context, object, index, value, &exception);
    CYThrow(context, exception);
}

void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef value, JSPropertyAttributes attributes) {
    JSValueRef exception(NULL);
    JSObjectSetProperty(context, object, name, value, attributes, &exception);
    CYThrow(context, exception);
}

void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef (*callback)(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *), JSPropertyAttributes attributes) {
    CYSetProperty(context, object, name, JSObjectMakeFunctionWithCallback(context, name, callback), attributes);
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
    // XXX: this is very wrong; it needs to convert to UTF16 and then create from there
    return CYCopyJSString(value.data);
}

JSStringRef CYCopyJSString(JSContextRef context, JSValueRef value) {
    if (JSValueIsNull(context, value))
        return NULL;
    JSValueRef exception(NULL);
    JSStringRef string(JSValueToStringCopy(context, value, &exception));
    CYThrow(context, exception);
    return string;
}

static CYUTF16String CYCastUTF16String(JSStringRef value) {
    return CYUTF16String(JSStringGetCharactersPtr(value), JSStringGetLength(value));
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

static JSClassRef All_;
static JSClassRef Context_;
JSClassRef Functor_;
static JSClassRef Global_;
static JSClassRef Pointer_;
static JSClassRef Struct_;

JSStringRef Array_s;
JSStringRef cy_s;
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

static JSStringRef Result_;

void CYFinalize(JSObjectRef object) {
    CYData *internal(reinterpret_cast<CYData *>(JSObjectGetPrivate(object)));
    _assert(internal->count_ != _not(unsigned));
    if (--internal->count_ == 0)
        delete internal;
}

void Structor_(CYPool &pool, sig::Type *&type) {
    if (
        type->primitive == sig::pointer_P &&
        type->data.data.type != NULL &&
        type->data.data.type->primitive == sig::struct_P &&
        type->data.data.type->name != NULL &&
        strcmp(type->data.data.type->name, "_objc_class") == 0
    ) {
        type->primitive = sig::typename_P;
        type->data.data.type = NULL;
        return;
    }

    if (type->primitive != sig::struct_P || type->name == NULL)
        return;

    size_t length(strlen(type->name));
    char keyed[length + 2];
    memcpy(keyed + 1, type->name, length + 1);

    static const char *modes = "34";
    for (size_t i(0); i != 2; ++i) {
        char mode(modes[i]);
        keyed[0] = mode;

        if (CYBridgeEntry *entry = CYBridgeHash(keyed, length + 1))
            switch (mode) {
                case '3':
                    sig::Parse(pool, &type->data.signature, entry->value_, &Structor_);
                break;

                case '4': {
                    sig::Signature signature;
                    sig::Parse(pool, &signature, entry->value_, &Structor_);
                    type = signature.elements[0].type;
                } break;
            }
    }
}

JSClassRef Type_privateData::Class_;

struct Context :
    CYData
{
    JSGlobalContextRef context_;

    Context(JSGlobalContextRef context) :
        context_(context)
    {
    }
};

struct Pointer :
    CYOwned
{
    Type_privateData *type_;
    size_t length_;

    Pointer(void *value, JSContextRef context, JSObjectRef owner, size_t length, sig::Type *type) :
        CYOwned(value, context, owner),
        type_(new(*pool_) Type_privateData(type)),
        length_(length)
    {
    }
};

struct Struct_privateData :
    CYOwned
{
    Type_privateData *type_;

    Struct_privateData(JSContextRef context, JSObjectRef owner) :
        CYOwned(NULL, context, owner)
    {
    }
};

typedef std::map<const char *, Type_privateData *, CYCStringLess> TypeMap;
static TypeMap Types_;

JSObjectRef CYMakeStruct(JSContextRef context, void *data, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Struct_privateData *internal(new Struct_privateData(context, owner));
    CYPool &pool(*internal->pool_);
    Type_privateData *typical(new(pool) Type_privateData(type, ffi));
    internal->type_ = typical;

    if (owner != NULL)
        internal->value_ = data;
    else {
        size_t size(typical->GetFFI()->size);
        void *copy(internal->pool_->malloc<void>(size));
        memcpy(copy, data, size);
        internal->value_ = copy;
    }

    return JSObjectMake(context, Struct_, internal);
}

static void *CYCastSymbol(const char *name) {
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
        return JSValueMakeNumber(context, static_cast<double>(value)); \
    }

CYCastJSValue_(int)
CYCastJSValue_(unsigned int)
CYCastJSValue_(long int)
CYCastJSValue_(long unsigned int)
CYCastJSValue_(long long int)
CYCastJSValue_(long long unsigned int)

JSValueRef CYJSUndefined(JSContextRef context) {
    return JSValueMakeUndefined(context);
}

double CYCastDouble(JSContextRef context, JSValueRef value) {
    JSValueRef exception(NULL);
    double number(JSValueToNumber(context, value, &exception));
    CYThrow(context, exception);
    return number;
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
    JSValueRef exception(NULL);
    JSObjectRef object(JSValueToObject(context, value, &exception));
    CYThrow(context, exception);
    return object;
}

JSValueRef CYCallAsFunction(JSContextRef context, JSObjectRef function, JSObjectRef _this, size_t count, const JSValueRef arguments[]) {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectCallAsFunction(context, function, _this, count, arguments, &exception));
    CYThrow(context, exception);
    return value;
}

bool CYIsCallable(JSContextRef context, JSValueRef value) {
    return value != NULL && JSValueIsObject(context, value) && JSObjectIsFunction(context, (JSObjectRef) value);
}

size_t CYArrayLength(JSContextRef context, JSObjectRef array) {
    return CYCastDouble(context, CYGetProperty(context, array, length_s));
}

JSValueRef CYArrayGet(JSContextRef context, JSObjectRef array, size_t index) {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetPropertyAtIndex(context, array, index, &exception));
    CYThrow(context, exception);
    return value;
}

void CYArrayPush(JSContextRef context, JSObjectRef array, JSValueRef value) {
    JSValueRef exception(NULL);
    JSValueRef arguments[1];
    arguments[0] = value;
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
    JSObjectCallAsFunction(context, CYCastJSObject(context, CYGetProperty(context, Array, push_s)), array, 1, arguments, &exception);
    CYThrow(context, exception);
}

static JSValueRef System_print(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count == 0)
        printf("\n");
    else {
        CYPool pool;
        printf("%s\n", CYPoolCString(pool, context, arguments[0]));
    }

    return CYJSUndefined(context);
} CYCatch(NULL) }

static size_t Nonce_(0);

static JSValueRef $cyq(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYPool pool;
    const char *name(pool.strcat(CYPoolCString(pool, context, arguments[0]), pool.itoa(Nonce_++), NULL));
    return CYCastJSValue(context, name);
}

static JSValueRef Cycript_gc_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    JSGarbageCollect(context);
    return CYJSUndefined(context);
}

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSValueRef value, JSValueRef *exception) { CYTry {
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
            CYStringify(str, string.data, string.size);
            std::string value(str.str());
            return pool.strmemdup(value.c_str(), value.size());
        } break;

        case kJSTypeObject:
            return CYPoolCCYON(pool, context, (JSObjectRef) value);
        default:
            throw CYJSError(context, "JSValueGetType() == 0x%x", type);
    }
} CYCatch(NULL) }

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSValueRef value) {
    JSValueRef exception(NULL);
    const char *cyon(CYPoolCCYON(pool, context, value, &exception));
    CYThrow(context, exception);
    return cyon;
}

const char *CYPoolCCYON(CYPool &pool, JSContextRef context, JSObjectRef object) {
    JSValueRef toCYON(CYGetProperty(context, object, toCYON_s));
    if (CYIsCallable(context, toCYON)) {
        JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toCYON, object, 0, NULL));
        _assert(value != NULL);
        return CYPoolCString(pool, context, value);
    }

    JSValueRef toJSON(CYGetProperty(context, object, toJSON_s));
    if (CYIsCallable(context, toJSON)) {
        JSValueRef arguments[1] = {CYCastJSValue(context, CYJSString(""))};
        JSValueRef exception(NULL);
        const char *cyon(CYPoolCCYON(pool, context, CYCallAsFunction(context, (JSObjectRef) toJSON, object, 1, arguments), &exception));
        CYThrow(context, exception);
        return cyon;
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

    std::ostringstream str;

    str << '{';

    // XXX: this is, sadly, going to leak
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context, object));

    bool comma(false);

    for (size_t index(0), count(JSPropertyNameArrayGetCount(names)); index != count; ++index) {
        JSStringRef name(JSPropertyNameArrayGetNameAtIndex(names, index));
        JSValueRef value(CYGetProperty(context, object, name));

        if (comma)
            str << ',';
        else
            comma = true;

        CYUTF8String string(CYPoolUTF8String(pool, context, name));
        if (CYIsKey(string))
            str << string.data;
        else
            CYStringify(str, string.data, string.size);

        str << ':' << CYPoolCCYON(pool, context, value);
    }

    str << '}';

    JSPropertyNameArrayRelease(names);

    std::string string(str.str());
    return pool.strmemdup(string.c_str(), string.size());
}

static JSValueRef Array_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    std::ostringstream str;

    str << '[';

    JSValueRef length(CYGetProperty(context, _this, length_s));
    bool comma(false);

    for (size_t index(0), count(CYCastDouble(context, length)); index != count; ++index) {
        JSValueRef value(CYGetProperty(context, _this, index));

        if (comma)
            str << ',';
        else
            comma = true;

        if (!JSValueIsUndefined(context, value))
            str << CYPoolCCYON(pool, context, value);
        else {
            str << ',';
            comma = false;
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
    CYStringify(str, string.data, string.size);

    std::string value(str.str());
    return CYCastJSValue(context, CYJSString(CYUTF8String(value.c_str(), value.size())));
} CYCatch(NULL) }

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, size_t length, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Pointer *internal(new Pointer(pointer, context, owner, length, type));
    return JSObjectMake(context, Pointer_, internal);
}

static JSObjectRef CYMakeFunctor(JSContextRef context, void (*function)(), const char *type) {
    return JSObjectMake(context, Functor_, new cy::Functor(type, function));
}

static JSObjectRef CYMakeFunctor(JSContextRef context, const char *symbol, const char *type, void **cache) {
    cy::Functor *internal;
    if (*cache != NULL)
        internal = reinterpret_cast<cy::Functor *>(*cache);
    else {
        void (*function)()(reinterpret_cast<void (*)()>(CYCastSymbol(symbol)));
        if (function == NULL)
            return NULL;

        internal = new cy::Functor(type, function);
        *cache = internal;
    }

    ++internal->count_;
    return JSObjectMake(context, Functor_, internal);
}

static bool CYGetOffset(CYPool &pool, JSContextRef context, JSStringRef value, ssize_t &index) {
    return CYGetOffset(CYPoolCString(pool, context, value), index);
}

void *CYCastPointer_(JSContextRef context, JSValueRef value) {
    switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return NULL;
        case kJSTypeObject: {
            JSObjectRef object((JSObjectRef) value);
            if (JSValueIsObjectOfClass(context, value, Pointer_)) {
                Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
                return internal->value_;
            }
            JSValueRef toPointer(CYGetProperty(context, object, toPointer_s));
            if (CYIsCallable(context, toPointer)) {
                JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toPointer, object, 0, NULL));
                _assert(value != NULL);
                return CYCastPointer_(context, value);
            }
        } default:
            double number(CYCastDouble(context, value));
            if (std::isnan(number))
                throw CYJSError(context, "cannot convert value to pointer");
            return reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<long long>(number)));
    }
}

void CYPoolFFI(CYPool *pool, JSContextRef context, sig::Type *type, ffi_type *ffi, void *data, JSValueRef value) {
    switch (type->primitive) {
        case sig::boolean_P:
            *reinterpret_cast<bool *>(data) = JSValueToBoolean(context, value);
        break;

#define CYPoolFFI_(primitive, native) \
        case sig::primitive ## _P: \
            *reinterpret_cast<native *>(data) = CYCastDouble(context, value); \
        break;

        CYPoolFFI_(uchar, unsigned char)
        CYPoolFFI_(char, char)
        CYPoolFFI_(ushort, unsigned short)
        CYPoolFFI_(short, short)
        CYPoolFFI_(ulong, unsigned long)
        CYPoolFFI_(long, long)
        CYPoolFFI_(uint, unsigned int)
        CYPoolFFI_(int, int)
        CYPoolFFI_(ulonglong, unsigned long long)
        CYPoolFFI_(longlong, long long)
        CYPoolFFI_(float, float)
        CYPoolFFI_(double, double)

        case sig::array_P: {
            uint8_t *base(reinterpret_cast<uint8_t *>(data));
            JSObjectRef aggregate(JSValueIsObject(context, value) ? (JSObjectRef) value : NULL);
            for (size_t index(0); index != type->data.data.size; ++index) {
                ffi_type *field(ffi->elements[index]);

                JSValueRef rhs;
                if (aggregate == NULL)
                    rhs = value;
                else {
                    rhs = CYGetProperty(context, aggregate, index);
                    if (JSValueIsUndefined(context, rhs))
                        throw CYJSError(context, "unable to extract array value");
                }

                CYPoolFFI(pool, context, type->data.data.type, field, base, rhs);
                // XXX: alignment?
                base += field->size;
            }
        } break;

        case sig::pointer_P:
            *reinterpret_cast<void **>(data) = CYCastPointer<void *>(context, value);
        break;

        case sig::string_P:
            _assert(pool != NULL);
            *reinterpret_cast<const char **>(data) = CYPoolCString(*pool, context, value);
        break;

        case sig::struct_P: {
            uint8_t *base(reinterpret_cast<uint8_t *>(data));
            JSObjectRef aggregate(JSValueIsObject(context, value) ? (JSObjectRef) value : NULL);
            for (size_t index(0); index != type->data.signature.count; ++index) {
                sig::Element *element(&type->data.signature.elements[index]);
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

                CYPoolFFI(pool, context, element->type, field, base, rhs);
                // XXX: alignment?
                base += field->size;
            }
        } break;

        case sig::void_P:
        break;

        default:
            if (hooks_ != NULL && hooks_->PoolFFI != NULL)
                if ((*hooks_->PoolFFI)(pool, context, type, ffi, data, value))
                    return;

            CYThrow("unimplemented signature code: '%c''\n", type->primitive);
    }
}

JSValueRef CYFromFFI(JSContextRef context, sig::Type *type, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) {
    switch (type->primitive) {
        case sig::boolean_P:
            return CYCastJSValue(context, *reinterpret_cast<bool *>(data));

#define CYFromFFI_(primitive, native) \
        case sig::primitive ## _P: \
            return CYCastJSValue(context, *reinterpret_cast<native *>(data)); \

        CYFromFFI_(uchar, unsigned char)
        CYFromFFI_(char, char)
        CYFromFFI_(ushort, unsigned short)
        CYFromFFI_(short, short)
        CYFromFFI_(ulong, unsigned long)
        CYFromFFI_(long, long)
        CYFromFFI_(uint, unsigned int)
        CYFromFFI_(int, int)
        CYFromFFI_(ulonglong, unsigned long long)
        CYFromFFI_(longlong, long long)
        CYFromFFI_(float, float)
        CYFromFFI_(double, double)

        case sig::array_P:
            if (void *pointer = data)
                return CYMakePointer(context, pointer, type->data.data.size, type->data.data.type, NULL, owner);
            else goto null;

        case sig::pointer_P:
            if (void *pointer = *reinterpret_cast<void **>(data))
                return CYMakePointer(context, pointer, _not(size_t), type->data.data.type, NULL, owner);
            else goto null;

        case sig::string_P:
            if (char *utf8 = *reinterpret_cast<char **>(data))
                return CYCastJSValue(context, utf8);
            else goto null;

        case sig::struct_P:
            return CYMakeStruct(context, data, type, ffi, owner);
        case sig::void_P:
            return CYJSUndefined(context);

        null:
            return CYJSNull(context);
        default:
            if (hooks_ != NULL && hooks_->FromFFI != NULL)
                if (JSValueRef value = (*hooks_->FromFFI)(context, type, ffi, data, initialize, owner))
                    return value;

            CYThrow("unimplemented signature code: '%c''\n", type->primitive);
    }
}

void CYExecuteClosure(ffi_cif *cif, void *result, void **arguments, void *arg, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef)) {
    Closure_privateData *internal(reinterpret_cast<Closure_privateData *>(arg));

    JSContextRef context(internal->context_);

    size_t count(internal->cif_.nargs);
    JSValueRef values[count];

    for (size_t index(0); index != count; ++index)
        values[index] = CYFromFFI(context, internal->signature_.elements[1 + index].type, internal->cif_.arg_types[index], arguments[index]);

    JSValueRef value(adapter(context, count, values, internal->function_));
    CYPoolFFI(NULL, context, internal->signature_.elements[0].type, internal->cif_.rtype, result, value);
}

static JSValueRef FunctionAdapter_(JSContextRef context, size_t count, JSValueRef values[], JSObjectRef function) {
    return CYCallAsFunction(context, function, NULL, count, values);
}

static void FunctionClosure_(ffi_cif *cif, void *result, void **arguments, void *arg) {
    CYExecuteClosure(cif, result, arguments, arg, &FunctionAdapter_);
}

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const char *type, void (*callback)(ffi_cif *, void *, void **, void *)) {
    // XXX: in case of exceptions this will leak
    // XXX: in point of fact, this may /need/ to leak :(
    Closure_privateData *internal(new Closure_privateData(context, function, type));

#if defined(__APPLE__) && defined(__arm__)
    void *executable;
    ffi_closure *writable(reinterpret_cast<ffi_closure *>(ffi_closure_alloc(sizeof(ffi_closure), &executable)));

    ffi_status status(ffi_prep_closure_loc(writable, &internal->cif_, callback, internal, executable));
    _assert(status == FFI_OK);

    internal->value_ = executable;
#else
    ffi_closure *closure((ffi_closure *) _syscall(mmap(
        NULL, sizeof(ffi_closure),
        PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
        -1, 0
    )));

    ffi_status status(ffi_prep_closure(closure, &internal->cif_, callback, internal));
    _assert(status == FFI_OK);

    _syscall(mprotect(closure, sizeof(*closure), PROT_READ | PROT_EXEC));

    internal->value_ = closure;
#endif

    return internal;
}

static JSObjectRef CYMakeFunctor(JSContextRef context, JSObjectRef function, const char *type) {
    Closure_privateData *internal(CYMakeFunctor_(context, function, type, &FunctionClosure_));
    JSObjectRef object(JSObjectMake(context, Functor_, internal));
    // XXX: see above notes about needing to leak
    JSValueProtect(CYGetJSContext(context), object);
    return object;
}

JSObjectRef CYGetCachedObject(JSContextRef context, JSStringRef name) {
    return CYCastJSObject(context, CYGetProperty(context, CYCastJSObject(context, CYGetProperty(context, CYGetGlobalObject(context), cy_s)), name));
}

static JSObjectRef CYMakeFunctor(JSContextRef context, JSValueRef value, const char *type) {
    JSObjectRef Function(CYGetCachedObject(context, CYJSString("Function")));

    JSValueRef exception(NULL);
    bool function(JSValueIsInstanceOfConstructor(context, value, Function, &exception));
    CYThrow(context, exception);

    if (function) {
        JSObjectRef function(CYCastJSObject(context, value));
        return CYMakeFunctor(context, function, type);
    } else {
        void (*function)()(CYCastPointer<void (*)()>(context, value));
        return CYMakeFunctor(context, function, type);
    }
}

static bool Index_(CYPool &pool, JSContextRef context, Struct_privateData *internal, JSStringRef property, ssize_t &index, uint8_t *&base) {
    Type_privateData *typical(internal->type_);
    sig::Type *type(typical->type_);
    if (type == NULL)
        return false;

    const char *name(CYPoolCString(pool, context, property));
    size_t length(strlen(name));
    double number(CYCastDouble(name, length));

    size_t count(type->data.signature.count);

    if (std::isnan(number)) {
        if (property == NULL)
            return false;

        sig::Element *elements(type->data.signature.elements);

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

    base = reinterpret_cast<uint8_t *>(internal->value_);
    for (ssize_t local(0); local != index; ++local)
        base += elements[local]->size;

    return true;
}

static JSValueRef Pointer_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));

    if (JSStringIsEqual(property, length_s))
        return internal->length_ == _not(size_t) ? CYJSUndefined(context) : CYCastJSValue(context, internal->length_);

    Type_privateData *typical(internal->type_);

    if (typical->type_ == NULL)
        return NULL;

    ssize_t offset;
    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        offset = 0;
    else if (!CYGetOffset(pool, context, property, offset))
        return NULL;

    ffi_type *ffi(typical->GetFFI());

    uint8_t *base(reinterpret_cast<uint8_t *>(internal->value_));
    base += ffi->size * offset;

    JSObjectRef owner(internal->GetOwner() ?: object);
    return CYFromFFI(context, typical->type_, ffi, base, false, owner);
} CYCatch(NULL) }

static bool Pointer_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    if (typical->type_ == NULL)
        return false;

    ssize_t offset;
    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        offset = 0;
    else if (!CYGetOffset(pool, context, property, offset))
        return false;

    ffi_type *ffi(typical->GetFFI());

    uint8_t *base(reinterpret_cast<uint8_t *>(internal->value_));
    base += ffi->size * offset;

    CYPoolFFI(NULL, context, typical->type_, ffi, base, value);
    return true;
} CYCatch(false) }

static JSValueRef Struct_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(_this)));
    Type_privateData *typical(internal->type_);
    return CYMakePointer(context, internal->value_, _not(size_t), typical->type_, typical->ffi_, _this);
}

static JSValueRef Struct_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    ssize_t index;
    uint8_t *base;

    if (!Index_(pool, context, internal, property, index, base))
        return NULL;

    JSObjectRef owner(internal->GetOwner() ?: object);

    return CYFromFFI(context, typical->type_->data.signature.elements[index].type, typical->GetFFI()->elements[index], base, false, owner);
} CYCatch(NULL) }

static bool Struct_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    ssize_t index;
    uint8_t *base;

    if (!Index_(pool, context, internal, property, index, base))
        return false;

    CYPoolFFI(NULL, context, typical->type_->data.signature.elements[index].type, typical->GetFFI()->elements[index], base, value);
    return true;
} CYCatch(false) }

static void Struct_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);
    sig::Type *type(typical->type_);

    if (type == NULL)
        return;

    size_t count(type->data.signature.count);
    sig::Element *elements(type->data.signature.elements);

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

JSValueRef CYCallFunction(CYPool &pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, JSValueRef *exception, sig::Signature *signature, ffi_cif *cif, void (*function)()) { CYTry {
    if (setups + count != signature->count - 1)
        throw CYJSError(context, "incorrect number of arguments to ffi function");

    size_t size(setups + count);
    void *values[size];
    memcpy(values, setup, sizeof(void *) * setups);

    for (size_t index(setups); index != size; ++index) {
        sig::Element *element(&signature->elements[index + 1]);
        ffi_type *ffi(cif->arg_types[index]);
        // XXX: alignment?
        values[index] = new(pool) uint8_t[ffi->size];
        CYPoolFFI(&pool, context, element->type, ffi, values[index], arguments[index - setups]);
    }

    uint8_t value[cif->rtype->size];

    if (hooks_ != NULL && hooks_->CallFunction != NULL)
        (*hooks_->CallFunction)(context, cif, function, value, values);
    else
        ffi_call(cif, function, value, values);

    return CYFromFFI(context, signature->elements[0].type, cif->rtype, value, initialize);
} CYCatch(NULL) }

static JSValueRef Functor_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYPool pool;
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    return CYCallFunction(pool, context, 0, NULL, count, arguments, false, exception, &internal->signature_, &internal->cif_, internal->GetValue());
}

JSObjectRef CYMakeType(JSContextRef context, const char *type) {
    Type_privateData *internal(new Type_privateData(type));
    return JSObjectMake(context, Type_privateData::Class_, internal);
}

JSObjectRef CYMakeType(JSContextRef context, sig::Type *type) {
    Type_privateData *internal(new Type_privateData(type));
    return JSObjectMake(context, Type_privateData::Class_, internal);
}

static JSValueRef All_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    for (size_t i(0), count(CYArrayLength(context, alls)); i != count; ++i)
        if (JSObjectRef space = CYCastJSObject(context, CYArrayGet(context, alls, count - i - 1)))
            if (JSValueRef value = CYGetProperty(context, space, property))
                if (!JSValueIsUndefined(context, value))
                    return value;

    CYPool pool;
    CYUTF8String name(CYPoolUTF8String(pool, context, property));

    size_t length(name.size);
    char keyed[length + 2];
    memcpy(keyed + 1, name.data, length + 1);

    static const char *modes = "0124";
    for (size_t i(0); i != 4; ++i) {
        char mode(modes[i]);
        keyed[0] = mode;

        if (CYBridgeEntry *entry = CYBridgeHash(keyed, length + 1))
            switch (mode) {
                case '0':
                    return JSEvaluateScript(CYGetJSContext(context), CYJSString(entry->value_), NULL, NULL, 0, NULL);

                case '1':
                    return CYMakeFunctor(context, name.data, entry->value_, &entry->cache_);

                case '2':
                    if (void *symbol = CYCastSymbol(name.data)) {
                        // XXX: this is horrendously inefficient
                        sig::Signature signature;
                        sig::Parse(pool, &signature, entry->value_, &Structor_);
                        ffi_cif cif;
                        sig::sig_ffi_cif(pool, &sig::ObjectiveC, &signature, &cif);
                        return CYFromFFI(context, signature.elements[0].type, cif.rtype, symbol);
                    } else return NULL;

                // XXX: implement case 3
                case '4':
                    return CYMakeType(context, entry->value_);
            }
    }

    return NULL;
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

static JSObjectRef Pointer_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to Pointer constructor");

    CYPool pool;

    void *value(CYCastPointer<void *>(context, arguments[0]));
    const char *type(CYPoolCString(pool, context, arguments[1]));

    sig::Signature signature;
    sig::Parse(pool, &signature, type, &Structor_);

    return CYMakePointer(context, value, _not(size_t), signature.elements[0].type, NULL, NULL);
} CYCatch(NULL) }

static JSObjectRef Type_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Type constructor");
    CYPool pool;
    const char *type(CYPoolCString(pool, context, arguments[0]));
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

    sig::Type type;
    type.name = NULL;
    type.flags = 0;

    type.primitive = sig::array_P;
    type.data.data.type = internal->type_;
    type.data.data.size = index;

    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_constant(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.constant");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type(*internal->type_);
    type.flags |= JOC_TYPE_CONST;
    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_pointerTo(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.pointerTo");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type;
    type.name = NULL;
    type.flags = 0;

    type.primitive = sig::pointer_P;
    type.data.data.type = internal->type_;
    type.data.data.size = 0;

    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_withName(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Type.withName");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;
    const char *name(CYPoolCString(pool, context, arguments[0]));

    sig::Type type(*internal->type_);
    type.name = name;
    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to type cast function");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

    sig::Type *type(internal->type_);
    ffi_type *ffi(internal->GetFFI());
    // XXX: alignment?
    uint8_t value[ffi->size];
    CYPool pool;
    CYPoolFFI(&pool, context, type, ffi, value, arguments[0]);
    return CYFromFFI(context, type, ffi, value);
} CYCatch(NULL) }

static JSObjectRef Type_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type allocator");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

    sig::Type *type(internal->type_);
    size_t length;

    if (type->primitive != sig::array_P)
        length = _not(size_t);
    else {
        length = type->data.data.size;
        type = type->data.data.type;
    }

    void *value(malloc(internal->GetFFI()->size));
    return CYMakePointer(context, value, length, type, NULL, NULL);
} CYCatch(NULL) }

static JSObjectRef Functor_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to Functor constructor");
    CYPool pool;
    const char *type(CYPoolCString(pool, context, arguments[1]));
    return CYMakeFunctor(context, arguments[0], type);
} CYCatch(NULL) }

static JSValueRef CYValue_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
} CYCatch(NULL) }

static JSValueRef CYValue_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return CYValue_callAsFunction_valueOf(context, object, _this, count, arguments, exception);
}

static JSValueRef CYValue_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));
    char string[32];
    sprintf(string, "%p", internal->value_);
    return CYCastJSValue(context, string);
} CYCatch(NULL) }

static JSValueRef Pointer_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(_this)));
    if (internal->length_ != _not(size_t)) {
        JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
        JSObjectRef toCYON(CYCastJSObject(context, CYGetProperty(context, Array, toCYON_s)));
        return CYCallAsFunction(context, toCYON, _this, count, arguments);
    } else {
        char string[32];
        sprintf(string, "%p", internal->value_);
        return CYCastJSValue(context, string);
    }
} CYCatch(NULL) }

static JSValueRef Functor_getProperty_type(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    CYPool pool;
    return CYCastJSValue(context, Unparse(pool, &internal->signature_));
}

static JSValueRef Type_getProperty_alignment(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->GetFFI()->alignment);
}

static JSValueRef Type_getProperty_name(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->type_->name);
}

static JSValueRef Type_getProperty_size(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->GetFFI()->size);
}

static JSValueRef Type_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
    CYPool pool;
    const char *type(sig::Unparse(pool, internal->type_));
    return CYCastJSValue(context, CYJSString(type));
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
    CYPool pool;
    const char *type(sig::Unparse(pool, internal->type_));
    std::ostringstream str;
    CYStringify(str, type, strlen(type));
    char *cyon(pool.strcat("new Type(", str.str().c_str(), ")", NULL));
    return CYCastJSValue(context, CYJSString(cyon));
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return Type_callAsFunction_toString(context, object, _this, count, arguments, exception);
}

static JSStaticFunction Pointer_staticFunctions[4] = {
    {"toCYON", &Pointer_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &CYValue_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &CYValue_callAsFunction_valueOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Struct_staticFunctions[2] = {
    {"$cya", &Struct_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Functor_staticFunctions[4] = {
    {"toCYON", &CYValue_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &CYValue_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &CYValue_callAsFunction_valueOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

namespace cy {
    JSStaticFunction const * const Functor::StaticFunctions = Functor_staticFunctions;
}

static JSStaticValue Functor_staticValues[2] = {
    {"type", &Functor_getProperty_type, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticValue Type_staticValues[4] = {
    {"alignment", &Type_getProperty_alignment, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"name", &Type_getProperty_name, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"size", &Type_getProperty_size, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Type_staticFunctions[8] = {
    {"arrayOf", &Type_callAsFunction_arrayOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"constant", &Type_callAsFunction_constant, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"pointerTo", &Type_callAsFunction_pointerTo, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"withName", &Type_callAsFunction_withName, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toCYON", &Type_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Type_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Type_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSObjectRef (*JSObjectMakeArray$)(JSContextRef, size_t, const JSValueRef[], JSValueRef *);

void CYSetArgs(int argc, const char *argv[]) {
    JSContextRef context(CYGetJSContext());
    JSValueRef args[argc];
    for (int i(0); i != argc; ++i)
        args[i] = CYCastJSValue(context, argv[i]);

    JSObjectRef array;
    if (JSObjectMakeArray$ != NULL) {
        JSValueRef exception(NULL);
        array = (*JSObjectMakeArray$)(context, argc, args, &exception);
        CYThrow(context, exception);
    } else {
        JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array")));
        JSValueRef value(CYCallAsFunction(context, Array, NULL, argc, args));
        array = CYCastJSObject(context, value);
    }

    JSObjectRef System(CYGetCachedObject(context, CYJSString("System")));
    CYSetProperty(context, System, CYJSString("args"), array);
}

JSObjectRef CYGetGlobalObject(JSContextRef context) {
    return JSContextGetGlobalObject(context);
}

const char *CYExecute(CYPool &pool, CYUTF8String code) {
    JSContextRef context(CYGetJSContext());
    JSValueRef exception(NULL), result;

    void *handle;
    if (hooks_ != NULL && hooks_->ExecuteStart != NULL)
        handle = (*hooks_->ExecuteStart)(context);
    else
        handle = NULL;

    const char *json;

    try {
        result = JSEvaluateScript(context, CYJSString(code), NULL, NULL, 0, &exception);
    } catch (const char *error) {
        return error;
    }

    if (exception != NULL) error:
        return CYPoolCString(pool, context, CYJSString(context, exception));

    if (JSValueIsUndefined(context, result))
        return NULL;

    try {
        json = CYPoolCCYON(pool, context, result, &exception);
    } catch (const char *error) {
        return error;
    }

    if (exception != NULL)
        goto error;

    CYSetProperty(context, CYGetGlobalObject(context), Result_, result);

    if (hooks_ != NULL && hooks_->ExecuteEnd != NULL)
        (*hooks_->ExecuteEnd)(context, handle);
    return json;
}

extern "C" void CydgetSetupContext(JSGlobalContextRef context) {
    CYSetupContext(context);
}

static bool initialized_ = false;

void CYInitializeDynamic() {
    if (!initialized_)
        initialized_ = true;
    else return;

    JSObjectMakeArray$ = reinterpret_cast<JSObjectRef (*)(JSContextRef, size_t, const JSValueRef[], JSValueRef *)>(dlsym(RTLD_DEFAULT, "JSObjectMakeArray"));

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.className = "All";
    definition.getProperty = &All_getProperty;
    definition.getPropertyNames = &All_getPropertyNames;
    All_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Context";
    definition.finalize = &CYFinalize;
    Context_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Functor";
    definition.staticFunctions = cy::Functor::StaticFunctions;
    definition.staticValues = Functor_staticValues;
    definition.callAsFunction = &Functor_callAsFunction;
    definition.finalize = &CYFinalize;
    Functor_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Pointer";
    definition.staticFunctions = Pointer_staticFunctions;
    definition.getProperty = &Pointer_getProperty;
    definition.setProperty = &Pointer_setProperty;
    definition.finalize = &CYFinalize;
    Pointer_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Struct";
    definition.staticFunctions = Struct_staticFunctions;
    definition.getProperty = &Struct_getProperty;
    definition.setProperty = &Struct_setProperty;
    definition.getPropertyNames = &Struct_getPropertyNames;
    definition.finalize = &CYFinalize;
    Struct_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Type";
    definition.staticValues = Type_staticValues;
    definition.staticFunctions = Type_staticFunctions;
    definition.callAsFunction = &Type_callAsFunction;
    definition.callAsConstructor = &Type_callAsConstructor;
    definition.finalize = &CYFinalize;
    Type_privateData::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Global";
    //definition.getProperty = &Global_getProperty;
    Global_ = JSClassCreate(&definition);

    Array_s = JSStringCreateWithUTF8CString("Array");
    cy_s = JSStringCreateWithUTF8CString("$cy");
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

    Result_ = JSStringCreateWithUTF8CString("_");

    if (hooks_ != NULL && hooks_->Initialize != NULL)
        (*hooks_->Initialize)();
}

void CYThrow(JSContextRef context, JSValueRef value) {
    if (value != NULL)
        throw CYJSError(context, value);
}

const char *CYJSError::PoolCString(CYPool &pool) const {
    // XXX: this used to be CYPoolCString
    return CYPoolCCYON(pool, context_, value_);
}

JSValueRef CYJSError::CastJSValue(JSContextRef context) const {
    // XXX: what if the context is different?
    return value_;
}

JSValueRef CYCastJSError(JSContextRef context, const char *message) {
    JSObjectRef Error(CYGetCachedObject(context, CYJSString("Error")));

    JSValueRef arguments[1] = {CYCastJSValue(context, message)};

    JSValueRef exception(NULL);
    JSValueRef value(JSObjectCallAsConstructor(context, Error, 1, arguments, &exception));
    CYThrow(context, exception);

    return value;
}

JSValueRef CYPoolError::CastJSValue(JSContextRef context) const {
    return CYCastJSError(context, message_);
}

CYJSError::CYJSError(JSContextRef context, const char *format, ...) {
    _assert(context != NULL);

    CYPool pool;

    va_list args;
    va_start(args, format);
    // XXX: there might be a beter way to think about this
    const char *message(pool.vsprintf(64, format, args));
    va_end(args);

    value_ = CYCastJSError(context, message);
}

JSGlobalContextRef CYGetJSContext(JSContextRef context) {
    return reinterpret_cast<Context *>(JSObjectGetPrivate(CYCastJSObject(context, CYGetProperty(context, CYGetGlobalObject(context), cy_s))))->context_;
}

extern "C" void CYSetupContext(JSGlobalContextRef context) {
    JSValueRef exception(NULL);

    CYInitializeDynamic();

    JSObjectRef global(CYGetGlobalObject(context));

    JSObjectRef cy(JSObjectMake(context, Context_, new Context(context)));
    CYSetProperty(context, global, cy_s, cy, kJSPropertyAttributeDontEnum);

/* Cache Globals {{{ */
    JSObjectRef Array(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Array"))));
    CYSetProperty(context, cy, CYJSString("Array"), Array);

    JSObjectRef Array_prototype(CYCastJSObject(context, CYGetProperty(context, Array, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Array_prototype"), Array_prototype);

    JSObjectRef Error(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Error"))));
    CYSetProperty(context, cy, CYJSString("Error"), Error);

    JSObjectRef Function(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Function"))));
    CYSetProperty(context, cy, CYJSString("Function"), Function);

    JSObjectRef Function_prototype(CYCastJSObject(context, CYGetProperty(context, Function, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Function_prototype"), Function_prototype);

    JSObjectRef Object(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Object"))));
    CYSetProperty(context, cy, CYJSString("Object"), Object);

    JSObjectRef Object_prototype(CYCastJSObject(context, CYGetProperty(context, Object, prototype_s)));
    CYSetProperty(context, cy, CYJSString("Object_prototype"), Object_prototype);

    JSObjectRef String(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("String"))));
    CYSetProperty(context, cy, CYJSString("String"), String);

    JSObjectRef String_prototype(CYCastJSObject(context, CYGetProperty(context, String, prototype_s)));
    CYSetProperty(context, cy, CYJSString("String_prototype"), String_prototype);
/* }}} */

    CYSetProperty(context, Array_prototype, toCYON_s, &Array_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);
    CYSetProperty(context, String_prototype, toCYON_s, &String_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);

    JSObjectRef cycript(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, global, CYJSString("Cycript"), cycript);
    CYSetProperty(context, cycript, CYJSString("gc"), &Cycript_gc_callAsFunction);

    JSObjectRef Functor(JSObjectMakeConstructor(context, Functor_, &Functor_new));
    JSObjectSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, Functor, prototype_s)), Function_prototype);
    CYSetProperty(context, cycript, CYJSString("Functor"), Functor);

    CYSetProperty(context, cycript, CYJSString("Pointer"), JSObjectMakeConstructor(context, Pointer_, &Pointer_new));
    CYSetProperty(context, cycript, CYJSString("Type"), JSObjectMakeConstructor(context, Type_privateData::Class_, &Type_new));

    JSObjectRef all(JSObjectMake(context, All_, NULL));
    CYSetProperty(context, cycript, CYJSString("all"), all);

    JSObjectRef alls(JSObjectCallAsConstructor(context, Array, 0, NULL, &exception));
    CYThrow(context, exception);
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

        JSObjectSetPrototype(context, last, all);
    }

    CYSetProperty(context, global, CYJSString("$cyq"), &$cyq, kJSPropertyAttributeDontEnum);

    JSObjectRef System(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cy, CYJSString("System"), System);

    CYSetProperty(context, global, CYJSString("system"), System);
    CYSetProperty(context, System, CYJSString("args"), CYJSNull(context));
    //CYSetProperty(context, System, CYJSString("global"), global);
    CYSetProperty(context, System, CYJSString("print"), &System_print);

    if (CYBridgeEntry *entry = CYBridgeHash("1dlerror", 8))
        entry->cache_ = new cy::Functor(entry->value_, reinterpret_cast<void (*)()>(&dlerror));

    if (hooks_ != NULL && hooks_->SetupContext != NULL)
        (*hooks_->SetupContext)(context);

    CYArrayPush(context, alls, cycript);
}

JSGlobalContextRef CYGetJSContext() {
    CYInitializeDynamic();

    static JSGlobalContextRef context_;

    if (context_ == NULL) {
        context_ = JSGlobalContextCreate(Global_);
        CYSetupContext(context_);
    }

    return context_;
}
