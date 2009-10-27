/* Cycript - Error.hppution Server and Disassembler
 * Copyright (C) 2009  Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
 *        Redistribution and use in source and binary
 * forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation
 *    and/or other materials provided with the
 *    distribution.
 * 3. The name of the author may not be used to endorse
 *    or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#ifndef CYCRIPT_STRING_HPP
#define CYCRIPT_STRING_HPP

#include "cycript.hpp"

struct CYUTF8String {
    const char *data;
    size_t size;

    CYUTF8String(const char *data, size_t size) :
        data(data),
        size(size)
    {
    }

    bool operator ==(const char *value) const {
        size_t length(strlen(data));
        return length == size && memcmp(value, data, length) == 0;
    }
};

struct CYUTF16String {
    const uint16_t *data;
    size_t size;

    CYUTF16String(const uint16_t *data, size_t size) :
        data(data),
        size(size)
    {
    }
};

JSStringRef CYCopyJSString(const char *value);
JSStringRef CYCopyJSString(JSStringRef value);
JSStringRef CYCopyJSString(CYUTF8String value);
JSStringRef CYCopyJSString(JSContextRef context, JSValueRef value);

class CYJSString {
  private:
    JSStringRef string_;

    void Clear_() {
        if (string_ != NULL)
            JSStringRelease(string_);
    }

  public:
    CYJSString(const CYJSString &rhs) :
        string_(CYCopyJSString(rhs.string_))
    {
    }

    template <typename Arg0_>
    CYJSString(Arg0_ arg0) :
        string_(CYCopyJSString(arg0))
    {
    }

    template <typename Arg0_, typename Arg1_>
    CYJSString(Arg0_ arg0, Arg1_ arg1) :
        string_(CYCopyJSString(arg0, arg1))
    {
    }

    CYJSString &operator =(const CYJSString &rhs) {
        Clear_();
        string_ = CYCopyJSString(rhs.string_);
        return *this;
    }

    ~CYJSString() {
        Clear_();
    }

    void Clear() {
        Clear_();
        string_ = NULL;
    }

    operator JSStringRef() const {
        return string_;
    }
};

size_t CYGetIndex(const CYUTF8String &value);
bool CYIsKey(CYUTF8String value);
bool CYGetOffset(const char *value, ssize_t &index);

const char *CYPoolCString(apr_pool_t *pool, JSContextRef context, JSValueRef value);

#endif/*CYCRIPT_STRING_HPP*/
