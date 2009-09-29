/* Cyrker - Remove Execution Server and Disassembler
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
#include "Struct.hpp"

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include <apr-1/apr_pools.h>
#include <apr-1/apr_strings.h>

#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFLogUtilities.h>

#include <CFNetwork/CFNetwork.h>
#include <Foundation/Foundation.h>

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>

#include <WebKit/WebScriptObject.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <iostream>
#include <ext/stdio_filebuf.h>
#include <set>
#include <map>

#include "Parser.hpp"

#undef _assert
#undef _trace

#define _assert(test) do { \
    if (!(test)) \
        @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"_assert(%s):%s(%u):%s", #test, __FILE__, __LINE__, __FUNCTION__] userInfo:nil]; \
} while (false)

#define _trace() do { \
    CFLog(kCFLogLevelNotice, CFSTR("_trace():%u"), __LINE__); \
} while (false)

/* APR Pool Helpers {{{ */
void *operator new(size_t size, apr_pool_t *pool) {
    return apr_palloc(pool, size);
}

void *operator new [](size_t size, apr_pool_t *pool) {
    return apr_palloc(pool, size);
}

class CYPool {
  private:
    apr_pool_t *pool_;

  public:
    CYPool() {
        apr_pool_create(&pool_, NULL);
    }

    ~CYPool() {
        apr_pool_destroy(pool_);
    }

    operator apr_pool_t *() const {
        return pool_;
    }

    char *operator ()(const char *data) const {
        return apr_pstrdup(pool_, data);
    }

    char *operator ()(const char *data, size_t size) const {
        return apr_pstrndup(pool_, data, size);
    }
};
/* }}} */

#define _pooled _H<NSAutoreleasePool> _pool([[NSAutoreleasePool alloc] init], true);

static JSContextRef Context_;

static JSClassRef Functor_;
static JSClassRef Instance_;
static JSClassRef Pointer_;
static JSClassRef Selector_;

static JSObjectRef Array_;

static JSStringRef name_;
static JSStringRef message_;
static JSStringRef length_;

static Class NSCFBoolean_;

static NSMutableDictionary *Bridge_;

struct Client {
    CFHTTPMessageRef message_;
    CFSocketRef socket_;
};

JSObjectRef CYMakeObject(JSContextRef context, id object) {
    return JSObjectMake(context, Instance_, [object retain]);
}

@interface NSMethodSignature (Cycript)
- (NSString *) _typeString;
@end

@interface NSObject (Cycript)
- (NSString *) cy$toJSON;
- (JSValueRef) cy$JSValueInContext:(JSContextRef)context;
@end

@interface NSString (Cycript)
- (void *) cy$symbol;
@end

@interface NSNumber (Cycript)
- (void *) cy$symbol;
@end

@implementation NSObject (Cycript)

- (NSString *) cy$toJSON {
    return [self description];
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return CYMakeObject(context, self);
}

@end

@implementation WebUndefined (Cycript)

- (NSString *) cy$toJSON {
    return @"undefined";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return JSValueMakeUndefined(context);
}

@end

@implementation NSArray (Cycript)

- (NSString *) cy$toJSON {
    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"["];

    bool comma(false);
    for (id object in self) {
        if (comma)
            [json appendString:@","];
        else
            comma = true;
        [json appendString:[object cy$toJSON]];
    }

    [json appendString:@"]"];
    return json;
}

@end

@implementation NSDictionary (Cycript)

- (NSString *) cy$toJSON {
    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"("];
    [json appendString:@"{"];

    bool comma(false);
    for (id key in self) {
        if (comma)
            [json appendString:@","];
        else
            comma = true;
        [json appendString:[key cy$toJSON]];
        [json appendString:@":"];
        NSObject *object([self objectForKey:key]);
        [json appendString:[object cy$toJSON]];
    }

    [json appendString:@"})"];
    return json;
}

@end

@implementation NSNumber (Cycript)

- (NSString *) cy$toJSON {
    return [self class] != NSCFBoolean_ ? [self stringValue] : [self boolValue] ? @"true" : @"false";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return [self class] != NSCFBoolean_ ? JSValueMakeNumber(context, [self doubleValue]) : JSValueMakeBoolean(context, [self boolValue]);
}

- (void *) cy$symbol {
    return [self pointerValue];
}

@end

@implementation NSString (Cycript)

- (NSString *) cy$toJSON {
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

- (void *) cy$symbol {
    return dlsym(RTLD_DEFAULT, [self UTF8String]);
}

@end

@interface CYJSObject : NSDictionary {
    JSObjectRef object_;
    JSContextRef context_;
}

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context;

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

JSContextRef JSGetContext() {
    return Context_;
}

#define CYCatch \
    @catch (id error) { \
        CYThrow(context, error, exception); \
        return NULL; \
    }

void CYThrow(JSContextRef context, JSValueRef value);

id CYCastNSObject(JSContextRef context, JSObjectRef object) {
    if (JSValueIsObjectOfClass(context, object, Instance_))
        return reinterpret_cast<id>(JSObjectGetPrivate(object));
    JSValueRef exception(NULL);
    bool array(JSValueIsInstanceOfConstructor(context, object, Array_, &exception));
    CYThrow(context, exception);
    if (array)
        return [[[CYJSArray alloc] initWithJSObject:object inContext:context] autorelease];
    return [[[CYJSObject alloc] initWithJSObject:object inContext:context] autorelease];
}

JSStringRef CYCopyJSString(id value) {
    return JSStringCreateWithCFString(reinterpret_cast<CFStringRef>([value description]));
}

JSStringRef CYCopyJSString(const char *value) {
    return JSStringCreateWithUTF8CString(value);
}

JSStringRef CYCopyJSString(JSStringRef value) {
    return JSStringRetain(value);
}

JSStringRef CYCopyJSString(JSContextRef context, JSValueRef value) {
    JSValueRef exception(NULL);
    JSStringRef string(JSValueToStringCopy(context, value, &exception));
    CYThrow(context, exception);
    return string;
}

// XXX: this is not a safe handle
class CYString {
  private:
    JSStringRef string_;

  public:
    template <typename Arg0_>
    CYString(Arg0_ arg0) {
        string_ = CYCopyJSString(arg0);
    }

    template <typename Arg0_, typename Arg1_>
    CYString(Arg0_ arg0, Arg1_ arg1) {
        string_ = CYCopyJSString(arg0, arg1);
    }

    ~CYString() {
        JSStringRelease(string_);
    }

    operator JSStringRef() const {
        return string_;
    }
};

CFStringRef CYCopyCFString(JSStringRef value) {
    return JSStringCopyCFString(kCFAllocatorDefault, value);
}

CFStringRef CYCopyCFString(JSContextRef context, JSValueRef value) {
    return CYCopyCFString(CYString(context, value));
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

NSString *CYCastNSString(JSStringRef value) {
    return [reinterpret_cast<const NSString *>(CYCopyCFString(value)) autorelease];
}

CFTypeRef CYCopyCFType(JSContextRef context, JSValueRef value) {
    switch (JSType type = JSValueGetType(context, value)) {
        case kJSTypeUndefined:
            return CFRetain([WebUndefined undefined]);
        case kJSTypeNull:
            return nil;
        case kJSTypeBoolean:
            return CFRetain(JSValueToBoolean(context, value) ? kCFBooleanTrue : kCFBooleanFalse);
        case kJSTypeNumber:
            return CYCopyCFNumber(context, value);
        case kJSTypeString:
            return CYCopyCFString(context, value);
        case kJSTypeObject:
            return CFRetain((CFTypeRef) CYCastNSObject(context, (JSObjectRef) value));
        default:
            @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"JSValueGetType() == 0x%x", type] userInfo:nil];
    }
}

NSArray *CYCastNSArray(JSPropertyNameArrayRef names) {
    size_t size(JSPropertyNameArrayGetCount(names));
    NSMutableArray *array([NSMutableArray arrayWithCapacity:size]);
    for (size_t index(0); index != size; ++index)
        [array addObject:CYCastNSString(JSPropertyNameArrayGetNameAtIndex(names, index))];
    return array;
}

id CYCastNSObject(JSContextRef context, JSValueRef value) {
    const NSObject *object(reinterpret_cast<const NSObject *>(CYCopyCFType(context, value)));
    return object == nil ? nil : [object autorelease];
}

void CYThrow(JSContextRef context, JSValueRef value) {
    if (value == NULL)
        return;
    @throw CYCastNSObject(context, value);
}

JSValueRef CYCastJSValue(JSContextRef context, id value) {
    return value == nil ? JSValueMakeNull(context) : [value cy$JSValueInContext:context];
}

void CYThrow(JSContextRef context, id error, JSValueRef *exception) {
    *exception = CYCastJSValue(context, error);
}

@implementation CYJSObject

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context {
    if ((self = [super init]) != nil) {
        object_ = object;
        context_ = context;
    } return self;
}

- (NSUInteger) count {
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context_, object_));
    size_t size(JSPropertyNameArrayGetCount(names));
    JSPropertyNameArrayRelease(names);
    return size;
}

- (id) objectForKey:(id)key {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetProperty(context_, object_, CYString(key), &exception));
    CYThrow(context_, exception);
    return CYCastNSObject(context_, value);
}

- (NSEnumerator *) keyEnumerator {
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context_, object_));
    NSEnumerator *enumerator([CYCastNSArray(names) objectEnumerator]);
    JSPropertyNameArrayRelease(names);
    return enumerator;
}

- (void) setObject:(id)object forKey:(id)key {
    JSValueRef exception(NULL);
    JSObjectSetProperty(context_, object_, CYString(key), CYCastJSValue(context_, object), kJSPropertyAttributeNone, &exception);
    CYThrow(context_, exception);
}

- (void) removeObjectForKey:(id)key {
    JSValueRef exception(NULL);
    // XXX: this returns a bool... throw exception, or ignore?
    JSObjectDeleteProperty(context_, object_, CYString(key), &exception);
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
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetProperty(context_, object_, length_, &exception));
    CYThrow(context_, exception);
    return CYCastDouble(context_, value);
}

- (id) objectAtIndex:(NSUInteger)index {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetPropertyAtIndex(context_, object_, index, &exception));
    CYThrow(context_, exception);
    id object(CYCastNSObject(context_, value));
    return object == nil ? [NSNull null] : object;
}

@end

CFStringRef JSValueToJSONCopy(JSContextRef context, JSValueRef value) {
    id object(CYCastNSObject(context, value));
    return reinterpret_cast<CFStringRef>([(object == nil ? @"null" : [object cy$toJSON]) retain]);
}

static void OnData(CFSocketRef socket, CFSocketCallBackType type, CFDataRef address, const void *value, void *info) {
    switch (type) {
        case kCFSocketDataCallBack:
            CFDataRef data(reinterpret_cast<CFDataRef>(value));
            Client *client(reinterpret_cast<Client *>(info));

            if (client->message_ == NULL)
                client->message_ = CFHTTPMessageCreateEmpty(kCFAllocatorDefault, TRUE);

            if (!CFHTTPMessageAppendBytes(client->message_, CFDataGetBytePtr(data), CFDataGetLength(data)))
                CFLog(kCFLogLevelError, CFSTR("CFHTTPMessageAppendBytes()"));
            else if (CFHTTPMessageIsHeaderComplete(client->message_)) {
                CFURLRef url(CFHTTPMessageCopyRequestURL(client->message_));
                Boolean absolute;
                CFStringRef path(CFURLCopyStrictPath(url, &absolute));
                CFRelease(client->message_);

                CFStringRef code(CFURLCreateStringByReplacingPercentEscapes(kCFAllocatorDefault, path, CFSTR("")));
                CFRelease(path);

                JSStringRef script(JSStringCreateWithCFString(code));
                CFRelease(code);

                JSValueRef result(JSEvaluateScript(JSGetContext(), script, NULL, NULL, 0, NULL));
                JSStringRelease(script);

                CFHTTPMessageRef response(CFHTTPMessageCreateResponse(kCFAllocatorDefault, 200, NULL, kCFHTTPVersion1_1));
                CFHTTPMessageSetHeaderFieldValue(response, CFSTR("Content-Type"), CFSTR("application/json; charset=utf-8"));

                CFStringRef json(JSValueToJSONCopy(JSGetContext(), result));
                CFDataRef body(CFStringCreateExternalRepresentation(kCFAllocatorDefault, json, kCFStringEncodingUTF8, NULL));
                CFRelease(json);

                CFStringRef length(CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%u"), CFDataGetLength(body)));
                CFHTTPMessageSetHeaderFieldValue(response, CFSTR("Content-Length"), length);
                CFRelease(length);

                CFHTTPMessageSetBody(response, body);
                CFRelease(body);

                CFDataRef serialized(CFHTTPMessageCopySerializedMessage(response));
                CFRelease(response);

                CFSocketSendData(socket, NULL, serialized, 0);
                CFRelease(serialized);

                CFRelease(url);
            }
        break;
    }
}

static void OnAccept(CFSocketRef socket, CFSocketCallBackType type, CFDataRef address, const void *value, void *info) {
    switch (type) {
        case kCFSocketAcceptCallBack:
            Client *client(new Client());

            client->message_ = NULL;

            CFSocketContext context;
            context.version = 0;
            context.info = client;
            context.retain = NULL;
            context.release = NULL;
            context.copyDescription = NULL;

            client->socket_ = CFSocketCreateWithNative(kCFAllocatorDefault, *reinterpret_cast<const CFSocketNativeHandle *>(value), kCFSocketDataCallBack, &OnData, &context);

            CFRunLoopAddSource(CFRunLoopGetCurrent(), CFSocketCreateRunLoopSource(kCFAllocatorDefault, client->socket_, 0), kCFRunLoopDefaultMode);
        break;
    }
}

static JSValueRef Instance_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { _pooled
    @try {
        NSString *name(CYCastNSString(property));
        NSLog(@"%@", name);
        return NULL;
    } CYCatch
}

typedef id jocData;

static JSObjectRef Instance_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { _pooled
    @try {
        id data(reinterpret_cast<jocData>(JSObjectGetPrivate(object)));
        return CYMakeObject(context, [[data alloc] autorelease]);
    } CYCatch
}

struct ptrData {
    apr_pool_t *pool_;
    void *value_;
    sig::Type type_;

    void *operator new(size_t size) {
        apr_pool_t *pool;
        apr_pool_create(&pool, NULL);
        void *data(apr_palloc(pool, size));
        reinterpret_cast<ptrData *>(data)->pool_ = pool;
        return data;;
    }

    ptrData(void *value) :
        value_(value)
    {
    }
};

struct ffiData : ptrData {
    sig::Signature signature_;
    ffi_cif cif_;

    ffiData(void (*value)(), const char *type) :
        ptrData(reinterpret_cast<void *>(value))
    {
        sig::Parse(pool_, &signature_, type);
        sig::sig_ffi_cif(pool_, &sig::ObjectiveC, &signature_, &cif_);
    }
};

struct selData : ptrData {
    selData(SEL value) :
        ptrData(value)
    {
    }
};

static void Pointer_finalize(JSObjectRef object) {
    ptrData *data(reinterpret_cast<ptrData *>(JSObjectGetPrivate(object)));
    apr_pool_destroy(data->pool_);
}

static void Instance_finalize(JSObjectRef object) {
    id data(reinterpret_cast<jocData>(JSObjectGetPrivate(object)));
    [data release];
}

JSObjectRef CYMakeFunction(JSContextRef context, void (*function)(), const char *type) {
    ffiData *data(new ffiData(function, type));
    return JSObjectMake(context, Functor_, data);
}


JSObjectRef CYMakeFunction(JSContextRef context, void *function, const char *type) {
    return CYMakeFunction(context, reinterpret_cast<void (*)()>(function), type);
}

void CYSetProperty(JSContextRef context, JSObjectRef object, const char *name, JSValueRef value) {
    JSValueRef exception(NULL);
    JSObjectSetProperty(context, object, CYString(name), value, kJSPropertyAttributeNone, &exception);
    CYThrow(context, exception);
}

char *CYPoolCString(apr_pool_t *pool, JSStringRef value) {
    size_t size(JSStringGetMaximumUTF8CStringSize(value));
    char *string(new(pool) char[size]);
    JSStringGetUTF8CString(value, string, size);
    JSStringRelease(value);
    return string;
}

char *CYPoolCString(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    return CYPoolCString(pool, CYString(context, value));
}

// XXX: this macro is unhygenic
#define CYCastCString(context, value) ({ \
    JSValueRef exception(NULL); \
    JSStringRef string(JSValueToStringCopy(context, value, &exception)); \
    CYThrow(context, exception); \
    size_t size(JSStringGetMaximumUTF8CStringSize(string)); \
    char *utf8(reinterpret_cast<char *>(alloca(size))); \
    JSStringGetUTF8CString(string, utf8, size); \
    JSStringRelease(string); \
    utf8; \
})

SEL CYCastSEL(JSContextRef context, JSValueRef value) {
    if (JSValueIsNull(context, value))
        return NULL;
    else if (JSValueIsObjectOfClass(context, value, Selector_)) {
        selData *data(reinterpret_cast<selData *>(JSObjectGetPrivate((JSObjectRef) value)));
        return reinterpret_cast<SEL>(data->value_);
    } else
        return sel_registerName(CYCastCString(context, value));
}

void *CYCastPointer(JSContextRef context, JSValueRef value) {
    switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return NULL;
        case kJSTypeString:
            return dlsym(RTLD_DEFAULT, CYCastCString(context, value));
        case kJSTypeObject:
            if (JSValueIsObjectOfClass(context, value, Pointer_)) {
                ptrData *data(reinterpret_cast<ptrData *>(JSObjectGetPrivate((JSObjectRef) value)));
                return data->value_;
            }
        default:
            return reinterpret_cast<void *>(static_cast<uintptr_t>(CYCastDouble(context, value)));
    }
}

void CYPoolFFI(apr_pool_t *pool, JSContextRef context, sig::Type *type, void *data, JSValueRef value) {
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
            *reinterpret_cast<id *>(data) = CYCastNSObject(context, value);
        break;

        case sig::selector_P:
            *reinterpret_cast<SEL *>(data) = CYCastSEL(context, value);
        break;

        case sig::pointer_P:
            *reinterpret_cast<void **>(data) = CYCastPointer(context, value);
        break;

        case sig::string_P:
            *reinterpret_cast<char **>(data) = CYPoolCString(pool, context, value);
        break;

        case sig::struct_P:
            goto fail;

        case sig::void_P:
        break;

        default: fail:
            NSLog(@"CYPoolFFI(%c)\n", type->primitive);
            _assert(false);
    }
}

JSValueRef CYFromFFI(JSContextRef context, sig::Type *type, void *data) {
    JSValueRef value;

    switch (type->primitive) {
        case sig::boolean_P:
            value = JSValueMakeBoolean(context, *reinterpret_cast<bool *>(data));
        break;

#define CYFromFFI_(primitive, native) \
        case sig::primitive ## _P: \
            value = JSValueMakeNumber(context, *reinterpret_cast<native *>(data)); \
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

        case sig::object_P:
        case sig::typename_P: {
            value = CYCastJSValue(context, *reinterpret_cast<id *>(data));
        } break;

        case sig::selector_P: {
            if (SEL sel = *reinterpret_cast<SEL *>(data)) {
                selData *data(new selData(sel));
                value = JSObjectMake(context, Selector_, data);
            } else goto null;
        } break;

        case sig::pointer_P: {
            if (void *pointer = *reinterpret_cast<void **>(data)) {
                ptrData *data(new ptrData(pointer));
                value = JSObjectMake(context, Pointer_, data);
            } else goto null;
        } break;

        case sig::string_P: {
            if (char *utf8 = *reinterpret_cast<char **>(data))
                value = JSValueMakeString(context, CYString(utf8));
            else goto null;
        } break;

        case sig::struct_P:
            goto fail;

        case sig::void_P:
            value = JSValueMakeUndefined(context);
        break;

        null:
            value = JSValueMakeNull(context);
        break;

        default: fail:
            NSLog(@"CYFromFFI(%c)\n", type->primitive);
            _assert(false);
    }

    return value;
}

static JSValueRef CYCallFunction(JSContextRef context, size_t count, const JSValueRef *arguments, JSValueRef *exception, sig::Signature *signature, ffi_cif *cif, void (*function)()) { _pooled
    @try {
        if (count != signature->count - 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to ffi function" userInfo:nil];

        CYPool pool;
        void *values[count];

        for (unsigned index(0); index != count; ++index) {
            sig::Element *element(&signature->elements[index + 1]);
            // XXX: alignment?
            values[index] = new(pool) uint8_t[cif->arg_types[index]->size];
            CYPoolFFI(pool, context, element->type, values[index], arguments[index]);
        }

        uint8_t value[cif->rtype->size];
        ffi_call(cif, function, value, values);

        return CYFromFFI(context, signature->elements[0].type, value);
    } CYCatch
}

static JSValueRef Global_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { _pooled
    @try {
        NSString *name(CYCastNSString(property));
        if (Class _class = NSClassFromString(name))
            return CYMakeObject(context, _class);
        if (NSMutableArray *entry = [Bridge_ objectForKey:name])
            switch ([[entry objectAtIndex:0] intValue]) {
                case 0:
                    return JSEvaluateScript(JSGetContext(), CYString([entry objectAtIndex:1]), NULL, NULL, 0, NULL);
                case 1:
                    return CYMakeFunction(context, [name cy$symbol], [[entry objectAtIndex:1] UTF8String]);
                case 2:
                    CYPool pool;
                    sig::Signature signature;
                    sig::Parse(pool, &signature, [[entry objectAtIndex:1] UTF8String]);
                    return CYFromFFI(context, signature.elements[0].type, [name cy$symbol]);
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

static JSValueRef $objc_msgSend(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { _pooled
    const char *type;

    @try {
        if (count < 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"too few arguments to objc_msgSend" userInfo:nil];

        id self(CYCastNSObject(context, arguments[0]));
        if (self == nil)
            return JSValueMakeNull(context);

        SEL _cmd(CYCastSEL(context, arguments[1]));
        NSMethodSignature *method([self methodSignatureForSelector:_cmd]);
        if (method == nil)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:[NSString stringWithFormat:@"unrecognized selector %s sent to object %p", sel_getName(_cmd), self] userInfo:nil];

        type = [[method _typeString] UTF8String];
    } CYCatch

    CYPool pool;

    sig::Signature signature;
    sig::Parse(pool, &signature, type);

    ffi_cif cif;
    sig::sig_ffi_cif(pool, &sig::ObjectiveC, &signature, &cif);

    void (*function)() = stret(cif.rtype) ? reinterpret_cast<void (*)()>(&objc_msgSend_stret) : reinterpret_cast<void (*)()>(&objc_msgSend);
    return CYCallFunction(context, count, arguments, exception, &signature, &cif, function);
}

static JSValueRef ffi_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    ffiData *data(reinterpret_cast<ffiData *>(JSObjectGetPrivate(object)));
    return CYCallFunction(context, count, arguments, exception, &data->signature_, &data->cif_, reinterpret_cast<void (*)()>(data->value_));
}

JSObjectRef ffi(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
        if (count != 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to ffi constructor" userInfo:nil];
        void *function(CYCastPointer(context, arguments[0]));
        const char *type(CYCastCString(context, arguments[1]));
        return CYMakeFunction(context, function, type);
    } CYCatch
}

JSValueRef Pointer_getProperty_value(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    ptrData *data(reinterpret_cast<ptrData *>(JSObjectGetPrivate(object)));
    return JSValueMakeNumber(context, reinterpret_cast<uintptr_t>(data->value_));
}

static JSStaticValue Pointer_staticValues[2] = {
    {"value", &Pointer_getProperty_value, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

void cyparse(CYParser *parser);
extern int cydebug;

void CYConsole(FILE *fin, FILE *fout, FILE *ferr) {
    cydebug = 1;
    CYParser parser;
    cyparse(&parser);
}

MSInitialize { _pooled
    apr_initialize();

    NSCFBoolean_ = objc_getClass("NSCFBoolean");

    pid_t pid(getpid());

    struct sockaddr_in address;
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(10000 + pid);

    CFDataRef data(CFDataCreate(kCFAllocatorDefault, reinterpret_cast<UInt8 *>(&address), sizeof(address)));

    CFSocketSignature signature;
    signature.protocolFamily = AF_INET;
    signature.socketType = SOCK_STREAM;
    signature.protocol = IPPROTO_TCP;
    signature.address = data;

    CFSocketRef socket(CFSocketCreateWithSocketSignature(kCFAllocatorDefault, &signature, kCFSocketAcceptCallBack, &OnAccept, NULL));
    CFRunLoopAddSource(CFRunLoopGetCurrent(), CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0), kCFRunLoopDefaultMode);

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.className = "Pointer";
    definition.staticValues = Pointer_staticValues;
    definition.finalize = &Pointer_finalize;
    Pointer_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Functor";
    definition.parentClass = Pointer_;
    definition.callAsFunction = &ffi_callAsFunction;
    Functor_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Selector";
    definition.parentClass = Pointer_;
    Selector_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Instance_";
    definition.getProperty = &Instance_getProperty;
    definition.callAsConstructor = &Instance_callAsConstructor;
    definition.finalize = &Instance_finalize;
    Instance_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.getProperty = &Global_getProperty;
    JSClassRef Global(JSClassCreate(&definition));

    JSContextRef context(JSGlobalContextCreate(Global));
    Context_ = context;

    JSObjectRef global(JSContextGetGlobalObject(context));

    CYSetProperty(context, global, "ffi", JSObjectMakeConstructor(context, Functor_, &ffi));

    CYSetProperty(context, global, "objc_msgSend", JSObjectMakeFunctionWithCallback(context, CYString("objc_msgSend"), &$objc_msgSend));

    Bridge_ = [[NSMutableDictionary dictionaryWithContentsOfFile:@"/usr/lib/libcycript.plist"] retain];

    name_ = JSStringCreateWithUTF8CString("name");
    message_ = JSStringCreateWithUTF8CString("message");
    length_ = JSStringCreateWithUTF8CString("length");

    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetProperty(JSGetContext(), global, CYString("Array"), &exception));
    CYThrow(context, exception);
    Array_ = JSValueToObject(JSGetContext(), value, &exception);
    CYThrow(context, exception);
}
