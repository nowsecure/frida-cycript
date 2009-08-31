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
CFStringRef JSValueToJSONCopy(JSContextRef ctx, JSValueRef value);

int main() {
    for (;;) {
        NSAutoreleasePool *pool([[NSAutoreleasePool alloc] init]);

        std::cout << ">>> " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line))
            break;

        JSStringRef script(JSStringCreateWithUTF8CString(line.c_str()));

        JSValueRef exception;
        JSValueRef result(JSEvaluateScript(JSGetContext(), script, NULL, NULL, 0, &exception));
        if (result == NULL)
            result = exception;
        JSStringRelease(script);

        if (!JSValueIsUndefined(JSGetContext(), result)) {
            CFStringRef json(JSValueToJSONCopy(JSGetContext(), result));
            std::cout << [reinterpret_cast<const NSString *>(json) UTF8String] << std::endl;
            CFRelease(json);
        }

        [pool release];
    }

    return 0;
}
