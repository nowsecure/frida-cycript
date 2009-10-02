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
    virtual void Output(std::ostream &out) const = 0;
};

_finline std::ostream &operator <<(std::ostream &out, const CYThing &rhs) {
    rhs.Output(out);
    return out;
}

struct CYPart {
    virtual void Part(std::ostream &out) const = 0;
};

struct CYSource :
    CYNext<CYSource>
{
    virtual void Show(std::ostream &out) const;
    virtual void Output(std::ostream &out) const = 0;
    virtual void Output(std::ostream &out, bool block) const;
};

struct CYName :
    CYThing
{
    virtual const char *Name() const = 0;
};

struct CYWord :
    CYName
{
    const char *word_;

    CYWord(const char *word) :
        word_(word)
    {
    }

    const char *Value() const {
        return word_;
    }

    virtual const char *Name() const {
        return Value();
    }

    virtual void Output(std::ostream &out) const;
};

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
    CYIdentifier *identifier_;

    CYLabel(CYIdentifier *identifier, CYLabel *next) :
        CYNext<CYLabel>(next),
        identifier_(identifier)
    {
    }
};

struct CYStatement :
    CYSource
{
    CYLabel *label_;

    void AddLabel(CYIdentifier *identifier) {
        label_ = new CYLabel(identifier, label_);
    }
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

    std::string filename_;

    struct Error {
        cy::location location_;
        std::string message_;
    };

    typedef std::vector<Error> Errors;

    CYSource *source_;
    Errors errors_;

  private:
    void ScannerInit();
    void ScannerDestroy();

  public:
    CYDriver(const std::string &filename);
    ~CYDriver();
};

struct CYForInitialiser :
    CYPart
{
};

struct CYForInInitialiser :
    CYPart
{
};

struct CYExpression :
    CYNext<CYExpression>,
    CYForInitialiser,
    CYForInInitialiser
{
    virtual unsigned Precedence() const = 0;
    virtual void Part(std::ostream &out) const;
    virtual void Output(std::ostream &out) const = 0;
    void Output(std::ostream &out, unsigned precedence) const;
};

#define CYPrecedence(value) \
    virtual unsigned Precedence() const { \
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

    void Output(std::ostream &out) const;
};

struct CYLiteral :
    CYExpression
{
    CYPrecedence(0)
};

struct CYSelectorPart :
    CYNext<CYSelectorPart>
{
    CYWord *name_;
    bool value_;

    CYSelectorPart(CYWord *name, bool value, CYSelectorPart *next) :
        CYNext<CYSelectorPart>(next),
        name_(name),
        value_(value)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYSelector :
    CYLiteral
{
    CYSelectorPart *name_;

    CYSelector(CYSelectorPart *name) :
        name_(name)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYString :
    CYLiteral,
    CYName
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

    virtual const char *Name() const {
        return Value();
    }

    virtual void Output(std::ostream &out) const;
};

struct CYNumber :
    CYLiteral,
    CYName
{
    double value_;

    CYNumber(double value) :
        value_(value)
    {
    }

    double Value() const {
        return value_;
    }

    virtual const char *Name() const {
        throw;
    }

    virtual void Output(std::ostream &out) const;
};

struct CYNull :
    CYWord,
    CYLiteral
{
    CYNull() :
        CYWord("null")
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYThis :
    CYWord,
    CYExpression
{
    CYThis() :
        CYWord("this")
    {
    }

    CYPrecedence(0)

    virtual void Output(std::ostream &out) const;
};

struct CYBoolean :
    CYLiteral
{
    virtual bool Value() const = 0;
    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
};

struct CYPrefix :
    CYExpression
{
    CYExpression *rhs_;

    CYPrefix(CYExpression *rhs) :
        rhs_(rhs)
    {
    }

    virtual const char *Operator() const = 0;

    virtual void Output(std::ostream &out) const;
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

    virtual const char *Operator() const = 0;

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
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

    virtual const char *Operator() const = 0;

    virtual void Output(std::ostream &out) const;
};

struct CYArgument :
    CYNext<CYArgument>
{
    CYWord *name_;
    CYExpression *value_;

    CYArgument(CYWord *name, CYExpression *value, CYArgument *next = NULL) :
        CYNext<CYArgument>(next),
        name_(name),
        value_(value)
    {
    }

    void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
};

struct CYElement :
    CYNext<CYElement>
{
    CYExpression *value_;

    CYElement(CYExpression *value, CYElement *next) :
        CYNext<CYElement>(next),
        value_(value)
    {
    }

    void Output(std::ostream &out) const;
};

struct CYArray :
    CYLiteral
{
    CYElement *elements_;

    CYArray(CYElement *elements) :
        elements_(elements)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYDeclaration :
    CYThing,
    CYForInInitialiser
{
    CYIdentifier *identifier_;
    CYExpression *initialiser_;

    CYDeclaration(CYIdentifier *identifier, CYExpression *initialiser) :
        identifier_(identifier),
        initialiser_(initialiser)
    {
    }

    virtual void Part(std::ostream &out) const;
    virtual void Output(std::ostream &out) const;
};

struct CYDeclarations :
    CYStatement,
    CYForInitialiser
{
    CYDeclaration *declaration_;
    CYDeclarations *next_;

    CYDeclarations(CYDeclaration *declaration, CYDeclarations *next) :
        declaration_(declaration),
        next_(next)
    {
    }

    virtual void Part(std::ostream &out) const;
    virtual void Output(std::ostream &out) const;
};

struct CYParameter :
    CYNext<CYParameter>,
    CYThing
{
    CYIdentifier *name_;

    CYParameter(CYIdentifier *name, CYParameter *next) :
        CYNext<CYParameter>(next),
        name_(name)
    {
    }

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
};

struct CYProperty :
    CYNext<CYProperty>
{
    CYName *name_;
    CYExpression *value_;

    CYProperty(CYName *name, CYExpression *value, CYProperty *next) :
        CYNext<CYProperty>(next),
        name_(name),
        value_(value)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYObject :
    CYLiteral
{
    CYProperty *property_;

    CYObject(CYProperty *property) :
        property_(property)
    {
    }

    void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
};

struct CYMessage :
    CYExpression
{
    CYExpression *self_;
    CYArgument *arguments_;

    CYMessage(CYExpression *self, CYArgument *arguments) :
        self_(self),
        arguments_(arguments)
    {
    }

    CYPrecedence(0)

    virtual void Output(std::ostream &out) const;
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

    CYPrecedence(1)

    virtual void Output(std::ostream &out) const;
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

    CYPrecedence(1)

    virtual void Output(std::ostream &out) const;
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

    CYPrecedence(2)

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
};

struct CYLambda :
    CYExpression
{
    CYIdentifier *name_;
    CYParameter *parameters_;
    CYSource *body_;

    CYLambda(CYIdentifier *name, CYParameter *parameters, CYSource *body) :
        name_(name),
        parameters_(parameters),
        body_(body)
    {
    }

    CYPrecedence(0)

    virtual void Output(std::ostream &out) const;
};

struct CYFunction :
    CYLambda,
    CYSource
{
    CYFunction(CYIdentifier *name, CYParameter *parameters, CYSource *body) :
        CYLambda(name, parameters, body)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYExpress :
    CYStatement
{
    CYExpression *expression_;

    CYExpress(CYExpression *expression) :
        expression_(expression)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYContinue :
    CYStatement
{
    CYIdentifier *label_;

    CYContinue(CYIdentifier *label) :
        label_(label)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYBreak :
    CYStatement
{
    CYIdentifier *label_;

    CYBreak(CYIdentifier *label) :
        label_(label)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYReturn :
    CYStatement
{
    CYExpression *value_;

    CYReturn(CYExpression *value) :
        value_(value)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYEmpty :
    CYStatement
{
    virtual void Output(std::ostream &out) const;
    virtual void Output(std::ostream &out, bool block) const;
};

struct CYTry :
    CYStatement
{
    CYStatement *try_;
    CYCatch *catch_;
    CYStatement *finally_;

    CYTry(CYStatement *_try, CYCatch *_catch, CYStatement *finally) :
        try_(_try),
        catch_(_catch),
        finally_(finally)
    {
    }

    virtual void Output(std::ostream &out) const;
};

struct CYThrow :
    CYStatement
{
    CYExpression *value_;

    CYThrow(CYExpression *value) :
        value_(value)
    {
    }

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
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

    virtual void Output(std::ostream &out) const;
};

struct CYCondition :
    CYExpression
{
    CYExpression *test_;
    CYExpression *true_;
    CYExpression *false_;

    CYCondition(CYExpression *test, CYExpression *_true, CYExpression *_false) :
        true_(_true),
        false_(_false)
    {
    }

    CYPrecedence(15)

    virtual void Output(std::ostream &out) const;
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

    CYPrecedence(2)

    virtual void Output(std::ostream &out) const;
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

    CYPrecedence(1)

    virtual void Output(std::ostream &out) const;
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

#define CYPrefix_(op, name) \
    struct CY ## name : \
        CYPrefix \
    { \
        CY ## name(CYExpression *rhs) : \
            CYPrefix(rhs) \
        { \
        } \
    \
        CYPrecedence(4) \
    \
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYInfix_(precedence, op, name) \
    struct CY ## name : \
        CYInfix \
    { \
        CY ## name(CYExpression *lhs, CYExpression *rhs) : \
            CYInfix(lhs, rhs) \
        { \
        } \
    \
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

CYPrefix_("delete", Delete)
CYPrefix_("void", Void)
CYPrefix_("typeof", TypeOf)
CYPrefix_("++", PreIncrement)
CYPrefix_("--", PreDecrement)
CYPrefix_("-", Negate)
CYPrefix_("~", BitwiseNot)
CYPrefix_("!", LogicalNot)

CYInfix_(5, "*", Multiply)
CYInfix_(5, "/", Divide)
CYInfix_(5, "%", Modulus)
CYInfix_(6, "+", Add)
CYInfix_(6, "-", Subtract)
CYInfix_(7, "<<", ShiftLeft)
CYInfix_(7, ">>", ShiftRightSigned)
CYInfix_(7, ">>>", ShiftRightUnsigned)
CYInfix_(8, "<", Less)
CYInfix_(8, ">", Greater)
CYInfix_(8, "<=", LessOrEqual)
CYInfix_(8, ">=", GreaterOrEqual)
CYInfix_(8, "instanceof", InstanceOf)
CYInfix_(8, "in", In)
CYInfix_(9, "==", Equal)
CYInfix_(9, "!=", NotEqual)
CYInfix_(9, "===", Identical)
CYInfix_(9, "!==", NotIdentical)
CYInfix_(10, "&", BitwiseAnd)
CYInfix_(11, "^", BitwiseXOr)
CYInfix_(12, "|", BitwiseOr)
CYInfix_(13, "&&", LogicalAnd)
CYInfix_(14, "||", LogicalOr)

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
