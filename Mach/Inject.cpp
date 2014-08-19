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

#include "TargetConditionals.h"
#if TARGET_OS_IPHONE
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#define __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ __IPHONE_5_0
#endif

#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach/mach_vm.h>

#include <mach/machine/thread_status.h>

#ifdef __arm__
#include "Mach/Memory.hpp"
#endif

#include "Baton.hpp"
#include "Exception.hpp"
#include "Pooling.hpp"
#include "Trampoline.t.hpp"

extern "C" void CYHandleServer(pid_t);

extern "C" void *_dyld_get_all_image_infos();

void InjectLibrary(pid_t pid) {
    Dl_info addr;
    _assert(dladdr(reinterpret_cast<void *>(&CYHandleServer), &addr) != 0);

    size_t flength(strlen(addr.dli_fname));
    char library[flength + 4 + 1];
    memcpy(library, addr.dli_fname, flength);
    library[flength] = '\0';
    _assert(strcmp(library + flength - 6, ".dylib") == 0);
#if !TARGET_OS_IPHONE
    strcpy(library + flength - 6, "-###.dylib");
#endif

    mach_port_t self(mach_task_self()), task;
    _krncall(task_for_pid(self, pid, &task));

    task_dyld_info info;
#ifdef __arm__
    union {
        struct {
            uint32_t all_image_info_addr;
        } info_1;

        struct {
            uint32_t all_image_info_addr;
            uint32_t all_image_info_size;
            int32_t all_image_info_format;
        } info32;

        struct {
            uint64_t all_image_info_addr;
            uint64_t all_image_info_size;
            int32_t all_image_info_format;
        } info64;
    } infoXX;

    mach_msg_type_number_t count(sizeof(infoXX) / sizeof(natural_t));
    _krncall(task_info(task, TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&infoXX), &count));

    bool broken;

    switch (count) {
        case sizeof(infoXX.info_1) / sizeof(natural_t):
            broken = true;
            info.all_image_info_addr = infoXX.info_1.all_image_info_addr;
            info.all_image_info_size = 0;
            info.all_image_info_format = TASK_DYLD_ALL_IMAGE_INFO_32;
            break;
        case sizeof(infoXX.info32) / sizeof(natural_t):
            broken = true;
            info.all_image_info_addr = infoXX.info32.all_image_info_addr;
            info.all_image_info_size = infoXX.info32.all_image_info_size;
            info.all_image_info_format = infoXX.info32.all_image_info_format;
            break;
        case sizeof(infoXX.info64) / sizeof(natural_t):
            broken = false;
            info.all_image_info_addr = infoXX.info64.all_image_info_addr;
            info.all_image_info_size = infoXX.info64.all_image_info_size;
            info.all_image_info_format = infoXX.info64.all_image_info_format;
            break;
        default:
            _assert(false);
    }
#else
    mach_msg_type_number_t count(TASK_DYLD_INFO_COUNT);
    _krncall(task_info(task, TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&info), &count));
    _assert(count == TASK_DYLD_INFO_COUNT);
#endif
    _assert(info.all_image_info_addr != 0);

    thread_act_t thread;
    _krncall(thread_create(task, &thread));

    thread_state_t bottom;
    thread_state_flavor_t flavor;

#if defined (__i386__) || defined(__x86_64__)
    x86_thread_state_t state;
    memset(&state, 0, sizeof(state));

    bottom = reinterpret_cast<thread_state_t>(&state);
    flavor = MACHINE_THREAD_STATE;
    count = MACHINE_THREAD_STATE_COUNT;
#elif defined(__arm__) || defined(__arm64__)
    arm_unified_thread_state_t state;
    memset(&state, 0, sizeof(state));

    switch (info.all_image_info_format) {
        case TASK_DYLD_ALL_IMAGE_INFO_32:
            bottom = reinterpret_cast<thread_state_t>(&state.ts_32);
            flavor = ARM_THREAD_STATE;
            count = ARM_THREAD_STATE_COUNT;
            state.ash.flavor = ARM_THREAD_STATE32;
            break;
        case TASK_DYLD_ALL_IMAGE_INFO_64:
            bottom = reinterpret_cast<thread_state_t>(&state.ts_64);
            flavor = ARM_THREAD_STATE64;
            count = ARM_THREAD_STATE64_COUNT + 1;
            state.ash.flavor = ARM_THREAD_STATE64;
            break;
        default:
            _assert(false);
    }
#else
    #error XXX: implement
#endif

    mach_msg_type_number_t read(count);
    _krncall(thread_get_state(thread, flavor, bottom, &read));
    _assert(read == count);

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
#elif defined(__arm__) || defined(__arm64__)
    switch (state.ash.flavor) {
        case ARM_THREAD_STATE32:
            trampoline = &Trampoline_armv6_;
            align = 4;
            push = 0;
            break;
        case ARM_THREAD_STATE64:
            trampoline = &Trampoline_arm64_;
            align = 8;
            push = 0;
            break;
        default:
            _assert(false);
    }
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
    memset(baton->error, 0, sizeof(baton->error));
    memcpy(baton->library, library, length);

    mach_vm_size_t size(depth + Stack_);
    mach_vm_address_t stack;
    _krncall(mach_vm_allocate(task, &stack, size, true));

    mach_vm_address_t data(stack + Stack_);
    _krncall(mach_vm_write(task, data, reinterpret_cast<mach_vm_address_t>(baton), depth));

    mach_vm_address_t code;
    _krncall(mach_vm_allocate(task, &code, trampoline->size_, true));
    _krncall(mach_vm_write(task, code, reinterpret_cast<vm_offset_t>(trampoline->data_), trampoline->size_));
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
#elif defined(__arm__) || defined(__arm64__)
    switch (state.ash.flavor) {
        case ARM_THREAD_STATE32:
            state.ts_32.__r[0] = data;
            state.ts_32.__pc = code + trampoline->entry_;
            state.ts_32.__sp = stack + Stack_ - sizeof(frame);

            if ((state.ts_32.__pc & 0x1) != 0) {
                state.ts_32.__pc &= ~0x1;
                state.ts_32.__cpsr |= 0x20;
            }

            break;

        case ARM_THREAD_STATE64:
            state.ts_64.__x[0] = data;
            state.ts_64.__pc = code + trampoline->entry_;
            state.ts_64.__sp = stack + Stack_ - sizeof(frame);
            break;

        default:
            _assert(false);
    }
#else
    #error XXX: implement
#endif

    if (sizeof(frame) != 0)
        _krncall(mach_vm_write(task, stack + Stack_ - sizeof(frame), reinterpret_cast<mach_vm_address_t>(frame), sizeof(frame)));

    _krncall(thread_set_state(thread, flavor, bottom, read));
    _krncall(thread_resume(thread));

    loop: switch (kern_return_t status = thread_get_state(thread, flavor, bottom, &(read = count))) {
        case KERN_SUCCESS:
            usleep(10000);
            goto loop;

        case KERN_TERMINATED:
        case MACH_SEND_INVALID_DEST:
            break;

        default:
            _assert(false);
    }

    _krncall(mach_port_deallocate(self, thread));

    mach_vm_size_t error(sizeof(baton->error));
    _krncall(mach_vm_read_overwrite(task, data + offsetof(Baton, error), sizeof(baton->error), reinterpret_cast<mach_vm_address_t>(&baton->error), &error));
    _assert(error == sizeof(baton->error));

    if (baton->error[0] != '\0') {
        baton->error[sizeof(baton->error) - 1] = '\0';
        CYThrow("%s", baton->error);
    }

    _krncall(mach_vm_deallocate(task, code, trampoline->size_));
    _krncall(mach_vm_deallocate(task, stack, size));

    _krncall(mach_port_deallocate(self, task));
}
