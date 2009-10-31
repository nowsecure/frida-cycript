#include <dlfcn.h>
#include <mach/mach.h>

extern "C" {
#include <mach-o/nlist.h>
}

#include <cstdio>
#include <pthread.h>
#include <unistd.h>

#include "Baton.hpp"
#include "Exception.hpp"
#include "Pooling.hpp"
#include "Trampoline.t.hpp"

extern "C" void _pthread_set_self(pthread_t);

template <typename Type_>
static void nlset(Type_ &function, struct nlist *nl, size_t index) {
    struct nlist &name(nl[index]);
    uintptr_t value(name.n_value);
    if ((name.n_desc & N_ARM_THUMB_DEF) != 0)
        value |= 0x00000001;
    function = reinterpret_cast<Type_>(value);
}

void InjectLibrary(pid_t pid) {
    // XXX: break this into the build environment
    const char *library("/usr/lib/libcycript.dylib");

    static const size_t Stack_(8 * 1024);
    size_t length(strlen(library) + 1), depth(sizeof(Baton) + length);
    depth = (depth + sizeof(uintptr_t) + 1) / sizeof(uintptr_t) * sizeof(uintptr_t);

    CYPool pool;
    uint8_t *local(reinterpret_cast<uint8_t *>(apr_palloc(pool, depth)));
    Baton *baton(reinterpret_cast<Baton *>(local));

    struct nlist nl[2];
    memset(nl, 0, sizeof(nl));
    nl[0].n_un.n_name = (char *) "__pthread_set_self";
    nlist("/usr/lib/libSystem.B.dylib", nl);
    nlset(baton->_pthread_set_self, nl, 0);
    _assert(baton->_pthread_set_self != NULL);

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

    vm_address_t data;
    _krncall(vm_allocate(task, &data, size, true));
    vm_address_t stack(data + depth);
    vm_write(task, data, reinterpret_cast<vm_address_t>(baton), depth);

    vm_address_t code;
    _krncall(vm_allocate(task, &code, sizeof(Trampoline_), true));
    vm_write(task, code, reinterpret_cast<vm_address_t>(Trampoline_), sizeof(Trampoline_));
    _krncall(vm_protect(task, code, sizeof(Trampoline_), false, VM_PROT_READ | VM_PROT_EXECUTE));

    thread_act_t thread;
    _krncall(thread_create(task, &thread));

    thread_state_flavor_t flavor;
    mach_msg_type_number_t count;

#if defined(__arm__)
    arm_thread_state_t state;
    flavor = ARM_THREAD_STATE;
    count = ARM_THREAD_STATE_COUNT;
#else
    #error XXX: implement
#endif

    memset(&state, 0, sizeof(state));

    mach_msg_type_number_t read(count);
    _krncall(thread_get_state(thread, flavor, reinterpret_cast<thread_state_t>(&state), &read));
    _assert(count == count);

#if defined(__arm__)
    state.r[0] = data;
    state.r[1] = RTLD_LAZY | RTLD_GLOBAL;
    state.sp = stack + Stack_;
    state.pc = code;

    if ((state.pc & 0x1) != 0) {
        state.pc &= ~0x1;
        state.cpsr |= 0x20;
    }
#else
    #error XXX: implement
#endif

    _krncall(thread_set_state(thread, flavor, reinterpret_cast<thread_state_t>(&state), count));
    _krncall(thread_resume(thread));

    _krncall(mach_port_deallocate(self, task));
}
