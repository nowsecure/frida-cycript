/* Cycript - Inlining/Optimizing JavaScript Compiler
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
#include "Pooling.hpp"

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

size_t CYGetIndex(const CYUTF8String &value);
bool CYIsKey(CYUTF8String value);
bool CYGetOffset(const char *value, ssize_t &index);

CYUTF8String CYPoolUTF8String(apr_pool_t *pool, CYUTF16String utf16);
CYUTF16String CYPoolUTF16String(apr_pool_t *pool, CYUTF8String utf8);

#endif/*CYCRIPT_STRING_HPP*/
