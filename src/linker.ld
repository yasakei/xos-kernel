/* -------------------------------------------------------------------
   mit license

   copyright (c) 2026 xos

   permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "software"), to deal in the software without
   restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or
   sell copies of the software, and to permit persons to whom the
   software is furnished to do so, subject to the following
   conditions:

   the above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the software.

   the software is provided "as is", without warranty of any kind,
   express or implied, including but not limited to the warranties
   of merchantability, fitness for a particular purpose and
   noninfringement. in no event shall the authors or copyright
   holders be liable for any claim, damages or other liability,
   whether in an action of contract, tort or otherwise, arising
   from, out of or in connection with the software or the use or
   other dealings in the software.
   ------------------------------------------------------------------- */

/* xos kernel linker script
   tells the linker where to place each section in memory */

ENTRY(kernel_entry)

SECTIONS
{
    /* kernel loads at 0x8000 (32kb) - the bootloader puts it here */
    . = 0x8000;

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
