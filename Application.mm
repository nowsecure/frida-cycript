#include <iostream>
#include <string>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFLogUtilities.h>

#include <Foundation/Foundation.h>

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>

#define _trace() do { \
    CFLog(kCFLogLevelNotice, CFSTR("_trace(%u)"), __LINE__); \
} while (false)

JSContextRef JSGetContext(void);
void CYThrow(JSContextRef context, id error, JSValueRef *exception);
CFStringRef JSValueToJSONCopy(JSContextRef context, JSValueRef value);

int main() {
    for (;;) {
        NSAutoreleasePool *pool([[NSAutoreleasePool alloc] init]);

        std::cout << ">>> " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line))
            break;

        JSStringRef script(JSStringCreateWithUTF8CString(line.c_str()));

        JSContextRef context(JSGetContext());

        JSValueRef exception(NULL);
        JSValueRef result(JSEvaluateScript(context, script, NULL, NULL, 0, &exception));
        JSStringRelease(script);

        if (exception != NULL)
            result = exception;

        if (!JSValueIsUndefined(context, result)) {
            CFStringRef json;

            @try { json:
                json = JSValueToJSONCopy(context, result);
            } @catch (id error) {
                CYThrow(context, error, &result);
                goto json;
            }

            std::cout << [reinterpret_cast<const NSString *>(json) UTF8String] << std::endl;
            CFRelease(json);
        }

        [pool release];
    }

    return 0;
}
