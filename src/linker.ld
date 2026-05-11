ENTRY(kernel_entry)

SECTIONS
{
    . = 0x8000;  /* Kernel starts at 32KB where bootloader loads it */
    
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }
    
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }
    
    .bss : ALIGN(4K)
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
}
