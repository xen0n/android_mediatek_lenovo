#include "dram_buffer.h"
#include "typedefs.h"
#include "platform.h"
#include "emi.h"

#define MOD "[DRAMBUF]"
#define RESERVED_DRAM_BUF_SIZE (5 * 1024 * 1024)

dram_buf_t* g_dram_buf = 0;

void init_dram_buffer(){
#if !CFG_FPGA_PLATFORM
    u32 dram_size = platform_memory_size();

    COMPILE_ASSERT(RESERVED_DRAM_BUF_SIZE >= sizeof(dram_buf_t));
    ASSERT(dram_size >= RESERVED_DRAM_BUF_SIZE + CFG_MAX_TEE_DRAM_SIZE);

    print("%s DRAM size: %d\n" ,MOD, dram_size);
    print("%s TEE MAX DRAM size: %d\n" ,MOD, CFG_MAX_TEE_DRAM_SIZE);

    g_dram_buf = (dram_size - RESERVED_DRAM_BUF_SIZE - CFG_MAX_TEE_DRAM_SIZE) + CFG_DRAM_ADDR;
#else
    g_dram_buf = 0xC00000 + CFG_DRAM_ADDR;
#endif
}

