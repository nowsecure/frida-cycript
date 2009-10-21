/* Cycript - Remove Execution Server and Disassembler
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

#ifndef CYPARSER_HPP
#define CYPARSER_HPP

#include <cstdlib>
#include <string>
#include <vector>

#include "location.hh"
#include "Pooling.hpp"

template <typename Type_>
struct CYNext {
    Type_ *next_;

    CYNext() :
        next_(NULL)
    {
    }

    CYNext(Type_ *next) :
        next_(next)
    {
    }

    void SetNext(Type_ *next) {
        next_ = next;
    }
};

struct CYThing {
    virtual void Output(struct CYOutput &out) const = 0;
};

struct CYOutput {
    std::ostream &out_;
    bool pretty_;
    unsigned indent_;

    enum {
        NoMode,
        NoLetter,
        NoPlus,
        NoHyphen,
        Terminated
    } mode_;

    CYOutput(std::ostream &out) :
        out_(out),
        pretty_(false),
        indent_(0),
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
};

struct CYClassName {
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

    const char *Value() const {
        return word_;
    }

    virtual void Output(CYOutput &out) const;

    virtual void ClassName(CYOutput &out, bool object) const;
    virtual void PropertyName(CYOutput &out) const;
};

_finline std::ostream &operator <<(std::ostream &lhs, const CYWord &rhs) {
    return lhs << rhs.Value();
}

struct CYIdentifier :
    CYWord
{
    CYIdentifier(const char *word) :
        CYWord(word)
    {
    }
};

struct CYLabel :
    CYNext<CYLabel>
{
    CYIdentifier *name_;

    CYLabel(CYIdentifier *name, CYLabel *next) :
        CYNext<CYLabel>(next),
        name_(name)
    {
    }
};

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
    CYNoBF =         (CYNoBrace | CYNoFunction),
};

struct CYStatement :
    CYNext<CYStatement>
{
    CYLabel *labels_;

    CYStatement() :
        labels_(NULL)
    {
    }

    void AddLabel(CYIdentifier *identifier) {
        labels_ = new CYLabel(identifier, labels_);
    }

    bool Single(CYOutput &out, CYFlags flags) const;
    void Multiple(CYOutput &out, CYFlags flags = CYNoFlags) const;

  private:
    virtual void Output(CYOutput &out, CYFlags flags) const = 0;
};

struct CYBlock :
    CYStatement
{
    CYStatement *statements_;

    CYBlock(CYStatement *statements) :
        statements_(statements)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

enum CYState {
    CYClear,
    CYRestricted,
    CYNewLine
};

class CYDriver {
  public:
    CYPool pool_;

    CYState state_;
    void *scanner_;

    const char *data_;
    size_t size_;
    FILE *file_;

    bool strict_;

    enum Condition {
        RegExStart,
        RegExRest
    };

    std::string filename_;

    struct Error {
        bool warning_;
        cy::location location_;
        std::string message_;
    };

    typedef std::vector<Error> Errors;

    CYStatement *program_;
    Errors errors_;

  private:
    void ScannerInit();
    void ScannerDestroy();

  public:
    CYDriver(const std::string &filename);
    ~CYDriver();

    void SetCondition(Condition condition);

    void Warning(const cy::location &location, const char *message);
};

struct CYForInitialiser {
    virtual void For(CYOutput &out) const = 0;
};

struct CYForInInitialiser {
    virtual void ForIn(CYOutput &out, CYFlags flags) const = 0;
    virtual const char *ForEachIn() const = 0;
    virtual void ForEachIn(CYOutput &out) const = 0;
};

struct CYExpression :
    CYNext<CYExpression>,
    CYForInitialiser,
    CYForInInitialiser,
    CYClassName,
    CYThing
{
    virtual unsigned Precedence() const = 0;

    virtual bool RightHand() const {
        return true;
    }

    virtual void For(CYOutput &out) const;
    virtual void ForIn(CYOutput &out, CYFlags flags) const;

    virtual const char *ForEachIn() const;
    virtual void ForEachIn(CYOutput &out) const;

    virtual void Output(CYOutput &out) const;
    virtual void Output(CYOutput &out, CYFlags flags) const = 0;
    void Output(CYOutput &out, unsigned precedence, CYFlags flags) const;

    virtual void ClassName(CYOutput &out, bool object) const;

    virtual const char *Word() const {
        return NULL;
    }
};

#define CYAlphabetic(value) \
    virtual bool Alphabetic() const { \
        return value; \
    }

#define CYPrecedence(value) \
    virtual unsigned Precedence() const { \
        return value; \
    }

#define CYRightHand(value) \
    virtual bool RightHand() const { \
        return value; \
    }

struct CYCompound :
    CYExpression
{
    CYExpression *expressions_;

    CYCompound(CYExpression *expressions) :
        expressions_(expressions)
    {
    }

    void AddPrev(CYExpression *expression) {
        CYExpression *last(expression);
        while (last->next_ != NULL)
            last = last->next_;
        last->SetNext(expressions_);
        expressions_ = expression;
    }

    CYPrecedence(17)

    void Output(CYOutput &out, CYFlags flags) const;
};

struct CYComprehension :
    CYNext<CYComprehension>,
    CYThing
{
    void Output(CYOutput &out) const;
    virtual const char *Name() const = 0;

    virtual void Begin_(CYOutput &out) const = 0;

    virtual void End_(CYOutput &out) const {
    }
};

struct CYForInComprehension :
    CYComprehension
{
    CYIdentifier *name_;
    CYExpression *set_;

    CYForInComprehension(CYIdentifier *name, CYExpression *set) :
        name_(name),
        set_(set)
    {
    }

    virtual const char *Name() const {
        return name_->Value();
    }

    virtual void Begin_(CYOutput &out) const;
};

struct CYForEachInComprehension :
    CYComprehension
{
    CYIdentifier *name_;
    CYExpression *set_;

    CYForEachInComprehension(CYIdentifier *name, CYExpression *set) :
        name_(name),
        set_(set)
    {
    }

    virtual const char *Name() const {
        return name_->Value();
    }

    virtual void Begin_(CYOutput &out) const;
    virtual void End_(CYOutput &out) const;
};

struct CYIfComprehension :
    CYComprehension
{
    CYExpression *test_;

    CYIfComprehension(CYExpression *test) :
        test_(test)
    {
    }

    virtual const char *Name() const {
        return NULL;
    }

    virtual void Begin_(CYOutput &out) const;
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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYLiteral :
    CYExpression
{
    CYPrecedence(0)
    CYRightHand(false)
};

struct CYMagic :
    CYExpression
{
    CYPrecedence(0)
    CYRightHand(false)
};

struct CYSelectorPart :
    CYNext<CYSelectorPart>,
    CYThing
{
    CYWord *name_;
    bool value_;

    CYSelectorPart(CYWord *name, bool value, CYSelectorPart *next) :
        CYNext<CYSelectorPart>(next),
        name_(name),
        value_(value)
    {
    }

    virtual void Output(CYOutput &out) const;
};

struct CYSelector :
    CYLiteral
{
    CYSelectorPart *name_;

    CYSelector(CYSelectorPart *name) :
        name_(name)
    {
    }

    CYPrecedence(1)

    virtual void Output(CYOutput &out, CYFlags flags) const;
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
    CYLiteral,
    CYPropertyName
{
    const char *value_;
    size_t size_;

    CYString(const char *value, size_t size) :
        value_(value),
        size_(size)
    {
    }

    CYString(const CYIdentifier *identifier) :
        value_(identifier->Value()),
        size_(strlen(value_))
    {
    }

    const char *Value() const {
        return value_;
    }

    virtual const char *Word() const;

    virtual void Output(CYOutput &out, CYFlags flags) const;
    virtual void PropertyName(CYOutput &out) const;
};

struct CYNumber :
    CYLiteral,
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

    virtual void Output(CYOutput &out, CYFlags flags) const;
    virtual void PropertyName(CYOutput &out) const;
};

struct CYRegEx :
    CYLiteral
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
    CYLiteral
{
    CYNull() :
        CYWord("null")
    {
    }

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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYBoolean :
    CYLiteral
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
};

struct CYVariable :
    CYExpression
{
    CYIdentifier *name_;

    CYVariable(CYIdentifier *name) :
        name_(name)
    {
    }

    CYPrecedence(0)
    CYRightHand(false)

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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYArgument :
    CYNext<CYArgument>,
    CYThing
{
    CYWord *name_;
    CYExpression *value_;

    CYArgument(CYWord *name, CYExpression *value, CYArgument *next = NULL) :
        CYNext<CYArgument>(next),
        name_(name),
        value_(value)
    {
    }

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
    CYStatement *code_;

    CYClause(CYExpression *_case, CYStatement *code) :
        case_(_case),
        code_(code)
    {
    }

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

    void Output(CYOutput &out) const;
};

struct CYArray :
    CYLiteral
{
    CYElement *elements_;

    CYArray(CYElement *elements) :
        elements_(elements)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYDeclaration :
    CYForInInitialiser
{
    CYIdentifier *identifier_;
    CYExpression *initialiser_;

    CYDeclaration(CYIdentifier *identifier, CYExpression *initialiser) :
        identifier_(identifier),
        initialiser_(initialiser)
    {
    }

    virtual void ForIn(CYOutput &out, CYFlags flags) const;

    virtual const char *ForEachIn() const;
    virtual void ForEachIn(CYOutput &out) const;

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYDeclarations :
    CYNext<CYDeclarations>,
    CYForInitialiser,
    CYThing
{
    CYDeclaration *declaration_;

    CYDeclarations(CYDeclaration *declaration, CYDeclarations *next) :
        CYNext<CYDeclarations>(next),
        declaration_(declaration)
    {
    }

    virtual void For(CYOutput &out) const;

    virtual void Output(CYOutput &out) const;
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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYLet :
    CYStatement
{
    CYDeclarations *declarations_;
    CYStatement *statements_;

    CYLet(CYDeclarations *declarations, CYStatement *statements) :
        declarations_(declarations),
        statements_(statements)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYField :
    CYNext<CYField>
{
    virtual void Output(CYOutput &out) const;
};

struct CYMessageParameter :
    CYNext<CYMessageParameter>
{
    CYWord *tag_;
    CYExpression *type_;
    CYIdentifier *name_;

    CYMessageParameter(CYWord *tag, CYExpression *type, CYIdentifier *name) :
        tag_(tag),
        type_(type),
        name_(name)
    {
    }
};

struct CYMessage :
    CYNext<CYMessage>
{
    bool instance_;
    CYExpression *type_;
    CYMessageParameter *parameter_;
    CYStatement *body_;

    CYMessage(bool instance, CYExpression *type, CYMessageParameter *parameter, CYStatement *body) :
        instance_(instance),
        type_(type),
        parameter_(parameter),
        body_(body)
    {
    }

    virtual void Output(CYOutput &out, bool replace) const;
};

struct CYClass {
    CYClassName *name_;
    CYExpression *super_;
    CYField *fields_;
    CYMessage *messages_;

    CYClass(CYClassName *name, CYExpression *super, CYField *fields, CYMessage *messages) :
        name_(name),
        super_(super),
        fields_(fields),
        messages_(messages)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYClassExpression :
    CYClass,
    CYExpression
{
    CYClassExpression(CYClassName *name, CYExpression *super, CYField *fields, CYMessage *messages) :
        CYClass(name, super, fields, messages)
    {
    }

    CYPrecedence(0)

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYClassStatement :
    CYClass,
    CYStatement
{
    CYClassStatement(CYClassName *name, CYExpression *super, CYField *fields, CYMessage *messages) :
        CYClass(name, super, fields, messages)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYCategory :
    CYStatement
{
    CYClassName *name_;
    CYMessage *messages_;

    CYCategory(CYClassName *name, CYMessage *messages) :
        name_(name),
        messages_(messages)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFunctionParameter :
    CYNext<CYFunctionParameter>,
    CYThing
{
    CYIdentifier *name_;

    CYFunctionParameter(CYIdentifier *name, CYFunctionParameter *next) :
        CYNext<CYFunctionParameter>(next),
        name_(name)
    {
    }

    virtual void Output(CYOutput &out) const;
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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYForEachIn :
    CYStatement
{
    CYForInInitialiser *initialiser_;
    CYExpression *set_;
    CYStatement *code_;

    CYForEachIn(CYForInInitialiser *initialiser, CYExpression *set, CYStatement *code) :
        initialiser_(initialiser),
        set_(set),
        code_(code)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYProperty :
    CYNext<CYProperty>,
    CYThing
{
    CYPropertyName *name_;
    CYExpression *value_;

    CYProperty(CYPropertyName *name, CYExpression *value, CYProperty *next) :
        CYNext<CYProperty>(next),
        name_(name),
        value_(value)
    {
    }

    virtual void Output(CYOutput &out) const;
};

struct CYObject :
    CYLiteral
{
    CYProperty *property_;

    CYObject(CYProperty *property) :
        property_(property)
    {
    }

    void Output(CYOutput &out, CYFlags flags) const;
};

struct CYCatch :
    CYThing
{
    CYIdentifier *name_;
    CYStatement *code_;

    CYCatch(CYIdentifier *name, CYStatement *code) :
        name_(name),
        code_(code)
    {
    }

    virtual void Output(CYOutput &out) const;
};

struct CYSend :
    CYExpression
{
    CYExpression *self_;
    CYArgument *arguments_;

    CYSend(CYExpression *self, CYArgument *arguments) :
        self_(self),
        arguments_(arguments)
    {
    }

    CYPrecedence(0)

    virtual void Output(CYOutput &out, CYFlags flags) const;
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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYNew :
    CYExpression
{
    CYExpression *constructor_;
    CYArgument *arguments_;

    CYNew(CYExpression *constructor, CYArgument *arguments) :
        constructor_(constructor),
        arguments_(arguments)
    {
    }

    virtual unsigned Precedence() const {
        return arguments_ == NULL ? 2 : 1;
    }

    CYRightHand(false)

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYCall :
    CYExpression
{
    CYExpression *function_;
    CYArgument *arguments_;

    CYCall(CYExpression *function, CYArgument *arguments) :
        function_(function),
        arguments_(arguments)
    {
    }

    CYPrecedence(1)
    CYRightHand(false)

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYIf :
    CYStatement
{
    CYExpression *test_;
    CYStatement *true_;
    CYStatement *false_;

    CYIf(CYExpression *test, CYStatement *_true, CYStatement *_false) :
        test_(test),
        true_(_true),
        false_(_false)
    {
    }

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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFunction {
    CYIdentifier *name_;
    CYFunctionParameter *parameters_;
    CYStatement *body_;

    CYFunction(CYIdentifier *name, CYFunctionParameter *parameters, CYStatement *body) :
        name_(name),
        parameters_(parameters),
        body_(body)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFunctionExpression :
    CYFunction,
    CYExpression
{
    CYFunctionExpression(CYIdentifier *name, CYFunctionParameter *parameters, CYStatement *body) :
        CYFunction(name, parameters, body)
    {
    }

    CYPrecedence(0)
    CYRightHand(false)

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFunctionStatement :
    CYFunction,
    CYStatement
{
    CYFunctionStatement(CYIdentifier *name, CYFunctionParameter *parameters, CYStatement *body) :
        CYFunction(name, parameters, body)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYExpress :
    CYStatement
{
    CYExpression *expression_;

    CYExpress(CYExpression *expression) :
        expression_(expression)
    {
    }

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

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYEmpty :
    CYStatement
{
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYFinally :
    CYThing
{
    CYStatement *code_;

    CYFinally(CYStatement *code) :
        code_(code)
    {
    }

    virtual void Output(CYOutput &out) const;
};

struct CYTry :
    CYStatement
{
    CYStatement *code_;
    CYCatch *catch_;
    CYFinally *finally_;

    CYTry(CYStatement *code, CYCatch *_catch, CYFinally *finally) :
        code_(code),
        catch_(_catch),
        finally_(finally)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYThrow :
    CYStatement
{
    CYExpression *value_;

    CYThrow(CYExpression *value) :
        value_(value)
    {
    }

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

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
    CYPrecedence(2)

    virtual void Output(CYOutput &out, CYFlags flags) const;
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
    CYPrecedence(1)

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

#define CYPostfix_(op, name) \
    struct CY ## name : \
        CYPostfix \
    { \
        CY ## name(CYExpression *lhs) : \
            CYPostfix(lhs) \
        { \
        } \
    \
        CYPrecedence(3) \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYPrefix_(alphabetic, op, name) \
    struct CY ## name : \
        CYPrefix \
    { \
        CY ## name(CYExpression *rhs) : \
            CYPrefix(rhs) \
        { \
        } \
    \
        CYAlphabetic(alphabetic) \
        CYPrecedence(4) \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYInfix_(alphabetic, precedence, op, name) \
    struct CY ## name : \
        CYInfix \
    { \
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

#define CYAssignment_(op, name) \
    struct CY ## name ## Assign : \
        CYAssignment \
    { \
        CY ## name ## Assign(CYExpression *lhs, CYExpression *rhs) : \
            CYAssignment(lhs, rhs) \
        { \
        } \
    \
        CYPrecedence(16) \
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

CYInfix_(false, 5, "*", Multiply)
CYInfix_(false, 5, "/", Divide)
CYInfix_(false, 5, "%", Modulus)
CYInfix_(false, 6, "+", Add)
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

#endif/*CYPARSER_HPP*/
