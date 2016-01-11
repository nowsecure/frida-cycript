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

#include <cmath>

#include <map>
#include <set>

#include <dlfcn.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#include <mach/mach.h>
#endif

#include <objc/message.h>
#include <objc/runtime.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/JSStringRefCF.h>
#endif

#include <Foundation/Foundation.h>

#include "Code.hpp"
#include "Decode.hpp"
#include "Error.hpp"
#include "Functor.hpp"
#include "JavaScript.hpp"
#include "String.hpp"
#include "Execute.hpp"

#include "ObjectiveC/Internal.hpp"
#include "ObjectiveC/Syntax.hpp"

#define CYObjectiveTry_ { \
    try
#define CYObjectiveTry { \
    JSContextRef context(context_); \
    try
#define CYObjectiveCatch \
    catch (const CYException &error) { \
        @throw CYCastNSObject(NULL, context, error.CastJSValue(context, "Error")); \
    } \
}

#define CYPoolTry { \
    id _saved(nil); \
    NSAutoreleasePool *_pool([[NSAutoreleasePool alloc] init]); \
    @try
#define CYPoolCatch(value) \
    @catch (NSException *error) { \
        _saved = [error retain]; \
        throw CYJSError(context, CYCastJSValue(context, error)); \
        return value; \
    } @finally { \
        [_pool release]; \
        if (_saved != nil) \
            [_saved autorelease]; \
    } \
}

#define CYSadTry { \
    @try
#define CYSadCatch(value) \
    @catch (NSException *error ) { \
        throw CYJSError(context, CYCastJSValue(context, error)); \
    } return value; \
}

#define _oassert(test) \
    if (!(test)) \
        @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:@"_assert(" #test ")" userInfo:nil];

@class NSBlock;

struct BlockLiteral {
    Class isa;
    int flags;
    int reserved;
    void (*invoke)(void *, ...);
    void *descriptor;
};

struct BlockDescriptor1 {
    unsigned long int reserved;
    unsigned long int size;
};

struct BlockDescriptor2 {
    void (*copy_helper)(BlockLiteral *dst, BlockLiteral *src);
    void (*dispose_helper)(BlockLiteral *src);
};

struct BlockDescriptor3 {
    const char *signature;
    const char *layout;
};

enum {
    BLOCK_DEALLOCATING = 0x0001,
    BLOCK_REFCOUNT_MASK = 0xfffe,
    BLOCK_NEEDS_FREE = 1 << 24,
    BLOCK_HAS_COPY_DISPOSE = 1 << 25,
    BLOCK_HAS_CTOR = 1 << 26,
    BLOCK_IS_GC = 1 << 27,
    BLOCK_IS_GLOBAL = 1 << 28,
    BLOCK_HAS_STRET = 1 << 29,
    BLOCK_HAS_SIGNATURE = 1 << 30,
};

static bool CYIsClass(id self) {
    return class_isMetaClass(object_getClass(self));
}

JSValueRef CYSendMessage(CYPool &pool, JSContextRef context, id self, Class super, SEL _cmd, size_t count, const JSValueRef arguments[], bool initialize);

/* Objective-C Pool Release {{{ */
void CYPoolRelease_(void *data) {
    id object(reinterpret_cast<id>(data));
    [object release];
}

id CYPoolRelease_(CYPool *pool, id object) {
    if (object == nil)
        return nil;
    else if (pool == NULL)
        return [object autorelease];
    else {
        pool->atexit(CYPoolRelease_);
        return object;
    }
}

template <typename Type_>
Type_ CYPoolRelease(CYPool *pool, Type_ object) {
    return (Type_) CYPoolRelease_(pool, (id) object);
}
/* }}} */
/* Objective-C Strings {{{ */
CYUTF8String CYPoolUTF8String(CYPool &pool, JSContextRef context, NSString *value) {
    size_t size([value maximumLengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
    char *string(new(pool) char[size + 1]);
    if (![value getCString:string maxLength:size encoding:NSUTF8StringEncoding])
        throw CYJSError(context, "[NSString getCString:maxLength:encoding:] == NO");
    return CYUTF8String(string, [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
}

const char *CYPoolCString(CYPool &pool, JSContextRef context, NSString *value) {
    CYUTF8String utf8(CYPoolUTF8String(pool, context, value));
    _assert(memchr(utf8.data, '\0', utf8.size) == NULL);
    return utf8.data;
}

#ifdef __clang__
JSStringRef CYCopyJSString(JSContextRef context, NSString *value) {
    return JSStringCreateWithCFString(reinterpret_cast<CFStringRef>(value));
}
#endif

JSStringRef CYCopyJSString(JSContextRef context, NSObject *value) {
    if (value == nil)
        return NULL;
    // XXX: this definition scares me; is anyone using this?!
    NSString *string([value description]);
#ifdef __clang__
    return CYCopyJSString(context, string);
#else
    CYPool pool;
    return CYCopyJSString(CYPoolUTF8String(pool, context, string));
#endif
}

NSString *CYCopyNSString(const CYUTF8String &value) {
#ifdef __APPLE__
    return (NSString *) CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(value.data), value.size, kCFStringEncodingUTF8, true);
#else
    return [[NSString alloc] initWithBytes:value.data length:value.size encoding:NSUTF8StringEncoding];
#endif
}

NSString *CYCopyNSString(JSContextRef context, JSStringRef value) {
#ifdef __APPLE__
    return (NSString *) JSStringCopyCFString(kCFAllocatorDefault, value);
#else
    CYPool pool;
    return CYCopyNSString(CYPoolUTF8String(pool, context, value));
#endif
}

NSString *CYCopyNSString(JSContextRef context, JSValueRef value) {
    return CYCopyNSString(context, CYJSString(context, value));
}

NSString *CYCastNSString(CYPool *pool, const CYUTF8String &value) {
    return CYPoolRelease(pool, CYCopyNSString(value));
}

NSString *CYCastNSString(CYPool *pool, SEL sel) {
    const char *name(sel_getName(sel));
    return CYPoolRelease(pool, CYCopyNSString(CYUTF8String(name, strlen(name))));
}

NSString *CYCastNSString(CYPool *pool, JSContextRef context, JSStringRef value) {
    return CYPoolRelease(pool, CYCopyNSString(context, value));
}

CYUTF8String CYCastUTF8String(NSString *value) {
    NSData *data([value dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO]);
    return CYUTF8String(reinterpret_cast<const char *>([data bytes]), [data length]);
}
/* }}} */

JSValueRef CYCastJSValue(JSContextRef context, NSObject *value);

void CYThrow(JSContextRef context, NSException *error, JSValueRef *exception) {
    if (exception == NULL)
        throw error;
    *exception = CYCastJSValue(context, error);
}

size_t CYGetIndex(NSString *value) {
    return CYGetIndex(CYCastUTF8String(value));
}

bool CYGetOffset(CYPool &pool, JSContextRef context, NSString *value, ssize_t &index) {
    return CYGetOffset(CYPoolCString(pool, context, value), index);
}

static JSClassRef ObjectiveC_Classes_;
static JSClassRef ObjectiveC_Constants_;
static JSClassRef ObjectiveC_Protocols_;

#ifdef __APPLE__
static JSClassRef ObjectiveC_Image_Classes_;
static JSClassRef ObjectiveC_Images_;
#endif

#ifdef __APPLE__
static Class __NSMallocBlock__;
static Class NSCFBoolean_;
static Class NSCFType_;
static Class NSGenericDeallocHandler_;
#else
static Class NSBoolNumber_;
#endif

static Class NSArray_;
static Class NSBlock_;
static Class NSDictionary_;
static Class NSNumber_;
static Class NSObject_;
static Class NSString_;
static Class NSZombie_;
static Class Object_;

static JSValueRef Instance_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception);

JSValueRef Messages::GetPrototype(JSContextRef context) const {
#ifdef __APPLE__
    if (value_ == NSCFBoolean_)
#else
    if (value_ == NSBoolNumber_)
#endif
        return CYGetCachedObject(context, CYJSString("BooleanInstance_prototype"));
    if (value_ == NSArray_)
        return CYGetCachedObject(context, CYJSString("ArrayInstance_prototype"));
    if (value_ == NSBlock_)
        return CYGetCachedObject(context, CYJSString("FunctionInstance_prototype"));
    if (value_ == NSNumber_)
        return CYGetCachedObject(context, CYJSString("NumberInstance_prototype"));
    if (value_ == NSDictionary_)
        return CYGetCachedObject(context, CYJSString("ObjectInstance_prototype"));
    if (value_ == NSString_)
        return CYGetCachedObject(context, CYJSString("StringInstance_prototype"));

    if (Class super = class_getSuperclass(value_))
        if (class_isMetaClass(value_) && !class_isMetaClass(super))
            return CYGetCachedObject(context, CYJSString("TypeInstance_prototype"));
        else
            return CYPrivate<Messages>::Cache(context, super);
    else
        return CYGetCachedObject(context, CYJSString("Instance_prototype"));
}

bool CYIsKindOfClass(id object, Class _class) {
    for (Class isa(object_getClass(object)); isa != NULL; isa = class_getSuperclass(isa))
        if (isa == _class)
            return true;
    return false;
}

JSValueRef Instance::GetPrototype(JSContextRef context) const {
    return CYPrivate<Messages>::Cache(context, object_getClass(value_));
}

Instance::Instance(id value, Flags flags) :
    value_(value),
    flags_(flags)
{
    if (IsPermanent());
    /*else if ([value retainCount] == NSUInteger(-1))
        flags_ |= Instance::Permanent;*/
    else
        value_ = [value_ retain];
}

Instance::~Instance() {
    if (!IsPermanent())
        [value_ release];
}

struct Message_privateData :
    cy::Functor
{
    static JSClassRef Class_;

    SEL sel_;

    Message_privateData(SEL sel, const char *type, IMP value) :
        cy::Functor(reinterpret_cast<void (*)()>(value), type),
        sel_(sel)
    {
    }

    virtual CYPropertyName *GetName(CYPool &pool) const;

    static JSObjectRef Make(JSContextRef context, SEL sel, const char *type, IMP value);
};

JSClassRef Message_privateData::Class_;

JSObjectRef CYMakeInstance(JSContextRef context, id object, Instance::Flags flags = Instance::None) {
    _assert(object != nil);

#ifdef __APPLE__
    JSWeakObjectMapRef weak(CYCastPointer<JSWeakObjectMapRef>(context, CYGetCachedValue(context, weak_s)));

    if (weak != NULL && &JSWeakObjectMapGet != NULL)
        if (JSObjectRef instance = JSWeakObjectMapGet(context, weak, object))
            return instance;
#endif

    JSObjectRef instance;
    if (false);
    else if (CYIsKindOfClass(object, NSBlock_))
        instance = CYPrivate<Block>::Make(context, object, flags);
    else if (CYIsClass(object))
        instance = CYPrivate<Constructor>::Make(context, object, flags);
    else
        instance = CYPrivate<Instance>::Make(context, object, flags);

#ifdef __APPLE__
    if (weak != NULL && &JSWeakObjectMapSet != NULL)
        JSWeakObjectMapSet(context, weak, object, instance);
#endif

    return instance;
}

@interface NSMethodSignature (Cycript)
- (NSString *) _typeString;
@end

@interface NSObject (Cycript)

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context;
- (JSType) cy$JSType;

- (JSValueRef) cy$toJSON:(NSString *)key inContext:(JSContextRef)context;
- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects;

- (bool) cy$hasProperty:(NSString *)name;
- (NSObject *) cy$getProperty:(NSString *)name;
- (JSValueRef) cy$getProperty:(NSString *)name inContext:(JSContextRef)context;
- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value;
- (bool) cy$deleteProperty:(NSString *)name;
- (void) cy$getPropertyNames:(JSPropertyNameAccumulatorRef)names inContext:(JSContextRef)context;

+ (bool) cy$hasImplicitProperties;

@end

@protocol Cycript
- (id) cy$box;
- (JSValueRef) cy$valueOfInContext:(JSContextRef)context;
@end

NSString *CYCastNSCYON(id value, bool objective, std::set<void *> &objects) {
    _assert(value != nil);

    Class _class(object_getClass(value));

    if (class_isMetaClass(_class)) {
        const char *name(class_getName(value));
        if (class_isMetaClass(value))
            return [NSString stringWithFormat:@"object_getClass(%s)", name];
        else
            return [NSString stringWithUTF8String:name];
    }

    if (_class == NSZombie_)
        return [NSString stringWithFormat:@"<_NSZombie_: %p>", value];

    SEL sel(@selector(cy$toCYON:inSet:));

    if (objc_method *toCYON = class_getInstanceMethod(_class, sel))
        return reinterpret_cast<NSString *(*)(id, SEL, bool, std::set<void *> &)>(method_getImplementation(toCYON))(value, sel, objective, objects);
    else if (objc_method *methodSignatureForSelector = class_getInstanceMethod(_class, @selector(methodSignatureForSelector:)))
        if (reinterpret_cast<NSMethodSignature *(*)(id, SEL, SEL)>(method_getImplementation(methodSignatureForSelector))(value, @selector(methodSignatureForSelector:), sel) != nil)
            return [value cy$toCYON:objective inSet:objects];

    return [NSString stringWithFormat:@"%@", value];
}

NSString *CYCastNSCYON(id value, bool objective, std::set<void *> *objects) {
    if (objects != NULL)
        return CYCastNSCYON(value, objective, *objects);
    else {
        std::set<void *> objects;
        return CYCastNSCYON(value, objective, objects);
    }
}

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

        for (char *token(pool_.strdup(attributes)), *next; token != NULL; token = next) {
            if ((next = strchr(token, ',')) != NULL)
                *next++ = '\0';
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
            getter_ = pool_.strdup(name);
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

@interface CYWebUndefined : NSObject {
}

+ (CYWebUndefined *) undefined;

@end

@implementation CYWebUndefined

+ (CYWebUndefined *) undefined {
    static CYWebUndefined *instance_([[CYWebUndefined alloc] init]);
    return instance_;
}

@end

#define WebUndefined CYWebUndefined

/* Bridge: CYJSObject {{{ */
@interface CYJSObject : NSMutableDictionary {
    JSObjectRef object_;
    JSGlobalContextRef context_;
}

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context;

- (NSUInteger) count;
- (id) objectForKey:(id)key;
- (NSEnumerator *) keyEnumerator;
- (void) setObject:(id)object forKey:(id)key;
- (void) removeObjectForKey:(id)key;

@end
/* }}} */
/* Bridge: CYJSArray {{{ */
@interface CYJSArray : NSMutableArray {
    JSObjectRef object_;
    JSGlobalContextRef context_;
}

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context;

- (NSUInteger) count;
- (id) objectAtIndex:(NSUInteger)index;

- (void) addObject:(id)anObject;
- (void) insertObject:(id)anObject atIndex:(NSUInteger)index;
- (void) removeLastObject;
- (void) removeObjectAtIndex:(NSUInteger)index;
- (void) replaceObjectAtIndex:(NSUInteger)index withObject:(id)anObject;

@end
/* }}} */

_finline bool CYJSValueIsNSObject(JSContextRef context, JSValueRef value) {
    return JSValueIsObjectOfClass(context, value, CYPrivate<Instance>::Class_);
}

_finline bool CYJSValueIsInstanceOfCachedConstructor(JSContextRef context, JSValueRef value, JSStringRef cache) {
    return _jsccall(JSValueIsInstanceOfConstructor, context, value, CYGetCachedObject(context, cache));
}

#ifdef __APPLE__
struct CYBlockDescriptor {
    struct {
        BlockDescriptor1 one_;
        BlockDescriptor2 two_;
        BlockDescriptor3 three_;
    } d_;

    Closure_privateData *internal_;
};

void CYDisposeBlock(BlockLiteral *literal) {
    delete reinterpret_cast<CYBlockDescriptor *>(literal->descriptor)->internal_;
}

static JSValueRef BlockAdapter_(JSContextRef context, size_t count, JSValueRef values[], JSObjectRef function) {
    JSObjectRef _this(CYCastJSObject(context, values[0]));
    return CYCallAsFunction(context, function, _this, count - 1, values + 1);
}

NSBlock *CYMakeBlock(JSContextRef context, JSObjectRef function, sig::Signature &signature) {
    _assert(__NSMallocBlock__ != Nil);
    BlockLiteral *literal(reinterpret_cast<BlockLiteral *>(malloc(sizeof(BlockLiteral))));

    CYBlockDescriptor *descriptor(new CYBlockDescriptor);
    memset(&descriptor->d_, 0, sizeof(descriptor->d_));

    descriptor->internal_ = CYMakeFunctor_(context, function, signature, &BlockAdapter_);
    literal->invoke = reinterpret_cast<void (*)(void *, ...)>(descriptor->internal_->value_);

    literal->isa = __NSMallocBlock__;
    literal->flags = BLOCK_HAS_SIGNATURE | BLOCK_HAS_COPY_DISPOSE | BLOCK_IS_GLOBAL;
    literal->reserved = 0;
    literal->descriptor = descriptor;

    descriptor->d_.one_.size = sizeof(descriptor->d_);
    descriptor->d_.two_.dispose_helper = &CYDisposeBlock;
    descriptor->d_.three_.signature = sig::Unparse(*descriptor->internal_->pool_, &signature);

    return reinterpret_cast<NSBlock *>(literal);
}
#endif

NSObject *CYCastNSObject(CYPool *pool, JSContextRef context, JSObjectRef object) {
    if (CYJSValueIsNSObject(context, object)) {
        Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
        return internal->value_;
    }

    if (NSObject *pointer = CYCastPointerEx<NSObject *>(context, object))
        return pointer;

    bool array(CYJSValueIsInstanceOfCachedConstructor(context, object, Array_s));
    id value(array ? [CYJSArray alloc] : [CYJSObject alloc]);
    return CYPoolRelease(pool, [value initWithJSObject:object inContext:context]);
}

NSNumber *CYCopyNSNumber(JSContextRef context, JSValueRef value) {
    return [[NSNumber alloc] initWithDouble:CYCastDouble(context, value)];
}

#ifndef __APPLE__
@interface NSBoolNumber : NSNumber {
}
@end
#endif

id CYNSObject(CYPool *pool, JSContextRef context, JSValueRef value, bool cast) {
    id object;
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
#ifdef __APPLE__
            object = (id) (CYCastBool(context, value) ? kCFBooleanTrue : kCFBooleanFalse);
            copy = false;
#else
            object = [[NSBoolNumber alloc] initWithBool:CYCastBool(context, value)];
            copy = true;
#endif
        break;

        case kJSTypeNumber:
            object = CYCopyNSNumber(context, value);
            copy = true;
        break;

        case kJSTypeString:
            object = CYCopyNSString(context, value);
            copy = true;
        break;

        case kJSTypeObject:
            // XXX: this might could be more efficient
            object = CYCastNSObject(pool, context, (JSObjectRef) value);
            copy = false;
        break;

        default:
            throw CYJSError(context, "JSValueGetType() == 0x%x", type);
        break;
    }

    if (cast != copy)
        return object;
    else if (copy)
        return CYPoolRelease(pool, object);
    else
        return [object retain];
}

NSObject *CYCastNSObject(CYPool *pool, JSContextRef context, JSValueRef value) {
    return CYNSObject(pool, context, value, true);
}

NSObject *CYCopyNSObject(CYPool &pool, JSContextRef context, JSValueRef value) {
    return CYNSObject(&pool, context, value, false);
}

/* Bridge: NSArray {{{ */
@implementation NSArray (Cycript)

- (id) cy$box {
    return [[self mutableCopy] autorelease];
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    _oassert(objects.insert(self).second);

    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"@["];

    bool comma(false);
#ifdef __clang__
    for (id object in self) {
#else
    for (size_t index(0), count([self count]); index != count; ++index) {
        id object([self objectAtIndex:index]);
#endif
        if (comma)
            [json appendString:@","];
        else
            comma = true;
        if (object != nil && [object cy$JSType] != kJSTypeUndefined)
            [json appendString:CYCastNSCYON(object, true, objects)];
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

    size_t index(CYGetIndex(name));
    if (index == _not(size_t) || index >= [self count])
        return [super cy$hasProperty:name];
    else
        return true;
}

- (NSObject *) cy$getProperty:(NSString *)name {
    size_t index(CYGetIndex(name));
    if (index == _not(size_t) || index >= [self count])
        return [super cy$getProperty:name];
    else
        return [self objectAtIndex:index];
}

- (JSValueRef) cy$getProperty:(NSString *)name inContext:(JSContextRef)context {
    CYObjectiveTry_ {
        if ([name isEqualToString:@"length"])
            return CYCastJSValue(context, [self count]);
    } CYObjectiveCatch

    return [super cy$getProperty:name inContext:context];
}

- (void) cy$getPropertyNames:(JSPropertyNameAccumulatorRef)names inContext:(JSContextRef)context {
    [super cy$getPropertyNames:names inContext:context];

    for (size_t index(0), count([self count]); index != count; ++index) {
        id object([self objectAtIndex:index]);
        if (object == nil || [object cy$JSType] != kJSTypeUndefined) {
            char name[32];
            sprintf(name, "%zu", index);
            JSPropertyNameAccumulatorAddName(names, CYJSString(name));
        }
    }
}

+ (bool) cy$hasImplicitProperties {
    return false;
}

@end
/* }}} */
/* Bridge: NSBlock {{{ */
#ifdef __APPLE__
@interface NSBlock : NSObject
- (void) invoke;
@end

static const char *CYBlockEncoding(NSBlock *self);
static bool CYBlockSignature(CYPool &pool, NSBlock *self, sig::Signature &signature);

@implementation NSBlock (Cycript)

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    CYLocalPool pool;

    sig::Block type;
    if (!CYBlockSignature(pool, self, type.signature))
        return [super cy$toCYON:objective inSet:objects];
    _oassert(objects.insert(self).second);

    CYType *typed((new(pool) CYTypeExpression(CYDecodeType(pool, &type)))->typed_);
    CYTypeModifier *&modifier(CYGetLast(typed->modifier_));
    CYTypeBlockWith *with(dynamic_cast<CYTypeBlockWith *>(modifier));
    _assert(with != NULL);
    CYObjCBlock *block(new(pool) CYObjCBlock(typed, with->parameters_, NULL));
    modifier = NULL;

    std::ostringstream str;
    CYOptions options;
    CYOutput out(*str.rdbuf(), options);
    block->Output(out, CYNoFlags);

    std::string value(str.str());
    return CYCastNSString(NULL, CYUTF8String(value.c_str(), value.size()));
}

@end
#endif
/* }}} */
/* Bridge: NSBoolNumber {{{ */
#ifndef __APPLE__
@implementation NSBoolNumber (Cycript)

- (JSType) cy$JSType {
    return kJSTypeBoolean;
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    NSString *value([self boolValue] ? @"true" : @"false");
    return objective ? value : [NSString stringWithFormat:@"@%@", value];
}

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context { CYObjectiveTry_ {
    return CYCastJSValue(context, (bool) [self boolValue]);
} CYObjectiveCatch }

@end
#endif
/* }}} */
/* Bridge: NSDictionary {{{ */
@implementation NSDictionary (Cycript)

- (id) cy$box {
    return [[self mutableCopy] autorelease];
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    _oassert(objects.insert(self).second);

    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"@{"];

    bool comma(false);
#ifdef __clang__
    for (NSObject *key in self) {
#else
    NSEnumerator *keys([self keyEnumerator]);
    while (NSObject *key = [keys nextObject]) {
#endif
        if (comma)
            [json appendString:@","];
        else
            comma = true;
        [json appendString:CYCastNSCYON(key, true, objects)];
        [json appendString:@":"];
        NSObject *object([self objectForKey:key]);
        [json appendString:CYCastNSCYON(object, true, objects)];
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

- (void) cy$getPropertyNames:(JSPropertyNameAccumulatorRef)names inContext:(JSContextRef)context {
    [super cy$getPropertyNames:names inContext:context];

#ifdef __clang__
    for (NSObject *key in self) {
#else
    NSEnumerator *keys([self keyEnumerator]);
    while (NSObject *key = [keys nextObject]) {
#endif
        JSPropertyNameAccumulatorAddName(names, CYJSString(context, key));
    }
}

+ (bool) cy$hasImplicitProperties {
    return false;
}

@end
/* }}} */
/* Bridge: NSMutableArray {{{ */
@implementation NSMutableArray (Cycript)

- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value {
    if ([name isEqualToString:@"length"]) {
        // XXX: is this not intelligent?
        NSNumber *number(reinterpret_cast<NSNumber *>(value));
        NSUInteger size([number unsignedIntegerValue]);
        NSUInteger count([self count]);
        if (size < count)
            [self removeObjectsInRange:NSMakeRange(size, count - size)];
        else if (size != count) {
            WebUndefined *undefined([WebUndefined undefined]);
            for (size_t i(count); i != size; ++i)
                [self addObject:undefined];
        }
        return true;
    }

    size_t index(CYGetIndex(name));
    if (index == _not(size_t))
        return [super cy$setProperty:name to:value];

    id object(value ?: [NSNull null]);

    size_t count([self count]);
    if (index < count)
        [self replaceObjectAtIndex:index withObject:object];
    else {
        if (index != count) {
            WebUndefined *undefined([WebUndefined undefined]);
            for (size_t i(count); i != index; ++i)
                [self addObject:undefined];
        }

        [self addObject:object];
    }

    return true;
}

- (bool) cy$deleteProperty:(NSString *)name {
    size_t index(CYGetIndex(name));
    if (index == _not(size_t) || index >= [self count])
        return [super cy$deleteProperty:name];
    [self replaceObjectAtIndex:index withObject:[WebUndefined undefined]];
    return true;
}

@end
/* }}} */
/* Bridge: NSMutableDictionary {{{ */
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
/* }}} */
/* Bridge: NSNumber {{{ */
@implementation NSNumber (Cycript)

- (JSType) cy$JSType {
#ifdef __APPLE__
    // XXX: this just seems stupid
    if ([self class] == NSCFBoolean_)
        return kJSTypeBoolean;
#endif
    return kJSTypeNumber;
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    NSString *value([self cy$JSType] != kJSTypeBoolean ? [self stringValue] : [self boolValue] ? @"true" : @"false");
    return objective ? value : [NSString stringWithFormat:@"@%@", value];
}

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context { CYObjectiveTry_ {
    return [self cy$JSType] != kJSTypeBoolean ? CYCastJSValue(context, [self doubleValue]) : CYCastJSValue(context, static_cast<bool>([self boolValue]));
} CYObjectiveCatch }

@end
/* }}} */
/* Bridge: NSNull {{{ */
@implementation NSNull (Cycript)

- (JSType) cy$JSType {
    return kJSTypeNull;
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    NSString *value(@"null");
    return objective ? value : [NSString stringWithFormat:@"@%@", value];
}

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context { CYObjectiveTry_ {
    return CYJSNull(context);
} CYObjectiveCatch }

@end
/* }}} */
/* Bridge: NSObject {{{ */
@implementation NSObject (Cycript)

- (id) cy$box {
    return self;
}

- (JSValueRef) cy$toJSON:(NSString *)key inContext:(JSContextRef)context {
    return [self cy$valueOfInContext:context];
}

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context { CYObjectiveTry_ {
    return NULL;
} CYObjectiveCatch }

- (JSType) cy$JSType {
    return kJSTypeObject;
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    return [@"#" stringByAppendingString:[[self description] cy$toCYON:true inSet:objects]];
}

- (bool) cy$hasProperty:(NSString *)name {
    return false;
}

- (NSObject *) cy$getProperty:(NSString *)name {
    return nil;
}

- (JSValueRef) cy$getProperty:(NSString *)name inContext:(JSContextRef)context { CYObjectiveTry_ {
    if (NSObject *value = [self cy$getProperty:name])
        return CYCastJSValue(context, value);
    return NULL;
} CYObjectiveCatch }

- (bool) cy$setProperty:(NSString *)name to:(NSObject *)value {
    return false;
}

- (bool) cy$deleteProperty:(NSString *)name {
    return false;
}

- (void) cy$getPropertyNames:(JSPropertyNameAccumulatorRef)names inContext:(JSContextRef)context {
}

+ (bool) cy$hasImplicitProperties {
    return true;
}

@end
/* }}} */
/* Bridge: NSOrderedSet {{{ */
#ifdef __APPLE__
@implementation NSOrderedSet (Cycript)

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    _oassert(objects.insert(self).second);

    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"[NSOrderedSet orderedSetWithArray:"];
    [json appendString:CYCastNSCYON([self array], true, objects)];
    [json appendString:@"]]"];
    return json;
}

@end
#endif
/* }}} */
/* Bridge: NSProxy {{{ */
@implementation NSProxy (Cycript)

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    return [[self description] cy$toCYON:objective inSet:objects];
}

@end
/* }}} */
/* Bridge: NSSet {{{ */
@implementation NSSet (Cycript)

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    _oassert(objects.insert(self).second);

    NSMutableString *json([[[NSMutableString alloc] init] autorelease]);
    [json appendString:@"[NSSet setWithArray:"];
    [json appendString:CYCastNSCYON([self allObjects], true, objects)];
    [json appendString:@"]]"];
    return json;
}

@end
/* }}} */
/* Bridge: NSString {{{ */
@implementation NSString (Cycript)

- (id) cy$box {
    return [[self copy] autorelease];
}

- (JSType) cy$JSType {
    return kJSTypeString;
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    std::ostringstream str;
    if (!objective)
        str << '@';
    CYUTF8String string(CYCastUTF8String(self));
    CYStringify(str, string.data, string.size, CYStringifyModeNative);
    std::string value(str.str());
    return CYCastNSString(NULL, CYUTF8String(value.c_str(), value.size()));
}

- (bool) cy$hasProperty:(NSString *)name {
    size_t index(CYGetIndex(name));
    if (index == _not(size_t) || index >= [self length])
        return [super cy$hasProperty:name];
    else
        return true;
}

- (NSObject *) cy$getProperty:(NSString *)name {
    size_t index(CYGetIndex(name));
    if (index == _not(size_t) || index >= [self length])
        return [super cy$getProperty:name];
    else
        return [self substringWithRange:NSMakeRange(index, 1)];
}

- (void) cy$getPropertyNames:(JSPropertyNameAccumulatorRef)names inContext:(JSContextRef)context {
    [super cy$getPropertyNames:names inContext:context];

    for (size_t index(0), length([self length]); index != length; ++index) {
        char name[32];
        sprintf(name, "%zu", index);
        JSPropertyNameAccumulatorAddName(names, CYJSString(name));
    }
}

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context { CYObjectiveTry_ {
    return CYCastJSValue(context, CYJSString(context, self));
} CYObjectiveCatch }

@end
/* }}} */
/* Bridge: WebUndefined {{{ */
@implementation WebUndefined (Cycript)

- (JSType) cy$JSType {
    return kJSTypeUndefined;
}

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects {
    NSString *value(@"undefined");
    return value; // XXX: maybe use the below code, adding @undefined?
    //return objective ? value : [NSString stringWithFormat:@"@%@", value];
}

- (JSValueRef) cy$valueOfInContext:(JSContextRef)context { CYObjectiveTry_ {
    return CYJSUndefined(context);
} CYObjectiveCatch }

@end
/* }}} */

Class CYCastClass(CYPool &pool, JSContextRef context, JSValueRef value) {
    id self(CYCastNSObject(&pool, context, value));
    if (CYIsClass(self))
        return (Class) self;
    throw CYJSError(context, "got something that is not a Class");
    return NULL;
}

NSArray *CYCastNSArray(JSContextRef context, JSPropertyNameArrayRef names) {
    CYPool pool;
    size_t size(JSPropertyNameArrayGetCount(names));
    NSMutableArray *array([NSMutableArray arrayWithCapacity:size]);
    for (size_t index(0); index != size; ++index)
        [array addObject:CYCastNSString(&pool, context, JSPropertyNameArrayGetNameAtIndex(names, index))];
    return array;
}

JSValueRef CYCastJSValue(JSContextRef context, NSObject *value) {
    if (value == nil)
        return CYJSNull(context);
    return CYMakeInstance(context, value);
}

@implementation CYJSObject

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context { CYObjectiveTry_ {
    if ((self = [super init]) != nil) {
        object_ = object;
        context_ = CYGetJSContext(context);
        JSGlobalContextRetain(context_);
        JSValueProtect(context_, object_);
    } return self;
} CYObjectiveCatch }

- (void) dealloc { CYObjectiveTry {
    JSValueUnprotect(context_, object_);
    JSGlobalContextRelease(context_);
    [super dealloc];
} CYObjectiveCatch }

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects { CYObjectiveTry {
    CYPool pool;
    const char *cyon(CYPoolCCYON(pool, context, object_, objects));
    if (cyon == NULL)
        return [super cy$toCYON:objective inSet:objects];
    else
        return [NSString stringWithUTF8String:cyon];
} CYObjectiveCatch }

- (NSUInteger) count { CYObjectiveTry {
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context, object_));
    size_t size(JSPropertyNameArrayGetCount(names));
    JSPropertyNameArrayRelease(names);
    return size;
} CYObjectiveCatch }

- (id) objectForKey:(id)key { CYObjectiveTry {
    JSValueRef value(CYGetProperty(context, object_, CYJSString(context, (NSObject *) key)));
    if (JSValueIsUndefined(context, value))
        return nil;
    return CYCastNSObject(NULL, context, value) ?: [NSNull null];
} CYObjectiveCatch }

- (NSEnumerator *) keyEnumerator { CYObjectiveTry {
    JSPropertyNameArrayRef names(JSObjectCopyPropertyNames(context, object_));
    NSEnumerator *enumerator([CYCastNSArray(context, names) objectEnumerator]);
    JSPropertyNameArrayRelease(names);
    return enumerator;
} CYObjectiveCatch }

- (void) setObject:(id)object forKey:(id)key { CYObjectiveTry {
    CYSetProperty(context, object_, CYJSString(context, (NSObject *) key), CYCastJSValue(context, (NSString *) object));
} CYObjectiveCatch }

- (void) removeObjectForKey:(id)key { CYObjectiveTry {
    (void) _jsccall(JSObjectDeleteProperty, context, object_, CYJSString(context, (NSObject *) key));
} CYObjectiveCatch }

@end

@implementation CYJSArray

- (NSString *) cy$toCYON:(bool)objective inSet:(std::set<void *> &)objects { CYObjectiveTry {
    CYPool pool;
    return [NSString stringWithUTF8String:CYPoolCCYON(pool, context, object_, objects)];
} CYObjectiveCatch }

- (id) initWithJSObject:(JSObjectRef)object inContext:(JSContextRef)context { CYObjectiveTry_ {
    if ((self = [super init]) != nil) {
        object_ = object;
        context_ = CYGetJSContext(context);
        JSGlobalContextRetain(context_);
        JSValueProtect(context_, object_);
    } return self;
} CYObjectiveCatch }

- (void) dealloc { CYObjectiveTry {
    JSValueUnprotect(context_, object_);
    JSGlobalContextRelease(context_);
    [super dealloc];
} CYObjectiveCatch }

- (NSUInteger) count { CYObjectiveTry {
    return CYArrayLength(context, object_);
} CYObjectiveCatch }

- (id) objectAtIndex:(NSUInteger)index { CYObjectiveTry {
    size_t bounds([self count]);
    if (index >= bounds)
        @throw [NSException exceptionWithName:NSRangeException reason:[NSString stringWithFormat:@"*** -[CYJSArray objectAtIndex:]: index (%zu) beyond bounds (%zu)", static_cast<size_t>(index), bounds] userInfo:nil];
    JSValueRef value(_jsccall(JSObjectGetPropertyAtIndex, context, object_, index));
    return CYCastNSObject(NULL, context, value) ?: [NSNull null];
} CYObjectiveCatch }

- (void) addObject:(id)object { CYObjectiveTry {
    CYArrayPush(context, object_, CYCastJSValue(context, (NSObject *) object));
} CYObjectiveCatch }

- (void) insertObject:(id)object atIndex:(NSUInteger)index { CYObjectiveTry {
    size_t bounds([self count] + 1);
    if (index >= bounds)
        @throw [NSException exceptionWithName:NSRangeException reason:[NSString stringWithFormat:@"*** -[CYJSArray insertObject:atIndex:]: index (%zu) beyond bounds (%zu)", static_cast<size_t>(index), bounds] userInfo:nil];
    JSValueRef arguments[3];
    arguments[0] = CYCastJSValue(context, index);
    arguments[1] = CYCastJSValue(context, 0);
    arguments[2] = CYCastJSValue(context, (NSObject *) object);
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
    _jsccall(JSObjectCallAsFunction, context, CYCastJSObject(context, CYGetProperty(context, Array, splice_s)), object_, 3, arguments);
} CYObjectiveCatch }

- (void) removeLastObject { CYObjectiveTry {
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
    _jsccall(JSObjectCallAsFunction, context, CYCastJSObject(context, CYGetProperty(context, Array, pop_s)), object_, 0, NULL);
} CYObjectiveCatch }

- (void) removeObjectAtIndex:(NSUInteger)index { CYObjectiveTry {
    size_t bounds([self count]);
    if (index >= bounds)
        @throw [NSException exceptionWithName:NSRangeException reason:[NSString stringWithFormat:@"*** -[CYJSArray removeObjectAtIndex:]: index (%zu) beyond bounds (%zu)", static_cast<size_t>(index), bounds] userInfo:nil];
    JSValueRef arguments[2];
    arguments[0] = CYCastJSValue(context, index);
    arguments[1] = CYCastJSValue(context, 1);
    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array_prototype")));
    _jsccall(JSObjectCallAsFunction, context, CYCastJSObject(context, CYGetProperty(context, Array, splice_s)), object_, 2, arguments);
} CYObjectiveCatch }

- (void) replaceObjectAtIndex:(NSUInteger)index withObject:(id)object { CYObjectiveTry {
    size_t bounds([self count]);
    if (index >= bounds)
        @throw [NSException exceptionWithName:NSRangeException reason:[NSString stringWithFormat:@"*** -[CYJSArray replaceObjectAtIndex:withObject:]: index (%zu) beyond bounds (%zu)", static_cast<size_t>(index), bounds] userInfo:nil];
    CYSetProperty(context, object_, index, CYCastJSValue(context, (NSObject *) object));
} CYObjectiveCatch }

@end

// XXX: inherit from or replace with CYJSObject
@interface CYInternal : NSObject {
    JSGlobalContextRef context_;
    JSObjectRef object_;
}

@end

@implementation CYInternal

- (void) dealloc { CYObjectiveTry {
    JSValueUnprotect(context_, object_);
    JSGlobalContextRelease(context_);
    [super dealloc];
} CYObjectiveCatch }

- (id) initInContext:(JSContextRef)context { CYObjectiveTry_ {
    if ((self = [super init]) != nil) {
        context_ = CYGetJSContext(context);
        JSGlobalContextRetain(context_);
    } return self;
} CYObjectiveCatch }

- (bool) hasProperty:(JSStringRef)name inContext:(JSContextRef)context {
    if (object_ == NULL)
        return false;

    return JSObjectHasProperty(context, object_, name);
}

- (JSValueRef) getProperty:(JSStringRef)name inContext:(JSContextRef)context {
    if (object_ == NULL)
        return NULL;

    return CYGetProperty(context, object_, name);
}

- (void) setProperty:(JSStringRef)name toValue:(JSValueRef)value inContext:(JSContextRef)context {
    @synchronized (self) {
        if (object_ == NULL) {
            object_ = JSObjectMake(context, NULL, NULL);
            JSValueProtect(context, object_);
        }
    }

    CYSetProperty(context, object_, name, value);
}

+ (CYInternal *) get:(id)object {
#ifdef __APPLE__
    if (&objc_getAssociatedObject == NULL)
        return nil;

    @synchronized (object) {
        if (CYInternal *internal = objc_getAssociatedObject(object, @selector(cy$internal)))
            return internal;
    }
#endif

    return nil;
}

+ (CYInternal *) set:(id)object inContext:(JSContextRef)context {
#ifdef __APPLE__
    if (&objc_getAssociatedObject == NULL)
        return nil;

    @synchronized (object) {
        if (CYInternal *internal = objc_getAssociatedObject(object, @selector(cy$internal)))
            return internal;

        if (&objc_setAssociatedObject == NULL)
            return nil;

        CYInternal *internal([[[CYInternal alloc] initInContext:context] autorelease]);
        objc_setAssociatedObject(object, @selector(cy$internal), internal, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return internal;
    }
#endif

    return nil;
}

@end

static JSValueRef CYCastJSValue(JSContextRef context, SEL sel) {
    if (sel == NULL)
        return CYJSNull(context);
    return CYPrivate<Selector_privateData>::Make(context, sel);
}

static SEL CYCastSEL(JSContextRef context, JSValueRef value) {
    if (JSValueIsObjectOfClass(context, value, CYPrivate<Selector_privateData>::Class_)) {
        Selector_privateData *internal(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate((JSObjectRef) value)));
        return reinterpret_cast<SEL>(internal->value_);
    } else {
        CYPool pool;
        return sel_registerName(CYPoolCString(pool, context, value));
    }
}

void *CYObjectiveC_ExecuteStart(JSContextRef context) { CYSadTry {
    return (void *) [[NSAutoreleasePool alloc] init];
} CYSadCatch(NULL) }

void CYObjectiveC_ExecuteEnd(JSContextRef context, void *handle) { CYSadTry {
    return [(NSAutoreleasePool *) handle release];
} CYSadCatch() }

static void CYObjectiveC_CallFunction(CYPool &pool, JSContextRef context, ffi_cif *cif, void (*function)(), void *value, void **values) { CYSadTry {
    CYCallFunction(pool, context, cif, function, value, values);
} CYSadCatch() }

static NSBlock *CYCastNSBlock(CYPool &pool, JSContextRef context, JSValueRef value, const sig::Signature *signature) {
#ifdef __APPLE__
    if (JSValueIsNull(context, value))
        return nil;
    JSObjectRef object(CYCastJSObject(context, value));

    if (JSValueIsObjectOfClass(context, object, CYPrivate<Block>::Class_))
        return reinterpret_cast<Instance *>(JSObjectGetPrivate(object))->value_;

    if (JSValueIsObjectOfClass(context, object, CYPrivate<Instance>::Class_)) {
        _assert(reinterpret_cast<Instance *>(JSObjectGetPrivate(object))->value_ == nil);
        return nil;
    }

    _assert(JSObjectIsFunction(context, object));

    _assert(signature != NULL);
    _assert(signature->count != 0);

    sig::Signature modified;
    modified.count = signature->count + 1;
    modified.elements = new(pool) sig::Element[modified.count];

    modified.elements[0] = signature->elements[0];
    memcpy(modified.elements + 2, signature->elements + 1, sizeof(sig::Element) * (signature->count - 1));

    modified.elements[1].name = NULL;
    modified.elements[1].type = new(pool) sig::Object();
    modified.elements[1].offset = _not(size_t);

    return CYMakeBlock(context, object, modified);
#else
    _assert(false);
#endif
}

namespace sig {

void Block::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    // XXX: this function actually needs to handle null pools as it is an autorelease
    _assert(pool != NULL);
    *reinterpret_cast<id *>(data) = CYCastNSBlock(*pool, context, value, &signature);
}

// XXX: assigning to an indirect id * works for return values, but not for properties and fields
void Object::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    *reinterpret_cast<id *>(data) = CYCastNSObject(pool, context, value);
}

void Meta::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    *reinterpret_cast<id *>(data) = CYCastNSObject(pool, context, value);
}

void Selector::PoolFFI(CYPool *pool, JSContextRef context, ffi_type *ffi, void *data, JSValueRef value) const {
    *reinterpret_cast<SEL *>(data) = CYCastSEL(context, value);
}

JSValueRef Object::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    NSObject *value(*reinterpret_cast<NSObject **>(data));
    if (value == NULL)
        return CYJSNull(context);
    JSObjectRef object(CYMakeInstance(context, value));

    if (initialize) {
        Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));

        if (internal->IsUninitialized()) {
            internal->flags_ &= ~Instance::Uninitialized;
            if (internal->value_ == nil)
                internal->value_ = value;
            else
                _assert(internal->value_ == value);
        }

        [value release];
    }

    return object;
}

JSValueRef Meta::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    if (Class value = *reinterpret_cast<Class *>(data))
        return CYMakeInstance(context, value, Instance::Permanent);
    return CYJSNull(context);
}

JSValueRef Selector::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return CYCastJSValue(context, *reinterpret_cast<SEL *>(data));
}

JSValueRef Block::FromFFI(JSContextRef context, ffi_type *ffi, void *data, bool initialize, JSObjectRef owner) const {
    return CYCastJSValue(context, *reinterpret_cast<NSObject **>(data));
}

}

static bool CYImplements(id object, Class _class, SEL selector, bool devoid = false) {
    if (objc_method *method = class_getInstanceMethod(_class, selector)) {
        if (!devoid)
            return true;
        char type[16];
        method_getReturnType(method, type, sizeof(type));
        if (type[0] != 'v')
            return true;
    }

    // XXX: possibly use a more "awesome" check?
    return false;
}

static JSValueRef MessageAdapter_(JSContextRef context, size_t count, JSValueRef values[], JSObjectRef function) {
    JSObjectRef _this(CYCastJSObject(context, values[0]));
    return CYCallAsFunction(context, function, _this, count - 2, values + 2);
}

JSObjectRef Message_privateData::Make(JSContextRef context, SEL sel, const char *type, IMP value) {
    Message_privateData *internal(new Message_privateData(sel, type, value));
    JSObjectRef object(JSObjectMake(context, Message_privateData::Class_, internal));
    CYSetPrototype(context, object, CYGetCachedValue(context, CYJSString("Functor_prototype")));
    return object;
}

static IMP CYMakeMessage(JSContextRef context, JSValueRef value, const char *encoding) {
    JSObjectRef function(CYCastJSObject(context, value));
    CYPool pool;
    sig::Signature signature;
    sig::Parse(pool, &signature, encoding, &Structor_);
    Closure_privateData *internal(CYMakeFunctor_(context, function, signature, &MessageAdapter_));
    // XXX: see notes in Library.cpp about needing to leak
    return reinterpret_cast<IMP>(internal->value_);
}

static bool Messages_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    auto internal(CYPrivate<Messages>::Get(context, object));
    Class _class(internal->value_);

    CYPool pool;
    const char *name(CYPoolCString(pool, context, property));

    if (SEL sel = sel_getUid(name))
        if (class_getInstanceMethod(_class, sel) != NULL)
            return true;

    return false;
}

static JSValueRef Messages_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYPrivate<Messages>::Get(context, object));
    Class _class(internal->value_);

    CYPool pool;
    const char *name(CYPoolCString(pool, context, property));

    if (SEL sel = sel_getUid(name))
        if (objc_method *method = class_getInstanceMethod(_class, sel))
            return Message_privateData::Make(context, sel, method_getTypeEncoding(method), method_getImplementation(method));

    return NULL;
} CYCatch(NULL) }

static bool Messages_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    auto internal(CYPrivate<Messages>::Get(context, object));
    Class _class(internal->value_);

    CYPool pool;
    const char *name(CYPoolCString(pool, context, property));
    SEL sel(sel_registerName(name));

    const char *type;
    IMP imp;

    if (JSValueIsObjectOfClass(context, value, Message_privateData::Class_)) {
        Message_privateData *message(reinterpret_cast<Message_privateData *>(JSObjectGetPrivate((JSObjectRef) value)));
        type = sig::Unparse(pool, &message->signature_);
        imp = reinterpret_cast<IMP>(message->value_);
    } else if (objc_method *method = class_getInstanceMethod(_class, sel)) {
        type = method_getTypeEncoding(method);
        imp = CYMakeMessage(context, value, type);
    } else return false;

    objc_method *method(NULL);
    unsigned int size;
    objc_method **methods(class_copyMethodList(_class, &size));
    pool.atexit(free, methods);

    for (size_t i(0); i != size; ++i)
        if (sel_isEqual(method_getName(methods[i]), sel)) {
            method = methods[i];
            break;
        }

    if (method != NULL)
        method_setImplementation(method, imp);
    else
        class_addMethod(_class, sel, imp, type);

    return true;
} CYCatch(false) }

static JSValueRef Messages_complete_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count == 2) {
        if (!CYCastBool(context, arguments[1]))
            return CYObjectMakeArray(context, 0, NULL);
        count = 1;
    }

    _assert(count == 1);
    CYPool pool;
    CYUTF8String prefix(CYPoolUTF8String(pool, context, CYJSString(context, arguments[0])));

    auto internal(CYPrivate<Messages>::Get(context, _this));
    Class _class(internal->value_);

    unsigned int size;
    objc_method **data(class_copyMethodList(_class, &size));
    pool.atexit(free, data);

    JSObjectRef array(NULL); {
        CYArrayBuilder<1024> values(context, array);

        for (size_t i(0); i != size; ++i) {
            CYUTF8String name(sel_getName(method_getName(data[i])));
            if (CYStartsWith(name, prefix))
                values(CYCastJSValue(context, CYJSString(name)));
        }
    } return array;
} CYCatch(NULL) }

static bool CYHasImplicitProperties(JSContextRef context, Class _class) {
    if (!CYCastBool(context, CYGetCachedValue(context, CYJSString("cydget"))))
        if (class_getProperty(NSObject_, "description") != NULL)
            return false;
    // XXX: this is an evil hack to deal with NSProxy; fix elsewhere
    if (!CYImplements(_class, object_getClass(_class), @selector(cy$hasImplicitProperties)))
        return true;
    return [_class cy$hasImplicitProperties];
}

static objc_property_t CYFindProperty(CYPool &pool, Class _class, const char *name) {
    if (_class == Nil)
        return NULL;
    if (objc_property_t property = class_getProperty(_class, name))
        return property;
    return NULL;

    /* // XXX: I don't think any of this is required
    unsigned int count;
    Protocol **protocols(class_copyProtocolList(_class, &count));
    // XXX: just implement a scope guard already :/
    pool.atexit(free, protocols);

    for (unsigned int i(0); i != count; ++i)
        if (objc_property_t property = protocol_getProperty(protocols[i], name, true, true))
            return property;

    return CYFindProperty(pool, class_getSuperclass(_class), name); */
}

static bool Instance_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);

    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        return true;

    CYPool pool;
    NSString *name(CYCastNSString(&pool, context, property));

    if (CYInternal *internal = [CYInternal get:self])
        if ([internal hasProperty:property inContext:context])
            return true;

    Class _class(object_getClass(self));

    CYPoolTry {
        // XXX: this is an evil hack to deal with NSProxy; fix elsewhere
        if (CYImplements(self, _class, @selector(cy$hasProperty:)))
            if ([self cy$hasProperty:name])
                return true;
    } CYPoolCatch(false)

    const char *string(CYPoolCString(pool, context, name));

    if (CYFindProperty(pool, _class, string) != NULL)
        return true;

    if (CYHasImplicitProperties(context, _class))
        if (SEL sel = sel_getUid(string))
            if (CYImplements(self, _class, sel, true))
                return true;

    return false;
}

static JSValueRef Instance_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);

    if (JSStringIsEqualToUTF8CString(property, "$cyi"))
        return CYPrivate<Interior>::Make(context, self, context, object);

    CYPool pool;
    NSString *name(CYCastNSString(&pool, context, property));

    if (CYInternal *internal = [CYInternal get:self])
        if (JSValueRef value = [internal getProperty:property inContext:context])
            return value;

    CYPoolTry {
        if (JSValueRef value = [self cy$getProperty:name inContext:context])
            return value;
    } CYPoolCatch(NULL)

    const char *string(CYPoolCString(pool, context, name));
    Class _class(object_getClass(self));

    if (objc_property_t property = CYFindProperty(pool, _class, string)) {
        PropertyAttributes attributes(property);
        SEL sel(sel_registerName(attributes.Getter()));
        return CYSendMessage(pool, context, self, NULL, sel, 0, NULL, false);
    }

    if (CYHasImplicitProperties(context, _class))
        if (SEL sel = sel_getUid(string))
            if (CYImplements(self, _class, sel, true))
                return CYSendMessage(pool, context, self, NULL, sel, 0, NULL, false);

    return NULL;
} CYCatch(NULL) }

static bool Instance_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);

    CYPool pool;

    NSString *name(CYCastNSString(&pool, context, property));
    NSObject *data(CYCastNSObject(&pool, context, value));

    CYPoolTry {
        if ([self cy$setProperty:name to:data])
            return true;
    } CYPoolCatch(false)

    const char *string(CYPoolCString(pool, context, name));
    Class _class(object_getClass(self));

    if (objc_property_t property = CYFindProperty(pool, _class, string)) {
        PropertyAttributes attributes(property);
        if (const char *setter = attributes.Setter()) {
            SEL sel(sel_registerName(setter));
            JSValueRef arguments[1] = {value};
            CYSendMessage(pool, context, self, NULL, sel, 1, arguments, false);
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
            CYSendMessage(pool, context, self, NULL, sel, 1, arguments, false);
            return true;
        }

    if (CYInternal *internal = [CYInternal set:self inContext:context]) {
        [internal setProperty:property toValue:value inContext:context];
        return true;
    }

    return false;
} CYCatch(false) }

static bool Instance_deleteProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);

    CYPoolTry {
        NSString *name(CYCastNSString(NULL, context, property));
        return [self cy$deleteProperty:name];
    } CYPoolCatch(false)
} CYCatch(false) return /*XXX*/ false; }

static void CYForEachProperty(CYPool &pool, Class _class, const Functor<void (objc_method *, const char *)> &code) {
    for (; _class != Nil; _class = class_getSuperclass(_class)) {
        unsigned int size;
        objc_method **data(class_copyMethodList(_class, &size));
        pool.atexit(free, data);

        for (size_t i(0); i != size; ++i) {
            objc_method *method(data[i]);

            const char *name(sel_getName(method_getName(method)));
            if (strchr(name, ':') != NULL)
                continue;

            code(method, name);
        }
    }
}

static void Instance_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);

    CYPool pool;
    Class _class(object_getClass(self));

    for (Class current(_class); current != Nil; current = class_getSuperclass(current)) {
        unsigned int size;
        objc_property_t *data(class_copyPropertyList(current, &size));
        pool.atexit(free, data);

        for (size_t i(0); i != size; ++i)
            JSPropertyNameAccumulatorAddName(names, CYJSString(property_getName(data[i])));
    }

    if (CYHasImplicitProperties(context, _class))
        CYForEachProperty(pool, _class, fun([&](objc_method *method, const char *name) {
            JSPropertyNameAccumulatorAddName(names, CYJSString(name));
        }));

    CYPoolTry {
        // XXX: this is an evil hack to deal with NSProxy; fix elsewhere
        if (CYImplements(self, _class, @selector(cy$getPropertyNames:inContext:)))
            [self cy$getPropertyNames:names inContext:context];
    } CYPoolCatch()
}

static JSValueRef Instance_complete_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (!CYJSValueIsNSObject(context, _this))
        return CYObjectMakeArray(context, 0, NULL);

    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    id self(internal->value_);

    _assert(count == 1 || count == 2);
    CYPool pool;
    Class _class(object_getClass(self));

    CYUTF8String prefix(CYPoolUTF8String(pool, context, CYJSString(context, arguments[0])));

    JSObjectRef array(NULL); {
        CYArrayBuilder<1024> values(context, array);

        CYForEachProperty(pool, _class, fun([&](objc_method *method, const char *name) {
            if (!CYStartsWith(name, prefix))
                return;
            const char *type(method_getTypeEncoding(method));
            if (type == NULL || *type == '\0' || *type == 'v')
                return;
            if (class_getProperty(_class, name) != NULL)
                return;
            values(CYCastJSValue(context, CYJSString(pool.strcat(name, "()", NULL))));
        }));
    } return array;
} CYCatchObject() }

static JSObjectRef Constructor_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    auto internal(CYPrivate<Constructor>::Get(context, object));
    JSObjectRef value(CYMakeInstance(context, [internal->value_ alloc], Instance::Uninitialized));
    return value;
} CYCatchObject() }

static const char *CYBlockEncoding(NSBlock *self) {
    BlockLiteral *literal(reinterpret_cast<BlockLiteral *>(self));
    if ((literal->flags & BLOCK_HAS_SIGNATURE) == 0)
        return NULL;
    uint8_t *descriptor(reinterpret_cast<uint8_t *>(literal->descriptor));
    descriptor += sizeof(BlockDescriptor1);
    if ((literal->flags & BLOCK_HAS_COPY_DISPOSE) != 0)
        descriptor += sizeof(BlockDescriptor2);
    BlockDescriptor3 *descriptor3(reinterpret_cast<BlockDescriptor3 *>(descriptor));
    return descriptor3->signature;
}

static bool CYBlockSignature(CYPool &pool, NSBlock *self, sig::Signature &signature) {
    const char *encoding(CYBlockEncoding(self));
    if (encoding == NULL)
        return false;

    sig::Parse(pool, &signature, encoding, &Structor_);
    _assert(signature.count >= 2);

    _assert(dynamic_cast<sig::Object *>(signature.elements[1].type) != NULL);
    signature.elements[1] = signature.elements[0];

    ++signature.elements;
    --signature.count;

    return true;
}

static JSValueRef Block_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);

    if (const char *encoding = CYBlockEncoding(self)) {
        CYPool pool;

        void *setup[1];
        setup[0] = &self;

        sig::Signature signature;
        sig::Parse(pool, &signature, encoding, &Structor_);

        ffi_cif cif;
        sig::sig_ffi_cif(pool, 0, signature, &cif);

        BlockLiteral *literal(reinterpret_cast<BlockLiteral *>(self));
        void (*function)() = reinterpret_cast<void (*)()>(literal->invoke);
        return CYCallFunction(pool, context, 1, setup, count, arguments, false, false, signature, &cif, function);
    }

    if (count != 0)
        CYThrow("NSBlock without signature field passed arguments");

    CYPoolTry {
        [self invoke];
    } CYPoolCatch(NULL);

    return NULL;
} CYCatch(NULL) }

static bool Constructor_hasInstance(JSContextRef context, JSObjectRef constructor, JSValueRef instance, JSValueRef *exception) { CYTry {
    auto internal(CYPrivate<Constructor>::Get(context, constructor));
    Class _class(internal->value_);

    if (CYJSValueIsNSObject(context, instance)) {
        Instance *linternal(reinterpret_cast<Instance *>(JSObjectGetPrivate((JSObjectRef) instance)));
        // XXX: this isn't always safe
        return [linternal->value_ isKindOfClass:_class];
    }

    return false;
} CYCatch(false) }

static JSValueRef Instance_box_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count == 0)
        throw CYJSError(context, "incorrect number of arguments to Instance");
    CYPool pool;
    id value(CYCastNSObject(&pool, context, arguments[0]));
    if (value == nil)
        value = [NSNull null];
    return CYCastJSValue(context, [value cy$box]);
} CYCatch(NULL) }

static bool Interior_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    Interior *internal(reinterpret_cast<Interior *>(JSObjectGetPrivate(object)));
    CYPool pool;

    id self(internal->value_);
    const char *name(CYPoolCString(pool, context, property));

    if (object_getInstanceVariable(self, name, NULL) != NULL)
        return true;

    return false;
}

static void CYBitField(CYPool &pool, unsigned &length, unsigned &shift, id self, Ivar ivar, const char *encoding, unsigned offset) {
    length = CYCastDouble(encoding + 1);
    shift = 0;

    unsigned int size;
    objc_ivar **ivars(class_copyIvarList(object_getClass(self), &size));
    pool.atexit(free, ivars);

    for (size_t i(0); i != size; ++i)
        if (ivars[i] == ivar)
            break;
        else if (ivar_getOffset(ivars[i]) == offset) {
            const char *encoding(ivar_getTypeEncoding(ivars[i]));
            _assert(encoding != NULL);
            _assert(encoding[0] == 'b');
            shift += CYCastDouble(encoding + 1);
        }
}

static JSValueRef Interior_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Interior *internal(reinterpret_cast<Interior *>(JSObjectGetPrivate(object)));
    CYPool pool;

    id self(internal->value_);
    const char *name(CYPoolCString(pool, context, property));

    if (objc_ivar *ivar = object_getInstanceVariable(self, name, NULL)) {
        ptrdiff_t offset(ivar_getOffset(ivar));
        void *data(reinterpret_cast<uint8_t *>(self) + offset);

        const char *encoding(ivar_getTypeEncoding(ivar));
        _assert(encoding != NULL);
        _assert(encoding[0] != '\0');
        if (encoding[0] == 'b') {
            unsigned length, shift;
            CYBitField(pool, length, shift, self, ivar, encoding, offset);
            _assert(shift + length <= sizeof(uintptr_t) * 8);
            uintptr_t &field(*reinterpret_cast<uintptr_t *>(data));
            uintptr_t mask((1 << length) - 1);
            return CYCastJSValue(context, (field >> shift) & mask);
        } else {
#if defined(__APPLE__) && defined(__LP64__)
            // XXX: maybe do even more verifications here
            if (strcmp(name, "isa") == 0)
                return CYCastJSValue(context, object_getClass(self));
#endif

            auto type(new(pool) Type_privateData(encoding));
            return type->type_->FromFFI(context, type->GetFFI(), data);
        }
    }

    return NULL;
} CYCatch(NULL) }

static bool Interior_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    Interior *internal(reinterpret_cast<Interior *>(JSObjectGetPrivate(object)));
    CYPool pool;

    id self(internal->value_);
    const char *name(CYPoolCString(pool, context, property));

    if (objc_ivar *ivar = object_getInstanceVariable(self, name, NULL)) {
        ptrdiff_t offset(ivar_getOffset(ivar));
        void *data(reinterpret_cast<uint8_t *>(self) + offset);

        const char *encoding(ivar_getTypeEncoding(ivar));
        _assert(encoding != NULL);
        if (encoding[0] == 'b') {
            unsigned length, shift;
            CYBitField(pool, length, shift, self, ivar, encoding, offset);
            _assert(shift + length <= sizeof(uintptr_t) * 8);
            uintptr_t &field(*reinterpret_cast<uintptr_t *>(data));
            uintptr_t mask((1 << length) - 1);
            field = field & ~(mask << shift) | (uintptr_t(CYCastDouble(context, value)) & mask) << shift;
        } else {
            auto type(new(pool) Type_privateData(ivar_getTypeEncoding(ivar)));
            type->type_->PoolFFI(&pool, context, type->GetFFI(), reinterpret_cast<uint8_t *>(self) + ivar_getOffset(ivar), value);
            return true;
        }
    }

    return false;
} CYCatch(false) }

static void Interior_getPropertyNames_(CYPool &pool, Class _class, JSPropertyNameAccumulatorRef names) {
    if (Class super = class_getSuperclass(_class))
        Interior_getPropertyNames_(pool, super, names);

    unsigned int size;
    objc_ivar **data(class_copyIvarList(_class, &size));
    pool.atexit(free, data);

    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(ivar_getName(data[i])));
}

static void Interior_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    Interior *internal(reinterpret_cast<Interior *>(JSObjectGetPrivate(object)));
    CYPool pool;

    id self(internal->value_);
    Class _class(object_getClass(self));

    Interior_getPropertyNames_(pool, _class, names);
}

static JSValueRef Interior_callAsFunction_$cya(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Interior *internal(reinterpret_cast<Interior *>(JSObjectGetPrivate(object)));
    return internal->owner_;
} CYCatch(NULL) }

static bool ObjectiveC_Classes_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    CYPool pool;
    return objc_getClass(CYPoolCString(pool, context, property)) != Nil;
}

static JSValueRef ObjectiveC_Classes_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    NSString *name(CYCastNSString(&pool, context, property));
    if (Class _class = NSClassFromString(name))
        return CYMakeInstance(context, _class, Instance::Permanent);
    return NULL;
} CYCatch(NULL) }

static Class *CYCopyClassList(size_t &size) {
    size = objc_getClassList(NULL, 0);
    Class *data(reinterpret_cast<Class *>(malloc(sizeof(Class) * size)));

    for (;;) {
        size_t writ(objc_getClassList(data, size));
        if (writ <= size) {
            size = writ;
            return data;
        }

        Class *copy(reinterpret_cast<Class *>(realloc(data, sizeof(Class) * writ)));
        if (copy == NULL) {
            free(data);
            return NULL;
        }

        data = copy;
        size = writ;
    }
}

static void ObjectiveC_Classes_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYPool pool;

    size_t size;
    if (Class *data = CYCopyClassList(size)) {
        pool.atexit(free, data);
        for (size_t i(0); i != size; ++i)
            JSPropertyNameAccumulatorAddName(names, CYJSString(class_getName(data[i])));
    }
}

#ifdef __APPLE__
static JSValueRef ObjectiveC_Image_Classes_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    const char *internal(reinterpret_cast<const char *>(JSObjectGetPrivate(object)));

    CYPool pool;
    const char *name(CYPoolCString(pool, context, property));

    unsigned int size;
    const char **data(objc_copyClassNamesForImage(internal, &size));
    pool.atexit(free, data);

    JSValueRef value;
    for (size_t i(0); i != size; ++i)
        if (strcmp(name, data[i]) == 0) {
            if (Class _class = objc_getClass(name))
                return CYMakeInstance(context, _class, Instance::Permanent);
            else
                return NULL;
        }

    return NULL;
} CYCatch(NULL) }

static void ObjectiveC_Image_Classes_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    const char *internal(reinterpret_cast<const char *>(JSObjectGetPrivate(object)));
    CYPool pool;

    unsigned int size;
    const char **data(objc_copyClassNamesForImage(internal, &size));
    pool.atexit(free, data);

    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(data[i]));
}

static JSValueRef ObjectiveC_Images_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    CYUTF8String name(CYPoolUTF8String(pool, context, property));

    unsigned int size;
    const char **data(objc_copyImageNames(&size));
    pool.atexit(free, data);

    for (size_t i(0); i != size; ++i)
        if (name == data[i]) {
            JSObjectRef value(JSObjectMake(context, NULL, NULL));
            CYSetProperty(context, value, CYJSString("classes"), JSObjectMake(context, ObjectiveC_Image_Classes_, const_cast<char *>(data[i])));
            return value;
        }

    return NULL;
} CYCatch(NULL) }

static void ObjectiveC_Images_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYPool pool;

    unsigned int size;
    const char **data(objc_copyImageNames(&size));
    pool.atexit(free, data);

    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(data[i]));
}
#endif

static JSValueRef ObjectiveC_Protocols_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    const char *name(CYPoolCString(pool, context, property));
    if (Protocol *protocol = objc_getProtocol(name))
        return CYMakeInstance(context, protocol, Instance::Permanent);
    return NULL;
} CYCatch(NULL) }

static void ObjectiveC_Protocols_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYPool pool;

    unsigned int size;
    Protocol **data(objc_copyProtocolList(&size));
    pool.atexit(free, data);

    for (size_t i(0); i != size; ++i)
        JSPropertyNameAccumulatorAddName(names, CYJSString(protocol_getName(data[i])));
}

static JSValueRef ObjectiveC_Constants_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYPool pool;
    CYUTF8String name(CYPoolUTF8String(pool, context, property));
    if (name == "nil")
        return CYJSNull(context);
    return NULL;
} CYCatch(NULL) }

static void ObjectiveC_Constants_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    JSPropertyNameAccumulatorAddName(names, CYJSString("nil"));
}

#ifdef __APPLE__
static kern_return_t CYReadMemory(task_t task, vm_address_t address, vm_size_t size, void **data) {
    *data = reinterpret_cast<void *>(address);
    return KERN_SUCCESS;
}

struct CYChoice {
    std::set<Class> query_;
    JSContextRef context_;
    JSObjectRef results_;
};

struct CYObjectStruct {
    Class isa_;
};

static void choose_(task_t task, void *baton, unsigned type, vm_range_t *ranges, unsigned count) {
    CYChoice *choice(reinterpret_cast<CYChoice *>(baton));
    JSContextRef context(choice->context_);

    for (unsigned i(0); i != count; ++i) {
        vm_range_t &range(ranges[i]);
        void *data(reinterpret_cast<void *>(range.address));
        size_t size(range.size);

        if (size < sizeof(CYObjectStruct))
            continue;

        uintptr_t *pointers(reinterpret_cast<uintptr_t *>(data));
#if defined(__APPLE__) && defined(__LP64__)
        Class isa(reinterpret_cast<Class>(pointers[0] & 0x1fffffff8));
#else
        Class isa(reinterpret_cast<Class>(pointers[0]));
#endif

        std::set<Class>::const_iterator result(choice->query_.find(isa));
        if (result == choice->query_.end())
            continue;

        size_t needed(class_getInstanceSize(*result));
        // XXX: if (size < needed)

        size_t boundary(496);
#ifdef __LP64__
        boundary *= 2;
#endif
        if (needed <= boundary && (needed + 15) / 16 * 16 != size || needed > boundary && (needed + 511) / 512 * 512 != size)
            continue;
        CYArrayPush(context, choice->results_, CYCastJSValue(context, reinterpret_cast<id>(data)));
    }
}

static JSValueRef choose(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "choose() takes a class argument");

    CYGarbageCollect(context);

    CYPool pool;
    id _class(CYCastNSObject(&pool, context, arguments[0]));

    vm_address_t *zones(NULL);
    unsigned size(0);
    kern_return_t error(malloc_get_all_zones(0, &CYReadMemory, &zones, &size));
    _assert(error == KERN_SUCCESS);

    JSObjectRef Array(CYGetCachedObject(context, CYJSString("Array")));
    JSObjectRef results(_jsccall(JSObjectCallAsConstructor, context, Array, 0, NULL));

    CYChoice choice;
    choice.context_ = context;
    choice.results_ = results;

    size_t number;
    Class *classes(CYCopyClassList(number));
    _assert(classes != NULL);
    pool.atexit(free, classes);

    for (size_t i(0); i != number; ++i)
        for (Class current(classes[i]); current != Nil; current = class_getSuperclass(current))
            if (current == _class) {
                choice.query_.insert(classes[i]);
                break;
            }

    for (unsigned i(0); i != size; ++i) {
        const malloc_zone_t *zone(reinterpret_cast<const malloc_zone_t *>(zones[i]));
        if (zone == NULL || zone->introspect == NULL)
            continue;

        zone->introspect->enumerator(mach_task_self(), &choice, MALLOC_PTR_IN_USE_RANGE_TYPE, zones[i], &CYReadMemory, &choose_);
    }

    return results;
} CYCatch(NULL) }
#endif

#ifdef __APPLE__
#if defined(__i386__) || defined(__x86_64__)
#define OBJC_MAX_STRUCT_BY_VALUE 8
static int struct_forward_array[] = {
    0, 0, 0, 1, 0, 1, 1, 1, 0 };
#elif defined(__arm__)
#define OBJC_MAX_STRUCT_BY_VALUE 1
static int struct_forward_array[] = {
    0, 0 };
#elif defined(__arm64__)
#define CY_NO_STRET
#else
#error missing objc-runtime-info
#endif

#ifndef CY_NO_STRET
static bool stret(ffi_type *ffi_type) {
    return ffi_type->type == FFI_TYPE_STRUCT && (
        ffi_type->size > OBJC_MAX_STRUCT_BY_VALUE ||
        struct_forward_array[ffi_type->size] != 0
    );
}
#endif
#else
#define CY_NO_STRET
#endif

JSValueRef CYSendMessage(CYPool &pool, JSContextRef context, id self, Class _class, SEL _cmd, size_t count, const JSValueRef arguments[], bool initialize) {
    const char *type;

    if (_class == NULL)
        _class = object_getClass(self);

    IMP imp;

    if (objc_method *method = class_getInstanceMethod(_class, _cmd)) {
        imp = method_getImplementation(method);
        type = method_getTypeEncoding(method);
    } else {
        imp = NULL;

        CYPoolTry {
            if (NSMethodSignature *method = [self methodSignatureForSelector:_cmd])
                type = CYPoolCString(pool, context, [method _typeString]);
            else
                type = NULL;
        } CYPoolCatch(NULL)

        if (type == NULL)
            throw CYJSError(context, "unrecognized selector %s sent to object %p", sel_getName(_cmd), self);
    }

    void *setup[2];
    setup[0] = &self;
    setup[1] = &_cmd;

    sig::Signature signature;
    sig::Parse(pool, &signature, type, &Structor_);

    ffi_cif cif;
    sig::sig_ffi_cif(pool, 0, signature, &cif);

    if (imp == NULL) {
#ifndef CY_NO_STRET
        if (stret(cif.rtype))
            imp = class_getMethodImplementation_stret(_class, _cmd);
        else
#endif
            imp = class_getMethodImplementation(_class, _cmd);
    }

    void (*function)() = reinterpret_cast<void (*)()>(imp);
    return CYCallFunction(pool, context, 2, setup, count, arguments, initialize, true, signature, &cif, function);
}

static JSValueRef $objc_msgSend(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[]) {
    if (count < 2)
        throw CYJSError(context, "too few arguments to objc_msgSend");

    CYPool pool;

    bool uninitialized;

    id self;
    SEL _cmd;
    Class _class;

    if (JSValueIsObjectOfClass(context, arguments[0], CYPrivate<cy::Super>::Class_)) {
        cy::Super *internal(reinterpret_cast<cy::Super *>(JSObjectGetPrivate((JSObjectRef) arguments[0])));
        self = internal->value_;
        _class = internal->class_;;
        uninitialized = false;
    } else if (CYJSValueIsNSObject(context, arguments[0])) {
        Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate((JSObjectRef) arguments[0])));
        self = internal->value_;
        _class = nil;
        uninitialized = internal->IsUninitialized();
        if (uninitialized && [internal->value_ retainCount] != NSUInteger(-1))
            internal->value_ = nil;
    } else {
        self = CYCastNSObject(&pool, context, arguments[0]);
        _class = nil;
        uninitialized = false;
    }

    if (self == nil)
        return CYJSNull(context);

    _cmd = CYCastSEL(context, arguments[1]);

    return CYSendMessage(pool, context, self, _class, _cmd, count - 2, arguments + 2, uninitialized);
}

static JSValueRef $objc_msgSend(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    return $objc_msgSend(context, object, _this, count, arguments);
} CYCatch(NULL) }

static JSValueRef Selector_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    JSValueRef setup[count + 2];
    setup[0] = _this;
    setup[1] = object;
    memcpy(setup + 2, arguments, sizeof(JSValueRef) * count);
    return $objc_msgSend(context, NULL, NULL, count + 2, setup);
} CYCatch(NULL) }

static JSValueRef Message_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYPool pool;
    Message_privateData *internal(reinterpret_cast<Message_privateData *>(JSObjectGetPrivate(object)));

    // XXX: handle Instance::Uninitialized?
    id self(CYCastNSObject(&pool, context, _this));

    void *setup[2];
    setup[0] = &self;
    setup[1] = &internal->sel_;

    return CYCallFunction(pool, context, 2, setup, count, arguments, false, true, internal->signature_, &internal->cif_, internal->value_);
} CYCatch(NULL) }

CYPropertyName *Message_privateData::GetName(CYPool &pool) const {
    return new(pool) CYString(pool.strcat(":", sel_getName(sel_), NULL));
}

static JSObjectRef Super_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 2)
        throw CYJSError(context, "incorrect number of arguments to objc_super constructor");
    CYPool pool;
    id self(CYCastNSObject(&pool, context, arguments[0]));
    Class _class(CYCastClass(pool, context, arguments[1]));
    return CYPrivate<cy::Super>::Make(context, self, _class);
} CYCatchObject() }

static JSObjectRef Selector_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Selector constructor");
    CYPool pool;
    const char *name(CYPoolCString(pool, context, arguments[0]));
    return CYPrivate<Selector_privateData>::Make(context, sel_registerName(name));
} CYCatchObject() }

static JSObjectRef Instance_new(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Instance constructor");
    return CYMakeInstance(context, CYCastPointer<id>(context, arguments[0]));
} CYCatchObject() }

static JSValueRef Selector_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    return CYMakeType(context, sig::Selector());
} CYCatch(NULL) }

static JSValueRef Instance_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);
    return CYMakeType(context, sig::Object(class_getName(object_getClass(self))));
} CYCatch(NULL) }

static JSValueRef FunctionInstance_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    CYPool pool;
    sig::Block type;
    if (!CYBlockSignature(pool, internal->value_, type.signature))
        return CYJSNull(context);
    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSValueRef Constructor_getProperty_$cyt(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    return CYMakeType(context, sig::Meta());
} CYCatch(NULL) }

static JSValueRef Instance_getProperty_constructor(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    return CYMakeInstance(context, object_getClass(internal->value_), Instance::Permanent);
} CYCatch(NULL) }

static JSValueRef Constructor_getProperty_prototype(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(object)));
    id self(internal->value_);
    return CYPrivate<Messages>::Cache(context, self);
} CYCatch(NULL) }

static JSValueRef Instance_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    std::set<void *> *objects(CYCastObjects(context, _this, count, arguments));

    if (!CYJSValueIsNSObject(context, _this))
        return NULL;

    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, CYJSString(context, CYCastNSCYON(internal->value_, false, objects)));
} CYCatch(NULL) }

static JSValueRef Instance_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (!CYJSValueIsNSObject(context, _this))
        return NULL;

    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    id value(internal->value_);

    CYPoolTry {
        NSString *key;
        if (count == 0)
            key = nil;
        else
            key = CYCastNSString(NULL, context, CYJSString(context, arguments[0]));

        if (!CYImplements(value, object_getClass(value), @selector(cy$toJSON:inContext:)))
            return CYJSUndefined(context);
        else if (JSValueRef json = [value cy$toJSON:key inContext:context])
            return json;
        else
            return CYCastJSValue(context, CYJSString(context, [value description]));
    } CYPoolCatch(NULL)
} CYCatch(NULL) return /*XXX*/ NULL; }

static JSValueRef Instance_callAsFunction_valueOf(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (!CYJSValueIsNSObject(context, _this))
        return NULL;

    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    id value(internal->value_);
    _assert(value != nil);

    if (![value respondsToSelector:@selector(cy$valueOfInContext:)])
        return _this;

    if (JSValueRef result = [value cy$valueOfInContext:context])
        return result;

    return _this;
} CYCatch(NULL) return /*XXX*/ NULL; }

static JSValueRef Instance_callAsFunction_toPointer(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (!CYJSValueIsNSObject(context, _this))
        return NULL;
    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    // XXX: return CYMakePointer(context, internal->value_, sig::Object(class_getName(object_getClass(internal->value_))), NULL, object);
    // XXX: return CYMakePointer(context, internal->value_, sig::Meta(), NULL, object);
    return CYCastJSValue(context, reinterpret_cast<uintptr_t>(internal->value_));
} CYCatch(NULL) return /*XXX*/ NULL; }

static JSValueRef Instance_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (!CYJSValueIsNSObject(context, _this))
        return NULL;

    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    id value(internal->value_);

    CYPoolTry {
        // XXX: this seems like a stupid implementation; what if it crashes? why not use the CYONifier backend?
        return CYCastJSValue(context, CYJSString(context, [value description]));
    } CYPoolCatch(NULL)
} CYCatch(NULL) return /*XXX*/ NULL; }

static JSValueRef Class_callAsFunction_pointerTo(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (!CYJSValueIsNSObject(context, _this))
        return NULL;

    Instance *internal(reinterpret_cast<Instance *>(JSObjectGetPrivate(_this)));
    id value(internal->value_);

    if (!CYIsClass(value))
        CYThrow("non-Class object cannot be used as Type");

    sig::Object type(class_getName(value));
    return CYMakeType(context, type);
} CYCatch(NULL) return /*XXX*/ NULL; }

static JSValueRef Selector_callAsFunction_toString(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Selector_privateData *internal(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate(_this)));
    return CYCastJSValue(context, sel_getName(internal->value_));
} CYCatch(NULL) }

static JSValueRef Selector_callAsFunction_toJSON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) {
    return Selector_callAsFunction_toString(context, object, _this, count, arguments, exception);
}

static JSValueRef Selector_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    Selector_privateData *internal(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate(_this)));
    const char *name(sel_getName(internal->value_));

    CYPoolTry {
        NSString *string([NSString stringWithFormat:@"@selector(%s)", name]);
        return CYCastJSValue(context, CYJSString(context, string));
    } CYPoolCatch(NULL)
} CYCatch(NULL) return /*XXX*/ NULL; }

static JSValueRef Selector_callAsFunction_type(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    if (count != 1)
        throw CYJSError(context, "incorrect number of arguments to Selector.type");

    CYPool pool;
    Selector_privateData *internal(reinterpret_cast<Selector_privateData *>(JSObjectGetPrivate(_this)));
    SEL sel(internal->value_);

    Class _class(_require(CYCastClass(pool, context, arguments[0])));
    objc_method *method(_require(class_getInstanceMethod(_class, sel)));
    const char *encoding(method_getTypeEncoding(method));

    sig::Function type(false);
    sig::Parse(pool, &type.signature, encoding, &Structor_);
    return CYMakeType(context, type);
} CYCatch(NULL) }

static JSStaticValue Selector_staticValues[2] = {
    {"$cyt", &Selector_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticValue Instance_staticValues[3] = {
    {"$cyt", &Instance_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"constructor", &Instance_getProperty_constructor, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticValue Block_staticValues[3] = {
    {"$cyt", &FunctionInstance_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Instance_staticFunctions[7] = {
    {"cy$complete", &Instance_complete_callAsFunction, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Prototype_staticFunctions[6] = {
    {"toCYON", &Instance_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Instance_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"valueOf", &Instance_callAsFunction_valueOf, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toPointer", &Instance_callAsFunction_toPointer, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Instance_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Messages_staticFunctions[2] = {
    {"cy$complete", &Messages_complete_callAsFunction, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue Constructor_staticValues[3] = {
    {"$cyt", &Constructor_getProperty_$cyt, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"prototype", &Constructor_getProperty_prototype, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction Constructor_staticFunctions[2] = {
    {"pointerTo", &Class_callAsFunction_pointerTo, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Interior_staticFunctions[2] = {
    {"$cya", &Interior_callAsFunction_$cya, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction Selector_staticFunctions[5] = {
    {"toCYON", &Selector_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toJSON", &Selector_callAsFunction_toJSON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"toString", &Selector_callAsFunction_toString, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"type", &Selector_callAsFunction_type, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

#ifdef __APPLE__
JSValueRef NSCFType$cy$toJSON$inContext$(id self, SEL sel, JSValueRef key, JSContextRef context) { CYObjectiveTry_ {
    return CYCastJSValue(context, [(NSString *) CFCopyDescription((CFTypeRef) self) autorelease]);
} CYObjectiveCatch }
#endif

void CYObjectiveC_Initialize() { /*XXX*/ JSContextRef context(NULL); CYPoolTry {
    NSArray_ = objc_getClass("NSArray");
    NSBlock_ = objc_getClass("NSBlock");
    NSDictionary_ = objc_getClass("NSDictionary");
    NSNumber_ = objc_getClass("NSNumber");
    NSObject_ = objc_getClass("NSObject");
    NSString_ = objc_getClass("NSString");
    Object_ = objc_getClass("Object");

#ifdef __APPLE__
    __NSMallocBlock__ = objc_getClass("__NSMallocBlock__");

    // XXX: apparently, iOS now has both of these
    NSCFBoolean_ = objc_getClass("__NSCFBoolean");
    if (NSCFBoolean_ == nil)
        NSCFBoolean_ = objc_getClass("NSCFBoolean");

    NSCFType_ = objc_getClass("NSCFType");

    NSZombie_ = objc_getClass("_NSZombie_");
#else
    NSBoolNumber_ = objc_getClass("NSBoolNumber");
    NSZombie_ = objc_getClass("NSZombie");
#endif

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Messages";
    definition.staticFunctions = Messages_staticFunctions;
    definition.hasProperty = &Messages_hasProperty;
    definition.getProperty = &Messages_getProperty;
    definition.setProperty = &Messages_setProperty;
    CYPrivate<Messages>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Instance";
    definition.staticValues = Instance_staticValues;
    definition.staticFunctions = Instance_staticFunctions;
    definition.hasProperty = &Instance_hasProperty;
    definition.getProperty = &Instance_getProperty;
    definition.setProperty = &Instance_setProperty;
    definition.deleteProperty = &Instance_deleteProperty;
    definition.getPropertyNames = &Instance_getPropertyNames;
    definition.finalize = &CYFinalize;
    CYPrivate<Instance>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Block";
    definition.parentClass = CYPrivate<Instance>::Class_;
    definition.staticValues = Block_staticValues;
    definition.callAsFunction = &Block_callAsFunction;
    CYPrivate<Block>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Constructor";
    definition.parentClass = CYPrivate<Instance>::Class_;
    definition.staticValues = Constructor_staticValues;
    definition.staticFunctions = Constructor_staticFunctions;
    definition.hasInstance = &Constructor_hasInstance;
    definition.callAsConstructor = &Constructor_callAsConstructor;
    CYPrivate<Constructor>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Interior";
    definition.staticFunctions = Interior_staticFunctions;
    definition.hasProperty = &Interior_hasProperty;
    definition.getProperty = &Interior_getProperty;
    definition.setProperty = &Interior_setProperty;
    definition.getPropertyNames = &Interior_getPropertyNames;
    definition.finalize = &CYFinalize;
    CYPrivate<Interior>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Message";
    definition.parentClass = cy::Functor::Class_;
    definition.callAsFunction = &Message_callAsFunction;
    definition.finalize = &CYFinalize;
    Message_privateData::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Prototype";
    definition.staticFunctions = Prototype_staticFunctions;
    definition.finalize = &CYFinalize;
    CYPrivate<Prototype>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "Selector";
    definition.staticValues = Selector_staticValues;
    definition.staticFunctions = Selector_staticFunctions;
    definition.callAsFunction = &Selector_callAsFunction;
    definition.finalize = &CYFinalize;
    CYPrivate<Selector_privateData>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "Super";
    definition.finalize = &CYFinalize;
    CYPrivate<cy::Super>::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "ObjectiveC::Classes";
    definition.hasProperty = &ObjectiveC_Classes_hasProperty;
    definition.getProperty = &ObjectiveC_Classes_getProperty;
    definition.getPropertyNames = &ObjectiveC_Classes_getPropertyNames;
    ObjectiveC_Classes_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "ObjectiveC::Constants";
    definition.getProperty = &ObjectiveC_Constants_getProperty;
    definition.getPropertyNames = &ObjectiveC_Constants_getPropertyNames;
    ObjectiveC_Constants_ = JSClassCreate(&definition);

#ifdef __APPLE__
    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "ObjectiveC::Images";
    definition.getProperty = &ObjectiveC_Images_getProperty;
    definition.getPropertyNames = &ObjectiveC_Images_getPropertyNames;
    ObjectiveC_Images_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "ObjectiveC::Image::Classes";
    definition.getProperty = &ObjectiveC_Image_Classes_getProperty;
    definition.getPropertyNames = &ObjectiveC_Image_Classes_getPropertyNames;
    ObjectiveC_Image_Classes_ = JSClassCreate(&definition);
#endif

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "ObjectiveC::Protocols";
    definition.getProperty = &ObjectiveC_Protocols_getProperty;
    definition.getPropertyNames = &ObjectiveC_Protocols_getPropertyNames;
    ObjectiveC_Protocols_ = JSClassCreate(&definition);

#ifdef __APPLE__
    class_addMethod(NSCFType_, @selector(cy$toJSON:inContext:), reinterpret_cast<IMP>(&NSCFType$cy$toJSON$inContext$),
        // XXX: this is horrible; there has to be a better way to do this
    #ifdef __LP64__
        "^{OpaqueJSValue=}32@0:8@16^{OpaqueJSContext=}24"
    #else
        "^{OpaqueJSValue=}16@0:4@8^{OpaqueJSContext=}12"
    #endif
    );
#endif
} CYPoolCatch() }

void CYObjectiveC_SetupContext(JSContextRef context) { CYPoolTry {
    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cy(CYCastJSObject(context, CYGetProperty(context, global, cy_s)));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef all(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("all"))));
    JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    JSObjectRef ObjectiveC(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cycript, CYJSString("ObjectiveC"), ObjectiveC);

    JSObjectRef protocols(JSObjectMake(context, ObjectiveC_Protocols_, NULL));
    CYSetProperty(context, ObjectiveC, CYJSString("protocols"), protocols);
    CYArrayPush(context, alls, protocols);

    JSObjectRef classes(JSObjectMake(context, ObjectiveC_Classes_, NULL));
    CYSetProperty(context, ObjectiveC, CYJSString("classes"), classes);
    CYArrayPush(context, alls, classes);

    JSObjectRef constants(JSObjectMake(context, ObjectiveC_Constants_, NULL));
    CYSetProperty(context, ObjectiveC, CYJSString("constants"), constants);
    CYArrayPush(context, alls, constants);

#ifdef __APPLE__
    CYSetProperty(context, ObjectiveC, CYJSString("images"), JSObjectMake(context, ObjectiveC_Images_, NULL));
#endif

    JSObjectRef Message(JSObjectMakeConstructor(context, Message_privateData::Class_, NULL));
    JSObjectRef Selector(JSObjectMakeConstructor(context, CYPrivate<Selector_privateData>::Class_, &Selector_new));
    JSObjectRef Super(JSObjectMakeConstructor(context, CYPrivate<cy::Super>::Class_, &Super_new));

    JSObjectRef Instance(JSObjectMakeConstructor(context, CYPrivate<::Instance>::Class_, &Instance_new));
    JSObjectRef Instance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("Instance_prototype"), Instance_prototype);
    CYSetProperty(context, Instance, prototype_s, Instance_prototype);

    JSObjectRef ArrayInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("ArrayInstance_prototype"), ArrayInstance_prototype);
    JSObjectRef Array_prototype(CYGetCachedObject(context, CYJSString("Array_prototype")));
    CYSetPrototype(context, ArrayInstance_prototype, Array_prototype);

    JSObjectRef BooleanInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("BooleanInstance_prototype"), BooleanInstance_prototype);
    JSObjectRef Boolean_prototype(CYGetCachedObject(context, CYJSString("Boolean_prototype")));
    CYSetPrototype(context, BooleanInstance_prototype, Boolean_prototype);

    JSObjectRef FunctionInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("FunctionInstance_prototype"), FunctionInstance_prototype);
    JSObjectRef Function_prototype(CYGetCachedObject(context, CYJSString("Function_prototype")));
    CYSetPrototype(context, FunctionInstance_prototype, Function_prototype);

    JSObjectRef NumberInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("NumberInstance_prototype"), NumberInstance_prototype);
    JSObjectRef Number_prototype(CYGetCachedObject(context, CYJSString("Number_prototype")));
    CYSetPrototype(context, NumberInstance_prototype, Number_prototype);

    JSObjectRef ObjectInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("ObjectInstance_prototype"), ObjectInstance_prototype);
    JSObjectRef Object_prototype(CYGetCachedObject(context, CYJSString("Object_prototype")));
    CYSetPrototype(context, ObjectInstance_prototype, Object_prototype);

    JSObjectRef StringInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("StringInstance_prototype"), StringInstance_prototype);
    JSObjectRef String_prototype(CYGetCachedObject(context, CYJSString("String_prototype")));
    CYSetPrototype(context, StringInstance_prototype, String_prototype);

    JSObjectRef TypeInstance_prototype(JSObjectMake(context, CYPrivate<Prototype>::Class_, NULL));
    CYSetProperty(context, cy, CYJSString("TypeInstance_prototype"), TypeInstance_prototype);
    // XXX: maybe TypeInstance should have Type as its prototype? FWIW, that's why I named it like this ;P

    CYSetProperty(context, cycript, CYJSString("Instance"), Instance);
    CYSetProperty(context, cycript, CYJSString("Message"), Message);
    CYSetProperty(context, cycript, CYJSString("Selector"), Selector);
    CYSetProperty(context, cycript, CYJSString("objc_super"), Super);

    JSObjectRef box(JSObjectMakeFunctionWithCallback(context, CYJSString("box"), &Instance_box_callAsFunction));
    CYSetProperty(context, Instance, CYJSString("box"), box, kJSPropertyAttributeDontEnum);

#ifdef __APPLE__
    CYSetProperty(context, all, CYJSString("choose"), &choose, kJSPropertyAttributeDontEnum);
#endif

    CYSetProperty(context, all, CYJSString("objc_msgSend"), &$objc_msgSend, kJSPropertyAttributeDontEnum);

    CYSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, Message, prototype_s)), Function_prototype);
    CYSetPrototype(context, CYCastJSObject(context, CYGetProperty(context, Selector, prototype_s)), Function_prototype);

    JSObjectRef cache(CYGetCachedObject(context, CYJSString("cache")));
    CYSetProperty(context, cache, CYJSString("YES"), JSValueMakeBoolean(context, true), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("NO"), JSValueMakeBoolean(context, false), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("id"), CYMakeType(context, sig::Object()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("Class"), CYMakeType(context, sig::Meta()), kJSPropertyAttributeDontEnum);
    CYSetProperty(context, cache, CYJSString("SEL"), CYMakeType(context, sig::Selector()), kJSPropertyAttributeDontEnum);

    CYSetProperty(context, cy, CYJSString("cydget"), CYCastJSValue(context, false));
} CYPoolCatch() }

static void *CYObjectiveC_CastSymbol(const char *name) {
    if (false);
#ifdef __GNU_LIBOBJC__
    else if (strcmp(name, "object_getClass") == 0)
        return reinterpret_cast<void *>(&object_getClass);
#endif
    return NULL;
}

static CYHook CYObjectiveCHook = {
    &CYObjectiveC_ExecuteStart,
    &CYObjectiveC_ExecuteEnd,
    &CYObjectiveC_CallFunction,
    &CYObjectiveC_Initialize,
    &CYObjectiveC_SetupContext,
    &CYObjectiveC_CastSymbol,
};

CYRegisterHook CYObjectiveC(&CYObjectiveCHook);

_extern void CydgetSetupContext(JSGlobalContextRef context) { CYObjectiveTry_ {
    CYSetupContext(context);
    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cy(CYCastJSObject(context, CYGetProperty(context, global, cy_s)));
    CYSetProperty(context, cy, CYJSString("cydget"), CYCastJSValue(context, true));
} CYObjectiveCatch }

_extern void CydgetMemoryParse(const uint16_t **data, size_t *size) { try {
    CYPool pool;

    CYUTF8String utf8(CYPoolUTF8String(pool, CYUTF16String(*data, *size)));
    utf8 = CYPoolCode(pool, utf8);

    CYUTF16String utf16(CYPoolUTF16String(pool, CYUTF8String(utf8.data, utf8.size)));
    size_t bytes(utf16.size * sizeof(uint16_t));
    uint16_t *copy(reinterpret_cast<uint16_t *>(malloc(bytes)));
    memcpy(copy, utf16.data, bytes);

    *data = copy;
    *size = utf16.size;
} catch (const CYException &exception) {
    CYPool pool;
    @throw [NSException exceptionWithName:NSRangeException reason:[NSString stringWithFormat:@"%s", exception.PoolCString(pool)] userInfo:nil];
} }
