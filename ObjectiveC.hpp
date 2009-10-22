#ifndef CYCRIPT_OBJECTIVEC_HPP
#define CYCRIPT_OBJECTIVEC_HPP

#include "Parser.hpp"

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

    CYString *Replace(CYContext &context);
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

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYField :
    CYNext<CYField>
{
    CYStatement *Replace(CYContext &context) const;
    void Output(CYOutput &out) const;
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

    CYFunctionParameter *Parameters(CYContext &context) const;
    CYSelector *Selector(CYContext &context) const;
    CYSelectorPart *SelectorPart(CYContext &context) const;
};

struct CYMessage :
    CYNext<CYMessage>
{
    bool instance_;
    CYExpression *type_;
    CYMessageParameter *parameters_;
    CYStatement *statements_;

    CYMessage(bool instance, CYExpression *type, CYMessageParameter *parameter, CYStatement *statements) :
        instance_(instance),
        type_(type),
        parameters_(parameter),
        statements_(statements)
    {
    }

    CYStatement *Replace(CYContext &context, bool replace) const;
    void Output(CYOutput &out, bool replace) const;
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

    CYExpression *Replace_(CYContext &context);
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

    virtual CYExpression *Replace(CYContext &context);
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

    virtual CYStatement *Replace(CYContext &context);
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

    virtual CYStatement *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
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

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

#endif/*CYCRIPT_OBJECTIVEC_HPP*/
