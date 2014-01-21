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

#include "sig/parse.hpp"
#include "Error.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace sig {

void Parse_(CYPool &pool, struct Signature *signature, const char **name, char eos, Callback callback);
struct Type *Parse_(CYPool &pool, const char **name, char eos, bool named, Callback callback);


/* XXX: I really screwed up this time */
void *prealloc_(CYPool &pool, void *odata, size_t osize, size_t nsize) {
    void *ndata(pool.malloc<void>(nsize));
    memcpy(ndata, odata, osize);
    return ndata;
}

void Parse_(CYPool &pool, struct Signature *signature, const char **name, char eos, Callback callback) {
    _assert(*name != NULL);

    // XXX: this is just a stupid check :(
    bool named(**name == '"');

    signature->elements = NULL;
    signature->count = 0;

    for (;;) {
        if (**name == eos) {
            ++*name;
            return;
        }

        signature->elements = (struct Element *) prealloc_(pool, signature->elements, signature->count * sizeof(struct Element), (signature->count + 1) * sizeof(struct Element));
        _assert(signature->elements != NULL);

        struct Element *element = &signature->elements[signature->count++];

        if (**name != '"')
            element->name = NULL;
        else {
            const char *quote = strchr(++*name, '"');
            element->name = pool.strmemdup(*name, quote - *name);
            *name = quote + 1;
        }

        element->type = Parse_(pool, name, eos, named, callback);

        if (**name < '0' || **name > '9')
            element->offset = _not(size_t);
        else {
            element->offset = 0;

            do
                element->offset = element->offset * 10 + (*(*name)++ - '0');
            while (**name >= '0' && **name <= '9');
        }
    }
}

Type *Parse_(CYPool &pool, const char **name, char eos, bool named, Callback callback) {
    char next = *(*name)++;

    Type *type(new(pool) Type());
    _assert(type != NULL);
    memset(type, 0, sizeof(Type));

  parse:
    switch (next) {
        case '?': type->primitive = unknown_P; break;
        case '#': type->primitive = typename_P; break;

        case '(':
            if (type->data.signature.count < 2)
                type->primitive = struct_P;
            else
                type->primitive = union_P;
            next = ')';
        goto aggregate;

        case '*': type->primitive = string_P; break;
        case ':': type->primitive = selector_P; break;

        case '@': {
            char next(**name);

            if (next == '?') {
                type->primitive = block_P;
                ++*name;
            } else {
                type->primitive = object_P;

                if (next == '"') {
                    const char *quote = strchr(*name + 1, '"');
                    if (quote == NULL) {
                        printf("unterminated specific id type {%s}\n", *name - 10);
                        _assert(false);
                    } else if (!named || quote[1] == eos || quote[1] == '"') {
                        type->name = pool.strmemdup(*name + 1, quote - *name - 1);
                        *name = quote + 1;
                    }
                }
            }

        } break;

        case 'B': type->primitive = boolean_P; break;
        case 'C': type->primitive = uchar_P; break;
        case 'I': type->primitive = uint_P; break;
        case 'L': type->primitive = ulong_P; break;
        case 'Q': type->primitive = ulonglong_P; break;
        case 'S': type->primitive = ushort_P; break;

        case '[':
            type->primitive = array_P;
            type->data.data.size = strtoul(*name, (char **) name, 10);
            type->data.data.type = Parse_(pool, name, eos, false, callback);
            if (**name != ']') {
                printf("']' != \"%s\"\n", *name);
                _assert(false);
            }
            ++*name;
        break;

        case '^':
            type->primitive = pointer_P;
            if (**name == '"')
                // XXX: why is this here?
                type->data.data.type = NULL;
            else
                type->data.data.type = Parse_(pool, name, eos, named, callback);
        break;

        case 'b':
            type->primitive = bit_P;
            type->data.data.size = strtoul(*name, (char **) name, 10);
        break;

        case 'c': type->primitive = char_P; break;
        case 'd': type->primitive = double_P; break;
        case 'f': type->primitive = float_P; break;
        case 'i': type->primitive = int_P; break;
        case 'l': type->primitive = long_P; break;
        case 'q': type->primitive = longlong_P; break;
        case 's': type->primitive = short_P; break;
        case 'v': type->primitive = void_P; break;

#ifdef __LP64__
        case 'F': type->primitive = double_P; break;
#else
        case 'F': type->primitive = float_P; break;
#endif

        case '{':
            type->primitive = struct_P;
            next = '}';
        goto aggregate;

        aggregate: {
            char end = next;
            const char *begin = *name;
            do next = *(*name)++;
            while (
                next != '=' &&
                next != '}'
            );
            size_t length = *name - begin - 1;
            if (strncmp(begin, "?", length) != 0)
                type->name = (char *) pool.strmemdup(begin, length);
            else
                type->name = NULL;

            // XXX: this types thing is a throwback to JocStrap

            if (next == '=')
                Parse_(pool, &type->data.signature, name, end, callback);
        } break;

        case 'N': type->flags |= JOC_TYPE_INOUT; goto next;
        case 'n': type->flags |= JOC_TYPE_IN; goto next;
        case 'O': type->flags |= JOC_TYPE_BYCOPY; goto next;
        case 'o': type->flags |= JOC_TYPE_OUT; goto next;
        case 'R': type->flags |= JOC_TYPE_BYREF; goto next;
        case 'r': type->flags |= JOC_TYPE_CONST; goto next;
        case 'V': type->flags |= JOC_TYPE_ONEWAY; goto next;

        next:
            next = *(*name)++;
            goto parse;
        break;

        default:
            printf("invalid type character: '%c' {%s}\n", next, *name - 10);
            _assert(false);
    }

    if (callback != NULL)
        (*callback)(pool, type);

    return type;
}

void Parse(CYPool &pool, struct Signature *signature, const char *name, Callback callback) {
    const char *temp = name;
    Parse_(pool, signature, &temp, '\0', callback);
    _assert(temp[-1] == '\0');
}

const char *Unparse(CYPool &pool, struct Signature *signature) {
    const char *value = "";
    size_t offset;

    for (offset = 0; offset != signature->count; ++offset) {
        const char *type = Unparse(pool, signature->elements[offset].type);
        value = pool.strcat(value, type, NULL);
    }

    return value;
}

const char *Unparse_(CYPool &pool, struct Type *type) {
    switch (type->primitive) {
        case function_P: {
            if (type->data.signature.count == 0)
                return "?";
            std::ostringstream out;
            for (size_t i(0); i != type->data.signature.count; ++i) {
                Element &element(type->data.signature.elements[i]);
                out << Unparse(pool, element.type);
                if (element.offset != _not(size_t))
                    out << pool.itoa(element.offset);
            }
            return pool.strdup(out.str().c_str());
        } break;

        case unknown_P: return "?";
        case typename_P: return "#";
        case union_P: return pool.strcat("(", Unparse(pool, &type->data.signature), ")", NULL);
        case string_P: return "*";
        case selector_P: return ":";
        case block_P: return "@?";
        case object_P: return type->name == NULL ? "@" : pool.strcat("@\"", type->name, "\"", NULL);
        case boolean_P: return "B";
        case uchar_P: return "C";
        case uint_P: return "I";
        case ulong_P: return "L";
        case ulonglong_P: return "Q";
        case ushort_P: return "S";

        case array_P: {
            const char *value = Unparse(pool, type->data.data.type);
            return pool.strcat("[", pool.itoa(type->data.data.size), value, "]", NULL);
        } break;

        case pointer_P: {
            // XXX: protect against the weird '"' check in Parse_
            _assert(type->data.data.type != NULL);
            if (type->data.data.type->primitive == function_P)
                return "^?";
            else
                return pool.strcat("^", Unparse(pool, type->data.data.type), NULL);
        } break;

        case bit_P: return pool.strcat("b", pool.itoa(type->data.data.size), NULL);
        case char_P: return "c";
        case double_P: return "d";
        case float_P: return "f";
        case int_P: return "i";
        case long_P: return "l";
        case longlong_P: return "q";
        case short_P: return "s";
        case void_P: return "v";
        case struct_P: return pool.strcat("{", type->name == NULL ? "?" : type->name, "=", Unparse(pool, &type->data.signature), "}", NULL);
    }

    _assert(false);
    return NULL;
}

const char *Unparse(CYPool &pool, struct Type *type) {
    if (type == NULL)
        return "?";

    const char *base(Unparse_(pool, type));
    if (type->flags == 0)
        return base;

    #define iovec_(base, size) \
        (struct iovec) {const_cast<char *>(base), size}

    size_t size(strlen(base));
    char buffer[7 + size];
    size_t offset(0);

    if ((type->flags & JOC_TYPE_INOUT) != 0)
        buffer[offset++] = 'N';
    if ((type->flags & JOC_TYPE_IN) != 0)
        buffer[offset++] = 'n';
    if ((type->flags & JOC_TYPE_BYCOPY) != 0)
        buffer[offset++] = 'O';
    if ((type->flags & JOC_TYPE_OUT) != 0)
        buffer[offset++] = 'o';
    if ((type->flags & JOC_TYPE_BYREF) != 0)
        buffer[offset++] = 'R';
    if ((type->flags & JOC_TYPE_CONST) != 0)
        buffer[offset++] = 'r';
    if ((type->flags & JOC_TYPE_ONEWAY) != 0)
        buffer[offset++] = 'V';

    memcpy(buffer + offset, base, size);
    return pool.strmemdup(buffer, offset + size);
}

}
