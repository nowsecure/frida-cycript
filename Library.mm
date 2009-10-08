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
#include "cycript.hpp"

#include "sig/parse.hpp"
#include "sig/ffi_type.hpp"

#include "Pooling.hpp"
#include "Struct.hpp"

#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFLogUtilities.h>

#include <CFNetwork/CFNetwork.h>

#include <WebKit/WebScriptObject.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/mman.h>

#include <iostream>
#include <ext/stdio_filebuf.h>
#include <set>
#include <map>

#include "Parser.hpp"
#include "Cycript.tab.hh"

#undef _assert
#undef _trace

#define _assert(test) do { \
    if (!(test)) \
        @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"_assert(%s):%s(%u):%s", #test, __FILE__, __LINE__, __FUNCTION__] userInfo:nil]; \
} while (false)

#define _trace() do { \
    CFLog(kCFLogLevelNotice, CFSTR("_trace():%u"), __LINE__); \
} while (false)

static JSContextRef Context_;
static JSObjectRef System_;

static JSClassRef Functor_;
static JSClassRef Instance_;
static JSClassRef Pointer_;
static JSClassRef Selector_;

static JSObjectRef Array_;
static JSObjectRef Function_;

static JSStringRef name_;
static JSStringRef message_;
static JSStringRef length_;

static Class NSCFBoolean_;

static NSMutableDictionary *Bridge_;

struct Client {
    CFHTTPMessageRef message_;
    CFSocketRef socket_;
};

JSObjectRef CYMakeInstance(JSContextRef context, id object) {
    return JSObjectMake(context, Instance_, object);
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

@interface NSMethodSignature (Cycript)
- (NSString *) _typeString;
@end

@interface NSObject (Cycript)
- (bool) cy$isUndefined;
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

- (bool) cy$isUndefined {
    return false;
}

- (NSString *) cy$toJSON {
    return [self description];
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return CYMakeInstance(context, self);
}

@end

@implementation WebUndefined (Cycript)

- (bool) cy$isUndefined {
    return true;
}

- (NSString *) cy$toJSON {
    return @"undefined";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return CYJSUndefined(context);
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
        if (![object cy$isUndefined])
            [json appendString:[object cy$toJSON]];
        else {
            [json appendString:@","];
            comma = false;
        }
    }

    [json appendString:@"]"];
    return json;
}

@end

@implementation NSDictionary (Cycript)

- (NSString *) cy$toJSON {
    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"({"];

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
    return [self class] != NSCFBoolean_ ? CYCastJSValue(context, [self doubleValue]) : CYCastJSValue(context, [self boolValue]);
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

CYRange WordStartRange_(0x1000000000LLU,0x7fffffe87fffffeLLU); // A-Za-z_$
CYRange WordEndRange_(0x3ff001000000000LLU,0x7fffffe87fffffeLLU); // A-Za-z_$0-9

JSContextRef CYGetJSContext() {
    return Context_;
}

#define CYCatch \
    @catch (id error) { \
        NSLog(@"e:%@", error); \
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
    if (pool == NULL)
        return [object autorelease];
    else {
        apr_pool_cleanup_register(pool, object, &CYPoolRelease_, &apr_pool_cleanup_null);
        return object;
    }
}

id CYCastNSObject(apr_pool_t *pool, JSContextRef context, JSObjectRef object) {
    if (JSValueIsObjectOfClass(context, object, Instance_))
        return reinterpret_cast<id>(JSObjectGetPrivate(object));
    JSValueRef exception(NULL);
    bool array(JSValueIsInstanceOfConstructor(context, object, Array_, &exception));
    CYThrow(context, exception);
    id value(array ? [CYJSArray alloc] : [CYJSObject alloc]);
    return CYPoolRelease(pool, [value initWithJSObject:object inContext:context]);
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

NSString *CYCastNSString(apr_pool_t *pool, JSStringRef value) {
    return CYPoolRelease(pool, reinterpret_cast<const NSString *>(CYCopyCFString(value)));
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
        return CYPoolRelease(pool, (id) object);
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
    return value == nil ? CYJSNull(context) : [value cy$JSValueInContext:context];
}

JSObjectRef CYCastJSObject(JSContextRef context, JSValueRef value) {
    JSValueRef exception(NULL);
    JSObjectRef object(JSValueToObject(context, value, &exception));
    CYThrow(context, exception);
    return object;
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
    return CYCastNSObject(NULL, context_, CYGetProperty(context_, object_, CYJSString(key)));
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
    // XXX: this returns a bool... throw exception, or ignore?
    JSObjectDeleteProperty(context_, object_, CYJSString(key), &exception);
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
    id object(CYCastNSObject(NULL, context_, value));
    return object == nil ? [NSNull null] : object;
}

@end

CFStringRef CYCopyJSONString(JSContextRef context, JSValueRef value) {
    id object(CYCastNSObject(NULL, context, value));
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

                JSValueRef result(JSEvaluateScript(CYGetJSContext(), script, NULL, NULL, 0, NULL));
                JSStringRelease(script);

                CFHTTPMessageRef response(CFHTTPMessageCreateResponse(kCFAllocatorDefault, 200, NULL, kCFHTTPVersion1_1));
                CFHTTPMessageSetHeaderFieldValue(response, CFSTR("Content-Type"), CFSTR("application/json; charset=utf-8"));

                CFStringRef json(CYCopyJSONString(CYGetJSContext(), result));
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

static JSValueRef Instance_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    @try {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        NSLog(@"get:%@", name);
        return NULL;
    } CYCatch
}

static bool Instance_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) {
    @try {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        NSLog(@"set:%@", name);
        return false;
    } CYCatch
}

static bool Instance_deleteProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    @try {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        NSLog(@"delete:%@", name);
        return false;
    } CYCatch
}

typedef id jocData;

static JSObjectRef Instance_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
        id data(reinterpret_cast<jocData>(JSObjectGetPrivate(object)));
        return CYMakeInstance(context, [data alloc]);
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

    ffiData(const char *type, void (*value)() = NULL) :
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

    SEL GetValue() const {
        return reinterpret_cast<SEL>(value_);
    }
};

JSObjectRef CYMakeSelector(JSContextRef context, SEL sel) {
    selData *data(new selData(sel));
    return JSObjectMake(context, Selector_, data);
}

JSObjectRef CYMakePointer(JSContextRef context, void *pointer) {
    ptrData *data(new ptrData(pointer));
    return JSObjectMake(context, Pointer_, data);
}

static void Pointer_finalize(JSObjectRef object) {
    ptrData *data(reinterpret_cast<ptrData *>(JSObjectGetPrivate(object)));
    apr_pool_destroy(data->pool_);
}

/*static void Instance_finalize(JSObjectRef object) {
    id data(reinterpret_cast<jocData>(JSObjectGetPrivate(object)));
}*/

JSObjectRef CYMakeFunctor(JSContextRef context, void (*function)(), const char *type) {
    ffiData *data(new ffiData(type, function));
    return JSObjectMake(context, Functor_, data);
}

void Closure_(ffi_cif *cif, void *result, void **arguments, void *arg) {
}

JSObjectRef CYMakeFunctor(JSContextRef context, JSObjectRef function, const char *type) {
    // XXX: in case of exceptions this will leak
    ffiData *data(new ffiData(type));

    ffi_closure *closure;
    _syscall(closure = (ffi_closure *) mmap(
        NULL, sizeof(ffi_closure),
        PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
        -1, 0
    ));

    ffi_status status(ffi_prep_closure(closure, &data->cif_, &Closure_, data));
    _assert(status == FFI_OK);

    _syscall(mprotect(closure, sizeof(*closure), PROT_READ | PROT_EXEC));

    return JSObjectMake(context, Functor_, data);
}

char *CYPoolCString(apr_pool_t *pool, JSStringRef value) {
    size_t size(JSStringGetMaximumUTF8CStringSize(value));
    char *string(new(pool) char[size]);
    JSStringGetUTF8CString(value, string, size);
    return string;
}

char *CYPoolCString(apr_pool_t *pool, JSContextRef context, JSValueRef value) {
    if (JSValueIsNull(context, value))
        return NULL;
    return CYPoolCString(pool, CYJSString(context, value));
}

// XXX: this macro is unhygenic
#define CYCastCString(context, value) ({ \
    char *utf8; \
    if (value == NULL) \
        utf8 = NULL; \
    else { \
        JSStringRef string(CYCopyJSString(context, value)); \
        size_t size(JSStringGetMaximumUTF8CStringSize(string)); \
        utf8 = reinterpret_cast<char *>(alloca(size)); \
        JSStringGetUTF8CString(string, utf8, size); \
        JSStringRelease(string); \
    } \
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

void *CYCastPointer_(JSContextRef context, JSValueRef value) {
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

template <typename Type_>
_finline Type_ CYCastPointer(JSContextRef context, JSValueRef value) {
    return reinterpret_cast<Type_>(CYCastPointer_(context, value));
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
            *reinterpret_cast<id *>(data) = CYCastNSObject(pool, context, value);
        break;

        case sig::selector_P:
            *reinterpret_cast<SEL *>(data) = CYCastSEL(context, value);
        break;

        case sig::pointer_P:
            *reinterpret_cast<void **>(data) = CYCastPointer<void *>(context, value);
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

        case sig::object_P:
            value = CYCastJSValue(context, *reinterpret_cast<id *>(data));
        break;

        case sig::typename_P:
            value = CYMakeInstance(context, *reinterpret_cast<Class *>(data));
        break;

        case sig::selector_P:
            if (SEL sel = *reinterpret_cast<SEL *>(data))
                value = CYMakeSelector(context, sel);
            else goto null;
        break;

        case sig::pointer_P:
            if (void *pointer = *reinterpret_cast<void **>(data))
                value = CYMakePointer(context, pointer);
            else goto null;
        break;

        case sig::string_P:
            if (char *utf8 = *reinterpret_cast<char **>(data))
                value = CYCastJSValue(context, utf8);
            else goto null;
        break;

        case sig::struct_P:
            goto fail;

        case sig::void_P:
            value = CYJSUndefined(context);
        break;

        null:
            value = CYJSNull(context);
        break;

        default: fail:
            NSLog(@"CYFromFFI(%c)\n", type->primitive);
            _assert(false);
    }

    return value;
}

static JSValueRef CYCallFunction(JSContextRef context, size_t count, const JSValueRef *arguments, JSValueRef *exception, sig::Signature *signature, ffi_cif *cif, void (*function)()) {
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

static JSValueRef Global_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    @try {
        CYPool pool;
        NSString *name(CYCastNSString(pool, property));
        if (Class _class = NSClassFromString(name))
            return CYMakeInstance(context, _class);
        if (NSMutableArray *entry = [Bridge_ objectForKey:name])
            switch ([[entry objectAtIndex:0] intValue]) {
                case 0:
                    return JSEvaluateScript(CYGetJSContext(), CYJSString([entry objectAtIndex:1]), NULL, NULL, 0, NULL);
                case 1:
                    return CYMakeFunctor(context, reinterpret_cast<void (*)()>([name cy$symbol]), [[entry objectAtIndex:1] UTF8String]);
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

extern "C" {
    int *_NSGetArgc(void);
    char ***_NSGetArgv(void);
    int UIApplicationMain(int argc, char *argv[], NSString *principalClassName, NSString *delegateClassName);
}

static JSValueRef System_print(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
        NSLog(@"%s", CYCastCString(context, arguments[0]));
        return CYJSUndefined(context);
    } CYCatch
}

static JSValueRef CYApplicationMain(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
        CYPool pool;
        NSString *name(CYCastNSObject(pool, context, arguments[0]));
        int argc(*_NSGetArgc() - 1);
        char **argv(*_NSGetArgv() + 1);
        for (int i(0); i != argc; ++i)
            NSLog(@"argv[%i]=%s", i, argv[i]);
        return CYCastJSValue(context, UIApplicationMain(argc, argv, name, name));
    } CYCatch
}

static JSValueRef $objc_msgSend(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    const char *type;

    @try {
        if (count < 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"too few arguments to objc_msgSend" userInfo:nil];

        CYPool pool;

        id self(CYCastNSObject(pool, context, arguments[0]));
        if (self == nil)
            return CYJSNull(context);

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

static JSValueRef Selector_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    JSValueRef setup[count + 2];
    setup[0] = _this;
    setup[1] = object;
    memmove(setup + 2, arguments, sizeof(JSValueRef) * count);
    return $objc_msgSend(context, NULL, NULL, count + 2, setup, exception);
}

static JSValueRef Functor_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    ffiData *data(reinterpret_cast<ffiData *>(JSObjectGetPrivate(object)));
    return CYCallFunction(context, count, arguments, exception, &data->signature_, &data->cif_, reinterpret_cast<void (*)()>(data->value_));
}

JSObjectRef Selector_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
        if (count != 1)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Selector constructor" userInfo:nil];
        const char *name(CYCastCString(context, arguments[0]));
        return CYMakeSelector(context, sel_registerName(name));
    } CYCatch
}

JSObjectRef Functor_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
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

JSValueRef Pointer_getProperty_value(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    ptrData *data(reinterpret_cast<ptrData *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(data->value_));
}

JSValueRef Selector_getProperty_prototype(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) {
    return Function_;
}

static JSValueRef Selector_callAsFunction_type(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    @try {
        if (count != 2)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"incorrect number of arguments to Selector.type" userInfo:nil];
        CYPool pool;
        selData *data(reinterpret_cast<selData *>(JSObjectGetPrivate(_this)));
        Class _class(CYCastNSObject(pool, context, arguments[0]));
        bool instance(CYCastBool(context, arguments[1]));
        SEL sel(data->GetValue());
        if (Method method = (*(instance ? &class_getInstanceMethod : class_getClassMethod))(_class, sel))
            return CYCastJSValue(context, method_getTypeEncoding(method));
        else if (NSString *type = [Bridge_ objectForKey:[NSString stringWithFormat:@":%s", sel_getName(sel)]])
            return CYCastJSValue(context, CYJSString(type));
        else
            return CYJSNull(context);
    } CYCatch
}

static JSStaticValue Pointer_staticValues[2] = {
    {"value", &Pointer_getProperty_value, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

/*static JSStaticValue Selector_staticValues[2] = {
    {"prototype", &Selector_getProperty_prototype, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};*/

static JSStaticFunction Selector_staticFunctions[2] = {
    {"type", &Selector_callAsFunction_type, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
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
    definition.callAsFunction = &Functor_callAsFunction;
    Functor_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Selector";
    definition.parentClass = Pointer_;
    //definition.staticValues = Selector_staticValues;
    definition.staticFunctions = Selector_staticFunctions;
    definition.callAsFunction = &Selector_callAsFunction;
    Selector_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Instance";
    definition.getProperty = &Instance_getProperty;
    definition.setProperty = &Instance_setProperty;
    definition.deleteProperty = &Instance_deleteProperty;
    definition.callAsConstructor = &Instance_callAsConstructor;
    //definition.finalize = &Instance_finalize;
    Instance_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.getProperty = &Global_getProperty;
    JSClassRef Global(JSClassCreate(&definition));

    JSContextRef context(JSGlobalContextCreate(Global));
    Context_ = context;

    JSObjectRef global(JSContextGetGlobalObject(context));

    CYSetProperty(context, global, CYJSString("Selector"), JSObjectMakeConstructor(context, Selector_, &Selector_new));
    CYSetProperty(context, global, CYJSString("Functor"), JSObjectMakeConstructor(context, Functor_, &Functor_new));

    CYSetProperty(context, global, CYJSString("CYApplicationMain"), JSObjectMakeFunctionWithCallback(context, CYJSString("CYApplicationMain"), &CYApplicationMain));
    CYSetProperty(context, global, CYJSString("objc_msgSend"), JSObjectMakeFunctionWithCallback(context, CYJSString("objc_msgSend"), &$objc_msgSend));

    System_ = JSObjectMake(context, NULL, NULL);
    CYSetProperty(context, global, CYJSString("system"), System_);
    CYSetProperty(context, System_, CYJSString("args"), CYJSNull(context));
    CYSetProperty(context, System_, CYJSString("global"), global);

    CYSetProperty(context, System_, CYJSString("print"), JSObjectMakeFunctionWithCallback(context, CYJSString("print"), &System_print));

    Bridge_ = [[NSMutableDictionary dictionaryWithContentsOfFile:@"/usr/lib/libcycript.plist"] retain];

    name_ = JSStringCreateWithUTF8CString("name");
    message_ = JSStringCreateWithUTF8CString("message");
    length_ = JSStringCreateWithUTF8CString("length");

    Array_ = CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Array")));
    Function_ = CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Function")));
}
