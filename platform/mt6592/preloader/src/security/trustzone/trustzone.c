/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 * 
 * MediaTek Inc. (C) 2010. All rights reserved.
 * 
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

/* Include header files */
#include "trustzone.h"
#include "typedefs.h"
#include "platform.h"
#include "dram_buffer.h"
#include "tz_sec_reg.h"
#include "device_apc.h"
#include "tz_mem.h"
#include "sec_devinfo.h"

#if CFG_BOOT_ARGUMENT
#define bootarg g_dram_buf->bootarg
#endif

#define teearg g_dram_buf->teearg

/* platform dependent. configurable. */
#define CFG_TEE_CORE_SIZE  (0x500000)

/**************************************************************************
 *  EXTERNAL FUNCTIONS
 **************************************************************************/
extern void tz_sram_sec_init(void);
extern void tz_sec_mem_init(u32 start, u32 end);
extern void tz_dapc_sec_init(void);
extern void tz_set_module_apc(unsigned int module, E_MASK_DOM domain_num , APC_ATTR permission_control);
extern void tz_boot_share_page_init(void);
extern void cpu_wake_up_forever_wfi(void); //definition in hotplug.s

static u32 tee_secmem_size = 0;
static u32 tee_entry_addr = 0;
static const u8 tee_img_vfy_pubk[MTEE_IMG_VFY_PUBK_SZ] = {MTEE_IMG_VFY_PUBK};

static u32 spm_cpu_wfi[] = {
    MP0_CPU1_STANDBYWFI, MP0_CPU2_STANDBYWFI, MP0_CPU3_STANDBYWFI,
    MP1_CPU0_STANDBYWFI, MP1_CPU1_STANDBYWFI, MP1_CPU2_STANDBYWFI, MP1_CPU3_STANDBYWFI};
static u32 spm_pwr_con[] = {
    SPM_MP0_FC1_PWR_CON, SPM_MP0_FC2_PWR_CON, SPM_MP0_FC3_PWR_CON,
    SPM_MP1_FC0_PWR_CON, SPM_MP1_FC1_PWR_CON, SPM_MP1_FC2_PWR_CON, SPM_MP1_FC3_PWR_CON};
static u32 spm_l1_pdn[]  = {
    SPM_MP0_FC1_L1_PDN, SPM_MP0_FC2_L1_PDN, SPM_MP0_FC3_L1_PDN,
    SPM_MP1_FC0_L1_PDN, SPM_MP1_FC1_L1_PDN, SPM_MP1_FC2_L1_PDN, SPM_MP1_FC3_L1_PDN};
static u32 spm_pwr_sts[] = {
    MP0_FC1, MP0_FC2, MP0_FC3,
    MP1_FC0, MP1_FC1, MP1_FC2, MP1_FC3};

static void spm_mtcmos_pdn_cpu(unsigned int id, int chkWfiBeforePdn)
{
    u32 cpu_wfi, pwr_con, l1_pdn, pwr_sts;

    id--;
    cpu_wfi = spm_cpu_wfi[id];
    pwr_con = spm_pwr_con[id];
    l1_pdn  = spm_l1_pdn[id];
    pwr_sts = spm_pwr_sts[id];

    if (chkWfiBeforePdn)
        while ((READ_REGISTER_UINT32(SPM_SLEEP_TIMER_STA) & cpu_wfi) == 0);

    WRITE_REGISTER_UINT32(pwr_con, READ_REGISTER_UINT32(pwr_con) | SRAM_CKISO);
    WRITE_REGISTER_UINT32(pwr_con, READ_REGISTER_UINT32(pwr_con) & ~SRAM_ISOINT_B);
    WRITE_REGISTER_UINT32(l1_pdn, READ_REGISTER_UINT32(l1_pdn) | L1_PDN);
    while ((READ_REGISTER_UINT32(l1_pdn) & L1_PDN_ACK) != L1_PDN_ACK);

    WRITE_REGISTER_UINT32(pwr_con, READ_REGISTER_UINT32(pwr_con) | PWR_ISO);
    WRITE_REGISTER_UINT32(pwr_con, (READ_REGISTER_UINT32(pwr_con) | PWR_CLK_DIS) & ~PWR_RST_B);

    WRITE_REGISTER_UINT32(pwr_con, READ_REGISTER_UINT32(pwr_con) & ~PWR_ON);
    WRITE_REGISTER_UINT32(pwr_con, READ_REGISTER_UINT32(pwr_con) & ~PWR_ON_S);
    while (((READ_REGISTER_UINT32(SPM_PWR_STATUS) & pwr_sts) != 0) || ((READ_REGISTER_UINT32(SPM_PWR_STATUS_S) & pwr_sts) != 0));
}

static void spm_mtcmos_pdn_cpusys1(int isL2Pdn)
{
    WRITE_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON, READ_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON) | SRAM_CKISO);
    WRITE_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON, READ_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON) & ~SRAM_ISOINT_B);
    if (isL2Pdn)
    {
        WRITE_REGISTER_UINT32(SPM_MP1_L2_DAT_PDN, READ_REGISTER_UINT32(SPM_MP1_L2_DAT_PDN) | L2_SRAM_PDN);
        while ((READ_REGISTER_UINT32(SPM_MP1_L2_DAT_PDN) & L2_SRAM_PDN_ACK) != L2_SRAM_PDN_ACK);
    }
    else
    {
        WRITE_REGISTER_UINT32(SPM_MP1_L2_DAT_SLEEP_B, READ_REGISTER_UINT32(SPM_MP1_L2_DAT_SLEEP_B) & ~L2_SRAM_SLEEP_B);
        while ((READ_REGISTER_UINT32(SPM_MP1_L2_DAT_SLEEP_B) & L2_SRAM_SLEEP_B_ACK) != 0);
    }

    WRITE_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON, READ_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON) | PWR_ISO);
    WRITE_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON, (READ_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);

    WRITE_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON, READ_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON) & ~PWR_ON);
    WRITE_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON, READ_REGISTER_UINT32(SPM_MP1_CPU_PWR_CON) & ~PWR_ON_S);
    while (((READ_REGISTER_UINT32(SPM_PWR_STATUS) & MP1_CPU) != 0) || ((READ_REGISTER_UINT32(SPM_PWR_STATUS_S) & MP1_CPU) != 0));
}

static void tz_set_other_registers(void)
{
    unsigned int value;

    /* ACTLR */
    asm volatile
    (
        "MRC p15, 0, %0, c1, c0, 1\n"
        "BIC %0, %0, #1 << 15\n"        /* DDVM: bit15 */
        "MCR p15, 0, %0, c1, c0, 1\n"
        : "+r"(value)
        :
        : "cc"
    );
    print("%s ACTLR: 0x%x\n", MOD, value);

    /* NSACR */
    asm volatile
    (
        "MRC p15, 0, %0, c1, c1, 2\n"
        : "+r"(value)
        :
        : "cc"
    );
    print("%s NSACR: 0x%x\n", MOD, value);
}

static void tz_sec_brom_pdn(void)
{
    unsigned int i;
    unsigned int value;

    /* enable register control */
    WRITE_REGISTER_UINT32(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

    spm_mtcmos_pdn_cpu(7, 0);
    spm_mtcmos_pdn_cpu(6, 0);
    spm_mtcmos_pdn_cpu(5, 0);
    spm_mtcmos_pdn_cpu(4, 0);

    value = seclib_get_devinfo_with_index(E_AREA3);
    value = (value << 16) >> 28;
    switch (value)
    {
    case 0xF: //mt6588
        spm_mtcmos_pdn_cpusys1(1);
        break;
    case 0xC: //mt6591
    case 0x0: //mt6592
        spm_mtcmos_pdn_cpusys1(0);
        break;
    default:
        break;
    }

    WRITE_REGISTER_UINT32(SLAVE_JUMP_REG, cpu_wake_up_forever_wfi);
    WRITE_REGISTER_UINT32(SLAVE3_MAGIC_REG, SLAVE3_MAGIC_NUM);
    spm_mtcmos_pdn_cpu(3, 1);
    WRITE_REGISTER_UINT32(SLAVE2_MAGIC_REG, SLAVE2_MAGIC_NUM);
    spm_mtcmos_pdn_cpu(2, 1);
    WRITE_REGISTER_UINT32(SLAVE1_MAGIC_REG, SLAVE1_MAGIC_NUM);
    spm_mtcmos_pdn_cpu(1, 1);
    /* CPU0 is reserved for running. Can't power down it */

    print("%s (B)tz_sec_brom_pdn is 0x%x.\n", MOD, READ_REGISTER_UINT32(BOOTROM_PWR_CTRL));
    WRITE_REGISTER_UINT32(BOOTROM_PWR_CTRL, (READ_REGISTER_UINT32(BOOTROM_PWR_CTRL)| 0x80000000));
    print("%s (E)tz_sec_brom_pdn is 0x%x.\n", MOD, READ_REGISTER_UINT32(BOOTROM_PWR_CTRL));
}

static void tz_cci_sec_init(void)
{
    print("%s (B)tz_cci_sec_init is (0x%x) 0x%x.\n", MOD, CCI400_SEC_ACCESS, READ_REGISTER_UINT32(CCI400_SEC_ACCESS));
    WRITE_REGISTER_UINT32(CCI400_SEC_ACCESS, (READ_REGISTER_UINT32(CCI400_SEC_ACCESS)| 0x00000001));
    print("%s (E)tz_cci_sec_init is (0x%x) 0x%x.\n", MOD, CCI400_SEC_ACCESS, READ_REGISTER_UINT32(CCI400_SEC_ACCESS));   
}

static void tee_sec_config(void)
{
    u32 start, end;

    start = tee_entry_addr;
    end   = tee_entry_addr + tee_secmem_size;

    tz_sram_sec_init();

    // disable MPU protect to secure OS secure memory before SQC(for debug)
//#if defined(TRUSTONIC_TEE_SUPPORT) && defined(MTK_SEC_VIDEO_PATH_SUPPORT)
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
    // dose not enable MPU protect to secure memory
#else
    tz_sec_mem_init(start, end);
#endif

    tz_sec_brom_pdn();
    tz_dapc_sec_init();
    tz_set_module_apc(MOD_INFRA_MCU_BIU_CONF, E_DOMAIN_0, E_L1);
    tz_boot_share_page_init();
    tz_cci_sec_init();
}

void tee_set_secmem_size(u32 size)
{
    /* should be larger default TEE DRAM size and less than MAX TEE DRAM size */
    ASSERT(size >= CFG_TEE_DRAM_SIZE && size <= CFG_MAX_TEE_DRAM_SIZE);
    tee_secmem_size = size;
}

void tee_get_secmem_size(u32 *size)
{
    *size = tee_secmem_size;
}

void tee_set_entry(u32 addr)
{
    tee_entry_addr = addr;
}

void tee_set_hwuid(u8 *id, u32 size)
{
    memcpy(teearg.hwuid, id, size);
}

int tee_verify_image(u32 *addr, u32 size)
{
    u32 haddr = *addr; /* tee header address */
    int ret;
    
    /* verify tee image and addr is updated to pointer entry point */
    ret = trustonic_tee_verify(addr, size, tee_img_vfy_pubk);
    if (ret)
        return ret;
    
    ret = trustonic_tee_decrypt(haddr, size);

    return ret;
}
/*
 * addr is NW Jump addr.
 * arg1 is NW Component bootargs.
 * arg2 is NW Component bootargs size
 */
void tee_jump(u32 addr, u32 arg1, u32 arg2)
{
    typedef void (*jump_func_type)(u32 addr ,u32 arg1, u32 arg2, u32 arg3) __attribute__ ((__noreturn__));
    jump_func_type jump_func;
    u32 full_memory_size = 0;
    sec_mem_arg_t sec_mem_arg;
    
    /* Configure platform's security settings */
    tee_sec_config();
    
    /* prepare trustonic's TEE arguments */    
    teearg.magic        = TEE_ARGUMENT_MAGIC;		/* Trustonic's TEE magic number */
    teearg.version      = TEE_ARGUMENT_VERSION;		/* Trustonic's TEE argument block version */
    teearg.NWEntry      = addr;                     /* NW Entry point after t-base */
    teearg.NWBootArgs   = arg1;                     /* NW boot args (propagated by t-base in r4 before jump) */
    teearg.NWBootArgsSize = arg2;                   /* NW boot args size (propagated by t-base in r5 before jump) */
    teearg.dRamBase     = CFG_DRAM_ADDR;			/* DRAM base address */
    teearg.dRamSize     = platform_memory_size();	/* Full DRAM size */
    teearg.secDRamBase  = tee_entry_addr;			/* Secure DRAM base address */    
    teearg.secDRamSize  = CFG_TEE_CORE_SIZE;        /* Secure DRAM size */
    teearg.sRamBase     = 0x00000000;				/* SRAM base address */
    teearg.sRamSize     = 0x00000000;				/* SRAM size */
    teearg.secSRamBase  = 0x00000000;				/* Secure SRAM base address */
    teearg.secSRamSize  = 0x00000000;				/* Secure SRAM size */
    teearg.log_port     = UART0_BASE;				/* UART logging : UART base address. Can be same as preloader's one or not */
    teearg.log_baudrate = bootarg.log_baudrate;		/* UART logging : UART baud rate */
       
    sec_mem_arg.magic = SEC_MEM_MAGIC;
    sec_mem_arg.version = SEC_MEM_VERSION;
    sec_mem_arg.svp_mem_start = tee_entry_addr + CFG_TEE_CORE_SIZE;
    sec_mem_arg.svp_mem_end = tee_entry_addr + tee_secmem_size;
    sec_mem_arg.tplay_table_size = SEC_MEM_TPLAY_TABLE_SIZE;

#if 0
    print("%s sec_mem_arg.magic: 0x%x\n",MOD, sec_mem_arg.magic);
    print("%s sec_mem_arg.version: 0x%x\n",MOD, sec_mem_arg.version);
    print("%s sec_mem_arg.svp_mem_start: 0x%x\n",MOD, sec_mem_arg.svp_mem_start);
    print("%s sec_mem_arg.svp_mem_end: 0x%x\n",MOD, sec_mem_arg.svp_mem_end);
    print("%s sec_mem_arg.tplay_table_size: 0x%x\n",MOD, sec_mem_arg.tplay_table_size);
#endif

    memcpy((void*)TEE_PARAMETER_ADDR, &sec_mem_arg, sizeof(sec_mem_arg_t));    

    /* Last configuration before jump */
    tz_set_other_registers();

    /* Jump to TEE */
    print("%s addr: 0x%x\n",MOD, tee_entry_addr);    
    jump_func = (jump_func_type)tee_entry_addr;
    //(*jump_func)(addr, arg1, arg2, (u32)&teearg);
    (*jump_func)(0, (u32)&teearg, 0, 0);
    // Never return.
}
