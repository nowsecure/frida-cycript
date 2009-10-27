/* Cycript - Remote Execution Server and Disassembler
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

#ifndef CYCRIPT_OBJECTIVEC_SYNTAX_HPP
#define CYCRIPT_OBJECTIVEC_SYNTAX_HPP

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
    CYBlock code_;

    CYMessage(bool instance, CYExpression *type, CYMessageParameter *parameter, CYStatement *statements) :
        instance_(instance),
        type_(type),
        parameters_(parameter),
        code_(statements)
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
