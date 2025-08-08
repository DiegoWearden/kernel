.text
.align 4
.global context_switch
// void context_switch(uint64_t** old_sp, uint64_t* new_sp)
context_switch:
    // x0 = &old_sp, x1 = new_sp
    // Save callee-saved registers x19-x29 and lr to current stack
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    // Save current sp to *old_sp
    mov x2, sp
    str x2, [x0]
    // Switch to new stack
    mov sp, x1
    // Restore callee-saved
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret

.global thread_trampoline
// After context restore, SP points to [arg][entry] placed by make_stack
thread_trampoline:
    ldr x0, [sp]        // arg
    ldr x1, [sp, #8]    // entry
    blr x1
1:  wfe
    b 1b 