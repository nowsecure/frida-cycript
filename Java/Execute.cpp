/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
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

#include <map>
#include <sstream>
#include <vector>

#ifdef __APPLE__
#include <JavaVM/jni.h>
#else
#include <jni.h>
#endif

#include "cycript.hpp"
#include "Execute.hpp"
#include "Internal.hpp"
#include "JavaScript.hpp"
#include "Pooling.hpp"

#define _jnicall(expr) ({ \
    jint _value(expr); \
    if (_value != JNI_OK) \
        CYThrow("_jnicall(%s) == %d", #expr, _value); \
})

#define _envcall(jni, expr) ({ \
    __typeof__(jni->expr) _value(jni->expr); \
    if (jthrowable _error = jni->ExceptionOccurred()) { \
        jni->ExceptionClear(); \
        CYThrow("_envcall(%s): %p", #expr, _error); \
    } \
_value; })

#define _envcallv(jni, expr) do { \
    jni->expr; \
    if (jthrowable _error = jni->ExceptionOccurred()) { \
        jni->ExceptionClear(); \
        CYThrow("_envcall(%s): %p", #expr, _error); \
    } \
} while (false)

extern "C" {
    // Android's jni.h seriously doesn't declare these :/
    jint JNI_CreateJavaVM(JavaVM **, void **, void *);
    jint JNI_GetCreatedJavaVMs(JavaVM **, jsize, jsize *);
}

JNIEnv *GetJNI() {
    static JavaVM *jvm(NULL);
    static JNIEnv *jni(NULL);

    if (jni != NULL)
        return jni;
    jint version(JNI_VERSION_1_4);

    jsize capacity(16);
    JavaVM *jvms[capacity];
    jsize size;
    _jnicall(JNI_GetCreatedJavaVMs(jvms, capacity, &size));

    if (size != 0) {
        jvm = jvms[0];
        _jnicall(jvm->GetEnv(reinterpret_cast<void **>(&jni), version));
    } else {
        JavaVMInitArgs args;
        memset(&args, 0, sizeof(args));
        args.version = version;
        _jnicall(JNI_CreateJavaVM(&jvm, reinterpret_cast<void **>(&jni), &args));
    }

    return jni;
}

class CYJavaUTF8String :
    public CYUTF8String
{
  private:
    JNIEnv *jni_;
    jstring value_;

  public:
    CYJavaUTF8String(JNIEnv *jni, jstring value) :
        jni_(jni),
        value_(value)
    {
        size = jni_->GetStringUTFLength(value_);
        data = jni_->GetStringUTFChars(value_, NULL);
    }

    ~CYJavaUTF8String() {
        if (value_ != NULL)
           jni_->ReleaseStringUTFChars(value_, data);
    }

    CYJavaUTF8String(const CYJavaUTF8String &) = delete;

    CYJavaUTF8String(CYJavaUTF8String &&rhs) :
        jni_(rhs.jni_),
        value_(rhs.value_)
    {
        rhs.value_ = NULL;
    }
};

CYJavaUTF8String CYCastUTF8String(JNIEnv *jni, jstring value) {
    return CYJavaUTF8String(jni, value);
}

JSStringRef CYCopyJSString(JNIEnv *jni, jstring value) {
    return CYCopyJSString(CYCastUTF8String(jni, value));
}

template <typename Value_>
struct CYJavaGlobal {
    JNIEnv *jni_;
    Value_ value_;

    CYJavaGlobal() :
        jni_(NULL),
        value_(NULL)
    {
    }

    CYJavaGlobal(JNIEnv *jni, Value_ value) :
        jni_(jni),
        value_(static_cast<Value_>(_envcall(jni_, NewGlobalRef(value))))
    {
    }

    CYJavaGlobal(const CYJavaGlobal &value) :
        CYJavaGlobal(value.jni_, value.value_)
    {
    }

    CYJavaGlobal(CYJavaGlobal &&value) :
        jni_(value.jni_),
        value_(value.value_)
    {
        value.value_ = NULL;
    }

    ~CYJavaGlobal() {
        if (value_ != NULL)
            _envcallv(jni_, DeleteGlobalRef(value_));
    }

    operator bool() const {
        return value_ != NULL;
    }

    operator JNIEnv *() const {
        return jni_;
    }

    operator Value_() const {
        return value_;
    }
};

template <typename Internal_, typename Value_>
struct CYJavaValue :
    CYPrivate<Internal_>
{
    CYJavaGlobal<Value_> value_;

    CYJavaValue(JNIEnv *jni, Value_ value) :
        value_(jni, value)
    {
    }

    CYJavaValue(const CYJavaValue &) = delete;
};

#define CYJavaForEachPrimitive \
    CYJavaForEachPrimitive_(Z, Boolean, Boolean) \
    CYJavaForEachPrimitive_(B, Byte, Byte) \
    CYJavaForEachPrimitive_(C, Char, Character) \
    CYJavaForEachPrimitive_(S, Short, Short) \
    CYJavaForEachPrimitive_(I, Int, Integer) \
    CYJavaForEachPrimitive_(J, Long, Long) \
    CYJavaForEachPrimitive_(F, Float, Float) \
    CYJavaForEachPrimitive_(D, Double, Double)

enum CYJavaPrimitive : char {
    CYJavaPrimitiveObject,
    CYJavaPrimitiveVoid,
#define CYJavaForEachPrimitive_(T, Typ, Type) \
    CYJavaPrimitive ## Type,
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
};

template <typename Type_>
static _finline JSValueRef CYJavaCastJSValue(JSContextRef context, Type_ value) {
    return CYCastJSValue(context, value);
}

static _finline JSValueRef CYJavaCastJSValue(JSContextRef context, jboolean value) {
    return CYCastJSValue(context, static_cast<bool>(value));
}

static std::map<std::string, CYJavaPrimitive> Primitives_;

static CYJavaPrimitive CYJavaGetPrimitive(JNIEnv *jni, jobject type, jmethodID Class$get$$Name) {
    CYJavaUTF8String name(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(type, Class$get$$Name))));
    auto primitive(Primitives_.find(name));
    return primitive != Primitives_.end() ? primitive->second : CYJavaPrimitiveObject;
}

typedef std::vector<CYJavaPrimitive> CYJavaShorty;

static CYJavaShorty CYJavaGetShorty(JNIEnv *jni, jobjectArray types, jmethodID Class$get$$Name) {
    size_t count(_envcall(jni, GetArrayLength(types)));
    CYJavaShorty shorty(count);
    for (size_t index(0); index != count; ++index)
        shorty[index] = CYJavaGetPrimitive(jni, _envcall(jni, GetObjectArrayElement(types, index)), Class$get$$Name);
    return shorty;
}

struct CYJavaField {
    jfieldID field_;
    CYJavaPrimitive primitive_;
};

typedef std::map<std::string, CYJavaField> CYJavaFieldMap;

struct CYJavaSignature {
    CYJavaGlobal<jobject> method;
    CYJavaPrimitive primitive;
    CYJavaShorty shorty;

    CYJavaSignature(JNIEnv *jni, jobject method, CYJavaPrimitive primitive, const CYJavaShorty &shorty) :
        method(jni, method),
        primitive(primitive),
        shorty(shorty)
    {
    }

    CYJavaSignature(unsigned count) :
        shorty(count)
    {
    }

    bool operator <(const CYJavaSignature &rhs) const {
        return shorty.size() < rhs.shorty.size();
    }
};

typedef std::multiset<CYJavaSignature> CYJavaOverload;

struct CYJavaMethod :
    CYPrivate<CYJavaMethod>
{
    JNIEnv *jni_;
    CYJavaOverload overload_;

    // XXX: figure out move constructors on Apple's crappy toolchain
    CYJavaMethod(JNIEnv *jni, const CYJavaOverload &overload) :
        jni_(jni),
        overload_(overload)
    {
    }
};

struct CYJavaClass :
    CYJavaValue<CYJavaClass, jclass>
{
    CYJavaFieldMap static_;
    CYJavaFieldMap instance_;
    CYJavaOverload overload_;

    CYJavaClass(JNIEnv *jni, jclass value) :
        CYJavaValue(jni, value)
    {
    }
};

static JSObjectRef CYGetJavaClass(JSContextRef context, JNIEnv *jni, jclass _class);

struct CYJavaObject :
    CYJavaValue<CYJavaObject, jobject>
{
    CYJavaClass *table_;

    CYJavaObject(JNIEnv *jni, jobject value, JSContextRef context) :
        CYJavaValue(jni, value),
        table_(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(CYGetJavaClass(context, jni, _envcall(jni, GetObjectClass(value))))))
    {
    }

    JSValueRef GetPrototype(JSContextRef context) const;
};

struct CYJavaInterior :
    CYJavaValue<CYJavaInterior, jobject>
{
    CYJavaClass *table_;

    CYJavaInterior(JNIEnv *jni, jobject value, CYJavaClass *table) :
        CYJavaValue(jni, value),
        table_(table)
    {
    }
};

struct CYJavaPackage :
    CYPrivate<CYJavaPackage>
{
    typedef std::vector<std::string> Path;
    Path package_;

    _finline CYJavaPackage(const Path &package) :
        package_(package)
    {
    }
};

JSValueRef CYJavaObject::GetPrototype(JSContextRef context) const {
    JNIEnv *jni(value_);
    return CYGetProperty(context, CYGetJavaClass(context, jni, _envcall(jni, GetObjectClass(value_))), prototype_s);
}

static JSValueRef CYCastJSValue(JSContextRef context, JNIEnv *jni, jobject value) {
    if (value == NULL)
        return CYJSNull(context);
    return CYJavaObject::Make(context, jni, value, context);
}

static jstring CYCastJavaString(JNIEnv *jni, CYUTF16String value) {
    return _envcall(jni, NewString(value.data, value.size));
}

static jstring CYCastJavaString(JNIEnv *jni, JSStringRef value) {
    return CYCastJavaString(jni, CYCastUTF16String(value));
}

#define CYCastJava$(T, Type, jtype, Cast) \
_disused static jobject CYCastJava ## Type(JNIEnv *jni, JSContextRef context, JSValueRef value) { \
    jclass Type$(_envcall(jni, FindClass("java/lang/" #Type))); \
    jmethodID Type$init$(_envcall(jni, GetMethodID(Type$, "<init>", "(" #T ")V"))); \
    return _envcall(jni, NewObject(Type$, Type$init$, static_cast<jtype>(Cast(context, value)))); \
}

CYCastJava$(Z, Boolean, jboolean, CYCastBool)
CYCastJava$(B, Byte, jbyte, CYCastDouble)
CYCastJava$(C, Character, jchar, CYCastDouble)
CYCastJava$(S, Short, jshort, CYCastDouble)
CYCastJava$(I, Integer, jint, CYCastDouble)
CYCastJava$(J, Long, jlong, CYCastDouble)
CYCastJava$(F, Float, jfloat, CYCastDouble)
CYCastJava$(D, Double, jdouble, CYCastDouble)

static jobject CYCastJavaObject(JNIEnv *jni, JSContextRef context, JSObjectRef value) {
    JSObjectRef object(CYCastJSObject(context, value));
    if (JSValueIsObjectOfClass(context, value, CYJavaObject::Class_)) {
        CYJavaObject *internal(reinterpret_cast<CYJavaObject *>(JSObjectGetPrivate(object)));
        return internal->value_;
    }

    _assert(false);
}

static jobject CYCastJavaObject(JNIEnv *jni, JSContextRef context, JSValueRef value) {
    switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return NULL;
        case kJSTypeBoolean:
            return CYCastJavaBoolean(jni, context, value);
        case kJSTypeNumber:
            return CYCastJavaDouble(jni, context, value);
        case kJSTypeString:
            return CYCastJavaString(jni, CYJSString(context, value));
        case kJSTypeObject:
            return CYCastJavaObject(jni, context, CYCastJSObject(context, value));

        case kJSTypeUndefined:
        default:
            _assert(false);
    }
}

static JSObjectRef CYGetJavaClass(JSContextRef context, JNIEnv *jni, jclass value) {
    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cy(CYCastJSObject(context, CYGetProperty(context, global, cy_s)));

    jclass Class$(_envcall(jni, FindClass("java/lang/Class")));
    jmethodID Class$getCanonicalName(_envcall(jni, GetMethodID(Class$, "getCanonicalName", "()Ljava/lang/String;")));

    CYJSString name(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(value, Class$getCanonicalName))));
    JSValueRef cached(CYGetProperty(context, cy, name));
    if (!JSValueIsUndefined(context, cached))
        return CYCastJSObject(context, cached);

    jmethodID Class$getDeclaredConstructors(_envcall(jni, GetMethodID(Class$, "getDeclaredConstructors", "()[Ljava/lang/reflect/Constructor;")));
    jmethodID Class$getDeclaredFields(_envcall(jni, GetMethodID(Class$, "getDeclaredFields", "()[Ljava/lang/reflect/Field;")));
    jmethodID Class$getDeclaredMethods(_envcall(jni, GetMethodID(Class$, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;")));

    jclass Constructor$(_envcall(jni, FindClass("java/lang/reflect/Constructor")));
    //jmethodID Constructor$getModifiers(_envcall(jni, GetMethodID(Constructor$, "getModifiers", "()I")));
    jmethodID Constructor$getParameterTypes(_envcall(jni, GetMethodID(Constructor$, "getParameterTypes", "()[Ljava/lang/Class;")));

    jclass Field$(_envcall(jni, FindClass("java/lang/reflect/Field")));
    jmethodID Field$getModifiers(_envcall(jni, GetMethodID(Field$, "getModifiers", "()I")));
    jmethodID Field$getName(_envcall(jni, GetMethodID(Field$, "getName", "()Ljava/lang/String;")));
    jmethodID Field$getType(_envcall(jni, GetMethodID(Field$, "getType", "()Ljava/lang/Class;")));

    jclass Method$(_envcall(jni, FindClass("java/lang/reflect/Method")));
    jmethodID Method$getModifiers(_envcall(jni, GetMethodID(Method$, "getModifiers", "()I")));
    jmethodID Method$getName(_envcall(jni, GetMethodID(Method$, "getName", "()Ljava/lang/String;")));
    jmethodID Method$getParameterTypes(_envcall(jni, GetMethodID(Method$, "getParameterTypes", "()[Ljava/lang/Class;")));
    jmethodID Method$getReturnType(_envcall(jni, GetMethodID(Method$, "getReturnType", "()Ljava/lang/Class;")));

    jclass Modifier$(_envcall(jni, FindClass("java/lang/reflect/Modifier")));
    jmethodID Modifier$isStatic(_envcall(jni, GetStaticMethodID(Modifier$, "isStatic", "(I)Z")));

    CYJavaClass *table(new CYJavaClass(jni, value));

    for (jclass prototype(value); prototype != NULL; prototype = _envcall(jni, GetSuperclass(prototype))) {
        jobjectArray fields(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(prototype, Class$getDeclaredFields))));

        for (jsize i(0), e(_envcall(jni, GetArrayLength(fields))); i != e; ++i) {
            jobject field(_envcall(jni, GetObjectArrayElement(fields, e - i - 1)));
            jint modifiers(_envcall(jni, CallIntMethod(field, Field$getModifiers)));
            bool instance(!_envcall(jni, CallStaticBooleanMethod(Modifier$, Modifier$isStatic, modifiers)));
            auto &map(instance ? table->instance_ : table->static_);
            CYJavaUTF8String name(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(field, Field$getName))));
            jfieldID id(_envcall(jni, FromReflectedField(field)));
            jobject type(_envcall(jni, CallObjectMethod(field, Field$getType)));
            map.insert(std::make_pair(std::string(name), CYJavaField{id, CYJavaGetPrimitive(jni, type, Class$getCanonicalName)}));
        }
    }

    JSObjectRef constructor(JSObjectMake(context, CYJavaClass::Class_, table));
    JSObjectRef indirect(JSObjectMake(context, NULL, NULL));
    CYSetPrototype(context, constructor, indirect);

    JSObjectRef prototype(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, constructor, prototype_s, prototype, kJSPropertyAttributeDontEnum);

    jobjectArray constructors(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(value, Class$getDeclaredConstructors))));

    for (jsize i(0), e(_envcall(jni, GetArrayLength(constructors))); i != e; ++i) {
        jobject constructor(_envcall(jni, GetObjectArrayElement(constructors, i)));
        jobjectArray parameters(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(constructor, Constructor$getParameterTypes))));
        CYJavaShorty shorty(CYJavaGetShorty(jni, parameters, Class$getCanonicalName));
        table->overload_.insert(CYJavaSignature(jni, constructor, CYJavaPrimitiveObject, shorty));
    }

    jobjectArray methods(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(value, Class$getDeclaredMethods))));

    std::map<std::pair<bool, std::string>, CYJavaOverload> entries;

    for (jsize i(0), e(_envcall(jni, GetArrayLength(methods))); i != e; ++i) {
        jobject method(_envcall(jni, GetObjectArrayElement(methods, i)));
        jint modifiers(_envcall(jni, CallIntMethod(method, Method$getModifiers)));
        bool instance(!_envcall(jni, CallStaticBooleanMethod(Modifier$, Modifier$isStatic, modifiers)));
        CYJavaUTF8String name(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(method, Method$getName))));
        jobjectArray parameters(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(method, Method$getParameterTypes))));
        CYJavaShorty shorty(CYJavaGetShorty(jni, parameters, Class$getCanonicalName));
        jobject type(_envcall(jni, CallObjectMethod(method, Method$getReturnType)));
        auto primitive(CYJavaGetPrimitive(jni, type, Class$getCanonicalName));
        entries[std::make_pair(instance, std::string(name))].insert(CYJavaSignature(jni, method, primitive, shorty));
    }

    for (const auto &entry : entries) {
        bool instance(entry.first.first);
        CYJSString name(entry.first.second);
        auto &overload(entry.second);
        auto target(instance ? prototype : indirect);
        JSValueRef wrapper(CYJavaMethod::Make(context, jni, overload));
        CYSetProperty(context, target, name, wrapper, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete);
    }

    // XXX: for some reason kJSPropertyAttributeDontEnum doesn't work if there's already a property with the same name
    // by not linking the prototypes until after we set the properties, we hide the parent property from this issue :(

    if (jclass super = _envcall(jni, GetSuperclass(value))) {
        JSObjectRef parent(CYGetJavaClass(context, jni, super));
        CYSetPrototype(context, indirect, CYGetPrototype(context, parent));
        CYSetPrototype(context, prototype, CYGetProperty(context, parent, prototype_s));
    }

    CYSetProperty(context, cy, name, constructor);
    return constructor;
}

static jobjectArray CYCastJavaArguments(JNIEnv *jni, const CYJavaShorty &shorty, JSContextRef context, const JSValueRef arguments[], jclass Object$) {
    jobjectArray array(_envcall(jni, NewObjectArray(shorty.size(), Object$, NULL)));
    for (size_t index(0); index != shorty.size(); ++index) {
        jobject argument;
        switch (shorty[index]) {
            case CYJavaPrimitiveObject:
                argument = CYCastJavaObject(jni, context, arguments[index]);
                break;
#define CYJavaForEachPrimitive_(T, Typ, Type) \
            case CYJavaPrimitive ## Type: \
                argument = CYCastJava ## Type(jni, context, arguments[index]); \
                break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
            default:
                _assert(false);
        }
        _envcallv(jni, SetObjectArrayElement(array, index, argument));
    }

    return array;
}

static JSValueRef JavaMethod_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYJavaMethod *internal(reinterpret_cast<CYJavaMethod *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->jni_);

    jclass Object$(_envcall(jni, FindClass("java/lang/Object")));

    jclass Method$(_envcall(jni, FindClass("java/lang/reflect/Method")));
    jmethodID Method$invoke(_envcall(jni, GetMethodID(Method$, "invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;")));

    jobject self(CYCastJavaObject(jni, context, _this));

    CYJavaSignature bound(count);
    for (auto overload(internal->overload_.lower_bound(bound)), e(internal->overload_.upper_bound(bound)); overload != e; ++overload) {
        jobjectArray array(CYCastJavaArguments(jni, overload->shorty, context, arguments, Object$));
        jobject object(_envcall(jni, CallObjectMethod(overload->method, Method$invoke, self, array)));
        return CYCastJSValue(context, jni, object);
    }

    CYThrow("invalid method call");
} CYCatch(NULL) }

static JSObjectRef JavaClass_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(table->value_);

    jclass Object$(_envcall(jni, FindClass("java/lang/Object")));

    jclass Constructor$(_envcall(jni, FindClass("java/lang/reflect/Constructor")));
    jmethodID Constructor$newInstance(_envcall(jni, GetMethodID(Constructor$, "newInstance", "([Ljava/lang/Object;)Ljava/lang/Object;")));

    CYJavaSignature bound(count);
    for (auto overload(table->overload_.lower_bound(bound)), e(table->overload_.upper_bound(bound)); overload != e; ++overload) {
        jobjectArray array(CYCastJavaArguments(jni, overload->shorty, context, arguments, Object$));
        jobject object(_envcall(jni, CallObjectMethod(overload->method, Constructor$newInstance, array)));
        return CYCastJSObject(context, CYCastJSValue(context, jni, object));
    }

    CYThrow("invalid constructor call");
} CYCatch(NULL) }

static bool JavaClass_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return false;
    return true;
}

static JSValueRef JavaClass_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(table->value_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return NULL;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            return CYCastJSValue(context, jni, _envcall(jni, GetStaticObjectField(table->value_, field->second.field_)));
#define CYJavaForEachPrimitive_(T, Typ, Type) \
        case CYJavaPrimitive ## Type: \
            return CYJavaCastJSValue(context, _envcall(jni, GetStatic ## Typ ## Field(table->value_, field->second.field_)));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaClass_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(table->value_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return false;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            _envcallv(jni, SetStaticObjectField(table->value_, field->second.field_, CYCastJavaObject(jni, context, value)));
#define CYJavaForEachPrimitive_(T, Typ, Type) \
        case CYJavaPrimitive ## Type: \
            _envcallv(jni, SetStatic ## Typ ## Field(table->value_, field->second.field_, CYCastDouble(context, value))); \
            break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static void JavaClass_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    for (const auto &field : table->static_)
        JSPropertyNameAccumulatorAddName(names, CYJSString(field.first));
}

static JSValueRef JavaClass_getProperty_class(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    return CYCastJSValue(context, table->value_, table->value_);
} CYCatch(NULL) }

static bool JavaInterior_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    CYJavaInterior *internal(reinterpret_cast<CYJavaInterior *>(JSObjectGetPrivate(object)));
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->instance_.find(name));
    if (field == table->instance_.end())
        return false;
    return true;
}

static JSValueRef JavaInterior_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaInterior *internal(reinterpret_cast<CYJavaInterior *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->instance_.find(name));
    if (field == table->instance_.end())
        return NULL;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            return CYCastJSValue(context, jni, _envcall(jni, GetObjectField(internal->value_, field->second.field_)));
#define CYJavaForEachPrimitive_(T, Typ, Type) \
        case CYJavaPrimitive ## Type: \
            return CYJavaCastJSValue(context, _envcall(jni, Get ## Typ ## Field(internal->value_, field->second.field_)));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaInterior_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYJavaInterior *internal(reinterpret_cast<CYJavaInterior *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->instance_.find(name));
    if (field == table->instance_.end())
        return false;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            _envcallv(jni, SetObjectField(table->value_, field->second.field_, CYCastJavaObject(jni, context, value)));
#define CYJavaForEachPrimitive_(T, Typ, Type) \
        case CYJavaPrimitive ## Type: \
            _envcallv(jni, Set ## Typ ## Field(table->value_, field->second.field_, CYCastDouble(context, value))); \
            break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static void JavaInterior_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYJavaInterior *internal(reinterpret_cast<CYJavaInterior *>(JSObjectGetPrivate(object)));
    CYJavaClass *table(internal->table_);
    for (const auto &field : table->instance_)
        JSPropertyNameAccumulatorAddName(names, CYJSString(field.first));
}

static JSValueRef JavaObject_getProperty_constructor(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaObject *internal(reinterpret_cast<CYJavaObject *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    return CYGetJavaClass(context, jni, _envcall(jni, GetObjectClass(internal->value_)));
} CYCatch(NULL) }

static JSValueRef JavaObject_getProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaObject *internal(reinterpret_cast<CYJavaObject *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    return CYJavaInterior::Make(context, jni, internal->value_, internal->table_);
} CYCatch(NULL) }

static bool CYJavaPackage_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    return true;
}

static JSValueRef CYJavaPackage_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaPackage *internal(reinterpret_cast<CYJavaPackage *>(JSObjectGetPrivate(object)));
    CYJavaPackage::Path package(internal->package_);

    CYPool pool;
    const char *next(CYPoolCString(pool, context, property));

    std::ostringstream name;
    for (auto &package : internal->package_)
        name << package << '/';
    name << next;

    JNIEnv *jni(GetJNI());
    if (jclass _class = jni->FindClass(name.str().c_str()))
        return CYGetJavaClass(context, jni, _class);
    jni->ExceptionClear();

    package.push_back(next);
    return CYJavaPackage::Make(context, package);
} CYCatch(NULL) }

static JSStaticValue JavaClass_staticValues[2] = {
    {"class", &JavaClass_getProperty_class, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticValue JavaObject_staticValues[3] = {
    {"constructor", &JavaObject_getProperty_constructor, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"$cyi", &JavaObject_getProperty_$cyi, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction JavaPackage_staticFunctions[1] = {
    {NULL, NULL, 0}
};

void CYJava_Initialize() {
    Primitives_.insert(std::make_pair("void", CYJavaPrimitiveVoid));
    Primitives_.insert(std::make_pair("boolean", CYJavaPrimitiveBoolean));
    Primitives_.insert(std::make_pair("byte", CYJavaPrimitiveByte));
    Primitives_.insert(std::make_pair("char", CYJavaPrimitiveCharacter));
    Primitives_.insert(std::make_pair("short", CYJavaPrimitiveShort));
    Primitives_.insert(std::make_pair("int", CYJavaPrimitiveInteger));
    Primitives_.insert(std::make_pair("long", CYJavaPrimitiveLong));
    Primitives_.insert(std::make_pair("float", CYJavaPrimitiveFloat));
    Primitives_.insert(std::make_pair("double", CYJavaPrimitiveDouble));

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.className = "JavaClass";
    definition.staticValues = JavaClass_staticValues;
    definition.hasProperty = &JavaClass_hasProperty;
    definition.getProperty = &JavaClass_getProperty;
    definition.setProperty = &JavaClass_setProperty;
    definition.getPropertyNames = &JavaClass_getPropertyNames;
    definition.callAsConstructor = &JavaClass_callAsConstructor;
    definition.finalize = &CYFinalize;
    CYJavaClass::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "JavaInterior";
    definition.hasProperty = &JavaInterior_hasProperty;
    definition.getProperty = &JavaInterior_getProperty;
    definition.setProperty = &JavaInterior_setProperty;
    definition.getPropertyNames = &JavaInterior_getPropertyNames;
    definition.finalize = &CYFinalize;
    CYJavaInterior::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "JavaMethod";
    definition.callAsFunction = &JavaMethod_callAsFunction;
    definition.finalize = &CYFinalize;
    CYJavaMethod::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "JavaObject";
    definition.staticValues = JavaObject_staticValues;
    definition.finalize = &CYFinalize;
    CYJavaObject::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "JavaPackage";
    definition.staticFunctions = JavaPackage_staticFunctions;
    definition.hasProperty = &CYJavaPackage_hasProperty;
    definition.getProperty = &CYJavaPackage_getProperty;
    definition.finalize = &CYFinalize;
    CYJavaPackage::Class_ = JSClassCreate(&definition);
}

void CYJava_SetupContext(JSContextRef context) {
    JSObjectRef global(CYGetGlobalObject(context));
    //JSObjectRef cy(CYCastJSObject(context, CYGetProperty(context, global, cy_s)));
    JSObjectRef cycript(CYCastJSObject(context, CYGetProperty(context, global, CYJSString("Cycript"))));
    JSObjectRef all(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("all"))));
    //JSObjectRef alls(CYCastJSObject(context, CYGetProperty(context, cycript, CYJSString("alls"))));

    JSObjectRef Java(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, cycript, CYJSString("Java"), Java);

    JSObjectRef Packages(CYJavaPackage::Make(context, CYJavaPackage::Path()));
    CYSetProperty(context, all, CYJSString("Packages"), Packages);

    for (auto name : (const char *[]) {"java", "javax", "android", "com", "net", "org"}) {
        CYJSString js(name);
        CYSetProperty(context, all, js, CYGetProperty(context, Packages, js));
    }
}

static CYHook CYJavaHook = {
    NULL,
    NULL,
    NULL,
    &CYJava_Initialize,
    &CYJava_SetupContext,
    NULL,
};

CYRegisterHook CYJava(&CYJavaHook);
