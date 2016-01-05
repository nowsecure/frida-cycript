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

#include <map>
#include <sstream>
#include <vector>

#include <dlfcn.h>

#ifdef __APPLE__
#include <JavaVM/jni.h>
#else
#include <jni.h>
#endif

#ifdef __ANDROID__
// XXX: this is deprecated?!?!?!?!?!?!
#include <sys/system_properties.h>
#endif

#include "cycript.hpp"
#include "Error.hpp"
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
        throw CYJavaError(CYJavaLocal<jthrowable>(jni, _error)); \
    } \
_value; })

#define _envcallv(jni, expr) do { \
    jni->expr; \
    if (jthrowable _error = jni->ExceptionOccurred()) { \
        jni->ExceptionClear(); \
        throw CYJavaError(CYJavaLocal<jthrowable>(jni, _error)); \
    } \
} while (false)

#define CYJavaTry \
    CYJavaEnv jni(env); \
    auto &protect(*reinterpret_cast<CYProtect *>(jprotect)); \
    _disused JSContextRef context(protect); \
    _disused JSObjectRef object(protect); \
    try
#define CYJavaCatch(value) \
    catch (const CYException &error) { \
        jni->Throw(CYCastJavaObject(jni, context, error.CastJSValue(context, "Error")).cast<jthrowable>()); \
        return value; \
    }

static JNIEnv *GetJNI(JSContextRef context);

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
struct IsJavaPrimitive { static const bool value = false; };

#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
    template <> \
    struct IsJavaPrimitive<j ## type> { static const bool value = true; };
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_

// Java References {{{
template <typename Value_>
struct CYJavaRef {
    JNIEnv *jni_;
    Value_ value_;

    _finline CYJavaRef(JNIEnv *jni, Value_ value) :
        jni_(jni),
        value_(value)
    {
    }

    _finline operator Value_() const {
        return value_;
    }

    _finline JNIEnv *jni() const {
        return jni_;
    }

    // XXX: this is only needed to support CYJavaEnv relying on C variadics
    _finline Value_ get() const {
        return value_;
    }

    template <typename Other_>
    _finline CYJavaRef<Other_> cast() const {
        return {jni_, static_cast<Other_>(value_)};
    }

    // XXX: this should be tied into CYJavaFrame
    Value_ leak() {
        Value_ value(value_);
        value_ = NULL;
        return value;
    }
};

template <typename Value_, void (JNIEnv::*Delete_)(jobject)>
struct CYJavaDelete :
    CYJavaRef<Value_>
{
    _finline CYJavaDelete(JNIEnv *jni, Value_ value) :
        CYJavaRef<Value_>(jni, value)
    {
    }

    void clear() {
        if (this->value_ != NULL)
            (this->jni_->*Delete_)(this->value_);
        this->value_ = NULL;
    }

    ~CYJavaDelete() {
        clear();
    }
};

template <typename Value_>
struct CYJavaGlobal :
    CYJavaDelete<Value_, &JNIEnv::DeleteGlobalRef>
{
    typedef CYJavaDelete<Value_, &JNIEnv::DeleteGlobalRef> CYJavaBase;

    CYJavaGlobal() :
        CYJavaBase(NULL, NULL)
    {
    }

    template <typename Other_>
    CYJavaGlobal(const CYJavaRef<Other_> &other) :
        CYJavaBase(other.jni_, static_cast<Other_>(other.jni_->NewGlobalRef(other.value_)))
    {
    }

    CYJavaGlobal(const CYJavaGlobal<Value_> &other) :
        CYJavaGlobal(static_cast<const CYJavaRef<Value_> &>(other))
    {
    }

    CYJavaGlobal(CYJavaGlobal &&value) :
        CYJavaBase(value.jni_, value.value_)
    {
        value.value_ = NULL;
    }
};

template <typename Value_>
struct CYJavaLocal :
    CYJavaDelete<Value_, &JNIEnv::DeleteLocalRef>
{
    typedef CYJavaDelete<Value_, &JNIEnv::DeleteLocalRef> CYJavaBase;

    CYJavaLocal() :
        CYJavaBase(NULL, NULL)
    {
    }

    CYJavaLocal(JNIEnv *jni, Value_ value) :
        CYJavaBase(jni, value)
    {
    }

    template <typename Other_>
    CYJavaLocal(const CYJavaRef<Other_> &other) :
        CYJavaLocal(other.jni_, static_cast<Other_>(other.jni_->NewLocalRef(other.value_)))
    {
    }

    template <typename Other_>
    CYJavaLocal(CYJavaRef<Other_> &&other) :
        CYJavaLocal(other.jni_, other.value_)
    {
        other.value_ = NULL;
    }

    CYJavaLocal(CYJavaLocal &&other) :
        CYJavaLocal(static_cast<CYJavaRef<Value_> &&>(other))
    {
    }

    template <typename Other_>
    CYJavaLocal &operator =(CYJavaLocal<Other_> &&other) {
        this->clear();
        this->jni_ = other.jni_;
        this->value_ = other.value_;
        other.value_ = NULL;
        return *this;
    }
};
// }}}
// Java Strings {{{
static CYJavaLocal<jstring> CYCastJavaString(const CYJavaRef<jobject> &value);

class CYJavaUTF8String :
    public CYUTF8String
{
  private:
    const CYJavaRef<jstring> *value_;

  public:
    CYJavaUTF8String(const CYJavaRef<jstring> &value) :
        value_(&value)
    {
        _assert(value);
        JNIEnv *jni(value.jni());
        size = jni->GetStringUTFLength(value);
        data = jni->GetStringUTFChars(value, NULL);
    }

    CYJavaUTF8String(const CYJavaRef<jobject> &value) :
        CYJavaUTF8String(CYCastJavaString(value))
    {
    }

    ~CYJavaUTF8String() {
        if (value_ != NULL) {
            JNIEnv *jni(value_->jni());
            jni->ReleaseStringUTFChars(*value_, data);
        }
    }

    CYJavaUTF8String(const CYJavaUTF8String &) = delete;

    CYJavaUTF8String(CYJavaUTF8String &&rhs) :
        value_(rhs.value_)
    {
        rhs.value_ = NULL;
    }
};

CYJavaUTF8String CYCastUTF8String(const CYJavaRef<jstring> &value) {
    return CYJavaUTF8String(value);
}

JSStringRef CYCopyJSString(const CYJavaRef<jstring> &value) {
    return CYCopyJSString(CYCastUTF8String(value));
}
// }}}
// Java Error {{{
struct CYJavaError :
    CYException
{
    CYJavaGlobal<jthrowable> value_;

    CYJavaError(const CYJavaRef<jthrowable> &value) :
        value_(value)
    {
    }

    virtual const char *PoolCString(CYPool &pool) const {
        return CYPoolCString(pool, CYJavaUTF8String(value_.cast<jobject>()));
    }

    virtual JSValueRef CastJSValue(JSContextRef context, const char *name) const;
};
// }}}

struct CYJavaFrame {
    JNIEnv *jni_;

    CYJavaFrame(JNIEnv *jni, jint capacity) :
        jni_(jni)
    {
        _assert(jni->PushLocalFrame(capacity) == 0);
    }

    ~CYJavaFrame() {
        operator ()(NULL);
    }

    operator JNIEnv *() const {
        return jni_;
    }

    jobject operator ()(jobject object) {
        JNIEnv *jni(jni_);
        jni_ = NULL;
        return jni->PopLocalFrame(object);
    }
};

struct CYJavaEnv {
  private:
    JNIEnv *jni;

  public:
    CYJavaEnv(JNIEnv *jni) :
        jni(jni)
    {
    }

    template <typename Other_>
    CYJavaEnv(const CYJavaRef<Other_> &value) :
        jni(value.jni())
    {
    }

    operator JNIEnv *() const {
        return jni;
    }

    JNIEnv *operator ->() const {
        return jni;
    }

    CYJavaLocal<jclass> FindClass(const char *name) const {
        return {jni, _envcall(jni, FindClass(name))};
    }

    CYJavaLocal<jclass> GetObjectClass(jobject object) const {
        return {jni, _envcall(jni, GetObjectClass(object))};
    }

    CYJavaLocal<jclass> GetSuperclass(jclass _class) const {
        return {jni, _envcall(jni, GetSuperclass(_class))};
    }

    CYJavaLocal<jobject> NewObject(jclass _class, jmethodID method, ...) const {
        va_list args;
        va_start(args, method);
        jobject object(_envcall(jni, NewObjectV(_class, method, args)));
        va_end(args);
        return {jni, object};
    }

    CYJavaLocal<jobject> NewObjectA(jclass _class, jmethodID method, jvalue *args) const {
        return {jni, _envcall(jni, NewObjectA(_class, method, args))};
    }

    CYJavaLocal<jstring> NewString(const jchar *data, jsize size) const {
        return {jni, _envcall(jni, NewString(data, size))};
    }

#define CYJavaEnv_(Code) \
    template <typename... Args_> \
    void Code(Args_ &&... args) const { \
        _envcallv(jni, Code(cy::Forward<Args_>(args)...)); \
    }

#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
    CYJavaEnv_(Get ## Typ ## ArrayRegion) \
    CYJavaEnv_(Set ## Typ ## Field) \
    CYJavaEnv_(SetStatic ## Typ ## Field)
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_

    CYJavaEnv_(CallVoidMethod)
    CYJavaEnv_(CallStaticVoidMethod)
    CYJavaEnv_(CallVoidMethodA)
    CYJavaEnv_(CallStaticVoidMethodA)
    CYJavaEnv_(SetObjectArrayElement)
    CYJavaEnv_(SetObjectField)
    CYJavaEnv_(SetStaticObjectField)
#undef CYJavaEnv_

#define CYJavaEnv_(Code) \
    template <typename... Args_> \
    auto Code(Args_ &&... args) const -> decltype(jni->Code(cy::Forward<Args_>(args)...)) { \
        return _envcall(jni, Code(cy::Forward<Args_>(args)...)); \
    }

#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
    CYJavaEnv_(Call ## Typ ## Method) \
    CYJavaEnv_(CallStatic ## Typ ## Method) \
    CYJavaEnv_(Call ## Typ ## MethodA) \
    CYJavaEnv_(CallStatic ## Typ ## MethodA) \
    CYJavaEnv_(Get ## Typ ## Field) \
    CYJavaEnv_(GetStatic ## Typ ## Field)
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_

    CYJavaEnv_(FromReflectedField)
    CYJavaEnv_(FromReflectedMethod)
    CYJavaEnv_(GetArrayLength)
    CYJavaEnv_(GetMethodID)
    CYJavaEnv_(GetStaticMethodID)
    CYJavaEnv_(IsSameObject)
    CYJavaEnv_(RegisterNatives)
#undef CYJavaEnv_

#define CYJavaEnv_(Code) \
    template <typename Other_, typename... Args_> \
    auto Code(Args_ &&... args) const -> CYJavaLocal<Other_> { \
        return {jni, static_cast<Other_>(_envcall(jni, Code(cy::Forward<Args_>(args)...)))}; \
    }

    CYJavaEnv_(CallObjectMethod)
    CYJavaEnv_(CallStaticObjectMethod)
    CYJavaEnv_(CallObjectMethodA)
    CYJavaEnv_(CallStaticObjectMethodA)
    CYJavaEnv_(GetObjectArrayElement)
    CYJavaEnv_(GetObjectField)
    CYJavaEnv_(GetStaticObjectField)
#undef CYJavaEnv_
};

static CYJavaLocal<jstring> CYCastJavaString(const CYJavaRef<jobject> &value) {
    CYJavaEnv jni(value);
    auto Object$(jni.FindClass("java/lang/Object"));
    auto Object$toString(jni.GetMethodID(Object$, "toString", "()Ljava/lang/String;"));
    return jni.CallObjectMethod<jstring>(value, Object$toString);
}

template <typename Internal_, typename Value_>
struct CYJavaValue :
    CYPrivate<Internal_>
{
    CYJavaGlobal<Value_> value_;

    CYJavaValue(const CYJavaRef<Value_> &value) :
        value_(value)
    {
    }

    CYJavaValue(const CYJavaValue &) = delete;
};

static JSValueRef CYCastJSValue(JSContextRef context, const CYJavaRef<jobject> &value);

template <typename Other_>
static _finline JSValueRef CYCastJSValue(JSContextRef context, const CYJavaRef<Other_> &value) {
    return CYCastJSValue(context, value.template cast<jobject>());
}

template <typename Type_>
static _finline JSValueRef CYJavaCastJSValue(JSContextRef context, Type_ value) {
    return CYCastJSValue(context, value);
}

static _finline JSValueRef CYJavaCastJSValue(JSContextRef context, jboolean value) {
    return CYCastJSValue(context, static_cast<bool>(value));
}

JSValueRef CYJavaError::CastJSValue(JSContextRef context, const char *name) const {
    return CYCastJSValue(context, value_);
}

static std::map<std::string, CYJavaPrimitive> Primitives_;

static CYJavaPrimitive CYJavaGetPrimitive(JSContextRef context, const CYJavaRef<jclass> &type, jmethodID Class$get$$Name) {
    CYJavaEnv jni(type);
    auto string(jni.CallObjectMethod<jstring>(type, Class$get$$Name));
    _assert(string);

    CYJavaUTF8String name(string);
    auto primitive(Primitives_.find(name));
    return primitive != Primitives_.end() ? primitive->second : CYJavaPrimitiveObject;
}

typedef std::vector<CYJavaPrimitive> CYJavaShorty;

static CYJavaShorty CYJavaGetShorty(JSContextRef context, const CYJavaRef<jobjectArray> &types, jmethodID Class$get$$Name) {
    CYJavaEnv jni(types);
    size_t count(jni.GetArrayLength(types));
    CYJavaShorty shorty(count);
    for (size_t index(0); index != count; ++index)
        shorty[index] = CYJavaGetPrimitive(context, jni.GetObjectArrayElement<jclass>(types, index), Class$get$$Name);
    return shorty;
}

struct CYJavaField {
    jfieldID field_;
    CYJavaPrimitive primitive_;
};

typedef std::map<std::string, CYJavaField> CYJavaFieldMap;

struct CYJavaSignature {
    CYJavaGlobal<jobject> reflected_;
    jmethodID method_;
    CYJavaPrimitive primitive_;
    CYJavaShorty shorty_;

    CYJavaSignature(const CYJavaRef<jobject> &reflected, jmethodID method, CYJavaPrimitive primitive, const CYJavaShorty &shorty) :
        reflected_(reflected),
        method_(method),
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
    CYJavaOverload overload_;

    CYJavaMethod(const CYJavaOverload &overload) :
        overload_(overload)
    {
    }
};

struct CYJavaStaticMethod :
    CYPrivate<CYJavaStaticMethod>
{
    CYJavaOverload overload_;

    CYJavaStaticMethod(const CYJavaOverload &overload) :
        overload_(overload)
    {
    }
};

struct CYJavaClass :
    CYJavaValue<CYJavaClass, jclass>
{
    bool interface_;

    CYJavaFieldMap static_;
    CYJavaFieldMap instance_;
    CYJavaOverload overload_;

    CYJavaClass(const CYJavaRef<jclass> &value, bool interface) :
        CYJavaValue(value),
        interface_(interface)
    {
    }
};

static JSObjectRef CYGetJavaClass(JSContextRef context, const CYJavaRef<jclass> &_class);

struct CYJavaObject :
    CYJavaValue<CYJavaObject, jobject>
{
    CYJavaClass *table_;

    CYJavaObject(const CYJavaRef<jobject> &value, CYJavaClass *table) :
        CYJavaValue(value),
        table_(table)
    {
    }

    JSValueRef GetPrototype(JSContextRef context) const;
};

struct CYJavaInterior :
    CYJavaValue<CYJavaInterior, jobject>
{
    CYJavaClass *table_;

    CYJavaInterior(const CYJavaRef<jobject> &value, CYJavaClass *table) :
        CYJavaValue(value),
        table_(table)
    {
    }
};

struct CYJavaStaticInterior :
    CYJavaValue<CYJavaStaticInterior, jclass>
{
    CYJavaClass *table_;

    CYJavaStaticInterior(const CYJavaRef<jclass> &value, CYJavaClass *table) :
        CYJavaValue(value),
        table_(table)
    {
    }
};

struct CYJavaArray :
    CYJavaValue<CYJavaArray, jarray>
{
    CYJavaPrimitive primitive_;

    CYJavaArray(const CYJavaRef<jarray> &value, CYJavaPrimitive primitive) :
        CYJavaValue(value),
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
    CYJavaEnv jni(value_);
    return CYGetProperty(context, CYGetJavaClass(context, jni.GetObjectClass(value_)), prototype_s);
}

JSValueRef CYJavaArray::GetPrototype(JSContextRef context) const {
    return CYGetCachedObject(context, CYJSString("Array_prototype"));
}

static JSValueRef CYCastJSValue(JSContextRef context, const CYJavaRef<jobject> &value) {
    if (!value)
        return CYJSNull(context);
    CYJavaEnv jni(value);

    auto _class(jni.GetObjectClass(value));
    if (jni.IsSameObject(_class, jni.FindClass("java/lang/String")))
        return CYCastJSValue(context, CYJSString(value.cast<jstring>()));

    auto Class$(jni.FindClass("java/lang/Class"));
    auto Class$isArray(jni.GetMethodID(Class$, "isArray", "()Z"));
    if (jni.CallBooleanMethod(_class, Class$isArray)) {
        auto Class$getComponentType(jni.GetMethodID(Class$, "getComponentType", "()Ljava/lang/Class;"));
        auto component(jni.CallObjectMethod<jclass>(_class, Class$getComponentType));
        auto Class$getName(jni.GetMethodID(Class$, "getName", "()Ljava/lang/String;"));
        return CYJavaArray::Make(context, value.cast<jarray>(), CYJavaGetPrimitive(context, component, Class$getName));
    }

    auto Wrapper$(jni.FindClass("Cycript$Wrapper"));
    if (jni.IsSameObject(_class, Wrapper$)) {
        auto Wrapper$getProtect(jni.GetMethodID(Wrapper$, "getProtect", "()J"));
        auto &protect(*reinterpret_cast<CYProtect *>(jni.CallLongMethod(value, Wrapper$getProtect)));
        return protect;
    }

    CYJavaClass *table(reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(CYGetJavaClass(context, _class))));
    return CYJavaObject::Make(context, value, table);
}

static _finline JSObjectRef CYCastJSObject(JSContextRef context, const CYJavaRef<jobject> &value) {
    return CYCastJSObject(context, CYCastJSValue(context, value));
}

static CYJavaLocal<jstring> CYCastJavaString(const CYJavaEnv &jni, JSContextRef context, CYUTF16String value) {
    return jni.NewString(value.data, value.size);
}

static CYJavaLocal<jstring> CYCastJavaString(const CYJavaEnv &jni, JSContextRef context, JSStringRef value) {
    return CYCastJavaString(jni, context, CYCastUTF16String(value));
}

#define CYCastJava$(T, Type, jtype, Cast) \
_disused static CYJavaLocal<jobject> CYCastJava ## Type(const CYJavaEnv &jni, JSContextRef context, JSValueRef value) { \
    auto Type$(jni.FindClass("java/lang/" #Type)); \
    auto Type$init$(jni.GetMethodID(Type$, "<init>", "(" #T ")V")); \
    return jni.NewObject(Type$, Type$init$, static_cast<jtype>(Cast(context, value))); \
}

CYCastJava$(Z, Boolean, jboolean, CYCastBool)
CYCastJava$(B, Byte, jbyte, CYCastDouble)
CYCastJava$(C, Character, jchar, CYCastDouble)
CYCastJava$(S, Short, jshort, CYCastDouble)
CYCastJava$(I, Integer, jint, CYCastDouble)
CYCastJava$(J, Long, jlong, CYCastDouble)
CYCastJava$(F, Float, jfloat, CYCastDouble)
CYCastJava$(D, Double, jdouble, CYCastDouble)

static CYJavaClass *CYGetJavaTable(JSContextRef context, JSObjectRef object) {
    if (!JSValueIsObjectOfClass(context, object, CYJavaClass::Class_))
        return NULL;
    return reinterpret_cast<CYJavaClass *>(JSObjectGetPrivate(object));
}

static CYJavaObject *CYGetJavaObject(JSContextRef context, JSObjectRef object) {
    if (!JSValueIsObjectOfClass(context, object, CYJavaObject::Class_))
        return NULL;
    return reinterpret_cast<CYJavaObject *>(JSObjectGetPrivate(object));
}

static CYJavaLocal<jobject> CYCastJavaObject(const CYJavaEnv &jni, JSContextRef context, JSObjectRef value) {
    if (CYJavaObject *internal = CYGetJavaObject(context, value))
        return internal->value_;

    auto Wrapper$(jni.FindClass("Cycript$Wrapper"));
    auto Wrapper$$init$(jni.GetMethodID(Wrapper$, "<init>", "(J)V"));
    CYProtect *protect(new CYProtect(context, value));
    return jni.NewObject(Wrapper$, Wrapper$$init$, reinterpret_cast<jlong>(protect));
}

static CYJavaLocal<jobject> CYCastJavaObject(const CYJavaEnv &jni, JSContextRef context, JSValueRef value) {
    switch (JSValueGetType(context, value)) {
        case kJSTypeNull:
            return {jni, NULL};
        case kJSTypeBoolean:
            return CYCastJavaBoolean(jni, context, value);
        case kJSTypeNumber:
            return CYCastJavaDouble(jni, context, value);
        case kJSTypeString:
            return CYCastJavaString(jni, context, CYJSString(context, value));
        case kJSTypeObject:
            return CYCastJavaObject(jni, context, CYCastJSObject(context, value));

        case kJSTypeUndefined:
            // XXX: I am currently relying on this for dynamic proxy of void method
            return {jni, NULL};
        default:
            _assert(false);
    }
}

static JSObjectRef CYGetJavaClass(JSContextRef context, const CYJavaRef<jclass> &value) {
    CYJavaEnv jni(value);
    CYJavaFrame frame(jni, 64);

    JSObjectRef global(CYGetGlobalObject(context));
    JSObjectRef cy(CYCastJSObject(context, CYGetProperty(context, global, cy_s)));

    auto Class$(jni.FindClass("java/lang/Class"));
    auto Class$getName(jni.GetMethodID(Class$, "getName", "()Ljava/lang/String;"));

    CYJSString name(jni.CallObjectMethod<jstring>(value, Class$getName));
    JSValueRef cached(CYGetProperty(context, cy, name));
    if (!JSValueIsUndefined(context, cached))
        return CYCastJSObject(context, cached);

    JSObjectRef constructor;
    JSObjectRef prototype;

    {

    auto Class$isInterface(jni.GetMethodID(Class$, "isInterface", "()Z"));

    auto Class$getDeclaredConstructors(jni.GetMethodID(Class$, "getDeclaredConstructors", "()[Ljava/lang/reflect/Constructor;"));
    auto Class$getDeclaredFields(jni.GetMethodID(Class$, "getDeclaredFields", "()[Ljava/lang/reflect/Field;"));
    auto Class$getDeclaredMethods(jni.GetMethodID(Class$, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;"));

    auto Constructor$(jni.FindClass("java/lang/reflect/Constructor"));
    //auto Constructor$getModifiers(jni.GetMethodID(Constructor$, "getModifiers", "()I"));
    auto Constructor$getParameterTypes(jni.GetMethodID(Constructor$, "getParameterTypes", "()[Ljava/lang/Class;"));

    auto Field$(jni.FindClass("java/lang/reflect/Field"));
    auto Field$getModifiers(jni.GetMethodID(Field$, "getModifiers", "()I"));
    auto Field$getName(jni.GetMethodID(Field$, "getName", "()Ljava/lang/String;"));
    auto Field$getType(jni.GetMethodID(Field$, "getType", "()Ljava/lang/Class;"));

    auto Method$(jni.FindClass("java/lang/reflect/Method"));
    auto Method$getModifiers(jni.GetMethodID(Method$, "getModifiers", "()I"));
    auto Method$getName(jni.GetMethodID(Method$, "getName", "()Ljava/lang/String;"));
    auto Method$getParameterTypes(jni.GetMethodID(Method$, "getParameterTypes", "()[Ljava/lang/Class;"));
    auto Method$getReturnType(jni.GetMethodID(Method$, "getReturnType", "()Ljava/lang/Class;"));

    auto Modifier$(jni.FindClass("java/lang/reflect/Modifier"));
    auto Modifier$isStatic(jni.GetStaticMethodID(Modifier$, "isStatic", "(I)Z"));

    auto interface(jni.CallBooleanMethod(value, Class$isInterface));
    auto table(new CYJavaClass(value, interface));

    for (CYJavaLocal<jclass> prototype(value); prototype; prototype = jni.GetSuperclass(prototype)) {
        auto fields(jni.CallObjectMethod<jobjectArray>(prototype, Class$getDeclaredFields));

        for (jsize i(0), e(jni.GetArrayLength(fields)); i != e; ++i) {
            auto field(jni.GetObjectArrayElement<jobject>(fields, e - i - 1));
            auto modifiers(jni.CallIntMethod(field, Field$getModifiers));
            auto instance(!jni.CallStaticBooleanMethod(Modifier$, Modifier$isStatic, modifiers));
            auto &map(instance ? table->instance_ : table->static_);
            CYJavaUTF8String name(jni.CallObjectMethod<jstring>(field, Field$getName));
            auto id(jni.FromReflectedField(field));
            auto type(jni.CallObjectMethod<jclass>(field, Field$getType));
            map.insert(std::make_pair(std::string(name), CYJavaField{id, CYJavaGetPrimitive(context, type, Class$getName)}));
        }
    }

    constructor = JSObjectMake(context, CYJavaClass::Class_, table);

    prototype = JSObjectMake(context, NULL, NULL);
    CYSetProperty(context, constructor, prototype_s, prototype, kJSPropertyAttributeDontEnum);

    auto constructors(jni.CallObjectMethod<jobjectArray>(value, Class$getDeclaredConstructors));

    for (jsize i(0), e(jni.GetArrayLength(constructors)); i != e; ++i) {
        auto constructor(jni.GetObjectArrayElement<jobject>(constructors, i));
        auto parameters(jni.CallObjectMethod<jobjectArray>(constructor, Constructor$getParameterTypes));
        CYJavaShorty shorty(CYJavaGetShorty(context, parameters, Class$getName));
        auto id(jni.FromReflectedMethod(constructor));
        table->overload_.insert(CYJavaSignature(constructor, id, CYJavaPrimitiveObject, shorty));
    }

    auto methods(jni.CallObjectMethod<jobjectArray>(value, Class$getDeclaredMethods));

    std::map<std::pair<bool, std::string>, CYJavaOverload> entries;

    for (jsize i(0), e(jni.GetArrayLength(methods)); i != e; ++i) {
        auto method(jni.GetObjectArrayElement<jobject>(methods, i));
        auto modifiers(jni.CallIntMethod(method, Method$getModifiers));
        auto instance(!jni.CallStaticBooleanMethod(Modifier$, Modifier$isStatic, modifiers));
        CYJavaUTF8String name(jni.CallObjectMethod<jstring>(method, Method$getName));
        auto parameters(jni.CallObjectMethod<jobjectArray>(method, Method$getParameterTypes));
        CYJavaShorty shorty(CYJavaGetShorty(context, parameters, Class$getName));
        auto type(jni.CallObjectMethod<jclass>(method, Method$getReturnType));
        auto primitive(CYJavaGetPrimitive(context, type, Class$getName));
        auto id(jni.FromReflectedMethod(method));
        entries[std::make_pair(instance, std::string(name))].insert(CYJavaSignature(method, id, primitive, shorty));
    }

    for (const auto &entry : entries) {
        bool instance(entry.first.first);
        CYJSString name(entry.first.second);
        auto &overload(entry.second);
        if (instance)
            CYSetProperty(context, prototype, name, CYJavaMethod::Make(context, overload), kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete);
        else
            CYSetProperty(context, constructor, name, CYJavaStaticMethod::Make(context, overload), kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete);
    }

    }

    // XXX: for some reason kJSPropertyAttributeDontEnum doesn't work if there's already a property with the same name
    // by not linking the prototypes until after we set the properties, we hide the parent property from this issue :(

    if (auto super = jni.GetSuperclass(value)) {
        JSObjectRef parent(CYGetJavaClass(context, super));
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

static bool CYCastJavaArguments(const CYJavaFrame &frame, const CYJavaShorty &shorty, JSContextRef context, const JSValueRef arguments[], jvalue *array) {
    CYJavaEnv jni(frame);

    for (size_t index(0); index != shorty.size(); ++index) {
        JSValueRef argument(arguments[index]);
        JSType type(JSValueGetType(context, argument));
        jvalue &value(array[index]);

        switch (CYJavaPrimitive primitive = shorty[index]) {
            case CYJavaPrimitiveObject:
                // XXX: figure out a way to tie this in to the CYJavaFrame
                value.l = CYCastJavaObject(jni, context, argument).leak();
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
    auto internal(CYJavaMethod::Get(context, object));
    CYJavaObject *self(CYGetJavaObject(context, _this));
    _assert(self != NULL);
    CYJavaEnv jni(self->value_);

    CYJavaSignature bound(count);
    for (auto overload(internal->overload_.lower_bound(bound)), e(internal->overload_.upper_bound(bound)); overload != e; ++overload) {
        CYJavaFrame frame(jni, count + 16);
        jvalue array[count];
        if (!CYCastJavaArguments(frame, overload->shorty_, context, arguments, array))
            continue;
        jvalue *values(array);
        switch (overload->primitive_) {
            case CYJavaPrimitiveObject:
                return CYCastJSValue(context, jni.CallObjectMethodA<jobject>(self->value_, overload->method_, values));
            case CYJavaPrimitiveVoid:
                jni.CallVoidMethodA(self->value_, overload->method_, values);
                return CYJSUndefined(context);
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
            case CYJavaPrimitive ## Type: \
                return CYJavaCastJSValue(context, jni.Call ## Typ ## MethodA(self->value_, overload->method_, values));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
            default: _assert(false);
        }
    }

    CYThrow("invalid method call");
} CYCatch(NULL) }

static JSValueRef JavaStaticMethod_callAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    auto internal(CYJavaStaticMethod::Get(context, object));
    CYJavaClass *table(CYGetJavaTable(context, _this));
    CYJavaEnv jni(table->value_);

    CYJavaSignature bound(count);
    for (auto overload(internal->overload_.lower_bound(bound)), e(internal->overload_.upper_bound(bound)); overload != e; ++overload) {
        CYJavaFrame frame(jni, count + 16);
        jvalue array[count];
        if (!CYCastJavaArguments(frame, overload->shorty_, context, arguments, array))
            continue;
        jvalue *values(array);
        switch (overload->primitive_) {
            case CYJavaPrimitiveObject:
                return CYCastJSValue(context, jni.CallStaticObjectMethodA<jobject>(table->value_, overload->method_, values));
            case CYJavaPrimitiveVoid:
                jni.CallStaticVoidMethodA(table->value_, overload->method_, values);
                return CYJSUndefined(context);
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
            case CYJavaPrimitive ## Type: \
                return CYJavaCastJSValue(context, jni.CallStatic ## Typ ## MethodA(table->value_, overload->method_, values));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
            default: _assert(false);
        }
    }

    CYThrow("invalid method call");
} CYCatch(NULL) }

static JSObjectRef JavaClass_callAsConstructor(JSContextRef context, JSObjectRef object, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    auto table(CYJavaClass::Get(context, object));
    CYJavaEnv jni(table->value_);
    jclass _class(table->value_);

    if (table->interface_ && count == 1) {
        auto Cycript$(jni.FindClass("Cycript"));
        auto Cycript$Make(jni.GetStaticMethodID(Cycript$, "proxy", "(Ljava/lang/Class;LCycript$Wrapper;)Ljava/lang/Object;"));
        return CYCastJSObject(context, jni.CallObjectMethod<jobject>(Cycript$, Cycript$Make, _class, CYCastJavaObject(jni, context, CYCastJSObject(context, arguments[0])).get()));
    }

    CYJavaSignature bound(count);
    for (auto overload(table->overload_.lower_bound(bound)), e(table->overload_.upper_bound(bound)); overload != e; ++overload) {
        CYJavaFrame frame(jni, count + 16);
        jvalue array[count];
        if (!CYCastJavaArguments(frame, overload->shorty_, context, arguments, array))
            continue;
        jvalue *values(array);
        auto object(jni.NewObjectA(_class, overload->method_, values));
        return CYCastJSObject(context, object);
    }

    CYThrow("invalid constructor call");
} CYCatch(NULL) }

static bool JavaStaticInterior_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    auto internal(CYJavaStaticInterior::Get(context, object));
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return false;
    return true;
}

static JSValueRef JavaStaticInterior_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYJavaStaticInterior::Get(context, object));
    CYJavaClass *table(internal->table_);
    CYJavaEnv jni(table->value_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return NULL;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            return CYCastJSValue(context, jni.GetStaticObjectField<jobject>(table->value_, field->second.field_));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            return CYJavaCastJSValue(context, jni.GetStatic ## Typ ## Field(table->value_, field->second.field_));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaStaticInterior_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    auto internal(CYJavaStaticInterior::Get(context, object));
    CYJavaClass *table(internal->table_);
    CYJavaEnv jni(table->value_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->static_.find(name));
    if (field == table->static_.end())
        return false;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            jni.SetStaticObjectField(table->value_, field->second.field_, CYCastJavaObject(jni, context, value));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            jni.SetStatic ## Typ ## Field(table->value_, field->second.field_, CYCastDouble(context, value)); \
            break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static void JavaStaticInterior_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    auto internal(CYJavaStaticInterior::Get(context, object));
    CYJavaClass *table(internal->table_);
    for (const auto &field : table->static_)
        JSPropertyNameAccumulatorAddName(names, CYJSString(field.first));
}

static JSValueRef JavaClass_getProperty_class(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto table(CYJavaClass::Get(context, object));
    return CYCastJSValue(context, table->value_);
} CYCatch(NULL) }

static bool JavaInterior_hasProperty(JSContextRef context, JSObjectRef object, JSStringRef property) {
    auto internal(CYJavaInterior::Get(context, object));
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->instance_.find(name));
    if (field == table->instance_.end())
        return false;
    return true;
}

static JSValueRef JavaInterior_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYJavaInterior::Get(context, object));
    CYJavaEnv jni(internal->value_);
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->instance_.find(name));
    if (field == table->instance_.end())
        return NULL;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            return CYCastJSValue(context, jni.GetObjectField<jobject>(internal->value_, field->second.field_));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            return CYJavaCastJSValue(context, jni.Get ## Typ ## Field(internal->value_, field->second.field_));
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaInterior_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    auto internal(CYJavaInterior::Get(context, object));
    CYJavaEnv jni(internal->value_);
    CYJavaClass *table(internal->table_);
    CYPool pool;
    auto name(CYPoolUTF8String(pool, context, property));
    auto field(table->instance_.find(name));
    if (field == table->instance_.end())
        return false;

    switch (field->second.primitive_) {
        case CYJavaPrimitiveObject:
            jni.SetObjectField(table->value_, field->second.field_, CYCastJavaObject(jni, context, value));
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: \
            jni.Set ## Typ ## Field(table->value_, field->second.field_, CYCastDouble(context, value)); \
            break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static void JavaInterior_getPropertyNames(JSContextRef context, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    auto internal(CYJavaInterior::Get(context, object));
    CYJavaClass *table(internal->table_);
    for (const auto &field : table->instance_)
        JSPropertyNameAccumulatorAddName(names, CYJSString(field.first));
}

static JSValueRef JavaObject_getProperty_constructor(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYJavaObject::Get(context, object));
    CYJavaEnv jni(internal->value_);
    return CYGetJavaClass(context, jni.GetObjectClass(internal->value_));
} CYCatch(NULL) }

static JSValueRef JavaClass_getProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYJavaClass::Get(context, object));
    return CYJavaStaticInterior::Make(context, internal->value_, internal);
} CYCatch(NULL) }

static JSValueRef JavaObject_getProperty_$cyi(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYJavaObject::Get(context, object));
    return CYJavaInterior::Make(context, internal->value_, internal->table_);
} CYCatch(NULL) }

static JSValueRef JavaClass_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    auto internal(CYJavaClass::Get(context, _this));
    CYJavaEnv jni(internal->value_);
    auto Class$(jni.FindClass("java/lang/Class"));
    auto Class$getCanonicalName(jni.GetMethodID(Class$, "getCanonicalName", "()Ljava/lang/String;"));
    return CYCastJSValue(context, CYJSString(jni.CallObjectMethod<jstring>(internal->value_, Class$getCanonicalName)));
} CYCatch(NULL) }

static JSValueRef JavaMethod_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    std::ostringstream cyon;
    return CYCastJSValue(context, CYJSString(cyon.str()));
} CYCatch(NULL) }

static JSValueRef JavaStaticMethod_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    std::ostringstream cyon;
    return CYCastJSValue(context, CYJSString(cyon.str()));
} CYCatch(NULL) }

static JSValueRef JavaArray_getProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef *exception) { CYTry {
    auto internal(CYJavaArray::Get(context, object));
    CYJavaEnv jni(internal->value_);
    if (JSStringIsEqual(property, length_s))
        return CYCastJSValue(context, jni.GetArrayLength(internal->value_));

    CYPool pool;
    ssize_t offset;
    if (!CYGetOffset(pool, context, property, offset))
        return NULL;

    if (internal->primitive_ == CYJavaPrimitiveObject)
        return CYCastJSValue(context, jni.GetObjectArrayElement<jobject>(static_cast<jobjectArray>(internal->value_.value_), offset));
    else switch (internal->primitive_) {
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: { \
            j ## type element; \
            jni.Get ## Typ ## ArrayRegion(static_cast<j ## type ## Array>(internal->value_.value_), offset, 1, &element); \
            return CYJavaCastJSValue(context, element); \
        } break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }
} CYCatch(NULL) }

static bool JavaArray_setProperty(JSContextRef context, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef *exception) { CYTry {
    auto internal(CYJavaArray::Get(context, object));
    CYJavaEnv jni(internal->value_);

    CYPool pool;
    ssize_t offset;
    if (!CYGetOffset(pool, context, property, offset))
        return false;

    if (internal->primitive_ == CYJavaPrimitiveObject)
        jni.SetObjectArrayElement(static_cast<jobjectArray>(internal->value_.value_), offset, CYCastJavaObject(jni, context, value));
    else switch (internal->primitive_) {
#define CYJavaForEachPrimitive_(T, t, Typ, Type, type) \
        case CYJavaPrimitive ## Type: { \
            j ## type element; \
            jni.Get ## Typ ## ArrayRegion(static_cast<j ## type ## Array>(internal->value_.value_), offset, 1, &element); \
            return CYJavaCastJSValue(context, element); \
        } break;
CYJavaForEachPrimitive
#undef CYJavaForEachPrimitive_
        default: _assert(false);
    }

    return true;
} CYCatch(false) }

static JSValueRef JavaPackage_callAsFunction_toCYON(JSContextRef context, JSObjectRef object, JSObjectRef _this, size_t count, const JSValueRef arguments[], JSValueRef *exception) { CYTry {
    auto internal(CYJavaPackage::Get(context, _this));
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
    auto internal(CYJavaPackage::Get(context, object));
    CYJavaPackage::Path package(internal->package_);

    CYPool pool;
    const char *next(CYPoolCString(pool, context, property));

    std::ostringstream name;
    for (auto &package : internal->package_)
        name << package << '/';
    name << next;

    JNIEnv *jni(GetJNI(context));
    if (auto _class = jni->FindClass(name.str().c_str()))
        return CYGetJavaClass(context, CYJavaLocal<jclass>(jni, _class));
    jni->ExceptionClear();

    package.push_back(next);
    return CYJavaPackage::Make(context, package);
} CYCatch(NULL) }

static void Cycript_delete(JNIEnv *env, jclass api, jlong jprotect) { CYJavaTry {
    delete &protect;
} CYJavaCatch() }

static jobject Cycript_handle(JNIEnv *env, jclass api, jlong jprotect, jstring property, jobjectArray jarguments) { CYJavaTry {
    JSValueRef function(CYGetProperty(context, object, CYJSString(CYJavaRef<jstring>(jni, property))));
    if (JSValueIsUndefined(context, function))
        return NULL;

    size_t count(jarguments == NULL ? 0 : jni.GetArrayLength(jarguments));
    JSValueRef arguments[count];
    for (size_t index(0); index != count; ++index)
        arguments[index] = CYCastJSValue(context, jni.GetObjectArrayElement<jobject>(jarguments, index));

    return CYCastJavaObject(jni, context, CYCallAsFunction(context, CYCastJSObject(context, function), object, count, arguments)).leak();
} CYJavaCatch(NULL) }

static JNINativeMethod Cycript_[] = {
    {(char *) "delete", (char *) "(J)V", (void *) &Cycript_delete},
    {(char *) "handle", (char *) "(JLjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;", (void *) &Cycript_handle},
};

template <typename Type_>
static _finline void dlset(Type_ &function, const char *name, void *handle) {
    function = reinterpret_cast<Type_>(dlsym(handle, name));
}

jint CYJavaVersion(JNI_VERSION_1_4);

static JNIEnv *CYGetCreatedJava(jint (*$JNI_GetCreatedJavaVMs)(JavaVM **, jsize, jsize *)) {
    jsize capacity(16);
    JavaVM *jvms[capacity];
    jsize size;
    _jnicall($JNI_GetCreatedJavaVMs(jvms, capacity, &size));
    if (size == 0)
        return NULL;
    JavaVM *jvm(jvms[0]);
    JNIEnv *jni;
    _jnicall(jvm->GetEnv(reinterpret_cast<void **>(&jni), CYJavaVersion));
    return jni;
}

static JNIEnv *GetJNI_(JSContextRef context) {
    static JavaVM *jvm(NULL);
    static JNIEnv *jni(NULL);

    if (jni != NULL)
        return jni;

    CYPool pool;
    void *handle(RTLD_DEFAULT);
    std::string library;

    jint (*$JNI_GetCreatedJavaVMs)(JavaVM **jvms, jsize capacity, jsize *size);
    dlset($JNI_GetCreatedJavaVMs, "JNI_GetCreatedJavaVMs", handle);

    if ($JNI_GetCreatedJavaVMs != NULL) {
        if (JNIEnv *jni = CYGetCreatedJava($JNI_GetCreatedJavaVMs))
            return jni;
    } else {
        std::vector<const char *> guesses;

#ifdef __ANDROID__
        char android[PROP_VALUE_MAX];
        if (__system_property_get("persist.sys.dalvik.vm.lib", android) != 0)
            guesses.push_back(android);
#endif

        guesses.push_back("libart.so");
        guesses.push_back("libdvm.so");
        guesses.push_back("libjvm.so");

        for (const char *guess : guesses) {
            handle = dlopen(guess, RTLD_LAZY | RTLD_GLOBAL);
            if (handle != NULL) {
                library = guess;
                break;
            }
        }

        _assert(library.size() != 0);

        dlset($JNI_GetCreatedJavaVMs, "JNI_GetCreatedJavaVMs", handle);
        if (JNIEnv *jni = CYGetCreatedJava($JNI_GetCreatedJavaVMs))
            return jni;
    }

    std::vector<JavaVMOption> options;

    {
        std::ostringstream option;
        option << "-Djava.class.path=";
        option << CYPoolLibraryPath(pool) << "/libcycript.jar";
        if (const char *classpath = getenv("CLASSPATH"))
            option << ':' << classpath;
        options.push_back(JavaVMOption{pool.strdup(option.str().c_str()), NULL});
    }

    // To use libnativehelper to access JNI_GetCreatedJavaVMs, you need JniInvocation.
    // ...but there can only be one JniInvocation, and assuradely the other VM has it.
    // Essentially, this API makes no sense. We need it for AndroidRuntime, though :/.

    if (void *libnativehelper = dlopen("libnativehelper.so", RTLD_LAZY | RTLD_GLOBAL)) {
        class JniInvocation$;
        JniInvocation$ *(*JniInvocation$$init$)(JniInvocation$ *self)(NULL);
        bool (*JniInvocation$Init)(JniInvocation$ *self, const char *library)(NULL);
        JniInvocation$ *(*JniInvocation$finalize)(JniInvocation$ *self)(NULL);

        dlset(JniInvocation$$init$, "_ZN13JniInvocationC1Ev", libnativehelper);
        dlset(JniInvocation$Init, "_ZN13JniInvocation4InitEPKc", libnativehelper);
        dlset(JniInvocation$finalize, "_ZN13JniInvocationD1Ev", libnativehelper);

        if (JniInvocation$$init$ == NULL)
            dlclose(libnativehelper);
        else {
            // XXX: we should attach a pool to the VM itself and deallocate this there
            //auto invocation(pool.calloc<JniInvocation$>(1, 1024));
            //_assert(JniInvocation$finalize != NULL);
            //pool.atexit(reinterpret_cast<void (*)(void *)>(JniInvocation$finalize), invocation);

            auto invocation(static_cast<JniInvocation$ *>(calloc(1, 1024)));
            JniInvocation$$init$(invocation);

            _assert(JniInvocation$Init != NULL);
            JniInvocation$Init(invocation, NULL);

            dlset($JNI_GetCreatedJavaVMs, "JNI_GetCreatedJavaVMs", libnativehelper);
            if (JNIEnv *jni = CYGetCreatedJava($JNI_GetCreatedJavaVMs))
                return jni;
        }
    }

    if (void *libandroid_runtime = dlopen("libandroid_runtime.so", RTLD_LAZY | RTLD_GLOBAL)) {
        class AndroidRuntime$;
        AndroidRuntime$ *(*AndroidRuntime$$init$)(AndroidRuntime$ *self, char *args, unsigned int size)(NULL);
        int (*AndroidRuntime$startVm)(AndroidRuntime$ *self, JavaVM **jvm, JNIEnv **jni)(NULL);
        int (*AndroidRuntime$startReg)(JNIEnv *jni)(NULL);
        int (*AndroidRuntime$addOption)(AndroidRuntime$ *self, const char *option, void *extra)(NULL);
        int (*AndroidRuntime$addVmArguments)(AndroidRuntime$ *self, int, const char *const argv[])(NULL);
        AndroidRuntime$ *(*AndroidRuntime$finalize)(AndroidRuntime$ *self)(NULL);

        dlset(AndroidRuntime$$init$, "_ZN7android14AndroidRuntimeC1EPcj", libandroid_runtime);
        dlset(AndroidRuntime$startVm, "_ZN7android14AndroidRuntime7startVmEPP7_JavaVMPP7_JNIEnv", libandroid_runtime);
        dlset(AndroidRuntime$startReg, "_ZN7android14AndroidRuntime8startRegEP7_JNIEnv", libandroid_runtime);
        dlset(AndroidRuntime$addOption, "_ZN7android14AndroidRuntime9addOptionEPKcPv", libandroid_runtime);
        dlset(AndroidRuntime$addVmArguments, "_ZN7android14AndroidRuntime14addVmArgumentsEiPKPKc", libandroid_runtime);
        dlset(AndroidRuntime$finalize, "_ZN7android14AndroidRuntimeD1Ev", libandroid_runtime);

        // XXX: it would also be interesting to attach this to a global pool
        AndroidRuntime$ *runtime(pool.calloc<AndroidRuntime$>(1, 1024));

        _assert(AndroidRuntime$$init$ != NULL);
        AndroidRuntime$$init$(runtime, NULL, 0);

        if (AndroidRuntime$addOption == NULL) {
            _assert(AndroidRuntime$addVmArguments != NULL);
            std::vector<const char *> arguments;
            for (const auto &option : options)
                arguments.push_back(option.optionString);
            AndroidRuntime$addVmArguments(runtime, arguments.size(), arguments.data());
        } else for (const auto &option : options)
            AndroidRuntime$addOption(runtime, option.optionString, option.extraInfo);

        int failure;

        _assert(AndroidRuntime$startVm != NULL);
        failure = AndroidRuntime$startVm(runtime, &jvm, &jni);
        _assert(failure == 0);

        _assert(AndroidRuntime$startReg != NULL);
        failure = AndroidRuntime$startReg(jni);
        _assert(failure == 0);

        return jni;
    }

    jint (*$JNI_CreateJavaVM)(JavaVM **jvm, void **, void *);
    dlset($JNI_CreateJavaVM, "JNI_CreateJavaVM", handle);

    JavaVMInitArgs args;
    memset(&args, 0, sizeof(args));
    args.version = CYJavaVersion;
    args.nOptions = options.size();
    args.options = options.data();
    _jnicall($JNI_CreateJavaVM(&jvm, reinterpret_cast<void **>(&jni), &args));
    return jni;
}

static JNIEnv *GetJNI(JSContextRef context) {
    CYJavaEnv jni(GetJNI_(context));
    auto Cycript$(jni.FindClass("Cycript"));
    jni.RegisterNatives(Cycript$, Cycript_, sizeof(Cycript_) / sizeof(Cycript_[0]));
    return jni;
}

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

static JSStaticFunction JavaStaticMethod_staticFunctions[2] = {
    {"toCYON", &JavaStaticMethod_callAsFunction_toCYON, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete},
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
    definition.className = "JavaStaticMethod";
    definition.staticFunctions = JavaStaticMethod_staticFunctions;
    definition.callAsFunction = &JavaStaticMethod_callAsFunction;
    definition.finalize = &CYFinalize;
    CYJavaStaticMethod::Class_ = JSClassCreate(&definition);

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
    definition.className = "JavaStaticInterior";
    definition.hasProperty = &JavaStaticInterior_hasProperty;
    definition.getProperty = &JavaStaticInterior_getProperty;
    definition.setProperty = &JavaStaticInterior_setProperty;
    definition.getPropertyNames = &JavaStaticInterior_getPropertyNames;
    definition.finalize = &CYFinalize;
    CYJavaStaticInterior::Class_ = JSClassCreate(&definition);
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
