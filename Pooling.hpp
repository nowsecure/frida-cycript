#ifndef CYPOOLING_HPP
#define CYPOOLING_HPP

#include <apr-1/apr_pools.h>
#include <apr-1/apr_strings.h>

#include <minimal/stdlib.h>

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
    CYPool() {
        apr_pool_create(&pool_, NULL);
    }

    ~CYPool() {
        apr_pool_destroy(pool_);
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

#endif/*CYPOOLING_HPP*/
