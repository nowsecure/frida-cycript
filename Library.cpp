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

#include <dlfcn.h>
#include <iconv.h>

#include "cycript.hpp"

#include "Pooling.hpp"
#include "Context.hpp"

#include <sys/mman.h>

#include <iostream>
#include <ext/stdio_filebuf.h>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "Parser.hpp"
#include "Cycript.tab.hh"

#include "Error.hpp"
#include "String.hpp"

/* C Strings {{{ */
template <typename Type_>
_finline size_t iconv_(size_t (*iconv)(iconv_t, Type_, size_t *, char **, size_t *), iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
    return iconv(cd, const_cast<Type_>(inbuf), inbytesleft, outbuf, outbytesleft);
}

#ifdef __GLIBC__
#define UCS_2_INTERNAL "UCS-2"
#else
#define UCS_2_INTERNAL "UCS-2-INTERNAL"
#endif

CYUTF8String CYPoolUTF8String(apr_pool_t *pool, CYUTF16String utf16) {
    _assert(pool != NULL);

    const char *in(reinterpret_cast<const char *>(utf16.data));

    iconv_t conversion(_syscall(iconv_open("UTF-8", UCS_2_INTERNAL)));

    // XXX: this is wrong
    size_t size(utf16.size * 5);
    char *out(new(pool) char[size]);
    CYUTF8String utf8(out, size);

    size = utf16.size * 2;
    _syscall(iconv_(&iconv, conversion, const_cast<char **>(&in), &size, &out, &utf8.size));

    *out = '\0';
    utf8.size = out - utf8.data;

    _syscall(iconv_close(conversion));

    return utf8;
}

CYUTF16String CYPoolUTF16String(apr_pool_t *pool, CYUTF8String utf8) {
    _assert(pool != NULL);

    const char *in(utf8.data);

    iconv_t conversion(_syscall(iconv_open(UCS_2_INTERNAL, "UTF-8")));

    // XXX: this is wrong
    size_t size(utf8.size * 5);
    uint16_t *temp(new (pool) uint16_t[size]);
    CYUTF16String utf16(temp, size * 2);
    char *out(reinterpret_cast<char *>(temp));

    size = utf8.size;
    _syscall(iconv_(&iconv, conversion, const_cast<char **>(&in), &size, &out, &utf16.size));

    utf16.size = reinterpret_cast<uint16_t *>(out) - utf16.data;
    temp[utf16.size] = 0;

    _syscall(iconv_close(conversion));

    return utf16;
}
/* }}} */
/* Index Offsets {{{ */
size_t CYGetIndex(const CYUTF8String &value) {
    if (value.data[0] != '0') {
        size_t index(0);
        for (size_t i(0); i != value.size; ++i) {
            if (!DigitRange_[value.data[i]])
                return _not(size_t);
            index *= 10;
            index += value.data[i] - '0';
        }
        return index;
    } else if (value.size == 1)
        return 0;
    else
        return _not(size_t);
}

// XXX: this isn't actually right
bool CYGetOffset(const char *value, ssize_t &index) {
    if (value[0] != '0') {
        char *end;
        index = strtol(value, &end, 10);
        if (value + strlen(value) == end)
            return true;
    } else if (value[1] == '\0') {
        index = 0;
        return true;
    }

    return false;
}
/* }}} */
/* JavaScript *ify {{{ */
void CYStringify(std::ostringstream &str, const char *data, size_t size) {
    unsigned quot(0), apos(0);
    for (const char *value(data), *end(data + size); value != end; ++value)
        if (*value == '"')
            ++quot;
        else if (*value == '\'')
            ++apos;

    bool single(quot > apos);

    str << (single ? '\'' : '"');

    for (const char *value(data), *end(data + size); value != end; ++value)
        switch (*value) {
            case '\\': str << "\\\\"; break;
            case '\b': str << "\\b"; break;
            case '\f': str << "\\f"; break;
            case '\n': str << "\\n"; break;
            case '\r': str << "\\r"; break;
            case '\t': str << "\\t"; break;
            case '\v': str << "\\v"; break;

            case '"':
                if (!single)
                    str << "\\\"";
                else goto simple;
            break;

            case '\'':
                if (single)
                    str << "\\'";
                else goto simple;
            break;

            default:
                // this test is designed to be "awesome", generating neither warnings nor incorrect results
                if (*value < 0x20 || *value >= 0x7f)
                    str << "\\x" << std::setbase(16) << std::setw(2) << std::setfill('0') << unsigned(uint8_t(*value));
                else simple:
                    str << *value;
        }

    str << (single ? '\'' : '"');
}

void CYNumerify(std::ostringstream &str, double value) {
    char string[32];
    // XXX: I want this to print 1e3 rather than 1000
    sprintf(string, "%.17g", value);
    str << string;
}

bool CYIsKey(CYUTF8String value) {
    const char *data(value.data);
    size_t size(value.size);

    if (size == 0)
        return false;

    if (DigitRange_[data[0]]) {
        size_t index(CYGetIndex(value));
        if (index == _not(size_t))
            return false;
    } else {
        if (!WordStartRange_[data[0]])
            return false;
        for (size_t i(1); i != size; ++i)
            if (!WordEndRange_[data[i]])
                return false;
    }

    return true;
}
/* }}} */

double CYCastDouble(const char *value, size_t size) {
    char *end;
    double number(strtod(value, &end));
    if (end != value + size)
        return NAN;
    return number;
}

double CYCastDouble(const char *value) {
    return CYCastDouble(value, strlen(value));
}

extern "C" void CydgetPoolParse(apr_pool_t *pool, const uint16_t **data, size_t *size) {
    CYDriver driver("");
    cy::parser parser(driver);

    CYUTF8String utf8(CYPoolUTF8String(pool, CYUTF16String(*data, *size)));

    driver.data_ = utf8.data;
    driver.size_ = utf8.size;

    if (parser.parse() != 0 || !driver.errors_.empty())
        return;

    CYOptions options;
    CYContext context(driver.pool_, options);
    driver.program_->Replace(context);
    std::ostringstream str;
    CYOutput out(str, options);
    out << *driver.program_;
    std::string code(str.str());

    CYUTF16String utf16(CYPoolUTF16String(pool, CYUTF8String(code.c_str(), code.size())));

    *data = utf16.data;
    *size = utf16.size;
}

static apr_pool_t *Pool_;

static bool initialized_;

void CYInitializeStatic() {
    if (!initialized_)
        initialized_ = true;
    else return;

    _aprcall(apr_initialize());
    _aprcall(apr_pool_create(&Pool_, NULL));
}

apr_pool_t *CYGetGlobalPool() {
    CYInitializeStatic();
    return Pool_;
}

void CYThrow(const char *format, ...) {
    va_list args;
    va_start(args, format);
    throw CYPoolError(format, args);
    // XXX: does this matter? :(
    va_end(args);
}

const char *CYPoolError::PoolCString(apr_pool_t *pool) const {
    return apr_pstrdup(pool, message_);
}

CYPoolError::CYPoolError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    message_ = apr_pvsprintf(pool_, format, args);
    va_end(args);
}

CYPoolError::CYPoolError(const char *format, va_list args) {
    message_ = apr_pvsprintf(pool_, format, args);
}
