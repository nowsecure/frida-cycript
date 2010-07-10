/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2010  Jay Freeman (saurik)
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

#ifndef CYPOOLING_HPP
#define CYPOOLING_HPP

#include <apr_pools.h>
#include <apr_strings.h>

#include "Exception.hpp"
#include "Local.hpp"
#include "Standard.hpp"

#include <cstdlib>

_finline void *operator new(size_t size, apr_pool_t *pool) {
    return apr_palloc(pool, size);
}

_finline void *operator new [](size_t size, apr_pool_t *pool) {
    return apr_palloc(pool, size);
}

class CYPool {
  private:
    apr_pool_t *pool_;

  public:
    CYPool(apr_pool_t *pool = NULL) {
        _aprcall(apr_pool_create(&pool_, pool));
    }

    ~CYPool() {
        apr_pool_destroy(pool_);
    }

    void Clear() {
        apr_pool_clear(pool_);
    }

    operator apr_pool_t *() const {
        return pool_;
    }

    char *operator ()(const char *data) const {
        return apr_pstrdup(pool_, data);
    }

    char *operator ()(const char *data, size_t size) const {
        return apr_pstrndup(pool_, data, size);
    }
};

struct CYData {
    apr_pool_t *pool_;
    unsigned count_;

    CYData() :
        count_(1)
    {
    }

    virtual ~CYData() {
    }

    static void *operator new(size_t size, apr_pool_t *pool) {
        void *data(apr_palloc(pool, size));
        reinterpret_cast<CYData *>(data)->pool_ = pool;
        return data;
    }

    static void *operator new(size_t size) {
        apr_pool_t *pool;
        _aprcall(apr_pool_create(&pool, NULL));
        return operator new(size, pool);
    }

    static void operator delete(void *data) {
        apr_pool_destroy(reinterpret_cast<CYData *>(data)->pool_);
    }
};

template <typename Type_>
struct CYPoolAllocator {
    apr_pool_t *pool_;

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
        return reinterpret_cast<pointer>(apr_palloc(pool_, size));
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
    CYLocal<apr_pool_t> local_;

  public:
    CYLocalPool() :
        CYPool(),
        local_(operator apr_pool_t *())
    {
    }
};

#define $pool \
    CYLocal<apr_pool_t>::Get()

#endif/*CYPOOLING_HPP*/
