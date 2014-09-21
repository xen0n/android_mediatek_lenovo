#include <stddef.h>

#ifndef _TZ_MEM_H_
#define _TZ_MEM_H_

#define SRAM_BASE_ADDRESS   0x00100000
#define SRAM_START_ADDR     0x00102140
#define VECTOR_START        (SRAM_START_ADDR + 0xBAC0)

typedef struct tz_memory_t {
    short next, previous;
} tz_memory_t;

#define FREE            ((short)(0x0001))
#define IS_FREE(x)      ((x)->next & FREE)
#define CLEAR_FREE(x)   ((x)->next &= ~FREE)
#define SET_FREE(x)     ((x)->next |= FREE)
#define FROM_ADDR(x)    ((short)(ptrdiff_t)(x))
#define TO_ADDR(x)      ((tz_memory_t *)(SRAM_BASE_ADDRESS + ((x) & ~FREE)))

/* SEC MEM magic */
#define SEC_MEM_MAGIC                   (0x3C562817U)
/* SEC MEM version */
#define SEC_MEM_VERSION                 (0x00010000U)
/* Tplay Table Size */
#define SEC_MEM_TPLAY_TABLE_SIZE        (0x1000)//4KB by default

#define TEE_PARAMETER_ADDR              (0x100100)

typedef struct {
    unsigned int magic;           // Magic number
    unsigned int version;         // version
    unsigned int svp_mem_start;   // MM sec mem pool start addr.
    unsigned int svp_mem_end;     // MM sec mem pool end addr.
    unsigned int tplay_table_size;  //tplay handle-to-physical table size, its start address should be (svp_mem_end - tplay_table_size);
} sec_mem_arg_t;

#endif