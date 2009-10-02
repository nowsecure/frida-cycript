#include "Parser.hpp"

#include <iostream>
#include <iomanip>

#define CYPA 16

void CYAddressOf::Output(std::ostream &out) const {
    rhs_->Output(out, 1);
    out << ".$()";
}

void CYArgument::Output(std::ostream &out) const {
    if (name_ != NULL) {
        out << *name_;
        if (value_ != NULL)
            out << ":";
    }
    if (value_ != NULL)
        value_->Output(out, false);
    if (next_ != NULL) {
        if (next_->name_ == NULL)
            out << ',';
        else
            out << ' ';
        next_->Output(out);
    }
}

void CYArray::Output(std::ostream &out) const {
    out << '[';
    if (elements_ != NULL)
        elements_->Output(out);
    out << ']';
}

void CYAssignment::Output(std::ostream &out) const {
    lhs_->Output(out, Precedence() - 1);
    out << Operator();
    rhs_->Output(out, Precedence());
}

void CYBoolean::Output(std::ostream &out) const {
    out << (Value() ? "true" : "false");
}

void CYBreak::Output(std::ostream &out) const {
    out << "break";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYCall::Output(std::ostream &out) const {
    function_->Output(out, Precedence());
    out << '(';
    if (arguments_ != NULL)
        arguments_->Output(out);
    out << ')';
}

void CYCatch::Output(std::ostream &out) const {
    out << "catch(" << *name_ << ')';
    code_->Output(out, true);
}

void CYCondition::Output(std::ostream &out) const {
    test_->Output(out, Precedence() - 1);
    out << '?';
    if (true_ != NULL)
        true_->Output(out, CYPA);
    out << ':';
    false_->Output(out, CYPA);
}

void CYContinue::Output(std::ostream &out) const {
    out << "continue";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYClause::Output(std::ostream &out) const {
    if (case_ != NULL) {
        out << "case";
        case_->Output(out);
    } else
        out << "default";
    out << ':';
    if (code_ != NULL)
        code_->Output(out, false);
    out << *next_;
}

void CYDeclaration::Part(std::ostream &out) const {
    out << "var ";
    Output(out);
}

void CYDeclaration::Output(std::ostream &out) const {
    out << *identifier_;
    if (initialiser_ != NULL) {
        out << '=';
        initialiser_->Output(out, CYPA);
    }
}

void CYDeclarations::Part(std::ostream &out) const {
    out << "var ";

    const CYDeclarations *declaration(this);
  output:
    out << *declaration->declaration_;
    declaration = declaration->next_;

    if (declaration != NULL) {
        out << ',';
        goto output;
    }
}

void CYDeclarations::Output(std::ostream &out) const {
    Part(out);
    out << ';';
}

void CYDoWhile::Output(std::ostream &out) const {
    out << "do ";
    code_->Output(out, false);
    out << "while(";
    test_->Output(out);
    out << ';';
}

void CYElement::Output(std::ostream &out) const {
    if (value_ != NULL)
        value_->Output(out, CYPA);
    if (next_ != NULL || value_ == NULL)
        out << ',';
    if (next_ != NULL)
        next_->Output(out);
}

void CYEmpty::Output(std::ostream &out) const {
    out << ';';
}

void CYEmpty::Output(std::ostream &out, bool block) const {
    if (next_ != NULL)
        CYSource::Output(out, block);
    else
        out << "{}";
}

void CYExpress::Output(std::ostream &out) const {
    expression_->Output(out);
    out << ';';
}

void CYExpression::Part(std::ostream &out) const {
    // XXX: this should notice "in" expressions
    // XXX: this should handle LeftHandSideExpression
    Output(out);
}

void CYCompound::Output(std::ostream &out) const {
    if (CYExpression *expression = expressions_)
        for (;;) {
            expression->Output(out);
            expression = expression->next_;
            if (expression == NULL)
                break;
            out << ',';
        }
}

void CYExpression::Output(std::ostream &out, unsigned precedence) const {
    bool protect(precedence < Precedence());
    if (protect)
        out << '(';
    Output(out);
    if (protect)
        out << ')';
}

void CYFor::Output(std::ostream &out) const {
    out << "for(";
    if (initialiser_ != NULL)
        initialiser_->Part(out);
    out << ';';
    if (test_ != NULL)
        test_->Output(out);
    out << ';';
    if (increment_ != NULL)
        increment_->Output(out);
    out << ')';
    code_->Output(out, false);
}

void CYForIn::Output(std::ostream &out) const {
    out << "for(";
    initialiser_->Part(out);
    out << " in ";
    set_->Output(out);
    out << ')';
    code_->Output(out, false);
}

void CYFunction::Output(std::ostream &out) const {
    CYLambda::Output(out);
}

void CYIf::Output(std::ostream &out) const {
    out << "if(";
    test_->Output(out);
    out << ')';
    true_->Output(out, true);
    if (false_ != NULL) {
        out << "else ";
        false_->Output(out, false);
    }
}

void CYIndirect::Output(std::ostream &out) const {
    rhs_->Output(out, 1);
    out << "[0]";
}

void CYInfix::Output(std::ostream &out) const {
    lhs_->Output(out, Precedence());
    out << Operator();
    rhs_->Output(out, Precedence() - 1);
}

void CYLambda::Output(std::ostream &out) const {
    out << "function";
    if (name_ != NULL)
        out << ' ' << *name_;
    out << '(';
    if (parameters_ != NULL)
        out << *parameters_;
    out << ')';
    body_->Output(out, true);
}

void CYMember::Output(std::ostream &out) const {
    object_->Output(out, Precedence());
    if (const char *word = property_->Word())
        out << '.' << word;
    else {
        out << '[';
        property_->Output(out);
        out << ']';
    }
}

void CYMessage::Output(std::ostream &out) const {
    out << "objc_msgSend(";
    self_->Output(out, CYPA);
    out << ",\"";
    for (CYArgument *argument(arguments_); argument != NULL; argument = argument->next_)
        if (argument->name_ != NULL) {
            out << *argument->name_;
            if (argument->value_ != NULL)
                out << ':';
        }
    out << "\"";
    for (CYArgument *argument(arguments_); argument != NULL; argument = argument->next_)
        if (argument->value_ != NULL) {
            out << ",";
            argument->value_->Output(out, CYPA);
        }
    out << ')';
}

void CYNew::Output(std::ostream &out) const {
    out << "new";
    // XXX: I don't /always/ need this character
    out << ' ';
    constructor_->Output(out, Precedence());
    out << '(';
    if (arguments_ != NULL)
        arguments_->Output(out);
    out << ')';
}

void CYNull::Output(std::ostream &out) const {
    CYWord::Output(out);
}

void CYNumber::Output(std::ostream &out) const {
    // XXX: this is not a useful formatting
    out << Value();
}

void CYObject::Output(std::ostream &out) const {
    out << '{';
    if (property_ != NULL)
        property_->Output(out);
    out << '}';
}

void CYParameter::Output(std::ostream &out) const {
    out << *name_;
    if (next_ != NULL) {
        out << ',';
        out << *next_;
    }
}

void CYPostfix::Output(std::ostream &out) const {
    lhs_->Output(out, Precedence());
    out << Operator();
}

void CYPrefix::Output(std::ostream &out) const {
    out << Operator();
    rhs_->Output(out, Precedence());
}

void CYProperty::Output(std::ostream &out) const {
    out << *name_ << ':';
    value_->Output(out, CYPA);
    if (next_ != NULL) {
        out << ',';
        next_->Output(out);
    }
}

void CYReturn::Output(std::ostream &out) const {
    out << "return";
    if (value_ != NULL) {
        out << ' ';
        value_->Output(out);
    }
    out << ';';
}

void CYSelector::Output(std::ostream &out) const {
    out << "new SEL(\"";
    if (name_ != NULL)
        name_->Output(out);
    out << "\")";
}

void CYSelectorPart::Output(std::ostream &out) const {
    if (name_ != NULL)
        out << *name_;
    if (value_)
        out << ':';
    if (next_ != NULL)
        next_->Output(out);
}

void CYSource::Show(std::ostream &out) const {
    for (const CYSource *next(this); next != NULL; next = next->next_)
        next->Output(out, false);
}

void CYSource::Output(std::ostream &out, bool block) const {
    if (!block && next_ == NULL)
        Output(out);
    else {
        out << '{';
        Show(out);
        out << '}';
    }
}

void CYString::Output(std::ostream &out) const {
    out << '\"';
    for (const char *value(value_), *end(value_ + size_); value != end; ++value)
        switch (*value) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            case '\v': out << "\\v"; break;

            default:
                if (*value < 0x20 || *value >= 0x7f)
                    out << "\\x" << std::setbase(16) << std::setw(2) << std::setfill('0') << unsigned(*value);
                else
                    out << *value;
        }
    out << '\"';
}

void CYSwitch::Output(std::ostream &out) const {
    out << "switch(";
    value_->Output(out);
    out << "){";
    if (clauses_ != NULL)
        out << *clauses_;
    out << '}';
}

void CYThis::Output(std::ostream &out) const {
    CYWord::Output(out);
}

void CYThrow::Output(std::ostream &out) const {
    out << "throw";
    if (value_ != NULL) {
        out << ' ';
        value_->Output(out);
    }
    out << ';';
}

void CYTry::Output(std::ostream &out) const {
    out << "try";
    try_->Output(out, true);
    if (catch_ != NULL)
        out << catch_;
    if (finally_ != NULL) {
        out << "finally";
        finally_->Output(out, true);
    }
}

void CYVariable::Output(std::ostream &out) const {
    out << *name_;
}

void CYWhile::Output(std::ostream &out) const {
    out << "while(";
    test_->Output(out);
    out << ')';
    code_->Output(out, false);
}

void CYWith::Output(std::ostream &out) const {
    out << "with(";
    scope_->Output(out);
    out << ')';
    code_->Output(out, false);
}

void CYWord::Output(std::ostream &out) const {
    out << Value();
}
