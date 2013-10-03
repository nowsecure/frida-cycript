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

#include <dlfcn.h>

#include "cycript.hpp"

#include "Pooling.hpp"

#include <sys/mman.h>

#include <iostream>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "Error.hpp"
#include "Execute.hpp"
#include "Parser.hpp"
#include "String.hpp"

#include "Cycript.tab.hh"
#include "Driver.hpp"

#include "ConvertUTF.h"

/* C Strings {{{ */
CYUTF8String CYPoolUTF8String(CYPool &pool, CYUTF16String utf16) {
    // XXX: this is wrong
    size_t size(utf16.size * 5);
    char *temp(new(pool) char[size]);

    const uint16_t *lhs(utf16.data);
    uint8_t *rhs(reinterpret_cast<uint8_t *>(temp));
    _assert(ConvertUTF16toUTF8(&lhs, lhs + utf16.size, &rhs, rhs + size, lenientConversion) == conversionOK);

    *rhs = 0;
    return CYUTF8String(temp, reinterpret_cast<char *>(rhs) - temp);
}

CYUTF16String CYPoolUTF16String(CYPool &pool, CYUTF8String utf8) {
    // XXX: this is wrong
    size_t size(utf8.size * 5);
    uint16_t *temp(new (pool) uint16_t[size]);

    const uint8_t *lhs(reinterpret_cast<const uint8_t *>(utf8.data));
    uint16_t *rhs(temp);
    _assert(ConvertUTF8toUTF16(&lhs, lhs + utf8.size, &rhs, rhs + size, lenientConversion) == conversionOK);

    *rhs = 0;
    return CYUTF16String(temp, rhs - temp);
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

extern "C" void CydgetMemoryParse(const uint16_t **data, size_t *size) {
    CYLocalPool local;

    CYUTF8String utf8(CYPoolUTF8String(local, CYUTF16String(*data, *size)));
    CYStream stream(utf8.data, utf8.data + utf8.size);
    CYDriver driver(stream);

    cy::parser parser(driver);
    if (parser.parse() != 0 || !driver.errors_.empty())
        return;

    CYOptions options;
    CYContext context(options);
    driver.program_->Replace(context);
    std::ostringstream str;
    CYOutput out(str, options);
    out << *driver.program_;
    std::string code(str.str());

    CYPool pool;
    CYUTF16String utf16(CYPoolUTF16String(pool, CYUTF8String(code.c_str(), code.size())));

    size_t bytes(utf16.size * sizeof(uint16_t));
    uint16_t *copy(reinterpret_cast<uint16_t *>(malloc(bytes)));
    memcpy(copy, utf16.data, bytes);

    *data = copy;
    *size = utf16.size;
}

CYPool &CYGetGlobalPool() {
    static CYPool pool;
    return pool;
}

void CYThrow(const char *format, ...) {
    va_list args;
    va_start(args, format);
    throw CYPoolError(format, args);
    // XXX: does this matter? :(
    va_end(args);
}

const char *CYPoolError::PoolCString(CYPool &pool) const {
    return pool.strdup(message_);
}

CYPoolError::CYPoolError(const CYPoolError &rhs) :
    message_(pool_.strdup(rhs.message_))
{
}

CYPoolError::CYPoolError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    // XXX: there might be a beter way to think about this
    message_ = pool_.vsprintf(64, format, args);
    va_end(args);
}

CYPoolError::CYPoolError(const char *format, va_list args) {
    // XXX: there might be a beter way to think about this
    message_ = pool_.vsprintf(64, format, args);
}
