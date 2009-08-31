#ifndef SIG_PARSE_H
#define SIG_PARSE_H

#include "sig/types.hpp"

#include <apr-1/apr_pools.h>

namespace sig {

extern void (*sig_aggregate)(apr_pool_t *pool, enum Primitive primitive, const char *name, struct Signature *signature, const char *types);

void sig_parse_signature(apr_pool_t *pool, struct Signature *signature, const char **name, char eos);
struct Type *sig_parse_type(apr_pool_t *pool, const char **name, char eos, bool named);

void Parse(apr_pool_t *pool, struct Signature *signature, const char *name);

const char *sig_unparse_signature(apr_pool_t *pool, struct Signature *signature);
const char *sig_unparse_type(apr_pool_t *pool, struct Type *type);

}

#endif/*SIG_PARSE_H*/
