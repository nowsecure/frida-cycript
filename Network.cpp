#include "cycript.hpp"

#include <sys/types.h>
#include <sys/socket.h>

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
