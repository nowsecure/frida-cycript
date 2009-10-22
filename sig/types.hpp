/* Cycript - Remote Execution Server and Disassembler
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

#ifndef SIG_TYPES_H
#define SIG_TYPES_H

#include "minimal/stdlib.h"

namespace sig {

enum Primitive {
    typename_P = '#',
    union_P = '(',
    string_P = '*',
    selector_P = ':',
    object_P = '@',
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
    char *name;
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
    char *name;
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

_finline bool IsAggregate(Primitive primitive) {
    return primitive == struct_P || primitive == union_P;
}

}

#endif/*SIG_TYPES_H*/
