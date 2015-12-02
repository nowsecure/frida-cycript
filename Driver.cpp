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

#include "Cycript.tab.hh"
#include "Driver.hpp"

CYDriver::CYDriver(CYPool &pool, std::istream &data, const std::string &filename) :
    pool_(pool),
    newline_(false),
    last_(false),
    next_(false),
    data_(data),
    debug_(0),
    strict_(false),
    highlight_(false),
    filename_(filename),
    script_(NULL),
    auto_(false),
    context_(NULL),
    mode_(AutoNone)
{
    in_.push(false);
    return_.push(false);
    template_.push(false);
    yield_.push(false);

    ScannerInit();
}

CYDriver::~CYDriver() {
    ScannerDestroy();
}

bool CYDriver::Parse(CYMark mark) {
    mark_ = mark;
    CYLocal<CYPool> local(&pool_);
    cy::parser parser(*this);
#ifdef YYDEBUG
    parser.set_debug_level(debug_);
#endif
    return parser.parse() != 0;
}

void CYDriver::Replace(CYOptions &options) {
    CYLocal<CYPool> local(&pool_);
    CYContext context(options);
    script_->Replace(context);
}

void CYDriver::Warning(const cy::parser::location_type &location, const char *message) {
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
