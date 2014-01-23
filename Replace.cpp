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

#include "Parser.hpp"
#include "Replace.hpp"

#include <iomanip>

CYFunctionExpression *CYNonLocalize(CYContext &context, CYFunctionExpression *function) {
    function->nonlocal_ = context.nextlocal_;
    return function;
}

CYExpression *CYAdd::Replace(CYContext &context) {
    CYInfix::Replace(context);

    CYString *lhs(dynamic_cast<CYString *>(lhs_));
    CYString *rhs(dynamic_cast<CYString *>(rhs_));

    if (lhs != NULL || rhs != NULL) {
        if (lhs == NULL) {
            lhs = lhs_->String(context);
            if (lhs == NULL)
                return this;
        } else if (rhs == NULL) {
            rhs = rhs_->String(context);
            if (rhs == NULL)
                return this;
        }

        return lhs->Concat(context, rhs);
    }

    if (CYNumber *lhn = lhs_->Number(context))
        if (CYNumber *rhn = rhs_->Number(context))
            return $D(lhn->Value() + rhn->Value());

    return this;
}

CYExpression *CYAddressOf::Replace(CYContext &context) {
    return $C0($M(rhs_, $S("$cya")));
}

CYArgument *CYArgument::Replace(CYContext &context) { $T(NULL)
    context.Replace(value_);
    next_ = next_->Replace(context);

    if (value_ == NULL) {
        if (next_ == NULL)
            return NULL;
        else
            value_ = $U;
    }

    return this;
}

CYExpression *CYArray::Replace(CYContext &context) {
    elements_->Replace(context);
    return this;
}

CYExpression *CYArrayComprehension::Replace(CYContext &context) {
    CYVariable *cyv($V("$cyv"));

    return $C0($F(NULL, $P1($L("$cyv"), comprehensions_->Parameters(context)), $$->*
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
    context.ReplaceAll(statements_);
    if (statements_ == NULL)
        return $ CYEmpty();
    return this;
}

CYStatement *CYBreak::Replace(CYContext &context) {
    return this;
}

CYExpression *CYCall::AddArgument(CYContext &context, CYExpression *value) {
    CYArgument **argument(&arguments_);
    while (*argument != NULL)
        argument = &(*argument)->next_;
    *argument = $ CYArgument(value);
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
    CYScope scope(true, context, code_.statements_);

    context.Replace(name_);
    context.scope_->Declare(context, name_, CYIdentifierCatch);

    code_.Replace(context);
    scope.Close();
}

} }

void CYClause::Replace(CYContext &context) { $T()
    context.Replace(case_);
    context.ReplaceAll(statements_);
    next_->Replace(context);
}

CYStatement *CYComment::Replace(CYContext &context) {
    return this;
}

CYExpression *CYCompound::Replace(CYContext &context) {
    if (next_ == NULL)
        return expression_;

    context.Replace(expression_);
    context.Replace(next_);

    if (CYCompound *compound = dynamic_cast<CYCompound *>(expression_)) {
        expression_ = compound->expression_;
        compound->expression_ = compound->next_;
        compound->next_ = next_;
        next_ = compound;
    }

    return this;
}

CYExpression *CYCompound::Primitive(CYContext &context) {
    CYExpression *expression(expression_);
    if (expression == NULL || next_ != NULL)
        return NULL;
    return expression->Primitive(context);
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

void CYContext::NonLocal(CYStatement *&statements) {
    CYContext &context(*this);

    if (nextlocal_ != NULL && nextlocal_->identifier_ != NULL) {
        CYIdentifier *cye($I("$cye")->Replace(context));
        CYIdentifier *unique(nextlocal_->identifier_->Replace(context));

        CYStatement *declare(
            $ CYVar($L1($ CYDeclaration(unique, $ CYObject()))));

        cy::Syntax::Catch *rescue(
            $ cy::Syntax::Catch(cye, $$->*
                $ CYIf($ CYIdentical($M($V(cye), $S("$cyk")), $V(unique)), $$->*
                    $ CYReturn($M($V(cye), $S("$cyv"))))->*
                $ cy::Syntax::Throw($V(cye))));

        context.Replace(declare);
        rescue->Replace(context);

        statements = $$->*
            declare->*
            $ cy::Syntax::Try(statements, rescue, NULL);
    }
}

CYIdentifier *CYContext::Unique() {
    return $ CYIdentifier($pool.strcat("$cy", $pool.itoa(unique_++), NULL));
}

CYStatement *CYContinue::Replace(CYContext &context) {
    return this;
}

CYStatement *CYDebugger::Replace(CYContext &context) {
    return this;
}

CYAssignment *CYDeclaration::Assignment(CYContext &context) {
    if (initialiser_ == NULL)
        return NULL;

    CYAssignment *value($ CYAssign(Variable(context), initialiser_));
    initialiser_ = NULL;
    return value;
}

CYVariable *CYDeclaration::Variable(CYContext &context) {
    return $V(identifier_);
}

CYStatement *CYDeclaration::ForEachIn(CYContext &context, CYExpression *value) {
    return $ CYVar($L1($ CYDeclaration(identifier_, value)));
}

CYExpression *CYDeclaration::Replace(CYContext &context) {
    context.Replace(identifier_);
    context.scope_->Declare(context, identifier_, CYIdentifierVariable);
    return Variable(context);
}

void CYDeclarations::Replace(CYContext &context) { $T()
    declaration_->Replace(context);
    next_->Replace(context);
}

CYProperty *CYDeclarations::Property(CYContext &context) { $T(NULL)
    return $ CYProperty(declaration_->identifier_, declaration_->initialiser_, next_->Property(context));
}

CYFunctionParameter *CYDeclarations::Parameter(CYContext &context) { $T(NULL)
    return $ CYFunctionParameter($ CYDeclaration(declaration_->identifier_), next_->Parameter(context));
}

CYArgument *CYDeclarations::Argument(CYContext &context) { $T(NULL)
    return $ CYArgument(declaration_->initialiser_, next_->Argument(context));
}

CYCompound *CYDeclarations::Compound(CYContext &context) { $T(NULL)
    CYCompound *compound(next_->Compound(context));
    if (CYAssignment *assignment = declaration_->Assignment(context))
        compound = $ CYCompound(assignment, compound);
    return compound;
}

CYExpression *CYDirectMember::Replace(CYContext &context) {
    context.Replace(object_);
    context.Replace(property_);
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

CYStatement *CYEmpty::Replace(CYContext &context) {
    return NULL;
}

CYExpression *CYEncodedType::Replace(CYContext &context) {
    return typed_->Replace(context);
}

CYStatement *CYExpress::Replace(CYContext &context) {
    while (CYExpress *express = dynamic_cast<CYExpress *>(next_)) {
        expression_ = $ CYCompound(expression_, express->expression_);
        SetNext(express->next_);
    }

    context.Replace(expression_);
    if (expression_ == NULL)
        return $ CYEmpty();

    return this;
}

CYExpression *CYExpression::AddArgument(CYContext &context, CYExpression *value) {
    return $C1(this, value);
}

CYExpression *CYExpression::ClassName(CYContext &context, bool object) {
    return this;
}

CYStatement *CYExpression::ForEachIn(CYContext &context, CYExpression *value) {
    return $E($ CYAssign(this, value));
}

CYAssignment *CYExpression::Assignment(CYContext &context) {
    return NULL;
}

CYNumber *CYFalse::Number(CYContext &context) {
    return $D(0);
}

CYString *CYFalse::String(CYContext &context) {
    return $S("false");
}

CYExpression *CYFatArrow::Replace(CYContext &context) {
    CYFunctionExpression *function($ CYFunctionExpression(NULL, parameters_, code_));
    function->this_.SetNext(context.this_);
    return function;
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

CYCompound *CYForDeclarations::Replace(CYContext &context) {
    declarations_->Replace(context);
    return declarations_->Compound(context);
}

// XXX: this still feels highly suboptimal
CYStatement *CYForIn::Replace(CYContext &context) {
    if (CYAssignment *assignment = initialiser_->Assignment(context))
        return $ CYBlock($$->*
            $E(assignment)->*
            this
        );

    context.Replace(initialiser_);
    context.Replace(set_);
    context.Replace(code_);
    return this;
}

CYFunctionParameter *CYForInComprehension::Parameter(CYContext &context) const {
    return $ CYFunctionParameter($ CYDeclaration(name_));
}

CYStatement *CYForInComprehension::Replace(CYContext &context, CYStatement *statement) const {
    return $ CYForIn($V(name_), set_, CYComprehension::Replace(context, statement));
}

CYStatement *CYForOf::Replace(CYContext &context) {
    if (CYAssignment *assignment = initialiser_->Assignment(context))
        return $ CYBlock($$->*
            $E(assignment)->*
            this
        );

    CYIdentifier *cys($I("$cys")), *cyt($I("$cyt"));

    return $ CYLetStatement($L2($ CYDeclaration(cys, set_), $ CYDeclaration(cyt)), $$->*
        $ CYForIn($V(cyt), $V(cys), $ CYBlock($$->*
            initialiser_->ForEachIn(context, $M($V(cys), $V(cyt)))->*
            code_
        ))
    );
}

CYFunctionParameter *CYForOfComprehension::Parameter(CYContext &context) const {
    return $ CYFunctionParameter($ CYDeclaration(name_));
}

CYStatement *CYForOfComprehension::Replace(CYContext &context, CYStatement *statement) const {
    CYIdentifier *cys($I("cys"));

    return $E($C0($F(NULL, $P1($L("$cys")), $$->*
        $E($ CYAssign($V(cys), set_))->*
        $ CYForIn($V(name_), $V(cys), $ CYBlock($$->*
            $E($ CYAssign($V(name_), $M($V(cys), $V(name_))))->*
            CYComprehension::Replace(context, statement)
        ))
    )));
}

void CYFunction::Inject(CYContext &context) {
    context.Replace(name_);
    context.scope_->Declare(context, name_, CYIdentifierOther);
}

void CYFunction::Replace_(CYContext &context, bool outer) {
    if (outer)
        Inject(context);

    CYThisScope *_this(context.this_);
    context.this_ = CYGetLast(&this_);

    CYNonLocal *nonlocal(context.nonlocal_);
    CYNonLocal *nextlocal(context.nextlocal_);

    bool localize;
    if (nonlocal_ != NULL) {
        localize = false;
        context.nonlocal_ = nonlocal_;
    } else {
        localize = true;
        nonlocal_ = $ CYNonLocal();
        context.nextlocal_ = nonlocal_;
    }

    CYScope scope(!localize, context, code_.statements_);

    if (!outer && name_ != NULL)
        Inject(context);

    parameters_->Replace(context, code_);
    code_.Replace(context);

    if (CYIdentifier *identifier = this_.identifier_)
        code_.statements_ = $$->*
            $ CYVar($L1($ CYDeclaration(identifier, $ CYThis())))->*
            code_.statements_;

    if (localize)
        context.NonLocal(code_.statements_);

    context.nextlocal_ = nextlocal;
    context.nonlocal_ = nonlocal;

    context.this_ = _this;

    scope.Close();
}

CYExpression *CYFunctionExpression::Replace(CYContext &context) {
    Replace_(context, false);
    return this;
}

void CYFunctionParameter::Replace(CYContext &context, CYBlock &code) { $T()
    CYAssignment *assignment(initialiser_->Assignment(context));
    context.Replace(initialiser_);

    next_->Replace(context, code);

    if (assignment != NULL)
        // XXX: this cast is quite incorrect
        code.AddPrev($ CYIf($ CYIdentical($ CYTypeOf(dynamic_cast<CYExpression *>(initialiser_)), $S("undefined")), $$->*
            $E(assignment)
        ));
}

CYStatement *CYFunctionStatement::Replace(CYContext &context) {
    Replace_(context, true);
    return this;
}

CYIdentifier *CYIdentifier::Replace(CYContext &context) {
    if (replace_ != NULL && replace_ != this)
        return replace_->Replace(context);
    replace_ = context.scope_->Lookup(context, this);
    return replace_;
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
    return $M(rhs_, $S("$cyi"));
}

CYExpression *CYIndirectMember::Replace(CYContext &context) {
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

CYExpression *CYLambda::Replace(CYContext &context) {
    return $N2($V("Functor"), $ CYFunctionExpression(NULL, parameters_->Parameters(context), statements_), parameters_->TypeSignature(context, typed_->Replace(context)));
}

CYStatement *CYLetStatement::Replace(CYContext &context) {
    return $E($ CYCall(CYNonLocalize(context, $ CYFunctionExpression(NULL, declarations_->Parameter(context), code_)), declarations_->Argument(context)));
}

CYExpression *CYMultiply::Replace(CYContext &context) {
    CYInfix::Replace(context);

    if (CYNumber *lhn = lhs_->Number(context))
        if (CYNumber *rhn = rhs_->Number(context))
            return $D(lhn->Value() * rhn->Value());

    return this;
}

namespace cy {
namespace Syntax {

CYExpression *New::AddArgument(CYContext &context, CYExpression *value) {
    CYSetLast(arguments_) = $ CYArgument(value);
    return this;
}

CYExpression *New::Replace(CYContext &context) {
    context.Replace(constructor_);
    arguments_->Replace(context);
    return this;
}

} }

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
    return $S($pool.sprintf(24, "%.17g", Value()));
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

// XXX: this is evil evil black magic. don't ask, don't tell... don't believe!
#define MappingSet "0etnirsoalfucdphmgyvbxTwSNECAFjDLkMOIBPqzRH$_WXUVGYKQJZ"
//#define MappingSet "0abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ$_"

namespace {
    struct IdentifierUsageLess :
        std::binary_function<CYIdentifier *, CYIdentifier *, bool>
    {
        _finline bool operator ()(CYIdentifier *lhs, CYIdentifier *rhs) const {
            if (lhs->usage_ != rhs->usage_)
                return lhs->usage_ > rhs->usage_;
            return lhs < rhs;
        }
    };

    typedef std::set<CYIdentifier *, IdentifierUsageLess> IdentifierUsages;
}

void CYProgram::Replace(CYContext &context) {
    CYScope scope(true, context, statements_);

    context.nextlocal_ = $ CYNonLocal();
    context.ReplaceAll(statements_);
    context.NonLocal(statements_);

    scope.Close();

    size_t offset(0);

    CYCStringSet external;
    for (CYIdentifierValueSet::const_iterator i(scope.identifiers_.begin()); i != scope.identifiers_.end(); ++i)
        external.insert((*i)->Word());

    IdentifierUsages usages;

    if (offset < context.rename_.size())
        CYForEach (i, context.rename_[offset].identifier_)
            usages.insert(i);

    // XXX: totalling the probable occurrences and sorting by them would improve the result
    for (CYIdentifierUsageVector::const_iterator i(context.rename_.begin()); i != context.rename_.end(); ++i, ++offset) {
        //std::cout << *i << ":" << (*i)->offset_ << std::endl;

        const char *name;

        if (context.options_.verbose_)
            name = $pool.strcat("$", $pool.itoa(offset), NULL);
        else {
            char id[8];
            id[7] = '\0';

          id:
            unsigned position(7), local(offset + 1);

            do {
                unsigned index(local % (sizeof(MappingSet) - 1));
                local /= sizeof(MappingSet) - 1;
                id[--position] = MappingSet[index];
            } while (local != 0);

            if (external.find(id + position) != external.end()) {
                ++offset;
                goto id;
            }

            name = $pool.strmemdup(id + position, 7 - position);
            // XXX: at some point, this could become a keyword
        }

        CYForEach (identifier, i->identifier_)
            identifier->Set(name);
    }
}

void CYProperty::Replace(CYContext &context) { $T()
    context.Replace(value_);
    next_->Replace(context);
    if (value_ == NULL)
        value_ = $U;
}

CYStatement *CYReturn::Replace(CYContext &context) {
    if (context.nonlocal_ != NULL) {
        CYProperty *value(value_ == NULL ? NULL : $ CYProperty($S("$cyv"), value_));
        return $ cy::Syntax::Throw($ CYObject(
            $ CYProperty($S("$cyk"), $V(context.nonlocal_->Target(context)), value)
        ));
    }

    context.Replace(value_);
    return this;
}

CYExpression *CYRubyBlock::Replace(CYContext &context) {
    // XXX: this needs to do something much more epic to handle return
    return call_->AddArgument(context, proc_->Replace(context));
}

CYExpression *CYRubyProc::Replace(CYContext &context) {
    return CYNonLocalize(context, $ CYFunctionExpression(NULL, parameters_, code_));
}

CYScope::CYScope(bool transparent, CYContext &context, CYStatement *&statements) :
    transparent_(transparent),
    context_(context),
    statements_(statements),
    parent_(context.scope_)
{
    context_.scope_ = this;
}

CYScope::~CYScope() {
}

void CYScope::Close() {
    context_.scope_ = parent_;
    Scope(context_, statements_);
}

void CYScope::Declare(CYContext &context, CYIdentifier *identifier, CYIdentifierFlags flags) {
    if (!transparent_ || flags == CYIdentifierArgument || flags == CYIdentifierCatch)
        internal_.insert(CYIdentifierAddressFlagsMap::value_type(identifier, flags));
    else if (parent_ != NULL)
        parent_->Declare(context, identifier, flags);
}

CYIdentifier *CYScope::Lookup(CYContext &context, CYIdentifier *identifier) {
    std::pair<CYIdentifierValueSet::iterator, bool> insert(identifiers_.insert(identifier));
    return *insert.first;
}

void CYScope::Merge(CYContext &context, CYIdentifier *identifier) {
    std::pair<CYIdentifierValueSet::iterator, bool> insert(identifiers_.insert(identifier));
    if (!insert.second) {
        if ((*insert.first)->offset_ < identifier->offset_)
            (*insert.first)->offset_ = identifier->offset_;
        identifier->replace_ = *insert.first;
        (*insert.first)->usage_ += identifier->usage_ + 1;
    }
}

namespace {
    struct IdentifierOffset {
        size_t offset_;
        CYIdentifierFlags flags_;
        size_t usage_;
        CYIdentifier *identifier_;

        IdentifierOffset(CYIdentifier *identifier, CYIdentifierFlags flags) :
            offset_(identifier->offset_),
            flags_(flags),
            usage_(identifier->usage_),
            identifier_(identifier)
        {
        }
    };

    struct IdentifierOffsetLess :
        std::binary_function<const IdentifierOffset &, const IdentifierOffset &, bool>
    {
        _finline bool operator ()(const IdentifierOffset &lhs, const IdentifierOffset &rhs) const {
            if (lhs.offset_ != rhs.offset_)
                return lhs.offset_ < rhs.offset_;
            if (lhs.flags_ != rhs.flags_)
                return lhs.flags_ < rhs.flags_;
            /*if (lhs.usage_ != rhs.usage_)
                return lhs.usage_ < rhs.usage_;*/
            return lhs.identifier_ < rhs.identifier_;
        }
    };

    typedef std::set<IdentifierOffset, IdentifierOffsetLess> IdentifierOffsets;
}

void CYScope::Scope(CYContext &context, CYStatement *&statements) {
    if (parent_ == NULL)
        return;

    CYDeclarations *last(NULL), *curr(NULL);

    IdentifierOffsets offsets;

    for (CYIdentifierAddressFlagsMap::const_iterator i(internal_.begin()); i != internal_.end(); ++i)
        if (i->second != CYIdentifierMagic)
            offsets.insert(IdentifierOffset(i->first, i->second));

    size_t offset(0);

    for (IdentifierOffsets::const_iterator i(offsets.begin()); i != offsets.end(); ++i) {
        if (i->flags_ == CYIdentifierVariable) {
            CYDeclarations *next($ CYDeclarations($ CYDeclaration(i->identifier_)));
            if (last == NULL)
                last = next;
            if (curr != NULL)
                curr->SetNext(next);
            curr = next;
        }

        if (offset < i->offset_)
            offset = i->offset_;
        if (context.rename_.size() <= offset)
            context.rename_.resize(offset + 1);

        CYIdentifierUsage &rename(context.rename_[offset++]);
        i->identifier_->SetNext(rename.identifier_);
        rename.identifier_ = i->identifier_;
        rename.usage_ += i->identifier_->usage_ + 1;
    }

    if (last != NULL) {
        CYVar *var($ CYVar(last));
        var->SetNext(statements);
        statements = var;
    }

    for (CYIdentifierValueSet::const_iterator i(identifiers_.begin()); i != identifiers_.end(); ++i)
        if (internal_.find(*i) == internal_.end()) {
            //std::cout << *i << '=' << offset << std::endl;
            if ((*i)->offset_ < offset)
                (*i)->offset_ = offset;
            parent_->Merge(context, *i);
        }
}

CYString *CYString::Concat(CYContext &context, CYString *rhs) const {
    size_t size(size_ + rhs->size_);
    char *value($ char[size + 1]);
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
    if (context.this_ != NULL)
        return $V(context.this_->Identifier(context));
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

CYExpression *CYTypeArrayOf::Replace_(CYContext &context, CYExpression *type) {
    return next_->Replace(context, $ CYCall($ CYDirectMember(type, $ CYString("arrayOf")), $ CYArgument(size_)));
}

CYExpression *CYTypeBlockWith::Replace_(CYContext &context, CYExpression *type) {
    return next_->Replace(context, $ CYCall($ CYDirectMember(type, $ CYString("blockWith")), parameters_->Argument(context)));
}

CYExpression *CYTypeConstant::Replace_(CYContext &context, CYExpression *type) {
    return next_->Replace(context, $ CYCall($ CYDirectMember(type, $ CYString("constant"))));
}

CYStatement *CYTypeDefinition::Replace(CYContext &context) {
    return $E($ CYAssign($V(typed_->identifier_), typed_->Replace(context)));
}

CYExpression *CYTypeError::Replace(CYContext &context) {
    _assert(false);
    return NULL;
}

CYExpression *CYTypeModifier::Replace(CYContext &context, CYExpression *type) { $T(type)
    return Replace_(context, type);
}

CYExpression *CYTypeFunctionWith::Replace_(CYContext &context, CYExpression *type) {
    return next_->Replace(context, $ CYCall($ CYDirectMember(type, $ CYString("functionWith")), parameters_->Argument(context)));
}

CYExpression *CYTypeLong::Replace(CYContext &context) {
    return $ CYCall($ CYDirectMember(specifier_->Replace(context), $ CYString("long")));
}

CYExpression *CYTypePointerTo::Replace_(CYContext &context, CYExpression *type) {
    return next_->Replace(context, $ CYCall($ CYDirectMember(type, $ CYString("pointerTo"))));
}

CYExpression *CYTypeShort::Replace(CYContext &context) {
    return $ CYCall($ CYDirectMember(specifier_->Replace(context), $ CYString("short")));
}

CYExpression *CYTypeSigned::Replace(CYContext &context) {
    return $ CYCall($ CYDirectMember(specifier_->Replace(context), $ CYString("signed")));
}

CYExpression *CYTypeUnsigned::Replace(CYContext &context) {
    return $ CYCall($ CYDirectMember(specifier_->Replace(context), $ CYString("unsigned")));
}

CYExpression *CYTypeVariable::Replace(CYContext &context) {
    return $V(name_);
}

CYExpression *CYTypeVoid::Replace(CYContext &context) {
    return $N1($V("Type"), $ CYString("v"));
}

CYExpression *CYTypeVolatile::Replace_(CYContext &context, CYExpression *type) {
    return next_->Replace(context, $ CYCall($ CYDirectMember(type, $ CYString("volatile"))));
}

CYExpression *CYTypedIdentifier::Replace(CYContext &context) {
    return modifier_->Replace(context, specifier_->Replace(context));
}

CYArgument *CYTypedParameter::Argument(CYContext &context) { $T(NULL)
    return $ CYArgument(typed_->Replace(context), next_->Argument(context));
}

CYFunctionParameter *CYTypedParameter::Parameters(CYContext &context) { $T(NULL)
    return $ CYFunctionParameter($ CYDeclaration(typed_->identifier_ ?: context.Unique()), next_->Parameters(context));
}

CYExpression *CYTypedParameter::TypeSignature(CYContext &context, CYExpression *prefix) { $T(prefix)
    return next_->TypeSignature(context, $ CYAdd(prefix, typed_->Replace(context)));
}

CYStatement *CYVar::Replace(CYContext &context) {
    declarations_->Replace(context);
    if (CYCompound *compound = declarations_->Compound(context))
        return $E(compound);
    return $ CYEmpty();
}

CYExpression *CYVariable::Replace(CYContext &context) {
    context.Replace(name_);
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
