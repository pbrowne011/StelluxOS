OUTPUT_FORMAT(elf64-littleaarch64)
ENTRY(_start)

/* ARM64 starts at 0x40080000 for QEMU virt machine */
KERNEL_OFFSET = 0xffffffff80000000;
KERNEL_START  = 0x40100000;

SECTIONS
{
    . = KERNEL_START;
    
    __ksymstart = .;
    /* Bootstrap section */
    .bootstrap : AT(KERNEL_START)
    {
        *(.text.boot)     /* Specific ARM boot code */
        *(.text.bootstrap)
        *(.data.bootstrap)
        *(.bss.bootstrap)
        . = ALIGN(0x1000);
    }

    /* Higher-half kernel section at 0xffffffff80000000 */
    . += KERNEL_OFFSET;
    .text : AT(ADDR(.text) - KERNEL_OFFSET)
    {
        *(.text)
        . = ALIGN(0x1000);
    }

    .data : AT(ADDR(.data) - KERNEL_OFFSET)
    {
        *(.data)
        . = ALIGN(0x1000);
    }

    .bss : AT(ADDR(.bss) - KERNEL_OFFSET)
    {
        *(COMMON)
        *(.bss)
        . = ALIGN(0x1000);
    }

    /* Privileged Text Section */
    .ktext : AT(ADDR(.ktext) - KERNEL_OFFSET)
    {
        *(.ktext)
        *(.ktext.*)
        . = ALIGN(0x1000);
    }

    /* Privileged Data Section */
    .kdata : AT(ADDR(.kdata) - KERNEL_OFFSET)
    {
        *(.kdata)
        *(.kdata.*)
        . = ALIGN(0x1000);
    }

    /* Privileged Read-Only Data Section */
    .krodata : AT(ADDR(.krodata) - KERNEL_OFFSET)
    {
        *(.krodata)
        *(.krodata.*)
        . = ALIGN(0x1000);
    }

    __per_cpu_start = .;
    .percpu : AT(ADDR(.percpu) - KERNEL_OFFSET) {
        *(.percpu)
    }
    __per_cpu_end = .;
    __per_cpu_size = __per_cpu_end - __per_cpu_start;
    __ksymend = .;
}