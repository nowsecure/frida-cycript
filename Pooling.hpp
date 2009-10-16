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
        _aprcall(apr_pool_create(&pool_, NULL));
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

#endif/*CYPOOLING_HPP*/
