/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include "cycript.hpp"

#include <sstream>

#include "Syntax.hpp"

void CYOutput::Terminate() {
    operator ()(';');
    mode_ = NoMode;
}

CYOutput &CYOutput::operator <<(char rhs) {
    if (rhs == ' ' || rhs == '\n')
        if (pretty_)
            operator ()(rhs);
        else goto done;
    else if (rhs == '\t')
        if (pretty_)
            for (unsigned i(0); i != indent_; ++i)
                operator ()("    ", 4);
        else goto done;
    else if (rhs == '\r') {
        if (right_) {
            operator ()('\n');
            right_ = false;
        } goto done;
    } else goto work;

    right_ = true;
    mode_ = NoMode;
    goto done;

  work:
    if (mode_ == Terminated && rhs != '}') {
        right_ = true;
        operator ()(';');
    }

    if (rhs == ';') {
        if (pretty_)
            goto none;
        else {
            mode_ = Terminated;
            goto done;
        }
    } else if (rhs == '+') {
        if (mode_ == NoPlus)
            operator ()(' ');
        mode_ = NoPlus;
    } else if (rhs == '-') {
        if (mode_ == NoHyphen)
            operator ()(' ');
        mode_ = NoHyphen;
    } else if (WordEndRange_[rhs]) {
        if (mode_ == NoLetter)
            operator ()(' ');
        mode_ = NoLetter;
    } else none:
        mode_ = NoMode;

    right_ = true;
    operator ()(rhs);
  done:
    return *this;
}

CYOutput &CYOutput::operator <<(const char *rhs) {
    size_t size(strlen(rhs));

    if (size == 1)
        return *this << *rhs;

    if (mode_ == Terminated)
        operator ()(';');
    else if (
        mode_ == NoPlus && *rhs == '+' ||
        mode_ == NoHyphen && *rhs == '-' ||
        mode_ == NoLetter && WordEndRange_[*rhs]
    )
        operator ()(' ');

    char last(rhs[size - 1]);
    if (WordEndRange_[last] || last == '/')
        mode_ = NoLetter;
    else
        mode_ = NoMode;

    right_ = true;
    operator ()(rhs, size);
    return *this;
}

void CYArgument::Output(CYOutput &out) const {
    if (name_ != NULL) {
        out << *name_;
        if (value_ != NULL)
            out << ':' << ' ';
    }
    if (value_ != NULL)
        value_->Output(out, CYAssign::Precedence_, CYNoFlags);
    if (next_ != NULL) {
        out << ',';
        out << ' ' << *next_;
    }
}

void CYArray::Output(CYOutput &out, CYFlags flags) const {
    out << '[' << elements_ << ']';
}

void CYArrayComprehension::Output(CYOutput &out, CYFlags flags) const {
    out << '[' << *expression_ << ' ' << *comprehensions_ << ']';
}

void CYAssignment::Output(CYOutput &out, CYFlags flags) const {
    lhs_->Output(out, Precedence() - 1, CYLeft(flags) | CYNoRightHand);
    out << ' ' << Operator() << ' ';
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYBlock::Output(CYOutput &out, CYFlags flags) const {
    out << '{' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << '\t' << '}';
}

void CYBoolean::Output(CYOutput &out, CYFlags flags) const {
    out << '!' << (Value() ? "0" : "1");
    if ((flags & CYNoInteger) != 0)
        out << '.';
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

namespace cy {
namespace Syntax {

void Catch::Output(CYOutput &out) const {
    out << ' ' << "catch" << ' ' << '(' << *name_ << ')' << ' ';
    out << '{' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << '\t' << '}';
}

} }

void CYClassExpression::Output(CYOutput &out, CYFlags flags) const {
    bool protect((flags & CYNoClass) != 0);
    if (protect)
        out << '(';
    out << "class";
    if (name_ != NULL)
        out << ' ' << *name_;
    out << *tail_;;
    if (protect)
        out << ')';
}

void CYClassStatement::Output(CYOutput &out, CYFlags flags) const {
    out << "class" << ' ' << *name_  << *tail_;
}

void CYClassTail::Output(CYOutput &out) const {
    if (extends_ == NULL)
        out << ' ';
    else {
        out << '\n';
        ++out.indent_;
        out << "extends" << ' ';
        extends_->Output(out, CYAssign::Precedence_ - 1, CYNoFlags);
        out << '\n';
        --out.indent_;
    }

    out << '{' << '\n';
    ++out.indent_;

    --out.indent_;
    out << '}';
}

void CYCompound::Output(CYOutput &out, CYFlags flags) const {
    if (next_ == NULL)
        expression_->Output(out, flags);
    else {
        expression_->Output(out, CYLeft(flags));
        out << ',' << ' ';
        next_->Output(out, CYRight(flags));
    }
}

void CYComputed::PropertyName(CYOutput &out) const {
    out << '[';
    expression_->Output(out, CYAssign::Precedence_, CYNoFlags);
    out << ']';
}

void CYCondition::Output(CYOutput &out, CYFlags flags) const {
    test_->Output(out, Precedence() - 1, CYLeft(flags));
    out << ' ' << '?' << ' ';
    if (true_ != NULL)
        true_->Output(out, CYAssign::Precedence_, CYNoFlags);
    out << ' ' << ':' << ' ';
    false_->Output(out, CYAssign::Precedence_, CYRight(flags));
}

void CYContinue::Output(CYOutput &out, CYFlags flags) const {
    out << "continue";
    if (label_ != NULL)
        out << ' ' << *label_;
    out << ';';
}

void CYClause::Output(CYOutput &out) const {
    out << '\t';
    if (case_ != NULL)
        out << "case" << ' ' << *case_;
    else
        out << "default";
    out << ':' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << next_;
}

void CYDebugger::Output(CYOutput &out, CYFlags flags) const {
    out << "debugger" << ';';
}

void CYDeclaration::Output(CYOutput &out, CYFlags flags) const {
    out << *identifier_;
    //out.out_ << ':' << identifier_->usage_ << '#' << identifier_->offset_;
    if (initialiser_ != NULL) {
        out << ' ' << '=' << ' ';
        initialiser_->Output(out, CYAssign::Precedence_, CYRight(flags));
    }
}

void CYDeclarations::Output(CYOutput &out) const {
    Output(out, CYNoFlags);
}

void CYDeclarations::Output(CYOutput &out, CYFlags flags) const {
    const CYDeclarations *declaration(this);
    bool first(true);

    for (;;) {
        CYDeclarations *next(declaration->next_);

        CYFlags jacks(first ? CYLeft(flags) : next == NULL ? CYRight(flags) : CYCenter(flags));
        first = false;
        declaration->declaration_->Output(out, jacks);

        if (next == NULL)
            break;

        out << ',' << ' ';
        declaration = next;
    }
}

void CYDirectMember::Output(CYOutput &out, CYFlags flags) const {
    object_->Output(out, Precedence(), CYLeft(flags) | CYNoInteger);
    if (const char *word = property_->Word())
        out << '.' << word;
    else
        out << '[' << *property_ << ']';
}

void CYDoWhile::Output(CYOutput &out, CYFlags flags) const {
    out << "do";

    unsigned line(out.position_.line);
    unsigned indent(out.indent_);
    code_->Single(out, CYCenter(flags), CYCompactLong);

    if (out.position_.line != line && out.recent_ == indent)
        out << ' ';
    else
        out << '\n' << '\t';

    out << "while" << ' ' << '(' << *test_ << ')';
}

void CYElementSpread::Output(CYOutput &out) const {
    out << "..." << value_;
}

void CYElementValue::Output(CYOutput &out) const {
    if (value_ != NULL)
        value_->Output(out, CYAssign::Precedence_, CYNoFlags);
    if (next_ != NULL || value_ == NULL) {
        out << ',';
        if (next_ != NULL && !next_->Elision())
            out << ' ';
    }
    if (next_ != NULL)
        next_->Output(out);
}

void CYEmpty::Output(CYOutput &out, CYFlags flags) const {
    out.Terminate();
}

void CYEval::Output(CYOutput &out, CYFlags flags) const {
    _assert(false);
}

void CYExpress::Output(CYOutput &out, CYFlags flags) const {
    expression_->Output(out, flags | CYNoBFC);
    out << ';';
}

void CYExpression::Output(CYOutput &out) const {
    Output(out, CYNoFlags);
}

void CYExpression::Output(CYOutput &out, int precedence, CYFlags flags) const {
    if (precedence < Precedence() || (flags & CYNoRightHand) != 0 && RightHand())
        out << '(' << *this << ')';
    else
        Output(out, flags);
}

void CYExternal::Output(CYOutput &out, CYFlags flags) const {
    out << "extern" << abi_ << typed_ << ';';
}

void CYFatArrow::Output(CYOutput &out, CYFlags flags) const {
    out << '(' << parameters_ << ')' << ' ' << "=>" << ' ' << '{' << code_ << '}';
}

void CYFinally::Output(CYOutput &out) const {
    out << ' ' << "finally" << ' ';
    out << '{' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << '\t' << '}';
}

void CYFor::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << '(';
    if (initialiser_ != NULL)
        initialiser_->Output(out, CYNoIn);
    out.Terminate();
    if (test_ != NULL)
        out << ' ';
    out << test_;
    out.Terminate();
    if (increment_ != NULL)
        out << ' ';
    out << increment_;
    out << ')';
    code_->Single(out, CYRight(flags), CYCompactShort);
}

void CYForLexical::Output(CYOutput &out, CYFlags flags) const {
    out << (constant_ ? "const" : "let") << ' ';
    declaration_->Output(out, CYRight(flags));
}

void CYForIn::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << '(';
    initialiser_->Output(out, CYNoIn | CYNoRightHand);
    out << ' ' << "in" << ' ' << *set_ << ')';
    code_->Single(out, CYRight(flags), CYCompactShort);
}

void CYForInComprehension::Output(CYOutput &out) const {
    out << "for" << ' ' << '(';
    declaration_->Output(out, CYNoIn | CYNoRightHand);
    out << ' ' << "in" << ' ' << *set_ << ')';
}

void CYForOf::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << '(';
    initialiser_->Output(out, CYNoRightHand);
    out << ' ' << "of" << ' ' << *set_ << ')';
    code_->Single(out, CYRight(flags), CYCompactShort);
}

void CYForOfComprehension::Output(CYOutput &out) const {
    out << "for" << ' ' << '(';
    declaration_->Output(out, CYNoRightHand);
    out << ' ' << "of" << ' ' << *set_ << ')' << next_;
}

void CYForVariable::Output(CYOutput &out, CYFlags flags) const {
    out << "var" << ' ';
    declaration_->Output(out, CYRight(flags));
}

void CYFunction::Output(CYOutput &out) const {
    out << '(' << parameters_ << ')' << ' ';
    out << '{' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << '\t' << '}';
}

void CYFunctionExpression::Output(CYOutput &out, CYFlags flags) const {
    // XXX: one could imagine using + here to save a byte
    bool protect((flags & CYNoFunction) != 0);
    if (protect)
        out << '(';
    out << "function";
    if (name_ != NULL)
        out << ' ' << *name_;
    CYFunction::Output(out);
    if (protect)
        out << ')';
}

void CYFunctionStatement::Output(CYOutput &out, CYFlags flags) const {
    out << "function" << ' ' << *name_;
    CYFunction::Output(out);
}

void CYFunctionParameter::Output(CYOutput &out) const {
    initialiser_->Output(out, CYNoFlags);
    if (next_ != NULL)
        out << ',' << ' ' << *next_;
}

const char *CYIdentifier::Word() const {
    return next_ == NULL || next_ == this ? CYWord::Word() : next_->Word();
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
    else
        jacks |= protect ? CYNoFlags : CYCenter(flags);

    unsigned line(out.position_.line);
    unsigned indent(out.indent_);
    true_->Single(out, jacks, CYCompactShort);

    if (false_ != NULL) {
        if (out.position_.line != line && out.recent_ == indent)
            out << ' ';
        else
            out << '\n' << '\t';

        out << "else";
        false_->Single(out, right, CYCompactLong);
    }

    if (protect)
        out << '}';
}

void CYIfComprehension::Output(CYOutput &out) const {
    out << "if" << ' ' << '(' << *test_ << ')' << next_;
}

void CYImport::Output(CYOutput &out, CYFlags flags) const {
    out << "@import";
}

void CYIndirect::Output(CYOutput &out, CYFlags flags) const {
    out << "*";
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYIndirectMember::Output(CYOutput &out, CYFlags flags) const {
    object_->Output(out, Precedence(), CYLeft(flags));
    if (const char *word = property_->Word())
        out << "->" << word;
    else
        out << "->" << '[' << *property_ << ']';
}

void CYInfix::Output(CYOutput &out, CYFlags flags) const {
    const char *name(Operator());
    bool protect((flags & CYNoIn) != 0 && strcmp(name, "in") == 0);
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

void CYLabel::Output(CYOutput &out, CYFlags flags) const {
    out << *name_ << ':';
    statement_->Single(out, CYRight(flags), CYCompactShort);
}

void CYParenthetical::Output(CYOutput &out, CYFlags flags) const {
    out << '(';
    expression_->Output(out, CYCompound::Precedence_, CYNoFlags);
    out << ')';
}

void CYStatement::Output(CYOutput &out) const {
    Multiple(out);
}

void CYTemplate::Output(CYOutput &out, CYFlags flags) const {
    _assert(false);
}

void CYTypeArrayOf::Output(CYOutput &out, CYIdentifier *identifier) const {
    next_->Output(out, Precedence(), identifier);
    out << '[';
    out << size_;
    out << ']';
}

void CYTypeBlockWith::Output(CYOutput &out, CYIdentifier *identifier) const {
    out << '(' << '^';
    next_->Output(out, Precedence(), identifier);
    out << ')' << '(' << parameters_ << ')';
}

void CYTypeConstant::Output(CYOutput &out, CYIdentifier *identifier) const {
    out << "const" << ' ';
    next_->Output(out, Precedence(), identifier);
}

void CYTypeFunctionWith::Output(CYOutput &out, CYIdentifier *identifier) const {
    next_->Output(out, Precedence(), identifier);
    out << '(' << parameters_ << ')';
}

void CYTypePointerTo::Output(CYOutput &out, CYIdentifier *identifier) const {
    out << '*';
    next_->Output(out, Precedence(), identifier);
}

void CYTypeVolatile::Output(CYOutput &out, CYIdentifier *identifier) const {
    out << "volatile";
    next_->Output(out, Precedence(), identifier);
}

void CYTypeModifier::Output(CYOutput &out, int precedence, CYIdentifier *identifier) const {
    if (this == NULL) {
        out << identifier;
        return;
    }

    bool protect(precedence > Precedence());

    if (protect)
        out << '(';
    Output(out, identifier);
    if (protect)
        out << ')';
}

void CYTypedIdentifier::Output(CYOutput &out) const {
    specifier_->Output(out);
    modifier_->Output(out, 0, identifier_);
}

void CYEncodedType::Output(CYOutput &out, CYFlags flags) const {
    out << "@encode(" << typed_ << ")";
}

void CYTypedParameter::Output(CYOutput &out) const {
    out << typed_;
    if (next_ != NULL)
        out << ',' << ' ' << next_;
}

void CYLambda::Output(CYOutput &out, CYFlags flags) const {
    // XXX: this is seriously wrong
    out << "[](";
    out << ")->";
    out << "{";
    out << "}";
}

void CYTypeDefinition::Output(CYOutput &out, CYFlags flags) const {
    out << "typedef" << ' ' << *typed_;
}

void CYLet::Output(CYOutput &out, CYFlags flags) const {
    out << "let" << ' ';
    declarations_->Output(out, flags); // XXX: flags
    out << ';';
}

void CYModule::Output(CYOutput &out) const {
    out << part_;
    if (next_ != NULL)
        out << '.' << next_;
}

namespace cy {
namespace Syntax {

void New::Output(CYOutput &out, CYFlags flags) const {
    out << "new" << ' ';
    CYFlags jacks(CYNoCall | CYCenter(flags));
    constructor_->Output(out, Precedence(), jacks);
    if (arguments_ != NULL)
        out << '(' << *arguments_ << ')';
}

} }

void CYNull::Output(CYOutput &out, CYFlags flags) const {
    out << "null";
}

void CYNumber::Output(CYOutput &out, CYFlags flags) const {
    std::ostringstream str;
    CYNumerify(str, Value());
    std::string value(str.str());
    out << value.c_str();
    // XXX: this should probably also handle hex conversions and exponents
    if ((flags & CYNoInteger) != 0 && value.find('.') == std::string::npos)
        out << '.';
}

void CYNumber::PropertyName(CYOutput &out) const {
    Output(out, CYNoFlags);
}

void CYObject::Output(CYOutput &out, CYFlags flags) const {
    bool protect((flags & CYNoBrace) != 0);
    if (protect)
        out << '(';
    out << '{' << '\n';
    ++out.indent_;
    out << properties_;
    --out.indent_;
    out << '\t' << '}';
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

void CYScript::Output(CYOutput &out) const {
    out << code_;
}

void CYProperty::Output(CYOutput &out) const {
    if (next_ != NULL || out.pretty_)
        out << ',';
    out << '\n' <<  next_;
}

void CYPropertyGetter::Output(CYOutput &out) const {
    out << "get" << ' ';
    name_->PropertyName(out);
    CYFunction::Output(out);
    CYProperty::Output(out);
}

void CYPropertyMethod::Output(CYOutput &out) const {
    name_->PropertyName(out);
    CYFunction::Output(out);
    CYProperty::Output(out);
}

void CYPropertySetter::Output(CYOutput &out) const {
    out << "set" << ' ';
    name_->PropertyName(out);
    CYFunction::Output(out);
    CYProperty::Output(out);
}

void CYPropertyValue::Output(CYOutput &out) const {
    out << '\t';
    name_->PropertyName(out);
    out << ':' << ' ';
    value_->Output(out, CYAssign::Precedence_, CYNoFlags);
    CYProperty::Output(out);
}

void CYRegEx::Output(CYOutput &out, CYFlags flags) const {
    out << Value();
}

void CYReturn::Output(CYOutput &out, CYFlags flags) const {
    out << "return";
    if (value_ != NULL)
        out << ' ' << *value_;
    out << ';';
}

void CYRubyBlock::Output(CYOutput &out, CYFlags flags) const {
    call_->Output(out, CYLeft(flags));
    out << ' ';
    proc_->Output(out, CYRight(flags));
}

void CYRubyProc::Output(CYOutput &out, CYFlags flags) const {
    out << '{' << ' ' << '|' << parameters_ << '|' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << '\t' << '}';
}

void CYStatement::Multiple(CYOutput &out, CYFlags flags) const {
    bool first(true);
    CYForEach (next, this) {
        bool last(next->next_ == NULL);
        CYFlags jacks(first ? last ? flags : CYLeft(flags) : last ? CYRight(flags) : CYCenter(flags));
        first = false;
        out << '\t';
        next->Output(out, jacks);
        out << '\n';
    }
}

void CYStatement::Single(CYOutput &out, CYFlags flags, CYCompactType request) const {
    if (this == NULL)
        return out.Terminate();

    _assert(next_ == NULL);

    CYCompactType compact(Compact());

    if (compact >= request)
        out << ' ';
    else {
        out << '\n';
        ++out.indent_;
        out << '\t';
    }

    Output(out, flags);

    if (compact < request)
        --out.indent_;
}

void CYString::Output(CYOutput &out, CYFlags flags) const {
    std::ostringstream str;
    CYStringify(str, value_, size_);
    out << str.str().c_str();
}

void CYString::PropertyName(CYOutput &out) const {
    if (const char *word = Word())
        out << word;
    else
        out << *this;
}

static const char *Reserved_[] = {
    "false", "null", "true",

    "break", "case", "catch", "continue", "default",
    "delete", "do", "else", "finally", "for", "function",
    "if", "in", "instanceof", "new", "return", "switch",
    "this", "throw", "try", "typeof", "var", "void",
    "while", "with",

    "debugger", "const",

    "class", "enum", "export", "extends", "import", "super",

    "abstract", "boolean", "byte", "char", "double", "final",
    "float", "goto", "int", "long", "native", "short",
    "synchronized", "throws", "transient", "volatile",

    "let", "yield",

    NULL
};

const char *CYString::Word() const {
    if (size_ == 0 || !WordStartRange_[value_[0]])
        return NULL;
    for (size_t i(1); i != size_; ++i)
        if (!WordEndRange_[value_[i]])
            return NULL;
    const char *value(Value());
    for (const char **reserved(Reserved_); *reserved != NULL; ++reserved)
        if (strcmp(*reserved, value) == 0)
            return NULL;
    return value;
}

void CYSuperAccess::Output(CYOutput &out, CYFlags flags) const {
    out << "super";
    if (const char *word = property_->Word())
        out << '.' << word;
    else
        out << '[' << *property_ << ']';
}

void CYSuperCall::Output(CYOutput &out, CYFlags flags) const {
    out << "super" << '(' << arguments_ << ')';
}

void CYSwitch::Output(CYOutput &out, CYFlags flags) const {
    out << "switch" << ' ' << '(' << *value_ << ')' << ' ' << '{' << '\n';
    ++out.indent_;
    out << clauses_;
    --out.indent_;
    out << '\t' << '}';
}

void CYThis::Output(CYOutput &out, CYFlags flags) const {
    out << "this";
}

namespace cy {
namespace Syntax {

void Throw::Output(CYOutput &out, CYFlags flags) const {
    out << "throw";
    if (value_ != NULL)
        out << ' ' << *value_;
    out << ';';
}

void Try::Output(CYOutput &out, CYFlags flags) const {
    out << "try" << ' ';
    out << '{' << '\n';
    ++out.indent_;
    out << code_;
    --out.indent_;
    out << '\t' << '}';
    out << catch_ << finally_;
}

} }

void CYTypeError::Output(CYOutput &out) const {
    out << "@error";
}

void CYTypeLong::Output(CYOutput &out) const {
    out << "long" << specifier_;
}

void CYTypeShort::Output(CYOutput &out) const {
    out << "short" << specifier_;
}

void CYTypeSigned::Output(CYOutput &out) const {
    out << "signed" << specifier_;
}

void CYTypeUnsigned::Output(CYOutput &out) const {
    out << "unsigned" << specifier_;
}

void CYTypeVariable::Output(CYOutput &out) const {
    out << *name_;
}

void CYTypeVoid::Output(CYOutput &out) const {
    out << "void";
}

void CYVar::Output(CYOutput &out, CYFlags flags) const {
    out << "var" << ' ';
    declarations_->Output(out, flags); // XXX: flags
    out << ';';
}

void CYVariable::Output(CYOutput &out, CYFlags flags) const {
    out << *name_;
}

void CYWhile::Output(CYOutput &out, CYFlags flags) const {
    out << "while" << ' ' << '(' << *test_ << ')';
    code_->Single(out, CYRight(flags), CYCompactShort);
}

void CYWith::Output(CYOutput &out, CYFlags flags) const {
    out << "with" << ' ' << '(' << *scope_ << ')';
    code_->Single(out, CYRight(flags), CYCompactShort);
}

void CYWord::Output(CYOutput &out) const {
    out << Word();
    if (out.options_.verbose_) {
        out('@');
        char number[32];
        sprintf(number, "%p", this);
        out(number);
    }
}

void CYWord::PropertyName(CYOutput &out) const {
    Output(out);
}

const char *CYWord::Word() const {
    return word_;
}
