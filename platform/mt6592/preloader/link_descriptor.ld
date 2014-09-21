OUTPUT_ARCH(arm)

ENTRY(_start)

romBase = 0x00200400;
ramBase = 0x00102180;
dramBase = 0x87B00000;



SECTIONS {

    . = romBase;
    .start ALIGN(4) : {	
        *(.text.start)
    } 
    
    . = romBase + 0x01FC;
    .rom_info ALIGN(4) : {	    	    	    	     
    	*(.data.rom_info)
    } 

    .text ALIGN(4) : {
        *(.text)
        *(.text.*)        
    } 

    .rodata ALIGN(4) : {
        *(.rodata)
        *(.rodata.*)        
    } 

    .data ALIGN(4) : {
        *(.data)
        *(.data.*)        
    } 

    .got ALIGN(4) : {
        *(.got)
        *(.got.*)        
    } 

    __boot_end = .;

    . = ramBase;
    .bss ALIGN(16) : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } 
	.secbuf ALIGN(4) : {
		_secbuf_start = .;
		*(.secbuf)
		_secbuf_end = .;
	}

}
