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
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "cycript.hpp"

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include "Pooling.hpp"
#include "Execute.hpp"

#include <sys/mman.h>
#include <sys/stat.h>

#include <iostream>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "Code.hpp"
#include "Decode.hpp"
#include "Error.hpp"
#include "JavaScript.hpp"
#include "String.hpp"

struct CYHooks *hooks_;

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

void CYSetPrototype(JSContextRef context, JSObjectRef object, JSValueRef value) {
    JSObjectSetPrototype(context, object, value);
    _assert(JSObjectGetPrototype(context, object) == value);
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
    return _jsccall(JSValueToStringCopy, context, value);
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
JSStringRef cyi_s;
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

size_t CYArrayLength(JSContextRef context, JSObjectRef array) {
    return CYCastDouble(context, CYGetProperty(context, array, length_s));
}

JSValueRef CYArrayGet(JSContextRef context, JSObjectRef array, size_t index) {
    return _jsccall(JSObjectGetPropertyAtIndex, context, array, index);
}

void CYArrayPush(JSContextRef context, JSObjectRef array, JSValueRef value) {
    JSValueRef arguments[1];
    arguments[0] = value;
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
    _jsccall(JSObjectCallAsFunction, context, CYCastJSObject(context, CYGetProperty(context, Array, push_s)), array, 1, arguments);
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

static JSValueRef $cyq(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    const char *name(pool.strcat(CYPoolCString(pool, context, arguments[0]), pool.itoa(Nonce_++), NULL));
    return CYCastJSValue(context, name);
} CYCatch(NULL) }

static void (*JSSynchronousGarbageCollectForDebugging$)(JSContextRef);

void CYGarbageCollect(JSContextRef context) {
    (JSSynchronousGarbageCollectForDebugging$ ?: &JSGarbageCollect)(context);
}

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
            CYStringify(str, string.data, string.size);
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
        JSValueRef arguments[1] = {CYCastJSValue(context, static_cast<double>(reinterpret_cast<uintptr_t>(&objects)))};
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
            CYStringify(str, string.data, string.size);

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
    CYStringify(str, string.data, string.size);

    std::string value(str.str());
    return CYCastJSValue(context, CYJSString(CYUTF8String(value.c_str(), value.size())));
} CYCatch(NULL) }

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, size_t length, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Pointer *internal(new Pointer(pointer, context, owner, length, type));
    return JSObjectMake(context, Pointer_, internal);
}

static JSObjectRef CYMakeFunctor(JSContextRef context, void (*function)(), const sig::Signature &signature) {
    return JSObjectMake(context, Functor_, new cy::Functor(signature, function));
}

static JSObjectRef CYMakeFunctor(JSContextRef context, const char *symbol, const char *encoding, void **cache) {
    cy::Functor *internal;
    if (*cache != NULL)
        internal = reinterpret_cast<cy::Functor *>(*cache);
    else {
        void (*function)()(reinterpret_cast<void (*)()>(CYCastSymbol(symbol)));
        if (function == NULL)
            return NULL;

        internal = new cy::Functor(encoding, function);
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

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const sig::Signature &signature, void (*callback)(ffi_cif *, void *, void **, void *)) {
    // XXX: in case of exceptions this will leak
    // XXX: in point of fact, this may /need/ to leak :(
    Closure_privateData *internal(new Closure_privateData(context, function, signature));

#if defined(__APPLE__) && (defined(__arm__) || defined(__arm64__))
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

static JSObjectRef CYMakeFunctor(JSContextRef context, JSObjectRef function, const sig::Signature &signature) {
    Closure_privateData *internal(CYMakeFunctor_(context, function, signature, &FunctionClosure_));
    JSObjectRef object(JSObjectMake(context, Functor_, internal));
    // XXX: see above notes about needing to leak
    JSValueProtect(CYGetJSContext(context), object);
    return object;
}

JSObjectRef CYGetCachedObject(JSContextRef context, JSStringRef name) {
    return CYCastJSObject(context, CYGetProperty(context, CYCastJSObject(context, CYGetProperty(context, CYGetGlobalObject(context), cy_s)), name));
}

static JSObjectRef CYMakeFunctor(JSContextRef context, JSValueRef value, const sig::Signature &signature) {
    JSObjectRef Function(CYGetCachedObject(context, CYJSString("Function")));

    bool function(_jsccall(JSValueIsInstanceOfConstructor, context, value, Function));
    if (function) {
        JSObjectRef function(CYCastJSObject(context, value));
        return CYMakeFunctor(context, function, signature);
    } else {
        void (*function)()(CYCastPointer<void (*)()>(context, value));
        return CYMakeFunctor(context, function, signature);
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
    sig::Type &type(*typical->type_);

    ssize_t offset;
    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        offset = 0;
    else if (!CYGetOffset(pool, context, property, offset))
        return NULL;

    if (type.primitive == sig::function_P)
        return CYMakeFunctor(context, reinterpret_cast<void (*)()>(internal->value_), type.data.signature);

    ffi_type *ffi(typical->GetFFI());

    uint8_t *base(reinterpret_cast<uint8_t *>(internal->value_));
    base += ffi->size * offset;

    JSObjectRef owner(internal->GetOwner() ?: object);
    return CYFromFFI(context, &type, ffi, base, false, owner);
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

static JSValueRef Struct_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(_this)));
    Type_privateData *typical(internal->type_);
    return CYMakePointer(context, internal->value_, _not(size_t), typical->type_, typical->ffi_, _this);
} CYCatch(NULL) }

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

JSValueRef CYCallFunction(CYPool &pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, sig::Signature *signature, ffi_cif *cif, void (*function)()) {
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
}

static JSValueRef Functor_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    return CYCallFunction(pool, context, 0, NULL, count, arguments, false, &internal->signature_, &internal->cif_, internal->GetValue());
} CYCatch(NULL) }

JSObjectRef CYMakeType(JSContextRef context, const char *encoding) {
    Type_privateData *internal(new Type_privateData(encoding));
    return JSObjectMake(context, Type_privateData::Class_, internal);
}

JSObjectRef CYMakeType(JSContextRef context, sig::Type *type) {
    Type_privateData *internal(new Type_privateData(type));
    return JSObjectMake(context, Type_privateData::Class_, internal);
}

JSObjectRef CYMakeType(JSContextRef context, sig::Signature *signature) {
    CYPool pool;

    sig::Type type;
    type.name = NULL;
    type.flags = 0;

    type.primitive = sig::function_P;
    sig::Copy(pool, type.data.signature, *signature);

    return CYMakeType(context, &type);
}

static bool All_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    for (size_t i(0), count(CYArrayLength(context, alls)); i != count; ++i)
        if (JSObjectRef space = CYCastJSObject(context, CYArrayGet(context, alls, count - i - 1)))
            if (CYHasProperty(context, space, property))
                return true;

    CYPool pool;
    CYUTF8String name(CYPoolUTF8String(pool, context, property));

    size_t length(name.size);
    char keyed[length + 2];
    memcpy(keyed + 1, name.data, length + 1);

    static const char *modes = "0124";
    for (size_t i(0); i != 4; ++i) {
        keyed[0] = modes[i];
        if (CYBridgeHash(keyed, length + 1) != NULL)
            return true;
    }

    return false;
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

static JSValueRef Type_callAsFunction_$With(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], sig::Primitive primitive, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    CYPool pool;

    sig::Type type;
    type.name = NULL;
    type.flags = 0;

    type.primitive = primitive;
    type.data.signature.elements = new(pool) sig::Element[1 + count];
    type.data.signature.count = 1 + count;

    type.data.signature.elements[0].name = NULL;
    type.data.signature.elements[0].type = internal->type_;
    type.data.signature.elements[0].offset = _not(size_t);

    for (size_t i(0); i != count; ++i) {
        sig::Element &element(type.data.signature.elements[i + 1]);
        element.name = NULL;
        element.offset = _not(size_t);

        JSObjectRef object(CYCastJSObject(context, arguments[i]));
        _assert(JSValueIsObjectOfClass(context, object, Type_privateData::Class_));
        Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

        element.type = internal->type_;
    }

    return CYMakeType(context, &type);
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

static JSValueRef Type_callAsFunction_blockWith(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return Type_callAsFunction_$With(context, object, _this, count, arguments, sig::block_P, exception);
}

static JSValueRef Type_callAsFunction_constant(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.constant");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type(*internal->type_);
    type.flags |= JOC_TYPE_CONST;
    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_long(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.long");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type(*internal->type_);

    switch (type.primitive) {
        case sig::short_P: type.primitive = sig::int_P; break;
        case sig::int_P: type.primitive = sig::long_P; break;
        case sig::long_P: type.primitive = sig::longlong_P; break;
        default: throw CYJSError(context, "invalid type argument to Type.long");
    }

    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_short(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.short");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type(*internal->type_);

    switch (type.primitive) {
        case sig::int_P: type.primitive = sig::short_P; break;
        case sig::long_P: type.primitive = sig::int_P; break;
        case sig::longlong_P: type.primitive = sig::long_P; break;
        default: throw CYJSError(context, "invalid type argument to Type.short");
    }

    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_signed(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.signed");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type(*internal->type_);

    switch (type.primitive) {
        case sig::char_P: case sig::uchar_P: type.primitive = sig::char_P; break;
        case sig::short_P: case sig::ushort_P: type.primitive = sig::short_P; break;
        case sig::int_P: case sig::uint_P: type.primitive = sig::int_P; break;
        case sig::long_P: case sig::ulong_P: type.primitive = sig::long_P; break;
        case sig::longlong_P: case sig::ulonglong_P: type.primitive = sig::longlong_P; break;
        default: throw CYJSError(context, "invalid type argument to Type.signed");
    }

    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_unsigned(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.unsigned");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type(*internal->type_);

    switch (type.primitive) {
        case sig::char_P: case sig::uchar_P: type.primitive = sig::uchar_P; break;
        case sig::short_P: case sig::ushort_P: type.primitive = sig::ushort_P; break;
        case sig::int_P: case sig::uint_P: type.primitive = sig::uint_P; break;
        case sig::long_P: case sig::ulong_P: type.primitive = sig::ulong_P; break;
        case sig::longlong_P: case sig::ulonglong_P: type.primitive = sig::ulonglong_P; break;
        default: throw CYJSError(context, "invalid type argument to Type.unsigned");
    }

    return CYMakeType(context, &type);
} CYCatch(NULL) }

static JSValueRef Type_callAsFunction_functionWith(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return Type_callAsFunction_$With(context, object, _this, count, arguments, sig::function_P, exception);
}

static JSValueRef Type_callAsFunction_pointerTo(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to Type.pointerTo");
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));

    sig::Type type;
    type.name = NULL;

    if (internal->type_->primitive == sig::char_P) {
        type.flags = internal->type_->flags;
        type.primitive = sig::string_P;
        type.data.data.type = NULL;
        type.data.data.size = 0;
    } else {
        type.flags = 0;
        type.primitive = sig::pointer_P;
        type.data.data.type = internal->type_;
        type.data.data.size = 0;
    }

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

    if (internal->type_->primitive == sig::function_P)
        return CYMakeFunctor(context, arguments[0], internal->type_->data.signature);

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

    void *value(calloc(1, internal->GetFFI()->size));
    return CYMakePointer(context, value, length, type, NULL, NULL);
} CYCatch(NULL) }

static JSObjectRef Functor_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to Functor constructor");
    CYPool pool;
    const char *encoding(CYPoolCString(pool, context, arguments[1]));
    sig::Signature signature;
    sig::Parse(pool, &signature, encoding, &Structor_);
    return CYMakeFunctor(context, arguments[0], signature);
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
    std::set<void *> *objects(CYCastObjects(context, _this, count, arguments));

    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(_this)));
    if (internal->length_ != _not(size_t)) {
        JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
        JSObjectRef toCYON(CYCastJSObject(context, CYGetProperty(context, Array, toCYON_s)));
        return CYCallAsFunction(context, toCYON, _this, count, arguments);
    } else if (internal->type_->type_ == NULL) pointer: {
        char string[32];
        sprintf(string, "%p", internal->value_);
        return CYCastJSValue(context, string);
    } try {
        JSValueRef value(CYGetProperty(context, _this, cyi_s));
        if (JSValueIsUndefined(context, value))
            goto pointer;
        CYPool pool;
        return CYCastJSValue(context, pool.strcat("&", CYPoolCCYON(pool, context, value, objects), NULL));
    } catch (const CYException &e) {
        goto pointer;
    }
} CYCatch(NULL) }

static JSValueRef Pointer_getProperty_type(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    return CYMakeType(context, internal->type_->type_);
} CYCatch(NULL) }

static JSValueRef Functor_getProperty_type(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    return CYMakeType(context, &internal->signature_);
} CYCatch(NULL) }

static JSValueRef Type_getProperty_alignment(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->GetFFI()->alignment);
} CYCatch(NULL) }

static JSValueRef Type_getProperty_name(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, internal->type_->name);
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
    std::ostringstream out;
    CYOptions options;
    CYOutput output(out, options);
    (new(pool) CYEncodedType(Decode(pool, internal->type_)))->Output(output, CYNoFlags);
    return CYCastJSValue(context, CYJSString(out.str().c_str()));
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

static JSStaticValue Pointer_staticValues[2] = {
    {"type", &Pointer_getProperty_type, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
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

namespace cy {
    JSStaticValue const * const Functor::StaticValues = Functor_staticValues;
}

static JSStaticValue Type_staticValues[4] = {
    {"alignment", &Type_getProperty_alignment, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"name", &Type_getProperty_name, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"size", &Type_getProperty_size, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Type_staticFunctions[14] = {
    {"arrayOf", &Type_callAsFunction_arrayOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"blockWith", &Type_callAsFunction_blockWith, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"constant", &Type_callAsFunction_constant, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"functionWith", &Type_callAsFunction_functionWith, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"long", &Type_callAsFunction_long, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"pointerTo", &Type_callAsFunction_pointerTo, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"short", &Type_callAsFunction_short, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"signed", &Type_callAsFunction_signed, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"withName", &Type_callAsFunction_withName, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toCYON", &Type_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Type_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Type_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"unsigned", &Type_callAsFunction_unsigned, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSObjectRef (*JSObjectMakeArray$)(JSContextRef, size_t, const JSValueRef[], JSValueRef *);

void CYSetArgs(int argc, const char *argv[]) {
    JSContextRef context(CYGetJSContext());
    JSValueRef args[argc];
    for (int i(0); i != argc; ++i)
        args[i] = CYCastJSValue(context, argv[i]);

    JSObjectRef array;
    if (JSObjectMakeArray$ != NULL)
        array = _jsccall(*JSObjectMakeArray$, context, argc, args);
    else {
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

class ExecutionHandle {
  private:
    JSContextRef context_;
    void *handle_;

  public:
    ExecutionHandle(JSContextRef context) :
        context_(context)
    {
        if (hooks_ != NULL && hooks_->ExecuteStart != NULL)
            handle_ = (*hooks_->ExecuteStart)(context_);
        else
            handle_ = NULL;
    }

    ~ExecutionHandle() {
        if (hooks_ != NULL && hooks_->ExecuteEnd != NULL)
            (*hooks_->ExecuteEnd)(context_, handle_);
    }
};

const char *CYExecute(JSContextRef context, CYPool &pool, CYUTF8String code) {
    JSValueRef exception(NULL);

    ExecutionHandle handle(context);

    JSValueRef result; try {
        result = JSEvaluateScript(context, CYJSString(code), NULL, NULL, 0, &exception);
    } catch (const char *error) {
        return error;
    }

    if (exception != NULL) error:
        return CYPoolCString(pool, context, CYJSString(context, exception));

    if (JSValueIsUndefined(context, result))
        return NULL;

    const char *json; try {
        std::set<void *> objects;
        json = CYPoolCCYON(pool, context, result, objects, &exception);
    } catch (const char *error) {
        return error;
    }

    if (exception != NULL)
        goto error;

    CYSetProperty(context, CYGetGlobalObject(context), Result_, result);

    return json;
}

static bool initialized_ = false;

void CYInitializeDynamic() {
    if (!initialized_)
        initialized_ = true;
    else return;

    JSObjectMakeArray$ = reinterpret_cast<JSObjectRef (*)(JSContextRef, size_t, const JSValueRef[], JSValueRef *)>(dlsym(RTLD_DEFAULT, "JSObjectMakeArray"));
    JSSynchronousGarbageCollectForDebugging$ = reinterpret_cast<void (*)(JSContextRef)>(dlsym(RTLD_DEFAULT, "JSSynchronousGarbageCollectForDebugging"));

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.className = "All";
    definition.hasProperty = &All_hasProperty;
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
    definition.staticValues = Pointer_staticValues;
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
    cyi_s = JSStringCreateWithUTF8CString("$cyi");
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
    std::set<void *> objects;
    // XXX: this used to be CYPoolCString
    return CYPoolCCYON(pool, context_, value_, objects);
}

JSValueRef CYJSError::CastJSValue(JSContextRef context) const {
    // XXX: what if the context is different?
    return value_;
}

JSValueRef CYCastJSError(JSContextRef context, const char *message) {
    JSObjectRef Error(CYGetCachedObject(context, CYJSString("Error")));
    JSValueRef arguments[1] = {CYCastJSValue(context, message)};
    return _jsccall(JSObjectCallAsConstructor, context, Error, 1, arguments);
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

extern "C" bool CydgetMemoryParse(const uint16_t **data, size_t *size);

void *CYMapFile(const char *path, size_t *psize) {
    int fd(_syscall_(open(path, O_RDONLY), 1, {ENOENT}));
    if (fd == -1)
        return NULL;

    struct stat stat;
    _syscall(fstat(fd, &stat));
    size_t size(stat.st_size);

    *psize = size;

    void *base;
    _syscall(base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));

    _syscall(close(fd));
    return base;
}

static JSValueRef require(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    _assert(count == 1);
    CYPool pool;

    Dl_info addr;
    _assert(dladdr(reinterpret_cast<void *>(&require), &addr) != 0);
    char *lib(pool.strdup(addr.dli_fname));

    char *slash(strrchr(lib, '/'));
    _assert(slash != NULL);
    *slash = '\0';

    CYJSString property("exports");
    JSObjectRef module;

    const char *name(CYPoolCString(pool, context, arguments[0]));
    const char *path(pool.strcat(lib, "/cycript0.9/", name, ".cy", NULL));

    CYJSString key(path);
    JSObjectRef modules(CYGetCachedObject(context, CYJSString("modules")));
    JSValueRef cache(CYGetProperty(context, modules, key));

    if (!JSValueIsUndefined(context, cache))
        module = CYCastJSObject(context, cache);
    else {
        CYUTF8String code;
        code.data = reinterpret_cast<char *>(CYMapFile(path, &code.size));

        if (code.data == NULL) {
            if (strchr(name, '/') == NULL && (
                dlopen(pool.strcat("/System/Library/Frameworks/", name, ".framework/", name, NULL), RTLD_LAZY | RTLD_GLOBAL) != NULL ||
                dlopen(pool.strcat("/System/Library/PrivateFrameworks/", name, ".framework/", name, NULL), RTLD_LAZY | RTLD_GLOBAL) != NULL ||
            false))
                return CYJSUndefined(NULL);

            CYThrow("Can't find module: %s", name);
        }

        module = JSObjectMake(context, NULL, NULL);
        CYSetProperty(context, modules, key, module);

        JSObjectRef exports(JSObjectMake(context, NULL, NULL));
        CYSetProperty(context, module, property, exports);

        std::stringstream wrap;
        wrap << "(function (exports, require, module) { " << code << "\n});";
        code = CYPoolCode(pool, wrap);

        JSValueRef value(_jsccall(JSEvaluateScript, context, CYJSString(code), NULL, NULL, 0));
        JSObjectRef function(CYCastJSObject(context, value));

        JSValueRef arguments[3] = { exports, JSObjectMakeFunctionWithCallback(context, CYJSString("require"), &require), module };
        CYCallAsFunction(context, function, NULL, 3, arguments);
    }

    return CYGetProperty(context, module, property);
} CYCatch(NULL) }

extern "C" void CYSetupContext(JSGlobalContextRef context) {
    CYInitializeDynamic();

    JSObjectRef global(CYGetGlobalObject(context));

    JSObjectRef cy(JSObjectMake(context, Context_, new Context(context)));
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
/* }}} */

    CYSetProperty(context, Array_prototype, toCYON_s, &Array_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);
    CYSetProperty(context, String_prototype, toCYON_s, &String_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);

    JSObjectRef cycript(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, global, CYJSString("Cycript"), cycript);
    CYSetProperty(context, cycript, CYJSString("gc"), &Cycript_gc_callAsFunction);

    JSObjectRef Functor(JSObjectMakeConstructor(context, Functor_, &Functor_new));
    CYSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, Functor, prototype_s)), Function_prototype);
    CYSetProperty(context, cycript, CYJSString("Functor"), Functor);

    CYSetProperty(context, cycript, CYJSString("Pointer"), JSObjectMakeConstructor(context, Pointer_, &Pointer_new));
    CYSetProperty(context, cycript, CYJSString("Type"), JSObjectMakeConstructor(context, Type_privateData::Class_, &Type_new));

    JSObjectRef modules(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cy, CYJSString("modules"), modules);

    JSObjectRef all(JSObjectMake(context, All_, NULL));
    CYSetProperty(context, cycript, CYJSString("all"), all);

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

        CYSetPrototype(context, last, all);
    }

    CYSetProperty(context, global, CYJSString("$cyq"), &$cyq, kJSPropertyAttributeDontEnum);

    JSObjectRef System(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cy, CYJSString("System"), System);

    CYSetProperty(context, all, CYJSString("require"), &require, kJSPropertyAttributeDontEnum);

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

static JSGlobalContextRef context_;

JSGlobalContextRef CYGetJSContext() {
    CYInitializeDynamic();

    if (context_ == NULL) {
        context_ = JSGlobalContextCreate(Global_);
        CYSetupContext(context_);
    }

    return context_;
}

void CYDestroyContext() {
    if (context_ == NULL)
        return;
    JSGlobalContextRelease(context_);
    context_ = NULL;
}
