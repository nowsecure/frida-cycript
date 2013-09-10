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

#include "Cycript.tab.hh"
#include "Driver.hpp"

CYDriver::CYDriver(std::istream &data, const std::string &filename) :
    state_(CYClear),
    data_(data),
    strict_(false),
    commented_(false),
    filename_(filename),
    program_(NULL),
    auto_(false),
    context_(NULL),
    mode_(AutoNone)
{
    memset(&no_, 0, sizeof(no_));
    in_.push(false);
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
