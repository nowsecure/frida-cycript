/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2013  Jay Freeman (saurik)
*/

/* GNU General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include <Pooling.hpp>

#include <CoreFoundation/CFLogUtilities.h>
#include <CFNetwork/CFNetwork.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

struct Client {
    CFHTTPMessageRef message_;
    CFSocketRef socket_;
};

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

                JSValueRef result(JSEvaluateScript(CYGetJSContext(), script, NULL, NULL, 0, NULL));
                JSStringRelease(script);

                CFHTTPMessageRef response(CFHTTPMessageCreateResponse(kCFAllocatorDefault, 200, NULL, kCFHTTPVersion1_1));
                CFHTTPMessageSetHeaderFieldValue(response, CFSTR("Content-Type"), CFSTR("application/json; charset=utf-8"));

                CFStringRef json(CYCopyJSONString(CYGetJSContext(), result, NULL));
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

int main(int argc, char *argv[]) {
    {
        struct sockaddr_in address;
        address.sin_len = sizeof(address);
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(787);

        CFDataRef data(CFDataCreate(kCFAllocatorDefault, reinterpret_cast<UInt8 *>(&address), sizeof(address)));

        CFSocketSignature signature;
        signature.protocolFamily = AF_INET;
        signature.socketType = SOCK_STREAM;
        signature.protocol = IPPROTO_TCP;
        signature.address = data;

        CFSocketRef socket(CFSocketCreateWithSocketSignature(kCFAllocatorDefault, &signature, kCFSocketAcceptCallBack, &OnAccept, NULL));
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0), kCFRunLoopDefaultMode);
    }

    {
        CYServer *server(new CYServer());
        server->socket_ = _syscall(socket(PF_UNIX, SOCK_STREAM, 0));

        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;

        sprintf(address.sun_path, "/tmp/.s.cy");
    }
}
