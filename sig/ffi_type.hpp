#ifndef SIG_FFI_TYPE_H
#define SIG_FFI_TYPE_H

#include <apr-1/apr_pools.h>
#include <ffi.h>

#include "sig/types.hpp"

namespace sig {

ffi_type *ObjectiveC(apr_pool_t *pool, struct Type *type);
ffi_type *Java(apr_pool_t *pool, struct Type *type);

void sig_ffi_cif(
    apr_pool_t *pool,
    ffi_type *(*sig_ffi_type)(apr_pool_t *, struct Type *),
    struct Signature *signature,
    ffi_cif *cif,
    size_t skip = 0,
    ffi_type **types = NULL,
    size_t offset = 0
);

}

#endif/*SIG_FFI_TYPE_H*/
