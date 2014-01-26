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

#ifndef CYCRIPT_HPP
#define CYCRIPT_HPP

#include <sig/types.hpp>
#include <sstream>

#include "Pooling.hpp"

bool CYRecvAll_(int socket, uint8_t *data, size_t size);
bool CYSendAll_(int socket, const uint8_t *data, size_t size);

void CYNumerify(std::ostringstream &str, double value);
void CYStringify(std::ostringstream &str, const char *data, size_t size);

double CYCastDouble(const char *value, size_t size);
double CYCastDouble(const char *value);

extern "C" void CYHandleClient(int socket);

template <typename Type_>
bool CYRecvAll(int socket, Type_ *data, size_t size) {
    return CYRecvAll_(socket, reinterpret_cast<uint8_t *>(data), size);
}

template <typename Type_>
bool CYSendAll(int socket, const Type_ *data, size_t size) {
    return CYSendAll_(socket, reinterpret_cast<const uint8_t *>(data), size);
}

CYPool &CYGetGlobalPool();

#endif/*CYCRIPT_HPP*/
