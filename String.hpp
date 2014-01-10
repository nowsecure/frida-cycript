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

#ifndef CYCRIPT_STRING_HPP
#define CYCRIPT_STRING_HPP

#include "cycript.hpp"
#include "Pooling.hpp"

#include <iostream>

struct CYUTF8String {
    const char *data;
    size_t size;

    CYUTF8String() :
        data(NULL),
        size(0)
    {
    }

    CYUTF8String(const char *data) :
        data(data),
        size(strlen(data))
    {
    }

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

static inline std::ostream &operator <<(std::ostream &lhs, CYUTF8String &rhs) {
    lhs.write(rhs.data, rhs.size);
    return lhs;
}

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

CYUTF8String CYPoolUTF8String(CYPool &pool, CYUTF16String utf16);
CYUTF16String CYPoolUTF16String(CYPool &pool, CYUTF8String utf8);

#endif/*CYCRIPT_STRING_HPP*/
