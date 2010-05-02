/* Cycript - Inlining/Optimizing JavaScript Compiler
 * Copyright (C) 2009  Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
 *        Redistribution and use in source and binary
 * forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation
 *    and/or other materials provided with the
 *    distribution.
 * 3. The name of the author may not be used to endorse
 *    or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#include <dlfcn.h>
#include <mach/mach.h>

#include <mach/i386/thread_status.h>

#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "Baton.hpp"
#include "Exception.hpp"
#include "Pooling.hpp"
#include "Trampoline.t.hpp"

extern "C" void __pthread_set_self(pthread_t);

void InjectLibrary(pid_t pid) {
    // DOUG: turn this into some kind of -D passed from configure
    const char *library("/usr/lib/libcycript.dylib");

    static const size_t Stack_(8 * 1024);
    size_t length(strlen(library) + 1), depth(sizeof(Baton) + length);
    depth = (depth + sizeof(uintptr_t) + 1) / sizeof(uintptr_t) * sizeof(uintptr_t);

    CYPool pool;
    uint8_t *local(reinterpret_cast<uint8_t *>(apr_palloc(pool, depth)));
    Baton *baton(reinterpret_cast<Baton *>(local));

    baton->__pthread_set_self = &__pthread_set_self;

    baton->pthread_create = &pthread_create;
    baton->pthread_join = &pthread_join;

    baton->dlopen = &dlopen;
    baton->dlsym = &dlsym;

    baton->mach_thread_self = &mach_thread_self;
    baton->thread_terminate = &thread_terminate;

    baton->pid = getpid();
    memcpy(baton->library, library, length);

    vm_size_t size(depth + Stack_);

    mach_port_t self(mach_task_self()), task;
    _krncall(task_for_pid(self, pid, &task));

    vm_address_t stack;
    _krncall(vm_allocate(task, &stack, size, true));
    vm_address_t data(stack + Stack_);

    vm_write(task, data, reinterpret_cast<vm_address_t>(baton), depth);

    thread_act_t thread;
    _krncall(thread_create(task, &thread));

    thread_state_flavor_t flavor;
    mach_msg_type_number_t count;
    size_t push;

    Trampoline *trampoline;

#if defined(__arm__)
    trampoline = &Trampoline_arm_;
    arm_thread_state_t state;
    flavor = ARM_THREAD_STATE;
    count = ARM_THREAD_STATE_COUNT;
    push = 0;
#elif defined(__i386__)
    trampoline = &Trampoline_i386_;
    i386_thread_state_t state;
    flavor = i386_THREAD_STATE;
    count = i386_THREAD_STATE_COUNT;
    push = 5;
#elif defined(__x86_64__)
    trampoline = &Trampoline_x86_64_;
    x86_thread_state64_t state;
    flavor = x86_THREAD_STATE64;
    count = x86_THREAD_STATE64_COUNT;
    push = 2;
#else
    #error XXX: implement
#endif

    vm_address_t code;
    _krncall(vm_allocate(task, &code, trampoline->size_, true));
    vm_write(task, code, reinterpret_cast<vm_address_t>(trampoline->data_), trampoline->size_);
    _krncall(vm_protect(task, code, trampoline->size_, false, VM_PROT_READ | VM_PROT_EXECUTE));

    printf("_ptss:%p\n", baton->__pthread_set_self);
    printf("dlsym:%p\n", baton->dlsym);
    printf("code:%zx\n", (size_t) code);

    uint32_t frame[push];
    if (sizeof(frame) != 0)
        memset(frame, 0, sizeof(frame));
    memset(&state, 0, sizeof(state));

    mach_msg_type_number_t read(count);
    _krncall(thread_get_state(thread, flavor, reinterpret_cast<thread_state_t>(&state), &read));
    _assert(count == count);

#if defined(__arm__)
    state.r[0] = data;
    state.sp = stack + Stack_;
    state.pc = code + trampoline->entry_;

    if ((state.pc & 0x1) != 0) {
        state.pc &= ~0x1;
        state.cpsr |= 0x20;
    }
#elif defined(__i386__)
    frame[1] = data;

    state.__eip = code + trampoline->entry_;
    state.__esp = stack + Stack_ - sizeof(frame);
#elif defined(__x86_64__)
    frame[0] = 0xdeadbeef;
    state.__rdi = data;
    state.__rip = code + trampoline->entry_;
    state.__rsp = stack + Stack_ - sizeof(frame);
#else
    #error XXX: implement
#endif

    if (sizeof(frame) != 0)
        vm_write(task, stack + Stack_ - sizeof(frame), reinterpret_cast<vm_address_t>(frame), sizeof(frame));

    _krncall(thread_set_state(thread, flavor, reinterpret_cast<thread_state_t>(&state), count));
    _krncall(thread_resume(thread));

    _krncall(mach_port_deallocate(self, task));
}
