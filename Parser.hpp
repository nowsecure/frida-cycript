#ifndef CYPARSER_HPP
#define CYPARSER_HPP

class CYParser {
};

struct CYExpression {
};

struct CYToken {
    virtual const char *Text() const = 0;
};

struct CYTokenLiteral :
    CYExpression,
    virtual CYToken
{
};

struct CYTokenString :
    CYTokenLiteral
{
};

struct CYTokenNumber :
    CYTokenLiteral
{
};

struct CYTokenWord :
    virtual CYToken
{
};

struct CYTokenIdentifier :
    CYExpression,
    CYTokenWord
{
    const char *word_;

    virtual const char *Text() const {
        return word_;
    }
};

struct CYExpressionPrefix :
    CYExpression
{
    CYExpression *rhs_;

    CYExpressionPrefix(CYExpression *rhs) :
        rhs_(rhs)
    {
    }
};

struct CYExpressionInfix :
    CYExpression
{
    CYExpression *lhs_;
    CYExpression *rhs_;

    CYExpressionInfix(CYExpression *lhs, CYExpression *rhs) :
        lhs_(lhs),
        rhs_(rhs)
    {
    }
};

struct CYExpressionPostfix :
    CYExpression
{
    CYExpression *lhs_;

    CYExpressionPostfix(CYExpression *lhs) :
        lhs_(lhs)
    {
    }
};

struct CYExpressionAssignment :
    CYExpression
{
    CYExpression *lhs_;
    CYExpression *rhs_;

    CYExpressionAssignment(CYExpression *lhs, CYExpression *rhs) :
        lhs_(lhs),
        rhs_(rhs)
    {
    }
};

#endif/*CYPARSER_HPP*/
