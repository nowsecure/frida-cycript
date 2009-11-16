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
#include "Context.hpp"

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
                return this;
        } else if (rhs == NULL) {
            rhs = rhp->String(context);
            if (rhs == NULL)
                return this;
        }

        return lhs->Concat(context, rhs);
    }

    if (CYNumber *lhn = lhp->Number(context))
        if (CYNumber *rhn = rhp->Number(context))
            return $D(lhn->Value() + rhn->Value());

    return this;
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
    return this;
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
    return this;
}

CYStatement *CYBlock::Replace(CYContext &context) {
    statements_ = statements_->ReplaceAll(context);
    if (statements_ == NULL)
        return $ CYEmpty();
    return this;
}

CYStatement *CYBreak::Replace(CYContext &context) {
    return this;
}

CYExpression *CYCall::Replace(CYContext &context) {
    context.Replace(function_);
    arguments_->Replace(context);
    return this;
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

CYStatement *CYComment::Replace(CYContext &context) {
    return this;
}

CYExpression *CYCompound::Replace(CYContext &context) {
    expressions_ = expressions_->ReplaceAll(context);
    return expressions_ == NULL ? NULL : this;
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
    return this;
}

CYStatement *CYContinue::Replace(CYContext &context) {
    return this;
}

CYAssignment *CYDeclaration::Assignment(CYContext &context) {
    CYExpression *variable(Replace(context));
    return initialiser_ == NULL ? NULL : $ CYAssign(variable, initialiser_);
}

CYExpression *CYDeclaration::ForEachIn(CYContext &context) {
    return $ CYVariable(identifier_);
}

CYExpression *CYDeclaration::Replace(CYContext &context) {
    CYIdentifier *identifier(identifier_->Replace(context));
    context.scope_->internal_.insert(CYIdentifierAddressFlagsMap::value_type(identifier, CYIdentifierVariable));
    return $ CYVariable(identifier);
}

CYProperty *CYDeclarations::Property(CYContext &context) { $T(NULL)
    return $ CYProperty(declaration_->identifier_, declaration_->initialiser_ ?: $U, next_->Property(context));
}

CYCompound *CYDeclarations::Replace(CYContext &context) {
    CYCompound *compound;
    if (next_ == NULL) compound:
        compound = $ CYCompound();
    else {
        compound = next_->Replace(context);
        if (compound == NULL)
            goto compound;
    }

    if (CYAssignment *assignment = declaration_->Assignment(context))
        compound->AddPrev(assignment);
    return compound;
}

CYExpression *CYDirectMember::Replace(CYContext &context) {
    Replace_(context);
    return this;
}

CYStatement *CYDoWhile::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(code_);
    return this;
}

void CYElement::Replace(CYContext &context) { $T()
    context.Replace(value_);
    next_->Replace(context);
}

CYStatement *CYEmpty::Collapse(CYContext &context) {
    return next_;
}

CYStatement *CYEmpty::Replace(CYContext &context) {
    return this;
}

CYStatement *CYExpress::Collapse(CYContext &context) {
    if (CYExpress *express = dynamic_cast<CYExpress *>(next_)) {
        CYCompound *next(dynamic_cast<CYCompound *>(express->expression_));
        if (next == NULL)
            next = $ CYCompound(express->expression_);
        next->AddPrev(expression_);
        expression_ = next;
        SetNext(express->next_);
    }

    return this;
}

CYStatement *CYExpress::Replace(CYContext &context) {
    context.Replace(expression_);
    if (expression_ == NULL)
        return $ CYEmpty();
    return this;
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
    context.Replace(initialiser_);
    context.Replace(test_);
    context.Replace(increment_);
    context.Replace(code_);
    return this;
}

CYStatement *CYForIn::Replace(CYContext &context) {
    // XXX: this actually might need a prefix statement
    context.Replace(initialiser_);
    context.Replace(set_);
    context.Replace(code_);
    return this;
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
    parent_ = context.scope_;
    context.scope_ = this;

    parameters_->Replace(context);
    code_.Replace(context);

    context.scope_ = parent_;
    Scope(context, code_.statements_);
}

CYExpression *CYFunctionExpression::Replace(CYContext &context) {
    Replace_(context);
    return this;
}

void CYFunctionParameter::Replace(CYContext &context) { $T()
    name_ = name_->Replace(context);
    context.scope_->internal_.insert(CYIdentifierAddressFlagsMap::value_type(name_, CYIdentifierArgument));
    next_->Replace(context);
}

CYStatement *CYFunctionStatement::Replace(CYContext &context) {
    Replace_(context);
    return this;
}

CYIdentifier *CYIdentifier::Replace(CYContext &context) {
    if (replace_ != NULL)
        return replace_;

    CYIdentifierValueSet &identifiers(context.scope_->identifiers_);
    std::pair<CYIdentifierValueSet::iterator, bool> insert(identifiers.insert(this));
    if (!insert.second)
        return *insert.first;

    replace_ = this;
    return this;
}

CYStatement *CYIf::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(true_);
    context.Replace(false_);
    return this;
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
    return this;
}

CYStatement *CYLabel::Replace(CYContext &context) {
    context.Replace(statement_);
    return this;
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
    return this;
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
    return this;
}

CYExpression *CYPostfix::Replace(CYContext &context) {
    context.Replace(lhs_);
    return this;
}

CYExpression *CYPrefix::Replace(CYContext &context) {
    context.Replace(rhs_);
    return this;
}

void CYProgram::Replace(CYContext &context) {
    parent_ = context.scope_;
    context.scope_ = this;
    statements_ = statements_->ReplaceAll(context);
    context.scope_ = parent_;
    Scope(context, statements_);
}

void CYProperty::Replace(CYContext &context) { $T()
    context.Replace(value_);
    next_->Replace(context);
}

CYStatement *CYReturn::Replace(CYContext &context) {
    context.Replace(value_);
    return this;
}

void CYScope::Add(CYContext &context, CYIdentifierAddressSet &external) {
    for (CYIdentifierAddressSet::const_iterator i(external.begin()); i != external.end(); ++i) {
        std::pair<CYIdentifierAddressSet::iterator, bool> insert(identifiers_.insert(*i));
        if (!insert.second)
            (*i)->replace_ = *insert.first;
    }
}

void CYScope::Scope(CYContext &context, CYStatement *&statements) {
    CYIdentifierAddressSet external;

    if (context.options_.verbose_)
        std::cout << this << ':';

    CYDeclarations *last(NULL), *curr(NULL);

    for (CYIdentifierValueSet::const_iterator i(identifiers_.begin()); i != identifiers_.end(); ++i)
        if (internal_.find(*i) == internal_.end()) {
            if (context.options_.verbose_)
                std::cout << ' ' << (*i)->Word() << '@' << static_cast<const CYWord *>(*i);
            external.insert(*i);
        } else {
            if (context.options_.verbose_) {
                std::cout << ' ' << offset_ << ':' << (*i)->Word() << '@' << static_cast<const CYWord *>(*i);
                (*i)->Set(apr_psprintf(context.pool_, "$%u", offset_++));
            } else {
                (*i)->Set(apr_psprintf(context.pool_, "$%u", offset_++));
            }

            CYDeclarations *next($ CYDeclarations($ CYDeclaration(*i)));
            if (last == NULL)
                last = next;
            if (curr != NULL)
                curr->SetNext(next);
            curr = next;
        }

    if (context.options_.verbose_)
        std::cout << " ->" << parent_ << std::endl;

    if (last != NULL) {
        CYVar *var($ CYVar(last));
        var->SetNext(statements);
        statements = var;
    }

    if (parent_ != NULL) {
        if (parent_->offset_ < offset_)
            parent_->offset_ = offset_;
        parent_->Add(context, external);
    }
}

CYStatement *CYStatement::Collapse(CYContext &context) {
    return this;
}

CYStatement *CYStatement::ReplaceAll(CYContext &context) { $T(NULL)
    CYStatement *replace(this);
    context.Replace(replace);
    replace->SetNext(next_->ReplaceAll(context));
    return replace->Collapse(context);
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
    return this;
}

CYExpression *CYThis::Replace(CYContext &context) {
    return this;
}

namespace cy {
namespace Syntax {

CYStatement *Throw::Replace(CYContext &context) {
    context.Replace(value_);
    return this;
}

} }

CYExpression *CYTrivial::Replace(CYContext &context) {
    return this;
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
    return this;
}

} }

CYStatement *CYVar::Replace(CYContext &context) {
    return $E(declarations_->Replace(context));
}

CYExpression *CYVariable::Replace(CYContext &context) {
    name_ = name_->Replace(context);
    return this;
}

CYStatement *CYWhile::Replace(CYContext &context) {
    context.Replace(test_);
    context.Replace(code_);
    return this;
}

CYStatement *CYWith::Replace(CYContext &context) {
    context.Replace(scope_);
    context.Replace(code_);
    return this;
}

CYExpression *CYWord::ClassName(CYContext &context, bool object) {
    CYString *name($S(this));
    if (object)
        return $C1($V("objc_getClass"), name);
    else
        return name;
}
