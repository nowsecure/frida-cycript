/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "Pooling.hpp"
#include "sig/parse.hpp"

namespace sig {

void Copy(CYPool &pool, Element &lhs, const Element &rhs) {
    lhs.name = pool.strdup(rhs.name);
    if (rhs.type == NULL)
        lhs.type = NULL;
    else
        lhs.type = rhs.type->Copy(pool);
    lhs.offset = rhs.offset;
}

void Copy(CYPool &pool, Signature &lhs, const Signature &rhs) {
    size_t count(rhs.count);
    lhs.count = count;
    lhs.elements = new(pool) Element[count];
    for (size_t index(0); index != count; ++index)
        Copy(pool, lhs.elements[index], rhs.elements[index]);
}

Void *Void::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Void();
}

Unknown *Unknown::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Unknown();
}

String *String::Copy(CYPool &pool, const char *rename) const {
    return new(pool) String();
}

#ifdef CY_OBJECTIVEC
Meta *Meta::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Meta();
}

Selector *Selector::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Selector();
}
#endif

Bits *Bits::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Bits(size);
}

Pointer *Pointer::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Pointer(*type.Copy(pool));
}

Array *Array::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Array(*type.Copy(pool), size);
}

#ifdef CY_OBJECTIVEC
Object *Object::Copy(CYPool &pool, const char *rename) const {
    return new(pool) Object(pool.strdup(name));
}
#endif

Aggregate *Aggregate::Copy(CYPool &pool, const char *rename) const {
    Aggregate *copy(new(pool) Aggregate(overlap, rename ?: pool.strdup(name)));
    sig::Copy(pool, copy->signature, signature);
    return copy;
}

Function *Function::Copy(CYPool &pool, const char *rename) const {
    Function *copy(new(pool) Function(variadic));
    sig::Copy(pool, copy->signature, signature);
    return copy;
}

#ifdef CY_OBJECTIVEC
Block *Block::Copy(CYPool &pool, const char *rename) const {
    Block *copy(new(pool) Block());
    sig::Copy(pool, copy->signature, signature);
    return copy;
}
#endif

void Copy(CYPool &pool, ffi_type &lhs, ffi_type &rhs) {
    lhs.size = rhs.size;
    lhs.alignment = rhs.alignment;
    lhs.type = rhs.type;
    if (rhs.elements == NULL)
        lhs.elements = NULL;
    else {
        size_t count(0);
        while (rhs.elements[count] != NULL)
            ++count;

        lhs.elements = new(pool) ffi_type *[count + 1];
        lhs.elements[count] = NULL;

        for (size_t index(0); index != count; ++index) {
            // XXX: if these are libffi native then you can just take them
            ffi_type *ffi(new(pool) ffi_type);
            lhs.elements[index] = ffi;
            sig::Copy(pool, *ffi, *rhs.elements[index]);
        }
    }
}

const char *Type::GetName() const {
    return NULL;
}

const char *Aggregate::GetName() const {
    return name;
}

}
