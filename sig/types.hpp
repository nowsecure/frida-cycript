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
