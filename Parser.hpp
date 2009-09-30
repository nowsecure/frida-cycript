#ifndef CYPARSER_HPP
#define CYPARSER_HPP

#include <cstdlib>
#include <string>

#include "Pooling.hpp"

template <typename Type_>
struct CYNext {
    Type_ *next_;

    CYNext() :
        next_(NULL)
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

struct CYLabel {
    CYIdentifier *identifier_;
    CYLabel *next_;

    CYLabel(CYIdentifier *identifier, CYLabel *next) :
        identifier_(identifier),
        next_(next)
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

class CYDriver {
  public:
    CYPool pool_;
    std::string filename_;
    CYSource *source_;
    void *scanner_;

  private:
    void ScannerInit();
    void ScannerDestroy();

  public:
    CYDriver(const std::string &filename);
    ~CYDriver();

    void Clear();
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
    virtual void Part(std::ostream &out) const;
    virtual void Output(std::ostream &out) const = 0;
    void Output(std::ostream &out, bool raw) const;
};

_finline std::ostream &operator <<(std::ostream &out, const CYExpression &rhs) {
    rhs.Output(out, false);
    return out;
}

struct CYLiteral :
    CYExpression
{
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
    CYInfix
{
    CYAssignment(CYExpression *lhs, CYExpression *rhs) :
        CYInfix(lhs, rhs)
    {
    }

    virtual const char *Operator() const = 0;
};

struct CYArgument {
    CYWord *name_;
    CYExpression *value_;
    CYArgument *next_;

    CYArgument(CYWord *name, CYExpression *value, CYArgument *next = NULL) :
        name_(name),
        value_(value),
        next_(next)
    {
    }

    void Output(std::ostream &out, bool send) const;
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
    CYLiteral
{
    CYExpression *value_;
    CYElement *next_;

    CYElement(CYExpression *value, CYElement *next) :
        value_(value),
        next_(next)
    {
    }

    void Output(std::ostream &out, bool raw) const;
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
    CYThing
{
    CYIdentifier *name_;
    CYParameter *next_;

    CYParameter(CYIdentifier *name, CYParameter *next) :
        name_(name),
        next_(next)
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
    CYLiteral
{
    CYName *name_;
    CYExpression *value_;
    CYProperty *next_;

    CYProperty(CYName *name, CYExpression *value, CYProperty *next) :
        name_(name),
        value_(value),
        next_(next)
    {
    }

    void Output(std::ostream &out, bool raw) const;
    virtual void Output(std::ostream &out) const;
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
        virtual const char *Operator() const { \
            return op; \
        } \
    };

#define CYInfix_(op, name) \
    struct CY ## name : \
        CYInfix \
    { \
        CY ## name(CYExpression *lhs, CYExpression *rhs) : \
            CYInfix(lhs, rhs) \
        { \
        } \
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

CYInfix_("*", Multiply)
CYInfix_("/", Divide)
CYInfix_("%", Modulus)
CYInfix_("+", Add)
CYInfix_("-", Subtract)
CYInfix_("<<", ShiftLeft)
CYInfix_(">>", ShiftRightSigned)
CYInfix_(">>>", ShiftRightUnsigned)
CYInfix_("<", Less)
CYInfix_(">", Greater)
CYInfix_("<=", LessOrEqual)
CYInfix_(">=", GreaterOrEqual)
CYInfix_("instanceof", InstanceOf)
CYInfix_("in", In)
CYInfix_("==", Equal)
CYInfix_("!=", NotEqual)
CYInfix_("===", Identical)
CYInfix_("!==", NotIdentical)
CYInfix_("&", BitwiseAnd)
CYInfix_("^", BitwiseXOr)
CYInfix_("|", BitwiseOr)
CYInfix_("&&", LogicalAnd)
CYInfix_("||", LogicalOr)

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
