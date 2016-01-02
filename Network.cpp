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

#include "cycript.hpp"

#include <sys/types.h>
#include <sys/socket.h>

#include "Error.hpp"

_visible bool CYRecvAll_(int socket, uint8_t *data, size_t size) {
    while (size != 0) if (size_t writ = _syscall(recv(socket, data, size, 0))) {
        data += writ;
        size -= writ;
    } else
        return false;
    return true;
}

_visible bool CYSendAll_(int socket, const uint8_t *data, size_t size) {
    while (size != 0) if (size_t writ = _syscall(send(socket, data, size, 0))) {
        data += writ;
        size -= writ;
    } else
        return false;
    return true;
}
