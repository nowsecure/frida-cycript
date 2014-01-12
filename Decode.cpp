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
        case sig::function_P: {
            _assert(type->data.signature.count != 0);
            CYTypedParameter *parameter(NULL);
            for (size_t i(type->data.signature.count - 1); i != 0; --i)
                parameter = $ CYTypedParameter(Decode(pool, type->data.signature.elements[i].type), parameter);
            return Decode(pool, type->data.signature.elements[0].type)->Modify($ CYTypeFunctionWith(parameter));
        } break;

        case sig::typename_P: return $ CYTypedIdentifier($V("Class"));
        case sig::union_P: _assert(false); break;
        case sig::string_P: return $ CYTypedIdentifier($V("char"), $ CYTypeConstant($ CYTypePointerTo()));
        case sig::selector_P: return $ CYTypedIdentifier($V("SEL"));
        case sig::block_P: _assert(false); break;

        case sig::object_P: {
            if (type->name == NULL)
                return $ CYTypedIdentifier($V("id"));
            else
                return $ CYTypedIdentifier($V(type->name), $ CYTypePointerTo());
        } break;

        case sig::boolean_P: return $ CYTypedIdentifier($V("bool"));
        case sig::uchar_P: return $ CYTypedIdentifier($V("uchar"));
        case sig::uint_P: return $ CYTypedIdentifier($V("uint"));
        case sig::ulong_P: return $ CYTypedIdentifier($V("ulong"));
        case sig::ulonglong_P: return $ CYTypedIdentifier($V("ulonglong"));
        case sig::ushort_P: return $ CYTypedIdentifier($V("ushort"));
        case sig::array_P: return Decode(pool, type->data.data.type)->Modify($ CYTypeArrayOf($D(type->data.data.size)));

        // XXX: once again, the issue of void here is incorrect
        case sig::pointer_P: {
            CYTypedIdentifier *typed;
            if (type->data.data.type == NULL)
                typed = $ CYTypedIdentifier($V("void"));
            else
                typed = Decode(pool, type->data.data.type);
            return typed->Modify($ CYTypePointerTo());
        } break;

        case sig::bit_P: _assert(false); break;
        case sig::char_P: return $ CYTypedIdentifier($V("char"));
        case sig::double_P: return $ CYTypedIdentifier($V("double"));
        case sig::float_P: return $ CYTypedIdentifier($V("float"));
        case sig::int_P: return $ CYTypedIdentifier($V("int"));
        case sig::long_P: return $ CYTypedIdentifier($V("long"));
        case sig::longlong_P: return $ CYTypedIdentifier($V("longlong"));
        case sig::short_P: return $ CYTypedIdentifier($V("short"));

        // XXX: this happens to work, but is totally wrong
        case sig::void_P: return $ CYTypedIdentifier($V("void"));

        case sig::struct_P: {
            _assert(type->name != NULL);
            return $ CYTypedIdentifier($V(type->name));
        } break;
    }

    _assert(false);
    return NULL;
}

CYTypedIdentifier *Decode(CYPool &pool, struct sig::Type *type) {
    CYTypedIdentifier *typed(Decode_(pool, type));
    if ((type->flags & JOC_TYPE_CONST) != 0)
        typed = typed->Modify($ CYTypeConstant());
    return typed;
}
