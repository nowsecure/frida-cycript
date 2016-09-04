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

#ifndef CYCRIPT_REPLACE_HPP
#define CYCRIPT_REPLACE_HPP

#include "Syntax.hpp"

#define $ new($pool)

#define $D(...) \
    ($ CYNumber(__VA_ARGS__))
#define $E(...) \
    ($ CYExpress(__VA_ARGS__))
#define $F(...) \
    ($ CYFunctionExpression(__VA_ARGS__))
#define $I(...) \
    ($ CYIdentifier(__VA_ARGS__))
#define $M(...) \
    ($ CYDirectMember(__VA_ARGS__))
#define $P(...) \
    ($ CYFunctionParameter(__VA_ARGS__))
#define $S(...) \
    ($ CYString(__VA_ARGS__))
#define $U \
    $V($I("undefined"))
#define $V(name) \
    ($ CYVariable(name))

#define $T(...) \
    if (this == NULL) \
        return __VA_ARGS__;
#define $$ \
    CYStatements()

#define $P1(arg0, ...) \
    $P(arg0 CY_VA_ARGS(__VA_ARGS__))
#define $P2(arg0, arg1, ...) \
    $P(arg0, $P1(arg1 CY_VA_ARGS(__VA_ARGS__)))
#define $P3(arg0, arg1, arg2, ...) \
    $P(arg0, $P2(arg1, arg2 CY_VA_ARGS(__VA_ARGS__)))
#define $P4(arg0, arg1, arg2, arg3, ...) \
    $P(arg0, $P3(arg1, arg2, arg3 CY_VA_ARGS(__VA_ARGS__)))
#define $P5(arg0, arg1, arg2, arg3, arg4, ...) \
    $P(arg0, $P4(arg1, arg2, arg3, arg4 CY_VA_ARGS(__VA_ARGS__)))
#define $P6(arg0, arg1, arg2, arg3, arg4, arg5, ...) \
    $P(arg0, $P5(arg1, arg2, arg3, arg4, arg5 CY_VA_ARGS(__VA_ARGS__)))

#define $C(...) \
    ($ CYCall(__VA_ARGS__))
#define $C_(...) \
    ($ CYArgument(__VA_ARGS__))
#define $N(...) \
    ($ cy::Syntax::New(__VA_ARGS__))

#define $C1_(arg0, ...) \
    $C_(arg0 CY_VA_ARGS(__VA_ARGS__))
#define $C2_(arg0, arg1, ...) \
    $C_(arg0, $C1_(arg1 CY_VA_ARGS(__VA_ARGS__)))
#define $C3_(arg0, arg1, arg2, ...) \
    $C_(arg0, $C2_(arg1, arg2 CY_VA_ARGS(__VA_ARGS__)))
#define $C4_(arg0, arg1, arg2, arg3, ...) \
    $C_(arg0, $C3_(arg1, arg2, arg3 CY_VA_ARGS(__VA_ARGS__)))
#define $C5_(arg0, arg1, arg2, arg3, arg4, ...) \
    $C_(arg0, $C4_(arg1, arg2, arg3, arg4 CY_VA_ARGS(__VA_ARGS__)))
#define $C6_(arg0, arg1, arg2, arg3, arg4, arg5, ...) \
    $C_(arg0, $C5_(arg1, arg2, arg3, arg4, arg5 CY_VA_ARGS(__VA_ARGS__)))

#define $C0(func, ...) \
    $C(func CY_VA_ARGS(__VA_ARGS__))
#define $C1(func, ...) \
    $C(func, $C1_(__VA_ARGS__))
#define $C2(func, ...) \
    $C(func, $C2_(__VA_ARGS__))
#define $C3(func, ...) \
    $C(func, $C3_(__VA_ARGS__))
#define $C4(func, ...) \
    $C(func, $C4_(__VA_ARGS__))
#define $C5(func, ...) \
    $C(func, $C5_(__VA_ARGS__))

#define $N0(func, ...) \
    $N(func CY_VA_ARGS(__VA_ARGS__))
#define $N1(func, ...) \
    $N(func, $C1_(__VA_ARGS__))
#define $N2(func, ...) \
    $N(func, $C2_(__VA_ARGS__))
#define $N3(func, ...) \
    $N(func, $C3_(__VA_ARGS__))
#define $N4(func, ...) \
    $N(func, $C4_(__VA_ARGS__))
#define $N5(func, ...) \
    $N(func, $C5_(__VA_ARGS__))

#define $B(...) \
    $ CYBinding(__VA_ARGS__)
#define $B1(arg0) \
    $ CYBindings(arg0)
#define $B2(arg0, ...) \
    $ CYBindings(arg0, $B1(__VA_ARGS__))
#define $B3(arg0, ...) \
    $ CYBindings(arg0, $B2(__VA_ARGS__))
#define $B4(arg0, ...) \
    $ CYBindings(arg0, $B3(__VA_ARGS__))
#define $B5(arg0, ...) \
    $ CYBindings(arg0, $B4(__VA_ARGS__))

#endif/*CYCRIPT_REPLACE_HPP*/
