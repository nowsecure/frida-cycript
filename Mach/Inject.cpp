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

#include <dlfcn.h>

#include <mach/mach.h>

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#ifdef TARGET_OS_IPHONE
#include <mach/vm_map.h>
#define mach_vm_allocate vm_allocate
#define mach_vm_protect vm_protect
#define mach_vm_write vm_write
#define mach_vm_address_t vm_address_t
#else
#include <mach/mach_vm.h>
#endif

#include <mach/machine/thread_status.h>

#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "Baton.hpp"
#include "Exception.hpp"
#include "Pooling.hpp"
#include "Trampoline.t.hpp"

extern "C" void CYHandleServer(pid_t);

void InjectLibrary(pid_t pid) {
    Dl_info addr;
    _assert(dladdr(reinterpret_cast<void *>(&CYHandleServer), &addr) != 0);

    size_t flength(strlen(addr.dli_fname));
    char library[flength + 4 + 1];
    memcpy(library, addr.dli_fname, flength);
    library[flength] = '\0';
    _assert(strcmp(library + flength - 6, ".dylib") == 0);
    strcpy(library + flength - 6, "-any.dylib");

    mach_port_t self(mach_task_self()), task;
    _krncall(task_for_pid(self, pid, &task));

    mach_msg_type_number_t count;

    task_dyld_info info;
    count = TASK_DYLD_INFO_COUNT;
    _krncall(task_info(task, TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&info), &count));
    _assert(count == TASK_DYLD_INFO_COUNT);
    _assert(info.all_image_info_addr != 0);

    thread_act_t thread;
    _krncall(thread_create(task, &thread));

#if defined (__i386__) || defined(__x86_64__)
    x86_thread_state_t state;
#elif defined(__arm__)
    arm_thread_state_t state;
#else
    #error XXX: implement
#endif

    memset(&state, 0, sizeof(state));
    mach_msg_type_number_t read(MACHINE_THREAD_STATE_COUNT);
    _krncall(thread_get_state(thread, MACHINE_THREAD_STATE, reinterpret_cast<thread_state_t>(&state), &read));
    _assert(read == MACHINE_THREAD_STATE_COUNT);

    Trampoline *trampoline;
    size_t align;
    size_t push;

#if defined(__i386__) || defined(__x86_64__)
    switch (state.tsh.flavor) {
        case i386_THREAD_STATE:
            trampoline = &Trampoline_i386_;
            align = 4;
            push = 5;
            break;
        case x86_THREAD_STATE64:
            trampoline = &Trampoline_x86_64_;
            align = 8;
            push = 2;
            break;
        default:
            _assert(false);
    }
#elif defined(__arm__)
    trampoline = &Trampoline_armv6_;
    align = 4;
    push = 0;
#else
    #error XXX: implement
#endif

    static const size_t Stack_(8 * 1024);
    size_t length(strlen(library) + 1), depth(sizeof(Baton) + length);
    depth = (depth + align + 1) / align * align;

    CYPool pool;
    uint8_t *local(pool.malloc<uint8_t>(depth));
    Baton *baton(reinterpret_cast<Baton *>(local));

    baton->dyld = info.all_image_info_addr;
    baton->pid = getpid();
    memcpy(baton->library, library, length);

    mach_vm_size_t size(depth + Stack_);
    mach_vm_address_t stack;
    _krncall(mach_vm_allocate(task, &stack, size, true));

    mach_vm_address_t data(stack + Stack_);
    _krncall(mach_vm_write(task, data, reinterpret_cast<mach_vm_address_t>(baton), depth));

    mach_vm_address_t code;
    _krncall(mach_vm_allocate(task, &code, trampoline->size_, true));
    _krncall(mach_vm_write(task, code, reinterpret_cast<mach_vm_address_t>(trampoline->data_), trampoline->size_));
    _krncall(mach_vm_protect(task, code, trampoline->size_, false, VM_PROT_READ | VM_PROT_EXECUTE));

    uint32_t frame[push];
    if (sizeof(frame) != 0)
        memset(frame, 0, sizeof(frame));

#if defined(__i386__) || defined(__x86_64__)
    switch (state.tsh.flavor) {
        case i386_THREAD_STATE:
            frame[1] = data;
            state.uts.ts32.__eip = code + trampoline->entry_;
            state.uts.ts32.__esp = stack + Stack_ - sizeof(frame);
            break;
        case x86_THREAD_STATE64:
            state.uts.ts64.__rdi = data;
            state.uts.ts64.__rip = code + trampoline->entry_;
            state.uts.ts64.__rsp = stack + Stack_ - sizeof(frame);
            break;
        default:
            _assert(false);
    }
#elif defined(__arm__)
    state.__r[0] = data;
    state.__pc = code + trampoline->entry_;
    state.__sp = stack + Stack_ - sizeof(frame);

    if ((state.__pc & 0x1) != 0) {
        state.__pc &= ~0x1;
        state.__cpsr |= 0x20;
    }
#else
    #error XXX: implement
#endif

    if (sizeof(frame) != 0)
        _krncall(mach_vm_write(task, stack + Stack_ - sizeof(frame), reinterpret_cast<mach_vm_address_t>(frame), sizeof(frame)));

    _krncall(thread_set_state(thread, MACHINE_THREAD_STATE, reinterpret_cast<thread_state_t>(&state), MACHINE_THREAD_STATE_COUNT));
    _krncall(thread_resume(thread));

    _krncall(mach_port_deallocate(self, task));
}
