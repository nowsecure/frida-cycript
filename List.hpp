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

#ifndef CYCRIPT_LIST_HPP
#define CYCRIPT_LIST_HPP

template <typename Type_>
struct CYNext {
    Type_ *next_;

    CYNext() :
        next_(NULL)
    {
    }

    CYNext(Type_ *next) :
        next_(next)
    {
    }

    void SetNext(Type_ *next) {
        next_ = next;
    }
};

template <typename Type_>
Type_ *&CYSetLast(Type_ *&list) {
    if (list == NULL)
        return list;

    Type_ *next(list);
    while (next->next_ != NULL)
        next = next->next_;
    return next->next_;
}

template <typename Type_>
Type_ *CYGetLast(Type_ *list) {
    if (list == NULL)
        return NULL;

    Type_ *next(list);
    while (next->next_ != NULL)
        next = next->next_;
    return next;
}

#define CYForEach(value, list) \
    for (__typeof__(*list) *value(list); value != NULL; value = value->next_)

#endif/*CYCRIPT_LIST_HPP*/
