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

#include "Error.hpp"

#include "sig/ffi_type.hpp"
#include "sig/types.hpp"

#if FFI_LONG_LONG_MAX == 9223372036854775807LL
#define ffi_type_slonglong ffi_type_sint64
#define ffi_type_ulonglong ffi_type_uint64
#else
#error need to configure for long long
#endif

namespace sig {

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
ffi_type *Primitive<long double>::GetFFI(CYPool &pool) const {
    return &ffi_type_longdouble;
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

#ifdef __SIZEOF_INT128__
template <>
ffi_type *Primitive<signed __int128>::GetFFI(CYPool &pool) const {
    _assert(false);
}
#endif

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

#ifdef __SIZEOF_INT128__
template <>
ffi_type *Primitive<unsigned __int128>::GetFFI(CYPool &pool) const {
    _assert(false);
}
#endif

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

#ifdef CY_OBJECTIVEC
ffi_type *Meta::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}

ffi_type *Selector::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}
#endif

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

#ifdef CY_OBJECTIVEC
ffi_type *Object::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}
#endif

ffi_type *Enum::GetFFI(CYPool &pool) const {
    return type.GetFFI(pool);
}

ffi_type *Aggregate::GetFFI(CYPool &pool) const {
    _assert(!overlap);
    _assert(signature.count != _not(size_t));

    ffi_type *ffi(new(pool) ffi_type());
    ffi->size = 0;
    ffi->alignment = 0;
    ffi->type = FFI_TYPE_STRUCT;

    if (signature.count == 0) {
        // https://gcc.gnu.org/ml/gcc-patches/2015-01/msg01286.html
        ffi->elements = new(pool) ffi_type *[2];
        ffi->elements[0] = &ffi_type_void;
        ffi->elements[1] = NULL;
    } else {
        ffi->elements = new(pool) ffi_type *[signature.count + 1];
        for (size_t index(0); index != signature.count; ++index)
            ffi->elements[index] = signature.elements[index].type->GetFFI(pool);
        ffi->elements[signature.count] = NULL;
    }

    return ffi;
}

ffi_type *Function::GetFFI(CYPool &pool) const {
    _assert(false);
}

#ifdef CY_OBJECTIVEC
ffi_type *Block::GetFFI(CYPool &pool) const {
    return &ffi_type_pointer;
}
#endif

void sig_ffi_cif(CYPool &pool, size_t variadic, const Signature &signature, ffi_cif *cif) {
    _assert(signature.count != 0);
    size_t count(signature.count - 1);
    ffi_type *type(signature.elements[0].type->GetFFI(pool));

    ffi_type **types(new(pool) ffi_type *[count]);
    for (size_t index(0); index != count; ++index)
        types[index] = signature.elements[index + 1].type->GetFFI(pool);

    ffi_status status;
#ifdef HAVE_FFI_PREP_CIF_VAR
    if (variadic == 0)
#endif
        status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, count, type, types);
#ifdef HAVE_FFI_PREP_CIF_VAR
    else
        status = ffi_prep_cif_var(cif, FFI_DEFAULT_ABI, variadic - 1, count, type, types);
#endif
    _assert(status == FFI_OK);
}

}
