/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
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

#include "Error.hpp"

#include "sig/ffi_type.hpp"
#include "sig/types.hpp"

#define ffi_type_slonglong ffi_type_sint64
#define ffi_type_ulonglong ffi_type_uint64

namespace sig {

void sig_ffi_types(
    CYPool &pool,
    const struct Signature *signature,
    ffi_type **types,
    size_t skip = 0,
    size_t offset = 0
) {
    _assert(signature->count >= skip);
    for (size_t index = skip; index != signature->count; ++index)
        types[index - skip + offset] = signature->elements[index].type->GetFFI(pool);
}

template <>
ffi_type *Primitive<bool>::GetFFI(CYPool &pool) const {
    return &ffi_type_uchar;
}

template <>
ffi_type *Primitive<char>::GetFFI(CYPool &pool) const {
    return &ffi_type_schar;
}

template <>
ffi_type *Primitive<float>::GetFFI(CYPool &pool) const {
    return &ffi_type_float;
}

template <>
ffi_type *Primitive<double>::GetFFI(CYPool &pool) const {
    return &ffi_type_double;
}

template <>
ffi_type *Primitive<signed char>::GetFFI(CYPool &pool) const {
    return &ffi_type_schar;
}

template <>
ffi_type *Primitive<signed int>::GetFFI(CYPool &pool) const {
    return &ffi_type_sint;
}

template <>
ffi_type *Primitive<signed long int>::GetFFI(CYPool &pool) const {
    return &ffi_type_slong;
}

template <>
ffi_type *Primitive<signed long long int>::GetFFI(CYPool &pool) const {
    return &ffi_type_slonglong;
}

template <>
ffi_type *Primitive<signed short int>::GetFFI(CYPool &pool) const {
    return &ffi_type_sshort;
}

template <>
ffi_type *Primitive<unsigned char>::GetFFI(CYPool &pool) const {
    return &ffi_type_uchar;
}

template <>
ffi_type *Primitive<unsigned int>::GetFFI(CYPool &pool) const {
    return &ffi_type_uint;
}

template <>
ffi_type *Primitive<unsigned long int>::GetFFI(CYPool &pool) const {
    return &ffi_type_ulong;
}

template <>
ffi_type *Primitive<unsigned long long int>::GetFFI(CYPool &pool) const {
    return &ffi_type_ulonglong;
}

template <>
ffi_type *Primitive<unsigned short int>::GetFFI(CYPool &pool) const {
    return &ffi_type_ushort;
}

ffi_type *Void::GetFFI(CYPool &pool) const {
    return &ffi_type_void;
}

ffi_type *Unknown::GetFFI(CYPool &pool) const {
    _assert(false);
}

ffi_type *String::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

ffi_type *Meta::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

ffi_type *Selector::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

ffi_type *Bits::GetFFI(CYPool &pool) const {
    /* XXX: we can totally make this work */
    _assert(false);
}

ffi_type *Pointer::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

ffi_type *Array::GetFFI(CYPool &pool) const {
    // XXX: this is really lame
    ffi_type *ffi(new(pool) ffi_type());
    ffi->size = 0;
    ffi->alignment = 0;
    ffi->type = FFI_TYPE_STRUCT;

    ffi_type *element(type.GetFFI(pool));

    ffi->elements = new(pool) ffi_type *[size + 1];
    for (size_t i(0); i != size; ++i)
        ffi->elements[i] = element;
    ffi->elements[size] = NULL;

    return ffi;
}

ffi_type *Object::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

ffi_type *Aggregate::GetFFI(CYPool &pool) const {
    // XXX: we can totally make overlap work
    _assert(!overlap);

    ffi_type *ffi(new(pool) ffi_type());
    ffi->size = 0;
    ffi->alignment = 0;
    ffi->type = FFI_TYPE_STRUCT;

    ffi->elements = new(pool) ffi_type *[signature.count + 1];
    sig_ffi_types(pool, &signature, ffi->elements);
    ffi->elements[signature.count] = NULL;

    return ffi;
}

ffi_type *Function::GetFFI(CYPool &pool) const {
    _assert(false);
}

ffi_type *Block::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

void sig_ffi_cif(
    CYPool &pool,
    struct Signature *signature,
    ffi_cif *cif,
    size_t skip,
    ffi_type **types,
    size_t offset
) {
    if (types == NULL)
        types = new(pool) ffi_type *[signature->count - 1];
    ffi_type *type = signature->elements[0].type->GetFFI(pool);
    sig_ffi_types(pool, signature, types, 1 + skip, offset);
    ffi_status status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, signature->count - 1 - skip + offset, type, types);
    _assert(status == FFI_OK);
}

}
