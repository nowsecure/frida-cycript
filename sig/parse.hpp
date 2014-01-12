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

#ifndef SIG_PARSE_H
#define SIG_PARSE_H

#include "Pooling.hpp"
#include "sig/types.hpp"

namespace sig {

typedef void (*Callback)(CYPool &pool, Type *&type);
void Parse(CYPool &pool, struct Signature *signature, const char *name, Callback callback);

const char *Unparse(CYPool &pool, struct Signature *signature);
const char *Unparse(CYPool &pool, struct Type *type);

void Copy(CYPool &pool, Type &lhs, const Type &rhs);
void Copy(CYPool &pool, Signature &lhs, const Signature &rhs);
void Copy(CYPool &pool, Type &lhs, const Type &rhs);

}

#endif/*SIG_PARSE_H*/
