#include "Parser.hpp"

#include <iostream>
#include <iomanip>

#include <objc/runtime.h>
#include <sstream>

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

void CYAddressOf::Output(CYOutput &out, CYFlags flags) const {
    rhs_->Output(out, 1, CYLeft(flags));
    out << ".$cya()";
}

void CYArgument::Output(CYOutput &out) const {
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

void CYArray::Output(CYOutput &out, CYFlags flags) const {
    out << '[';
    if (elements_ != NULL)
        elements_->Output(out);
    out << ']';
}

void CYArrayComprehension::Output(CYOutput &out, CYFlags flags) const {
    // XXX: I don't necc. need the ()s
    out << "(function($cyv";
    for (CYComprehension *comprehension(comprehensions_); comprehension != NULL; comprehension = comprehension->next_)
        if (const char *name = comprehension->Name())
            out << ',' << name;
    out << "){";
    out << "$cyv=[];";
    comprehensions_->Output(out);
    out << "$cyv.push(";
    expression_->Output(out, CYPA, CYNoFlags);
    out << ");";
    for (CYComprehension *comprehension(comprehensions_); comprehension != NULL; comprehension = comprehension->next_)
        comprehension->End_(out);
    out << "return $cyv;";
    out << "}())";
}

void CYAssignment::Output(CYOutput &out, CYFlags flags) const {
    lhs_->Output(out, Precedence() - 1, CYLeft(flags));
    out << Operator();
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYBlock::Output(CYOutput &out) const {
    for (CYSource *statement(statements_); statement != NULL; statement = statement->next_)
        statement->Output(out);
}

void CYBoolean::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << (Value() ? "true" : "false");
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYBreak::Output(CYOutput &out) const {
    out << "break";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYCall::Output(CYOutput &out, CYFlags flags) const {
    function_->Output(out, Precedence(), CYLeft(flags));
    out << '(';
    if (arguments_ != NULL)
        arguments_->Output(out);
    out << ')';
}

void CYCatch::Output(CYOutput &out) const {
    out << "catch(" << *name_ << ')';
    code_->Output(out, true);
}

void CYCategory::Output(CYOutput &out) const {
    out << "(function($cys,$cyp,$cyc,$cyn,$cyt){";
    out << "$cyp=object_getClass($cys);";
    out << "$cyc=$cys;";
    if (messages_ != NULL)
        messages_->Output(out, true);
    out << "})(";
    name_->ClassName(out, true);
    out << ");";
}

void CYClass::Output(CYOutput &out) const {
    Output(out, CYNoBF);
    out << ";";
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

void CYCompound::Output(CYOutput &out, CYFlags flags) const {
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

void CYComprehension::Output(CYOutput &out) const {
    Begin_(out);
    if (next_ != NULL)
        next_->Output(out);
}

void CYCondition::Output(CYOutput &out, CYFlags flags) const {
    test_->Output(out, Precedence() - 1, CYLeft(flags));
    out << '?';
    if (true_ != NULL)
        true_->Output(out, CYPA, CYNoFlags);
    out << ':';
    false_->Output(out, CYPA, CYRight(flags));
}

void CYContinue::Output(CYOutput &out) const {
    out << "continue";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYClause::Output(CYOutput &out) const {
    if (case_ != NULL) {
        out << "case";
        case_->Output(out, CYNoLeader);
    } else
        out << "default";
    out << ':';
    if (code_ != NULL)
        code_->Output(out, false);
    if (next_ != NULL)
        out << *next_;
}

const char *CYDeclaration::ForEachIn() const {
    return identifier_->Value();
}

void CYDeclaration::ForIn(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << "var ";
    Output(out, CYRight(flags));
}

void CYDeclaration::ForEachIn(CYOutput &out) const {
    out << *identifier_;
}

void CYDeclaration::Output(CYOutput &out, CYFlags flags) const {
    out << *identifier_;
    if (initialiser_ != NULL) {
        out << '=';
        initialiser_->Output(out, CYPA, CYRight(flags));
    } else if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYDeclarations::For(CYOutput &out) const {
    out << "var ";
    Output(out, CYNoIn);
}

void CYDeclarations::Output(CYOutput &out, CYFlags flags) const {
    const CYDeclarations *declaration(this);
  output:
    CYDeclarations *next(declaration->next_);
    CYFlags right(next == NULL ? CYRight(flags) : CYCenter(flags));
    declaration->declaration_->Output(out, right);

    if (next != NULL) {
        out << ',';
        declaration = next;
        goto output;
    }
}

void CYDirectMember::Output(CYOutput &out, CYFlags flags) const {
    object_->Output(out, Precedence(), CYLeft(flags));
    if (const char *word = property_->Word())
        out << '.' << word;
    else {
        out << '[';
        property_->Output(out, CYNoFlags);
        out << ']';
    }
}

void CYDoWhile::Output(CYOutput &out) const {
    // XXX: extra space character!
    out << "do ";
    code_->Output(out, false);
    out << "while(";
    test_->Output(out, CYNoFlags);
    out << ')';
}

void CYElement::Output(CYOutput &out) const {
    if (value_ != NULL)
        value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL || value_ == NULL)
        out << ',';
    if (next_ != NULL)
        next_->Output(out);
}

void CYEmpty::Output(CYOutput &out) const {
    out << ';';
}

void CYEmpty::Output(CYOutput &out, bool block) const {
    if (next_ != NULL)
        CYSource::Output(out, block);
    else
        out << "{}";
}

void CYExpress::Output(CYOutput &out) const {
    expression_->Output(out, CYNoBF);
    out << ';';
}

void CYExpression::ClassName(CYOutput &out, bool object) const {
    Output(out, CYPA, CYNoFlags);
}

const char *CYExpression::ForEachIn() const {
    return NULL;
}

void CYExpression::For(CYOutput &out) const {
    Output(out, CYNoIn);
}

void CYExpression::ForEachIn(CYOutput &out) const {
    // XXX: this should handle LeftHandSideExpression
    Output(out, CYPA, CYNoFlags);
}

void CYExpression::ForIn(CYOutput &out, CYFlags flags) const {
    // XXX: this should handle LeftHandSideExpression
    Output(out, flags);
}

void CYExpression::Output(CYOutput &out, unsigned precedence, CYFlags flags) const {
    if (precedence < Precedence()) {
        out << '(';
        Output(out, CYNoFlags);
        out << ')';
    } else
        Output(out, flags);
}

void CYField::Output(CYOutput &out) const {
    // XXX: implement!
}

void CYFor::Output(CYOutput &out) const {
    out << "for(";
    if (initialiser_ != NULL)
        initialiser_->For(out);
    out << ';';
    if (test_ != NULL)
        test_->Output(out, CYNoFlags);
    out << ';';
    if (increment_ != NULL)
        increment_->Output(out, CYNoFlags);
    out << ')';
    code_->Output(out, false);
}

void CYForEachIn::Output(CYOutput &out) const {
    out << "with({$cys:0,$cyt:0}){";

    out << "$cys=";
    set_->Output(out, CYPA, CYNoFlags);
    out << ";";

    out << "for($cyt in $cys){";

    initialiser_->ForEachIn(out);
    out << "=$cys[$cyt];";

    code_->Show(out);

    out << '}';

    out << '}';
}

void CYForEachInComprehension::Begin_(CYOutput &out) const {
    out << "(function($cys){";
    out << "$cys=";
    set_->Output(out, CYPA, CYNoFlags);
    out << ";";

    out << "for(" << *name_ << " in $cys){";
    out << *name_ << "=$cys[" << *name_ << "];";
}

void CYForEachInComprehension::End_(CYOutput &out) const {
    out << "}}());";
}

void CYForIn::Output(CYOutput &out) const {
    out << "for(";
    initialiser_->ForIn(out, CYNoIn | CYNoTrailer);
    out << "in";
    set_->Output(out, CYNoLeader);
    out << ')';
    code_->Output(out, false);
}

void CYForInComprehension::Begin_(CYOutput &out) const {
    out << "for(" << *name_ << " in";
    set_->Output(out, CYNoLeader);
    out << ')';
}

void CYFunction::Output(CYOutput &out) const {
    CYLambda::Output(out, CYNoFlags);
}

void CYFunctionParameter::Output(CYOutput &out) const {
    out << *name_;
    if (next_ != NULL) {
        out << ',';
        out << *next_;
    }
}

void CYIf::Output(CYOutput &out) const {
    out << "if(";
    test_->Output(out, CYNoFlags);
    out << ')';
    true_->Output(out, true);
    if (false_ != NULL) {
        out << "else ";
        false_->Output(out, false);
    }
}

void CYIfComprehension::Begin_(CYOutput &out) const {
    out << "if(";
    test_->Output(out, CYNoFlags);
    out << ')';
}

void CYIndirect::Output(CYOutput &out, CYFlags flags) const {
    rhs_->Output(out, 1, CYLeft(flags));
    out << ".$cyi";
}

void CYIndirectMember::Output(CYOutput &out, CYFlags flags) const {
    object_->Output(out, Precedence(), CYLeft(flags));
    out << ".$cyi";
    if (const char *word = property_->Word())
        out << '.' << word;
    else {
        out << '[';
        property_->Output(out, CYNoFlags);
        out << ']';
    }
}

void CYInfix::Output(CYOutput &out, CYFlags flags) const {
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
    if (strcmp(name, "-") == 0)
        right |= CYNoHyphen;
    rhs_->Output(out, Precedence() - 1, right);
    if (protect)
        out << ')';
}

void CYLambda::Output(CYOutput &out, CYFlags flags) const {
    bool protect((flags & CYNoFunction) != 0);
    if (protect)
        out << '(';
    else if ((flags & CYNoLeader) != 0)
        out << ' ';
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

void CYLet::Output(CYOutput &out) const {
    out << "let(";
    declarations_->Output(out, CYNoFlags);
    out << "){";
    if (statements_ != NULL)
        statements_->Show(out);
    out << "}";
}

void CYMessage::Output(CYOutput &out, bool replace) const {
    if (next_ != NULL)
        next_->Output(out, replace);
    out << "$cyn=new Selector(\"";
    for (CYMessageParameter *parameter(parameter_); parameter != NULL; parameter = parameter->next_)
        if (parameter->tag_ != NULL) {
            out << *parameter->tag_;
            if (parameter->name_ != NULL)
                out << ':';
        }
    out << "\");";
    out << "$cyt=$cyn.type($cy" << (instance_ ? 's' : 'p') << ");";
    out << "class_" << (replace ? "replace" : "add") << "Method($cy" << (instance_ ? 'c' : 'm') << ",$cyn,";
    out << "new Functor(function(self,_cmd";
    for (CYMessageParameter *parameter(parameter_); parameter != NULL; parameter = parameter->next_)
        if (parameter->name_ != NULL)
            out << ',' << *parameter->name_;
    out << "){return function(){";
    if (body_ != NULL)
        body_->Show(out);
    out << "}.call(self);},$cyt),$cyt);";
}

void CYNew::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << "new";
    constructor_->Output(out, Precedence(), CYCenter(flags) | CYNoLeader);
    out << '(';
    if (arguments_ != NULL)
        arguments_->Output(out);
    out << ')';
}

void CYNull::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    CYWord::Output(out);
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYNumber::Output(CYOutput &out, CYFlags flags) const {
    double value(Value());
    if ((flags & CYNoLeader) != 0 || value < 0 && (flags & CYNoHyphen) != 0)
        out << ' ';
    // XXX: decide on correct precision
    out.out_ << std::setprecision(9) << value;
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYNumber::PropertyName(CYOutput &out) const {
    Output(out);
}

void CYObject::Output(CYOutput &out, CYFlags flags) const {
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

void CYPostfix::Output(CYOutput &out, CYFlags flags) const {
    lhs_->Output(out, Precedence(), CYLeft(flags));
    out << Operator();
}

void CYPrefix::Output(CYOutput &out, CYFlags flags) const {
    const char *name(Operator());
    bool alphabetic(Alphabetic());
    if (alphabetic && (flags & CYNoLeader) != 0 || name[0] == '-' && (flags & CYNoHyphen) != 0)
        out << ' ';
    out << name;
    CYFlags right(CYRight(flags));
    if (alphabetic)
        right |= CYNoLeader;
    rhs_->Output(out, Precedence(), right);
}

void CYProperty::Output(CYOutput &out) const {
    name_->PropertyName(out);
    out << ':';
    value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL) {
        out << ',';
        next_->Output(out);
    }
}

void CYRegEx::Output(CYOutput &out, CYFlags flags) const {
    out << Value();
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYReturn::Output(CYOutput &out) const {
    out << "return";
    if (value_ != NULL)
        value_->Output(out, CYNoLeader);
    out << ';';
}

void CYSelector::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << "new Selector(\"";
    if (name_ != NULL)
        name_->Output(out);
    out << "\")";
}

void CYSelectorPart::Output(CYOutput &out) const {
    if (name_ != NULL)
        out << *name_;
    if (value_)
        out << ':';
    if (next_ != NULL)
        next_->Output(out);
}

void CYSend::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << "objc_msgSend(";
    self_->Output(out, CYPA, CYNoFlags);
    out << ",";
    std::ostringstream name;
    for (CYArgument *argument(arguments_); argument != NULL; argument = argument->next_)
        if (argument->name_ != NULL) {
            name << *argument->name_;
            if (argument->value_ != NULL)
                name << ':';
        }
    out.out_ << reinterpret_cast<void *>(sel_registerName(name.str().c_str()));
    for (CYArgument *argument(arguments_); argument != NULL; argument = argument->next_)
        if (argument->value_ != NULL) {
            out << ",";
            argument->value_->Output(out, CYPA, CYNoFlags);
        }
    out << ')';
}

void CYSource::Show(CYOutput &out) const {
    for (const CYSource *next(this); next != NULL; next = next->next_)
        next->Output_(out);
}

void CYSource::Output(CYOutput &out, bool block) const {
    if (!block && !IsBlock())
        Output(out);
    else {
        out << '{';
        Show(out);
        out << '}';
    }
}

void CYSource::Output_(CYOutput &out) const {
    Output(out);
}

void CYStatement::Output_(CYOutput &out) const {
    for (CYLabel *label(labels_); label != NULL; label = label->next_)
        out << *label->name_ << ':';
    Output(out);
}

void CYString::Output(CYOutput &out, CYFlags flags) const {
    unsigned quot(0), apos(0);
    for (const char *value(value_), *end(value_ + size_); value != end; ++value)
        if (*value == '"')
            ++quot;
        else if (*value == '\'')
            ++apos;

    bool single(quot > apos);

    out << (single ? '\'' : '"');
    for (const char *value(value_), *end(value_ + size_); value != end; ++value)
        switch (*value) {
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            case '\v': out << "\\v"; break;

            case '"':
                if (!single)
                    out << "\\\"";
                else goto simple;
            break;

            case '\'':
                if (single)
                    out << "\\'";
                else goto simple;
            break;

            default:
                if (*value < 0x20 || *value >= 0x7f)
                    out.out_ << "\\x" << std::setbase(16) << std::setw(2) << std::setfill('0') << unsigned(*value);
                else simple:
                    out << *value;
        }
    out << (single ? '\'' : '"');
}

void CYString::PropertyName(CYOutput &out) const {
    if (const char *word = Word())
        out << word;
    else
        Output(out);
}

void CYSwitch::Output(CYOutput &out) const {
    out << "switch(";
    value_->Output(out, CYNoFlags);
    out << "){";
    if (clauses_ != NULL)
        out << *clauses_;
    out << '}';
}

void CYThis::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    CYWord::Output(out);
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYThrow::Output(CYOutput &out) const {
    out << "throw";
    if (value_ != NULL)
        value_->Output(out, CYNoLeader);
    out << ';';
}

void CYTry::Output(CYOutput &out) const {
    out << "try";
    try_->Output(out, true);
    if (catch_ != NULL)
        catch_->Output(out);
    if (finally_ != NULL) {
        out << "finally";
        finally_->Output(out, true);
    }
}

void CYVar::Output(CYOutput &out) const {
    out << "var ";
    declarations_->Output(out, CYNoFlags);
    out << ';';
}

void CYVariable::Output(CYOutput &out, CYFlags flags) const {
    if ((flags & CYNoLeader) != 0)
        out << ' ';
    out << *name_;
    if ((flags & CYNoTrailer) != 0)
        out << ' ';
}

void CYWhile::Output(CYOutput &out) const {
    out << "while(";
    test_->Output(out, CYNoFlags);
    out << ')';
    code_->Output(out, false);
}

void CYWith::Output(CYOutput &out) const {
    out << "with(";
    scope_->Output(out, CYNoFlags);
    out << ')';
    code_->Output(out, false);
}

void CYWord::ClassName(CYOutput &out, bool object) const {
    if (object)
        out << "objc_getClass(";
    out << '"' << Value() << '"';
    if (object)
        out << ')';
}

void CYWord::Output(CYOutput &out) const {
    out << Value();
}

void CYWord::PropertyName(CYOutput &out) const {
    Output(out);
}
