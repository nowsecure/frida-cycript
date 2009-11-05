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

#include "Parser.hpp"

#include <iomanip>

#include "Replace.hpp"

CYExpression *CYAdd::Replace(CYContext &context) {
    CYInfix::Replace(context);

    CYExpression *lhp(lhs_->Primitive(context));
    CYExpression *rhp(rhs_->Primitive(context));

    CYString *lhs(dynamic_cast<CYString *>(lhp));
    CYString *rhs(dynamic_cast<CYString *>(rhp));

    if (lhs != NULL || rhs != NULL) {
        if (lhs == NULL) {
            lhs = lhp->String(context);
            if (lhs == NULL)
                return NULL;
        } else if (rhs == NULL) {
            rhs = rhp->String(context);
            if (rhs == NULL)
                return NULL;
        }

        return lhs->Concat(context, rhs);
    }

    if (CYNumber *lhn = lhp->Number(context))
        if (CYNumber *rhn = rhp->Number(context))
            return $D(lhn->Value() + rhn->Value());

    return NULL;
}

CYExpression *CYAddressOf::Replace(CYContext &context) {
    CYPrefix::Replace(context);
    return $C0($M(rhs_, $S("$cya")));
}

void CYArgument::Replace(CYContext &context) { $T()
    context.Replace(value_);
    next_->Replace(context);
}

CYExpression *CYArray::Replace(CYContext &context) {
    elements_->Replace(context);
    return NULL;
}

CYExpression *CYArrayComprehension::Replace(CYContext &context) {
    CYVariable *cyv($V("$cyv"));

    return $C0($F(NULL, $P1("$cyv", comprehensions_->Parameters(context)), $$->*
        $E($ CYAssign(cyv, $ CYArray()))->*
        comprehensions_->Replace(context, $E($C1($M(cyv, $S("push")), expression_)))->*
        $ CYReturn(cyv)
    ));
}

CYExpression *CYAssignment::Replace(CYContext &context) {
    context.Replace(lhs_);
    context.Replace(rhs_);
    return NULL;
}

CYStatement *CYBlock::Replace(CYContext &context) {
    statements_ = statements_->ReplaceAll(context);
    return NULL;
}

CYStatement *CYBreak::Replace(CYContext &context) {
    return NULL;
}

CYExpression *CYCall::Replace(CYContext &context) {
    context.Replace(function_);
    arguments_->Replace(context);
    return NULL;
}

namespace cy {
namespace Syntax {

void Catch::Replace(CYContext &context) { $T()
    code_.Replace(context);
}

} }

void CYClause::Replace(CYContext &context) { $T()
    context.Replace(case_);
    statements_ = statements_->ReplaceAll(context);
    next_->Replace(context);
}

CYExpression *CYCompound::Replace(CYContext &context) {
    expressions_ = expressions_->ReplaceAll(context);
    return NULL;
}

CYFunctionParameter *CYComprehension::Parameters(CYContext &context) const { $T(NULL)
    CYFunctionParameter *next(next_->Parameters(context));
    if (CYFunctionParameter *parameter = Parameter(context)) {
        parameter->SetNext(next);
        return parameter;
    } else
        return next;
}

CYStatement *CYComprehension::Replace(CYContext &context, CYStatement *statement) const {
    return next_ == NULL ? statement : next_->Replace(context, statement);
}

CYExpression *CYCondition::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(true_);
    context.Replace(false_);
    return NULL;
}

CYStatement *CYContinue::Replace(CYContext &context) {
    return NULL;
}

CYExpression *CYDeclaration::ForEachIn(CYContext &context) {
    return $ CYVariable(identifier_);
}

void CYDeclaration::Replace(CYContext &context) {
    context.Replace(initialiser_);
}

CYProperty *CYDeclarations::Property(CYContext &context) { $T(NULL)
    return $ CYProperty(declaration_->identifier_, declaration_->initialiser_ ?: $U, next_->Property(context));
}

void CYDeclarations::Replace(CYContext &context) { $T()
    declaration_->Replace(context);
    next_->Replace(context);
}

CYExpression *CYDirectMember::Replace(CYContext &context) {
    Replace_(context);
    return NULL;
}

CYStatement *CYDoWhile::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(code_);
    return NULL;
}

void CYElement::Replace(CYContext &context) { $T()
    context.Replace(value_);
    next_->Replace(context);
}

CYStatement *CYEmpty::Replace(CYContext &context) {
    return NULL;
}

CYStatement *CYExpress::Replace(CYContext &context) {
    context.Replace(expression_);
    return NULL;
}

CYExpression *CYExpression::ClassName(CYContext &context, bool object) {
    return this;
}

CYExpression *CYExpression::ForEachIn(CYContext &context) {
    return this;
}

CYExpression *CYExpression::ReplaceAll(CYContext &context) { $T(NULL)
    CYExpression *replace(this);
    context.Replace(replace);

    if (CYExpression *next = next_->ReplaceAll(context))
        replace->SetNext(next);
    else
        replace->SetNext(next_);

    return replace;
}

CYNumber *CYFalse::Number(CYContext &context) {
    return $D(0);
}

CYString *CYFalse::String(CYContext &context) {
    return $S("false");
}

void CYFinally::Replace(CYContext &context) { $T()
    code_.Replace(context);
}

CYStatement *CYFor::Replace(CYContext &context) {
    // XXX: initialiser_
    context.Replace(test_);
    context.Replace(increment_);
    context.Replace(code_);
    return NULL;
}

CYStatement *CYForIn::Replace(CYContext &context) {
    // XXX: initialiser_
    context.Replace(set_);
    context.Replace(code_);
    return NULL;
}

CYFunctionParameter *CYForInComprehension::Parameter(CYContext &context) const {
    return $ CYFunctionParameter(name_);
}

CYStatement *CYForInComprehension::Replace(CYContext &context, CYStatement *statement) const {
    return $ CYForIn($ CYVariable(name_), set_, CYComprehension::Replace(context, statement));
}

CYStatement *CYForEachIn::Replace(CYContext &context) {
    CYVariable *cys($V("$cys")), *cyt($V("$cyt"));

    return $ CYLet($L2($L($I("$cys"), set_), $L($I("$cyt"))), $$->*
        $ CYForIn(cyt, cys, $ CYBlock($$->*
            $E($ CYAssign(initialiser_->ForEachIn(context), $M(cys, cyt)))->*
            code_
        ))
    );
}

CYFunctionParameter *CYForEachInComprehension::Parameter(CYContext &context) const {
    return $ CYFunctionParameter(name_);
}

CYStatement *CYForEachInComprehension::Replace(CYContext &context, CYStatement *statement) const {
    CYVariable *cys($V("$cys")), *name($ CYVariable(name_));

    return $E($C0($F(NULL, $P1("$cys"), $$->*
        $E($ CYAssign(cys, set_))->*
        $ CYForIn(name, cys, $ CYBlock($$->*
            $E($ CYAssign(name, $M(cys, name)))->*
            CYComprehension::Replace(context, statement)
        ))
    )));
}

void CYFunction::Replace_(CYContext &context) {
    code_.Replace(context);
}

CYExpression *CYFunctionExpression::Replace(CYContext &context) {
    Replace_(context);
    return NULL;
}

CYStatement *CYFunctionStatement::Replace(CYContext &context) {
    Replace_(context);
    return NULL;
}

CYStatement *CYIf::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(true_);
    context.Replace(false_);
    return NULL;
}

CYFunctionParameter *CYIfComprehension::Parameter(CYContext &context) const {
    return NULL;
}

CYStatement *CYIfComprehension::Replace(CYContext &context, CYStatement *statement) const {
    return $ CYIf(test_, CYComprehension::Replace(context, statement));
}

CYExpression *CYIndirect::Replace(CYContext &context) {
    CYPrefix::Replace(context);
    return $M(rhs_, $S("$cyi"));
}

CYExpression *CYIndirectMember::Replace(CYContext &context) {
    Replace_(context);
    return $M($ CYIndirect(object_), property_);
}

CYExpression *CYInfix::Replace(CYContext &context) {
    context.Replace(lhs_);
    context.Replace(rhs_);
    return NULL;
}

CYStatement *CYLabel::Replace(CYContext &context) {
    context.Replace(statement_);
    return NULL;
}

CYStatement *CYLet::Replace(CYContext &context) {
    return $ CYWith($ CYObject(declarations_->Property(context)), &code_);
}

void CYMember::Replace_(CYContext &context) {
    context.Replace(object_);
    context.Replace(property_);
}

CYExpression *CYNew::Replace(CYContext &context) {
    context.Replace(constructor_);
    arguments_->Replace(context);
    return NULL;
}

CYNumber *CYNull::Number(CYContext &context) {
    return $D(0);
}

CYString *CYNull::String(CYContext &context) {
    return $S("null");
}

CYNumber *CYNumber::Number(CYContext &context) {
    return this;
}

CYString *CYNumber::String(CYContext &context) {
    // XXX: there is a precise algorithm for this
    return $S(apr_psprintf(context.pool_, "%.17g", Value()));
}

CYExpression *CYObject::Replace(CYContext &context) {
    properties_->Replace(context);
    return NULL;
}

CYExpression *CYPostfix::Replace(CYContext &context) {
    context.Replace(lhs_);
    return NULL;
}

CYExpression *CYPrefix::Replace(CYContext &context) {
    context.Replace(rhs_);
    return NULL;
}

void CYProgram::Replace(CYContext &context) {
    statements_ = statements_->ReplaceAll(context);
}

void CYProperty::Replace(CYContext &context) { $T()
    context.Replace(value_);
    next_->Replace(context);
}

CYStatement *CYReturn::Replace(CYContext &context) {
    context.Replace(value_);
    return NULL;
}

CYStatement *CYStatement::ReplaceAll(CYContext &context) { $T(NULL)
    CYStatement *replace(this);
    context.Replace(replace);

    if (CYStatement *next = next_->ReplaceAll(context))
        replace->SetNext(next);
    else
        replace->SetNext(next_);

    return replace;
}

CYString *CYString::Concat(CYContext &context, CYString *rhs) const {
    size_t size(size_ + rhs->size_);
    char *value(new(context.pool_) char[size + 1]);
    memcpy(value, value_, size_);
    memcpy(value + size_, rhs->value_, rhs->size_);
    value[size] = '\0';
    return $S(value, size);
}

CYNumber *CYString::Number(CYContext &context) {
    // XXX: there is a precise algorithm for this
    return NULL;
}

CYString *CYString::String(CYContext &context) {
    return this;
}

CYStatement *CYSwitch::Replace(CYContext &context) {
    context.Replace(value_);
    clauses_->Replace(context);
    return NULL;
}

CYExpression *CYThis::Replace(CYContext &context) {
    return NULL;
}

namespace cy {
namespace Syntax {

CYStatement *Throw::Replace(CYContext &context) {
    context.Replace(value_);
    return NULL;
}

} }

CYExpression *CYTrivial::Replace(CYContext &context) {
    return NULL;
}

CYNumber *CYTrue::Number(CYContext &context) {
    return $D(1);
}

CYString *CYTrue::String(CYContext &context) {
    return $S("true");
}

namespace cy {
namespace Syntax {

CYStatement *Try::Replace(CYContext &context) {
    code_.Replace(context);
    catch_->Replace(context);
    finally_->Replace(context);
    return NULL;
}

} }

CYStatement *CYVar::Replace(CYContext &context) {
    declarations_->Replace(context);
    return NULL;
}

CYExpression *CYVariable::Replace(CYContext &context) {
    return NULL;
}

CYStatement *CYWhile::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(code_);
    return NULL;
}

CYStatement *CYWith::Replace(CYContext &context) {
    context.Replace(scope_);
    context.Replace(code_);
    return NULL;
}

CYExpression *CYWord::ClassName(CYContext &context, bool object) {
    CYString *name($S(this));
    if (object)
        return $C1($V("objc_getClass"), name);
    else
        return name;
}
