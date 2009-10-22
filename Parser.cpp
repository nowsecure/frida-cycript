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

#include "Parser.hpp"
#include "Cycript.tab.hh"

CYRange DigitRange_    (0x3ff000000000000LLU, 0x000000000000000LLU); // 0-9
CYRange WordStartRange_(0x000001000000000LLU, 0x7fffffe87fffffeLLU); // A-Za-z_$
CYRange WordEndRange_  (0x3ff001000000000LLU, 0x7fffffe87fffffeLLU); // A-Za-z_$0-9

CYDriver::CYDriver(const std::string &filename) :
    state_(CYClear),
    data_(NULL),
    size_(0),
    file_(NULL),
    strict_(false),
    filename_(filename),
    program_(NULL)
{
    ScannerInit();
}

CYDriver::~CYDriver() {
    ScannerDestroy();
}

void CYDriver::Warning(const cy::location &location, const char *message) {
    if (!strict_)
        return;

    CYDriver::Error error;
    error.warning_ = true;
    error.location_ = location;
    error.message_ = message;
    errors_.push_back(error);
}

void cy::parser::error(const cy::parser::location_type &location, const std::string &message) {
    CYDriver::Error error;
    error.warning_ = false;
    error.location_ = location;
    error.message_ = message;
    driver.errors_.push_back(error);
}
