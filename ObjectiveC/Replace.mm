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

CYStatement *CYCategory::Replace(CYContext &context) {
    CYVariable *cyc($V("$cyc")), *cys($V("$cys"));

    return $E($C1($F(NULL, $P5("$cys", "$cyp", "$cyc", "$cyn", "$cyt"), $$->*
        $E($ CYAssign($V("$cyp"), $C1($V("object_getClass"), cys)))->*
        $E($ CYAssign(cyc, cys))->*
        $E($ CYAssign($V("$cym"), $C1($V("object_getClass"), cyc)))->*
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
        protocols_->Replace(context)->*
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

CYStatement *CYImport::Replace(CYContext &context) {
    return this;
}

CYStatement *CYMessage::Replace(CYContext &context, bool replace) const { $T(NULL)
    CYVariable *cyn($V("$cyn"));
    CYVariable *cyt($V("$cyt"));
    CYVariable *self($V("self"));
    CYVariable *_class($V(instance_ ? "$cys" : "$cyp"));

    return $ CYBlock($$->*
        next_->Replace(context, replace)->*
        $E($ CYAssign(cyn, parameters_->Selector(context)))->*
        $E($ CYAssign(cyt, $C1($M(cyn, $S("type")), _class)))->*
        $E($C4($V(replace ? "class_replaceMethod" : "class_addMethod"),
            $V(instance_ ? "$cyc" : "$cym"),
            cyn,
            $N2($V("Functor"), $F(NULL, $P2("self", "_cmd", parameters_->Parameters(context)), $$->*
                $ CYVar($L1($L($I("$cyr"), $N2($V("Super"), self, _class))))->*
                $ CYReturn($C1($M($F(NULL, NULL, code_), $S("call")), self))
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

CYStatement *CYProtocol::Replace(CYContext &context) const { $T(NULL)
    return $ CYBlock($$->*
        next_->Replace(context)->*
        $E($C2($V("class_addProtocol"),
            $V("$cyc"), name_
        ))
    );
}

CYExpression *CYSelector::Replace(CYContext &context) {
    return $N1($V("Selector"), name_->Replace(context));
}

CYString *CYSelectorPart::Replace(CYContext &context) {
    std::ostringstream str;
    for (const CYSelectorPart *part(this); part != NULL; part = part->next_) {
        if (part->name_ != NULL)
            str << part->name_->Word();
        if (part->value_)
            str << ':';
    }
    return $S(apr_pstrdup($pool, str.str().c_str()));
}

CYExpression *CYSendDirect::Replace(CYContext &context) {
    std::ostringstream name;
    CYArgument **argument(&arguments_);
    CYSelectorPart *selector(NULL), *current(NULL);

    while (*argument != NULL) {
        if ((*argument)->name_ != NULL) {
            CYSelectorPart *part($ CYSelectorPart((*argument)->name_, (*argument)->value_ != NULL));
            if (selector == NULL)
                selector = part;
            if (current != NULL)
                current->SetNext(part);
            current = part;
            (*argument)->name_ = NULL;
        }

        if ((*argument)->value_ == NULL)
            *argument = (*argument)->next_;
        else
            argument = &(*argument)->next_;
    }

    return $C2($V("objc_msgSend"), self_, ($ CYSelector(selector))->Replace(context), arguments_);
}

CYExpression *CYSendSuper::Replace(CYContext &context) {
    return $ CYSendDirect($V("cyr"), arguments_);
}
