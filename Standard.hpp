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
