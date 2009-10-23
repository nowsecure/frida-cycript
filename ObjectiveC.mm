/* Cycript - Remote Execution Server and Disassembler
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
#include "ObjectiveC.hpp"

#include <objc/runtime.h>
#include <sstream>

/* Objective-C Output {{{ */
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
    out << '[';

    self_->Output(out, CYPA, CYNoFlags);

    for (CYArgument *argument(arguments_); argument != NULL; argument = argument->next_)
        if (argument->name_ != NULL) {
            out << ' ' << *argument->name_;
            if (argument->value_ != NULL)
                out << ':' << *argument->value_;
        }

    out << ']';
}
/* }}} */
/* Objective-C Replace {{{ */
CYStatement *CYCategory::Replace(CYContext &context) {
    CYVariable *cyc($V("$cyc")), *cys($V("$cys"));

    return $E($C1($F(NULL, $P5("$cys", "$cyp", "$cyc", "$cyn", "$cyt"), $$->*
        $E($ CYAssign($V("$cyp"), $C1($V("object_getClass"), cys)))->*
        $E($ CYAssign(cyc, cys))->*
        messages_->Replace(context, true)
    ), name_->ClassName(context, true)));
}

CYExpression *CYClass::Replace_(CYContext &context) {
    CYVariable *cyc($V("$cyc")), *cys($V("$cys"));

    CYExpression *name(name_ != NULL ? name_->ClassName(context, false) : $C1($V("$cyq"), $S("CY$")));

    return $C1($F(NULL, $P6("$cys", "$cyp", "$cyc", "$cyn", "$cyt", "$cym"), $$->*
        $E($ CYAssign($V("$cyp"), $C1($V("object_getClass"), cys)))->*
        $E($ CYAssign(cyc, $C3($V("objc_allocateClassPair"), cys, name, $D(0))))->*
        $E($ CYAssign($V("$cym"), $C1($V("object_getClass"), cyc)))->*
        fields_->Replace(context)->*
        messages_->Replace(context, false)->*
        $E($C1($V("objc_registerClassPair"), cyc))->*
        $ CYReturn(cyc)
    ), super_ == NULL ? $ CYNull() : super_);
}

CYExpression *CYClassExpression::Replace(CYContext &context) {
    return Replace_(context);
}

CYStatement *CYClassStatement::Replace(CYContext &context) {
    return $E(Replace_(context));
}

CYStatement *CYField::Replace(CYContext &context) const {
    return NULL;
}

CYStatement *CYMessage::Replace(CYContext &context, bool replace) const { $T(NULL)
    CYVariable *cyn($V("$cyn"));
    CYVariable *cyt($V("$cyt"));

    return $ CYBlock($$->*
        next_->Replace(context, replace)->*
        $E($ CYAssign(cyn, parameters_->Selector(context)))->*
        $E($ CYAssign(cyt, $C1($M(cyn, $S("type")), $V(instance_ ? "$cys" : "$cyp"))))->*
        $E($C4($V(replace ? "class_replaceMethod" : "class_addMethod"),
            $V(instance_ ? "$cyc" : "$cym"),
            cyn,
            $N2($V("Functor"), $F(NULL, $P2("self", "_cmd", parameters_->Parameters(context)), $$->*
                $ CYReturn($C1($M($F(NULL, NULL, code_), $S("call")), $V("self")))
            ), cyt),
            cyt
        ))
    );
}

CYFunctionParameter *CYMessageParameter::Parameters(CYContext &context) const { $T(NULL)
    CYFunctionParameter *next(next_->Parameters(context));
    return name_ == NULL ? next : $ CYFunctionParameter(name_, next);
}

CYSelector *CYMessageParameter::Selector(CYContext &context) const {
    return $ CYSelector(SelectorPart(context));
}

CYSelectorPart *CYMessageParameter::SelectorPart(CYContext &context) const { $T(NULL)
    CYSelectorPart *next(next_->SelectorPart(context));
    return tag_ == NULL ? next : $ CYSelectorPart(tag_, name_ != NULL, next);
}

CYExpression *CYSelector::Replace(CYContext &context) {
    return $N1($V("Selector"), name_->Replace(context));
}

CYString *CYSelectorPart::Replace(CYContext &context) {
    std::ostringstream str;
    for (const CYSelectorPart *part(this); part != NULL; part = part->next_) {
        if (part->name_ != NULL)
            str << part->name_->Value();
        if (part->value_)
            str << ':';
    }
    return $S(apr_pstrdup(context.pool_, str.str().c_str()));
}

CYExpression *CYSend::Replace(CYContext &context) {
    std::ostringstream name;
    CYArgument **argument(&arguments_);

    while (*argument != NULL) {
        if ((*argument)->name_ != NULL) {
            name << *(*argument)->name_;
            (*argument)->name_ = NULL;
            if ((*argument)->value_ != NULL)
                name << ':';
        }

        if ((*argument)->value_ == NULL)
            *argument = (*argument)->next_;
        else
            argument = &(*argument)->next_;
    }

    SEL sel(sel_registerName(name.str().c_str()));
    double address(static_cast<double>(reinterpret_cast<uintptr_t>(sel)));

    return $C2($V("objc_msgSend"), self_, $D(address), arguments_);
}
/* }}} */
