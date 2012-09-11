/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2012  Jay Freeman (saurik)
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

#ifndef CYCRIPT_HIGHLIGHT_HPP
#define CYCRIPT_HIGHLIGHT_HPP

#include <iostream>

namespace hi { enum Value {
    Comment,
    Constant,
    Control,
    Escape,
    Identifier,
    Meta,
    Nothing,
    Operator,
    Structure,
    Type,
}; }

void CYLexerHighlight(const char *data, size_t size, std::ostream &output);

#endif/*CYCRIPT_HIGHLIGHT_HPP*/
