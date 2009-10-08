#ifndef CYCRIPT_HPP
#define CYCRIPT_HPP

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#endif

#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSStringRefCF.h>

#include <apr-1/apr_pools.h>

JSGlobalContextRef CYGetJSContext();
const char *CYPoolJSONString(apr_pool_t *pool, JSContextRef context, JSValueRef value, JSValueRef *exception);
void CYSetArgs(int argc, const char *argv[]);

#endif/*CYCRIPT_HPP*/
