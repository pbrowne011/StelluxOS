.section .text.bootstrap
.global _start

_start:
    // Clear x0-x30
    mov     x0, xzr
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr

    // Get current Exception Level into x0 (bits 2:3)
    mrs     x0, CurrentEL
    cmp     x0, #(1 << 2)    // Check if we're in EL2
    b.eq    .Lsetup_stack   // If not in EL2, skip EL2->EL1 transition

    // EL2 to EL1 transition
    msr     sctlr_el1, xzr   // Clear system control register
    mov     x0, #(1 << 31)   // Enable AArch64 for EL1
    msr     hcr_el2, x0
    
    mov     x0, #0x3c5       // EL1h with interrupts masked
    msr     spsr_el2, x0
    
    adr     x0, .Lsetup_stack  // Get address to return to
    msr     elr_el2, x0
    eret

.Lsetup_stack:
    // Get stack address and align it
    adr     x0, _start
    and     x0, x0, #~15    // 16-byte alignment
    add     x0, x0, #4096   // Add some space
    mov     sp, x0

/*
 * Calculate a specific arithmetic and bitwise operation sequence, storing the final result in x4 based
 * on the initial value 42.
 */
    mov x0, #42        // Load immediate value 42 into x0
    add x1, x0, #1     // Add 1 to x0 and store in x1
    lsl x2, x1, #2     // Logical shift left x1 by 2 and store in x2
    and x3, x2, #0xFF  // Bitwise AND x2 with 0xFF and store in x3
    sub x4, x3, #10    // Subtract 10 from x3 and store in x4

1:  // Simple spin loop
    wfe                     // Wait for event
    b       1b             // Branch backward