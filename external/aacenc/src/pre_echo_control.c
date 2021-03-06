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

/*
 ** Copyright 2003-2010, VisualOn, Inc.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */
/*******************************************************************************
	File:		pre_echo_control.c

	Content:	Pre echo control functions

*******************************************************************************/

#include "basic_op.h"
#include "oper_32b.h"

#include "oper_32b.h"
#include "pre_echo_control.h"


/*****************************************************************************
*
* function name:InitPreEchoControl
* description: init pre echo control parameter
*
*****************************************************************************/
void InitPreEchoControl(Word32 *pbThresholdNm1,
                        Word16  numPb,
                        Word32 *pbThresholdQuiet)
{
  Word16 pb;

  for(pb=0; pb<numPb; pb++) {
    pbThresholdNm1[pb] = pbThresholdQuiet[pb];
  }
}

/*****************************************************************************
*
* function name:PreEchoControl
* description: update shreshold to avoid pre echo
*			   thr(n) = max(rpmin*thrq(n), min(thrq(n), rpelev*thrq1(n)))
*
*
*****************************************************************************/
void PreEchoControl(Word32 *pbThresholdNm1,
                    Word16  numPb,
                    Word32  maxAllowedIncreaseFactor,
                    Word16  minRemainingThresholdFactor,
                    Word32 *pbThreshold,
                    Word16  mdctScale,
                    Word16  mdctScalenm1)
{
  Word32 i;
  Word32 tmpThreshold1, tmpThreshold2;
  Word32 scaling;

  /* maxAllowedIncreaseFactor is hard coded to 2 */
  (void)maxAllowedIncreaseFactor;

  scaling = ((mdctScale - mdctScalenm1) << 1);

  if ( scaling > 0 ) {
    for(i = 0; i < numPb; i++) {
      tmpThreshold1 = pbThresholdNm1[i] >> (scaling-1);
      tmpThreshold2 = L_mpy_ls(pbThreshold[i], minRemainingThresholdFactor);

      /* copy thresholds to internal memory */
      pbThresholdNm1[i] = pbThreshold[i];


      if(pbThreshold[i] > tmpThreshold1) {
        pbThreshold[i] = tmpThreshold1;
      }

      if(tmpThreshold2 > pbThreshold[i]) {
        pbThreshold[i] = tmpThreshold2;
      }

    }
  }
  else {
    scaling = -scaling;
    for(i = 0; i < numPb; i++) {

      tmpThreshold1 = pbThresholdNm1[i] << 1;
      tmpThreshold2 = L_mpy_ls(pbThreshold[i], minRemainingThresholdFactor);

      /* copy thresholds to internal memory */
      pbThresholdNm1[i] = pbThreshold[i];


      if(((pbThreshold[i] >> scaling) > tmpThreshold1)) {
        pbThreshold[i] = tmpThreshold1 << scaling;
      }

      if(tmpThreshold2 > pbThreshold[i]) {
        pbThreshold[i] = tmpThreshold2;
      }

    }
  }
}

