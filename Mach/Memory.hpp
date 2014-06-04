#ifndef MACH_MEMORY_HPP
#define MACH_MEMORY_HPP

static kern_return_t cy_vm_allocate(bool broken, vm_map_t target, mach_vm_address_t *address, mach_vm_size_t size, int flags) {
    if (!broken)
        return mach_vm_allocate(target, address, size, flags);
    vm_address_t address32(0);
    kern_return_t value(vm_allocate(target, &address32, size, flags));
    *address = address32;
    return value;
}

#define mach_vm_allocate(a, b, c, d) \
    cy_vm_allocate(broken, a, b, c, d)

static kern_return_t cy_vm_deallocate(bool broken, vm_map_t target, mach_vm_address_t address, mach_vm_size_t size) {
    if (!broken)
        return mach_vm_deallocate(target, address, size);
    return vm_deallocate(target, address, size);
}

#define mach_vm_deallocate(a, b, c) \
    cy_vm_deallocate(broken, a, b, c)

static kern_return_t cy_vm_protect(bool broken, vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection) {
    if (!broken)
        return mach_vm_protect(target_task, address, size, set_maximum, new_protection);
    return vm_protect(target_task, address, size, set_maximum, new_protection);
}

#define mach_vm_protect(a, b, c, d, e) \
    cy_vm_protect(broken, a, b, c, d, e)

static kern_return_t cy_vm_read_overwrite(bool broken, vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data, mach_vm_size_t *outsize) {
    if (!broken)
        return mach_vm_read_overwrite(target_task, address, size, data, outsize);
    vm_size_t outsize32(*outsize);
    kern_return_t value(vm_read_overwrite(target_task, address, data, size, &outsize32));
    *outsize = outsize32;
    return value;
}

#define mach_vm_read_overwrite(a, b, c, d, e) \
    cy_vm_read_overwrite(broken, a, b, c, d, e)

static kern_return_t cy_vm_write(bool broken, vm_map_t target_task, mach_vm_address_t address, vm_offset_t data, mach_msg_type_number_t dataCnt) {
    if (!broken)
        return mach_vm_write(target_task, address, data, dataCnt);
    return vm_write(target_task, address, data, dataCnt);
}

#define mach_vm_write(a, b, c, d) \
    cy_vm_write(broken, a, b, c, d)

#endif//MACH_MEMORY_HPP
