#ifndef SIG_PARSE_H
#define SIG_PARSE_H

#include "sig/types.hpp"

#include <apr-1/apr_pools.h>

namespace sig {

extern void (*sig_aggregate)(apr_pool_t *pool, enum Primitive primitive, const char *name, struct Signature *signature, const char *types);

void Parse(apr_pool_t *pool, struct Signature *signature, const char *name);

const char *Unparse(apr_pool_t *pool, struct Signature *signature);
const char *Unparse(apr_pool_t *pool, struct Type *type);

}

#endif/*SIG_PARSE_H*/
