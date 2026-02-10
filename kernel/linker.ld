/* QAIOS Kernel Linker Script */
/* linker.ld - Limine bootloader compatible (higher-half kernel) */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* Higher-half kernel virtual address - Limine HHDM offset */
KERNEL_VIRT = 0xFFFFFFFF80000000;  /* -2GB (higher half) */

PHDRS
{
    requests PT_LOAD FLAGS(6);  /* Read + Write for Limine requests */
    text    PT_LOAD FLAGS(5);   /* Read + Execute */
    rodata  PT_LOAD FLAGS(4);   /* Read only */
    data    PT_LOAD FLAGS(6);   /* Read + Write */
}

SECTIONS
{
    . = KERNEL_VIRT;
    
    _kernel_start = .;

    /* Limine requests section - must be writable */
    .limine_requests ALIGN(8) : {
        KEEP(*(.limine_requests))
    } :requests
    
    /* Text section */
    . = ALIGN(4K);
    _text_start = .;
    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    } :text
    _text_end = .;
    
    /* Read-only data */
    . = ALIGN(4K);
    _rodata_start = .;
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    } :rodata
    _rodata_end = .;
    
    /* Constructors / Destructors */
    . = ALIGN(8);
    __init_array_start = .;
    .init_array : {
        KEEP(*(.init_array))
        KEEP(*(SORT(.init_array.*)))
    } :rodata
    __init_array_end = .;
    
    . = ALIGN(8);
    __fini_array_start = .;
    .fini_array : {
        KEEP(*(.fini_array))
        KEEP(*(SORT(.fini_array.*)))
    } :rodata
    __fini_array_end = .;
    
    /* Data section */
    . = ALIGN(4K);
    _data_start = .;
    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    } :data
    _data_end = .;
    
    /* BSS section (uninitialized data) */
    . = ALIGN(4K);
    _bss_start = .;
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data
    . = ALIGN(4K);
    _bss_end = .;
    
    _kernel_end = .;
    
    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}

/* Provide symbols for kernel */
PROVIDE(_kernel_size = _kernel_end - _kernel_start);
PROVIDE(_text_size = _text_end - _text_start);
PROVIDE(_rodata_size = _rodata_end - _rodata_start);
PROVIDE(_data_size = _data_end - _data_start);
PROVIDE(_bss_size = _bss_end - _bss_start);
