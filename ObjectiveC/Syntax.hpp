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

#ifndef CYCRIPT_OBJECTIVEC_SYNTAX_HPP
#define CYCRIPT_OBJECTIVEC_SYNTAX_HPP

#include "Parser.hpp"

struct CYInstanceLiteral :
    CYExpression
{
    CYNumber *number_;

    CYInstanceLiteral(CYNumber *number) :
        number_(number)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYObjCBlock :
    CYExpression
{
    CYTypedIdentifier *typed_;
    CYTypedParameter *parameters_;
    CYStatement *code_;

    CYObjCBlock(CYTypedIdentifier *typed, CYTypedParameter *parameters, CYStatement *code) :
        typed_(typed),
        parameters_(parameters),
        code_(code)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYBox :
    CYExpression
{
    CYExpression *value_;

    CYBox(CYExpression *value) :
        value_(value)
    {
    }

    CYPrecedence(1)

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYSelectorPart :
    CYNext<CYSelectorPart>,
    CYThing
{
    CYWord *name_;
    bool value_;

    CYSelectorPart(CYWord *name, bool value, CYSelectorPart *next = NULL) :
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

struct CYClassField :
    CYNext<CYClassField>
{
    CYTypedIdentifier *typed_;

    CYClassField(CYTypedIdentifier *typed, CYClassField *next = NULL) :
        CYNext<CYClassField>(next),
        typed_(typed)
    {
    }

    CYStatement *Replace(CYContext &context) const;
    void Output(CYOutput &out) const;
};

struct CYMessageParameter :
    CYNext<CYMessageParameter>
{
    CYWord *tag_;
    CYTypedIdentifier *type_;

    CYMessageParameter(CYWord *tag, CYTypedIdentifier *type) :
        tag_(tag),
        type_(type)
    {
    }

    CYFunctionParameter *Parameters(CYContext &context) const;
    CYSelector *Selector(CYContext &context) const;
    CYSelectorPart *SelectorPart(CYContext &context) const;
    CYExpression *TypeSignature(CYContext &context) const;
};

struct CYMessage :
    CYNext<CYMessage>
{
    bool instance_;
    CYTypedIdentifier *type_;
    CYMessageParameter *parameters_;
    CYBlock code_;

    CYMessage(bool instance, CYTypedIdentifier *type, CYMessageParameter *parameter, CYStatement *code) :
        instance_(instance),
        type_(type),
        parameters_(parameter),
        code_(code)
    {
    }

    CYStatement *Replace(CYContext &context, bool replace) const;
    void Output(CYOutput &out, bool replace) const;

    CYExpression *TypeSignature(CYContext &context) const;
};

struct CYProtocol :
    CYNext<CYProtocol>,
    CYThing
{
    CYExpression *name_;

    CYProtocol(CYExpression *name, CYProtocol *next = NULL) :
        CYNext<CYProtocol>(next),
        name_(name)
    {
    }

    CYStatement *Replace(CYContext &context) const;
    void Output(CYOutput &out) const;
};

struct CYClass {
    CYClassName *name_;
    CYExpression *super_;
    CYProtocol *protocols_;
    CYClassField *fields_;
    CYMessage *messages_;

    CYClass(CYClassName *name, CYExpression *super, CYProtocol *protocols, CYClassField *fields, CYMessage *messages) :
        name_(name),
        super_(super),
        protocols_(protocols),
        fields_(fields),
        messages_(messages)
    {
    }

    virtual ~CYClass() {
    }

    CYExpression *Replace_(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYClassExpression :
    CYClass,
    CYExpression
{
    CYClassExpression(CYClassName *name, CYExpression *super, CYProtocol *protocols, CYClassField *fields, CYMessage *messages) :
        CYClass(name, super, protocols, fields, messages)
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
    CYClassStatement(CYClassName *name, CYExpression *super, CYProtocol *protocols, CYClassField *fields, CYMessage *messages) :
        CYClass(name, super, protocols, fields, messages)
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
    CYArgument *arguments_;

    CYSend(CYArgument *arguments) :
        arguments_(arguments)
    {
    }

    CYPrecedence(0)

    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYSendDirect :
    CYSend
{
    CYExpression *self_;

    CYSendDirect(CYExpression *self, CYArgument *arguments) :
        CYSend(arguments),
        self_(self)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

struct CYSendSuper :
    CYSend
{
    CYSendSuper(CYArgument *arguments) :
        CYSend(arguments)
    {
    }

    virtual CYExpression *Replace(CYContext &context);
    virtual void Output(CYOutput &out, CYFlags flags) const;
};

#endif/*CYCRIPT_OBJECTIVEC_SYNTAX_HPP*/
