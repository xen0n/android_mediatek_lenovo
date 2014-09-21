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

#define LOG_TAG "MHAL_JNI"

#include <jni.h>

#include <utils/Log.h>
#include <utils/threads.h>
#include <cutils/xlog.h>
#include <cutils/properties.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "ddp_drv.h"
#include "ddp_color_index.h"

using namespace android;

static int drvID = -1;
#define PQ_PARAM_IDX_DEFAULT    "0"

#define PQ_GAMMA_IDX_NUM    (21)
#define PQ_GAMMA_IDX_DEFAULT    "10"


/////////////////////////////////////////////////////////////////////////////////


static jint getCustomPQParamRange(JNIEnv *env, jobject thiz)
{
    return PQ_CUSTOM_PQPARAM_IDX_NUM ;
}


static jint getCustomPQParam(JNIEnv *env, jobject thiz)
{
    char value[PROPERTY_VALUE_MAX];
    int g_pqparam_idx = -1;
    {
        property_get("persist.sys.disp.pq.pqparamidx", value, PQ_PARAM_IDX_DEFAULT);
        g_pqparam_idx = atoi(value);
        XLOGD("[PQ][JNI] getCustomPQParam(), property get...pqparam[%d]", g_pqparam_idx);
    }
    return g_pqparam_idx;
}

static jboolean setCustomPQParam(JNIEnv *env, jobject thiz, int index)
{
   // int drvID = -1;    
    int pq_param_range = 0;
     char value[PROPERTY_VALUE_MAX];

    XLOGD("[PQ][JNI] setCustomPQParam...\n");

    if(drvID == -1) //initial
        drvID = open("/dev/mtk_disp", O_RDONLY, 0);


    if (index < 0)
    {
        index = 0;
    }
    else if (index >=PQ_CUSTOM_PQPARAM_IDX_NUM)
    {
        index = PQ_CUSTOM_PQPARAM_IDX_NUM-1;
    }
       
   snprintf(value, PROPERTY_VALUE_MAX, "%d\n", index);
   property_set("persist.sys.disp.pq.pqparamidx", value);  
  
   ioctl(drvID, DISP_IOCTL_SET_PQPARAM,&pqparam_custom[index]);
   ioctl(drvID,  DISP_IOCTL_SET_PQ_GAL_PARAM, &pqparam_custom[index]); 

    
    XLOGD("[PQ][JNI] setCustomPQParam..index = %d.\n",index);
    return JNI_TRUE;
}

/////////////////////////////////////////////////////////////////////////////////

static jint getGammaRange(JNIEnv *env, jobject thiz)
{
    return PQ_GAMMA_IDX_NUM;
}

static jint getGammaIndex(JNIEnv *env, jobject thiz)
{
    char value[PROPERTY_VALUE_MAX];
    int g_gamma_idx = -1;

    //if (g_gamma_idx == -1)   // always read from property
    {
        property_get("persist.sys.disp.pq.gammaidx", value, PQ_GAMMA_IDX_DEFAULT);
        g_gamma_idx = atoi(value);
        XLOGD("[PQ][JNI] getGammaIndex(), property get...gamma[%d]", g_gamma_idx);
    }
    
    return g_gamma_idx;
}

static jboolean setGammaIndex(JNIEnv *env, jobject thiz , int index)
{
    int lcm_index = 0;
    char value[PROPERTY_VALUE_MAX];
    int gammalut_range;

    XLOGD("[PQ][JNI] setGammaIndex...gamma[%d]", index);

    if(drvID == -1) //initial
        drvID = open("/dev/mtk_disp", O_RDONLY, 0);

    if (index < 0)
    {
        index = 0;
    }
    else if (index >= PQ_GAMMA_IDX_NUM)
    {
        index = PQ_GAMMA_IDX_NUM-1;
    }
        
    snprintf(value, PROPERTY_VALUE_MAX, "%d\n", index);
    property_set("persist.sys.disp.pq.gammaidx", value);

    ioctl(drvID, DISP_IOCTL_GET_LCMINDEX, &lcm_index);

    if (lcm_index < 0)
    {
        lcm_index = 0;
    }
    else if (lcm_index >=LCM_CNT)
    {
        lcm_index = LCM_CNT-1;
    }

    ioctl(drvID, DISP_IOCTL_SET_GAMMALUT,&gammaindex[lcm_index][index]);

    return JNI_TRUE;
}

/////////////////////////////////////////////////////////////////////////////////

//JNI register
////////////////////////////////////////////////////////////////
static const char *classPathName = "com/mediatek/pq/PictureQualityGammaJni";

static JNINativeMethod g_methods[] = {
  {"nativeGetPQParamRange",  "()I", (void*)getCustomPQParamRange},
  {"nativeSetPQParamIndex",  "(I)Z", (void*)setCustomPQParam },
  {"nativeGetPQParamIndex",  "()I", (void*)getCustomPQParam },

  {"nativeGetGammaRange",  "()I", (void*)getGammaRange },
  {"nativeGetGammaIndex",  "()I", (void*)getGammaIndex },
  {"nativeSetGammaIndex",  "(I)Z", (void*)setGammaIndex}
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        XLOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        XLOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

// ----------------------------------------------------------------------------

/*
 * This is called by the VM when the shared library is first loaded.
 */
 
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;
    
    XLOGI("JNI_OnLoad");
    
    if (JNI_OK != vm->GetEnv((void **)&env, JNI_VERSION_1_4)) {
        XLOGE("ERROR: GetEnv failed");
        goto bail;
    }

    if (!registerNativeMethods(env, classPathName, g_methods, sizeof(g_methods) / sizeof(g_methods[0]))) {
        XLOGE("ERROR: registerNatives failed");
        goto bail;
    }
    
    result = JNI_VERSION_1_4;
    
bail:
    return result;
}
