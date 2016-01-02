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

#endif/*CYCRIPT_LOCAL_HPP*/
