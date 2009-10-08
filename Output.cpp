#include "Parser.hpp"

#include <iostream>
#include <iomanip>

_finline CYFlags operator ~(CYFlags rhs) {
    return static_cast<CYFlags>(~static_cast<unsigned>(rhs));
}

_finline CYFlags operator &(CYFlags lhs, CYFlags rhs) {
    return static_cast<CYFlags>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));
}

_finline CYFlags operator |(CYFlags lhs, CYFlags rhs) {
    return static_cast<CYFlags>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

_finline CYFlags &operator |=(CYFlags &lhs, CYFlags rhs) {
    return lhs = lhs | rhs;
}

_finline CYFlags CYLeft(CYFlags flags) {
    return flags & ~CYNoTrailer;
}

_finline CYFlags CYCenter(CYFlags flags) {
    return flags & CYNoIn;
}

_finline CYFlags CYRight(CYFlags flags) {
    return flags & (CYNoIn | CYNoTrailer);
}

bool CYFalse::Value() const {
    return false;
}

bool CYTrue::Value() const {
    return true;
}

#define CYPA 16

void CYAddressOf::Output(std::ostream &out, CYFlags flags) const {
    rhs_->Output(out, 1, CYLeft(flags));
    out << ".$()";
}

void CYArgument::Output(std::ostream &out) const {
    if (name_ != NULL) {
        out << *name_;
        if (value_ != NULL)
            out << ":";
    }
    if (value_ != NULL)
        value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL) {
        if (next_->name_ == NULL)
            out << ',';
        else
            out << ' ';
        next_->Output(out);
    }
}

void CYArray::Output(std::ostream &out, CYFlags flags) const {
    out << '[';
    if (elements_ != NULL)
        elements_->Output(out);
    out << ']';
}

void CYAssignment::Output(std::ostream &out, CYFlags flags) const {
    lhs_->Output(out, Precedence() - 1, CYLeft(flags));
    out << Operator();
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYBoolean::Output(std::ostream &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << (Value() ? "true" : "false");
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYBreak::Output(std::ostream &out) const {
    out << "break";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYCall::Output(std::ostream &out, CYFlags flags) const {
    function_->Output(out, Precedence(), CYLeft(flags));
    out << '(';
    if (arguments_ != NULL)
        arguments_->Output(out);
    out << ')';
}

void CYCatch::Output(std::ostream &out) const {
    out << "catch(" << *name_ << ')';
    code_->Output(out, true);
}

void CYClass::Output(std::ostream &out) const {
    out << "(function($cys,$cyc,$cym,$cyn,$cyt){";
    out << "$cyc=objc_allocateClassPair($cys,\"" << *name_ << "\",0);";
    out << "$cym=object_getClass($cyc);";
    if (fields_ != NULL)
        fields_->Output(out);
    if (messages_ != NULL)
        messages_->Output(out);
    out << "objc_registerClassPair($cyc);";
    out << "})(";
    if (super_ != NULL)
        super_->Output(out, CYPA, CYNoFlags);
    else
        out << "null";
    out << ");";
}

void CYCondition::Output(std::ostream &out, CYFlags flags) const {
    test_->Output(out, Precedence() - 1, CYLeft(flags));
    out << '?';
    if (true_ != NULL)
        true_->Output(out, CYPA, CYNoFlags);
    out << ':';
    false_->Output(out, CYPA, CYRight(flags));
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
        case_->Output(out, CYNoFlags);
    } else
        out << "default";
    out << ':';
    if (code_ != NULL)
        code_->Output(out, false);
    out << *next_;
}

// XXX: deal with NoIn
void CYDeclaration::Part(std::ostream &out) const {
    out << "var ";
    Output(out);
}

void CYDeclaration::Output(std::ostream &out) const {
    out << *identifier_;
    if (initialiser_ != NULL) {
        out << '=';
        initialiser_->Output(out, CYPA, CYNoFlags);
    }
}

// XXX: deal with NoIn
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
    // XXX: extra space character!
    out << "do ";
    code_->Output(out, false);
    out << "while(";
    test_->Output(out, CYNoFlags);
    out << ')';
}

void CYElement::Output(std::ostream &out) const {
    if (value_ != NULL)
        value_->Output(out, CYPA, CYNoFlags);
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
    expression_->Output(out, CYNoFunction | CYNoBrace);
    out << ';';
}

void CYExpression::Part(std::ostream &out) const {
    // XXX: this should handle LeftHandSideExpression
    Output(out, CYNoIn);
}

void CYCompound::Output(std::ostream &out, CYFlags flags) const {
    if (CYExpression *expression = expressions_)
        if (CYExpression *next = expression->next_) {
            expression->Output(out, CYLeft(flags));
            CYFlags center(CYCenter(flags));
            while (next != NULL) {
                expression = next;
                out << ',';
                next = expression->next_;
                CYFlags right(next != NULL ? center : CYRight(flags));
                expression->Output(out, right);
            }
        } else
            expression->Output(out, flags);
}

void CYExpression::Output(std::ostream &out, unsigned precedence, CYFlags flags) const {
    if (precedence < Precedence()) {
        out << '(';
        Output(out, CYNoFlags);
        out << ')';
    } else
        Output(out, flags);
}

void CYField::Output(std::ostream &out) const {
    // XXX: implement!
}

void CYFor::Output(std::ostream &out) const {
    out << "for(";
    if (initialiser_ != NULL)
        initialiser_->Part(out);
    out << ';';
    if (test_ != NULL)
        test_->Output(out, CYNoFlags);
    out << ';';
    if (increment_ != NULL)
        increment_->Output(out, CYNoFlags);
    out << ')';
    code_->Output(out, false);
}

void CYForIn::Output(std::ostream &out) const {
    out << "for(";
    initialiser_->Part(out);
    // XXX: deal with this space character!
    out << ' ';
    out << "in";
    set_->Output(out, CYNoLeader);
    out << ')';
    code_->Output(out, false);
}

void CYFunction::Output(std::ostream &out) const {
    CYLambda::Output(out, CYNoFlags);
}

void CYFunctionParameter::Output(std::ostream &out) const {
    out << *name_;
    if (next_ != NULL) {
        out << ',';
        out << *next_;
    }
}

void CYIf::Output(std::ostream &out) const {
    out << "if(";
    test_->Output(out, CYNoFlags);
    out << ')';
    true_->Output(out, true);
    if (false_ != NULL) {
        out << "else ";
        false_->Output(out, false);
    }
}

void CYIndirect::Output(std::ostream &out, CYFlags flags) const {
    rhs_->Output(out, 1, CYLeft(flags));
    out << "[0]";
}

void CYInfix::Output(std::ostream &out, CYFlags flags) const {
    const char *name(Operator());
    bool protect((flags & CYNoIn) != 0 && strcmp(name, "in"));
    if (protect)
        out << '(';
    bool alphabetic(Alphabetic());
    CYFlags left(protect ? CYNoFlags : CYLeft(flags));
    if (alphabetic)
        left |= CYNoTrailer;
    lhs_->Output(out, Precedence(), left);
    out << name;
    CYFlags right(protect ? CYNoFlags : CYRight(flags));
    if (alphabetic)
        right |= CYNoLeader;
    rhs_->Output(out, Precedence() - 1, right);
    if (protect)
        out << ')';
}

void CYLambda::Output(std::ostream &out, CYFlags flags) const {
    bool protect((flags & CYNoFunction) != 0);
    if (protect)
        out << '(';
    out << "function";
    if (name_ != NULL)
        out << ' ' << *name_;
    out << '(';
    if (parameters_ != NULL)
        out << *parameters_;
    out << "){";
    if (body_ != NULL)
        body_->Show(out);
    out << '}';
    if (protect)
        out << ')';
}

void CYMember::Output(std::ostream &out, CYFlags flags) const {
    object_->Output(out, Precedence(), CYLeft(flags));
    if (const char *word = property_->Word())
        out << '.' << word;
    else {
        out << '[';
        property_->Output(out, CYNoFlags);
        out << ']';
    }
}

void CYMessage::Output(std::ostream &out) const {
    out << "$cyn=new Selector(\"";
    for (CYMessageParameter *parameter(parameter_); parameter != NULL; parameter = parameter->next_)
        if (parameter->tag_ != NULL) {
            out << *parameter->tag_;
            if (parameter->name_ != NULL)
                out << ':';
        }
    out << "\");";
    out << "$cyt=$cyn.type($cys," << (instance_ ? "true" : "false") << ");";
    out << "class_addMethod($cy" << (instance_ ? 'c' : 'm') << ",$cyn,";
    out << "new Functor(function(self,_cmd";
    for (CYMessageParameter *parameter(parameter_); parameter != NULL; parameter = parameter->next_)
        if (parameter->name_ != NULL)
            out << ',' << *parameter->name_;
    out << "){return function(){";
    if (body_ != NULL)
        body_->Show(out);
    out << "}.call(self);},$cyt),$cyt);";
}

void CYNew::Output(std::ostream &out, CYFlags flags) const {
    out << "new";
    constructor_->Output(out, Precedence(), CYCenter(flags) | CYNoLeader);
    out << '(';
    if (arguments_ != NULL)
        arguments_->Output(out);
    out << ')';
}

void CYNull::Output(std::ostream &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    CYWord::Output(out);
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYNumber::Output(std::ostream &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    // XXX: this is not a useful formatting
    out << Value();
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYObject::Output(std::ostream &out, CYFlags flags) const {
    bool protect((flags & CYNoBrace) != 0);
    if (protect)
        out << '(';
    out << '{';
    if (property_ != NULL)
        property_->Output(out);
    out << '}';
    if (protect)
        out << ')';
}

void CYPostfix::Output(std::ostream &out, CYFlags flags) const {
    lhs_->Output(out, Precedence(), CYLeft(flags));
    out << Operator();
}

void CYPrefix::Output(std::ostream &out, CYFlags flags) const {
    bool alphabetic(Alphabetic());
    out << Operator();
    CYFlags right(CYRight(flags));
    if (alphabetic)
        right |= CYNoLeader;
    rhs_->Output(out, Precedence(), right);
}

void CYProperty::Output(std::ostream &out) const {
    out << *name_ << ':';
    value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL) {
        out << ',';
        next_->Output(out);
    }
}

void CYReturn::Output(std::ostream &out) const {
    out << "return";
    if (value_ != NULL)
        value_->Output(out, CYNoLeader);
    out << ';';
}

void CYSelector::Output(std::ostream &out, CYFlags flags) const {
    out << "new Selector(\"";
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

void CYSend::Output(std::ostream &out, CYFlags flags) const {
    out << "objc_msgSend(";
    self_->Output(out, CYPA, CYNoFlags);
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
            argument->value_->Output(out, CYPA, CYNoFlags);
        }
    out << ')';
}

void CYSource::Show(std::ostream &out) const {
    for (const CYSource *next(this); next != NULL; next = next->next_)
        next->Output(out);
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

void CYString::Output(std::ostream &out, CYFlags flags) const {
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
    value_->Output(out, CYNoFlags);
    out << "){";
    if (clauses_ != NULL)
        out << *clauses_;
    out << '}';
}

void CYThis::Output(std::ostream &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    CYWord::Output(out);
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYThrow::Output(std::ostream &out) const {
    out << "throw";
    if (value_ != NULL)
        value_->Output(out, CYNoLeader);
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

void CYVariable::Output(std::ostream &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << *name_;
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYWhile::Output(std::ostream &out) const {
    out << "while(";
    test_->Output(out, CYNoFlags);
    out << ')';
    code_->Output(out, false);
}

void CYWith::Output(std::ostream &out) const {
    out << "with(";
    scope_->Output(out, CYNoFlags);
    out << ')';
    code_->Output(out, false);
}

void CYWord::Output(std::ostream &out) const {
    out << Value();
}
