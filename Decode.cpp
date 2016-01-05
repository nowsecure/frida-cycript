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

#include <sstream>

#include "Decode.hpp"
#include "Replace.hpp"

namespace sig {

template <>
CYTypedIdentifier *Primitive<bool>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeVariable("bool"));
}

template <>
CYTypedIdentifier *Primitive<char>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeCharacter(CYTypeNeutral));
}

template <>
CYTypedIdentifier *Primitive<double>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeVariable("double"));
}

template <>
CYTypedIdentifier *Primitive<float>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeVariable("float"));
}

template <>
CYTypedIdentifier *Primitive<signed char>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeCharacter(CYTypeSigned));
}

template <>
CYTypedIdentifier *Primitive<signed int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeSigned, 1));
}

#ifdef __SIZEOF_INT128__
template <>
CYTypedIdentifier *Primitive<signed __int128>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeInt128(CYTypeSigned));
}
#endif

template <>
CYTypedIdentifier *Primitive<signed long int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeSigned, 2));
}

template <>
CYTypedIdentifier *Primitive<signed long long int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeSigned, 3));
}

template <>
CYTypedIdentifier *Primitive<signed short int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeSigned, 0));
}

template <>
CYTypedIdentifier *Primitive<unsigned char>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeCharacter(CYTypeUnsigned));
}

template <>
CYTypedIdentifier *Primitive<unsigned int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeUnsigned, 1));
}

#ifdef __SIZEOF_INT128__
template <>
CYTypedIdentifier *Primitive<unsigned __int128>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeInt128(CYTypeUnsigned));
}
#endif

template <>
CYTypedIdentifier *Primitive<unsigned long int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeUnsigned, 2));
}

template <>
CYTypedIdentifier *Primitive<unsigned long long int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeUnsigned, 3));
}

template <>
CYTypedIdentifier *Primitive<unsigned short int>::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeIntegral(CYTypeUnsigned, 0));
}

CYTypedIdentifier *Void::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeVoid());
}

CYTypedIdentifier *Unknown::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeError());
}

CYTypedIdentifier *String::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeCharacter(CYTypeNeutral), $ CYTypePointerTo());
}

#ifdef CY_OBJECTIVEC
CYTypedIdentifier *Meta::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeVariable("Class"));
}

CYTypedIdentifier *Selector::Decode(CYPool &pool) const {
    return $ CYTypedIdentifier($ CYTypeVariable("SEL"));
}
#endif

CYTypedIdentifier *Bits::Decode(CYPool &pool) const {
    _assert(false);
}

CYTypedIdentifier *Pointer::Decode(CYPool &pool) const {
    return CYDecodeType(pool, &type)->Modify($ CYTypePointerTo());
}

CYTypedIdentifier *Array::Decode(CYPool &pool) const {
    return CYDecodeType(pool, &type)->Modify($ CYTypeArrayOf($D(size)));
}

#ifdef CY_OBJECTIVEC
CYTypedIdentifier *Object::Decode(CYPool &pool) const {
    if (name == NULL)
        return $ CYTypedIdentifier($ CYTypeVariable("id"));
    else
        return $ CYTypedIdentifier($ CYTypeVariable(name), $ CYTypePointerTo());
}
#endif

CYTypedIdentifier *Enum::Decode(CYPool &pool) const {
    CYEnumConstant *values(NULL);
    for (size_t i(count); i != 0; --i)
        values = $ CYEnumConstant($I(pool.strdup(constants[i - 1].name)), $D(constants[i - 1].value), values);
    CYIdentifier *identifier(name == NULL ? NULL : $I(name));
    CYTypedIdentifier *typed(type.Decode(pool));
    _assert(typed->modifier_ == NULL);
    return $ CYTypedIdentifier($ CYTypeEnum(identifier, typed->specifier_, values));
}

CYTypedIdentifier *Aggregate::Decode(CYPool &pool) const {
    _assert(!overlap);

    if (signature.count == _not(size_t)) {
        _assert(name != NULL);
        return $ CYTypedIdentifier($ CYTypeReference(CYTypeReferenceStruct, $I($pool.strdup(name))));
    }

    CYTypeStructField *fields(NULL);
    for (size_t i(signature.count); i != 0; --i) {
        sig::Element &element(signature.elements[i - 1]);
        CYTypedIdentifier *typed(CYDecodeType(pool, element.type));
        if (element.name != NULL)
            typed->identifier_ = $I(element.name);
        fields = $ CYTypeStructField(typed, fields);
    }
    CYIdentifier *identifier(name == NULL ? NULL : $I(name));
    return $ CYTypedIdentifier($ CYTypeStruct(identifier, $ CYStructTail(fields)));
}

CYTypedIdentifier *Callable::Decode(CYPool &pool) const {
    _assert(signature.count != 0);
    CYTypedParameter *parameters(NULL);
    for (size_t i(signature.count - 1); i != 0; --i)
        parameters = $ CYTypedParameter(CYDecodeType(pool, signature.elements[i].type), parameters);
    return Modify(pool, CYDecodeType(pool, signature.elements[0].type), parameters);
}

CYTypedIdentifier *Function::Modify(CYPool &pool, CYTypedIdentifier *result, CYTypedParameter *parameters) const {
    return result->Modify($ CYTypeFunctionWith(variadic, parameters));
}

#ifdef CY_OBJECTIVEC
CYTypedIdentifier *Block::Modify(CYPool &pool, CYTypedIdentifier *result, CYTypedParameter *parameters) const {
    return result->Modify($ CYTypeBlockWith(parameters));
}

CYTypedIdentifier *Block::Decode(CYPool &pool) const {
    if (signature.count == 0)
        return $ CYTypedIdentifier($ CYTypeVariable("NSBlock"), $ CYTypePointerTo());
    return Callable::Decode(pool);
}
#endif

}

CYTypedIdentifier *CYDecodeType(CYPool &pool, struct sig::Type *type) {
    CYTypedIdentifier *typed(type->Decode(pool));
    if ((type->flags & JOC_TYPE_CONST) != 0) {
        if (dynamic_cast<sig::String *>(type) != NULL)
            typed->modifier_ = $ CYTypeConstant(typed->modifier_);
        else
            typed = typed->Modify($ CYTypeConstant());
    }
    return typed;
}
