/* Cycript - Inlining/Optimizing JavaScript Compiler
 * Copyright (C) 2009  Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
 *        Redistribution and use in source and binary
 * forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation
 *    and/or other materials provided with the
 *    distribution.
 * 3. The name of the author may not be used to endorse
 *    or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#include "Replace.hpp"
#include "ObjectiveC/Syntax.hpp"

#include <Foundation/Foundation.h>
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
    out << "objc_registerClassPair($cyc);";
    out << "return $cyc;";
    out << "}(";
    if (super_ != NULL)
        super_->Output(out, CYPA, CYNoFlags);
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

void CYMessage::Output(CYOutput &out, bool replace) const {
    out << (instance_ ? '-' : '+');

    for (CYMessageParameter *parameter(parameters_); parameter != NULL; parameter = parameter->next_)
        if (parameter->tag_ != NULL) {
            out << ' ' << *parameter->tag_;
            if (parameter->name_ != NULL)
                out << ':' << *parameter->name_;
        }

    out << code_;
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
    for (CYArgument *argument(arguments_); argument != NULL; argument = argument->next_)
        if (argument->name_ != NULL) {
            out << ' ' << *argument->name_;
            if (argument->value_ != NULL)
                out << ':' << *argument->value_;
        }
}

void CYSendDirect::Output(CYOutput &out, CYFlags flags) const {
    out << '[';
    self_->Output(out, CYPA, CYNoFlags);
    CYSend::Output(out, flags);
    out << ']';
}

void CYSendSuper::Output(CYOutput &out, CYFlags flags) const {
    out << '[' << "super";
    CYSend::Output(out, flags);
    out << ']';
}
