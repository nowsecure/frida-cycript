#ifndef CYCRIPT_JAVASCRIPT_HPP
#define CYCRIPT_JAVASCRIPT_HPP

extern JSObjectRef Array_;
extern JSObjectRef Error_;
extern JSObjectRef Function_;
extern JSObjectRef String_;

extern JSStringRef length_;
extern JSStringRef message_;
extern JSStringRef name_;
extern JSStringRef prototype_;
extern JSStringRef toCYON_;
extern JSStringRef toJSON_;

extern JSObjectRef Object_prototype_;
extern JSObjectRef Function_prototype_;

extern JSObjectRef Array_prototype_;
extern JSObjectRef Array_pop_;
extern JSObjectRef Array_push_;
extern JSObjectRef Array_splice_;

#endif/*CYCRIPT_JAVASCRIPT_HPP*/
