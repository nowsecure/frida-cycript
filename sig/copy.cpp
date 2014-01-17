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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "Pooling.hpp"
#include "sig/parse.hpp"

#ifdef HAVE_FFI_FFI_H
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

namespace sig {

void Copy(CYPool &pool, Element &lhs, const Element &rhs) {
    lhs.name = pool.strdup(rhs.name);
    if (rhs.type == NULL)
        lhs.type = NULL;
    else {
        lhs.type = new(pool) Type;
        Copy(pool, *lhs.type, *rhs.type);
    }
    lhs.offset = rhs.offset;
}

void Copy(CYPool &pool, Signature &lhs, const Signature &rhs) {
    size_t count(rhs.count);
    lhs.count = count;
    lhs.elements = new(pool) Element[count];
    for (size_t index(0); index != count; ++index)
        Copy(pool, lhs.elements[index], rhs.elements[index]);
}

void Copy(CYPool &pool, Type &lhs, const Type &rhs) {
    lhs.primitive = rhs.primitive;
    lhs.name = pool.strdup(rhs.name);
    lhs.flags = rhs.flags;

    if (sig::IsFunctional(rhs.primitive) || sig::IsAggregate(rhs.primitive))
        Copy(pool, lhs.data.signature, rhs.data.signature);
    else {
        sig::Type *&lht(lhs.data.data.type);
        sig::Type *const &rht(rhs.data.data.type);

        if (rht == NULL)
            lht = NULL;
        else {
            lht = new(pool) Type;
            Copy(pool, *lht, *rht);
        }

        lhs.data.data.size = rhs.data.data.size;
    }
}

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

}
