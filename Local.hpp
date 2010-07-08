/* Cycript - Inlining/Optimizing JavaScript Compiler
 * Copyright (C) 2009  Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
 *        Redistribution and use in source and binary
 * forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation
 *    and/or other materials provided with the
 *    distribution.
 * 3. The name of the author may not be used to endorse
 *    or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#ifndef CYLOCAL_HPP
#define CYLOCAL_HPP

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

#endif/*CYLOCAL_HPP*/
