/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,   
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <linux/kernel.h>//for printk


#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    printk(KERN_INFO PFX "%s: " fmt, __FUNCTION__ ,##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         printk(KERN_ERR PFX "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_XLOG_INFO(fmt, args...) \
                do {    \
                    xlog_printk(ANDROID_LOG_INFO, "kd_camera_hw", fmt, ##args); \
                } while(0)
#else
#define PK_DBG(a,...)
#define PK_ERR(a,...)
#define PK_XLOG_INFO(fmt, args...)
#endif
//extern const char * get_ty_board_info();

#define AF_EN_PIN GPIO30
int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On, char* mode_name)
{

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4

#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3

	u32 pinSetIdx = 0;//default main sensor
	//const char * board_info=NULL;	
	u32 pinSet[3][8] = {
						//for main sensor for OV13850
						//notice there on state we mean a state which can make camer on.
						{GPIO_CAMERA_CMRST_PIN,
						GPIO_CAMERA_CMRST_PIN_M_GPIO,   /* mode */
						GPIO_OUT_ONE,                   /* ON state */
						GPIO_OUT_ZERO,                  /* OFF state */
						GPIO_CAMERA_CMPDN_PIN,
						GPIO_CAMERA_CMPDN_PIN_M_GPIO,
						GPIO_OUT_ONE,                  /* ON state */  
						GPIO_OUT_ZERO,         			/* OFF state */
						},
						//for sub sensor  0V5648 
						{GPIO_CAMERA_CMRST1_PIN,
						GPIO_CAMERA_CMRST1_PIN_M_GPIO,
						GPIO_OUT_ONE,                   /* ON state */
						GPIO_OUT_ZERO,					/* OFF state */
						GPIO_CAMERA_CMPDN1_PIN,
						GPIO_CAMERA_CMPDN1_PIN_M_GPIO,
						GPIO_OUT_ONE,					/* ONF state */
						GPIO_OUT_ZERO,					/* OFF state */
						},				

                   };

    if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx){
        pinSetIdx = 0;
    }
    else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
        pinSetIdx = 1;
    }
    else if (DUAL_CAMERA_MAIN_SECOND_SENSOR == SensorIdx) {
        pinSetIdx = 2;
		//not support.
		 return 0;
    }
	//board_info=get_ty_board_info();

    if (On) {  
		//power ON
		//set pwdn level.
		printk("----[%s][%d]-----[%s]-[%s][%d]----power on--------------\n",
				__FUNCTION__,__LINE__,currSensorName,mode_name,pinSetIdx);

		if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV13850_MIPI_RAW, currSensorName)))
		{  
        		//disable inactive sensor
        		if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMRST]) {
        			mt_set_gpio_mode(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE]);
        			mt_set_gpio_dir(pinSet[1-pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT);
        			mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]);	
        		}
        		if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMPDN]) {
        			mt_set_gpio_mode(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE]);
        			mt_set_gpio_dir(pinSet[1-pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT);	
        			mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF]);	
        		}		 
        		mdelay(5);
        		//PDN pin  and reset pin  
        		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN]) {	
                    if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE]))
        				{PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
                    if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT))
        				{PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
                    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF]))
        				{PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
                	}  
        		
        		//RST pin	
        		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE]))
        				{PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
        			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT))
        				{PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
        			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON]))
        				{PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        		}
        		
        		if(pinSetIdx==0)
        		{
        			//for main camera power on
        			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        			{
        		        PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
        			}	
        			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
        			{
        			     PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
        			}	
        		
        			//for ov13850
        			if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV13850_MIPI_RAW,currSensorName)))
        			{
        				printk("----[%s][%d]----------power on for 13850 1.2v--------------\n",__FUNCTION__,__LINE__);
        				if(TRUE != hwPowerOn(MT6323_POWER_LDO_VGP3, VOL_1200,mode_name))//yufeng
        				{
        				     PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
        				}	
        			}
        		}
                //enable active sensor      
                //PDN pin
                	mdelay(5);
        		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN]) {	
        			printk("-----------set pwdn-----[%d]----[%d]-------\n",pinSetIdx,pinSet[pinSetIdx][IDX_PS_CMPDN]);
                    	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE]))
        				{PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
                    	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT))
        				{PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
                    	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON]))
        				{PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
                	}   
        	
        		//set vcm power on and enable af pin!active low
        		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
        	    	{
        	        	PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
        	    	}  

			mdelay(20);
	}		
	else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5648_MIPI_RAW, currSensorName)))
	{

		//disable inactive sensor
		if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMRST]) {
			mt_set_gpio_mode(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE]);
			mt_set_gpio_dir(pinSet[1-pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT);
			mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]);	
		}
		if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMPDN]) {
			mt_set_gpio_mode(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE]);
			mt_set_gpio_dir(pinSet[1-pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT);	
			mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF]);	
		}	
		mdelay(5);

		//ISP_MCLK1_EN(true);

		//OV5648 Power UP
		//First Power Pin low and Reset Pin Low
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
			//PDN pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN], pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN], pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}

			//Reset pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST], pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],  pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		}        

		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}
		mdelay(2);
		//2.        
		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800, mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}
		mdelay(2);
		//3.
		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		} 
		mdelay(2);
		//4.yufeng
		//if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
		//{
		//PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
		//return -EIO;
		//goto _kdCISModulePowerOn_exit_;
		//}

		// wait power to be stable 
		mdelay(5);

		//enable active sensor
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
			//PDN pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN], pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN], pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}

			mdelay(2);

			//RST pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST], pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		}
		mdelay(20);
	}				

		printk("---------power on end--------------\n");
    }
    else {
		//power OFF
		printk("----[%s][%d]---------[%s][%s][%d]-power off--------------\n",
			 __FUNCTION__,__LINE__,currSensorName,mode_name,pinSetIdx);

		if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV13850_MIPI_RAW, currSensorName)))
    		{
    		//disable main camera.
    		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN]) {		
    		    if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE]))
    				{PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
    		    if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT))
    				{PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
    		    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF]))
    				{PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
    		}      
    
    		mdelay(5);
    		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
    		{
    		    PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
    		} 
    
    		//power down main camera.
    		if(pinSetIdx==0)
    		{	
    			//for ov13850
    			if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV13850_MIPI_RAW,currSensorName)))
    			{
    				printk("----[%s][%d]----------power down for ov13850 1.2v--------------\n",__FUNCTION__,__LINE__);
    				if(TRUE != hwPowerDown(MT6323_POWER_LDO_VGP3,mode_name))//yufeng
    				{
    				     PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
    				}	
    			}		
    			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name)) {
    	            PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
    	       	}
    			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
    	            PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
    			}
    
    			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE]))
    					{PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
    				if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT))
    					{PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
    				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]))
    					{PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
    			}
    		}
    
    		if(pinSetIdx==1)
    		{
    
    		}
    		
    		//RST pin	
    		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE]))
    				{PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
    			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT))
    				{PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
    			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]))
    				{PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
    		}
		}

		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5648_MIPI_RAW,currSensorName)))
        {
            //ISP_MCLK1_EN(false);
        
            //PK_DBG("[OFF]sensorIdx:%d \n",SensorIdx);
            if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
                if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
                if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
                if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
                if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
                if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
                if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
            }
            
            if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
                PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
            //if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
            //{
            //    PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //    //return -EIO;
            //    //goto _kdCISModulePowerOn_exit_;
            //}yufeng
            if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
                PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
            if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }

        }
		printk("---------power off end--------------\n");
    }//

    return 0;

_kdCISModulePowerOn_exit_:
	return -EIO;
		
}

EXPORT_SYMBOL(kdCISModulePowerOn);


//!--
//





