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

/******************************************************************************
*
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2001
*
*******************************************************************************/
#include "typedefs.h"
#include "nand_core.h"
#include "nand.h"
#include "platform.h"
#include "cust_part.h"
#include "pmt.h"
#include "dram_buffer.h"
#include "mmc_core.h"


#if CFG_PMT_SUPPORT

#define lastest_part g_dram_buf->lastest_part


#ifdef MTK_EMMC_SUPPORT

#define EMMC_PMT_BUFFER_SIZE             (0x20000)
#define emmc_pmt_buf g_dram_buf->emmc_pmt_buf

#define PMT_REGION_SIZE     (0x1000)   //4K
#define PMT_REGION_OFFSET   (0x100000) //1M

#define PMT_VER_V1P1        ("1.1")
#define PMT_VER_SIZE        (4)

#define PAGE_SIZE           (512)

extern u64 g_emmc_user_size;

static int load_pt(u8 *buf)
{
    int reval = ERR_NO_EXIST;
    u64 pt_start;
    u64 mpt_start;
	int pt_size = PMT_REGION_SIZE;
    int buffer_size = pt_size;
    u8 *pmt_buf = (u8 *)emmc_pmt_buf;
    blkdev_t *dev = blkdev_get(BOOTDEV_SDMMC);

    pt_start = g_emmc_user_size - PMT_REGION_OFFSET;
    mpt_start = pt_start + PMT_REGION_SIZE;

    printf("============func=%s===scan pmt from %llx=====\n", __func__, pt_start);

    /* try to find the pmt at fixed address, signature:0x50547631 */
    dev->bread(dev, (u32)(pt_start / 512), buffer_size / 512, (u8 *)pmt_buf, EMMC_PART_USER);
	if (is_valid_pt(pmt_buf)) {
        if (!memcmp(pmt_buf + PT_SIG_SIZE, PMT_VER_V1P1, PMT_VER_SIZE)) {
            if (is_valid_pt(&pmt_buf[pt_size - PT_SIG_SIZE])) {
                printf("find pt at %llx\n", pt_start);
		        memcpy(buf, pmt_buf + PT_SIG_SIZE + PMT_VER_SIZE, PART_MAX_COUNT * sizeof(pt_resident));
		        reval = DM_ERR_OK;
                return reval;
            } else {
                printf("invalid tail pt format\n");
                reval = ERR_NO_EXIST;
            }
        } else {
            printf("invalid pt version %s\n", pmt_buf + PT_SIG_SIZE);
            reval = ERR_NO_EXIST;
        }
    }

	dev->bread(dev, (u32)(mpt_start / 512), buffer_size / 512, (u8 *)pmt_buf, EMMC_PART_USER);
	if (is_valid_mpt(pmt_buf)) {
        if (!memcmp(pmt_buf + PT_SIG_SIZE, PMT_VER_V1P1, PMT_VER_SIZE)) {
            if (is_valid_mpt(&pmt_buf[pt_size - PT_SIG_SIZE])) {
		        memcpy(buf, pmt_buf + PT_SIG_SIZE + PMT_VER_SIZE, PART_MAX_COUNT * sizeof(pt_resident));
		        reval = DM_ERR_OK;
                printf("find mpt at %llx\n", mpt_start);
                return reval;
            } else {
                reval = ERR_NO_EXIST;
                printf("invalid tail mpt format\n");
            }
        } else {
            reval = ERR_NO_EXIST;
            printf("invalid mpt version %s\n", pmt_buf + PT_SIG_SIZE);
        }
	}

	return reval;
}

static void get_part_tab_from_complier(void)
{
	int index = 0;
	part_t *part = cust_part_tbl();
	MSG(INIT, "get_pt_from_complier\n");
	while (part->flags != PART_FLAG_END) {
		memcpy(lastest_part[index].name, part->name, MAX_PARTITION_NAME_LEN);
		lastest_part[index].size = (u64) part->blks * 512;
		lastest_part[index].part_id = (u64) part->part_id;
		lastest_part[index].offset = (u64) part->startblk * 512;
		lastest_part[index].mask_flags = PART_FLAG_NONE;	//this flag in kernel should be fufilled even though in flash is 0.
		print("get_ptr %s %llx\n", lastest_part[index].name, lastest_part[index].offset);
		index++;
		part++;
	}
}

#else   /* !MTK_EMMC_SUPPORT */

static pt_info pi;
static u8 sig_buf[PT_SIG_SIZE];

extern u32 BLOCK_SIZE;
extern u32 PAGE_SIZE;
extern struct nand_chip g_nand_chip;
extern unsigned char g_nand_spare[64];

extern int mtk_nand_write_page_hwecc(unsigned int logical_addr, char *buf);
extern int mtk_nand_read_page_hwecc(unsigned int logical_addr, char *buf);

static int total_size;

#define LPAGE 4096

char page_buf[LPAGE + 128];
char page_readbuf[LPAGE];

#define pmt_dat_buf g_dram_buf->pmt_dat_buf
#define pmt_read_buf g_dram_buf->pmt_read_buf


bool find_mirror_pt_from_bottom(int *start_addr)
{
	int mpt_locate;
	int mpt_start_addr = total_size + BLOCK_SIZE;//MPT_LOCATION*BLOCK_SIZE-PAGE_SIZE;
	int current_start_addr = 0;
	char pmt_spare[4];
	
	for (mpt_locate = (BLOCK_SIZE/PAGE_SIZE); mpt_locate > 0; mpt_locate--) {
		memset(pmt_spare, 0xFF, PT_SIG_SIZE);
		memset(g_nand_spare, 0xFF, 64);
		current_start_addr = mpt_start_addr + mpt_locate * PAGE_SIZE;
		if (!mtk_nand_read_page_hwecc(current_start_addr, page_readbuf)) {
			MSG(INIT, "find_mirror read  failed %x %x \n",current_start_addr, mpt_locate);
		}
		memcpy(pmt_spare, &g_nand_spare[1], PT_SIG_SIZE);
		//need enhance must be the larget sequnce number
		#if 0
		{
			int i;
			for (i=0;i<4;i++)
			MSG(INIT, " %x %x \n",page_readbuf[i],g_nand_spare[1+i]);	
			MSG(INIT,"%x %x " , *((u32*)page_readbuf), *((u32*)pmt_spare));
			MSG(INIT,"%x " , is_valid_mpt(page_readbuf));
			MSG(INIT,"%x \n" , is_valid_mpt(pmt_spare));
		}
		#endif
		if (is_valid_mpt(page_readbuf) && is_valid_mpt(&pmt_spare)) {
		      //if no pt, pt.has space is 0;
			pi.sequencenumber = g_nand_spare[5];
			MSG(INIT, "find_mirror find valid pt at %x sq %x \n",current_start_addr,pi.sequencenumber);
			break;
		} else {
			continue;
		}
	}
	if (mpt_locate==0) {
		MSG(INIT, "no valid mirror page\n");
		pi.sequencenumber = 0;
		return FALSE;
	} else {
		*start_addr = current_start_addr;
		return TRUE;
	}
}

static int load_pt(u8 *buf)
{
	int pt_start_addr;
	int pt_cur_addr;
	int pt_locate;
	int reval = DM_ERR_OK;
	int mirror_address;
	char pmt_spare[PT_SIG_SIZE];
	
	PAGE_SIZE = (u32)g_nand_chip.page_size;
	BLOCK_SIZE = (u32)g_nand_chip.erasesize;
	total_size = (int)g_nand_chip.chipsize;
	
	pt_start_addr = total_size;

	MSG(INIT, "load_pt from %x\n", pt_start_addr);
	memset(&pi, 0xFF, sizeof(pi));
	for (pt_locate = 0; pt_locate < (BLOCK_SIZE / PAGE_SIZE); pt_locate++) {
		pt_cur_addr = pt_start_addr + pt_locate * PAGE_SIZE;
		memset(pmt_spare, 0xFF, PT_SIG_SIZE);
		memset(g_nand_spare, 0xFF, 64);
		if (!mtk_nand_read_page_hwecc(pt_cur_addr, page_readbuf)) {
			MSG(INIT, "load_pt read pt failded\n");
		}
		memcpy(pmt_spare, &g_nand_spare[1] ,PT_SIG_SIZE);
		if (is_valid_pt(page_readbuf) && is_valid_pt(pmt_spare)) {
			pi.sequencenumber = g_nand_spare[5];
			MSG(INIT, "load_pt find valid pt at %x sq %x \n", pt_start_addr, pi.sequencenumber);
			break;
		} else {
			continue;
		}
	}

	if (pt_locate == (BLOCK_SIZE/PAGE_SIZE)) {
		//first download or download is not compelte after erase or can not download last time
		MSG(INIT, "load_pt find pt failed\n");
		pi.pt_has_space = 0; //or before download pt power lost
		
		if (!find_mirror_pt_from_bottom(&mirror_address)) {
			MSG(INIT, "First time download \n");
			reval=ERR_NO_EXIST;
			return reval;
		} else {
			//used the last valid mirror pt, at lease one is valid.
			mtk_nand_read_page_hwecc(mirror_address, page_readbuf);
		}
	}
	memcpy(buf,&page_readbuf[PT_SIG_SIZE],sizeof(lastest_part));

	return reval;
}


static void get_part_tab_from_complier(void)
{
	int index = 0;
	part_t *part = cust_part_tbl();
	MSG(INIT, "get_pt_from_complier\n");
	while (part->flags != PART_FLAG_END) {
		memcpy(lastest_part[index].name, part->name, MAX_PARTITION_NAME_LEN);
		lastest_part[index].size = part->blks * PAGE_SIZE;
		lastest_part[index].offset = part->startblk * PAGE_SIZE;
		lastest_part[index].mask_flags = PART_FLAG_NONE;  //this flag in kernel should be fufilled even though in flash is 0.
		MSG(INIT, "get_ptr %s %x\n", lastest_part[index].name, lastest_part[index].offset);
		index++;
		part++;
	}
}

#endif


void pmt_init(void)
{
	int ret = 0;
    int i = 0;

	memset(&lastest_part, 0, PART_MAX_COUNT * sizeof(pt_resident));

	ret = load_pt((u8 *)&lastest_part);
    if (ret == ERR_NO_EXIST) { 
        //first run preloader before dowload
		//and valid mirror last download or first download 
		get_part_tab_from_complier();	//get from complier
	} else {
		for (i = 0; i < PART_MAX_COUNT; i++) {
			if (lastest_part[i].size == 0) {
				break;
            }
			print("part %s size %llx %llx\n", lastest_part[i].name,
				  lastest_part[i].offset, lastest_part[i].size);
		}
	}
}


static part_t tempart;

static inline u32 PAGE_NUM(u32 logical_size)
{
	return ((unsigned long) (logical_size) / PAGE_SIZE);
}

part_t *pmt_get_part(part_t *part, int index)
{
    if (index >= PART_MAX_COUNT) {
        return NULL;
    }

    tempart.name = part->name;
    tempart.startblk = PAGE_NUM(lastest_part[index].offset);
    tempart.blks = PAGE_NUM(lastest_part[index].size);
    tempart.flags=part->flags;
    tempart.size=part->size;
    return &tempart;
}

#else   /* !CFG_PMT_SUPPORT */

void pmt_init(void) { }
part_t *pmt_get_part(part_t *part, int index) { return NULL; }

#endif
