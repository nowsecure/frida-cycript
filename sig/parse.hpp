/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2010  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#ifndef SIG_PARSE_H
#define SIG_PARSE_H

#include "sig/types.hpp"

#include <apr_pools.h>

namespace sig {

typedef void (*Callback)(apr_pool_t *pool, Type *&type);
void Parse(apr_pool_t *pool, struct Signature *signature, const char *name, Callback callback);

const char *Unparse(apr_pool_t *pool, struct Signature *signature);
const char *Unparse(apr_pool_t *pool, struct Type *type);

void Copy(apr_pool_t *pool, Type &lhs, Type &rhs);
void Copy(apr_pool_t *pool, Signature &lhs, Signature &rhs);
void Copy(apr_pool_t *pool, Type &lhs, Type &rhs);

}

#endif/*SIG_PARSE_H*/
