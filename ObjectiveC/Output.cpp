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

#include "Replace.hpp"
#include "ObjectiveC/Syntax.hpp"

#include <sstream>

void CYCategory::Output(CYOutput &out, CYFlags flags) const {
    out << "(function($cys,$cyp,$cyc,$cyn,$cyt){";
    out << "$cyp=object_getClass($cys);";
    out << "$cyc=$cys;";
    if (messages_ != NULL)
        messages_->Output(out, true);
    out << "})(";
    name_->ClassName(out, true);
    out << ')';
    out << ';';
}

void CYClass::Output(CYOutput &out, CYFlags flags) const {
    // XXX: I don't necc. need the ()s
    out << "(function($cys,$cyp,$cyc,$cyn,$cyt,$cym){";
    out << "$cyp=object_getClass($cys);";
    out << "$cyc=objc_allocateClassPair($cys,";
    if (name_ != NULL)
        name_->ClassName(out, false);
    else
        out << "$cyq(\"CY$\")";
    out << ",0);";
    out << "$cym=object_getClass($cyc);";
    if (fields_ != NULL)
        fields_->Output(out);
    if (messages_ != NULL)
        messages_->Output(out, false);
    if (protocols_ != NULL) {
        out << '<';
        out << *protocols_;
        out << '>';
    }
    out << "objc_registerClassPair($cyc);";
    out << "return $cyc;";
    out << "}(";
    if (super_ != NULL)
        super_->Output(out, CYAssign::Precedence_, CYNoFlags);
    else
        out << "null";
    out << "))";
}

void CYClassExpression::Output(CYOutput &out, CYFlags flags) const {
    CYClass::Output(out, flags);
}

void CYClassStatement::Output(CYOutput &out, CYFlags flags) const {
    CYClass::Output(out, flags);
}

void CYField::Output(CYOutput &out) const {
}

void CYImport::Output(CYOutput &out, CYFlags flags) const {
    out << "@import";
}

void CYInstanceLiteral::Output(CYOutput &out, CYFlags flags) const {
    out << '#';
    number_->Output(out, CYRight(flags));
}

void CYMessage::Output(CYOutput &out, bool replace) const {
    out << (instance_ ? '-' : '+');

    CYForEach (parameter, parameters_)
        if (parameter->tag_ != NULL) {
            out << ' ' << *parameter->tag_;
            if (parameter->name_ != NULL)
                out << ':' << *parameter->name_;
        }

    out << code_;
}

void CYModule::Output(CYOutput &out) const {
    out << part_;
    if (next_ != NULL)
        out << '.' << next_;
}

void CYBox::Output(CYOutput &out, CYFlags flags) const {
    out << '@';
    value_->Output(out, Precedence(), CYRight(flags));
}

void CYObjCBlock::Output(CYOutput &out, CYFlags flags) const {
    // XXX: this is seriously wrong
    out << "^(";
    out << ")";
    out << "{";
    out << "}";
}

void CYProtocol::Output(CYOutput &out) const {
    name_->Output(out, CYAssign::Precedence_, CYNoFlags);
    if (next_ != NULL)
        out << ',' << ' ' << *next_;
}

void CYSelector::Output(CYOutput &out, CYFlags flags) const {
    out << "@selector" << '(' << name_ << ')';
}

void CYSelectorPart::Output(CYOutput &out) const {
    out << name_;
    if (value_)
        out << ':';
    out << next_;
}

void CYSend::Output(CYOutput &out, CYFlags flags) const {
    CYForEach (argument, arguments_)
        if (argument->name_ != NULL) {
            out << ' ' << *argument->name_;
            if (argument->value_ != NULL)
                out << ':' << *argument->value_;
        }
}

void CYSendDirect::Output(CYOutput &out, CYFlags flags) const {
    out << '[';
    self_->Output(out, CYAssign::Precedence_, CYNoFlags);
    CYSend::Output(out, flags);
    out << ']';
}

void CYSendSuper::Output(CYOutput &out, CYFlags flags) const {
    out << '[' << "super";
    CYSend::Output(out, flags);
    out << ']';
}
