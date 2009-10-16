/* Cycript - Remove Execution Server and Disassembler
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

#define _GNU_SOURCE

#include <substrate.h>
#include "cycript.hpp"

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include "Pooling.hpp"
#include "Struct.hpp"

#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFLogUtilities.h>

#include <WebKit/WebScriptObject.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <sys/mman.h>

#include <iostream>
#include <ext/stdio_filebuf.h>
#include <set>
#include <map>

#include <sstream>
#include <cmath>

#include "Parser.hpp"
#include "Cycript.tab.hh"

#include <fcntl.h>

#include <apr-1/apr_thread_proc.h>

#undef _assert
#undef _trace

#define _assert(test) do { \
    if (!(test)) \
        @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"_assert(%s):%s(%u):%s", #test, __FILE__, __LINE__, __FUNCTION__] userInfo:nil]; \
} while (false)

#define _trace() do { \
    CFLog(kCFLogLevelNotice, CFSTR("_trace():%u"), __LINE__); \
} while (false)

#define CYPoolTry { \
    id _saved(nil); \
    NSAutoreleasePool *_pool([[NSAutoreleasePool alloc] init]); \
    @try
#define CYPoolCatch(value) \
    @catch (NSException *error) { \
        _saved = [error retain]; \
        @throw; \
        return value; \
    } @finally { \
        [_pool release]; \
        if (_saved != nil) \
            [_saved autorelease]; \
    } \
}

static JSGlobalContextRef Context_;
static JSObjectRef System_;
static JSObjectRef ObjectiveC_;

static JSClassRef Functor_;
static JSClassRef Instance_;
static JSClassRef Internal_;
static JSClassRef Pointer_;
static JSClassRef Runtime_;
static JSClassRef Selector_;
static JSClassRef Struct_;
static JSClassRef Type_;

static JSClassRef ObjectiveC_Classes_;
static JSClassRef ObjectiveC_Image_Classes_;
static JSClassRef ObjectiveC_Images_;
static JSClassRef ObjectiveC_Protocols_;

static JSObjectRef Array_;
static JSObjectRef Function_;

static JSStringRef Result_;

static JSStringRef length_;
static JSStringRef message_;
static JSStringRef name_;
static JSStringRef toCYON_;
static JSStringRef toJSON_;

static Class NSCFBoolean_;
static Class NSCFType_;
static Class NSMessageBuilder_;
static Class NSZombie_;
static Class Object_;

static NSArray *Bridge_;

struct CYData {
    apr_pool_t *pool_;

    virtual ~CYData() {
    }

    static void *operator new(size_t size, apr_pool_t *pool) {
        void *data(apr_palloc(pool, size));
        reinterpret_cast<CYData *>(data)->pool_ = pool;
        return data;
    }

    static void *operator new(size_t size) {
        apr_pool_t *pool;
        apr_pool_create(&pool, NULL);
        return operator new(size, pool);
    }

    static void operator delete(void *data) {
        apr_pool_destroy(reinterpret_cast<CYData *>(data)->pool_);
    }

    static void Finalize(JSObjectRef object) {
        delete reinterpret_cast<CYData *>(JSObjectGetPrivate(object));
    }
};

class Type_privateData;

struct CYValue :
    CYData
{
    void *value_;

    CYValue() {
    }

    CYValue(void *value) :
        value_(value)
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

struct Selector_privateData :
    CYValue
{
    Selector_privateData(SEL value) :
        CYValue(value)
    {
    }

    SEL GetValue() const {
        return reinterpret_cast<SEL>(value_);
    }

    virtual Type_privateData *GetType() const;
};

struct Instance :
    CYValue
{
    enum Flags {
        None          = 0,
        Transient     = (1 << 0),
        Uninitialized = (1 << 1),
    };

    Flags flags_;

    Instance(id value, Flags flags) :
        CYValue(value),
        flags_(flags)
    {
    }

    virtual ~Instance() {
        if ((flags_ & Transient) == 0)
            // XXX: does this handle background threads correctly?
            [GetValue() performSelector:@selector(release) withObject:nil afterDelay:0];
    }

    static JSObjectRef Make(JSContextRef context, id object, Flags flags) {
        return JSObjectMake(context, Instance_, new Instance(object, flags));
    }

    id GetValue() const {
        return reinterpret_cast<id>(value_);
    }

    bool IsUninitialized() const {
        return (flags_ & Uninitialized) != 0;
    }

    virtual Type_privateData *GetType() const;
};

struct Internal :
    CYValue
{
    JSObjectRef owner_;

    Internal(id value, JSObjectRef owner) :
        CYValue(value),
        owner_(owner)
    {
    }

    static JSObjectRef Make(JSContextRef context, id object, JSObjectRef owner) {
        return JSObjectMake(context, Internal_, new Internal(object, owner));
    }

    id GetValue() const {
        return reinterpret_cast<id>(value_);
    }
};

namespace sig {

void Copy(apr_pool_t *pool, Type &lhs, Type &rhs);

void Copy(apr_pool_t *pool, Element &lhs, Element &rhs) {
    lhs.name = apr_pstrdup(pool, rhs.name);
    if (rhs.type == NULL)
        lhs.type = NULL;
    else {
        lhs.type = new(pool) Type;
        Copy(pool, *lhs.type, *rhs.type);
    }
    lhs.offset = rhs.offset;
}

void Copy(apr_pool_t *pool, Signature &lhs, Signature &rhs) {
    size_t count(rhs.count);
    lhs.count = count;
    lhs.elements = new(pool) Element[count];
    for (size_t index(0); index != count; ++index)
        Copy(pool, lhs.elements[index], rhs.elements[index]);
}

void Copy(apr_pool_t *pool, Type &lhs, Type &rhs) {
    lhs.primitive = rhs.primitive;
    lhs.name = apr_pstrdup(pool, rhs.name);
    lhs.flags = rhs.flags;

    if (sig::IsAggregate(rhs.primitive))
        Copy(pool, lhs.data.signature, rhs.data.signature);
    else {
        if (rhs.data.data.type != NULL) {
            lhs.data.data.type = new(pool) Type;
            Copy(pool, *lhs.data.data.type, *rhs.data.data.type);
        }

        lhs.data.data.size = rhs.data.data.size;
    }
}

void Copy(apr_pool_t *pool, ffi_type &lhs, ffi_type &rhs) {
    lhs.size = rhs.size;
    lhs.alignment = rhs.alignment;
    lhs.type = rhs.type;
    if (rhs.elements == NULL)
        lhs.elements = NULL;
    else {
        size_t count(0);
        while (rhs.elements[count] != NULL)
            ++count;

        lhs.elements = new(pool) ffi_type *[count + 1];
        lhs.elements[count] = NULL;

        for (size_t index(0); index != count; ++index) {
            // XXX: if these are libffi native then you can just take them
            ffi_type *ffi(new(pool) ffi_type);
            lhs.elements[index] = ffi;
            sig::Copy(pool, *ffi, *rhs.elements[index]);
        }
    }
}

}

struct CStringMapLess :
    std::binary_function<const char *, const char *, bool>
{
    _finline bool operator ()(const char *lhs, const char *rhs) const {
        return strcmp(lhs, rhs) < 0;
    }
};

void Structor_(apr_pool_t *pool, const char *name, const char *types, sig::Type *&type) {
    if (name == NULL)
        return;

    CYPoolTry {
        if (NSMutableArray *entry = [[Bridge_ objectAtIndex:2] objectForKey:[NSString stringWithUTF8String:name]])
            switch ([[entry objectAtIndex:0] intValue]) {
                case 0: {
                    sig::Parse(pool, &type->data.signature, [[entry objectAtIndex:1] UTF8String], &Structor_);
                } break;

                case 1: {
                    sig::Signature signature;
                    sig::Parse(pool, &signature, [[entry objectAtIndex:1] UTF8String], &Structor_);
                    type = signature.elements[0].type;
                } break;
            }
    } CYPoolCatch()
}

struct Type_privateData :
    CYData
{
    static Type_privateData *Object;
    static Type_privateData *Selector;

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

Type_privateData *Type_privateData::Object;
Type_privateData *Type_privateData::Selector;

Type_privateData *Instance::GetType() const {
    return Type_privateData::Object;
}

Type_privateData *Selector_privateData::GetType() const {
    return Type_privateData::Selector;
}

struct Pointer :
    CYValue
{
    JSObjectRef owner_;
    Type_privateData *type_;

    Pointer(void *value, sig::Type *type, JSObjectRef owner) :
        CYValue(value),
        owner_(owner),
        type_(new(pool_) Type_privateData(type))
    {
    }
};

struct Struct_privateData :
    CYValue
{
    JSObjectRef owner_;
    Type_privateData *type_;

    Struct_privateData(JSObjectRef owner) :
        owner_(owner)
    {
    }
};

typedef std::map<const char *, Type_privateData *, CStringMapLess> TypeMap;
static TypeMap Types_;

JSObjectRef CYMakeStruct(JSContextRef context, void *data, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Struct_privateData *internal(new Struct_privateData(owner));
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

struct Functor_privateData :
    CYValue
{
    sig::Signature signature_;
    ffi_cif cif_;

    Functor_privateData(const char *type, void (*value)()) :
        CYValue(reinterpret_cast<void *>(value))
    {
        sig::Parse(pool_, &signature_, type, &Structor_);
        sig::sig_ffi_cif(pool_, &sig::ObjectiveC, &signature_, &cif_);
    }
};

struct ffoData :
    Functor_privateData
{
    JSContextRef context_;
    JSObjectRef function_;

    ffoData(const char *type) :
        Functor_privateData(type, NULL)
    {
    }
};

JSObjectRef CYMakeInstance(JSContextRef context, id object, bool transient) {
    Instance::Flags flags;

    if (transient)
        flags = Instance::Transient;
    else {
        flags = Instance::None;
        object = [object retain];
    }

    return Instance::Make(context, object, flags);
}

const char *CYPoolCString(apr_pool_t *pool, NSString *value) {
    if (pool == NULL)
        return [value UTF8String];
    else {
        size_t size([value maximumLengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1);
        char *string(new(pool) char[size]);
        if (![value getCString:string maxLength:size encoding:NSUTF8StringEncoding])
            @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:@"[NSString getCString:maxLength:encoding:] == NO" userInfo:nil];
        return string;
    }
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

bool CYGetIndex(const char *value, ssize_t &index) {
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

bool CYGetIndex(apr_pool_t *pool, NSString *value, ssize_t &index) {
    return CYGetIndex(CYPoolCString(pool, value), index);
}

NSString *CYPoolNSCYON(apr_pool_t *pool, id value);

@interface NSMethodSignature (Cycript)
- (NSString *) _typeString;
@end

@interface NSObject (Cycript)

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context;
- (JSType) cy$JSType;

- (NSObject *) cy$toJSON:(NSString *)key;
- (NSString *) cy$toCYON;
- (NSString *) cy$toKey;

- (bool) cy$hasProperty:(NSString *)name;
- (NSObject *) cy$getProperty:(NSString *)name;
- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value;
- (bool) cy$deleteProperty:(NSString *)name;

@end

@protocol Cycript
- (JSValueRef) cy$JSValueInContext:(JSContextRef)context;
@end

@interface NSString (Cycript)
- (void *) cy$symbol;
@end

struct PropertyAttributes {
    CYPool pool_;

    const char *name;

    const char *variable;

    const char *getter_;
    const char *setter_;

    bool readonly;
    bool copy;
    bool retain;
    bool nonatomic;
    bool dynamic;
    bool weak;
    bool garbage;

    PropertyAttributes(objc_property_t property) :
        variable(NULL),
        getter_(NULL),
        setter_(NULL),
        readonly(false),
        copy(false),
        retain(false),
        nonatomic(false),
        dynamic(false),
        weak(false),
        garbage(false)
    {
        name = property_getName(property);
        const char *attributes(property_getAttributes(property));

        for (char *state, *token(apr_strtok(apr_pstrdup(pool_, attributes), ",", &state)); token != NULL; token = apr_strtok(NULL, ",", &state)) {
            switch (*token) {
                case 'R': readonly = true; break;
                case 'C': copy = true; break;
                case '&': retain = true; break;
                case 'N': nonatomic = true; break;
                case 'G': getter_ = token + 1; break;
                case 'S': setter_ = token + 1; break;
                case 'V': variable = token + 1; break;
            }
        }

        /*if (variable == NULL) {
            variable = property_getName(property);
            size_t size(strlen(variable));
            char *name(new(pool_) char[size + 2]);
            name[0] = '_';
            memcpy(name + 1, variable, size);
            name[size + 1] = '\0';
            variable = name;
        }*/
    }

    const char *Getter() {
        if (getter_ == NULL)
            getter_ = apr_pstrdup(pool_, name);
        return getter_;
    }

    const char *Setter() {
        if (setter_ == NULL && !readonly) {
            size_t length(strlen(name));

            char *temp(new(pool_) char[length + 5]);
            temp[0] = 's';
            temp[1] = 'e';
            temp[2] = 't';

            if (length != 0) {
                temp[3] = toupper(name[0]);
                memcpy(temp + 4, name + 1, length - 1);
            }

            temp[length + 3] = ':';
            temp[length + 4] = '\0';
            setter_ = temp;
        }

        return setter_;
    }

};

@implementation NSProxy (Cycript)

- (NSObject *) cy$toJSON:(NSString *)key {
    return [self description];
}

- (NSString *) cy$toCYON {
    return [[self cy$toJSON:@""] cy$toCYON];
}

@end

@implementation NSObject (Cycript)

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return CYMakeInstance(context, self, false);
}

- (JSType) cy$JSType {
    return kJSTypeObject;
}

- (NSObject *) cy$toJSON:(NSString *)key {
    return [self description];
}

- (NSString *) cy$toCYON {
    return [[self cy$toJSON:@""] cy$toCYON];
}

- (NSString *) cy$toKey {
    return [self cy$toCYON];
}

- (bool) cy$hasProperty:(NSString *)name {
    return false;
}

- (NSObject *) cy$getProperty:(NSString *)name {
    return nil;
}

- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value {
    return false;
}

- (bool) cy$deleteProperty:(NSString *)name {
    return false;
}

@end

NSString *NSCFType$cy$toJSON(id self, SEL sel, NSString *key) {
    return [(NSString *) CFCopyDescription((CFTypeRef) self) autorelease];
}

@implementation WebUndefined (Cycript)

- (JSType) cy$JSType {
    return kJSTypeUndefined;
}

- (NSObject *) cy$toJSON:(NSString *)key {
    return self;
}

- (NSString *) cy$toCYON {
    return @"undefined";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return CYJSUndefined(context);
}

@end

@implementation NSNull (Cycript)

- (JSType) cy$JSType {
    return kJSTypeNull;
}

- (NSObject *) cy$toJSON:(NSString *)key {
    return self;
}

- (NSString *) cy$toCYON {
    return @"null";
}

@end

@implementation NSArray (Cycript)

- (NSString *) cy$toCYON {
    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"["];

    bool comma(false);
    for (id object in self) {
        if (comma)
            [json appendString:@","];
        else
            comma = true;
        if (object == nil || [object cy$JSType] != kJSTypeUndefined)
            [json appendString:CYPoolNSCYON(NULL, object)];
        else {
            [json appendString:@","];
            comma = false;
        }
    }

    [json appendString:@"]"];
    return json;
}

- (bool) cy$hasProperty:(NSString *)name {
    if ([name isEqualToString:@"length"])
        return true;

    ssize_t index;
    if (!CYGetIndex(NULL, name, index) || index < 0 || index >= static_cast<ssize_t>([self count]))
        return [super cy$hasProperty:name];
    else
        return true;
}

- (NSObject *) cy$getProperty:(NSString *)name {
    if ([name isEqualToString:@"length"])
        return [NSNumber numberWithUnsignedInteger:[self count]];

    ssize_t index;
    if (!CYGetIndex(NULL, name, index) || index < 0 || index >= static_cast<ssize_t>([self count]))
        return [super cy$getProperty:name];
    else
        return [self objectAtIndex:index];
}

@end

@implementation NSMutableArray (Cycript)

- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value {
    ssize_t index;
    if (!CYGetIndex(NULL, name, index) || index < 0 || index >= static_cast<ssize_t>([self count]))
        return [super cy$setProperty:name to:value];
    else {
        [self replaceObjectAtIndex:index withObject:(value ?: [NSNull null])];
        return true;
    }
}

- (bool) cy$deleteProperty:(NSString *)name {
    ssize_t index;
    if (!CYGetIndex(NULL, name, index) || index < 0 || index >= static_cast<ssize_t>([self count]))
        return [super cy$deleteProperty:name];
    else {
        [self removeObjectAtIndex:index];
        return true;
    }
}

@end

@implementation NSDictionary (Cycript)

- (NSString *) cy$toCYON {
    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"{"];

    bool comma(false);
    for (id key in self) {
        if (comma)
            [json appendString:@","];
        else
            comma = true;
        [json appendString:[key cy$toKey]];
        [json appendString:@":"];
        NSObject *object([self objectForKey:key]);
        [json appendString:CYPoolNSCYON(NULL, object)];
    }

    [json appendString:@"}"];
    return json;
}

- (bool) cy$hasProperty:(NSString *)name {
    return [self objectForKey:name] != nil;
}

- (NSObject *) cy$getProperty:(NSString *)name {
    return [self objectForKey:name];
}

@end

@implementation NSMutableDictionary (Cycript)

- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value {
    [self setObject:(value ?: [NSNull null]) forKey:name];
    return true;
}

- (bool) cy$deleteProperty:(NSString *)name {
    if ([self objectForKey:name] == nil)
        return false;
    else {
        [self removeObjectForKey:name];
        return true;
    }
}

@end

@implementation NSNumber (Cycript)

- (JSType) cy$JSType {
    // XXX: this just seems stupid
    return [self class] == NSCFBoolean_ ? kJSTypeBoolean : kJSTypeNumber;
}

- (NSObject *) cy$toJSON:(NSString *)key {
    return self;
}

- (NSString *) cy$toCYON {
    return [self cy$JSType] != kJSTypeBoolean ? [self stringValue] : [self boolValue] ? @"true" : @"false";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return [self cy$JSType] != kJSTypeBoolean ? CYCastJSValue(context, [self doubleValue]) : CYCastJSValue(context, [self boolValue]);
}

@end

@implementation NSString (Cycript)

- (JSType) cy$JSType {
    return kJSTypeString;
}

- (NSObject *) cy$toJSON:(NSString *)key {
    return self;
}

- (NSString *) cy$toCYON {
    // XXX: this should use the better code from Output.cpp
    CFMutableStringRef json(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, (CFStringRef) self));

    CFStringFindAndReplace(json, CFSTR("\\"), CFSTR("\\\\"), CFRangeMake(0, CFStringGetLength(json)), 0);
    CFStringFindAndReplace(json, CFSTR("\""), CFSTR("\\\""), CFRangeMake(0, CFStringGetLength(json)), 0);
    CFStringFindAndReplace(json, CFSTR("\t"), CFSTR("\\t"), CFRangeMake(0, CFStringGetLength(json)), 0);
    CFStringFindAndReplace(json, CFSTR("\r"), CFSTR("\\r"), CFRangeMake(0, CFStringGetLength(json)), 0);
    CFStringFindAndReplace(json, CFSTR("\n"), CFSTR("\\n"), CFRangeMake(0, CFStringGetLength(json)), 0);

    CFStringInsert(json, 0, CFSTR("\""));
    CFStringAppend(json, CFSTR("\""));

    return [reinterpret_cast<const NSString *>(json) autorelease];
}

- (NSString *) cy$toKey {
    const char *value([self UTF8String]);
    size_t size(strlen(value));

    if (size == 0)
        goto cyon;

    if (DigitRange_[value[0]]) {
        ssize_t index;
        if (!CYGetIndex(NULL, self, index) || index < 0)
            goto cyon;
    } else {
        if (!WordStartRange_[value[0]])
            goto cyon;
        for (size_t i(1); i != size; ++i)
            if (!WordEndRange_[value[i]])
                goto cyon;
    }

    return self;

  cyon:
    return [self cy$toCYON];
}

- (void *) cy$symbol {
    CYPool pool;
    return dlsym(RTLD_DEFAULT, CYPoolCString(pool, self));
}

@end

@interface CYJSObject : NSDictionary {
    JSObjectRef object_;
    JSContextRef context_;
}

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context;

- (NSString *) cy$toJSON:(NSString *)key;

- (NSUInteger) count;
- (id) objectForKey:(id)key;
- (NSEnumerator *) keyEnumerator;
- (void) setObject:(id)object forKey:(id)key;
- (void) removeObjectForKey:(id)key;

@end

@interface CYJSArray : NSArray {
    JSObjectRef object_;
    JSContextRef context_;
}

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context;

- (NSUInteger) count;
- (id) objectAtIndex:(NSUInteger)index;

@end

CYRange DigitRange_    (0x3ff000000000000LLU, 0x000000000000000LLU); // 0-9
CYRange WordStartRange_(0x000001000000000LLU, 0x7fffffe87fffffeLLU); // A-Za-z_$
CYRange WordEndRange_  (0x3ff001000000000LLU, 0x7fffffe87fffffeLLU); // A-Za-z_$0-9

#define CYTry \
    @try
#define CYCatch \
    @catch (id error) { \
        CYThrow(context, error, exception); \
        return NULL; \
    }

void CYThrow(JSContextRef context, JSValueRef value);

apr_status_t CYPoolRelease_(void *data) {
    id object(reinterpret_cast<id>(data));
    [object release];
    return APR_SUCCESS;
}

id CYPoolRelease(apr_pool_t *pool, id object) {
    if (object == nil)
        return nil;
    else if (pool == NULL)
        return [object autorelease];
    else {
        apr_pool_cleanup_register(pool, object, &CYPoolRelease_, &apr_pool_cleanup_null);
        return object;
    }
}

CFTypeRef CYPoolRelease(apr_pool_t *pool, CFTypeRef object) {
    return (CFTypeRef) CYPoolRelease(pool, (id) object);
}

id CYCastNSObject_(apr_pool_t *pool, JSContextRef context, JSObjectRef object) {
    JSValueRef exception(NULL);
    bool array(JSValueIsInstanceOfConstructor(context, object, Array_, &exception));
    CYThrow(context, exception);
    id value(array ? [CYJSArray alloc] : [CYJSObject alloc]);
    return CYPoolRelease(pool, [value initWithJSObject:object inContext:context]);
}

id CYCastNSObject(apr_pool_t *pool, JSContextRef context, JSObjectRef object) {
    if (!JSValueIsObjectOfClass(context, object, Instance_))
        return CYCastNSObject_(pool, context, object);
    else {
        Instance *data(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
        return data->GetValue();
    }
}

JSStringRef CYCopyJSString(id value) {
    return value == NULL ? NULL : JSStringCreateWithCFString(reinterpret_cast<CFStringRef>([value description]));
}

JSStringRef CYCopyJSString(const char *value) {
    return value == NULL ? NULL : JSStringCreateWithUTF8CString(value);
}

JSStringRef CYCopyJSString(JSStringRef value) {
    return value == NULL ? NULL : JSStringRetain(value);
}

JSStringRef CYCopyJSString(JSContextRef context, JSValueRef value) {
    if (JSValueIsNull(context, value))
        return NULL;
    JSValueRef exception(NULL);
    JSStringRef string(JSValueToStringCopy(context, value, &exception));
    CYThrow(context, exception);
    return string;
}

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

CFStringRef CYCopyCFString(JSStringRef value) {
    return JSStringCopyCFString(kCFAllocatorDefault, value);
}

CFStringRef CYCopyCFString(JSContextRef context, JSValueRef value) {
    return CYCopyCFString(CYJSString(context, value));
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

CFNumberRef CYCopyCFNumber(JSContextRef context, JSValueRef value) {
    double number(CYCastDouble(context, value));
    return CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &number);
}

CFStringRef CYCopyCFString(const char *value) {
    return CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);
}

NSString *CYCastNSString(apr_pool_t *pool, const char *value) {
    return (NSString *) CYPoolRelease(pool, CYCopyCFString(value));
}

NSString *CYCastNSString(apr_pool_t *pool, JSStringRef value) {
    return (NSString *) CYPoolRelease(pool, CYCopyCFString(value));
}

bool CYCastBool(JSContextRef context, JSValueRef value) {
    return JSValueToBoolean(context, value);
}

CFTypeRef CYCFType(apr_pool_t *pool, JSContextRef context, JSValueRef value, bool cast) {
    CFTypeRef object;
    bool copy;

    switch (JSType type = JSValueGetType(context, value)) {
        case kJSTypeUndefined:
            object = [WebUndefined undefined];
            copy = false;
        break;

        case kJSTypeNull:
            return NULL;
        break;

        case kJSTypeBoolean:
            object = CYCastBool(context, value) ? kCFBooleanTrue : kCFBooleanFalse;
            copy = false;
        break;

        case kJSTypeNumber:
            object = CYCopyCFNumber(context, value);
            copy = true;
        break;

        case kJSTypeString:
            object = CYCopyCFString(context, value);
            copy = true;
        break;

        case kJSTypeObject:
            // XXX: this might could be more efficient
            object = (CFTypeRef) CYCastNSObject(pool, context, (JSObjectRef) value);
            copy = false;
        break;

        default:
            @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"JSValueGetType() == 0x%x", type] userInfo:nil];
        break;
    }

    if (cast != copy)
        return object;
    else if (copy)
        return CYPoolRelease(pool, object);
    else
        return CFRetain(object);
}

CFTypeRef CYCastCFType(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    return CYCFType(pool, context, value, true);
}

CFTypeRef CYCopyCFType(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    return CYCFType(pool, context, value, false);
}

NSArray *CYCastNSArray(JSPropertyNameArrayRef names) {
    CYPool pool;
    size_t size(JSPropertyNameArrayGetCount(names));
    NSMutableArray *array([NSMutableArray arrayWithCapacity:size]);
    for (size_t index(0); index != size; ++index)
        [array addObject:CYCastNSString(pool, JSPropertyNameArrayGetNameAtIndex(names, index))];
    return array;
}

id CYCastNSObject(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    return reinterpret_cast<const NSObject *>(CYCastCFType(pool, context, value));
}

void CYThrow(JSContextRef context, JSValueRef value) {
    if (value == NULL)
        return;
    @throw CYCastNSObject(NULL, context, value);
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

JSValueRef CYCastJSValue(JSContextRef context, id value) {
    if (value == nil)
        return CYJSNull(context);
    else if ([value respondsToSelector:@selector(cy$JSValueInContext:)])
        return [value cy$JSValueInContext:context];
    else
        return CYMakeInstance(context, value, false);
}

JSObjectRef CYCastJSObject(JSContextRef context, JSValueRef value) {
    JSValueRef exception(NULL);
    JSObjectRef object(JSValueToObject(context, value, &exception));
    CYThrow(context, exception);
    return object;
}

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

void CYSetProperty(JSContextRef context, JSObjectRef object, JSStringRef name, JSValueRef value) {
    JSValueRef exception(NULL);
    JSObjectSetProperty(context, object, name, value, kJSPropertyAttributeNone, &exception);
    CYThrow(context, exception);
}

void CYThrow(JSContextRef context, id error, JSValueRef *exception) {
    if (exception == NULL)
        throw error;
    *exception = CYCastJSValue(context, error);
}

JSValueRef CYCallAsFunction(JSContextRef context, JSObjectRef function, JSObjectRef _this, size_t count, JSValueRef arguments[]) {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectCallAsFunction(context, function, _this, count, arguments, &exception));
    CYThrow(context, exception);
    return value;
}

bool CYIsCallable(JSContextRef context, JSValueRef value) {
    // XXX: this isn't actually correct
    return value != NULL && JSValueIsObject(context, value);
}

@implementation CYJSObject

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context {
    if ((self = [super init]) != nil) {
        object_ = object;
        context_ = context;
    } return self;
}

- (NSObject *) cy$toJSON:(NSString *)key {
    JSValueRef toJSON(CYGetProperty(context_, object_, toJSON_));
    if (!CYIsCallable(context_, toJSON))
        return [super cy$toJSON:key];
    else {
        JSValueRef arguments[1] = {CYCastJSValue(context_, key)};
        JSValueRef value(CYCallAsFunction(context_, (JSObjectRef) toJSON, object_, 1, arguments));
        // XXX: do I really want an NSNull here?!
        return CYCastNSObject(NULL, context_, value) ?: [NSNull null];
    }
}

- (NSString *) cy$toCYON {
    JSValueRef toCYON(CYGetProperty(context_, object_, toCYON_));
    if (!CYIsCallable(context_, toCYON))
        return [super cy$toCYON];
    else {
        JSValueRef value(CYCallAsFunction(context_, (JSObjectRef) toCYON, object_, 0, NULL));
        return CYCastNSString(NULL, CYJSString(context_, value));
    }
}

- (NSUInteger) count {
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context_, object_));
    size_t size(JSPropertyNameArrayGetCount(names));
    JSPropertyNameArrayRelease(names);
    return size;
}

- (id) objectForKey:(id)key {
    return CYCastNSObject(NULL, context_, CYGetProperty(context_, object_, CYJSString(key))) ?: [NSNull null];
}

- (NSEnumerator *) keyEnumerator {
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context_, object_));
    NSEnumerator *enumerator([CYCastNSArray(names) objectEnumerator]);
    JSPropertyNameArrayRelease(names);
    return enumerator;
}

- (void) setObject:(id)object forKey:(id)key {
    CYSetProperty(context_, object_, CYJSString(key), CYCastJSValue(context_, object));
}

- (void) removeObjectForKey:(id)key {
    JSValueRef exception(NULL);
    (void) JSObjectDeleteProperty(context_, object_, CYJSString(key), &exception);
    CYThrow(context_, exception);
}

@end

@implementation CYJSArray

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context {
    if ((self = [super init]) != nil) {
        object_ = object;
        context_ = context;
    } return self;
}

- (NSUInteger) count {
    return CYCastDouble(context_, CYGetProperty(context_, object_, length_));
}

- (id) objectAtIndex:(NSUInteger)index {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetPropertyAtIndex(context_, object_, index, &exception));
    CYThrow(context_, exception);
    return CYCastNSObject(NULL, context_, value) ?: [NSNull null];
}

@end

NSString *CYCopyNSCYON(id value) {
    NSString *string;

    if (value == nil)
        string = @"nil";
    else {
        Class _class(object_getClass(value));
        SEL sel(@selector(cy$toCYON));

        if (Method toCYON = class_getInstanceMethod(_class, sel))
            string = reinterpret_cast<NSString *(*)(id, SEL)>(method_getImplementation(toCYON))(value, sel);
        else if (Method methodSignatureForSelector = class_getInstanceMethod(_class, @selector(methodSignatureForSelector:))) {
            if (reinterpret_cast<NSMethodSignature *(*)(id, SEL, SEL)>(method_getImplementation(methodSignatureForSelector))(value, @selector(methodSignatureForSelector:), sel) != nil)
                string = [value cy$toCYON];
            else goto fail;
        } else fail: {
            if (value == NSZombie_)
                string = @"_NSZombie_";
            else if (_class == NSZombie_)
                string = [NSString stringWithFormat:@"<_NSZombie_: %p>", value];
            // XXX: frowny /in/ the pants
            else if (value == NSMessageBuilder_ || value == Object_)
                string = nil;
            else
                string = [NSString stringWithFormat:@"%@", value];
        }

        // XXX: frowny pants
        if (string == nil)
            string = @"undefined";
    }

    return [string retain];
}

NSString *CYCopyNSCYON(JSContextRef context, JSValueRef value, JSValueRef *exception) {
    CYTry {
        CYPoolTry {
            return CYCopyNSCYON(CYCastNSObject(NULL, context, value));
        } CYPoolCatch(NULL)
    } CYCatch
}

NSString *CYPoolNSCYON(apr_pool_t *pool, id value) {
    return CYPoolRelease(pool, static_cast<id>(CYCopyNSCYON(value)));
}

const char *CYPoolCCYON(apr_pool_t *pool, JSContextRef context, JSValueRef value, JSValueRef *exception) {
    if (NSString *json = CYCopyNSCYON(context, value, exception)) {
        const char *string(CYPoolCString(pool, json));
        [json release];
        return string;
    } else return NULL;
}

// XXX: use objc_getAssociatedObject and objc_setAssociatedObject on 10.6
struct CYInternal :
    CYData
{
    JSObjectRef object_;

    CYInternal() :
        object_(NULL)
    {
    }

    ~CYInternal() {
        // XXX: delete object_? ;(
    }

    static CYInternal *Get(id self) {
        CYInternal *internal(NULL);
        if (object_getInstanceVariable(self, "cy$internal_", reinterpret_cast<void **>(&internal)) == NULL) {
            // XXX: do something epic? ;P
        }

        return internal;
    }

    static CYInternal *Set(id self) {
        CYInternal *internal(NULL);
        if (Ivar ivar = object_getInstanceVariable(self, "cy$internal_", reinterpret_cast<void **>(&internal))) {
            if (internal == NULL) {
                internal = new CYInternal();
                object_setIvar(self, ivar, reinterpret_cast<id>(internal));
            }
        } else {
            // XXX: do something epic? ;P
        }

        return internal;
    }

    bool HasProperty(JSContextRef context, JSStringRef name) {
        if (object_ == NULL)
            return false;
        return JSObjectHasProperty(context, object_, name);
    }

    JSValueRef GetProperty(JSContextRef context, JSStringRef name) {
        if (object_ == NULL)
            return NULL;
        return CYGetProperty(context, object_, name);
    }

    void SetProperty(JSContextRef context, JSStringRef name, JSValueRef value) {
        if (object_ == NULL)
            object_ = JSObjectMake(context, NULL, NULL);
        CYSetProperty(context, object_, name, value);
    }
};

JSObjectRef CYMakeSelector(JSContextRef context, SEL sel) {
    Selector_privateData *data(new Selector_privateData(sel));
    return JSObjectMake(context, Selector_, data);
}

JSObjectRef CYMakePointer(JSContextRef context, void *pointer, sig::Type *type, ffi_type *ffi, JSObjectRef owner) {
    Pointer *data(new Pointer(pointer, type, owner));
    return JSObjectMake(context, Pointer_, data);
}

JSObjectRef CYMakeFunctor(JSContextRef context, void (*function)(), const char *type) {
    Functor_privateData *data(new Functor_privateData(type, function));
    return JSObjectMake(context, Functor_, data);
}

const char *CYPoolCString(apr_pool_t *pool, JSStringRef value) {
    if (pool == NULL) {
        const char *string([CYCastNSString(NULL, value) UTF8String]);
        return string;
    } else {
        size_t size(JSStringGetMaximumUTF8CStringSize(value));
        char *string(new(pool) char[size]);
        JSStringGetUTF8CString(value, string, size);
        return string;
    }
}

const char *CYPoolCString(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    return JSValueIsNull(context, value) ? NULL : CYPoolCString(pool, CYJSString(context, value));
}

bool CYGetIndex(apr_pool_t *pool, JSStringRef value, ssize_t &index) {
    return CYGetIndex(CYPoolCString(pool, value), index);
}

// XXX: this macro is unhygenic
#define CYCastCString(context, value) ({ \
    char *utf8; \
    if (value == NULL) \
        utf8 = NULL; \
    else if (JSStringRef string = CYCopyJSString(context, value)) { \
        size_t size(JSStringGetMaximumUTF8CStringSize(string)); \
        utf8 = reinterpret_cast<char *>(alloca(size)); \
        JSStringGetUTF8CString(string, utf8, size); \
        JSStringRelease(string); \
    } else \
        utf8 = NULL; \
    utf8; \
})

void *CYCastPointer_(JSContextRef context, JSValueRef value) {
    switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return NULL;
        /*case kJSTypeString:
            return dlsym(RTLD_DEFAULT, CYCastCString(context, value));
        case kJSTypeObject:
            if (JSValueIsObjectOfClass(context, value, Pointer_)) {
                Pointer *data(reinterpret_cast<Pointer *>(JSObjectGetPrivate((JSObjectRef) value)));
                return data->value_;
            }*/
        default:
            double number(CYCastDouble(context, value));
            if (std::isnan(number))
                @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"cannot convert value to pointer" userInfo:nil];
            return reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<long long>(number)));
    }
}

template <typename Type_>
_finline Type_ CYCastPointer(JSContextRef context, JSValueRef value) {
    return reinterpret_cast<Type_>(CYCastPointer_(context, value));
}

SEL CYCastSEL(JSContextRef context, JSValueRef value) {
    if (JSValueIsObjectOfClass(context, value, Selector_)) {
        Selector_privateData *data(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate((JSObjectRef) value)));
        return reinterpret_cast<SEL>(data->value_);
    } else
        return CYCastPointer<SEL>(context, value);
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

        case sig::object_P:
        case sig::typename_P:
            *reinterpret_cast<id *>(data) = CYCastNSObject(pool, context, value);
        break;

        case sig::selector_P:
            *reinterpret_cast<SEL *>(data) = CYCastSEL(context, value);
        break;

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
                            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"unable to extract structure value" userInfo:nil];
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
            NSLog(@"CYPoolFFI(%c)\n", type->primitive);
            _assert(false);
    }
}

JSValueRef CYFromFFI(JSContextRef context, sig::Type *type, ffi_type *ffi, void *data, bool initialize = false, JSObjectRef owner = NULL) {
    JSValueRef value;

    switch (type->primitive) {
        case sig::boolean_P:
            value = CYCastJSValue(context, *reinterpret_cast<bool *>(data));
        break;

#define CYFromFFI_(primitive, native) \
        case sig::primitive ## _P: \
            value = CYCastJSValue(context, *reinterpret_cast<native *>(data)); \
        break;

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

        case sig::object_P: {
            if (id object = *reinterpret_cast<id *>(data)) {
                value = CYCastJSValue(context, object);
                if (initialize)
                    [object release];
            } else goto null;
        } break;

        case sig::typename_P:
            value = CYMakeInstance(context, *reinterpret_cast<Class *>(data), true);
        break;

        case sig::selector_P:
            if (SEL sel = *reinterpret_cast<SEL *>(data))
                value = CYMakeSelector(context, sel);
            else goto null;
        break;

        case sig::pointer_P:
            if (void *pointer = *reinterpret_cast<void **>(data))
                value = CYMakePointer(context, pointer, type->data.data.type, ffi, owner);
            else goto null;
        break;

        case sig::string_P:
            if (char *utf8 = *reinterpret_cast<char **>(data))
                value = CYCastJSValue(context, utf8);
            else goto null;
        break;

        case sig::struct_P:
            value = CYMakeStruct(context, data, type, ffi, owner);
        break;

        case sig::void_P:
            value = CYJSUndefined(context);
        break;

        null:
            value = CYJSNull(context);
        break;

        default:
            NSLog(@"CYFromFFI(%c)\n", type->primitive);
            _assert(false);
    }

    return value;
}

static bool CYImplements(id object, Class _class, SEL selector) {
    // XXX: possibly use a more "awesome" check?
    return class_getInstanceMethod(_class, selector) != NULL;
}

static bool Instance_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->GetValue());

    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        return true;

    CYPool pool;
    NSString *name(CYCastNSString(pool, property));

    if (CYInternal *internal = CYInternal::Get(self))
        if (internal->HasProperty(context, property))
            return true;

    CYPoolTry {
        if ([self cy$hasProperty:name])
            return true;
    } CYPoolCatch(false)

    const char *string(CYPoolCString(pool, name));
    Class _class(object_getClass(self));

    if (class_getProperty(_class, string) != NULL)
        return true;

    if (SEL sel = sel_getUid(string))
        if (CYImplements(self, _class, sel))
            return true;

    return false;
}

static JSValueRef Instance_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->GetValue());

    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        return Internal::Make(context, self, object);

    CYTry {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));

        if (CYInternal *internal = CYInternal::Get(self))
            if (JSValueRef value = internal->GetProperty(context, property))
                return value;

        CYPoolTry {
            if (NSObject *data = [self cy$getProperty:name])
                return CYCastJSValue(context, data);
        } CYPoolCatch(NULL)

        const char *string(CYPoolCString(pool, name));
        Class _class(object_getClass(self));

        if (objc_property_t property = class_getProperty(_class, string)) {
            PropertyAttributes attributes(property);
            SEL sel(sel_registerName(attributes.Getter()));
            return CYSendMessage(pool, context, self, sel, 0, NULL, false, exception);
        }

        if (SEL sel = sel_getUid(string))
            if (CYImplements(self, _class, sel))
                return CYSendMessage(pool, context, self, sel, 0, NULL, false, exception);

        return NULL;
    } CYCatch
}

static bool Instance_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) {
    CYPool pool;

    CYTry {
        id self(CYCastNSObject(pool, context, object));
        NSString *name(CYCastNSString(pool, property));
        NSString *data(CYCastNSObject(pool, context, value));

        CYPoolTry {
            if ([self cy$setProperty:name to:data])
                return true;
        } CYPoolCatch(NULL)

        const char *string(CYPoolCString(pool, name));
        Class _class(object_getClass(self));

        if (objc_property_t property = class_getProperty(_class, string)) {
            PropertyAttributes attributes(property);
            if (const char *setter = attributes.Setter()) {
                SEL sel(sel_registerName(setter));
                JSValueRef arguments[1] = {value};
                CYSendMessage(pool, context, self, sel, 1, arguments, false, exception);
                return true;
            }
        }

        size_t length(strlen(string));

        char set[length + 5];

        set[0] = 's';
        set[1] = 'e';
        set[2] = 't';

        if (string[0] != '\0') {
            set[3] = toupper(string[0]);
            memcpy(set + 4, string + 1, length - 1);
        }

        set[length + 3] = ':';
        set[length + 4] = '\0';

        if (SEL sel = sel_getUid(set))
            if (CYImplements(self, _class, sel)) {
                JSValueRef arguments[1] = {value};
                CYSendMessage(pool, context, self, sel, 1, arguments, false, exception);
            }

        if (CYInternal *internal = CYInternal::Set(self)) {
            internal->SetProperty(context, property, value);
            return true;
        }

        return false;
    } CYCatch
}

static bool Instance_deleteProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYTry {
        CYPoolTry {
            id self(CYCastNSObject(NULL, context, object));
            NSString *name(CYCastNSString(NULL, property));
            return [self cy$deleteProperty:name];
        } CYPoolCatch(NULL)
    } CYCatch
}

static void Instance_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYPool pool;
    NSString *self(CYCastNSObject(pool, context, object));
    Class _class(object_getClass(self));

    {
        unsigned int size;
        objc_property_t *data(class_copyPropertyList(_class, &size));
        for (size_t i(0); i != size; ++i)
            JSPropertyNameAccumulatorAddName(names, CYJSString(property_getName(data[i])));
        free(data);
    }
}

static JSObjectRef Instance_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        Instance *data(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
        JSObjectRef value(Instance::Make(context, [data->GetValue() alloc], Instance::Uninitialized));
        return value;
    } CYCatch
}

static bool Internal_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    Internal *internal(reinterpret_cast<Internal *>(JSObjectGetPrivate(object)));
    CYPool pool;

    id self(internal->GetValue());
    const char *name(CYPoolCString(pool, property));

    if (object_getInstanceVariable(self, name, NULL) != NULL)
        return true;

    return false;
}

static JSValueRef Internal_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    Internal *internal(reinterpret_cast<Internal *>(JSObjectGetPrivate(object)));
    CYPool pool;

    CYTry {
        id self(internal->GetValue());
        const char *name(CYPoolCString(pool, property));

        if (Ivar ivar = object_getInstanceVariable(self, name, NULL)) {
            Type_privateData type(pool, ivar_getTypeEncoding(ivar));
            return CYFromFFI(context, type.type_, type.GetFFI(), reinterpret_cast<uint8_t *>(self) + ivar_getOffset(ivar));
        }

        return NULL;
    } CYCatch
}

static bool Internal_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) {
    Internal *internal(reinterpret_cast<Internal *>(JSObjectGetPrivate(object)));
    CYPool pool;

    CYTry {
        id self(internal->GetValue());
        const char *name(CYPoolCString(pool, property));

        if (Ivar ivar = object_getInstanceVariable(self, name, NULL)) {
            Type_privateData type(pool, ivar_getTypeEncoding(ivar));
            CYPoolFFI(pool, context, type.type_, type.GetFFI(), reinterpret_cast<uint8_t *>(self) + ivar_getOffset(ivar), value);
            return true;
        }

        return false;
    } CYCatch
}

static void Internal_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    Internal *internal(reinterpret_cast<Internal *>(JSObjectGetPrivate(object)));
    CYPool pool;

    id self(internal->GetValue());
    Class _class(object_getClass(self));

    for (Class super(_class); super != NULL; super = class_getSuperclass(super)) {
        unsigned int size;
        Ivar *data(class_copyIvarList(super, &size));
        for (size_t i(0); i != size; ++i)
            JSPropertyNameAccumulatorAddName(names, CYJSString(ivar_getName(data[i])));
        free(data);
    }
}

static JSValueRef Internal_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Internal *internal(reinterpret_cast<Internal *>(JSObjectGetPrivate(object)));
    return internal->owner_;
}

bool Index_(apr_pool_t *pool, Struct_privateData *internal, JSStringRef property, ssize_t &index, uint8_t *&base) {
    Type_privateData *typical(internal->type_);
    sig::Type *type(typical->type_);
    if (type == NULL)
        return false;

    const char *name(CYPoolCString(pool, property));
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

static JSValueRef Pointer_getIndex(JSContextRef context, JSObjectRef object, size_t index, JSValueRef *exception) {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    ffi_type *ffi(typical->GetFFI());

    uint8_t *base(reinterpret_cast<uint8_t *>(internal->value_));
    base += ffi->size * index;

    JSObjectRef owner(internal->owner_ ?: object);

    CYTry {
        return CYFromFFI(context, typical->type_, ffi, base, false, owner);
    } CYCatch
}

static JSValueRef Pointer_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    if (typical->type_ == NULL)
        return NULL;

    ssize_t index;
    if (!CYGetIndex(pool, property, index))
        return NULL;

    return Pointer_getIndex(context, object, index, exception);
}

static JSValueRef Pointer_getProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    return Pointer_getIndex(context, object, 0, exception);
}

static bool Pointer_setIndex(JSContextRef context, JSObjectRef object, size_t index, JSValueRef value, JSValueRef *exception) {
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    ffi_type *ffi(typical->GetFFI());

    uint8_t *base(reinterpret_cast<uint8_t *>(internal->value_));
    base += ffi->size * index;

    CYTry {
        CYPoolFFI(NULL, context, typical->type_, ffi, base, value);
        return true;
    } CYCatch
}

static bool Pointer_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) {
    CYPool pool;
    Pointer *internal(reinterpret_cast<Pointer *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    if (typical->type_ == NULL)
        return NULL;

    ssize_t index;
    if (!CYGetIndex(pool, property, index))
        return NULL;

    return Pointer_setIndex(context, object, 0, value, exception);
}

static bool Pointer_setProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) {
    return Pointer_setIndex(context, object, 0, value, exception);
}

static JSValueRef Struct_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(_this)));
    Type_privateData *typical(internal->type_);
    return CYMakePointer(context, internal->value_, typical->type_, typical->ffi_, _this);
}

static JSValueRef Struct_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYPool pool;
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    ssize_t index;
    uint8_t *base;

    if (!Index_(pool, internal, property, index, base))
        return NULL;

    JSObjectRef owner(internal->owner_ ?: object);

    CYTry {
        return CYFromFFI(context, typical->type_->data.signature.elements[index].type, typical->GetFFI()->elements[index], base, false, owner);
    } CYCatch
}

static bool Struct_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) {
    CYPool pool;
    Struct_privateData *internal(reinterpret_cast<Struct_privateData *>(JSObjectGetPrivate(object)));
    Type_privateData *typical(internal->type_);

    ssize_t index;
    uint8_t *base;

    if (!Index_(pool, internal, property, index, base))
        return false;

    CYTry {
        CYPoolFFI(NULL, context, typical->type_->data.signature.elements[index].type, typical->GetFFI()->elements[index], base, value);
        return true;
    } CYCatch
}

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
            sprintf(number, "%lu", index);
            name = number;
        }

        JSPropertyNameAccumulatorAddName(names, CYJSString(name));
    }
}

JSValueRef CYCallFunction(apr_pool_t *pool, JSContextRef context, size_t setups, void *setup[], size_t count, const JSValueRef arguments[], bool initialize, JSValueRef *exception, sig::Signature *signature, ffi_cif *cif, void (*function)()) {
    CYTry {
        if (setups + count != signature->count - 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to ffi function" userInfo:nil];

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
        ffi_call(cif, function, value, values);

        return CYFromFFI(context, signature->elements[0].type, cif->rtype, value, initialize);
    } CYCatch
}

void Closure_(ffi_cif *cif, void *result, void **arguments, void *arg) {
    ffoData *data(reinterpret_cast<ffoData *>(arg));

    JSContextRef context(data->context_);

    size_t count(data->cif_.nargs);
    JSValueRef values[count];

    for (size_t index(0); index != count; ++index)
        values[index] = CYFromFFI(context, data->signature_.elements[1 + index].type, data->cif_.arg_types[index], arguments[index]);

    JSValueRef value(CYCallAsFunction(context, data->function_, NULL, count, values));
    CYPoolFFI(NULL, context, data->signature_.elements[0].type, data->cif_.rtype, result, value);
}

JSObjectRef CYMakeFunctor(JSContextRef context, JSObjectRef function, const char *type) {
    // XXX: in case of exceptions this will leak
    ffoData *data(new ffoData(type));

    ffi_closure *closure((ffi_closure *) _syscall(mmap(
        NULL, sizeof(ffi_closure),
        PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
        -1, 0
    )));

    ffi_status status(ffi_prep_closure(closure, &data->cif_, &Closure_, data));
    _assert(status == FFI_OK);

    _syscall(mprotect(closure, sizeof(*closure), PROT_READ | PROT_EXEC));

    data->value_ = closure;

    data->context_ = CYGetJSContext();
    data->function_ = function;

    return JSObjectMake(context, Functor_, data);
}

static JSValueRef ObjectiveC_Classes_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYTry {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        if (Class _class = NSClassFromString(name))
            return CYMakeInstance(context, _class, true);
        return NULL;
    } CYCatch
}

static void ObjectiveC_Classes_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    size_t size(objc_getClassList(NULL, 0));
    Class *data(reinterpret_cast<Class *>(malloc(sizeof(Class) * size)));

  get:
    size_t writ(objc_getClassList(data, size));
    if (size < writ) {
        size = writ;
        if (Class *copy = reinterpret_cast<Class *>(realloc(data, sizeof(Class) * writ))) {
            data = copy;
            goto get;
        } else goto done;
    }

    for (size_t i(0); i != writ; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(class_getName(data[i])));

  done:
    free(data);
}

static JSValueRef ObjectiveC_Image_Classes_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    const char *internal(reinterpret_cast<const char *>(JSObjectGetPrivate(object)));

    CYTry {
        CYPool pool;
        const char *name(CYPoolCString(pool, property));
        unsigned int size;
        const char **data(objc_copyClassNamesForImage(internal, &size));
        JSValueRef value;
        for (size_t i(0); i != size; ++i)
            if (strcmp(name, data[i]) == 0) {
                if (Class _class = objc_getClass(name)) {
                    value = CYMakeInstance(context, _class, true);
                    goto free;
                } else
                    break;
            }
        value = NULL;
      free:
        free(data);
        return value;
    } CYCatch
}

static void ObjectiveC_Image_Classes_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    const char *internal(reinterpret_cast<const char *>(JSObjectGetPrivate(object)));
    unsigned int size;
    const char **data(objc_copyClassNamesForImage(internal, &size));
    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(data[i]));
    free(data);
}

static JSValueRef ObjectiveC_Images_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYTry {
        CYPool pool;
        const char *name(CYPoolCString(pool, property));
        unsigned int size;
        const char **data(objc_copyImageNames(&size));
        for (size_t i(0); i != size; ++i)
            if (strcmp(name, data[i]) == 0) {
                name = data[i];
                goto free;
            }
        name = NULL;
      free:
        free(data);
        if (name == NULL)
            return NULL;
        JSObjectRef value(JSObjectMake(context, NULL, NULL));
        CYSetProperty(context, value, CYJSString("classes"), JSObjectMake(context, ObjectiveC_Image_Classes_, const_cast<char *>(name)));
        return value;
    } CYCatch
}

static void ObjectiveC_Images_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    unsigned int size;
    const char **data(objc_copyImageNames(&size));
    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(data[i]));
    free(data);
}

static JSValueRef ObjectiveC_Protocols_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYTry {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        if (Protocol *protocol = NSProtocolFromString(name))
            return CYMakeInstance(context, protocol, true);
        return NULL;
    } CYCatch
}

static void ObjectiveC_Protocols_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    unsigned int size;
    Protocol **data(objc_copyProtocolList(&size));
    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(protocol_getName(data[i])));
    free(data);
}

static JSValueRef Runtime_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    if (JSStringIsEqualToUTF8CString(property, "nil"))
        return Instance::Make(context, nil, Instance::None);

    CYTry {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        if (Class _class = NSClassFromString(name))
            return CYMakeInstance(context, _class, true);
        if (NSMutableArray *entry = [[Bridge_ objectAtIndex:0] objectForKey:name])
            switch ([[entry objectAtIndex:0] intValue]) {
                case 0:
                    return JSEvaluateScript(CYGetJSContext(), CYJSString([entry objectAtIndex:1]), NULL, NULL, 0, NULL);
                case 1:
                    return CYMakeFunctor(context, reinterpret_cast<void (*)()>([name cy$symbol]), CYPoolCString(pool, [entry objectAtIndex:1]));
                case 2:
                    // XXX: this is horrendously inefficient
                    sig::Signature signature;
                    sig::Parse(pool, &signature, CYPoolCString(pool, [entry objectAtIndex:1]), &Structor_);
                    ffi_cif cif;
                    sig::sig_ffi_cif(pool, &sig::ObjectiveC, &signature, &cif);
                    return CYFromFFI(context, signature.elements[0].type, cif.rtype, [name cy$symbol]);
            }
        return NULL;
    } CYCatch
}

bool stret(ffi_type *ffi_type) {
    return ffi_type->type == FFI_TYPE_STRUCT && (
        ffi_type->size > OBJC_MAX_STRUCT_BY_VALUE ||
        struct_forward_array[ffi_type->size] != 0
    );
}

extern "C" {
    int *_NSGetArgc(void);
    char ***_NSGetArgv(void);
    int UIApplicationMain(int argc, char *argv[], NSString *principalClassName, NSString *delegateClassName);
}

static JSValueRef System_print(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        NSLog(@"%s", CYCastCString(context, arguments[0]));
        return CYJSUndefined(context);
    } CYCatch
}

JSValueRef CYSendMessage(apr_pool_t *pool, JSContextRef context, id self, SEL _cmd, size_t count, const JSValueRef arguments[], bool initialize, JSValueRef *exception) {
    const char *type;

    Class _class(object_getClass(self));
    if (Method method = class_getInstanceMethod(_class, _cmd))
        type = method_getTypeEncoding(method);
    else {
        CYTry {
            CYPoolTry {
                NSMethodSignature *method([self methodSignatureForSelector:_cmd]);
                if (method == nil)
                    @throw [NSException exceptionWithName:NSInvalidArgumentException reason:[NSString stringWithFormat:@"unrecognized selector %s sent to object %p", sel_getName(_cmd), self] userInfo:nil];
                type = CYPoolCString(pool, [method _typeString]);
            } CYPoolCatch(NULL)
        } CYCatch
    }

    void *setup[2];
    setup[0] = &self;
    setup[1] = &_cmd;

    sig::Signature signature;
    sig::Parse(pool, &signature, type, &Structor_);

    ffi_cif cif;
    sig::sig_ffi_cif(pool, &sig::ObjectiveC, &signature, &cif);

    void (*function)() = stret(cif.rtype) ? reinterpret_cast<void (*)()>(&objc_msgSend_stret) : reinterpret_cast<void (*)()>(&objc_msgSend);
    return CYCallFunction(pool, context, 2, setup, count, arguments, initialize, exception, &signature, &cif, function);
}

static JSValueRef $objc_msgSend(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYPool pool;

    bool uninitialized;

    id self;
    SEL _cmd;

    CYTry {
        if (count < 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"too few arguments to objc_msgSend" userInfo:nil];

        if (JSValueIsObjectOfClass(context, arguments[0], Instance_)) {
            Instance *data(reinterpret_cast<Instance *>(JSObjectGetPrivate((JSObjectRef) arguments[0])));
            self = data->GetValue();
            uninitialized = data->IsUninitialized();
            if (uninitialized)
                data->value_ = nil;
        } else {
            self = CYCastNSObject(pool, context, arguments[0]);
            uninitialized = false;
        }

        if (self == nil)
            return CYJSNull(context);

        _cmd = CYCastSEL(context, arguments[1]);
    } CYCatch

    return CYSendMessage(pool, context, self, _cmd, count - 2, arguments + 2, uninitialized, exception);
}

MSHook(void, CYDealloc, id self, SEL sel) {
    CYInternal *internal;
    object_getInstanceVariable(self, "cy$internal_", reinterpret_cast<void **>(&internal));
    if (internal != NULL)
        delete internal;
    _CYDealloc(self, sel);
}

MSHook(void, objc_registerClassPair, Class _class) {
    Class super(class_getSuperclass(_class));
    if (super == NULL || class_getInstanceVariable(super, "cy$internal_") == NULL) {
        class_addIvar(_class, "cy$internal_", sizeof(CYInternal *), log2(sizeof(CYInternal *)), "^{CYInternal}");
        MSHookMessage(_class, @selector(dealloc), MSHake(CYDealloc));
    }

    _objc_registerClassPair(_class);
}

static JSValueRef objc_registerClassPair_(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to objc_registerClassPair" userInfo:nil];
        CYPool pool;
        Class _class(CYCastNSObject(pool, context, arguments[0]));
        $objc_registerClassPair(_class);
        return CYJSUndefined(context);
    } CYCatch
}

static JSValueRef Selector_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    JSValueRef setup[count + 2];
    setup[0] = _this;
    setup[1] = object;
    memcpy(setup + 2, arguments, sizeof(JSValueRef) * count);
    return $objc_msgSend(context, NULL, NULL, count + 2, setup, exception);
}

static JSValueRef Functor_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYPool pool;
    Functor_privateData *data(reinterpret_cast<Functor_privateData *>(JSObjectGetPrivate(object)));
    return CYCallFunction(pool, context, 0, NULL, count, arguments, false, exception, &data->signature_, &data->cif_, reinterpret_cast<void (*)()>(data->value_));
}

JSObjectRef Selector_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Selector constructor" userInfo:nil];
        const char *name(CYCastCString(context, arguments[0]));
        return CYMakeSelector(context, sel_registerName(name));
    } CYCatch
}

JSObjectRef Pointer_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Functor constructor" userInfo:nil];

        void *value(CYCastPointer<void *>(context, arguments[0]));
        const char *type(CYCastCString(context, arguments[1]));

        CYPool pool;

        sig::Signature signature;
        sig::Parse(pool, &signature, type, &Structor_);

        return CYMakePointer(context, value, signature.elements[0].type, NULL, NULL);
    } CYCatch
}

JSObjectRef CYMakeType(JSContextRef context, JSObjectRef object, const char *type) {
    Type_privateData *internal(new Type_privateData(NULL, type));
    return JSObjectMake(context, Type_, internal);
}

JSObjectRef Type_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Type constructor" userInfo:nil];
        const char *type(CYCastCString(context, arguments[0]));
        return CYMakeType(context, object, type);
    } CYCatch
}

static JSValueRef Type_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to type cast function" userInfo:nil];
        Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
        sig::Type *type(internal->type_);
        ffi_type *ffi(internal->GetFFI());
        // XXX: alignment?
        uint8_t value[ffi->size];
        CYPool pool;
        CYPoolFFI(pool, context, type, ffi, value, arguments[0]);
        return CYFromFFI(context, type, ffi, value);
    } CYCatch
}

static JSObjectRef Type_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count > 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to type cast function" userInfo:nil];
        Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(object)));
        size_t size(count == 0 ? 0 : CYCastDouble(context, arguments[0]));
        // XXX: alignment?
        void *value(malloc(internal->GetFFI()->size * size));
        return CYMakePointer(context, value, internal->type_, internal->ffi_, NULL);
    } CYCatch
}

JSObjectRef Instance_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count > 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Instance constructor" userInfo:nil];
        id self(count == 0 ? nil : CYCastPointer<id>(context, arguments[0]));
        return Instance::Make(context, self, Instance::None);
    } CYCatch
}

JSObjectRef Functor_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Functor constructor" userInfo:nil];
        const char *type(CYCastCString(context, arguments[1]));
        JSValueRef exception(NULL);
        if (JSValueIsInstanceOfConstructor(context, arguments[0], Function_, &exception)) {
            JSObjectRef function(CYCastJSObject(context, arguments[0]));
            return CYMakeFunctor(context, function, type);
        } else if (exception != NULL) {
            return NULL;
        } else {
            void (*function)()(CYCastPointer<void (*)()>(context, arguments[0]));
            return CYMakeFunctor(context, function, type);
        }
    } CYCatch
}

JSValueRef CYValue_getProperty_value(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
}

JSValueRef Selector_getProperty_prototype(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    return Function_;
}

static JSValueRef CYValue_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));
    Type_privateData *typical(internal->GetType());

    sig::Type *type;
    ffi_type *ffi;

    if (typical == NULL) {
        type = NULL;
        ffi = NULL;
    } else {
        type = typical->type_;
        ffi = typical->ffi_;
    }

    return CYMakePointer(context, &internal->value_, type, ffi, object);
}

static JSValueRef CYValue_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));

    CYTry {
        return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
    } CYCatch
}

static JSValueRef CYValue_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return CYValue_callAsFunction_valueOf(context, object, _this, count, arguments, exception);
}

static JSValueRef CYValue_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYValue *internal(reinterpret_cast<CYValue *>(JSObjectGetPrivate(_this)));
    char string[32];
    sprintf(string, "%p", internal->value_);

    CYTry {
        return CYCastJSValue(context, string);
    } CYCatch
}

static JSValueRef Instance_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));

    CYTry {
        CYPoolTry {
            return CYCastJSValue(context, CYJSString(CYPoolNSCYON(NULL, internal->GetValue())));
        } CYPoolCatch(NULL)
    } CYCatch
}

static JSValueRef Instance_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));

    CYTry {
        CYPoolTry {
            NSString *key(count == 0 ? nil : CYCastNSString(NULL, CYJSString(context, arguments[0])));
            return CYCastJSValue(context, CYJSString([internal->GetValue() cy$toJSON:key]));
        } CYPoolCatch(NULL)
    } CYCatch
}

static JSValueRef Instance_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Instance *data(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));

    CYTry {
        CYPoolTry {
            return CYCastJSValue(context, CYJSString([data->GetValue() description]));
        } CYPoolCatch(NULL)
    } CYCatch
}

static JSValueRef Selector_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Selector_privateData *data(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate(_this)));

    CYTry {
        return CYCastJSValue(context, sel_getName(data->GetValue()));
    } CYCatch
}

static JSValueRef Selector_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return Selector_callAsFunction_toString(context, object, _this, count, arguments, exception);
}

static JSValueRef Selector_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    Selector_privateData *internal(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate(_this)));
    const char *name(sel_getName(internal->GetValue()));

    CYTry {
        CYPoolTry {
            return CYCastJSValue(context, CYJSString([NSString stringWithFormat:@"@selector(%s)", name]));
        } CYPoolCatch(NULL)
    } CYCatch
}

static JSValueRef Selector_callAsFunction_type(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        if (count != 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Selector.type" userInfo:nil];
        CYPool pool;
        Selector_privateData *internal(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate(_this)));
        Class _class(CYCastNSObject(pool, context, arguments[0]));
        SEL sel(internal->GetValue());
        if (Method method = class_getInstanceMethod(_class, sel))
            return CYCastJSValue(context, method_getTypeEncoding(method));
        else if (NSString *type = [[Bridge_ objectAtIndex:1] objectForKey:CYCastNSString(pool, sel_getName(sel))])
            return CYCastJSValue(context, CYJSString(type));
        else
            return CYJSNull(context);
    } CYCatch
}

static JSValueRef Type_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
        CYPool pool;
        const char *type(sig::Unparse(pool, internal->type_));
        CYPoolTry {
            return CYCastJSValue(context, CYJSString(type));
        } CYPoolCatch(NULL)
    } CYCatch
}

static JSValueRef Type_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    CYTry {
        Type_privateData *internal(reinterpret_cast<Type_privateData *>(JSObjectGetPrivate(_this)));
        CYPool pool;
        const char *type(sig::Unparse(pool, internal->type_));
        CYPoolTry {
            return CYCastJSValue(context, CYJSString([NSString stringWithFormat:@"new Type(%@)", [[NSString stringWithUTF8String:type] cy$toCYON]]));
        } CYPoolCatch(NULL)
    } CYCatch
}

static JSValueRef Type_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return Type_callAsFunction_toString(context, object, _this, count, arguments, exception);
}

static JSStaticValue CYValue_staticValues[2] = {
    {"value", &CYValue_getProperty_value, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticValue Pointer_staticValues[2] = {
    {"$cyi", &Pointer_getProperty_$cyi, &Pointer_setProperty_$cyi, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Pointer_staticFunctions[4] = {
    {"toCYON", &CYValue_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
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

/*static JSStaticValue Selector_staticValues[2] = {
    {"prototype", &Selector_getProperty_prototype, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};*/

static JSStaticValue Instance_staticValues[2] = {
    {"value", &CYValue_getProperty_value, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Instance_staticFunctions[5] = {
    {"$cya", &CYValue_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toCYON", &Instance_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Instance_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Instance_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Internal_staticFunctions[2] = {
    {"$cya", &Internal_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Selector_staticFunctions[5] = {
    {"toCYON", &Selector_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Selector_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Selector_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"type", &Selector_callAsFunction_type, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Type_staticFunctions[4] = {
    {"toCYON", &Type_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Type_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Type_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

CYDriver::CYDriver(const std::string &filename) :
    state_(CYClear),
    data_(NULL),
    size_(0),
    filename_(filename),
    source_(NULL)
{
    ScannerInit();
}

CYDriver::~CYDriver() {
    ScannerDestroy();
}

void cy::parser::error(const cy::parser::location_type &location, const std::string &message) {
    CYDriver::Error error;
    error.location_ = location;
    error.message_ = message;
    driver.errors_.push_back(error);
}

void CYSetArgs(int argc, const char *argv[]) {
    JSContextRef context(CYGetJSContext());
    JSValueRef args[argc];
    for (int i(0); i != argc; ++i)
        args[i] = CYCastJSValue(context, argv[i]);
    JSValueRef exception(NULL);
    JSObjectRef array(JSObjectMakeArray(context, argc, args, &exception));
    CYThrow(context, exception);
    CYSetProperty(context, System_, CYJSString("args"), array);
}

JSObjectRef CYGetGlobalObject(JSContextRef context) {
    return JSContextGetGlobalObject(context);
}

const char *CYExecute(apr_pool_t *pool, const char *code) { _pooled
    JSContextRef context(CYGetJSContext());
    JSValueRef exception(NULL), result;

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

    const char *json(CYPoolCCYON(pool, context, result, &exception));
    if (exception != NULL)
        goto error;

    CYSetProperty(context, CYGetGlobalObject(context), Result_, result);
    return json;
}

bool CYRecvAll_(int socket, uint8_t *data, size_t size) {
    while (size != 0) if (size_t writ = _syscall(recv(socket, data, size, 0))) {
        data += writ;
        size -= writ;
    } else
        return false;
    return true;
}

bool CYSendAll_(int socket, const uint8_t *data, size_t size) {
    while (size != 0) if (size_t writ = _syscall(send(socket, data, size, 0))) {
        data += writ;
        size -= writ;
    } else
        return false;
    return true;
}

static int Socket_;
apr_pool_t *Pool_;

struct CYExecute_ {
    apr_pool_t *pool_;
    const char * volatile data_;
};

// XXX: this is "tre lame"
@interface CYClient_ : NSObject {
}

- (void) execute:(NSValue *)value;

@end

@implementation CYClient_

- (void) execute:(NSValue *)value {
    CYExecute_ *execute(reinterpret_cast<CYExecute_ *>([value pointerValue]));
    const char *data(execute->data_);
    execute->data_ = NULL;
    execute->data_ = CYExecute(execute->pool_, data);
}

@end

struct CYClient :
    CYData
{
    int socket_;
    apr_thread_t *thread_;

    CYClient(int socket) :
        socket_(socket)
    {
    }

    ~CYClient() {
        _syscall(close(socket_));
    }

    void Handle() { _pooled
        CYClient_ *client = [[[CYClient_ alloc] init] autorelease];

        for (;;) {
            size_t size;
            if (!CYRecvAll(socket_, &size, sizeof(size)))
                return;

            CYPool pool;
            char *data(new(pool) char[size + 1]);
            if (!CYRecvAll(socket_, data, size))
                return;
            data[size] = '\0';

            CYDriver driver("");
            cy::parser parser(driver);

            driver.data_ = data;
            driver.size_ = size;

            const char *json;
            if (parser.parse() != 0 || !driver.errors_.empty()) {
                json = NULL;
                size = _not(size_t);
            } else {
                std::ostringstream str;
                driver.source_->Show(str);
                std::string code(str.str());
                CYExecute_ execute = {pool, code.c_str()};
                [client performSelectorOnMainThread:@selector(execute:) withObject:[NSValue valueWithPointer:&execute] waitUntilDone:YES];
                json = execute.data_;
                size = json == NULL ? _not(size_t) : strlen(json);
            }

            if (!CYSendAll(socket_, &size, sizeof(size)))
                return;
            if (json != NULL)
                if (!CYSendAll(socket_, json, size))
                    return;
        }
    }
};

static void * APR_THREAD_FUNC OnClient(apr_thread_t *thread, void *data) {
    CYClient *client(reinterpret_cast<CYClient *>(data));
    client->Handle();
    delete client;
    return NULL;
}

static void * APR_THREAD_FUNC Cyrver(apr_thread_t *thread, void *data) {
    for (;;) {
        int socket(_syscall(accept(Socket_, NULL, NULL)));
        CYClient *client(new CYClient(socket));
        apr_threadattr_t *attr;
        _aprcall(apr_threadattr_create(&attr, Pool_));
        _aprcall(apr_thread_create(&client->thread_, attr, &OnClient, client, client->pool_));
    }

    return NULL;
}

void Unlink() {
    pid_t pid(getpid());
    char path[104];
    sprintf(path, "/tmp/.s.cy.%u", pid);
    unlink(path);
}

MSInitialize { _pooled
    _aprcall(apr_initialize());
    _aprcall(apr_pool_create(&Pool_, NULL));

    Type_privateData::Object = new(Pool_) Type_privateData(Pool_, "@");
    Type_privateData::Selector = new(Pool_) Type_privateData(Pool_, ":");

    Bridge_ = [[NSMutableArray arrayWithContentsOfFile:@"/usr/lib/libcycript.plist"] retain];

    NSCFBoolean_ = objc_getClass("NSCFBoolean");
    NSCFType_ = objc_getClass("NSCFType");
    NSMessageBuilder_ = objc_getClass("NSMessageBuilder");
    NSZombie_ = objc_getClass("_NSZombie_");
    Object_ = objc_getClass("Object");

    Socket_ = _syscall(socket(PF_UNIX, SOCK_STREAM, 0));

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;

    pid_t pid(getpid());
    sprintf(address.sun_path, "/tmp/.s.cy.%u", pid);

    try {
        _syscall(bind(Socket_, reinterpret_cast<sockaddr *>(&address), SUN_LEN(&address)));
        atexit(&Unlink);
        _syscall(listen(Socket_, 0));

        apr_threadattr_t *attr;
        _aprcall(apr_threadattr_create(&attr, Pool_));

        apr_thread_t *thread;
        _aprcall(apr_thread_create(&thread, attr, &Cyrver, NULL, Pool_));
    } catch (...) {
        NSLog(@"failed to setup Cyrver");
    }
}

JSGlobalContextRef CYGetJSContext() {
    if (Context_ == NULL) {
        JSClassDefinition definition;

        definition = kJSClassDefinitionEmpty;
        definition.className = "Functor";
        definition.staticFunctions = Functor_staticFunctions;
        definition.callAsFunction = &Functor_callAsFunction;
        definition.finalize = &CYData::Finalize;
        Functor_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Instance";
        definition.staticValues = Instance_staticValues;
        definition.staticFunctions = Instance_staticFunctions;
        definition.hasProperty = &Instance_hasProperty;
        definition.getProperty = &Instance_getProperty;
        definition.setProperty = &Instance_setProperty;
        definition.deleteProperty = &Instance_deleteProperty;
        definition.getPropertyNames = &Instance_getPropertyNames;
        definition.callAsConstructor = &Instance_callAsConstructor;
        definition.finalize = &CYData::Finalize;
        Instance_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Internal";
        definition.staticFunctions = Internal_staticFunctions;
        definition.hasProperty = &Internal_hasProperty;
        definition.getProperty = &Internal_getProperty;
        definition.setProperty = &Internal_setProperty;
        definition.getPropertyNames = &Internal_getPropertyNames;
        definition.finalize = &CYData::Finalize;
        Internal_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Pointer";
        definition.staticValues = Pointer_staticValues;
        definition.staticFunctions = Pointer_staticFunctions;
        definition.getProperty = &Pointer_getProperty;
        definition.setProperty = &Pointer_setProperty;
        definition.finalize = &CYData::Finalize;
        Pointer_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Selector";
        definition.staticValues = CYValue_staticValues;
        //definition.staticValues = Selector_staticValues;
        definition.staticFunctions = Selector_staticFunctions;
        definition.callAsFunction = &Selector_callAsFunction;
        definition.finalize = &CYData::Finalize;
        Selector_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Struct";
        definition.staticFunctions = Struct_staticFunctions;
        definition.getProperty = &Struct_getProperty;
        definition.setProperty = &Struct_setProperty;
        definition.getPropertyNames = &Struct_getPropertyNames;
        definition.finalize = &CYData::Finalize;
        Struct_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Type";
        definition.staticFunctions = Type_staticFunctions;
        //definition.getProperty = &Type_getProperty;
        definition.callAsFunction = &Type_callAsFunction;
        definition.callAsConstructor = &Type_callAsConstructor;
        definition.finalize = &CYData::Finalize;
        Type_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "Runtime";
        definition.getProperty = &Runtime_getProperty;
        Runtime_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "ObjectiveC::Classes";
        definition.getProperty = &ObjectiveC_Classes_getProperty;
        definition.getPropertyNames = &ObjectiveC_Classes_getPropertyNames;
        ObjectiveC_Classes_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "ObjectiveC::Images";
        definition.getProperty = &ObjectiveC_Images_getProperty;
        definition.getPropertyNames = &ObjectiveC_Images_getPropertyNames;
        ObjectiveC_Images_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "ObjectiveC::Image::Classes";
        definition.getProperty = &ObjectiveC_Image_Classes_getProperty;
        definition.getPropertyNames = &ObjectiveC_Image_Classes_getPropertyNames;
        definition.finalize = &CYData::Finalize;
        ObjectiveC_Image_Classes_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        definition.className = "ObjectiveC::Protocols";
        definition.getProperty = &ObjectiveC_Protocols_getProperty;
        definition.getPropertyNames = &ObjectiveC_Protocols_getPropertyNames;
        ObjectiveC_Protocols_ = JSClassCreate(&definition);

        definition = kJSClassDefinitionEmpty;
        //definition.getProperty = &Global_getProperty;
        JSClassRef Global(JSClassCreate(&definition));

        JSGlobalContextRef context(JSGlobalContextCreate(Global));
        Context_ = context;

        JSObjectRef global(CYGetGlobalObject(context));

        JSObjectSetPrototype(context, global, JSObjectMake(context, Runtime_, NULL));
        ObjectiveC_ = JSObjectMake(context, NULL, NULL);
        CYSetProperty(context, global, CYJSString("ObjectiveC"), ObjectiveC_);

        CYSetProperty(context, ObjectiveC_, CYJSString("classes"), JSObjectMake(context, ObjectiveC_Classes_, NULL));
        CYSetProperty(context, ObjectiveC_, CYJSString("images"), JSObjectMake(context, ObjectiveC_Images_, NULL));
        CYSetProperty(context, ObjectiveC_, CYJSString("protocols"), JSObjectMake(context, ObjectiveC_Protocols_, NULL));

        CYSetProperty(context, global, CYJSString("Functor"), JSObjectMakeConstructor(context, Functor_, &Functor_new));
        CYSetProperty(context, global, CYJSString("Instance"), JSObjectMakeConstructor(context, Instance_, &Instance_new));
        CYSetProperty(context, global, CYJSString("Pointer"), JSObjectMakeConstructor(context, Pointer_, &Pointer_new));
        CYSetProperty(context, global, CYJSString("Selector"), JSObjectMakeConstructor(context, Selector_, &Selector_new));
        CYSetProperty(context, global, CYJSString("Type"), JSObjectMakeConstructor(context, Type_, &Type_new));

        MSHookFunction(&objc_registerClassPair, MSHake(objc_registerClassPair));

        class_addMethod(NSCFType_, @selector(cy$toJSON:), reinterpret_cast<IMP>(&NSCFType$cy$toJSON), "@12@0:4@8");

        CYSetProperty(context, global, CYJSString("objc_registerClassPair"), JSObjectMakeFunctionWithCallback(context, CYJSString("objc_registerClassPair"), &objc_registerClassPair_));
        CYSetProperty(context, global, CYJSString("objc_msgSend"), JSObjectMakeFunctionWithCallback(context, CYJSString("objc_msgSend"), &$objc_msgSend));

        System_ = JSObjectMake(context, NULL, NULL);
        CYSetProperty(context, global, CYJSString("system"), System_);
        CYSetProperty(context, System_, CYJSString("args"), CYJSNull(context));
        //CYSetProperty(context, System_, CYJSString("global"), global);

        CYSetProperty(context, System_, CYJSString("print"), JSObjectMakeFunctionWithCallback(context, CYJSString("print"), &System_print));

        Result_ = JSStringCreateWithUTF8CString("_");

        length_ = JSStringCreateWithUTF8CString("length");
        message_ = JSStringCreateWithUTF8CString("message");
        name_ = JSStringCreateWithUTF8CString("name");
        toCYON_ = JSStringCreateWithUTF8CString("toCYON");
        toJSON_ = JSStringCreateWithUTF8CString("toJSON");

        Array_ = CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Array")));
        Function_ = CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Function")));
    }

    return Context_;
}
