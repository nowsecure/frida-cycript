#ifndef CYCRIPT_H
#define CYCRIPT_H

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#endif

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>

JSContextRef CYGetJSContext();
CFStringRef CYCopyJSONString(JSContextRef context, JSValueRef value);

#ifdef __OBJC__
void CYThrow(JSContextRef context, id error, JSValueRef *exception);
#endif

#endif/*CYCRIPT_H*/
