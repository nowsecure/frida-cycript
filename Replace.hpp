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

#ifndef CYCRIPT_REPLACE_HPP
#define CYCRIPT_REPLACE_HPP

#include "Parser.hpp"

#define $ new($pool)

#define $D(args...) \
    ($ CYNumber(args))
#define $E(args...) \
    ($ CYExpress(args))
#define $F(args...) \
    ($ CYFunctionExpression(args))
#define $I(args...) \
    ($ CYIdentifier(args))
#define $M(args...) \
    ($ CYDirectMember(args))
#define $P(args...) \
    ($ CYFunctionParameter(args))
#define $S(args...) \
    ($ CYString(args))
#define $U \
    $V($I("undefined"))
#define $V(name) \
    ($ CYVariable(name))

#define $T(value) \
    if (this == NULL) \
        return value;
#define $$ \
    CYStatements()

#define $P1(arg0, args...) \
    $P(arg0, ##args)
#define $P2(arg0, arg1, args...) \
    $P(arg0, $P1(arg1, ##args))
#define $P3(arg0, arg1, arg2, args...) \
    $P(arg0, $P2(arg1, arg2, ##args))
#define $P4(arg0, arg1, arg2, arg3, args...) \
    $P(arg0, $P3(arg1, arg2, arg3, ##args))
#define $P5(arg0, arg1, arg2, arg3, arg4, args...) \
    $P(arg0, $P4(arg1, arg2, arg3, arg4, ##args))
#define $P6(arg0, arg1, arg2, arg3, arg4, arg5, args...) \
    $P(arg0, $P5(arg1, arg2, arg3, arg4, arg5, ##args))

#define $C(args...) \
    ($ CYCall(args))
#define $C_(args...) \
    ($ CYArgument(args))
#define $N(args...) \
    ($ cy::Syntax::New(args))

#define $C1_(arg0, args...) \
    $C_(arg0, ##args)
#define $C2_(arg0, arg1, args...) \
    $C_(arg0, $C1_(arg1, ##args))
#define $C3_(arg0, arg1, arg2, args...) \
    $C_(arg0, $C2_(arg1, arg2, ##args))
#define $C4_(arg0, arg1, arg2, arg3, args...) \
    $C_(arg0, $C3_(arg1, arg2, arg3, ##args))
#define $C5_(arg0, arg1, arg2, arg3, arg4, args...) \
    $C_(arg0, $C4_(arg1, arg2, arg3, arg4, ##args))
#define $C6_(arg0, arg1, arg2, arg3, arg4, arg5, args...) \
    $C_(arg0, $C5_(arg1, arg2, arg3, arg4, arg5, ##args))

#define $C0(func, args...) \
    $C(func, ##args)
#define $C1(func, args...) \
    $C(func, $C1_(args))
#define $C2(func, args...) \
    $C(func, $C2_(args))
#define $C3(func, args...) \
    $C(func, $C3_(args))
#define $C4(func, args...) \
    $C(func, $C4_(args))
#define $C5(func, args...) \
    $C(func, $C5_(args))

#define $N0(func, args...) \
    $N(func, ##args)
#define $N1(func, args...) \
    $N(func, $C1_(args))
#define $N2(func, args...) \
    $N(func, $C2_(args))
#define $N3(func, args...) \
    $N(func, $C3_(args))
#define $N4(func, args...) \
    $N(func, $C4_(args))
#define $N5(func, args...) \
    $N(func, $C5_(args))

#define $L(arg0, args...) \
    $ CYDeclaration($I(arg0), ##args)
#define $L1(arg0) \
    $ CYDeclarations(arg0)
#define $L2(arg0, args...) \
    $ CYDeclarations(arg0, $L1(args))
#define $L3(arg0, args...) \
    $ CYDeclarations(arg0, $L2(args))
#define $L4(arg0, args...) \
    $ CYDeclarations(arg0, $L3(args))
#define $L5(arg0, args...) \
    $ CYDeclarations(arg0, $L4(args))

#endif/*CYCRIPT_REPLACE_HPP*/
