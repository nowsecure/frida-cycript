/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
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

#ifndef CYCRIPT_HPP
#define CYCRIPT_HPP

#include <sstream>

#include <sig/types.hpp>

#include "Pooling.hpp"
#include "String.hpp"

bool CYRecvAll_(int socket, uint8_t *data, size_t size);
bool CYSendAll_(int socket, const uint8_t *data, size_t size);

double CYCastDouble(const char *value, size_t size);
double CYCastDouble(const char *value);

void CYHandleClient(int socket);

template <typename Type_>
bool CYRecvAll(int socket, Type_ *data, size_t size) {
    return CYRecvAll_(socket, reinterpret_cast<uint8_t *>(data), size);
}

template <typename Type_>
bool CYSendAll(int socket, const Type_ *data, size_t size) {
    return CYSendAll_(socket, reinterpret_cast<const uint8_t *>(data), size);
}

CYPool &CYGetGlobalPool();

char **CYComplete(const char *word, const std::string &line, CYUTF8String (*run)(CYPool &pool, const std::string &));

const char *CYPoolLibraryPath(CYPool &pool);

#endif/*CYCRIPT_HPP*/
