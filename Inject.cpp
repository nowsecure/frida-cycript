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

#include <sstream>
#include <string>

#include <dlfcn.h>

#include "Exception.hpp"
#include "Pooling.hpp"

#if defined(__APPLE__) && (defined(__i386__) || defined(__x86_64__))
#include <unistd.h>

#include <sys/fcntl.h>
#include <sys/mman.h>

#include <mach-o/loader.h>

#define CS_OPS_PIDOFFSET 6

extern "C" int csops(pid_t pid, unsigned int ops, void *useraddr, size_t usersize);
extern "C" int proc_pidpath(int pid, void *buffer, uint32_t buffersize);
#endif

int main(int argc, char * const argv[], char const * const envp[]);
extern "C" char *MSmain0(int argc, char *argv[]);

static std::string LibraryFor(void *address) {
    Dl_info info;
    _assert(dladdr(address, &info) != 0);
    return info.dli_fname;
}

template <typename Type_>
Type_ *shift(Type_ *data, size_t size) {
    return reinterpret_cast<Type_ *>(reinterpret_cast<uint8_t *>(data) + size);
}

void InjectLibrary(int pid, int argc, const char *const argv[]) {
    auto cynject(LibraryFor(reinterpret_cast<void *>(&main)));
    auto slash(cynject.rfind('/'));
    _assert(slash != std::string::npos);
    cynject = cynject.substr(0, slash) + "/cynject";

    auto library(LibraryFor(reinterpret_cast<void *>(&MSmain0)));

#if defined(__APPLE__) && (defined(__i386__) || defined(__x86_64__))
    off_t offset;
    _assert(csops(pid, CS_OPS_PIDOFFSET, &offset, sizeof(offset)) != -1);

    // XXX: implement a safe version of this
    char path[4096];
    int writ(proc_pidpath(pid, path, sizeof(path)));
    _assert(writ != 0);

    auto fd(_syscall(open(path, O_RDONLY)));

    auto page(getpagesize());
    auto size(page * 4);
    auto map(_syscall(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, offset)));

    _syscall(close(fd)); // XXX: _scope

    auto header(reinterpret_cast<mach_header *>(map));
    auto command(reinterpret_cast<load_command *>(header + 1));

    switch (header->magic) {
        case MH_MAGIC_64:
            command = shift(command, sizeof(uint32_t));
        case MH_MAGIC:
            break;
        default:
            _assert(false);
    }

    bool ios(false);
    for (decltype(header->ncmds) i(0); i != header->ncmds; ++i) {
        if (command->cmd == LC_VERSION_MIN_IPHONEOS)
            ios = true;
        command = shift(command, command->cmdsize);
    }

    _syscall(munmap(map, size)); // XXX: _scope

    auto length(library.size());
    _assert(length >= 6);
    length -= 6;

    _assert(library.substr(length) == ".dylib");
    library = library.substr(0, length);
    library += ios ? "-sim" : "-sys";
    library += ".dylib";
#endif

    std::ostringstream inject;
    inject << cynject << " " << std::dec << pid << " " << library;
    for (decltype(argc) i(0); i != argc; ++i)
        inject << " " << argv[i];

    _assert(system(inject.str().c_str()) == 0);
}
