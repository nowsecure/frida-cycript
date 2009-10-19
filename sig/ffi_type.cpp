/* Cycript - Remove Execution Server and Disassembler
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

#include "minimal/stdlib.h"

#include "sig/ffi_type.hpp"
#include "sig/types.hpp"

#define ffi_type_slonglong ffi_type_sint64
#define ffi_type_ulonglong ffi_type_uint64

namespace sig {

void sig_ffi_types(
    apr_pool_t *pool,
    ffi_type *(*sig_ffi_type)(apr_pool_t *, struct Type *),
    struct Signature *signature,
    ffi_type **types,
    size_t skip = 0,
    size_t offset = 0
) {
    _assert(signature->count >= skip);
    for (size_t index = skip; index != signature->count; ++index)
        types[index - skip + offset] = (*sig_ffi_type)(pool, signature->elements[index].type);
}

ffi_type *ObjectiveC(apr_pool_t *pool, struct Type *type) {
    switch (type->primitive) {
        case typename_P: return &ffi_type_pointer;

        case union_P:
            /* XXX: we can totally make this work */
            _assert(false);
        break;

        case string_P: return &ffi_type_pointer;
        case selector_P: return &ffi_type_pointer;
        case object_P: return &ffi_type_pointer;
        case boolean_P: return &ffi_type_uchar;
        case uchar_P: return &ffi_type_uchar;
        case uint_P: return &ffi_type_uint;
        case ulong_P: return &ffi_type_ulong;
        case ulonglong_P: return &ffi_type_ulonglong;
        case ushort_P: return &ffi_type_ushort;

        case array_P: {
            // XXX: this is really lame
            ffi_type *aggregate(reinterpret_cast<ffi_type *>(apr_palloc(pool, sizeof(ffi_type))));
            aggregate->size = 0;
            aggregate->alignment = 0;
            aggregate->type = FFI_TYPE_STRUCT;

            ffi_type *element(ObjectiveC(pool, type->data.data.type));
            size_t size(type->data.data.size);

            aggregate->elements = reinterpret_cast<ffi_type **>(apr_palloc(pool, (size + 1) * sizeof(ffi_type *)));
            for (size_t i(0); i != size; ++i)
                aggregate->elements[i] = element;
            aggregate->elements[size] = NULL;

            return aggregate;
        } break;

        case pointer_P: return &ffi_type_pointer;

        case bit_P:
            /* XXX: we can totally make this work */
            _assert(false);
        break;

        case char_P: return &ffi_type_schar;
        case double_P: return &ffi_type_double;
        case float_P: return &ffi_type_float;
        case int_P: return &ffi_type_sint;
        case long_P: return &ffi_type_slong;
        case longlong_P: return &ffi_type_slonglong;
        case short_P: return &ffi_type_sshort;

        case void_P: return &ffi_type_void;

        case struct_P: {
            ffi_type *aggregate(reinterpret_cast<ffi_type *>(apr_palloc(pool, sizeof(ffi_type))));
            aggregate->size = 0;
            aggregate->alignment = 0;
            aggregate->type = FFI_TYPE_STRUCT;

            aggregate->elements = reinterpret_cast<ffi_type **>(apr_palloc(pool, (type->data.signature.count + 1) * sizeof(ffi_type *)));
            sig_ffi_types(pool, &ObjectiveC, &type->data.signature, aggregate->elements);
            aggregate->elements[type->data.signature.count] = NULL;

            return aggregate;
        } break;

        default:
            _assert(false);
        break;
    }
}

ffi_type *Java(apr_pool_t *pool, struct Type *type) {
    switch (type->primitive) {
        case typename_P: return &ffi_type_pointer;
        case union_P: return &ffi_type_pointer;
        case string_P: return &ffi_type_pointer;
        case selector_P: return &ffi_type_pointer;
        case object_P: return &ffi_type_pointer;
        case boolean_P: return &ffi_type_uchar;
        case uchar_P: return &ffi_type_uchar;
        case uint_P: return &ffi_type_uint;
        case ulong_P: return &ffi_type_ulong;
        case ulonglong_P: return &ffi_type_ulonglong;
        case ushort_P: return &ffi_type_ushort;
        case array_P: return &ffi_type_pointer;
        case pointer_P: return &ffi_type_pointer;

        /* XXX: bit type */
        case bit_P: return &ffi_type_uint;

        case char_P: return &ffi_type_schar;
        case double_P: return &ffi_type_double;
        case float_P: return &ffi_type_double;
        case int_P: return &ffi_type_sint;
        case long_P: return &ffi_type_slong;
        case longlong_P: return &ffi_type_slonglong;
        case short_P: return &ffi_type_sshort;
        case void_P: return &ffi_type_void;
        case struct_P: return &ffi_type_pointer;

        default:
            _assert(false);
        break;
    }
}

void sig_ffi_cif(
    apr_pool_t *pool,
    ffi_type *(*sig_ffi_type)(apr_pool_t *, struct Type *),
    struct Signature *signature,
    ffi_cif *cif,
    size_t skip,
    ffi_type **types,
    size_t offset
) {
    if (types == NULL)
        types = reinterpret_cast<ffi_type **>(apr_palloc(pool, (signature->count - 1) * sizeof(ffi_type *)));
    ffi_type *type = (*sig_ffi_type)(pool, signature->elements[0].type);
    sig_ffi_types(pool, sig_ffi_type, signature, types, 1 + skip, offset);
    ffi_status status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, signature->count - 1 - skip + offset, type, types);
    _assert(status == FFI_OK);
}

}
