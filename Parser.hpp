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

#ifndef CYCRIPT_PARSER_HPP
#define CYCRIPT_PARSER_HPP

#include <iostream>

#include <string>
#include <vector>
#include <map>
#include <set>

#include <cstdio>
#include <cstdlib>

#include "List.hpp"
#include "Pooling.hpp"
#include "Options.hpp"

struct CYContext;

struct CYThing {
    virtual ~CYThing() {
    }

    virtual void Output(struct CYOutput &out) const = 0;
};

struct CYOutput {
    std::ostream &out_;
    CYOptions &options_;
    bool pretty_;
    unsigned indent_;
    bool right_;

    enum {
        NoMode,
        NoLetter,
        NoPlus,
        NoHyphen,
        Terminated
    } mode_;

    CYOutput(std::ostream &out, CYOptions &options) :
        out_(out),
        options_(options),
        pretty_(false),
        indent_(0),
        right_(false),
        mode_(NoMode)
    {
    }

    void Check(char value);
    void Terminate();

    CYOutput &operator <<(char rhs);
    CYOutput &operator <<(const char *rhs);

    _finline CYOutput &operator <<(const CYThing *rhs) {
        if (rhs != NULL)
            rhs->Output(*this);
        return *this;
    }

    _finline CYOutput &operator <<(const CYThing &rhs) {
        rhs.Output(*this);
        return *this;
    }
};

struct CYPropertyName {
    virtual void PropertyName(CYOutput &out) const = 0;

    virtual ~CYPropertyName() {
    }
};

struct CYExpression;
struct CYAssignment;

enum CYNeeded {
    CYNever     = -1,
    CYSometimes =  0,
    CYAlways    =  1,
};

enum CYFlags {
    CYNoFlags =      0,
    CYNoBrace =      (1 << 0),
    CYNoFunction =   (1 << 1),
    CYNoIn =         (1 << 2),
    CYNoCall =       (1 << 3),
    CYNoRightHand =  (1 << 4),
    CYNoDangle =     (1 << 5),
    CYNoInteger =    (1 << 6),
    CYNoBF =         (CYNoBrace | CYNoFunction),
};

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

struct CYStatement :
    CYNext<CYStatement>
{
    virtual ~CYStatement() {
    }

    void Single(CYOutput &out, CYFlags flags) const;
    void Multiple(CYOutput &out, CYFlags flags = CYNoFlags) const;

    virtual CYStatement *Replace(CYContext &context) = 0;

  private:
    virtual void Output(CYOutput &out, CYFlags flags) const = 0;
};

struct CYStatements {
    CYStatement *first_;
    CYStatement *last_;

    CYStatements() :
        first_(NULL),
        last_(NULL)
    {
    }

    operator CYStatement *() const {
        return first_;
    }

    CYStatements &operator ->*(CYStatement *next) {
        if (next != NULL)
            if (first_ == NULL) {
                first_ = next;
                last_ = next;
            } else for (;; last_ = last_->next_)
                if (last_->next_ == NULL) {
                    last_->next_ = next;
                    last_ = next;
                    break;
                }
        return *this;
    }
};

struct CYClassName {
    virtual ~CYClassName() {
    }

    virtual CYExpression *ClassName(CYContext &context, bool object) = 0;
    virtual void ClassName(CYOutput &out, bool object) const = 0;
};

struct CYWord :
    CYThing,
    CYPropertyName,
    CYClassName
{
    const char *word_;

    CYWord(const char *word) :
        word_(word)
    {
    }

    void Set(const char *value) {
        word_ = value;
    }

    virtual const char *Word() const;
    virtual void Output(CYOutput &out) const;

    virtual CYExpression *ClassName(CYContext &context, bool object);
    virtual void ClassName(CYOutput &out, bool object) const;
    virtual void PropertyName(CYOutput &out) const;
};

_finline std::ostream &operator <<(std::ostream &lhs, const CYWord &rhs) {
    lhs << &rhs << '=';
    return lhs << rhs.Word();
}

struct CYIdentifier :
    CYNext<CYIdentifier>,
    CYWord
{
    CYIdentifier *replace_;
    size_t offset_;
    size_t usage_;

    CYIdentifier(const char *word) :
        CYWord(word),
        replace_(NULL),
        offset_(0),
        usage_(0)
    {
    }

    virtual const char *Word() const;
    CYIdentifier *Replace(CYContext &context);
};

struct CYComment :
    CYStatement
{
    const char *value_;

    CYComment(const char *value) :
        value_(value)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYLabel :
    CYStatement
{
    CYIdentifier *name_;
    CYStatement *statement_;

    CYLabel(CYIdentifier *name, CYStatement *statement) :
        name_(name),
        statement_(statement)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYCStringLess :
    std::binary_function<const char *, const char *, bool>
{
    _finline bool operator ()(const char *lhs, const char *rhs) const {
        return strcmp(lhs, rhs) < 0;
    }
};

struct CYIdentifierValueLess :
    std::binary_function<CYIdentifier *, CYIdentifier *, bool>
{
    _finline bool operator ()(CYIdentifier *lhs, CYIdentifier *rhs) const {
        return CYCStringLess()(lhs->Word(), rhs->Word());
    }
};

enum CYIdentifierFlags {
    CYIdentifierArgument,
    CYIdentifierVariable,
    CYIdentifierOther,
    CYIdentifierMagic,
    CYIdentifierCatch,
};

typedef std::set<const char *, CYCStringLess> CYCStringSet;
typedef std::set<CYIdentifier *, CYIdentifierValueLess> CYIdentifierValueSet;
typedef std::map<CYIdentifier *, CYIdentifierFlags> CYIdentifierAddressFlagsMap;

struct CYIdentifierUsage {
    CYIdentifier *identifier_;
    size_t usage_;
};

typedef std::vector<CYIdentifierUsage> CYIdentifierUsageVector;

struct CYScope {
    bool transparent_;

    CYContext &context_;
    CYStatement *&statements_;

    CYScope *parent_;

    CYIdentifierAddressFlagsMap internal_;
    CYIdentifierValueSet identifiers_;

    CYScope(bool transparent, CYContext &context, CYStatement *&statements);
    virtual ~CYScope();

    void Close();

    void Declare(CYContext &context, CYIdentifier *identifier, CYIdentifierFlags flags);
    virtual CYIdentifier *Lookup(CYContext &context, CYIdentifier *identifier);
    void Merge(CYContext &context, CYIdentifier *identifier);
    void Scope(CYContext &context, CYStatement *&statements);
};

struct CYProgram :
    CYThing
{
    CYStatement *statements_;

    CYProgram(CYStatement *statements) :
        statements_(statements)
    {
    }

    virtual void Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYNonLocal;
struct CYThisScope;

struct CYContext {
    CYOptions &options_;

    CYScope *scope_;
    CYThisScope *this_;

    CYIdentifierUsageVector rename_;

    CYNonLocal *nonlocal_;
    CYNonLocal *nextlocal_;
    unsigned unique_;

    CYContext(CYOptions &options) :
        options_(options),
        scope_(NULL),
        this_(NULL),
        nonlocal_(NULL),
        nextlocal_(NULL),
        unique_(0)
    {
    }

    virtual ~CYContext() {
    }

    template <typename Type_>
    void ReplaceAll(Type_ *&values) {
        Type_ **last(&values);
        CYForEach (next, values) {
            Replace(*last = next);
            if (*last != NULL)
                last = &(*last)->next_;
        }
    }

    template <typename Type_>
    void Replace(Type_ *&value) {
        for (;;) if (value == NULL)
            break;
        else {
            Type_ *replace(value->Replace(*this));
            if (replace != value)
                value = replace;
            else break;
        }
    }

    void NonLocal(CYStatement *&statements);
    CYIdentifier *Unique();
};

struct CYNonLocal {
    CYIdentifier *identifier_;

    CYNonLocal() :
        identifier_(NULL)
    {
    }

    CYIdentifier *Target(CYContext &context) {
        if (identifier_ == NULL)
            identifier_ = context.Unique();
        return identifier_;
    }
};

struct CYThisScope :
    CYNext<CYThisScope>
{
    CYIdentifier *identifier_;

    CYThisScope() :
        identifier_(NULL)
    {
    }

    CYIdentifier *Identifier(CYContext &context) {
        if (next_ != NULL)
            return next_->Identifier(context);
        if (identifier_ == NULL)
            identifier_ = context.Unique();
        return identifier_;
    }
};

struct CYBlock :
    CYStatement,
    CYThing
{
    CYStatement *statements_;

    CYBlock(CYStatement *statements) :
        statements_(statements)
    {
    }

    operator CYStatement *() const {
        return statements_;
    }

    void AddPrev(CYStatement *statement) {
        CYSetLast(statement) = statements_;
        statements_ = statement;
    }

    virtual CYStatement *Replace(CYContext &context);

    virtual void Output(CYOutput &out) const;
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYForInitialiser {
    virtual ~CYForInitialiser() {
    }

    virtual CYExpression *Replace(CYContext &context) = 0;
    virtual void Output(CYOutput &out, CYFlags flags) const = 0;
};

struct CYForInInitialiser {
    virtual ~CYForInInitialiser() {
    }

    virtual void ForIn(CYOutput &out, CYFlags flags) const = 0;
    virtual CYStatement *ForEachIn(CYContext &out, CYExpression *value) = 0;

    virtual CYExpression *Replace(CYContext &context) = 0;
    virtual CYAssignment *Assignment(CYContext &context) = 0;

    virtual void Output(CYOutput &out, CYFlags flags) const = 0;
};

struct CYNumber;
struct CYString;

struct CYExpression :
    CYForInitialiser,
    CYForInInitialiser,
    CYClassName,
    CYThing
{
    virtual int Precedence() const = 0;

    virtual bool RightHand() const {
        return true;
    }

    virtual void ForIn(CYOutput &out, CYFlags flags) const;
    virtual CYStatement *ForEachIn(CYContext &out, CYExpression *value);

    virtual CYExpression *AddArgument(CYContext &context, CYExpression *value);

    virtual void Output(CYOutput &out) const;
    virtual void Output(CYOutput &out, CYFlags flags) const = 0;
    void Output(CYOutput &out, int precedence, CYFlags flags) const;

    virtual CYExpression *ClassName(CYContext &context, bool object);
    virtual void ClassName(CYOutput &out, bool object) const;

    virtual CYExpression *Replace(CYContext &context) = 0;
    virtual CYAssignment *Assignment(CYContext &context);

    virtual CYExpression *Primitive(CYContext &context) {
        return NULL;
    }

    virtual CYNumber *Number(CYContext &context) {
        return NULL;
    }

    virtual CYString *String(CYContext &context) {
        return NULL;
    }

    virtual const char *Word() const {
        return NULL;
    }
};

#define CYAlphabetic(value) \
    virtual bool Alphabetic() const { \
        return value; \
    }

#define CYPrecedence(value) \
    static const int Precedence_ = value; \
    virtual int Precedence() const { \
        return Precedence_; \
    }

#define CYRightHand(value) \
    virtual bool RightHand() const { \
        return value; \
    }

struct CYCompound :
    CYExpression
{
    CYExpression *expression_;
    CYExpression *next_;

    CYCompound(CYExpression *expression, CYExpression *next = NULL) :
        expression_(expression),
        next_(next)
    {
        if (expression_ == NULL)
            throw;
        _assert(expression_ != NULL);
    }

    CYPrecedence(17)

    virtual CYExpression *Replace(CYContext &context);
    void Output(CYOutput &out, CYFlags flags) const;

    virtual CYExpression *Primitive(CYContext &context);
};

struct CYDeclaration;

struct CYFunctionParameter :
    CYNext<CYFunctionParameter>,
    CYThing
{
    CYForInInitialiser *initialiser_;

    CYFunctionParameter(CYForInInitialiser *initialiser, CYFunctionParameter *next = NULL) :
        CYNext<CYFunctionParameter>(next),
        initialiser_(initialiser)
    {
    }

    void Replace(CYContext &context, CYBlock &code);
    void Output(CYOutput &out) const;
};

struct CYComprehension :
    CYNext<CYComprehension>,
    CYThing
{
    CYComprehension(CYComprehension *next = NULL) :
        CYNext<CYComprehension>(next)
    {
    }

    CYComprehension *Modify(CYComprehension *next) {
        next_ = next;
        return this;
    }

    virtual const char *Name() const = 0;

    virtual CYFunctionParameter *Parameter(CYContext &context) const = 0;
    CYFunctionParameter *Parameters(CYContext &context) const;
    virtual CYStatement *Replace(CYContext &context, CYStatement *statement) const;
    virtual void Output(CYOutput &out) const = 0;
};

struct CYForInComprehension :
    CYComprehension
{
    CYIdentifier *name_;
    CYExpression *set_;

    CYForInComprehension(CYIdentifier *name, CYExpression *set, CYComprehension *next = NULL) :
        CYComprehension(next),
        name_(name),
        set_(set)
    {
    }

    virtual const char *Name() const {
        return name_->Word();
    }

    virtual CYFunctionParameter *Parameter(CYContext &context) const;
    virtual CYStatement *Replace(CYContext &context, CYStatement *statement) const;
    virtual void Output(CYOutput &out) const;
};

struct CYForOfComprehension :
    CYComprehension
{
    CYIdentifier *name_;
    CYExpression *set_;

    CYForOfComprehension(CYIdentifier *name, CYExpression *set, CYComprehension *next = NULL) :
        CYComprehension(next),
        name_(name),
        set_(set)
    {
    }

    virtual const char *Name() const {
        return name_->Word();
    }

    virtual CYFunctionParameter *Parameter(CYContext &context) const;
    virtual CYStatement *Replace(CYContext &context, CYStatement *statement) const;
    virtual void Output(CYOutput &out) const;
};

struct CYIfComprehension :
    CYComprehension
{
    CYExpression *test_;

    CYIfComprehension(CYExpression *test, CYComprehension *next = NULL) :
        CYComprehension(next),
        test_(test)
    {
    }

    virtual const char *Name() const {
        return NULL;
    }

    virtual CYFunctionParameter *Parameter(CYContext &context) const;
    virtual CYStatement *Replace(CYContext &context, CYStatement *statement) const;
    virtual void Output(CYOutput &out) const;
};

struct CYArrayComprehension :
    CYExpression
{
    CYExpression *expression_;
    CYComprehension *comprehensions_;

    CYArrayComprehension(CYExpression *expression, CYComprehension *comprehensions) :
        expression_(expression),
        comprehensions_(comprehensions)
    {
    }

    CYPrecedence(0)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYLiteral :
    CYExpression
{
    CYPrecedence(0)
    CYRightHand(false)

    virtual CYExpression *Primitive(CYContext &context) {
        return this;
    }
};

struct CYTrivial :
    CYLiteral
{
    virtual CYExpression *Replace(CYContext &context);
};

struct CYMagic :
    CYExpression
{
    CYPrecedence(0)
    CYRightHand(false)
};

struct CYRange {
    uint64_t lo_;
    uint64_t hi_;

    CYRange(uint64_t lo, uint64_t hi) :
        lo_(lo), hi_(hi)
    {
    }

    bool operator [](uint8_t value) const {
        return !(value >> 7) && (value >> 6 ? hi_ : lo_) >> (value & 0x3f) & 0x1;
    }

    void operator()(uint8_t value) {
        if (value >> 7)
            return;
        (value >> 6 ? hi_ : lo_) |= uint64_t(0x1) << (value & 0x3f);
    }
};

extern CYRange DigitRange_;
extern CYRange WordStartRange_;
extern CYRange WordEndRange_;

struct CYString :
    CYTrivial,
    CYPropertyName
{
    const char *value_;
    size_t size_;

    CYString() :
        value_(NULL),
        size_(0)
    {
    }

    CYString(const char *value) :
        value_(value),
        size_(strlen(value))
    {
    }

    CYString(const char *value, size_t size) :
        value_(value),
        size_(size)
    {
    }

    CYString(const CYWord *word) :
        value_(word->Word()),
        size_(strlen(value_))
    {
    }

    const char *Value() const {
        return value_;
    }

    virtual const char *Word() const;

    virtual CYNumber *Number(CYContext &context);
    virtual CYString *String(CYContext &context);

    CYString *Concat(CYContext &out, CYString *rhs) const;
    virtual void Output(CYOutput &out, CYFlags flags) const;
    virtual void PropertyName(CYOutput &out) const;
};

struct CYNumber :
    CYTrivial,
    CYPropertyName
{
    double value_;

    CYNumber(double value) :
        value_(value)
    {
    }

    double Value() const {
        return value_;
    }

    virtual CYNumber *Number(CYContext &context);
    virtual CYString *String(CYContext &context);

    virtual void Output(CYOutput &out, CYFlags flags) const;
    virtual void PropertyName(CYOutput &out) const;
};

struct CYRegEx :
    CYTrivial
{
    const char *value_;

    CYRegEx(const char *value) :
        value_(value)
    {
    }

    const char *Value() const {
        return value_;
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYNull :
    CYWord,
    CYTrivial
{
    CYNull() :
        CYWord("null")
    {
    }

    virtual CYNumber *Number(CYContext &context);
    virtual CYString *String(CYContext &context);

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYThis :
    CYWord,
    CYMagic
{
    CYThis() :
        CYWord("this")
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYBoolean :
    CYTrivial
{
    virtual bool Value() const = 0;
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFalse :
    CYWord,
    CYBoolean
{
    CYFalse() :
        CYWord("false")
    {
    }

    virtual bool Value() const {
        return false;
    }

    virtual CYNumber *Number(CYContext &context);
    virtual CYString *String(CYContext &context);
};

struct CYTrue :
    CYWord,
    CYBoolean
{
    CYTrue() :
        CYWord("true")
    {
    }

    virtual bool Value() const {
        return true;
    }

    virtual CYNumber *Number(CYContext &context);
    virtual CYString *String(CYContext &context);
};

struct CYVariable :
    CYExpression
{
    CYIdentifier *name_;

    CYVariable(CYIdentifier *name) :
        name_(name)
    {
    }

    CYVariable(const char *name) :
        name_(new($pool) CYIdentifier(name))
    {
    }

    CYPrecedence(0)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYPrefix :
    CYExpression
{
    CYExpression *rhs_;

    CYPrefix(CYExpression *rhs) :
        rhs_(rhs)
    {
    }

    virtual bool Alphabetic() const = 0;
    virtual const char *Operator() const = 0;

    CYPrecedence(4)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYInfix :
    CYExpression
{
    CYExpression *lhs_;
    CYExpression *rhs_;

    CYInfix(CYExpression *lhs, CYExpression *rhs) :
        lhs_(lhs),
        rhs_(rhs)
    {
    }

    void SetLeft(CYExpression *lhs) {
        lhs_ = lhs;
    }

    virtual bool Alphabetic() const = 0;
    virtual const char *Operator() const = 0;

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYPostfix :
    CYExpression
{
    CYExpression *lhs_;

    CYPostfix(CYExpression *lhs) :
        lhs_(lhs)
    {
    }

    virtual const char *Operator() const = 0;

    CYPrecedence(3)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYAssignment :
    CYExpression
{
    CYExpression *lhs_;
    CYExpression *rhs_;

    CYAssignment(CYExpression *lhs, CYExpression *rhs) :
        lhs_(lhs),
        rhs_(rhs)
    {
    }

    void SetLeft(CYExpression *lhs) {
        lhs_ = lhs;
    }

    virtual const char *Operator() const = 0;

    CYPrecedence(16)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYArgument :
    CYNext<CYArgument>,
    CYThing
{
    CYWord *name_;
    CYExpression *value_;

    CYArgument(CYExpression *value, CYArgument *next = NULL) :
        CYNext<CYArgument>(next),
        name_(NULL),
        value_(value)
    {
    }

    CYArgument(CYWord *name, CYExpression *value, CYArgument *next = NULL) :
        CYNext<CYArgument>(next),
        name_(name),
        value_(value)
    {
    }

    CYArgument *Replace(CYContext &context);
    void Output(CYOutput &out) const;
};

struct CYBlank :
    public CYWord
{
    CYBlank() :
        CYWord("")
    {
    }
};

struct CYClause :
    CYThing,
    CYNext<CYClause>
{
    CYExpression *case_;
    CYStatement *statements_;

    CYClause(CYExpression *_case, CYStatement *statements) :
        case_(_case),
        statements_(statements)
    {
    }

    void Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYElement :
    CYNext<CYElement>,
    CYThing
{
    CYExpression *value_;

    CYElement(CYExpression *value, CYElement *next) :
        CYNext<CYElement>(next),
        value_(value)
    {
    }

    void Replace(CYContext &context);
    void Output(CYOutput &out) const;
};

struct CYArray :
    CYLiteral
{
    CYElement *elements_;

    CYArray(CYElement *elements = NULL) :
        elements_(elements)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYProperty :
    CYNext<CYProperty>,
    CYThing
{
    CYPropertyName *name_;
    CYExpression *value_;

    CYProperty(CYPropertyName *name, CYExpression *value, CYProperty *next = NULL) :
        CYNext<CYProperty>(next),
        name_(name),
        value_(value)
    {
    }

    void Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYDeclaration :
    CYForInInitialiser
{
    CYIdentifier *identifier_;
    CYExpression *initialiser_;

    CYDeclaration(CYIdentifier *identifier, CYExpression *initialiser = NULL) :
        identifier_(identifier),
        initialiser_(initialiser)
    {
    }

    virtual void ForIn(CYOutput &out, CYFlags flags) const;
    virtual CYStatement *ForEachIn(CYContext &out, CYExpression *value);

    virtual CYExpression *Replace(CYContext &context);

    virtual CYAssignment *Assignment(CYContext &context);
    CYVariable *Variable(CYContext &context);

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYDeclarations :
    CYNext<CYDeclarations>,
    CYThing
{
    CYDeclaration *declaration_;

    CYDeclarations(CYDeclaration *declaration, CYDeclarations *next = NULL) :
        CYNext<CYDeclarations>(next),
        declaration_(declaration)
    {
    }

    void Replace(CYContext &context);

    CYCompound *Compound(CYContext &context);
    CYProperty *Property(CYContext &context);
    CYArgument *Argument(CYContext &context);
    CYFunctionParameter *Parameter(CYContext &context);

    virtual void Output(CYOutput &out) const;
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYForDeclarations :
    CYForInitialiser
{
    CYDeclarations *declarations_;

    CYForDeclarations(CYDeclarations *declarations) :
        declarations_(declarations)
    {
    }

    virtual CYCompound *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYVar :
    CYStatement
{
    CYDeclarations *declarations_;

    CYVar(CYDeclarations *declarations) :
        declarations_(declarations)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYLetStatement :
    CYStatement
{
    CYDeclarations *declarations_;
    CYStatement *code_;

    CYLetStatement(CYDeclarations *declarations, CYStatement *code) :
        declarations_(declarations),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFor :
    CYStatement
{
    CYForInitialiser *initialiser_;
    CYExpression *test_;
    CYExpression *increment_;
    CYStatement *code_;

    CYFor(CYForInitialiser *initialiser, CYExpression *test, CYExpression *increment, CYStatement *code) :
        initialiser_(initialiser),
        test_(test),
        increment_(increment),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYForIn :
    CYStatement
{
    CYForInInitialiser *initialiser_;
    CYExpression *set_;
    CYStatement *code_;

    CYForIn(CYForInInitialiser *initialiser, CYExpression *set, CYStatement *code) :
        initialiser_(initialiser),
        set_(set),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYForOf :
    CYStatement
{
    CYForInInitialiser *initialiser_;
    CYExpression *set_;
    CYStatement *code_;

    CYForOf(CYForInInitialiser *initialiser, CYExpression *set, CYStatement *code) :
        initialiser_(initialiser),
        set_(set),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYObject :
    CYLiteral
{
    CYProperty *properties_;

    CYObject(CYProperty *properties = NULL) :
        properties_(properties)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    void Output(CYOutput &out, CYFlags flags) const;
};

struct CYMember :
    CYExpression
{
    CYExpression *object_;
    CYExpression *property_;

    CYMember(CYExpression *object, CYExpression *property) :
        object_(object),
        property_(property)
    {
    }

    void SetLeft(CYExpression *object) {
        object_ = object;
    }
};

struct CYDirectMember :
    CYMember
{
    CYDirectMember(CYExpression *object, CYExpression *property) :
        CYMember(object, property)
    {
    }

    CYPrecedence(1)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYIndirectMember :
    CYMember
{
    CYIndirectMember(CYExpression *object, CYExpression *property) :
        CYMember(object, property)
    {
    }

    CYPrecedence(1)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

namespace cy {
namespace Syntax {

struct New :
    CYExpression
{
    CYExpression *constructor_;
    CYArgument *arguments_;

    New(CYExpression *constructor, CYArgument *arguments) :
        constructor_(constructor),
        arguments_(arguments)
    {
    }

    virtual int Precedence() const {
        return arguments_ == NULL ? 2 : 1;
    }

    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;

    virtual CYExpression *AddArgument(CYContext &context, CYExpression *value);
};

} }

struct CYCall :
    CYExpression
{
    CYExpression *function_;
    CYArgument *arguments_;

    CYCall(CYExpression *function, CYArgument *arguments = NULL) :
        function_(function),
        arguments_(arguments)
    {
    }

    CYPrecedence(1)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;

    virtual CYExpression *AddArgument(CYContext &context, CYExpression *value);
};

struct CYRubyProc;

struct CYRubyBlock :
    CYExpression
{
    CYExpression *call_;
    CYRubyProc *proc_;

    CYRubyBlock(CYExpression *call, CYRubyProc *proc) :
        call_(call),
        proc_(proc)
    {
    }

    CYPrecedence(1)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYIf :
    CYStatement
{
    CYExpression *test_;
    CYStatement *true_;
    CYStatement *false_;

    CYIf(CYExpression *test, CYStatement *_true, CYStatement *_false = NULL) :
        test_(test),
        true_(_true),
        false_(_false)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYDoWhile :
    CYStatement
{
    CYExpression *test_;
    CYStatement *code_;

    CYDoWhile(CYExpression *test, CYStatement *code) :
        test_(test),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYWhile :
    CYStatement
{
    CYExpression *test_;
    CYStatement *code_;

    CYWhile(CYExpression *test, CYStatement *code) :
        test_(test),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

// XXX: this should be split up into CYAnonymousFunction and CYNamedFunction (subclass)
struct CYFunction {
    CYIdentifier *name_;
    CYFunctionParameter *parameters_;
    CYBlock code_;

    CYNonLocal *nonlocal_;
    CYThisScope this_;

    CYFunction(CYIdentifier *name, CYFunctionParameter *parameters, CYStatement *statements) :
        name_(name),
        parameters_(parameters),
        code_(statements),
        nonlocal_(NULL)
    {
    }

    virtual ~CYFunction() {
    }

    void Inject(CYContext &context);
    virtual void Replace_(CYContext &context, bool outer);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

// XXX: this should be split up into CYAnonymousFunctionExpression and CYNamedFunctionExpression
struct CYFunctionExpression :
    CYFunction,
    CYExpression
{
    CYFunctionExpression(CYIdentifier *name, CYFunctionParameter *parameters, CYStatement *statements) :
        CYFunction(name, parameters, statements)
    {
    }

    CYPrecedence(0)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

// XXX: this should derive from CYAnonymousFunction
struct CYFatArrow :
    CYFunction,
    CYExpression
{
    CYFatArrow(CYFunctionParameter *parameters, CYStatement *statements) :
        CYFunction(NULL, parameters, statements)
    {
    }

    CYPrecedence(0)
    CYRightHand(false)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

// XXX: this should derive from CYAnonymousFunctionExpression
struct CYRubyProc :
    CYFunctionExpression
{
    CYRubyProc(CYFunctionParameter *parameters, CYStatement *statements) :
        CYFunctionExpression(NULL, parameters, statements)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

// XXX: this should derive from CYNamedFunction
struct CYFunctionStatement :
    CYFunction,
    CYStatement
{
    CYFunctionStatement(CYIdentifier *name, CYFunctionParameter *parameters, CYStatement *statements) :
        CYFunction(name, parameters, statements)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYExpress :
    CYStatement
{
    CYExpression *expression_;

    CYExpress(CYExpression *expression) :
        expression_(expression)
    {
        if (expression_ == NULL)
            throw;
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYContinue :
    CYStatement
{
    CYIdentifier *label_;

    CYContinue(CYIdentifier *label) :
        label_(label)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYBreak :
    CYStatement
{
    CYIdentifier *label_;

    CYBreak(CYIdentifier *label) :
        label_(label)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYReturn :
    CYStatement
{
    CYExpression *value_;

    CYReturn(CYExpression *value) :
        value_(value)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYEmpty :
    CYStatement
{
    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFinally :
    CYThing
{
    CYBlock code_;

    CYFinally(CYStatement *statements) :
        code_(statements)
    {
    }

    void Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeSpecifier :
    CYThing
{
    virtual CYExpression *Replace(CYContext &context) = 0;
};

struct CYTypeError :
    CYTypeSpecifier
{
    CYTypeError() {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeVoid :
    CYTypeSpecifier
{
    CYTypeVoid() {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeVariable :
    CYTypeSpecifier
{
    CYIdentifier *name_;

    CYTypeVariable(CYIdentifier *name) :
        name_(name)
    {
    }

    CYTypeVariable(const char *name) :
        name_(new($pool) CYIdentifier(name))
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeUnsigned :
    CYTypeSpecifier
{
    CYTypeSpecifier *specifier_;

    CYTypeUnsigned(CYTypeSpecifier *specifier) :
        specifier_(specifier)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeSigned :
    CYTypeSpecifier
{
    CYTypeSpecifier *specifier_;

    CYTypeSigned(CYTypeSpecifier *specifier) :
        specifier_(specifier)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeLong :
    CYTypeSpecifier
{
    CYTypeSpecifier *specifier_;

    CYTypeLong(CYTypeSpecifier *specifier) :
        specifier_(specifier)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeShort :
    CYTypeSpecifier
{
    CYTypeSpecifier *specifier_;

    CYTypeShort(CYTypeSpecifier *specifier) :
        specifier_(specifier)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYTypeModifier :
    CYNext<CYTypeModifier>
{
    CYTypeModifier(CYTypeModifier *next) :
        CYNext<CYTypeModifier>(next)
    {
    }

    virtual int Precedence() const = 0;

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type) = 0;
    CYExpression *Replace(CYContext &context, CYExpression *type);

    virtual void Output(CYOutput &out, CYIdentifier *identifier) const = 0;
    void Output(CYOutput &out, int precedence, CYIdentifier *identifier) const;
};

struct CYTypeArrayOf :
    CYTypeModifier
{
    CYExpression *size_;

    CYTypeArrayOf(CYExpression *size, CYTypeModifier *next = NULL) :
        CYTypeModifier(next),
        size_(size)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type);
    virtual void Output(CYOutput &out, CYIdentifier *identifier) const;
};

struct CYTypeConstant :
    CYTypeModifier
{
    CYTypeConstant(CYTypeModifier *next = NULL) :
        CYTypeModifier(next)
    {
    }

    CYPrecedence(0)

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type);
    virtual void Output(CYOutput &out, CYIdentifier *identifier) const;
};

struct CYTypePointerTo :
    CYTypeModifier
{
    CYTypePointerTo(CYTypeModifier *next = NULL) :
        CYTypeModifier(next)
    {
    }

    CYPrecedence(0)

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type);
    virtual void Output(CYOutput &out, CYIdentifier *identifier) const;
};

struct CYTypeVolatile :
    CYTypeModifier
{
    CYTypeVolatile(CYTypeModifier *next = NULL) :
        CYTypeModifier(next)
    {
    }

    CYPrecedence(0)

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type);
    virtual void Output(CYOutput &out, CYIdentifier *identifier) const;
};

struct CYTypedIdentifier :
    CYNext<CYTypedIdentifier>,
    CYThing
{
    CYIdentifier *identifier_;
    CYTypeSpecifier *specifier_;
    CYTypeModifier *modifier_;

    CYTypedIdentifier(CYIdentifier *identifier = NULL) :
        identifier_(identifier),
        specifier_(NULL),
        modifier_(NULL)
    {
    }

    CYTypedIdentifier(CYTypeSpecifier *specifier, CYTypeModifier *modifier = NULL) :
        identifier_(NULL),
        specifier_(specifier),
        modifier_(modifier)
    {
    }

    inline CYTypedIdentifier *Modify(CYTypeModifier *modifier) {
        CYSetLast(modifier_) = modifier;
        return this;
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct CYEncodedType :
    CYExpression
{
    CYTypedIdentifier *typed_;

    CYEncodedType(CYTypedIdentifier *typed) :
        typed_(typed)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYTypedParameter :
    CYNext<CYTypedParameter>,
    CYThing
{
    CYTypedIdentifier *typed_;

    CYTypedParameter(CYTypedIdentifier *typed, CYTypedParameter *next) :
        CYNext<CYTypedParameter>(next),
        typed_(typed)
    {
    }

    CYArgument *Argument(CYContext &context);
    CYFunctionParameter *Parameters(CYContext &context);
    CYExpression *TypeSignature(CYContext &context, CYExpression *prefix);

    virtual void Output(CYOutput &out) const;
};

struct CYLambda :
    CYExpression
{
    CYTypedIdentifier *typed_;
    CYTypedParameter *parameters_;
    CYStatement *statements_;

    CYLambda(CYTypedIdentifier *typed, CYTypedParameter *parameters, CYStatement *statements) :
        typed_(typed),
        parameters_(parameters),
        statements_(statements)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYTypeDefinition :
    CYStatement
{
    CYTypedIdentifier *typed_;

    CYTypeDefinition(CYTypedIdentifier *typed) :
        typed_(typed)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYTypeBlockWith :
    CYTypeModifier
{
    CYTypedParameter *parameters_;

    CYTypeBlockWith(CYTypedParameter *parameters, CYTypeModifier *next = NULL) :
        CYTypeModifier(next),
        parameters_(parameters)
    {
    }

    CYPrecedence(0)

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type);
    virtual void Output(CYOutput &out, CYIdentifier *identifier) const;
};

struct CYTypeFunctionWith :
    CYTypeModifier
{
    CYTypedParameter *parameters_;

    CYTypeFunctionWith(CYTypedParameter *parameters, CYTypeModifier *next = NULL) :
        CYTypeModifier(next),
        parameters_(parameters)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace_(CYContext &context, CYExpression *type);
    virtual void Output(CYOutput &out, CYIdentifier *identifier) const;
};

namespace cy {
namespace Syntax {

struct Catch :
    CYThing
{
    CYIdentifier *name_;
    CYBlock code_;

    Catch(CYIdentifier *name, CYStatement *statements) :
        name_(name),
        code_(statements)
    {
    }

    void Replace(CYContext &context);
    virtual void Output(CYOutput &out) const;
};

struct Try :
    CYStatement
{
    CYBlock code_;
    Catch *catch_;
    CYFinally *finally_;

    Try(CYStatement *statements, Catch *_catch, CYFinally *finally) :
        code_(statements),
        catch_(_catch),
        finally_(finally)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct Throw :
    CYStatement
{
    CYExpression *value_;

    Throw(CYExpression *value = NULL) :
        value_(value)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

} }

struct CYWith :
    CYStatement
{
    CYExpression *scope_;
    CYStatement *code_;

    CYWith(CYExpression *scope, CYStatement *code) :
        scope_(scope),
        code_(code)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYSwitch :
    CYStatement
{
    CYExpression *value_;
    CYClause *clauses_;

    CYSwitch(CYExpression *value, CYClause *clauses) :
        value_(value),
        clauses_(clauses)
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYDebugger :
    CYStatement
{
    CYDebugger()
    {
    }

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYCondition :
    CYExpression
{
    CYExpression *test_;
    CYExpression *true_;
    CYExpression *false_;

    CYCondition(CYExpression *test, CYExpression *_true, CYExpression *_false) :
        test_(test),
        true_(_true),
        false_(_false)
    {
    }

    CYPrecedence(15)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYAddressOf :
    CYPrefix
{
    CYAddressOf(CYExpression *rhs) :
        CYPrefix(rhs)
    {
    }

    virtual const char *Operator() const {
        return "&";
    }

    CYAlphabetic(false)

    virtual CYExpression *Replace(CYContext &context);
};

struct CYIndirect :
    CYPrefix
{
    CYIndirect(CYExpression *rhs) :
        CYPrefix(rhs)
    {
    }

    virtual const char *Operator() const {
        return "*";
    }

    CYAlphabetic(false)

    virtual CYExpression *Replace(CYContext &context);
};

#define CYReplace \
    virtual CYExpression *Replace(CYContext &context);

#define CYPostfix_(op, name, args...) \
    struct CY ## name : \
        CYPostfix \
    { args \
        CY ## name(CYExpression *lhs) : \
            CYPostfix(lhs) \
        { \
        } \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYPrefix_(alphabetic, op, name, args...) \
    struct CY ## name : \
        CYPrefix \
    { args \
        CY ## name(CYExpression *rhs) : \
            CYPrefix(rhs) \
        { \
        } \
    \
        CYAlphabetic(alphabetic) \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYInfix_(alphabetic, precedence, op, name, args...) \
    struct CY ## name : \
        CYInfix \
    { args \
        CY ## name(CYExpression *lhs, CYExpression *rhs) : \
            CYInfix(lhs, rhs) \
        { \
        } \
    \
        CYAlphabetic(alphabetic) \
        CYPrecedence(precedence) \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYAssignment_(op, name, args...) \
    struct CY ## name ## Assign : \
        CYAssignment \
    { args \
        CY ## name ## Assign(CYExpression *lhs, CYExpression *rhs) : \
            CYAssignment(lhs, rhs) \
        { \
        } \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

CYPostfix_("++", PostIncrement)
CYPostfix_("--", PostDecrement)

CYPrefix_(true, "delete", Delete)
CYPrefix_(true, "void", Void)
CYPrefix_(true, "typeof", TypeOf)
CYPrefix_(false, "++", PreIncrement)
CYPrefix_(false, "--", PreDecrement)
CYPrefix_(false, "+", Affirm)
CYPrefix_(false, "-", Negate)
CYPrefix_(false, "~", BitwiseNot)
CYPrefix_(false, "!", LogicalNot)

CYInfix_(false, 5, "*", Multiply, CYReplace)
CYInfix_(false, 5, "/", Divide)
CYInfix_(false, 5, "%", Modulus)
CYInfix_(false, 6, "+", Add, CYReplace)
CYInfix_(false, 6, "-", Subtract)
CYInfix_(false, 7, "<<", ShiftLeft)
CYInfix_(false, 7, ">>", ShiftRightSigned)
CYInfix_(false, 7, ">>>", ShiftRightUnsigned)
CYInfix_(false, 8, "<", Less)
CYInfix_(false, 8, ">", Greater)
CYInfix_(false, 8, "<=", LessOrEqual)
CYInfix_(false, 8, ">=", GreaterOrEqual)
CYInfix_(true, 8, "instanceof", InstanceOf)
CYInfix_(true, 8, "in", In)
CYInfix_(false, 9, "==", Equal)
CYInfix_(false, 9, "!=", NotEqual)
CYInfix_(false, 9, "===", Identical)
CYInfix_(false, 9, "!==", NotIdentical)
CYInfix_(false, 10, "&", BitwiseAnd)
CYInfix_(false, 11, "^", BitwiseXOr)
CYInfix_(false, 12, "|", BitwiseOr)
CYInfix_(false, 13, "&&", LogicalAnd)
CYInfix_(false, 14, "||", LogicalOr)

CYAssignment_("=", )
CYAssignment_("*=", Multiply)
CYAssignment_("/=", Divide)
CYAssignment_("%=", Modulus)
CYAssignment_("+=", Add)
CYAssignment_("-=", Subtract)
CYAssignment_("<<=", ShiftLeft)
CYAssignment_(">>=", ShiftRightSigned)
CYAssignment_(">>>=", ShiftRightUnsigned)
CYAssignment_("&=", BitwiseAnd)
CYAssignment_("^=", BitwiseXOr)
CYAssignment_("|=", BitwiseOr)

#endif/*CYCRIPT_PARSER_HPP*/
