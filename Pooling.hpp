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

#ifndef CYCRIPT_POOLING_HPP
#define CYCRIPT_POOLING_HPP

#include <cstdarg>
#include <cstdlib>

#include <apr_pools.h>
#include <apr_strings.h>

#include "Exception.hpp"
#include "Local.hpp"
#include "Standard.hpp"

class CYPool {
  private:
    apr_pool_t *pool_;

    struct Cleaner {
        Cleaner *next_;
        void (*code_)(void *);
        void *data_;

        Cleaner(Cleaner *next, void (*code)(void *), void *data) :
            next_(next),
            code_(code),
            data_(data)
        {
        }
    } *cleaner_;

  public:
    CYPool() :
        cleaner_(NULL)
    {
        _aprcall(apr_pool_create(&pool_, NULL));
    }

    ~CYPool() {
        for (Cleaner *cleaner(cleaner_); cleaner_ != NULL; cleaner_ = cleaner_->next_)
            (*cleaner->code_)(cleaner->data_);
        apr_pool_destroy(pool_);
    }

    void Clear() {
        apr_pool_clear(pool_);
    }

    operator apr_pool_t *() const {
        return pool_;
    }

    void *operator()(size_t size) const {
        return apr_palloc(pool_, size);
    }

    char *strdup(const char *data) const {
        return apr_pstrdup(pool_, data);
    }

    char *strndup(const char *data, size_t size) const {
        return apr_pstrndup(pool_, data, size);
    }

    char *strmemdup(const char *data, size_t size) const {
        return apr_pstrmemdup(pool_, data, size);
    }

    char *sprintf(const char *format, ...) const {
        va_list args;
        va_start(args, format);
        char *data(vsprintf(format, args));
        va_end(args);
        return data;
    }

    char *vsprintf(const char *format, va_list args) const {
        return apr_pvsprintf(pool_, format, args);
    }

    void atexit(void (*code)(void *), void *data = NULL);
};

_finline void *operator new(size_t size, CYPool &pool) {
    return pool(size);
}

_finline void *operator new [](size_t size, CYPool &pool) {
    return pool(size);
}

_finline void CYPool::atexit(void (*code)(void *), void *data) {
    cleaner_ = new(*this) Cleaner(cleaner_, code, data);
}

struct CYData {
    CYPool *pool_;
    unsigned count_;

    CYData() :
        count_(1)
    {
    }

    CYData(CYPool &pool) :
        pool_(&pool),
        count_(_not(unsigned))
    {
    }

    virtual ~CYData() {
    }

    static void *operator new(size_t size, CYPool &pool) {
        void *data(pool(size));
        reinterpret_cast<CYData *>(data)->pool_ = &pool;
        return data;
    }

    static void *operator new(size_t size) {
        return operator new(size, *new CYPool());
    }

    static void operator delete(void *data) {
        delete reinterpret_cast<CYData *>(data)->pool_;
    }
};

template <typename Type_>
struct CYPoolAllocator {
    CYPool *pool_;

    typedef Type_ value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    CYPoolAllocator() :
        pool_(NULL)
    {
    }

    template <typename Right_>
    CYPoolAllocator(const CYPoolAllocator<Right_> &rhs) :
        pool_(rhs.pool_)
    {
    }

    pointer allocate(size_type size, const void *hint = 0) {
        return reinterpret_cast<pointer>((*pool_)(size));
    }

    void deallocate(pointer data, size_type size) {
    }

    void construct(pointer address, const Type_ &rhs) {
        new(address) Type_(rhs);
    }

    void destroy(pointer address) {
        address->~Type_();
    }

    template <typename Right_>
    inline bool operator==(const CYPoolAllocator<Right_> &rhs) {
        return pool_ == rhs.pool_;
    }

    template <typename Right_>
    inline bool operator!=(const CYPoolAllocator<Right_> &rhs) {
        return !operator==(rhs);
    }

    template <typename Right_>
    struct rebind {
        typedef CYPoolAllocator<Right_> other;
    };
};

class CYLocalPool :
    public CYPool
{
  private:
    CYLocal<CYPool> local_;

  public:
    CYLocalPool() :
        CYPool(),
        local_(this)
    {
    }
};

#define $pool \
    (*CYLocal<CYPool>::Get())

#endif/*CYCRIPT_POOLING_HPP*/
