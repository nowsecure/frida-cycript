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
    return flags;
}

_finline CYFlags CYRight(CYFlags flags) {
    return flags & ~CYNoBF;
}

_finline CYFlags CYCenter(CYFlags flags) {
    return CYRight(flags);
}

#define CYPA 16

void CYOutput::Terminate() {
    out_ << ';';
    mode_ = NoMode;
}

CYOutput &CYOutput::operator <<(char rhs) {
    if (rhs == ' ' || rhs == '\n')
        if (pretty_)
            out_ << rhs;
        else goto done;
    else if (rhs == '\t')
        if (pretty_)
            for (unsigned i(0); i != indent_; ++i)
                out_ << "    ";
        else goto done;
    else goto work;

    mode_ = NoMode;
    goto done;

  work:
    if (mode_ == Terminated && rhs != '}')
        out_ << ';';

    if (rhs == ';') {
        if (pretty_)
            goto none;
        else {
            mode_ = Terminated;
            goto done;
        }
    } else if (rhs == '-') {
        if (mode_ == NoHyphen)
            out_ << ' ';
        mode_ = NoHyphen;
    } else if (WordEndRange_[rhs]) {
        if (mode_ == NoLetter)
            out_ << ' ';
        mode_ = NoLetter;
    } else none:
        mode_ = NoMode;

    out_ << rhs;
  done:
    return *this;
}

CYOutput &CYOutput::operator <<(const char *rhs) {
    size_t size(strlen(rhs));

    if (size == 1)
        return *this << *rhs;

    if (mode_ == Terminated)
        out_ << ';';
    else if (
        mode_ == NoHyphen && *rhs == '-' ||
        mode_ == NoLetter && WordEndRange_[*rhs]
    )
        out_ << ' ';

    if (WordEndRange_[rhs[size - 1]])
        mode_ = NoLetter;
    else
        mode_ = NoMode;

    out_ << rhs;
    return *this;
}

void OutputBody(CYOutput &out, CYStatement *body) {
    out << ' ' << '{' << '\n';
    ++out.indent_;
    if (body != NULL)
        body->Multiple(out);
    --out.indent_;
    out << '\t' << '}';
}

void CYAddressOf::Output(CYOutput &out, CYFlags flags) const {
    rhs_->Output(out, 1, CYLeft(flags));
    out << ".$cya()";
}

void CYArgument::Output(CYOutput &out) const {
    if (name_ != NULL) {
        out << *name_;
        if (value_ != NULL)
            out << ':' << ' ';
    }
    if (value_ != NULL)
        value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL) {
        if (next_->name_ == NULL)
            out << ',';
        out << ' ' << *next_;
    }
}

void CYArray::Output(CYOutput &out, CYFlags flags) const {
    out << '[' << elements_ << ']';
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
    lhs_->Output(out, Precedence() - 1, CYLeft(flags) | CYNoRightHand);
    out << ' ' << Operator() << ' ';
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYBlock::Output(CYOutput &out, CYFlags flags) const {
    statements_->Single(out, flags);
}

void CYBoolean::Output(CYOutput &out, CYFlags flags) const {
    out << (Value() ? "true" : "false");
}

void CYBreak::Output(CYOutput &out, CYFlags flags) const {
    out << "break";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYCall::Output(CYOutput &out, CYFlags flags) const {
    bool protect((flags & CYNoCall) != 0);
    if (protect)
        out << '(';
    function_->Output(out, Precedence(), protect ? CYNoFlags : flags);
    out << '(' << arguments_ << ')';
    if (protect)
        out << ')';
}

void CYCatch::Output(CYOutput &out) const {
    out << "catch" << ' ' << '(' << *name_ << ')' << ' ' << '{';
    if (code_ != NULL)
        code_->Multiple(out);
    out << '}';
}

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

void CYCompound::Output(CYOutput &out, CYFlags flags) const {
    if (CYExpression *expression = expressions_)
        if (CYExpression *next = expression->next_) {
            expression->Output(out, CYLeft(flags));
            CYFlags center(CYCenter(flags));
            while (next != NULL) {
                expression = next;
                out << ',' << ' ';
                next = expression->next_;
                CYFlags right(next != NULL ? center : CYRight(flags));
                expression->Output(out, right);
            }
        } else
            expression->Output(out, flags);
}

void CYComprehension::Output(CYOutput &out) const {
    Begin_(out);
    out << next_;
}

void CYCondition::Output(CYOutput &out, CYFlags flags) const {
    test_->Output(out, Precedence() - 1, CYLeft(flags));
    out << ' ' << '?' << ' ';
    if (true_ != NULL)
        true_->Output(out, CYPA, CYNoFlags);
    out << ' ' << ':' << ' ';
    false_->Output(out, CYPA, CYRight(flags));
}

void CYContinue::Output(CYOutput &out, CYFlags flags) const {
    out << "continue";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYClause::Output(CYOutput &out) const {
    if (case_ != NULL)
        out << "case" << ' ' << *case_;
    else
        out << "default";
    out << ':' << '\n';
    if (code_ != NULL)
        code_->Multiple(out, CYNoFlags);
    out << next_;
}

const char *CYDeclaration::ForEachIn() const {
    return identifier_->Value();
}

void CYDeclaration::ForIn(CYOutput &out, CYFlags flags) const {
    out << "var";
    Output(out, CYRight(flags));
}

void CYDeclaration::ForEachIn(CYOutput &out) const {
    out << *identifier_;
}

void CYDeclaration::Output(CYOutput &out, CYFlags flags) const {
    out << *identifier_;
    if (initialiser_ != NULL) {
        out << ' ' << '=' << ' ';
        initialiser_->Output(out, CYPA, CYRight(flags));
    }
}

void CYDeclarations::For(CYOutput &out) const {
    out << "var";
    Output(out, CYNoIn);
}

void CYDeclarations::Output(CYOutput &out) const {
    Output(out, CYNoFlags);
}

void CYDeclarations::Output(CYOutput &out, CYFlags flags) const {
    const CYDeclarations *declaration(this);
    bool first(true);
  output:
    CYDeclarations *next(declaration->next_);
    CYFlags jacks(first ? CYLeft(flags) : next == NULL ? CYRight(flags) : CYCenter(flags));
    first = false;
    declaration->declaration_->Output(out, jacks);

    if (next != NULL) {
        out << ',' << ' ';
        declaration = next;
        goto output;
    }
}

void CYDirectMember::Output(CYOutput &out, CYFlags flags) const {
    object_->Output(out, Precedence(), CYLeft(flags));
    if (const char *word = property_->Word())
        out << '.' << word;
    else
        out << '[' << *property_ << ']';
}

void CYDoWhile::Output(CYOutput &out, CYFlags flags) const {
    out << "do";
    code_->Single(out, CYNoFlags);
    out << "while" << ' ' << '(' << *test_ << ')';
}

void CYElement::Output(CYOutput &out) const {
    if (value_ != NULL)
        value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL || value_ == NULL) {
        out << ',';
        if (next_ != NULL && next_->value_ != NULL)
            out << ' ';
    }
    if (next_ != NULL)
        next_->Output(out);
}

void CYEmpty::Output(CYOutput &out, CYFlags flags) const {
    out.Terminate();
}

void CYExpress::Output(CYOutput &out, CYFlags flags) const {
    expression_->Output(out, flags | CYNoBF);
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
    Output(out, CYPA, CYNoRightHand);
}

void CYExpression::ForIn(CYOutput &out, CYFlags flags) const {
    Output(out, flags | CYNoRightHand);
}

void CYExpression::Output(CYOutput &out) const {
    Output(out, CYNoFlags);
}

void CYExpression::Output(CYOutput &out, unsigned precedence, CYFlags flags) const {
    if (precedence < Precedence() || (flags & CYNoRightHand) != 0 && RightHand())
        out << '(' << *this << ')';
    else
        Output(out, flags);
}

void CYField::Output(CYOutput &out) const {
    // XXX: implement!
}

void CYFinally::Output(CYOutput &out) const {
    out << "finally" << ' ' << '{';
    if (code_ != NULL)
        code_->Multiple(out);
    out << '}';
}

void CYFor::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << '(';
    if (initialiser_ != NULL)
        initialiser_->For(out);
    out.Terminate();
    out << test_;
    out.Terminate();
    out << increment_;
    out << ')';
    code_->Single(out, CYNoFlags);
}

void CYForEachIn::Output(CYOutput &out, CYFlags flags) const {
    out << "with({$cys:0,$cyt:0}){";

    out << "$cys=";
    set_->Output(out, CYPA, CYNoFlags);
    out << ';';

    out << "for($cyt in $cys){";

    initialiser_->ForEachIn(out);
    out << "=$cys[$cyt];";

    code_->Multiple(out);

    out << '}';

    out << '}';
}

void CYForEachInComprehension::Begin_(CYOutput &out) const {
    out << "(function($cys){";
    out << "$cys=";
    set_->Output(out, CYPA, CYNoFlags);
    out << ';';

    out << "for(" << *name_ << " in $cys){";
    out << *name_ << "=$cys[" << *name_ << "];";
}

void CYForEachInComprehension::End_(CYOutput &out) const {
    out << "}}());";
}

void CYForIn::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << '(';
    initialiser_->ForIn(out, CYNoIn);
    out << "in" << *set_ << ')';
    code_->Single(out, CYRight(flags));
}

void CYForInComprehension::Begin_(CYOutput &out) const {
    out << "for" << ' ' << '(' << *name_ << "in" << *set_ << ')';
}

void CYFunction::Output(CYOutput &out, CYFlags flags) const {
    // XXX: one could imagine using + here to save a byte
    bool protect((flags & CYNoFunction) != 0);
    if (protect)
        out << '(';
    out << "function";
    if (name_ != NULL)
        out << ' ' << *name_;
    out << '(' << parameters_ << ')';
    OutputBody(out, body_);
    if (protect)
        out << ')';
}

void CYFunctionExpression::Output(CYOutput &out, CYFlags flags) const {
    CYFunction::Output(out, flags);
}

void CYFunctionStatement::Output(CYOutput &out, CYFlags flags) const {
    CYFunction::Output(out, flags);
}

void CYFunctionParameter::Output(CYOutput &out) const {
    out << *name_;
    if (next_ != NULL)
        out << ',' << ' ' << *next_;
}

void CYIf::Output(CYOutput &out, CYFlags flags) const {
    bool protect(false);
    if (false_ == NULL && (flags & CYNoDangle) != 0) {
        protect = true;
        out << '{';
    }

    out << "if" << ' ' << '(' << *test_ << ')';

    CYFlags right(protect ? CYNoFlags : CYRight(flags));
    CYFlags jacks(CYNoDangle);
    if (false_ == NULL)
        jacks |= right;

    bool single(true_->Single(out, jacks));

    if (false_ != NULL) {
        out << (single ? '\t' : ' ');
        out << "else";
        false_->Single(out, right);
    }

    if (protect)
        out << '}';
}

void CYIfComprehension::Begin_(CYOutput &out) const {
    out << "if" << '(' << *test_ << ')';
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
    else
        out << '[' << *property_ << ']';
}

void CYInfix::Output(CYOutput &out, CYFlags flags) const {
    const char *name(Operator());
    bool protect((flags & CYNoIn) != 0 && strcmp(name, "in"));
    if (protect)
        out << '(';
    CYFlags left(protect ? CYNoFlags : CYLeft(flags));
    lhs_->Output(out, Precedence(), left);
    out << ' ' << name << ' ';
    CYFlags right(protect ? CYNoFlags : CYRight(flags));
    rhs_->Output(out, Precedence() - 1, right);
    if (protect)
        out << ')';
}

void CYLet::Output(CYOutput &out, CYFlags flags) const {
    out << "let" << ' ' << '(' << *declarations_ << ')' << ' ' << '{';
    if (statements_ != NULL)
        statements_->Multiple(out);
    out << '}';
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
        body_->Multiple(out);
    out << "}.call(self);},$cyt),$cyt);";
}

void CYNew::Output(CYOutput &out, CYFlags flags) const {
    out << "new" << ' ';
    CYFlags jacks(CYNoCall | CYCenter(flags));
    constructor_->Output(out, Precedence(), jacks);
    if (arguments_ != NULL)
        out << '(' << *arguments_ << ')';
}

void CYNull::Output(CYOutput &out, CYFlags flags) const {
    CYWord::Output(out);
}

void CYNumber::Output(CYOutput &out, CYFlags flags) const {
    char value[32];
    sprintf(value, "%.17g", Value());
    out << value;
}

void CYNumber::PropertyName(CYOutput &out) const {
    Output(out, CYNoFlags);
}

void CYObject::Output(CYOutput &out, CYFlags flags) const {
    bool protect((flags & CYNoBrace) != 0);
    if (protect)
        out << '(';
    out << '{';
    out << property_;
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
    out << name;
    if (Alphabetic())
        out << ' ';
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYProperty::Output(CYOutput &out) const {
    name_->PropertyName(out);
    out << ':' << ' ';
    value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL)
        out << ',' << ' ' << *next_;
}

void CYRegEx::Output(CYOutput &out, CYFlags flags) const {
    out << Value();
}

void CYReturn::Output(CYOutput &out, CYFlags flags) const {
    out << "return" << value_ << ';';
}

void CYSelector::Output(CYOutput &out, CYFlags flags) const {
    out << "new Selector(\"";
    if (name_ != NULL)
        name_->Output(out);
    out << "\")";
}

void CYSelectorPart::Output(CYOutput &out) const {
    out << name_;
    if (value_)
        out << ':';
    out << next_;
}

void CYSend::Output(CYOutput &out, CYFlags flags) const {
    out << "objc_msgSend(";
    self_->Output(out, CYPA, CYNoFlags);
    out << ',';
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
            out << ',';
            argument->value_->Output(out, CYPA, CYNoFlags);
        }
    out << ')';
}

void CYStatement::Multiple(CYOutput &out, CYFlags flags) const {
    bool first(true);
    for (const CYStatement *next(this); next != NULL; next = next->next_) {
        bool last(next->next_ == NULL);
        CYFlags jacks(first ? last ? flags : CYLeft(flags) : last ? CYCenter(flags) : CYRight(flags));
        first = false;
        out << '\t';
        next->Output(out, jacks);
        out << '\n';
    }
}

bool CYStatement::Single(CYOutput &out, CYFlags flags) const {
    if (next_ != NULL) {
        out << ' ' << '{' << '\n';
        ++out.indent_;
        Multiple(out);
        --out.indent_;
        out << '\t' << '}';
        return false;
    } else {
        for (CYLabel *label(labels_); label != NULL; label = label->next_)
            out << ' ' << *label->name_ << ':';
        out << '\n';
        ++out.indent_;
        out << '\t';
        Output(out, flags);
        out << '\n';
        --out.indent_;
        return true;
    }
}

void CYString::Output(CYOutput &out, CYFlags flags) const {
    unsigned quot(0), apos(0);
    for (const char *value(value_), *end(value_ + size_); value != end; ++value)
        if (*value == '"')
            ++quot;
        else if (*value == '\'')
            ++apos;

    bool single(quot > apos);

    std::ostringstream str;

    str << (single ? '\'' : '"');
    for (const char *value(value_), *end(value_ + size_); value != end; ++value)
        switch (*value) {
            case '\\': str << "\\\\"; break;
            case '\b': str << "\\b"; break;
            case '\f': str << "\\f"; break;
            case '\n': str << "\\n"; break;
            case '\r': str << "\\r"; break;
            case '\t': str << "\\t"; break;
            case '\v': str << "\\v"; break;

            case '"':
                if (!single)
                    str << "\\\"";
                else goto simple;
            break;

            case '\'':
                if (single)
                    str << "\\'";
                else goto simple;
            break;

            default:
                if (*value < 0x20 || *value >= 0x7f)
                    str << "\\x" << std::setbase(16) << std::setw(2) << std::setfill('0') << unsigned(*value);
                else simple:
                    str << *value;
        }
    str << (single ? '\'' : '"');

    out << str.str().c_str();
}

void CYString::PropertyName(CYOutput &out) const {
    if (const char *word = Word())
        out << word;
    else
        out << *this;
}

const char *CYString::Word() const {
    if (size_ == 0 || !WordStartRange_[value_[0]])
        return NULL;
    for (size_t i(1); i != size_; ++i)
        if (!WordEndRange_[value_[i]])
            return NULL;
    const char *value(Value());
    // XXX: we should probably include the full ECMAScript3+5 list.
    static const char *reserveds[] = {"class", "const", "enum", "export", "extends", "import", "super", NULL};
    for (const char **reserved(reserveds); *reserved != NULL; ++reserved)
        if (strcmp(*reserved, value) == 0)
            return NULL;
    return value;
}

void CYSwitch::Output(CYOutput &out, CYFlags flags) const {
    out << "switch" << ' ' << '(' << *value_ << ')' << ' ' << '{';
    out << clauses_;
    out << '}';
}

void CYThis::Output(CYOutput &out, CYFlags flags) const {
    CYWord::Output(out);
}

void CYThrow::Output(CYOutput &out, CYFlags flags) const {
    out << "throw" << value_ << ';';
}

void CYTry::Output(CYOutput &out, CYFlags flags) const {
    out << "try" << ' ' << '{';
    if (code_ != NULL)
        code_->Multiple(out);
    out << '}';
    out << catch_;
    out << finally_;
}

void CYVar::Output(CYOutput &out, CYFlags flags) const {
    out << "var";
    declarations_->Output(out, flags);
    out << ';';
}

void CYVariable::Output(CYOutput &out, CYFlags flags) const {
    out << *name_;
}

void CYWhile::Output(CYOutput &out, CYFlags flags) const {
    out << "while" << '(' << *test_ << ')';
    code_->Single(out, CYRight(flags));
}

void CYWith::Output(CYOutput &out, CYFlags flags) const {
    out << "with" << '(' << *scope_ << ')';
    code_->Single(out, CYRight(flags));
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
