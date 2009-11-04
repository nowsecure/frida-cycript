/* Cycript - Inlining/Optimizing JavaScript Compiler
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

#include <sqlite3.h>

#include "Internal.hpp"

#include <dlfcn.h>
#include <iconv.h>

#include "cycript.hpp"

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include "Pooling.hpp"

#include <sys/mman.h>

#include <iostream>
#include <ext/stdio_filebuf.h>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "Parser.hpp"
#include "Cycript.tab.hh"

#include "Error.hpp"
#include "JavaScript.hpp"
#include "String.hpp"

#ifdef __OBJC__
#define CYCatch_ \
    catch (NSException *error) { \
        CYThrow(context, error, exception); \
        return NULL; \
    }
#else
#define CYCatch_
#endif

char *sqlite3_column_pooled(apr_pool_t *pool, sqlite3_stmt *stmt, int n) {
    if (const unsigned char *value = sqlite3_column_text(stmt, n))
        return apr_pstrdup(pool, (const char *) value);
    else return NULL;
}

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

void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef (*callback)(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *), JSPropertyAttributes attributes = kJSPropertyAttributeNone) {
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
    // XXX: this is very wrong
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

template <typename Type_>
_finline size_t iconv_(size_t (*iconv)(iconv_t, Type_, size_t *, char **, size_t *), iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
    return iconv(cd, const_cast<Type_>(inbuf), inbytesleft, outbuf, outbytesleft);
}

CYUTF8String CYPoolUTF8String(apr_pool_t *pool, JSContextRef context, JSStringRef value) {
    _assert(pool != NULL);

    CYUTF16String utf16(CYCastUTF16String(value));
    const char *in(reinterpret_cast<const char *>(utf16.data));

#ifdef __GLIBC__
    iconv_t conversion(_syscall(iconv_open("UTF-8", "UCS-2")));
#else
    iconv_t conversion(_syscall(iconv_open("UTF-8", "UCS-2-INTERNAL")));
#endif

    size_t size(JSStringGetMaximumUTF8CStringSize(value));
    char *out(new(pool) char[size]);
    CYUTF8String utf8(out, size);

    size = utf16.size * 2;
    _syscall(iconv_(&iconv, conversion, const_cast<char **>(&in), &size, &out, &utf8.size));

    *out = '\0';
    utf8.size = out - utf8.data;

    _syscall(iconv_close(conversion));

    return utf8;
}

const char *CYPoolCString(apr_pool_t *pool, JSContextRef context, JSStringRef value) {
    CYUTF8String utf8(CYPoolUTF8String(pool, context, value));
    _assert(memchr(utf8.data, '\0', utf8.size) == NULL);
    return utf8.data;
}

const char *CYPoolCString(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    return JSValueIsNull(context, value) ? NULL : CYPoolCString(pool, context, CYJSString(context, value));
}
/* }}} */

/* Index Offsets {{{ */
size_t CYGetIndex(const CYUTF8String &value) {
    if (value.data[0] != '0') {
        size_t index(0);
        for (size_t i(0); i != value.size; ++i) {
            if (!DigitRange_[value.data[i]])
                return _not(size_t);
            index *= 10;
            index += value.data[i] - '0';
        }
        return index;
    } else if (value.size == 1)
        return 0;
    else
        return _not(size_t);
}

size_t CYGetIndex(apr_pool_t *pool, JSContextRef context, JSStringRef value) {
    return CYGetIndex(CYPoolUTF8String(pool, context, value));
}

// XXX: this isn't actually right
bool CYGetOffset(const char *value, ssize_t &index) {
    if (value[0] != '0') {
        char *end;
        index = strtol(value, &end, 10);
        if (value + strlen(value) == end)
            return true;
    } else if (value[1] == '\0') {
        index = 0;
        return true;
    }

    return false;
}
/* }}} */

/* JavaScript *ify {{{ */
void CYStringify(std::ostringstream &str, const char *data, size_t size) {
    unsigned quot(0), apos(0);
    for (const char *value(data), *end(data + size); value != end; ++value)
        if (*value == '"')
            ++quot;
        else if (*value == '\'')
            ++apos;

    bool single(quot > apos);

    str << (single ? '\'' : '"');

    for (const char *value(data), *end(data + size); value != end; ++value)
        switch (*value) {
            case '\\': str << "\\\\"; break;
            case '\b': str << "\\b"; break;
            case '\f': str << "\\f"; break;
            case '\n': str << "\\n"; break;
            case '\r': str << "\\r"; break;
            case '\t': str << "\\t"; break;
            case '\v': str << "\\v"; break;

            case '"':
                if (!single)
                    str << "\\\"";
                else goto simple;
            break;

            case '\'':
                if (single)
                    str << "\\'";
                else goto simple;
            break;

            default:
                // this test is designed to be "awewsome", generating neither warnings nor incorrect results
                if (*value < 0x20 || *value >= 0x7f)
                    str << "\\x" << std::setbase(16) << std::setw(2) << std::setfill('0') << unsigned(uint8_t(*value));
                else simple:
                    str << *value;
        }

    str << (single ? '\'' : '"');
}

void CYNumerify(std::ostringstream &str, double value) {
    char string[32];
    // XXX: I want this to print 1e3 rather than 1000
    sprintf(string, "%.17g", value);
    str << string;
}

bool CYIsKey(CYUTF8String value) {
    const char *data(value.data);
    size_t size(value.size);

    if (size == 0)
        return false;

    if (DigitRange_[data[0]]) {
        size_t index(CYGetIndex(value));
        if (index == _not(size_t))
            return false;
    } else {
        if (!WordStartRange_[data[0]])
            return false;
        for (size_t i(1); i != size; ++i)
            if (!WordEndRange_[data[i]])
                return false;
    }

    return true;
}
/* }}} */

static JSGlobalContextRef Context_;

static JSClassRef Functor_;
static JSClassRef Global_;
static JSClassRef Pointer_;
static JSClassRef Runtime_;
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

static JSStringRef Result_;

sqlite3 *Bridge_;

void CYFinalize(JSObjectRef object) {
    delete reinterpret_cast<CYData *>(JSObjectGetPrivate(object));
}

struct CStringMapLess :
    std::binary_function<const char *, const char *, bool>
{
    _finline bool operator ()(const char *lhs, const char *rhs) const {
        return strcmp(lhs, rhs) < 0;
    }
};

void Structor_(apr_pool_t *pool, sig::Type *&type) {
    if (
        type->primitive == sig::pointer_P &&
        type->data.data.type != NULL &&
        type->data.data.type->primitive == sig::struct_P &&
        strcmp(type->data.data.type->name, "_objc_class") == 0
    ) {
        type->primitive = sig::typename_P;
        type->data.data.type = NULL;
        return;
    }

    if (type->primitive != sig::struct_P || type->name == NULL)
        return;

    sqlite3_stmt *statement;

    _sqlcall(sqlite3_prepare(Bridge_,
        "select "
            "\"bridge\".\"mode\", "
            "\"bridge\".\"value\" "
        "from \"bridge\" "
        "where"
            " \"bridge\".\"mode\" in (3, 4) and"
            " \"bridge\".\"name\" = ?"
        " limit 1"
    , -1, &statement, NULL));

    _sqlcall(sqlite3_bind_text(statement, 1, type->name, -1, SQLITE_STATIC));

    int mode;
    const char *value;

    if (_sqlcall(sqlite3_step(statement)) == SQLITE_DONE) {
        mode = -1;
        value = NULL;
    } else {
        mode = sqlite3_column_int(statement, 0);
        value = sqlite3_column_pooled(pool, statement, 1);
    }

    _sqlcall(sqlite3_finalize(statement));

    switch (mode) {
        default:
            _assert(false);
        case -1:
            break;

        case 3: {
            sig::Parse(pool, &type->data.signature, value, &Structor_);
        } break;

        case 4: {
            sig::Signature signature;
            sig::Parse(pool, &signature, value, &Structor_);
            type = signature.elements[0].type;
        } break;
    }
}

JSClassRef Type_privateData::Class_;

struct Pointer :
    CYOwned
{
    Type_privateData *type_;
    size_t length_;

    Pointer(void *value, JSContextRef context, JSObjectRef owner, size_t length, sig::Type *type) :
        CYOwned(value, context, owner),
        type_(new(pool_) Type_privateData(type)),
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

typedef std::map<const char *, Type_privateData *, CStringMapLess> TypeMap;
static TypeMap Types_;

JSObjectRef CYMakeStruct(JSContextRef context, void *data, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Struct_privateData *internal(new Struct_privateData(context, owner));
    apr_pool_t *pool(internal->pool_);
    Type_privateData *typical(new(pool) Type_privateData(type, ffi));
    internal->type_ = typical;

    if (owner != NULL)
        internal->value_ = data;
    else {
        size_t size(typical->GetFFI()->size);
        void *copy(apr_palloc(internal->pool_, size));
        memcpy(copy, data, size);
        internal->value_ = copy;
    }

    return JSObjectMake(context, Struct_, internal);
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

double CYCastDouble(const char *value, size_t size) {
    char *end;
    double number(strtod(value, &end));
    if (end != value + size)
        return NAN;
    return number;
}

double CYCastDouble(const char *value) {
    return CYCastDouble(value, strlen(value));
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

JSValueRef CYCallAsFunction(JSContextRef context, JSObjectRef function, JSObjectRef _this, size_t count, JSValueRef arguments[]) {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectCallAsFunction(context, function, _this, count, arguments, &exception));
    CYThrow(context, exception);
    return value;
}

bool CYIsCallable(JSContextRef context, JSValueRef value) {
    return value != NULL && JSValueIsObject(context, value) && JSObjectIsFunction(context, (JSObjectRef) value);
}

static JSValueRef System_print(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count == 0)
        printf("\n");
    else {
        CYPool pool;
        printf("%s\n", CYPoolCString(pool, context, arguments[0]));
    }

    return CYJSUndefined(context);
} CYCatch }

static size_t Nonce_(0);

static JSValueRef $cyq(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYPool pool;
    const char *name(apr_psprintf(pool, "%s%"APR_SIZE_T_FMT"", CYPoolCString(pool, context, arguments[0]), Nonce_++));
    return CYCastJSValue(context, name);
}

static JSValueRef Cycript_gc_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    JSGarbageCollect(context);
    return CYJSUndefined(context);
}

const char *CYPoolCCYON(apr_pool_t *pool, JSContextRef context, JSValueRef value, JSValueRef *exception) { CYTry {
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
            return apr_pstrmemdup(pool, value.c_str(), value.size());
        } break;

        case kJSTypeString: {
            std::ostringstream str;
            CYUTF8String string(CYPoolUTF8String(pool, context, CYJSString(context, value)));
            CYStringify(str, string.data, string.size);
            std::string value(str.str());
            return apr_pstrmemdup(pool, value.c_str(), value.size());
        } break;

        case kJSTypeObject:
            return CYPoolCCYON(pool, context, (JSObjectRef) value);
        default:
            throw CYJSError(context, "JSValueGetType() == 0x%x", type);
    }
} CYCatch }

const char *CYPoolCCYON(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    JSValueRef exception(NULL);
    const char *cyon(CYPoolCCYON(pool, context, value, &exception));
    CYThrow(context, exception);
    return cyon;
}

const char *CYPoolCCYON(apr_pool_t *pool, JSContextRef context, JSObjectRef object) {
    JSValueRef toCYON(CYGetProperty(context, object, toCYON_s));
    if (CYIsCallable(context, toCYON)) {
        JSValueRef value(CYCallAsFunction(context, (JSObjectRef) toCYON, object, 0, NULL));
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
    return apr_pstrmemdup(pool, string.c_str(), string.size());
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
} CYCatch }

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, size_t length, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Pointer *internal(new Pointer(pointer, context, owner, length, type));
    return JSObjectMake(context, Pointer_, internal);
}

static JSObjectRef CYMakeFunctor(JSContextRef context, void (*function)(), const char *type) {
    cy::Functor *internal(new cy::Functor(type, function));
    return JSObjectMake(context, Functor_, internal);
}

static bool CYGetOffset(apr_pool_t *pool, JSContextRef context, JSStringRef value, ssize_t &index) {
    return CYGetOffset(CYPoolCString(pool, context, value), index);
}

void *CYCastPointer_(JSContextRef context, JSValueRef value) {
    switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return NULL;
        /*case kJSTypeObject:
            if (JSValueIsObjectOfClass(context, value, Pointer_)) {
                Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate((JSObjectRef) value)));
                return internal->value_;
            }*/
        default:
            double number(CYCastDouble(context, value));
            if (std::isnan(number))
                throw CYJSError(context, "cannot convert value to pointer");
            return reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<long long>(number)));
    }
}

void CYPoolFFI(apr_pool_t *pool, JSContextRef context, sig::Type *type, ffi_type *ffi, void *data, JSValueRef value) {
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
            *reinterpret_cast<const char **>(data) = CYPoolCString(pool, context, value);
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

static void FunctionClosure_(ffi_cif *cif, void *result, void **arguments, void *arg) {
    Closure_privateData *internal(reinterpret_cast<Closure_privateData *>(arg));

    JSContextRef context(internal->context_);

    size_t count(internal->cif_.nargs);
    JSValueRef values[count];

    for (size_t index(0); index != count; ++index)
        values[index] = CYFromFFI(context, internal->signature_.elements[1 + index].type, internal->cif_.arg_types[index], arguments[index]);

    JSValueRef value(CYCallAsFunction(context, internal->function_, NULL, count, values));
    CYPoolFFI(NULL, context, internal->signature_.elements[0].type, internal->cif_.rtype, result, value);
}

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const char *type, void (*callback)(ffi_cif *, void *, void **, void *)) {
    // XXX: in case of exceptions this will leak
    // XXX: in point of fact, this may /need/ to leak :(
    Closure_privateData *internal(new Closure_privateData(context, function, type));

    ffi_closure *closure((ffi_closure *) _syscall(mmap(
        NULL, sizeof(ffi_closure),
        PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
        -1, 0
    )));

    ffi_status status(ffi_prep_closure(closure, &internal->cif_, callback, internal));
    _assert(status == FFI_OK);

    _syscall(mprotect(closure, sizeof(*closure), PROT_READ | PROT_EXEC));

    internal->value_ = closure;

    return internal;
}

static JSObjectRef CYMakeFunctor(JSContextRef context, JSObjectRef function, const char *type) {
    Closure_privateData *internal(CYMakeFunctor_(context, function, type, &FunctionClosure_));
    return JSObjectMake(context, Functor_, internal);
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

static bool Index_(apr_pool_t *pool, JSContextRef context, Struct_privateData *internal, JSStringRef property, ssize_t &index, uint8_t *&base) {
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
} CYCatch }

static bool Pointer_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
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

    CYPoolFFI(NULL, context, typical->type_, ffi, base, value);
    return true;
} CYCatch }

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
} CYCatch }

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
} CYCatch }

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

JSValueRef CYCallFunction(apr_pool_t *pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, JSValueRef *exception, sig::Signature *signature, ffi_cif *cif, void (*function)()) { CYTry {
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
        CYPoolFFI(pool, context, element->type, ffi, values[index], arguments[index - setups]);
    }

    uint8_t value[cif->rtype->size];

    if (hooks_ != NULL && hooks_->CallFunction != NULL)
        (*hooks_->CallFunction)(context, cif, function, value, values);
    else
        ffi_call(cif, function, value, values);

    return CYFromFFI(context, signature->elements[0].type, cif->rtype, value, initialize);
} CYCatch }

static JSValueRef Functor_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYPool pool;
    cy::Functor *internal(reinterpret_cast<cy::Functor *>(JSObjectGetPrivate(object)));
    return CYCallFunction(pool, context, 0, NULL, count, arguments, false, exception, &internal->signature_, &internal->cif_, internal->GetValue());
}

static JSObjectRef CYMakeType(JSContextRef context, const char *type) {
    Type_privateData *internal(new Type_privateData(type));
    return JSObjectMake(context, Type_privateData::Class_, internal);
}

static JSObjectRef CYMakeType(JSContextRef context, sig::Type *type) {
    Type_privateData *internal(new Type_privateData(type));
    return JSObjectMake(context, Type_privateData::Class_, internal);
}

static void *CYCastSymbol(const char *name) {
    return dlsym(RTLD_DEFAULT, name);
}

static JSValueRef Runtime_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    CYUTF8String name(CYPoolUTF8String(pool, context, property));

    if (hooks_ != NULL && hooks_->RuntimeProperty != NULL)
        if (JSValueRef value = (*hooks_->RuntimeProperty)(context, name))
            return value;

    sqlite3_stmt *statement;

    _sqlcall(sqlite3_prepare(Bridge_,
        "select "
            "\"bridge\".\"mode\", "
            "\"bridge\".\"value\" "
        "from \"bridge\" "
        "where"
            " \"bridge\".\"name\" = ?"
        " limit 1"
    , -1, &statement, NULL));

    _sqlcall(sqlite3_bind_text(statement, 1, name.data, name.size, SQLITE_STATIC));

    int mode;
    const char *value;

    if (_sqlcall(sqlite3_step(statement)) == SQLITE_DONE) {
        mode = -1;
        value = NULL;
    } else {
        mode = sqlite3_column_int(statement, 0);
        value = sqlite3_column_pooled(pool, statement, 1);
    }

    _sqlcall(sqlite3_finalize(statement));

    switch (mode) {
        default:
            _assert(false);
        case -1:
            return NULL;

        case 0:
            return JSEvaluateScript(CYGetJSContext(context), CYJSString(value), NULL, NULL, 0, NULL);

        case 1:
            if (void (*symbol)() = reinterpret_cast<void (*)()>(CYCastSymbol(name.data)))
                return CYMakeFunctor(context, symbol, value);
            else return NULL;

        case 2:
            if (void *symbol = CYCastSymbol(name.data)) {
                // XXX: this is horrendously inefficient
                sig::Signature signature;
                sig::Parse(pool, &signature, value, &Structor_);
                ffi_cif cif;
                sig::sig_ffi_cif(pool, &sig::ObjectiveC, &signature, &cif);
                return CYFromFFI(context, signature.elements[0].type, cif.rtype, symbol);
            } else return NULL;

        // XXX: implement case 3
        case 4:
            return CYMakeType(context, value);
    }
} CYCatch }

static JSObjectRef Pointer_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to Functor constructor");

    CYPool pool;

    void *value(CYCastPointer<void *>(context, arguments[0]));
    const char *type(CYPoolCString(pool, context, arguments[1]));

    sig::Signature signature;
    sig::Parse(pool, &signature, type, &Structor_);

    return CYMakePointer(context, value, _not(size_t), signature.elements[0].type, NULL, NULL);
} CYCatch }

static JSObjectRef Type_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Type constructor");
    CYPool pool;
    const char *type(CYPoolCString(pool, context, arguments[0]));
    return CYMakeType(context, type);
} CYCatch }

static JSValueRef Type_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

    sig::Type type;

    if (JSStringIsEqualToUTF8CString(property, "$cyi")) {
        type.primitive = sig::pointer_P;
        type.data.data.size = 0;
    } else {
        CYPool pool;
        size_t index(CYGetIndex(pool, context, property));
        if (index == _not(size_t))
            return NULL;
        type.primitive = sig::array_P;
        type.data.data.size = index;
    }

    type.name = NULL;
    type.flags = 0;

    type.data.data.type = internal->type_;

    return CYMakeType(context, &type);
} CYCatch }

static JSValueRef Type_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));

    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to type cast function");
    sig::Type *type(internal->type_);
    ffi_type *ffi(internal->GetFFI());
    // XXX: alignment?
    uint8_t value[ffi->size];
    CYPool pool;
    CYPoolFFI(pool, context, type, ffi, value, arguments[0]);
    return CYFromFFI(context, type, ffi, value);
} CYCatch }

static JSObjectRef Type_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 0)
        throw CYJSError(context, "incorrect number of arguments to type cast function");
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
} CYCatch }

static JSObjectRef Functor_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to Functor constructor");
    CYPool pool;
    const char *type(CYPoolCString(pool, context, arguments[1]));
    return CYMakeFunctor(context, arguments[0], type);
} CYCatch }

static JSValueRef CYValue_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
} CYCatch }

static JSValueRef CYValue_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return CYValue_callAsFunction_valueOf(context, object, _this, count, arguments, exception);
}

static JSValueRef CYValue_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));
    char string[32];
    sprintf(string, "%p", internal->value_);
    return CYCastJSValue(context, string);
} CYCatch }

static JSValueRef Pointer_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(_this)));
    if (internal->length_ != _not(size_t))
        // XXX: maybe dynamically look up Array.toCYON?
        return Array_callAsFunction_toCYON(context, object, _this, count, arguments, exception);
    else {
        char string[32];
        sprintf(string, "%p", internal->value_);
        return CYCastJSValue(context, string);
    }
} CYCatch }

static JSValueRef Type_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
    CYPool pool;
    const char *type(sig::Unparse(pool, internal->type_));
    return CYCastJSValue(context, CYJSString(type));
} CYCatch }

static JSValueRef Type_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
    CYPool pool;
    const char *type(sig::Unparse(pool, internal->type_));
    size_t size(strlen(type));
    char *cyon(new(pool) char[12 + size + 1]);
    memcpy(cyon, "new Type(\"", 10);
    cyon[12 + size] = '\0';
    cyon[12 + size - 2] = '"';
    cyon[12 + size - 1] = ')';
    memcpy(cyon + 10, type, size);
    return CYCastJSValue(context, CYJSString(cyon));
} CYCatch }

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

static JSStaticFunction Type_staticFunctions[4] = {
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

const char *CYExecute(apr_pool_t *pool, const char *code) {
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

    if (exception != NULL) { error:
        result = exception;
        exception = NULL;
    }

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

static apr_pool_t *Pool_;

static bool initialized_;

void CYInitialize() {
    if (!initialized_)
        initialized_ = true;
    else return;

    _aprcall(apr_initialize());
    _aprcall(apr_pool_create(&Pool_, NULL));
    _sqlcall(sqlite3_open("/usr/lib/libcycript.db", &Bridge_));

    JSObjectMakeArray$ = reinterpret_cast<JSObjectRef (*)(JSContextRef, size_t, const JSValueRef[], JSValueRef *)>(dlsym(RTLD_DEFAULT, "JSObjectMakeArray"));

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.className = "Functor";
    definition.staticFunctions = cy::Functor::StaticFunctions;
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
    definition.staticFunctions = Type_staticFunctions;
    definition.getProperty = &Type_getProperty;
    definition.callAsFunction = &Type_callAsFunction;
    definition.callAsConstructor = &Type_callAsConstructor;
    definition.finalize = &CYFinalize;
    Type_privateData::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Runtime";
    definition.getProperty = &Runtime_getProperty;
    Runtime_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
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

    Result_ = JSStringCreateWithUTF8CString("_");

    if (hooks_ != NULL && hooks_->Initialize != NULL)
        (*hooks_->Initialize)();
}

apr_pool_t *CYGetGlobalPool() {
    CYInitialize();
    return Pool_;
}

void CYThrow(JSContextRef context, JSValueRef value) {
    if (value != NULL)
        throw CYJSError(context, value);
}

const char *CYJSError::PoolCString(apr_pool_t *pool) const {
    // XXX: this used to be CYPoolCString
    return CYPoolCCYON(pool, context_, value_);
}

JSValueRef CYJSError::CastJSValue(JSContextRef context) const {
    // XXX: what if the context is different?
    return value_;
}

void CYThrow(const char *format, ...) {
    va_list args;
    va_start (args, format);
    throw CYPoolError(format, args);
    // XXX: does this matter? :(
    va_end (args);
}

const char *CYPoolError::PoolCString(apr_pool_t *pool) const {
    return apr_pstrdup(pool, message_);
}

CYPoolError::CYPoolError(const char *format, ...) {
    va_list args;
    va_start (args, format);
    message_ = apr_pvsprintf(pool_, format, args);
    va_end (args);
}

CYPoolError::CYPoolError(const char *format, va_list args) {
    message_ = apr_pvsprintf(pool_, format, args);
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
    va_start (args, format);
    const char *message(apr_pvsprintf(pool, format, args));
    va_end (args);

    value_ = CYCastJSError(context, message);
}

JSGlobalContextRef CYGetJSContext(JSContextRef context) {
    // XXX: do something better
    return Context_;
}

void CYSetupContext(JSGlobalContextRef context) {
    JSObjectRef global(CYGetGlobalObject(context));

    JSObjectRef cy(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, global, cy_s, cy);

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

    JSObjectSetPrototype(context, global, JSObjectMake(context, Runtime_, NULL));

    CYSetProperty(context, Array_prototype, toCYON_s, &Array_callAsFunction_toCYON, kJSPropertyAttributeDontEnum);

    JSObjectRef Functor(JSObjectMakeConstructor(context, Functor_, &Functor_new));
    JSObjectSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, Functor, prototype_s)), Function_prototype);
    CYSetProperty(context, global, CYJSString("Functor"), Functor);

    CYSetProperty(context, global, CYJSString("Pointer"), JSObjectMakeConstructor(context, Pointer_, &Pointer_new));
    CYSetProperty(context, global, CYJSString("Type"), JSObjectMakeConstructor(context, Type_privateData::Class_, &Type_new));

    JSObjectRef cycript(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, global, CYJSString("Cycript"), cycript);
    CYSetProperty(context, cycript, CYJSString("gc"), &Cycript_gc_callAsFunction);

    CYSetProperty(context, global, CYJSString("$cyq"), &$cyq);

    JSObjectRef System(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cy, CYJSString("System"), Function);

    CYSetProperty(context, global, CYJSString("system"), System);
    CYSetProperty(context, System, CYJSString("args"), CYJSNull(context));
    //CYSetProperty(context, System, CYJSString("global"), global);
    CYSetProperty(context, System, CYJSString("print"), &System_print);

    if (hooks_ != NULL && hooks_->SetupContext != NULL)
        (*hooks_->SetupContext)(context);
}

JSGlobalContextRef CYGetJSContext() {
    CYInitialize();

    if (Context_ == NULL) {
        Context_ = JSGlobalContextCreate(Global_);
        CYSetupContext(Context_);
    }

    return Context_;
}
