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

#include "cycript.hpp"

#include <sys/types.h>
#include <sys/socket.h>

#include "Error.hpp"

bool CYRecvAll_(int socket, uint8_t *data, size_t size) {
    while (size != 0) if (size_t writ = _syscall(recv(socket, data, size, 0))) {
        data += writ;
        size -= writ;
    } else
        return false;
    return true;
}

bool CYSendAll_(int socket, const uint8_t *data, size_t size) {
    while (size != 0) if (size_t writ = _syscall(send(socket, data, size, 0))) {
        data += writ;
        size -= writ;
    } else
        return false;
    return true;
}
