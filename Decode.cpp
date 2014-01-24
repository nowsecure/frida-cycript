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

#include <sstream>

#include "Decode.hpp"
#include "Replace.hpp"

CYTypedIdentifier *Decode_(CYPool &pool, struct sig::Type *type) {
    switch (type->primitive) {
        case sig::unknown_P: return $ CYTypedIdentifier($ CYTypeError());

        case sig::function_P: {
            _assert(type->data.signature.count != 0);
            CYTypedParameter *parameter(NULL);
            for (size_t i(type->data.signature.count - 1); i != 0; --i)
                parameter = $ CYTypedParameter(Decode(pool, type->data.signature.elements[i].type), parameter);
            return Decode(pool, type->data.signature.elements[0].type)->Modify($ CYTypeFunctionWith(parameter));
        } break;

        case sig::typename_P: return $ CYTypedIdentifier($ CYTypeVariable("Class"));
        case sig::union_P: _assert(false); break;
        case sig::string_P: return $ CYTypedIdentifier($ CYTypeVariable("char"), $ CYTypePointerTo());
        case sig::selector_P: return $ CYTypedIdentifier($ CYTypeVariable("SEL"));

        case sig::block_P: {
            if (type->data.signature.count == 0)
                return $ CYTypedIdentifier($ CYTypeVariable("NSBlock"), $ CYTypePointerTo());
            else {
                CYTypedParameter *parameter(NULL);
                for (size_t i(type->data.signature.count - 1); i != 0; --i)
                    parameter = $ CYTypedParameter(Decode(pool, type->data.signature.elements[i].type), parameter);
                return Decode(pool, type->data.signature.elements[0].type)->Modify($ CYTypeBlockWith(parameter));
            }
        } break;

        case sig::object_P: {
            if (type->name == NULL)
                return $ CYTypedIdentifier($ CYTypeVariable("id"));
            else
                return $ CYTypedIdentifier($ CYTypeVariable(type->name), $ CYTypePointerTo());
        } break;

        case sig::boolean_P: return $ CYTypedIdentifier($ CYTypeVariable("bool"));
        case sig::uchar_P: return $ CYTypedIdentifier($ CYTypeUnsigned($ CYTypeVariable("char")));
        case sig::uint_P: return $ CYTypedIdentifier($ CYTypeUnsigned($ CYTypeVariable("int")));
        case sig::ulong_P: return $ CYTypedIdentifier($ CYTypeUnsigned($ CYTypeLong($ CYTypeVariable("int"))));
        case sig::ulonglong_P: return $ CYTypedIdentifier($ CYTypeUnsigned($ CYTypeLong($ CYTypeLong($ CYTypeVariable("int")))));
        case sig::ushort_P: return $ CYTypedIdentifier($ CYTypeUnsigned($ CYTypeShort($ CYTypeVariable("int"))));
        case sig::array_P: return Decode(pool, type->data.data.type)->Modify($ CYTypeArrayOf($D(type->data.data.size)));

        case sig::pointer_P: {
            CYTypedIdentifier *typed;
            if (type->data.data.type == NULL)
                typed = $ CYTypedIdentifier($ CYTypeVoid());
            else
                typed = Decode(pool, type->data.data.type);
            return typed->Modify($ CYTypePointerTo());
        } break;

        case sig::bit_P: _assert(false); break;
        case sig::char_P: return $ CYTypedIdentifier($ CYTypeVariable("char"));
        case sig::double_P: return $ CYTypedIdentifier($ CYTypeVariable("double"));
        case sig::float_P: return $ CYTypedIdentifier($ CYTypeVariable("float"));
        case sig::int_P: return $ CYTypedIdentifier($ CYTypeVariable("int"));
        case sig::long_P: return $ CYTypedIdentifier($ CYTypeLong($ CYTypeVariable("int")));
        case sig::longlong_P: return $ CYTypedIdentifier($ CYTypeLong($ CYTypeLong($ CYTypeVariable("int"))));
        case sig::short_P: return $ CYTypedIdentifier($ CYTypeShort($ CYTypeVariable("int")));

        case sig::void_P: return $ CYTypedIdentifier($ CYTypeVoid());

        case sig::struct_P: {
            _assert(type->name != NULL);
            return $ CYTypedIdentifier($ CYTypeVariable(type->name));
        } break;
    }

    _assert(false);
    return NULL;
}

CYTypedIdentifier *Decode(CYPool &pool, struct sig::Type *type) {
    CYTypedIdentifier *typed(Decode_(pool, type));
    if ((type->flags & JOC_TYPE_CONST) != 0) {
        if (type->primitive == sig::string_P)
            typed->modifier_ = $ CYTypeConstant(typed->modifier_);
        else
            typed = typed->Modify($ CYTypeConstant());
    }
    return typed;
}
