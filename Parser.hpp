#ifndef CYPARSER_HPP
#define CYPARSER_HPP

#include <cstdlib>

class CYParser {
  public:
    void *scanner_;

  private:
    void ScannerInit();
    void ScannerDestroy();

  public:
    CYParser();
    ~CYParser();
};

struct CYSource {
    CYSource *next_;

    void SetNext(CYSource *next) {
        next_ = next;
    }
};

struct CYName {
    virtual const char *Name() const = 0;
};

struct CYToken {
    virtual const char *Text() const = 0;
};

struct CYWord :
    virtual CYToken,
    CYName
{
    const char *word_;

    CYWord(const char *word) :
        word_(word)
    {
    }

    virtual const char *Text() const {
        return word_;
    }

    virtual const char *Name() const {
        return Text();
    }
};

struct CYIdentifier :
    CYWord
{
    const char *word_;

    virtual const char *Text() const {
        return word_;
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

struct CYForInitialiser {
};

struct CYForInInitialiser {
};

struct CYExpression :
    CYStatement,
    CYForInitialiser,
    CYForInInitialiser
{
};

struct CYLiteral :
    CYExpression
{
};

struct CYString :
    CYLiteral,
    CYName
{
    const char *value_;

    CYString(const char *value) :
        value_(value)
    {
    }

    CYString(const CYIdentifier *identifier) :
        value_(identifier->Text())
    {
    }

    const char *String() const {
        return value_;
    }

    virtual const char *Name() const {
        return String();
    }
};

struct CYNumber :
    virtual CYToken,
    CYLiteral,
    CYName
{
    double Number() const {
        throw;
    }

    virtual const char *Name() const {
        throw;
    }
};

struct CYNull :
    CYWord,
    CYLiteral
{
    CYNull() :
        CYWord("null")
    {
    }
};

struct CYThis :
    CYWord,
    CYExpression
{
    CYThis() :
        CYWord("this")
    {
    }
};

struct CYBoolean :
    CYLiteral
{
};

struct CYFalse :
    CYWord,
    CYBoolean
{
    CYFalse() :
        CYWord("false")
    {
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
};

struct CYVariable :
    CYExpression
{
    CYIdentifier *name_;

    CYVariable(CYIdentifier *name) :
        name_(name)
    {
    }
};

struct CYPrefix :
    CYExpression
{
    CYExpression *rhs_;

    CYPrefix(CYExpression *rhs) :
        rhs_(rhs)
    {
    }
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
};

struct CYPostfix :
    CYExpression
{
    CYExpression *lhs_;

    CYPostfix(CYExpression *lhs) :
        lhs_(lhs)
    {
    }
};

struct CYAssignment :
    CYInfix
{
    CYAssignment(CYExpression *lhs, CYExpression *rhs) :
        CYInfix(lhs, rhs)
    {
    }
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
};

struct CYBlank :
    public CYWord
{
    CYBlank() :
        CYWord("")
    {
    }
};

struct CYClause {
    CYExpression *case_;
    CYStatement *code_;
    CYClause *next_;

    CYClause(CYExpression *_case, CYStatement *code) :
        case_(_case),
        code_(code)
    {
    }

    void SetNext(CYClause *next) {
        next_ = next;
    }
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
};

struct CYParameter {
    CYIdentifier *name_;
    CYParameter *next_;

    CYParameter(CYIdentifier *name, CYParameter *next) :
        name_(name),
        next_(next)
    {
    }
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
};

struct CYCatch {
    CYIdentifier *name_;
    CYStatement *code_;

    CYCatch(CYIdentifier *name, CYStatement *code) :
        name_(name),
        code_(code)
    {
    }
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
};

struct CYFunction :
    CYLambda
{
    CYFunction(CYIdentifier *name, CYParameter *parameters, CYSource *body) :
        CYLambda(name, parameters, body)
    {
    }
};

struct CYContinue :
    CYStatement
{
    CYIdentifier *label_;

    CYContinue(CYIdentifier *label) :
        label_(label)
    {
    }
};

struct CYBreak :
    CYStatement
{
    CYIdentifier *label_;

    CYBreak(CYIdentifier *label) :
        label_(label)
    {
    }
};

struct CYReturn :
    CYStatement
{
    CYExpression *value_;

    CYReturn(CYExpression *value) :
        value_(value)
    {
    }
};

struct CYEmpty :
    CYStatement
{
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
};

struct CYThrow :
    CYStatement
{
    CYExpression *value_;

    CYThrow(CYExpression *value) :
        value_(value)
    {
    }
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
};

#define CYPostfix_(op, name) \
    struct CY ## name : \
        CYPostfix \
    { \
        CY ## name(CYExpression *lhs) : \
            CYPostfix(lhs) \
        { \
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
    };

#define CYInfix_(op, name) \
    struct CY ## name : \
        CYInfix \
    { \
        CY ## name(CYExpression *lhs, CYExpression *rhs) : \
            CYInfix(lhs, rhs) \
        { \
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
CYPrefix_("*", Indirect)
CYPrefix_("&", AddressOf)

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
