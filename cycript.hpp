#ifndef CYCRIPT_HPP
#define CYCRIPT_HPP

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#endif

#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSStringRefCF.h>

JSContextRef CYGetJSContext();
CFStringRef CYCopyJSONString(JSContextRef context, JSValueRef value);
void CYSetArgs(int argc, const char *argv[]);

#ifdef __OBJC__
void CYThrow(JSContextRef context, id error, JSValueRef *exception);
#endif

#endif/*CYCRIPT_HPP*/
