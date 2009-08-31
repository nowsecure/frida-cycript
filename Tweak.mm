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

#include <substrate.h>

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

/* XXX: bad _assert */
#define _assert(test) do { \
    if ((test)) break; \
    CFLog(kCFLogLevelNotice, CFSTR("_assert(%s):%u"), #test, __LINE__); \
    throw; \
} while (false)

#define _trace() do { \
    CFLog(kCFLogLevelNotice, CFSTR("_trace():%u"), __LINE__); \
} while (false)

static JSContextRef context_;
static JSClassRef joc_;
static JSObjectRef Array_;
static JSStringRef name_;
static JSStringRef message_;
static JSStringRef length_;
static Class NSCFBoolean_;

struct Client {
    CFHTTPMessageRef message_;
    CFSocketRef socket_;
};

@interface NSObject (Cyrver)
- (NSString *) cy$toJSON;
// XXX: - (JSValueRef) cy$JSValueInContext:(JSContextRef)context;
@end

@implementation NSObject (Cyrver)

- (NSString *) cy$toJSON {
    return [self description];
}

@end

@implementation WebUndefined (Cyrver)

- (NSString *) cy$toJSON {
    return @"undefined";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return JSValueMakeUndefined(context);
}

@end

@implementation NSArray (Cyrver)

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

@implementation NSDictionary (Cyrver)

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

@implementation NSNumber (Cyrver)

- (NSString *) cy$toJSON {
    return [self class] != NSCFBoolean_ ? [self stringValue] : [self boolValue] ? @"true" : @"false";
}

- (JSValueRef) cy$JSValueInContext:(JSContextRef)context {
    return [self class] != NSCFBoolean_ ? JSValueMakeNumber(context, [self doubleValue]) : JSValueMakeBoolean(context, [self boolValue]);
}

@end

@implementation NSString (Cyrver)

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

@end

@interface CY$JSObject : NSDictionary {
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

@interface CY$JSArray : NSArray {
    JSObjectRef object_;
    JSContextRef context_;
}

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context;

- (NSUInteger) count;
- (id) objectAtIndex:(NSUInteger)index;

@end

JSContextRef JSGetContext() {
    return context_;
}

id JSObjectToNSObject(JSContextRef ctx, JSObjectRef object) {
    if (JSValueIsObjectOfClass(ctx, object, joc_))
        return reinterpret_cast<id>(JSObjectGetPrivate(object));
    // XXX: exception
    else if (JSValueIsInstanceOfConstructor(ctx, object, Array_, NULL))
        return [[[CY$JSArray alloc] initWithJSObject:object inContext:ctx] autorelease];
    else
        return [[[CY$JSObject alloc] initWithJSObject:object inContext:ctx] autorelease];
}

CFStringRef CYCopyCFString(JSStringRef value) {
    return JSStringCopyCFString(kCFAllocatorDefault, value);
}

void CYThrow(JSContextRef ctx, JSValueRef value);

CFStringRef CYCopyCFString(JSContextRef ctx, JSValueRef value) {
    JSValueRef exception(NULL);
    JSStringRef string(JSValueToStringCopy(ctx, value, &exception));
    CYThrow(context_, exception);
    CFStringRef object(CYCopyCFString(string));
    JSStringRelease(string);
    return object;
}

NSString *CYCastNSString(JSStringRef value) {
    return [reinterpret_cast<const NSString *>(CYCopyCFString(value)) autorelease];
}

CFTypeRef CYCopyCFType(JSContextRef ctx, JSValueRef value) {
    JSType type(JSValueGetType(ctx, value));

    switch (type) {
        case kJSTypeUndefined:
            return CFRetain([WebUndefined undefined]);
        break;

        case kJSTypeNull:
            return nil;
        break;

        case kJSTypeBoolean:
            return CFRetain(JSValueToBoolean(ctx, value) ? kCFBooleanTrue : kCFBooleanFalse);
        break;

        case kJSTypeNumber: {
            JSValueRef exception(NULL);
            double number(JSValueToNumber(ctx, value, &exception));
            CYThrow(context_, exception);
            return CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &number);
        } break;

        case kJSTypeString:
            return CYCopyCFString(context_, value);
        break;

        case kJSTypeObject:
            return CFRetain((CFTypeRef) JSObjectToNSObject(ctx, (JSObjectRef) value));
        break;

        default:
            _assert(false);
        break;
    }
}

NSArray *CYCastNSArray(JSPropertyNameArrayRef names) {
    size_t size(JSPropertyNameArrayGetCount(names));
    NSMutableArray *array([NSMutableArray arrayWithCapacity:size]);
    for (size_t index(0); index != size; ++index)
        [array addObject:CYCastNSString(JSPropertyNameArrayGetNameAtIndex(names, index))];
    return array;
}

id CYCastNSObject(JSContextRef ctx, JSValueRef value) {
    const NSObject *object(reinterpret_cast<const NSObject *>(CYCopyCFType(ctx, value)));
    return object == nil ? nil : [object autorelease];
}

void CYThrow(JSContextRef ctx, JSValueRef value) {
    if (value == NULL)
        return;
    @throw CYCastNSObject(ctx, value);
}

JSValueRef CYCastJSValue(JSContextRef ctx, id value) {
    return [value cy$JSValueInContext:ctx];
}

JSStringRef CYCopyJSString(JSContextRef ctx, id value) {
    return JSStringCreateWithCFString(reinterpret_cast<CFStringRef>([value description]));
}

@implementation CY$JSObject

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
    JSStringRef string(CYCopyJSString(context_, key));
    JSValueRef value(JSObjectGetProperty(context_, object_, string, &exception));
    JSStringRelease(string);
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
    JSStringRef string(CYCopyJSString(context_, key));
    JSObjectSetProperty(context_, object_, string, CYCastJSValue(context_, object), kJSPropertyAttributeNone, &exception);
    JSStringRelease(string);
    CYThrow(context_, exception);
}

- (void) removeObjectForKey:(id)key {
    JSValueRef exception(NULL);
    JSStringRef string(CYCopyJSString(context_, key));
    // XXX: this returns a bool
    JSObjectDeleteProperty(context_, object_, string, &exception);
    JSStringRelease(string);
    CYThrow(context_, exception);
}

@end

@implementation CY$JSArray

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
    double number(JSValueToNumber(context_, value, &exception));
    CYThrow(context_, exception);
    return number;
}

- (id) objectAtIndex:(NSUInteger)index {
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetPropertyAtIndex(context_, object_, index, &exception));
    CYThrow(context_, exception);
    id object(CYCastNSObject(context_, value));
    return object == nil ? [NSNull null] : object;
}

@end

CFStringRef JSValueToJSONCopy(JSContextRef ctx, JSValueRef value) {
    id object(CYCastNSObject(ctx, value));
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

static JSValueRef joc_getProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exception) {
    return NULL;
}

static JSValueRef obc_getProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exception) {
    NSString *name([(NSString *) JSStringCopyCFString(kCFAllocatorDefault, propertyName) autorelease]);
    if (Class _class = NSClassFromString(name))
        return JSObjectMake(ctx, joc_, _class);
    return NULL;
}

MSInitialize {
    NSAutoreleasePool *pool([[NSAutoreleasePool alloc] init]);

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
    definition.getProperty = &obc_getProperty;
    JSClassRef obc(JSClassCreate(&definition));

    definition = kJSClassDefinitionEmpty;
    definition.getProperty = &joc_getProperty;
    joc_ = JSClassCreate(&definition);

    context_ = JSGlobalContextCreate(obc);

    JSObjectRef global(JSContextGetGlobalObject(JSGetContext()));

    name_ = JSStringCreateWithUTF8CString("name");
    message_ = JSStringCreateWithUTF8CString("message");
    length_ = JSStringCreateWithUTF8CString("length");

    JSStringRef name(JSStringCreateWithUTF8CString("Array"));
    JSValueRef exception(NULL);
    JSValueRef value(JSObjectGetProperty(JSGetContext(), global, name, &exception));
    CYThrow(context_, exception);
    JSStringRelease(name);
    Array_ = JSValueToObject(JSGetContext(), value, &exception);
    CYThrow(context_, exception);

    [pool release];
}
