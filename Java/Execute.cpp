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
        _assert(value != NULL);
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
    CYJavaForEachPrimitive_(Z, z, Boolean, Boolean, boolean) \
    CYJavaForEachPrimitive_(B, b, Byte, Byte, byte) \
    CYJavaForEachPrimitive_(C, c, Char, Character, char) \
    CYJavaForEachPrimitive_(S, s, Short, Short, short) \
    CYJavaForEachPrimitive_(I, i, Int, Integer, int) \
    CYJavaForEachPrimitive_(J, j, Long, Long, long) \
    CYJavaForEachPrimitive_(F, f, Float, Float, float) \
    CYJavaForEachPrimitive_(D, d, Double, Double, double)

enum CYJavaPrimitive : char {
    CYJavaPrimitiveObject,
    CYJavaPrimitiveVoid,
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
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
    jstring string(static_cast<jstring>(_envcall(jni, CallObjectMethod(type, Class$get$$Name))));
    _assert(string != NULL);
    CYJavaUTF8String name(jni, string);
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
    jmethodID method_;
    CYJavaGlobal<jobject> reflected_;
    CYJavaPrimitive primitive_;
    CYJavaShorty shorty_;

    CYJavaSignature(JNIEnv *jni, jmethodID method, jobject reflected, CYJavaPrimitive primitive, const CYJavaShorty &shorty) :
        method_(method),
        reflected_(jni, reflected),
        primitive_(primitive),
        shorty_(shorty)
    {
    }

    CYJavaSignature(unsigned count) :
        shorty_(count)
    {
    }

    bool operator <(const CYJavaSignature &rhs) const {
        return shorty_.size() < rhs.shorty_.size();
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

    CYJavaObject(JNIEnv *jni, jobject value, CYJavaClass *table) :
        CYJavaValue(jni, value),
        table_(table)
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

struct CYJavaStatic :
    CYJavaValue<CYJavaStatic, jobject>
{
    CYJavaClass *table_;

    CYJavaStatic(JNIEnv *jni, jobject value, CYJavaClass *table) :
        CYJavaValue(jni, value),
        table_(table)
    {
    }
};

struct CYJavaArray :
    CYJavaValue<CYJavaArray, jarray>
{
    CYJavaPrimitive primitive_;

    CYJavaArray(JNIEnv *jni, jarray value, CYJavaPrimitive primitive) :
        CYJavaValue(jni, value),
        primitive_(primitive)
    {
    }

    JSValueRef GetPrototype(JSContextRef context) const;
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

JSValueRef CYJavaArray::GetPrototype(JSContextRef context) const {
    return CYGetCachedObject(context, CYJSString("Array_prototype"));
}

static JSValueRef CYCastJSValue(JSContextRef context, JNIEnv *jni, jobject value) {
    if (value == NULL)
        return CYJSNull(context);

    jclass _class(_envcall(jni, GetObjectClass(value)));
    if (_envcall(jni, IsSameObject(_class, _envcall(jni, FindClass("java/lang/String")))))
        return CYCastJSValue(context, CYJSString(CYJavaUTF8String(jni, static_cast<jstring>(value))));

    jclass Class$(_envcall(jni, FindClass("java/lang/Class")));
    jmethodID Class$isArray(_envcall(jni, GetMethodID(Class$, "isArray", "()Z")));
    if (_envcall(jni, CallBooleanMethod(_class, Class$isArray))) {
        jmethodID Class$getComponentType(_envcall(jni, GetMethodID(Class$, "getComponentType", "()Ljava/lang/Class;")));
        jclass component(static_cast<jclass>(_envcall(jni, CallObjectMethod(_class, Class$getComponentType))));
        jmethodID Class$getName(_envcall(jni, GetMethodID(Class$, "getName", "()Ljava/lang/String;")));
        return CYJavaArray::Make(context, jni, static_cast<jarray>(value), CYJavaGetPrimitive(jni, component, Class$getName));
    }

    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(CYGetJavaClass(context, jni, _class))));
    return CYJavaObject::Make(context, jni, value, table);
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
    jmethodID Class$getName(_envcall(jni, GetMethodID(Class$, "getName", "()Ljava/lang/String;")));

    CYJSString name(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(value, Class$getName))));
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
            map.insert(std::make_pair(std::string(name), CYJavaField{id, CYJavaGetPrimitive(jni, type, Class$getName)}));
        }
    }

    JSObjectRef constructor(JSObjectMake(context, CYJavaClass::Class_, table));

    JSObjectRef prototype(JSObjectMake(context, NULL, NULL));
    CYSetProperty(context, constructor, prototype_s, prototype, kJSPropertyAttributeDontEnum);

    jobjectArray constructors(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(value, Class$getDeclaredConstructors))));

    for (jsize i(0), e(_envcall(jni, GetArrayLength(constructors))); i != e; ++i) {
        jobject constructor(_envcall(jni, GetObjectArrayElement(constructors, i)));
        jobjectArray parameters(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(constructor, Constructor$getParameterTypes))));
        CYJavaShorty shorty(CYJavaGetShorty(jni, parameters, Class$getName));
        jmethodID id(_envcall(jni, FromReflectedMethod(constructor)));
        table->overload_.insert(CYJavaSignature(jni, id, constructor, CYJavaPrimitiveObject, shorty));
    }

    jobjectArray methods(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(value, Class$getDeclaredMethods))));

    std::map<std::pair<bool, std::string>, CYJavaOverload> entries;

    for (jsize i(0), e(_envcall(jni, GetArrayLength(methods))); i != e; ++i) {
        jobject method(_envcall(jni, GetObjectArrayElement(methods, i)));
        jint modifiers(_envcall(jni, CallIntMethod(method, Method$getModifiers)));
        bool instance(!_envcall(jni, CallStaticBooleanMethod(Modifier$, Modifier$isStatic, modifiers)));
        CYJavaUTF8String name(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(method, Method$getName))));
        jobjectArray parameters(static_cast<jobjectArray>(_envcall(jni, CallObjectMethod(method, Method$getParameterTypes))));
        CYJavaShorty shorty(CYJavaGetShorty(jni, parameters, Class$getName));
        jobject type(_envcall(jni, CallObjectMethod(method, Method$getReturnType)));
        auto primitive(CYJavaGetPrimitive(jni, type, Class$getName));
        jmethodID id(_envcall(jni, FromReflectedMethod(method)));
        entries[std::make_pair(instance, std::string(name))].insert(CYJavaSignature(jni, id, method, primitive, shorty));
    }

    for (const auto &entry : entries) {
        bool instance(entry.first.first);
        CYJSString name(entry.first.second);
        auto &overload(entry.second);
        auto target(instance ? prototype : constructor);
        JSValueRef wrapper(CYJavaMethod::Make(context, jni, overload));
        CYSetProperty(context, target, name, wrapper, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete);
    }

    // XXX: for some reason kJSPropertyAttributeDontEnum doesn't work if there's already a property with the same name
    // by not linking the prototypes until after we set the properties, we hide the parent property from this issue :(

    if (jclass super = _envcall(jni, GetSuperclass(value))) {
        JSObjectRef parent(CYGetJavaClass(context, jni, super));
        CYSetPrototype(context, constructor, parent);
        CYSetPrototype(context, prototype, CYGetProperty(context, parent, prototype_s));
    }

    CYSetProperty(context, cy, name, constructor);
    return constructor;
}

static void CYCastJavaNumeric(jvalue &value, CYJavaPrimitive primitive, JSContextRef context, JSValueRef argument) {
    switch (primitive) {
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            value.t = static_cast<j ## type>(CYCastDouble(context, argument)); \
            break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default:
            _assert(false);
    }
}

static bool CYCastJavaArguments(JNIEnv *jni, const CYJavaShorty &shorty, JSContextRef context, const JSValueRef arguments[], jvalue *array) {
    for (size_t index(0); index != shorty.size(); ++index) {
        JSValueRef argument(arguments[index]);
        JSType type(JSValueGetType(context, argument));
        jvalue &value(array[index]);

        switch (CYJavaPrimitive primitive = shorty[index]) {
            case CYJavaPrimitiveObject:
                value.l = CYCastJavaObject(jni, context, argument);
            break;

            case CYJavaPrimitiveBoolean:
                if (type != kJSTypeBoolean)
                    return false;
                value.z = CYCastBool(context, argument);
            break;

            case CYJavaPrimitiveCharacter:
                if (type == kJSTypeNumber)
                    CYCastJavaNumeric(value, primitive, context, argument);
                else if (type != kJSTypeString)
                    return false;
                else {
                    CYJSString string(context, argument);
                    if (JSStringGetLength(string) != 1)
                        return false;
                    else
                        value.c = JSStringGetCharactersPtr(string)[0];
                }
            break;

            case CYJavaPrimitiveByte:
            case CYJavaPrimitiveShort:
            case CYJavaPrimitiveInteger:
            case CYJavaPrimitiveLong:
            case CYJavaPrimitiveFloat:
            case CYJavaPrimitiveDouble:
                if (type != kJSTypeNumber)
                    return false;
                CYCastJavaNumeric(value, primitive, context, argument);
            break;

            default:
                _assert(false);
        }
    }

    return true;
}

static JSValueRef JavaMethod_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYJavaMethod *internal(reinterpret_cast<CYJavaMethod *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->jni_);
    jobject self(CYCastJavaObject(jni, context, _this));

    CYJavaSignature bound(count);
    for (auto overload(internal->overload_.lower_bound(bound)), e(internal->overload_.upper_bound(bound)); overload != e; ++overload) {
        jvalue array[count];
        if (!CYCastJavaArguments(jni, overload->shorty_, context, arguments, array))
            continue;
        switch (overload->primitive_) {
            case CYJavaPrimitiveObject:
                return CYCastJSValue(context, jni, _envcall(jni, CallObjectMethodA(self, overload->method_, array)));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
            case CYJavaPrimitive ## Type: \
                return CYJavaCastJSValue(context, _envcall(jni, Call ## Typ ## MethodA(self, overload->method_, array)));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
            default: _assert(false);
        }
    }

    CYThrow("invalid method call");
} CYCatch(NULL) }

static JSObjectRef JavaClass_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(table->value_);

    CYJavaSignature bound(count);
    for (auto overload(table->overload_.lower_bound(bound)), e(table->overload_.upper_bound(bound)); overload != e; ++overload) {
        jvalue array[count];
        if (!CYCastJavaArguments(jni, overload->shorty_, context, arguments, array))
            continue;
        jobject object(_envcall(jni, NewObjectA(table->value_, overload->method_, array)));
        // XXX: going through JSValueRef is kind of dumb, no?
        return CYCastJSObject(context, CYCastJSValue(context, jni, object));
    }

    CYThrow("invalid constructor call");
} CYCatch(NULL) }

static bool JavaStatic_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    CYJavaStatic *internal(reinterpret_cast<CYJavaStatic *>(JSObjectGetPrivate(object)));
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return false;
    return true;
}

static JSValueRef JavaStatic_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaStatic *internal(reinterpret_cast<CYJavaStatic *>(JSObjectGetPrivate(object)));
    CYJavaClass *table(internal->table_);
    JNIEnv *jni(table->value_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return NULL;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            return CYCastJSValue(context, jni, _envcall(jni, GetStaticObjectField(table->value_, field->second.field_)));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            return CYJavaCastJSValue(context, _envcall(jni, GetStatic ## Typ ## Field(table->value_, field->second.field_)));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaStatic_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYJavaStatic *internal(reinterpret_cast<CYJavaStatic *>(JSObjectGetPrivate(object)));
    CYJavaClass *table(internal->table_);
    JNIEnv *jni(table->value_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return false;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            _envcallv(jni, SetStaticObjectField(table->value_, field->second.field_, CYCastJavaObject(jni, context, value)));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            _envcallv(jni, SetStatic ## Typ ## Field(table->value_, field->second.field_, CYCastDouble(context, value))); \
            break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static void JavaStatic_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    CYJavaStatic *internal(reinterpret_cast<CYJavaStatic *>(JSObjectGetPrivate(object)));
    CYJavaClass *table(internal->table_);
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
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
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
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
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

static JSValueRef JavaClass_getProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaClass *internal(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    return CYJavaStatic::Make(context, jni, internal->value_, internal);
} CYCatch(NULL) }

static JSValueRef JavaObject_getProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaObject *internal(reinterpret_cast<CYJavaObject *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    return CYJavaInterior::Make(context, jni, internal->value_, internal->table_);
} CYCatch(NULL) }

static JSValueRef JavaClass_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYJavaClass *internal(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(_this)));
    JNIEnv *jni(internal->value_);
    jclass Class$(_envcall(jni, FindClass("java/lang/Class")));
    jmethodID Class$getCanonicalName(_envcall(jni, GetMethodID(Class$, "getCanonicalName", "()Ljava/lang/String;")));
    return CYCastJSValue(context, CYJSString(jni, static_cast<jstring>(_envcall(jni, CallObjectMethod(internal->value_, Class$getCanonicalName)))));
} CYCatch(NULL) }

static JSValueRef JavaMethod_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    std::ostringstream cyon;
    return CYCastJSValue(context, CYJSString(cyon.str()));
} CYCatch(NULL) }

static JSValueRef JavaArray_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    CYJavaArray *internal(reinterpret_cast<CYJavaArray *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);
    if (JSStringIsEqual(property, length_s))
        return CYCastJSValue(context, _envcall(jni, GetArrayLength(internal->value_)));

    CYPool pool;
    ssize_t offset;
    if (!CYGetOffset(pool, context, property, offset))
        return NULL;

    if (internal->primitive_ == CYJavaPrimitiveObject)
        return CYCastJSValue(context, jni, _envcall(jni, GetObjectArrayElement(static_cast<jobjectArray>(internal->value_.value_), offset)));
    else switch (internal->primitive_) {
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: { \
            j ## type element; \
            _envcallv(jni, Get ## Typ ## ArrayRegion(static_cast<j ## type ## Array>(internal->value_.value_), offset, 1, &element)); \
            return CYJavaCastJSValue(context, element); \
        } break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaArray_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    CYJavaArray *internal(reinterpret_cast<CYJavaArray *>(JSObjectGetPrivate(object)));
    JNIEnv *jni(internal->value_);

    CYPool pool;
    ssize_t offset;
    if (!CYGetOffset(pool, context, property, offset))
        return false;

    if (internal->primitive_ == CYJavaPrimitiveObject)
        _envcallv(jni, SetObjectArrayElement(static_cast<jobjectArray>(internal->value_.value_), offset, CYCastJavaObject(jni, context, value)));
    else switch (internal->primitive_) {
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: { \
            j ## type element; \
            _envcallv(jni, Get ## Typ ## ArrayRegion(static_cast<j ## type ## Array>(internal->value_.value_), offset, 1, &element)); \
            return CYJavaCastJSValue(context, element); \
        } break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static JSValueRef JavaPackage_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    CYJavaPackage *internal(reinterpret_cast<CYJavaPackage *>(JSObjectGetPrivate(_this)));
    std::ostringstream name;
    for (auto &package : internal->package_)
        name << package << '.';
    name << '*';
    return CYCastJSValue(context, CYJSString(name.str()));
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

static JSStaticValue JavaClass_staticValues[3] = {
    {"class", &JavaClass_getProperty_class, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"$cyi", &JavaClass_getProperty_$cyi, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction JavaClass_staticFunctions[2] = {
    {"toCYON", &JavaClass_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticValue JavaObject_staticValues[3] = {
    {"constructor", &JavaObject_getProperty_constructor, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {"$cyi", &JavaObject_getProperty_$cyi, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, NULL, 0}
};

static JSStaticFunction JavaMethod_staticFunctions[2] = {
    {"toCYON", &JavaMethod_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

static JSStaticFunction JavaPackage_staticFunctions[2] = {
    {"toCYON", &JavaPackage_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
    {NULL, NULL, 0}
};

void CYJava_Initialize() {
    Primitives_.insert(std::make_pair("void", CYJavaPrimitiveVoid));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
    Primitives_.insert(std::make_pair(#type, CYJavaPrimitive ## Type));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_

    JSClassDefinition definition;

    definition = kJSClassDefinitionEmpty;
    definition.className = "JavaClass";
    definition.staticValues = JavaClass_staticValues;
    definition.staticFunctions = JavaClass_staticFunctions;
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
    definition.staticFunctions = JavaMethod_staticFunctions;
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
    definition.className = "JavaArray";
    definition.getProperty = &JavaArray_getProperty;
    definition.setProperty = &JavaArray_setProperty;
    definition.finalize = &CYFinalize;
    CYJavaArray::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.className = "JavaPackage";
    definition.staticFunctions = JavaPackage_staticFunctions;
    definition.hasProperty = &CYJavaPackage_hasProperty;
    definition.getProperty = &CYJavaPackage_getProperty;
    definition.finalize = &CYFinalize;
    CYJavaPackage::Class_ = JSClassCreate(&definition);

    definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.className = "JavaStatic";
    definition.hasProperty = &JavaStatic_hasProperty;
    definition.getProperty = &JavaStatic_getProperty;
    definition.setProperty = &JavaStatic_setProperty;
    definition.getPropertyNames = &JavaStatic_getPropertyNames;
    definition.finalize = &CYFinalize;
    CYJavaStatic::Class_ = JSClassCreate(&definition);
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
