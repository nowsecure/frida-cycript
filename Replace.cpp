#include "Parser.hpp"

#include <iostream>
#include <iomanip>

#include <objc/runtime.h>
#include <sstream>

#define $ \
    new(context.pool_)

#define $D(args...) \
    ($ CYNumber(args))
#define $E(args...) \
    ($ CYExpress(args))
#define $F(args...) \
    ($ CYFunctionExpression(args))
#define $I(args...) \
    ($ CYIdentifier(args))
#define $M(args...) \
    ($ CYDirectMember(args))
#define $P(args...) \
    ($ CYFunctionParameter(args))
#define $S(args...) \
    ($ CYString(args))
#define $V(name) \
    ($ CYVariable($I(name)))

#define $T(value) \
    if (this == NULL) \
        return value;
#define $$ \
    CYStatements()

#define $P1(arg0, args...) \
    $P($I(arg0), ##args)
#define $P2(arg0, arg1, args...) \
    $P($I(arg0), $P1(arg1, ##args))
#define $P3(arg0, arg1, arg2, args...) \
    $P($I(arg0), $P2(arg1, arg2, ##args))
#define $P4(arg0, arg1, arg2, arg3, args...) \
    $P($I(arg0), $P3(arg1, arg2, arg3, ##args))
#define $P5(arg0, arg1, arg2, arg3, arg4, args...) \
    $P($I(arg0), $P4(arg1, arg2, arg3, arg4, ##args))
#define $P6(arg0, arg1, arg2, arg3, arg4, arg5, args...) \
    $P($I(arg0), $P5(arg1, arg2, arg3, arg4, arg5, ##args))

#define $C(args...) \
    ($ CYCall(args))
#define $C_(args...) \
    ($ CYArgument(args))
#define $N(args...) \
    ($ CYNew(args))

#define $C1_(arg0, args...) \
    $C_(arg0, ##args)
#define $C2_(arg0, arg1, args...) \
    $C_(arg0, $C1_(arg1, ##args))
#define $C3_(arg0, arg1, arg2, args...) \
    $C_(arg0, $C2_(arg1, arg2, ##args))
#define $C4_(arg0, arg1, arg2, arg3, args...) \
    $C_(arg0, $C3_(arg1, arg2, arg3, ##args))
#define $C5_(arg0, arg1, arg2, arg3, arg4, args...) \
    $C_(arg0, $C4_(arg1, arg2, arg3, arg4, ##args))
#define $C6_(arg0, arg1, arg2, arg3, arg4, arg5, args...) \
    $C_(arg0, $C5_(arg1, arg2, arg3, arg4, arg5, ##args))

#define $C0(func, args...) \
    $C(func, ##args)
#define $C1(func, args...) \
    $C(func, $C1_(args))
#define $C2(func, args...) \
    $C(func, $C2_(args))
#define $C3(func, args...) \
    $C(func, $C3_(args))
#define $C4(func, args...) \
    $C(func, $C4_(args))
#define $C5(func, args...) \
    $C(func, $C5_(args))

#define $N0(func, args...) \
    $N(func, ##args)
#define $N1(func, args...) \
    $N(func, $C1_(args))
#define $N2(func, args...) \
    $N(func, $C2_(args))
#define $N3(func, args...) \
    $N(func, $C3_(args))
#define $N4(func, args...) \
    $N(func, $C4_(args))
#define $N5(func, args...) \
    $N(func, $C5_(args))

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

void CYCatch::Replace(CYContext &context) { $T()
    code_.Replace(context);
}

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

CYStatement *CYField::Replace(CYContext &context) const {
    return NULL;
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

    return $ CYWith($ CYObject($ CYProperty($S("$cys"), $D(0), $ CYProperty($S("$cyt"), $D(0)))), $ CYBlock($$->*
        $E($ CYAssign(cys, set_))->*
        $ CYForIn(cyt, cys, $ CYBlock($$->*
            $E($ CYAssign(initialiser_->ForEachIn(context), $M(cys, cyt)))->*
            code_
        ))
    ));
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

void CYMember::Replace_(CYContext &context) {
    context.Replace(object_);
    context.Replace(property_);
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
                $ CYReturn($C1($M($F(NULL, NULL, statements_), $S("call")), $V("self")))
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

CYExpression *CYNew::Replace(CYContext &context) {
    context.Replace(constructor_);
    arguments_->Replace(context);
    return NULL;
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

CYExpression *CYSelector::Replace(CYContext &context) {
    return $N1($V("Selector"), name_->Replace(context));
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

CYStatement *CYStatement::ReplaceAll(CYContext &context) { $T(NULL)
    CYStatement *replace(this);
    context.Replace(replace);

    if (CYStatement *next = next_->ReplaceAll(context))
        replace->SetNext(next);
    else
        replace->SetNext(next_);

    return replace;
}

CYStatement *CYSwitch::Replace(CYContext &context) {
    context.Replace(value_);
    clauses_->Replace(context);
    return NULL;
}

CYExpression *CYThis::Replace(CYContext &context) {
    return NULL;
}

CYStatement *CYThrow::Replace(CYContext &context) {
    context.Replace(value_);
    return NULL;
}

CYExpression *CYTrivial::Replace(CYContext &context) {
    return NULL;
}

CYStatement *CYTry::Replace(CYContext &context) {
    code_.Replace(context);
    catch_->Replace(context);
    finally_->Replace(context);
    return NULL;
}

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
