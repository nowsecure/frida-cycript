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

#ifndef CYCRIPT_DRIVER_HPP
#define CYCRIPT_DRIVER_HPP

#include <iostream>

#include <stack>
#include <string>
#include <vector>

#include "location.hh"

#include "Parser.hpp"

enum CYState {
    CYClear,
    CYRestricted,
    CYNewLine
};

class CYDriver {
  public:
    void *scanner_;

    CYState state_;
    std::stack<bool> in_;

    struct {
        bool AtImplementation;
        bool Function;
        bool OpenBrace;
    } no_;

    std::istream &data_;

    bool strict_;
    bool commented_;

    enum Condition {
        RegExpCondition,
        XMLContentCondition,
        XMLTagCondition,
    };

    std::string filename_;

    struct Error {
        bool warning_;
        cy::location location_;
        std::string message_;
    };

    typedef std::vector<Error> Errors;

    CYProgram *program_;
    Errors errors_;

    bool auto_;

    struct Context {
        CYExpression *context_;

        Context(CYExpression *context) :
            context_(context)
        {
        }

        typedef std::vector<CYWord *> Words;
        Words words_;
    };

    typedef std::vector<Context> Contexts;
    Contexts contexts_;

    CYExpression *context_;

    enum Mode {
        AutoNone,
        AutoPrimary,
        AutoDirect,
        AutoIndirect,
        AutoMessage
    } mode_;

  private:
    void ScannerInit();
    void ScannerDestroy();

  public:
    CYDriver(std::istream &data, const std::string &filename = "");
    ~CYDriver();

    Condition GetCondition();
    void SetCondition(Condition condition);

    void PushCondition(Condition condition);
    void PopCondition();

    void Warning(const cy::location &location, const char *message);
};

#endif/*CYCRIPT_DRIVER_HPP*/
