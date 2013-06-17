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

#ifndef CYCRIPT_STANDARD_HPP
#define CYCRIPT_STANDARD_HPP

#define _not(type) \
    ((type) ~ (type) 0)

#define _finline \
    inline __attribute__((__always_inline__))
#define _disused \
    __attribute__((__unused__))

#define _label__(x) _label ## x
#define _label_(y) _label__(y)
#define _label _label_(__LINE__)

#define _packed \
    __attribute__((__packed__))
#define _noreturn \
    __attribute__((__noreturn__))

#endif/*CYCRIPT_STANDARD_HPP*/
