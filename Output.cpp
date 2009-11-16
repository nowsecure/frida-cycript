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

#include "cycript.hpp"
#include "Parser.hpp"

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
    return flags & ~(CYNoDangle | CYNoInteger);
}

_finline CYFlags CYRight(CYFlags flags) {
    return flags & ~CYNoBF;
}

_finline CYFlags CYCenter(CYFlags flags) {
    return CYLeft(CYRight(flags));
}

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
    else if (rhs == '\r') {
        if (right_) {
            out_ << '\n';
            right_ = false;
        } goto done;
    } else goto work;

    right_ = true;
    mode_ = NoMode;
    goto done;

  work:
    if (mode_ == Terminated && rhs != '}') {
        right_ = true;
        out_ << ';';
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
            out_ << ' ';
        mode_ = NoPlus;
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

    right_ = true;
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
        mode_ == NoPlus && *rhs == '+' ||
        mode_ == NoHyphen && *rhs == '-' ||
        mode_ == NoLetter && WordEndRange_[*rhs]
    )
        out_ << ' ';

    if (WordEndRange_[rhs[size - 1]])
        mode_ = NoLetter;
    else
        mode_ = NoMode;

    right_ = true;
    out_ << rhs;
    return *this;
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
    out << '[' << *expression_ << ' ' << *comprehensions_ << ']';
}

void CYAssignment::Output(CYOutput &out, CYFlags flags) const {
    lhs_->Output(out, Precedence() - 1, CYLeft(flags) | CYNoRightHand);
    out << ' ' << Operator() << ' ';
    rhs_->Output(out, Precedence(), CYRight(flags));
}

void CYBlock::Output(CYOutput &out) const {
    out << '{' << '\n';
    ++out.indent_;
    if (statements_ != NULL)
        statements_->Multiple(out);
    --out.indent_;
    out << '\t' << '}';
}

void CYBlock::Output(CYOutput &out, CYFlags flags) const {
    if (statements_ == NULL)
        out.Terminate();
    else if (statements_->next_ == NULL)
        statements_->Single(out, flags);
    else
        Output(out);
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

namespace cy {
namespace Syntax {

void Catch::Output(CYOutput &out) const {
    out << ' ' << "catch" << ' ' << '(' << *name_ << ')' << ' ' << code_;
}

} }

void CYComment::Output(CYOutput &out, CYFlags flags) const {
    out << '\r';
    out.out_ << value_;
    out.right_ = true;
    out << '\r';
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
    if (statements_ != NULL)
        statements_->Multiple(out);
    out << next_;
}

const char *CYDeclaration::ForEachIn() const {
    return identifier_->Word();
}

void CYDeclaration::ForIn(CYOutput &out, CYFlags flags) const {
    out << "var";
    Output(out, CYRight(flags));
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
    code_->Single(out, CYCenter(flags));
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

void CYFinally::Output(CYOutput &out) const {
    out << ' ' << "finally" << ' ' << code_;
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
    code_->Single(out, CYRight(flags));
}

void CYForEachIn::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << "each" << ' ' << '(';
    initialiser_->ForIn(out, CYNoIn);
    out << "in" << *set_ << ')';
    code_->Single(out, CYRight(flags));
}

void CYForEachInComprehension::Output(CYOutput &out) const {
    out << "for" << ' ' << "each" << ' ' << '(' << *name_ << ' ' << "in" << ' ' << *set_ << ')' << next_;
}

void CYForIn::Output(CYOutput &out, CYFlags flags) const {
    out << "for" << ' ' << '(';
    if (initialiser_ != NULL)
        initialiser_->ForIn(out, CYNoIn);
    out << "in" << *set_ << ')';
    code_->Single(out, CYRight(flags));
}

void CYForInComprehension::Output(CYOutput &out) const {
    out << "for" << ' ' << '(' << *name_ << ' ' << "in" << ' ' << *set_ << ')';
}

void CYFunction::Output(CYOutput &out, CYFlags flags) const {
    // XXX: one could imagine using + here to save a byte
    bool protect((flags & CYNoFunction) != 0);
    if (protect)
        out << '(';
    out << "function";
    if (out.options_.verbose_)
        out.out_ << ':' << static_cast<const CYScope *>(this);
    if (name_ != NULL)
        out << ' ' << *name_;
    out << '(' << parameters_ << ')';
    out << ' ' << code_;
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

const char *CYIdentifier::Word() const {
    return replace_ == NULL || replace_ == this ? CYWord::Word() : replace_->Word();
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

    true_->Single(out, jacks);

    if (false_ != NULL) {
        out << "else";
        false_->Single(out, right);
    }

    if (protect)
        out << '}';
}

void CYIfComprehension::Output(CYOutput &out) const {
    out << "if" << ' ' << '(' << *test_ << ')' << next_;
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
    out << *name_ << ':' << ' ';
    statement_->Single(out, CYRight(flags));
}

void CYLet::Output(CYOutput &out, CYFlags flags) const {
    out << "let" << ' ' << '(' << *declarations_ << ')' << ' ' << code_;
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

void CYProgram::Output(CYOutput &out) const {
    if (statements_ != NULL)
        statements_->Multiple(out);
}

void CYProperty::Output(CYOutput &out) const {
    out << '\t';
    name_->PropertyName(out);
    out << ':' << ' ';
    value_->Output(out, CYPA, CYNoFlags);
    if (next_ != NULL)
        out << ',' << '\n' << *next_;
    else
        out << '\n';
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

void CYStatement::Single(CYOutput &out, CYFlags flags) const {
    _assert(next_ == NULL);
    out << '\n';
    ++out.indent_;
    out << '\t';
    Output(out, flags);
    out << '\n';
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

    "each",

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

void CYSwitch::Output(CYOutput &out, CYFlags flags) const {
    out << "switch" << ' ' << '(' << *value_ << ')' << ' ' << '{';
    out << clauses_;
    out << '}';
}

void CYThis::Output(CYOutput &out, CYFlags flags) const {
    CYWord::Output(out);
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
    out << "try" << ' ' << code_ << catch_ << finally_;
}

} }

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
    out << '"' << Word() << '"';
    if (object)
        out << ')';
}

void CYWord::Output(CYOutput &out) const {
    out << Word();
    if (out.options_.verbose_)
        out.out_ << '@' << this;
}

void CYWord::PropertyName(CYOutput &out) const {
    Output(out);
}

const char *CYWord::Word() const {
    return word_;
}
