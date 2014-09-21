/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

/*******************************************************************************
 *
 * Filename:
 * ---------
 * audio_acf_default.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 * This file is the header of audio customization related parameters or definition.
 *
 * Author:
 * -------
 * Tina Tsai
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#ifndef AUDIO_ACF_DEFAULT_H
#define AUDIO_ACF_DEFAULT_H

   /* Compensation Filter HSF coeffs: default all pass filter       */
    /* BesLoudness also uses this coeffs    */ 
#define BES_LOUDNESS_HSF_COEFF \
0x7b78e01,   0xf090e3fd,   0x7b78e01,   0x7b6cc47a,   0x0,     \
0x7b15263,   0xf09d5b39,   0x7b15263,   0x7b06c4dc,   0x0,     \
0x794ac34,   0xf0d6a797,   0x794ac34,   0x792fc69a,   0x0,     \
0x772a3e4,   0xf11ab837,   0x772a3e4,   0x76fbc8a6,   0x0,     \
0x766cc8f,   0xf13266e1,   0x766cc8f,   0x7635c95b,   0x0,     \
0x7310072,   0xf19dff1c,   0x7310072,   0x72a9cc89,   0x0,     \
0x6f26907,   0xf21b2df2,   0x6f26907,   0x6e77d029,   0x0,     \
0x6dcf58c,   0xf24614e7,   0x6dcf58c,   0x6d01d163,   0x0,     \
0x67d8fa1,   0xf304e0bd,   0x67d8fa1,   0x6668d6b6,   0x0,     \
    \
0x7e0e895,   0xf03e2ed5,   0x7e0e895,   0x7e02c1e5,   0x0,     \
0x7de214a,   0xf043bd6b,   0x7de214a,   0x7dd3c20f,   0x0,     \
0x7d12439,   0xf05db78d,   0x7d12439,   0x7cf6c2d1,   0x0,     \
0x7c13d5b,   0xf07d8549,   0x7c13d5b,   0x7be2c3bb,   0x0,     \
0x7bb9584,   0xf088d4f8,   0x7bb9584,   0x7b7fc40c,   0x0,     \
0x7a111c6,   0xf0bddc73,   0x7a111c6,   0x79a4c582,   0x0,     \
0x78078fb,   0xf0ff0e09,   0x78078fb,   0x774ac73a,   0x0,     \
0x774da76,   0xf1164b13,   0x774da76,   0x766ec7d3,   0x0,     \
0x73e4a10,   0xf1836be0,   0x73e4a10,   0x7248ca7f,   0x0

#define BES_LOUDNESS_BPF_COEFF \
0x3c49dba4,   0x34d2245b,   0xcee40000,     \
0x3bfee7de,   0x33f21821,   0xd00e0000,     \
0x3ab41efd,   0x300fe102,   0xd53c0000,     \
0x39405016,   0x2bb2afe9,   0xdb0c0000,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
    \
0x3f96826f,   0x3ec27d90,   0xc1a70000,     \
0x3f8d82b8,   0x3ea67d47,   0xc1cc0000,     \
0x3f628435,   0x3e267bca,   0xc2770000,     \
0x3f2f8657,   0x3d8c79a8,   0xc3440000,     \
0x3f1d872e,   0x3d5678d1,   0xc38c0000,     \
0x3ecb8ba3,   0x3c5e745c,   0xc4d60000,     \
    \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
    \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
    \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
    \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
    \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
    \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0,     \
0x0,   0x0,   0x0

#define BES_LOUDNESS_LPF_COEFF \
0x1bc91bc9,   0x86c,   0x0,     \
0x1da81da8,   0x4ae,   0x0,     \
0x265d265d,   0xf345,   0x0,     \
0x32793279,   0xdb0c,   0x0,     \
0x37cb37cb,   0xd069,   0x0,     \
0x0,   0x0,   0x0
#define BES_LOUDNESS_WS_GAIN_MAX  0x0
#define BES_LOUDNESS_WS_GAIN_MIN  0x0
#define BES_LOUDNESS_FILTER_FIRST  0x0
#define BES_LOUDNESS_ATT_TIME  0x0
#define BES_LOUDNESS_REL_TIME  0x0
#define BES_LOUDNESS_GAIN_MAP_IN \
0xd3, 0xda, 0xed, 0xee, 0x0
#define BES_LOUDNESS_GAIN_MAP_OUT \
0x9, 0x9, 0xf, 0xf, 0x0
#endif
