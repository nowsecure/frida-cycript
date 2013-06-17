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

#ifndef CYCRIPT_LOCAL_HPP
#define CYCRIPT_LOCAL_HPP

#include <pthread.h>

template <typename Type_>
class CYLocal {
  private:
    static ::pthread_key_t key_;

    Type_ *last_;

  protected:
    static _finline void Set(Type_ *value) {
        _assert(::pthread_setspecific(key_, value) == 0);
    }

    static ::pthread_key_t Key_() {
        ::pthread_key_t key;
        ::pthread_key_create(&key, NULL);
        return key;
    }

  public:
    CYLocal(Type_ *next) {
        last_ = Get();
        Set(next);
    }

    _finline ~CYLocal() {
        Set(last_);
    }

    static _finline Type_ *Get() {
        return reinterpret_cast<Type_ *>(::pthread_getspecific(key_));
    }
};

template <typename Type_>
::pthread_key_t CYLocal<Type_>::key_ = Key_();

#endif/*CYCRIPT_LOCAL_HPP*/
