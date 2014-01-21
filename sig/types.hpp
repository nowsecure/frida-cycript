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

#ifndef SIG_TYPES_H
#define SIG_TYPES_H

#include <cstdlib>
#include <stdint.h>

#include "Standard.hpp"

namespace sig {

enum Primitive {
    function_P = '\0',
    block_P = '\a',

    unknown_P = '?',
    typename_P = '#',
    union_P = '(',
    string_P = '*',
    selector_P = ':',
    object_P = 'W',
    boolean_P = 'B',
    uchar_P = 'C',
    uint_P = 'I',
    ulong_P = 'L',
    ulonglong_P = 'Q',
    ushort_P = 'S',
    array_P = '[',
    pointer_P = '^',
    bit_P = 'b',
    char_P = 'c',
    double_P = 'd',
    float_P = 'f',
    int_P = 'i',
    long_P = 'l',
    longlong_P = 'q',
    short_P = 's',
    void_P = 'v',
    struct_P = '{'
};

struct Element {
    const char *name;
    struct Type *type;
    size_t offset;
};

struct Signature {
    struct Element *elements;
    size_t count;
};

#define JOC_TYPE_INOUT  (1 << 0)
#define JOC_TYPE_IN     (1 << 1)
#define JOC_TYPE_BYCOPY (1 << 2)
#define JOC_TYPE_OUT    (1 << 3)
#define JOC_TYPE_BYREF  (1 << 4)
#define JOC_TYPE_CONST  (1 << 5)
#define JOC_TYPE_ONEWAY (1 << 6)

struct Type {
    enum Primitive primitive;
    const char *name;
    uint8_t flags;

    union {
        struct {
            struct Type *type;
            size_t size;
        } data;

        struct Signature signature;
    } data;
};

struct Type *joc_parse_type(char **name, char eos, bool variable, bool signature);
void joc_parse_signature(struct Signature *signature, char **name, char eos, bool variable);

_finline bool IsFunctional(Primitive primitive) {
    return primitive == block_P || primitive == function_P;
}

_finline bool IsAggregate(Primitive primitive) {
    return primitive == struct_P || primitive == union_P;
}

}

#endif/*SIG_TYPES_H*/
