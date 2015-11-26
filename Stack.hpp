/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
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

#ifndef YY_CY_STACK_HH_INCLUDED
#define YY_CY_STACK_HH_INCLUDED

namespace cy {

template <class Type_>
class stack {
  public:
    typedef std::reverse_iterator<Type_ *> const_iterator;

  private:
    Type_ *data_;
    size_t size_;
    size_t capacity_;

    void destroy() {
        data_ -= size_;
        for (size_t i(0); i != size_; ++i)
            data_[i].~Type_();
    }

    void reserve(size_t capacity) {
        capacity_ = capacity;
        Type_ *data(static_cast<Type_ *>(::operator new(sizeof(Type_) * capacity_)));

        data_ -= size_;
        for (size_t i(0); i != size_; ++i) {
            Type_ &old(data_[i]);
            new (data + i) Type_(old);
            old.~Type_();
        }

        ::operator delete(data_);
        data_ = data + size_;
    }

  public:
    stack() :
        data_(NULL),
        size_(0)
    {
        reserve(200);
    }

    ~stack() {
        destroy();
        ::operator delete(data_);
    }

    _finline Type_ &operator [](size_t i) {
        return data_[-1 - i];
    }

    _finline const Type_ &operator [](size_t i) const {
        return data_[-1 - i];
    }

    _finline void push(Type_ &t) {
        if (size_ == capacity_)
            reserve(capacity_ * 2);
        new (data_++) Type_(t);
        ++size_;
    }

    _finline void pop() {
        (--data_)->~Type_();
        --size_;
    }

    _finline void pop(unsigned int size) {
        for (; size != 0; --size)
            pop();
    }

    void clear() {
        destroy();
        size_ = 0;
    }

    _finline size_t size() const {
        return size_;
    }

    _finline const_iterator begin() const {
        return const_iterator(data_);
    }

    _finline const_iterator end() const {
        return const_iterator(data_ - size_);
    }

  private:
    stack(const stack &);
    stack &operator =(const stack &);
};

template <class Type_, class Stack_ = stack<Type_> >
class slice {
  public:
    slice(const Stack_ &stack, unsigned int range) :
        stack_(stack),
        range_(range)
    {
    }

    _finline const Type_ &operator [](unsigned int i) const {
        return stack_[range_ - i];
    }

  private:
    const Stack_ &stack_;
    unsigned int range_;
};

}

#endif/*YY_CY_STACK_HH_INCLUDED*/
