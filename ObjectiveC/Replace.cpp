/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2010  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include "Replace.hpp"
#include "ObjectiveC/Syntax.hpp"

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
    CYForEach (part, this) {
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
