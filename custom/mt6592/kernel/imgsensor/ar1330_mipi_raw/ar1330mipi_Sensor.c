/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   YUSU
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *
 
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>


#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ar1330mipi_Sensor.h"
#include "ar1330mipi_Camera_Sensor_para.h"
#include "ar1330mipi_CameraCustomized.h"

#define AR1330MIPI_DRIVER_TRACE
#define AR1330MIPI_DEBUG
#ifdef AR1330MIPI_DEBUG
#define SENSORDB(fmt, arg...) printk("%s: " fmt "\n", __FUNCTION__ ,##arg)
#else
#define SENSORDB(x,...)
#endif

static DEFINE_SPINLOCK(ar1330mipi_drv_lock);

#define AUTO_FLICKER_NO 10
kal_uint16 AR1330_Frame_Length_preview = 0;
kal_bool AR1330DuringTestPattern = KAL_FALSE;
#define AR1330MIPI_TEST_PATTERN_CHECKSUM (0x9439a86e)

struct AR1330MIPI_SENSOR_STRUCT AR1330MIPI_sensor;
kal_uint16 Salve_addr=0x6c;
kal_uint16 AR1330MIPI_exposure_lines = 0x100;
kal_uint16 AR1330MIPI_sensor_global_gain = BASEGAIN, AR1330MIPI_sensor_gain_base = BASEGAIN;
kal_uint16 AR1330MIPI_sensor_gain_array[2][5] = {{0x0204,0x0208, 0x0206, 0x020C, 0x020A},{0x08,0x8, 0x8, 0x8, 0x8}};


MSDK_SENSOR_CONFIG_STRUCT AR1330MIPISensorConfigData;
kal_uint32 AR1330MIPI_FAC_SENSOR_REG;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT AR1330MIPISensorCCT[FACTORY_END_ADDR]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT AR1330MIPISensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/
static MSDK_SCENARIO_ID_ENUM AR1330_CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;



AR1330MIPI_MODE g_iAR1330MIPI_Mode = AR1330MIPI_MODE_PREVIEW;


static void AR1330MIPI_SetDummy(const kal_uint16 iDummyPixels, const kal_uint16 iDummyLines);


extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iMultiWriteReg(u8 *pData, u16 lens, u16 i2cId);


kal_uint16 AR1330MIPI_read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(puSendCmd , 2, (u8*)&get_byte, 2, Salve_addr);
    return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


void AR1330MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};
    iWriteRegI2C(puSendCmd , 4, Salve_addr);
}


#define AR1330_multi_write_cmos_sensor(pData, lens) iMultiWriteReg((u8*) pData, (u16) lens, Salve_addr)


kal_uint16 AR1330MIPI_read_cmos_sensor_8(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,Salve_addr);
    return get_byte;
}

void AR1330MIPI_write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
    char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
    iWriteRegI2C(puSendCmd , 3, Salve_addr);
}



/*************************************************************************
* FUNCTION
*    read_AR1330MIPI_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/

/*******************************************************************************
* 
********************************************************************************/
void AR1330MIPI_gain2reg(kal_uint16 iGain)
{
	kal_uint16 Analog_gain=0;
	kal_uint16 Digital_gain=0;
	kal_uint16 gain_step=0;
           //parameter_hold
 #if 0
    if(gain >= BASEGAIN && gain <= (kal_uint16)(1.33*BASEGAIN))
    {   
        Analog_gain=0x1000;
		gain_step=(gain*128)/(BASEGAIN);
		
    }
	else if(gain > (kal_uint16)(1.33*BASEGAIN) && gain <= (kal_uint16)(1.5*BASEGAIN))
	{
		Analog_gain=0x100D;
		gain_step=(gain*128)/(BASEGAIN*133/100);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
	else if(gain > (kal_uint16)(1.5*BASEGAIN) && gain <= (kal_uint16)(2*BASEGAIN))
	{
		Analog_gain=0x1007;
		gain_step=(gain*128)/(BASEGAIN*3/2);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
	else if(gain > (kal_uint16)(2*BASEGAIN) && gain <= (kal_uint16)(3*BASEGAIN))
	{
		Analog_gain=0x1001;
		gain_step=(gain*128)/(BASEGAIN*2);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
	else if(gain > (kal_uint16)(3*BASEGAIN) && gain <= (kal_uint16)(4*BASEGAIN))
	{
		Analog_gain=0x1002;
		gain_step=(gain*128)/(BASEGAIN*3);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
	else if(gain > (kal_uint16)(4*BASEGAIN) && gain <= (kal_uint16)(6*BASEGAIN))
	{
		Analog_gain=0x1003;
		gain_step=(gain*128)/(BASEGAIN*4);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
	else if(gain > (kal_uint16)(6*BASEGAIN) && gain <= (kal_uint16)(8*BASEGAIN))
	{
		Analog_gain=0x100A;
		gain_step=(gain*128)/(BASEGAIN*6);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
	else if(gain > (kal_uint16)(8*BASEGAIN) )
	{
		Analog_gain=0x100B;
		gain_step=(gain*128)/(BASEGAIN*8);
		//Digital_gain=((kal_uint16)(gain_step)*128)<<1;
	}
    else
    {
        SENSORDB("error gain setting");
		return 0;
    }
	
	Digital_gain=((gain_step)<<1);
	SENSORDB("gain=%d.",gain);
	SENSORDB("Analog_gain=%0x",Analog_gain);
	SENSORDB("Digital_gain=%0x",Digital_gain);
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01); 
	AR1330MIPI_write_cmos_sensor(0x305E,Analog_gain);
	AR1330MIPI_write_cmos_sensor(0x3032,Digital_gain);
	AR1330MIPI_write_cmos_sensor(0x3034,Digital_gain);
	AR1330MIPI_write_cmos_sensor(0x3036,Digital_gain);
	AR1330MIPI_write_cmos_sensor(0x3038,Digital_gain);
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);        //parameter_hold
  #else
  kal_uint16 reg_gain; //iGain4,iGain5,iGain6,iGain7;
	SENSORDB("[AR1330MIPIGain2Reg1] iGain is :%d \n", iGain);

	if(iGain>=512){
		reg_gain=(((iGain*128)/(8*BASEGAIN))<<5)|0x000B;
		}
	else if(iGain>=384)
    	{
		reg_gain=(((iGain*128)/(6*BASEGAIN))<<5)|0x000A;
    	}
	else if(iGain>=256){
		reg_gain=(((iGain*128)/(4*BASEGAIN))<<5)|0x0003;
	}
	else if(iGain>=192){
		reg_gain=(((iGain*128)/(3*BASEGAIN))<<5)|0x0002;
	}
	else if(iGain>=128){
		reg_gain=(((iGain*128)/(2*BASEGAIN))<<5)|0x0001;
		}
	else if(iGain>=96){
		reg_gain=(((iGain*128*2)/(3*BASEGAIN))<<5)|0x0007;
		}
	else if(iGain>=64){
		reg_gain=((iGain*128/BASEGAIN)<<5)|0x0000;
    	}
	

	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);
	AR1330MIPI_write_cmos_sensor(0x305E,reg_gain);
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);
  #endif
}


/*************************************************************************
* FUNCTION
* set_AR1330MIPI_gain
*
* DESCRIPTION
* This function is to set global gain to sensor.
*
* PARAMETERS
* gain : sensor global gain(base: 0x40)
*
* RETURNS
* the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
void AR1330MIPI_Set_gain(kal_uint16 iGain)
{
    spin_lock(&ar1330mipi_drv_lock);
	AR1330MIPI_sensor.gain=iGain;
	spin_unlock(&ar1330mipi_drv_lock);
	AR1330MIPI_gain2reg(iGain);
	
}


/*************************************************************************
* FUNCTION
*   AR1330MIPI_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of AR1330MIPI to change exposure time.
*
* PARAMETERS
*   shutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void AR1330MIPI_SetShutter(kal_uint16 iShutter)
{
    if(iShutter<=0)
		iShutter=1;
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);
	AR1330MIPI_write_cmos_sensor(0x0202,iShutter);
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);
	spin_lock(&ar1330mipi_drv_lock);
	AR1330MIPI_sensor.shutter=iShutter;
	spin_unlock(&ar1330mipi_drv_lock);
	SENSORDB("iShutter=%d.",iShutter);
}


/*************************************************************************
* FUNCTION
*   AR1330MIPI_read_shutter
*
* DESCRIPTION
*   This function to  Get exposure time.
*
* PARAMETERS
*   None
*
* RETURNS
*   shutter : exposured lines
*
* GLOBALS AFFECTED
*
*************************************************************************/




UINT32 AR1330MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId,UINT32 frameRate)
{

    kal_uint32 pclk;
    kal_int16 dummyLine;
    kal_uint16 lineLength,frameHeight;

    printk("AR1330SetMaxFramerate: scenarioID = %d, frame rate = %d\n",scenarioId,frameRate);
    switch(scenarioId) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pclk = AR1330MIPI_sensor.preview_vt_clk*100000;  
            lineLength = AR1330MIPI_PV_PERIOD_PIXEL_NUMS;  //3151
            frameHeight = (10*pclk)/frameRate/lineLength;
            dummyLine = frameHeight - AR1330MIPI_PV_PERIOD_PIXEL_NUMS;
            AR1330MIPI_SetDummy( 0,dummyLine);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pclk = AR1330MIPI_sensor.preview_vt_clk*100000;  
            lineLength = AR1330MIPI_PV_PERIOD_PIXEL_NUMS;  // 3151
            frameHeight = (10*pclk)/frameRate/lineLength;
            dummyLine = frameHeight - AR1330MIPI_PV_PERIOD_PIXEL_NUMS;
            AR1330MIPI_SetDummy( 0,dummyLine);
            break;  
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            pclk = AR1330MIPI_sensor.capture_vt_clk*100000; 
            lineLength = AR1330MIPI_FULL_PERIOD_PIXEL_NUMS;  //3694
            frameHeight = (10*pclk)/frameRate/lineLength;
            dummyLine = frameHeight - AR1330MIPI_FULL_PERIOD_PIXEL_NUMS;
            AR1330MIPI_SetDummy(0,dummyLine);
            break;  
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
            break;  
            
        default:
            break;
            
        }
    return ERROR_NONE;

}

UINT32 AR1330MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId,UINT32 *pframeRate)
{

    switch(scenarioId) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *pframeRate = 300;
            break;  
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:   
            *pframeRate = 240;
            break;  
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
            *pframeRate = 300;
            break;  
            
        default:
            break;
            
        }
    return ERROR_NONE;

}


/*******************************************************************************
* 
********************************************************************************/
void AR1330MIPI_camera_para_to_sensor(void)
{
    kal_uint32    i;

 /*   
    for(i=0; 0xFFFFFFFF!=AR1330MIPISensorReg[i].Addr; i++)
    {
        AR1330MIPI_write_cmos_sensor(AR1330MIPISensorReg[i].Addr, AR1330MIPISensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=AR1330MIPISensorReg[i].Addr; i++)
    {
        AR1330MIPI_write_cmos_sensor(AR1330MIPISensorReg[i].Addr, AR1330MIPISensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        AR1330MIPI_write_cmos_sensor(AR1330MIPISensorCCT[i].Addr, AR1330MIPISensorCCT[i].Para);
    }
    */
}


/*************************************************************************
* FUNCTION
*    AR1330MIPI_sensor_to_camera_para
*
* DESCRIPTION
*    // update camera_para from sensor register
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
void AR1330MIPI_sensor_to_camera_para(void)
{
    kal_uint32    i;
   /* 
    for(i=0; 0xFFFFFFFF!=AR1330MIPISensorReg[i].Addr; i++)
    {
        spin_lock(&ar1330mipi_drv_lock);
        AR1330MIPISensorReg[i].Para = AR1330MIPI_read_cmos_sensor(AR1330MIPISensorReg[i].Addr);
        spin_unlock(&ar1330mipi_drv_lock);        
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=AR1330MIPISensorReg[i].Addr; i++)
    {   
        spin_lock(&ar1330mipi_drv_lock);
        AR1330MIPISensorReg[i].Para = AR1330MIPI_read_cmos_sensor(AR1330MIPISensorReg[i].Addr);
        spin_unlock(&ar1330mipi_drv_lock);
    }
    */
}


/*************************************************************************
* FUNCTION
*    AR1330MIPI_get_sensor_group_count
*
* DESCRIPTION
*    //
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_int32  AR1330MIPI_get_sensor_group_count(void)
{
    return 0;
}


void AR1330MIPI_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{
#if 0
    switch (group_idx)
    {
        case PRE_GAIN:
            sprintf((char *)group_name_ptr, "CCT");
            *item_count_ptr = 5;
            break;
        case CMMCLK_CURRENT:
            sprintf((char *)group_name_ptr, "CMMCLK Current");
            *item_count_ptr = 1;
            break;
        case FRAME_RATE_LIMITATION:
            sprintf((char *)group_name_ptr, "Frame Rate Limitation");
            *item_count_ptr = 2;
            break;
        case REGISTER_EDITOR:
            sprintf((char *)group_name_ptr, "Register Editor");
            *item_count_ptr = 2;
            break;
        default:
            ASSERT(0);
    }
#endif
}


void AR1330MIPI_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
#if 0
    kal_int16 temp_reg=0;
    kal_uint16 temp_gain=0, temp_addr=0, temp_para=0;
/*    
    switch (group_idx)
    {
        case PRE_GAIN:
           switch (item_idx)
          {
              case 0:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-R");
                  temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gr");
                  temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gb");
                  temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-B");
                  temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                 sprintf((char *)info_ptr->ItemNamePtr,"SENSOR_BASEGAIN");
                 temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

            temp_para=AR1330MIPISensorCCT[temp_addr].Para;

           if(temp_para>=0x08 && temp_para<=0x78)
                temp_gain=(temp_para*BASEGAIN)/8;
            else
                ASSERT(0);

            temp_gain=(temp_gain*1000)/BASEGAIN;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min=1000;
            info_ptr->Max=15000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");
                
                    //temp_reg=AR1330MIPISensorReg[CMMCLK_CURRENT_INDEX].Para;
                    temp_reg = ISP_DRIVING_2MA;
                    if(temp_reg==ISP_DRIVING_2MA)
                    {
                        info_ptr->ItemValue=2;
                    }
                    else if(temp_reg==ISP_DRIVING_4MA)
                    {
                        info_ptr->ItemValue=4;
                    }
                    else if(temp_reg==ISP_DRIVING_6MA)
                    {
                        info_ptr->ItemValue=6;
                    }
                    else if(temp_reg==ISP_DRIVING_8MA)
                    {
                        info_ptr->ItemValue=8;
                    }
                
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_TRUE;
                    info_ptr->Min=2;
                    info_ptr->Max=8;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Max Exposure Lines");
                    info_ptr->ItemValue=    111;  //AR1330MIPI_MAX_EXPOSURE_LINES;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"Min Frame Rate");
                    info_ptr->ItemValue=12;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Addr.");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Value");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                default:
                ASSERT(0);
            }
            break;
        default:
            ASSERT(0);

    }
    */
#endif
}


kal_bool AR1330MIPI_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
#if 0
//   kal_int16 temp_reg;
   kal_uint16  temp_gain=0,temp_addr=0, temp_para=0;
/*
   switch (group_idx)
    {
        case PRE_GAIN:
            switch (item_idx)
            {
              case 0:
                temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

            temp_gain=((ItemValue*BASEGAIN+500)/1000);          //+500:get closed integer value

          if(temp_gain>=1*BASEGAIN && temp_gain<=15*BASEGAIN)
          {
             temp_para=(temp_gain*8+BASEGAIN/2)/BASEGAIN;
          }          
          else
              ASSERT(0);
            spin_lock(&ar1330mipi_drv_lock);
            AR1330MIPISensorCCT[temp_addr].Para = temp_para;
            spin_unlock(&ar1330mipi_drv_lock);

            AR1330MIPI_write_cmos_sensor(AR1330MIPISensorCCT[temp_addr].Addr,temp_para);

            spin_lock(&ar1330mipi_drv_lock);
            AR1330MIPI_sensor_gain_base=read_AR1330MIPI_gain();
            spin_unlock(&ar1330mipi_drv_lock);

            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    //no need to apply this item for driving current
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            ASSERT(0);
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
                    spin_lock(&ar1330mipi_drv_lock);
                    AR1330MIPI_FAC_SENSOR_REG=ItemValue;
                    spin_unlock(&ar1330mipi_drv_lock);
                    break;
                case 1:
                    AR1330MIPI_write_cmos_sensor(AR1330MIPI_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
   */
 #endif
    return KAL_TRUE;
}

static kal_uint8 AR1330_init_part1[] = {
0x30,0x1A,0x00,0x18,     // reset_register
0x30,0x64,0x58,0x40,     // smia_test
0x01,0x12,0x0A,0x0A,     // data_format
0x31,0xAE,0x02,0x04,     // serial_format

//Rev2_recommended_settings
0x30,0x42,0x11,0x04,
0x30,0x44,0x85,0x80,
0x30,0xD2,0x00,0x00,
0x30,0xD4,0x10,0x40,
0x30,0xDE,0x00,0x02,
0x31,0x80,0xD0,0xFF,
0x30,0xEE,0x31,0x55,
0x3E,0xCC,0x00,0xAF,
0x3E,0xD4,0xCC,0xAF,
0x3E,0xDA,0x60,0x24, 	
0x3E,0xD6,0x8A,0x20,
0x3E,0xDC,0x80,0xC1,
0x3E,0xDE,0x50,0xE0,
0x31,0x6a,0x82,0x00,
0x3E,0xD0,0x88,0x53,
0x3E,0xFC,0xB2,0xB7,
0x3E,0xE0,0x01,0x44,
0x3E,0xE4,0xA5,0x06,
0x31,0x6C,0x81,0x00,
0x31,0x6E,0x81,0x00,
0x3E,0xFA,0x8F,0x8F,
0x3E,0xFE,0x8F,0x80,
//Sequencer Settings
// @00 Jump Table
0x3D,0x00,0x04,0xFF,
0x3D,0x02,0x58,0x75,
0x3D,0x04,0xFF,0xFF,
0x3D,0x06,0xFF,0xFF,
// @04 Read
0x3D,0x08,0x82,0x71,
0x3D,0x0A,0x40,0x80,
0x3D,0x0C,0x56,0x80,
0x3D,0x0E,0x54,0x80,
0x3D,0x10,0x00,0x22,
0x3D,0x12,0x80,0x47,
0x3D,0x14,0x80,0x46,
0x3D,0x16,0x80,0x20,
0x3D,0x18,0x05,0xBC,
0x3D,0x1A,0x5D,0x80,
0x3D,0x1C,0x42,0x80,
0x3D,0x1E,0x46,0x80,
0x3D,0x20,0x47,0x10,
0x3D,0x22,0x06,0x8B,
0x3D,0x24,0x5F,0xFF,
0x3D,0x26,0xB8,0x59,
0x3D,0x28,0xA6,0x50,
0x3D,0x2A,0x9A,0x4F,
0x3D,0x2C,0x82,0x51,
0x3D,0x2E,0x80,0x5F,
0x3D,0x30,0x80,0x4D,
0x3D,0x32,0x8C,0x43,
0x3D,0x34,0xB6,0x43,
0x3D,0x36,0xFF,0x9C,
0x3D,0x38,0x51,0x88,
0x3D,0x3A,0x53,0xBA,
0x3D,0x3C,0x53,0x9A,
0x3D,0x3E,0x4F,0xA4,
0x3D,0x40,0x4F,0x80,
0x3D,0x42,0x42,0x80,
0x3D,0x44,0x52,0x80,
0x3D,0x46,0x00,0x81,
0x3D,0x48,0x80,0x00,
0x3D,0x4A,0x41,0x80,
0x3D,0x4C,0x44,0x80,
0x3D,0x4E,0x43,0x80,
0x3D,0x50,0x4A,0x80,
0x3D,0x52,0x49,0xAE,
0x3D,0x54,0x49,0x80,
0x3D,0x56,0x4A,0x80,
0x3D,0x58,0x43,0x80,
0x3D,0x5A,0x44,0x80,
0x3D,0x5C,0x40,0x80,
0x3D,0x5E,0x40,0x80,
0x3D,0x60,0x44,0x80,
0x3D,0x62,0x43,0x80,
0x3D,0x64,0x4A,0x80,
0x3D,0x66,0x49,0xAE,
0x3D,0x68,0x49,0x80,
0x3D,0x6A,0x4A,0x80,
0x3D,0x6C,0x43,0x80,
0x3D,0x6E,0x44,0x80,
0x3D,0x70,0x40,0x80,
0x3D,0x72,0x40,0x80,
0x3D,0x74,0x44,0x80,
0x3D,0x76,0x43,0x80,
0x3D,0x78,0x4A,0x80,
0x3D,0x7A,0x49,0xAE,
0x3D,0x7C,0x49,0x80,
0x3D,0x7E,0x4A,0x80,
0x3D,0x80,0x43,0x80,
0x3D,0x82,0x44,0x80,
0x3D,0x84,0x40,0x80,
0x3D,0x86,0x40,0x80,
0x3D,0x88,0x44,0x80,
0x3D,0x8A,0x43,0x80,
0x3D,0x8C,0x4A,0x80,
0x3D,0x8E,0x49,0x80,
0x3D,0x90,0x59,0x80,
0x3D,0x92,0x54,0x90,
0x3D,0x94,0x52,0x98,
0x3D,0x96,0x49,0x80,
0x3D,0x98,0x4A,0x80,
0x3D,0x9A,0x43,0x80,
0x3D,0x9C,0x00,0x30,
0x3D,0x9E,0x80,0x00,
0x3D,0xA0,0x41,0x80,
0x3D,0xA2,0x47,0x80,
0x3D,0xA4,0x41,0x80,
0x3D,0xA6,0x20,0x25,
0x3D,0xA8,0x82,0x56,
0x3D,0xAA,0x80,0x10,
0x3D,0xAC,0x1C,0x82,
0x3D,0xAE,0x70,0x00,
0x3D,0xB0,0x83,0x7A,
0x3D,0xB2,0x8E,0x20,
0x3D,0xB4,0x09,0x92,
0x3D,0xB6,0x5C,0x92,
0x3D,0xB8,0x00,0x12,
0x3D,0xBA,0x82,0x78,
0x3D,0xBC,0x8E,0x43,
0x3D,0xBE,0x80,0x04,
0x3D,0xC0,0x80,0x80,
0x3D,0xC2,0x02,0x40,
0x3D,0xC4,0x80,0xAF,
0x3D,0xC6,0x09,0x00,
0x3D,0xC8,0x80,0x02,
0x3D,0xCA,0x40,0x80,
0x3D,0xCC,0x04,0x80,
0x3D,0xCE,0xB0,0x04,
0x3D,0xD0,0x80,0x80,
0x3D,0xD2,0x02,0x40,
0x3D,0xD4,0x80,0x09,
0x3D,0xD6,0x00,0x80,
0x3D,0xD8,0x7C,0xC5,
0x3D,0xDA,0x9C,0x43,
0x3D,0xDC,0xE2,0x02,
0x3D,0xDE,0x40,0x80,
0x3D,0xE0,0x04,0x90,
0x3D,0xE2,0x82,0x79,
0x3D,0xE4,0xD7,0x41,
0x3D,0xE6,0x20,0x19,
0x3D,0xE8,0x80,0x70,
0x3D,0xEA,0x82,0x71,
0x3D,0xEC,0x40,0x80,
0x3D,0xEE,0x56,0x80,
0x3D,0xF0,0x54,0x80,
0x3D,0xF2,0x00,0x22,
0x3D,0xF4,0x80,0x47,
0x3D,0xF6,0x80,0x46,
0x3D,0xF8,0x80,0x20,
0x3D,0xFA,0x05,0xBC,
0x3D,0xFC,0x5D,0x80,
0x3D,0xFE,0x42,0x80,
0x3E,0x00,0x46,0x80,
0x3E,0x02,0x47,0x10,
0x3E,0x04,0x06,0xFF,
0x3E,0x06,0xC5,0x59,
0x3E,0x08,0xA6,0x50,
0x3E,0x0A,0x9A,0x4F,
0x3E,0x0C,0x82,0x51,
0x3E,0x0E,0x82,0x4D,
0x3E,0x10,0x8C,0x43,
0x3E,0x12,0xB6,0x43,
0x3E,0x14,0xCB,0x5F,
0x3E,0x16,0x80,0x5F,
0x3E,0x18,0xCD,0x51,
0x3E,0x1A,0x88,0x53,
0x3E,0x1C,0xBA,0x53,
0x3E,0x1E,0x9A,0x4F,
0x3E,0x20,0xA4,0x4F,
0x3E,0x22,0x80,0x42,
0x3E,0x24,0x80,0x52,
0x3E,0x26,0x80,0x00,
0x3E,0x28,0x81,0x80,
0x3E,0x2A,0x00,0x41,
0x3E,0x2C,0x80,0x44,
0x3E,0x2E,0x80,0x43,
0x3E,0x30,0x80,0x4A,
0x3E,0x32,0x80,0x49,
0x3E,0x34,0xAE,0x49,
0x3E,0x36,0x80,0x4A,
0x3E,0x38,0x80,0x43,
0x3E,0x3A,0x80,0x44,
0x3E,0x3C,0x80,0x40,
0x3E,0x3E,0x80,0x40,
0x3E,0x40,0x80,0x44,
0x3E,0x42,0x80,0x43,
0x3E,0x44,0x80,0x4A,
0x3E,0x46,0x80,0x49,
0x3E,0x48,0xAE,0x49,
0x3E,0x4A,0x80,0x4A,
0x3E,0x4C,0x80,0x43,
0x3E,0x4E,0x80,0x44,
0x3E,0x50,0x80,0x40,
0x3E,0x52,0x80,0x40,
0x3E,0x54,0x80,0x44,
0x3E,0x56,0x80,0x43,
0x3E,0x58,0x80,0x4A,
0x3E,0x5A,0x80,0x49,
0x3E,0x5C,0x85,0x5F,
0x3E,0x5E,0x80,0x5F,
0x3E,0x60,0xA5,0x49,
0x3E,0x62,0x80,0x4A,
0x3E,0x64,0x80,0x43,
0x3E,0x66,0x80,0x44,
0x3E,0x68,0x80,0x40,
0x3E,0x6A,0x80,0x40,
0x3E,0x6C,0x80,0x44,
0x3E,0x6E,0x80,0x43,
0x3E,0x70,0x80,0x4A,
0x3E,0x72,0x80,0x49,
0x3E,0x74,0x94,0x52,
0x3E,0x76,0x98,0x49,
0x3E,0x78,0x80,0x4A,
0x3E,0x7A,0x80,0x43,
0x3E,0x7C,0x80,0x00,
0x3E,0x7E,0x30,0x80,
0x3E,0x80,0x00,0x41,
0x3E,0x82,0x80,0x47,
0x3E,0x84,0x80,0x41,
0x3E,0x86,0x80,0x20,
0x3E,0x88,0x25,0x80,
0x3E,0x8A,0x59,0x80,
0x3E,0x8C,0x15,0x00,
0x3E,0x8E,0x80,0x10,
0x3E,0x90,0x1C,0x82,
0x3E,0x92,0x70,0x00,
0x3E,0x94,0x00,0x00,
0x3E,0x96,0x00,0x00,
0x3E,0x98,0x00,0x00,
0x3E,0x9A,0x00,0x00,
0x3E,0x9C,0x00,0x00,
0x3E,0x9E,0x00,0x00,
0x3E,0xA0,0x00,0x00,
0x3E,0xA2,0x00,0x00,
0x3E,0xA4,0x00,0x00,
0x3E,0xA6,0x00,0x00,
0x3E,0xA8,0x00,0x00,
0x3E,0xAA,0x00,0x00,
0x3E,0xAC,0x00,0x00,
0x3E,0xAE,0x00,0x00,
0x3E,0xB0,0x00,0x00,
0x3E,0xB2,0x00,0x00,
0x3E,0xB4,0x00,0x00,
0x3E,0xB6,0x00,0x00,
0x3E,0xB8,0x00,0x00,
0x3E,0xBA,0x00,0x00,
0x3E,0xBC,0x00,0x00,
0x3E,0xBE,0x00,0x00,
0x3E,0xC0,0x00,0x00,
0x3E,0xC2,0x00,0x00,
0x3E,0xC4,0x00,0x00,
0x3E,0xC6,0x00,0x00,
0x3E,0xC8,0x00,0x00,
0x3E,0xCA,0x00,0x00,

// PLL_Configuration
0x03,0x00,0x00,0x09,     // vt_pix_clk_div=9
0x03,0x02,0x00,0x01,     // vt_sys_clk_div=1
0x03,0x04,0x00,0x03,     // pre_pll_clk_div=3
0x03,0x06,0x00,0x3F,     // pll_multipler=63
0x03,0x08,0x00,0x0A,     // op_pix_clk_div=10
0x03,0x0A,0x00,0x01};     // op_sys_clk_div=1
	

static kal_uint8 AR1330_init_part2[] = {
	//MIPI_configuration_settings
0x31,0xB0,0x00,0x42,
0x31,0xB2,0x00,0x1A,
0x31,0xB4,0x52,0xCE,
0x31,0xB6,0x43,0x48,
0x31,0xB8,0x20,0x26,
0x31,0xBA,0x18,0x6A,
0x31,0xBC,0x85,0x0A,

//Lens_correction

//Defect_correction
0x31,0xE0,0x17,0x81,
0x3F,0x00,0x00,0x32,
0x3F,0x02,0x00,0xA2,
0x3F,0x04,0x00,0x02,
0x3F,0x06,0x00,0x04,
0x3F,0x08,0x00,0x08,
0x3F,0x0A,0x03,0x02,
0x3F,0x0C,0x05,0x04,
0x3F,0x10,0x02,0x05,
0x3F,0x12,0x02,0x05,
0x3F,0x14,0x02,0x05,
0x3F,0x16,0x06,0x10,
0x3F,0x18,0x06,0x10,
0x3F,0x1A,0x06,0x10,
0x3F,0x1E,0x00,0x27,
0x3F,0x40,0x02,0x0A, 
0x3F,0x42,0x02,0x0A,
0x3F,0x44,0x02,0x0A
};



/*******************************************************************************
*
********************************************************************************/
static void AR1330MIPI_Init_setting(void)
{
    SENSORDB( "Enter!");
#if 1
 #if 1  
    //IMAGE= 2104,1560, BAYER-10

    //Software Reset
    AR1330MIPI_write_cmos_sensor_8(0x0103, 0x01);    //SOFTWARE_RESET (clears itself)
    mDELAY(50);      //Initialization Time

    // Interface_configuration
    AR1330MIPI_write_cmos_sensor(0x301A, 0x0218);     // reset_register
    AR1330MIPI_write_cmos_sensor(0x3064, 0x5840);     // smia_test
    AR1330MIPI_write_cmos_sensor(0x0112, 0x0A0A);     // data_format
    AR1330MIPI_write_cmos_sensor(0x31AE, 0x0204);     // serial_format

    //Rev2_recommended_settings
    AR1330MIPI_write_cmos_sensor(0x3042, 0x1104);
    AR1330MIPI_write_cmos_sensor(0x3044, 0x8580);
    AR1330MIPI_write_cmos_sensor(0x30D2, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x30D4, 0x1040);
    AR1330MIPI_write_cmos_sensor(0x30DE, 0x0002);
    AR1330MIPI_write_cmos_sensor(0x3180, 0xD0FF);
    AR1330MIPI_write_cmos_sensor(0x30EE, 0x3155);
    AR1330MIPI_write_cmos_sensor(0x3ECC, 0x00AF);
    AR1330MIPI_write_cmos_sensor(0x3ED4, 0xCCAF);
    AR1330MIPI_write_cmos_sensor(0x3EDA, 0x6024);   
    AR1330MIPI_write_cmos_sensor(0x3ED6, 0x8A20);
    AR1330MIPI_write_cmos_sensor(0x3EDC, 0x80C1);
    AR1330MIPI_write_cmos_sensor(0x3EDE, 0x50E0);
    AR1330MIPI_write_cmos_sensor(0x316a, 0x8200);
    AR1330MIPI_write_cmos_sensor(0x3ED0, 0x8853);
    AR1330MIPI_write_cmos_sensor(0x3EFC, 0xB2B7);
    AR1330MIPI_write_cmos_sensor(0x3EE0, 0x0144);
    AR1330MIPI_write_cmos_sensor(0x3EE4, 0xA506);
    AR1330MIPI_write_cmos_sensor(0x316C, 0x8100);
    AR1330MIPI_write_cmos_sensor(0x316E, 0x8100);
    AR1330MIPI_write_cmos_sensor(0x3EFA, 0x8F8F);
    AR1330MIPI_write_cmos_sensor(0x3EFE, 0x8F80);

    //Sequencer Settings
    // @00 Jump Table
    AR1330MIPI_write_cmos_sensor(0x3D00, 0x04FF);
    AR1330MIPI_write_cmos_sensor(0x3D02, 0x5875); 
    AR1330MIPI_write_cmos_sensor(0x3D04, 0xFFFF);
    AR1330MIPI_write_cmos_sensor(0x3D06, 0xFFFF);
    
    // @04 Read
    AR1330MIPI_write_cmos_sensor(0x3D08, 0x8271);
    AR1330MIPI_write_cmos_sensor(0x3D0A, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D0C, 0x5680);
    AR1330MIPI_write_cmos_sensor(0x3D0E, 0x5480);
    AR1330MIPI_write_cmos_sensor(0x3D10, 0x0022);
    AR1330MIPI_write_cmos_sensor(0x3D12, 0x8047);
    AR1330MIPI_write_cmos_sensor(0x3D14, 0x8046);
    AR1330MIPI_write_cmos_sensor(0x3D16, 0x8020);
    AR1330MIPI_write_cmos_sensor(0x3D18, 0x05BC);
    AR1330MIPI_write_cmos_sensor(0x3D1A, 0x5D80);
    AR1330MIPI_write_cmos_sensor(0x3D1C, 0x4280);
    AR1330MIPI_write_cmos_sensor(0x3D1E, 0x4680);
    AR1330MIPI_write_cmos_sensor(0x3D20, 0x4710);
    AR1330MIPI_write_cmos_sensor(0x3D22, 0x068B);
    AR1330MIPI_write_cmos_sensor(0x3D24, 0x5FFF);
    AR1330MIPI_write_cmos_sensor(0x3D26, 0xB859);
    AR1330MIPI_write_cmos_sensor(0x3D28, 0xA650);
    AR1330MIPI_write_cmos_sensor(0x3D2A, 0x9A4F);
    AR1330MIPI_write_cmos_sensor(0x3D2C, 0x8251);
    AR1330MIPI_write_cmos_sensor(0x3D2E, 0x805F);
    AR1330MIPI_write_cmos_sensor(0x3D30, 0x804D);
    AR1330MIPI_write_cmos_sensor(0x3D32, 0x8C43);
    AR1330MIPI_write_cmos_sensor(0x3D34, 0xB643);
    AR1330MIPI_write_cmos_sensor(0x3D36, 0xFF9C);
    AR1330MIPI_write_cmos_sensor(0x3D38, 0x5188);
    AR1330MIPI_write_cmos_sensor(0x3D3A, 0x53BA);
    AR1330MIPI_write_cmos_sensor(0x3D3C, 0x539A);
    AR1330MIPI_write_cmos_sensor(0x3D3E, 0x4FA4);
    AR1330MIPI_write_cmos_sensor(0x3D40, 0x4F80);
    AR1330MIPI_write_cmos_sensor(0x3D42, 0x4280);
    AR1330MIPI_write_cmos_sensor(0x3D44, 0x5280);
    AR1330MIPI_write_cmos_sensor(0x3D46, 0x0081);
    AR1330MIPI_write_cmos_sensor(0x3D48, 0x8000);
    AR1330MIPI_write_cmos_sensor(0x3D4A, 0x4180);
    AR1330MIPI_write_cmos_sensor(0x3D4C, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D4E, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D50, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D52, 0x49AE);
    AR1330MIPI_write_cmos_sensor(0x3D54, 0x4980);
    AR1330MIPI_write_cmos_sensor(0x3D56, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D58, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D5A, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D5C, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D5E, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D60, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D62, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D64, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D66, 0x49AE);
    AR1330MIPI_write_cmos_sensor(0x3D68, 0x4980);
    AR1330MIPI_write_cmos_sensor(0x3D6A, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D6C, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D6E, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D70, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D72, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D74, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D76, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D78, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D7A, 0x49AE);
    AR1330MIPI_write_cmos_sensor(0x3D7C, 0x4980);
    AR1330MIPI_write_cmos_sensor(0x3D7E, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D80, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D82, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D84, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D86, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3D88, 0x4480);
    AR1330MIPI_write_cmos_sensor(0x3D8A, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D8C, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D8E, 0x4980);
    AR1330MIPI_write_cmos_sensor(0x3D90, 0x5980);
    AR1330MIPI_write_cmos_sensor(0x3D92, 0x5490);
    AR1330MIPI_write_cmos_sensor(0x3D94, 0x5298);
    AR1330MIPI_write_cmos_sensor(0x3D96, 0x4980);
    AR1330MIPI_write_cmos_sensor(0x3D98, 0x4A80);
    AR1330MIPI_write_cmos_sensor(0x3D9A, 0x4380);
    AR1330MIPI_write_cmos_sensor(0x3D9C, 0x0030);
    AR1330MIPI_write_cmos_sensor(0x3D9E, 0x8000);
    AR1330MIPI_write_cmos_sensor(0x3DA0, 0x4180);
    AR1330MIPI_write_cmos_sensor(0x3DA2, 0x4780);
    AR1330MIPI_write_cmos_sensor(0x3DA4, 0x4180);
    AR1330MIPI_write_cmos_sensor(0x3DA6, 0x2025);
    AR1330MIPI_write_cmos_sensor(0x3DA8, 0x8256);
    AR1330MIPI_write_cmos_sensor(0x3DAA, 0x8010);
    AR1330MIPI_write_cmos_sensor(0x3DAC, 0x1C82);
    AR1330MIPI_write_cmos_sensor(0x3DAE, 0x7000);
    AR1330MIPI_write_cmos_sensor(0x3DB0, 0x837A);
    AR1330MIPI_write_cmos_sensor(0x3DB2, 0x8E20);
    AR1330MIPI_write_cmos_sensor(0x3DB4, 0x0992);
    AR1330MIPI_write_cmos_sensor(0x3DB6, 0x5C92);
    AR1330MIPI_write_cmos_sensor(0x3DB8, 0x0012);
    AR1330MIPI_write_cmos_sensor(0x3DBA, 0x8278);
    AR1330MIPI_write_cmos_sensor(0x3DBC, 0x8E43);
    AR1330MIPI_write_cmos_sensor(0x3DBE, 0x8004);
    AR1330MIPI_write_cmos_sensor(0x3DC0, 0x8080);
    AR1330MIPI_write_cmos_sensor(0x3DC2, 0x0240);
    AR1330MIPI_write_cmos_sensor(0x3DC4, 0x80AF);
    AR1330MIPI_write_cmos_sensor(0x3DC6, 0x0900);
    AR1330MIPI_write_cmos_sensor(0x3DC8, 0x8002);
    AR1330MIPI_write_cmos_sensor(0x3DCA, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3DCC, 0x0480);
    AR1330MIPI_write_cmos_sensor(0x3DCE, 0xB004);
    AR1330MIPI_write_cmos_sensor(0x3DD0, 0x8080);
    AR1330MIPI_write_cmos_sensor(0x3DD2, 0x0240);
    AR1330MIPI_write_cmos_sensor(0x3DD4, 0x8009);
    AR1330MIPI_write_cmos_sensor(0x3DD6, 0x0080);
    AR1330MIPI_write_cmos_sensor(0x3DD8, 0x7CC5);
    AR1330MIPI_write_cmos_sensor(0x3DDA, 0x9C43);
    AR1330MIPI_write_cmos_sensor(0x3DDC, 0xE202);
    AR1330MIPI_write_cmos_sensor(0x3DDE, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3DE0, 0x0490);
    AR1330MIPI_write_cmos_sensor(0x3DE2, 0x8279);
    AR1330MIPI_write_cmos_sensor(0x3DE4, 0xD741);
    AR1330MIPI_write_cmos_sensor(0x3DE6, 0x2019);
    AR1330MIPI_write_cmos_sensor(0x3DE8, 0x8070);
    AR1330MIPI_write_cmos_sensor(0x3DEA, 0x8271);
    AR1330MIPI_write_cmos_sensor(0x3DEC, 0x4080);
    AR1330MIPI_write_cmos_sensor(0x3DEE, 0x5680);
    AR1330MIPI_write_cmos_sensor(0x3DF0, 0x5480);
    AR1330MIPI_write_cmos_sensor(0x3DF2, 0x0022);
    AR1330MIPI_write_cmos_sensor(0x3DF4, 0x8047);
    AR1330MIPI_write_cmos_sensor(0x3DF6, 0x8046);
    AR1330MIPI_write_cmos_sensor(0x3DF8, 0x8020);
    AR1330MIPI_write_cmos_sensor(0x3DFA, 0x05BC);
    AR1330MIPI_write_cmos_sensor(0x3DFC, 0x5D80);
    AR1330MIPI_write_cmos_sensor(0x3DFE, 0x4280);
    AR1330MIPI_write_cmos_sensor(0x3E00, 0x4680);
    AR1330MIPI_write_cmos_sensor(0x3E02, 0x4710);
    AR1330MIPI_write_cmos_sensor(0x3E04, 0x06FF);
    AR1330MIPI_write_cmos_sensor(0x3E06, 0xC559);
    AR1330MIPI_write_cmos_sensor(0x3E08, 0xA650);
    AR1330MIPI_write_cmos_sensor(0x3E0A, 0x9A4F);
    AR1330MIPI_write_cmos_sensor(0x3E0C, 0x8251);
    AR1330MIPI_write_cmos_sensor(0x3E0E, 0x824D);
    AR1330MIPI_write_cmos_sensor(0x3E10, 0x8C43);
    AR1330MIPI_write_cmos_sensor(0x3E12, 0xB643);
    AR1330MIPI_write_cmos_sensor(0x3E14, 0xCB5F);
    AR1330MIPI_write_cmos_sensor(0x3E16, 0x805F);
    AR1330MIPI_write_cmos_sensor(0x3E18, 0xCD51);
    AR1330MIPI_write_cmos_sensor(0x3E1A, 0x8853);
    AR1330MIPI_write_cmos_sensor(0x3E1C, 0xBA53);
    AR1330MIPI_write_cmos_sensor(0x3E1E, 0x9A4F);
    AR1330MIPI_write_cmos_sensor(0x3E20, 0xA44F);
    AR1330MIPI_write_cmos_sensor(0x3E22, 0x8042);
    AR1330MIPI_write_cmos_sensor(0x3E24, 0x8052);
    AR1330MIPI_write_cmos_sensor(0x3E26, 0x8000);
    AR1330MIPI_write_cmos_sensor(0x3E28, 0x8180);
    AR1330MIPI_write_cmos_sensor(0x3E2A, 0x0041);
    AR1330MIPI_write_cmos_sensor(0x3E2C, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E2E, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E30, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E32, 0x8049);
    AR1330MIPI_write_cmos_sensor(0x3E34, 0xAE49);
    AR1330MIPI_write_cmos_sensor(0x3E36, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E38, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E3A, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E3C, 0x8040);
    AR1330MIPI_write_cmos_sensor(0x3E3E, 0x8040);
    AR1330MIPI_write_cmos_sensor(0x3E40, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E42, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E44, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E46, 0x8049);
    AR1330MIPI_write_cmos_sensor(0x3E48, 0xAE49);
    AR1330MIPI_write_cmos_sensor(0x3E4A, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E4C, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E4E, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E50, 0x8040);
    AR1330MIPI_write_cmos_sensor(0x3E52, 0x8040);
    AR1330MIPI_write_cmos_sensor(0x3E54, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E56, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E58, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E5A, 0x8049);
    AR1330MIPI_write_cmos_sensor(0x3E5C, 0x855F);
    AR1330MIPI_write_cmos_sensor(0x3E5E, 0x805F);
    AR1330MIPI_write_cmos_sensor(0x3E60, 0xA549);
    AR1330MIPI_write_cmos_sensor(0x3E62, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E64, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E66, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E68, 0x8040);
    AR1330MIPI_write_cmos_sensor(0x3E6A, 0x8040);
    AR1330MIPI_write_cmos_sensor(0x3E6C, 0x8044);
    AR1330MIPI_write_cmos_sensor(0x3E6E, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E70, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E72, 0x8049);
    AR1330MIPI_write_cmos_sensor(0x3E74, 0x9452);
    AR1330MIPI_write_cmos_sensor(0x3E76, 0x9849);
    AR1330MIPI_write_cmos_sensor(0x3E78, 0x804A);
    AR1330MIPI_write_cmos_sensor(0x3E7A, 0x8043);
    AR1330MIPI_write_cmos_sensor(0x3E7C, 0x8000);
    AR1330MIPI_write_cmos_sensor(0x3E7E, 0x3080);
    AR1330MIPI_write_cmos_sensor(0x3E80, 0x0041);
    AR1330MIPI_write_cmos_sensor(0x3E82, 0x8047);
    AR1330MIPI_write_cmos_sensor(0x3E84, 0x8041);
    AR1330MIPI_write_cmos_sensor(0x3E86, 0x8020);
    AR1330MIPI_write_cmos_sensor(0x3E88, 0x2580);
    AR1330MIPI_write_cmos_sensor(0x3E8A, 0x5980);
    AR1330MIPI_write_cmos_sensor(0x3E8C, 0x1500);
    AR1330MIPI_write_cmos_sensor(0x3E8E, 0x8010);
    AR1330MIPI_write_cmos_sensor(0x3E90, 0x1C82);
    AR1330MIPI_write_cmos_sensor(0x3E92, 0x7000);
    AR1330MIPI_write_cmos_sensor(0x3E94, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3E96, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3E98, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3E9A, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3E9C, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3E9E, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EA0, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EA2, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EA4, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EA6, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EA8, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EAA, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EAC, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EAE, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EB0, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EB2, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EB4, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EB6, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EB8, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EBA, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EBC, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EBE, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EC0, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EC2, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EC4, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EC6, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3EC8, 0x0000);
    AR1330MIPI_write_cmos_sensor(0x3ECA, 0x0000);

    // PLL_Configuration
    AR1330MIPI_write_cmos_sensor(0x0300, 0x0009);     // vt_pix_clk_div=9
    AR1330MIPI_write_cmos_sensor(0x0302, 0x0001);     // vt_sys_clk_div=1
    AR1330MIPI_write_cmos_sensor(0x0304, 0x0003);     // pre_pll_clk_div=3
    AR1330MIPI_write_cmos_sensor(0x0306, 0x003F);     // pll_multipler=63
    AR1330MIPI_write_cmos_sensor(0x0308, 0x000A);     // op_pix_clk_div=10
    AR1330MIPI_write_cmos_sensor(0x030A, 0x0001);     // op_sys_clk_div=1

    mDELAY(10);

    //MIPI_configuration_settings
    AR1330MIPI_write_cmos_sensor(0x31B0, 0x0042);
    AR1330MIPI_write_cmos_sensor(0x31B2, 0x001A);
    AR1330MIPI_write_cmos_sensor(0x31B4, 0x52CE);
    AR1330MIPI_write_cmos_sensor(0x31B6, 0x4348);
    AR1330MIPI_write_cmos_sensor(0x31B8, 0x2026);
    AR1330MIPI_write_cmos_sensor(0x31BA, 0x186A);
    AR1330MIPI_write_cmos_sensor(0x31BC, 0x850A); 

    //Lens_correction

    //Defect_correction
    AR1330MIPI_write_cmos_sensor(0x3780, 0x8000);
    AR1330MIPI_write_cmos_sensor(0x31E0, 0x17C1);
    AR1330MIPI_write_cmos_sensor(0x31E6, 0x1000);
    AR1330MIPI_write_cmos_sensor(0x3F00, 0x0086);
    AR1330MIPI_write_cmos_sensor(0x3F02, 0x03AF);
    AR1330MIPI_write_cmos_sensor(0x3F04, 0x0020);
    AR1330MIPI_write_cmos_sensor(0x3F06, 0x0040);
    AR1330MIPI_write_cmos_sensor(0x3F08, 0x0070);
    AR1330MIPI_write_cmos_sensor(0x3F0A, 0x0102);
    AR1330MIPI_write_cmos_sensor(0x3F0C, 0x0407);
    AR1330MIPI_write_cmos_sensor(0x3F10, 0x0101);
    AR1330MIPI_write_cmos_sensor(0x3F12, 0x0101);
    AR1330MIPI_write_cmos_sensor(0x3F14, 0x0101);
    AR1330MIPI_write_cmos_sensor(0x3F16, 0x0409);
    AR1330MIPI_write_cmos_sensor(0x3F18, 0x0409);
    AR1330MIPI_write_cmos_sensor(0x3F1A, 0x0409);
    AR1330MIPI_write_cmos_sensor(0x3F1E, 0x0027);
    AR1330MIPI_write_cmos_sensor(0x3F40, 0x0101); 
    AR1330MIPI_write_cmos_sensor(0x3F42, 0x0101);
    AR1330MIPI_write_cmos_sensor(0x3F44, 0x0101);

    //Preview size
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);   // GROUPED_PARAMETER_HOLD
    AR1330MIPI_write_cmos_sensor(0x3004, 0x0008);   // X_ADDR_START_
    AR1330MIPI_write_cmos_sensor(0x3008, 0x1075);   // X_ADDR_END_
    AR1330MIPI_write_cmos_sensor(0x3002, 0x0008);   // Y_ADDR_START_
    AR1330MIPI_write_cmos_sensor(0x3006, 0x0C35);   // Y_ADDR_END_
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C1);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x317A, 0x416E);   // ANALOG_CONTROL6
    AR1330MIPI_write_cmos_sensor(0x0400, 0x0002);   // SCALING_MODE
    AR1330MIPI_write_cmos_sensor(0x0404, 0x0010);   // SCALE_M
    AR1330MIPI_write_cmos_sensor(0x0408, 0x1010);   // SECOND_RESIDUAL
    AR1330MIPI_write_cmos_sensor(0x040A, 0x0210);   // SECOND_CROP
    AR1330MIPI_write_cmos_sensor(0x306E, 0xF010);   // DATAPATH_SELECT
    AR1330MIPI_write_cmos_sensor(0x034C, 0x0838);   // X_OUTPUT_SIZE
    AR1330MIPI_write_cmos_sensor(0x034E, 0x0618);   // Y_OUTPUT_SIZE
    AR1330MIPI_write_cmos_sensor(0x300C, 0x1188);   // LINE_LENGTH_PCK_
    AR1330MIPI_write_cmos_sensor(0x300A, 0x0674);   // FRAME_LENGTH_LINES_
    AR1330MIPI_write_cmos_sensor(0x3012, 0x0673);   // COARSE_INTEGRATION_TIME_
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);   // GROUPED_PARAMETER_HOLD
    AR1330MIPI_write_cmos_sensor_8(0x0100, 0x01);   // MODE_SELECT
#else
//IMAGE= 2104,1560, BAYER-10

   //Software Reset
   AR1330MIPI_write_cmos_sensor_8(0x0103, 0x01);	//SOFTWARE_RESET (clears itself)
   mDELAY(50);		//Initialization Time

   // Interface_configuration
   AR1330MIPI_write_cmos_sensor(0x301A, 0x0018);	 // reset_register
   AR1330MIPI_write_cmos_sensor(0x3064, 0x5840);	 // smia_test
   AR1330MIPI_write_cmos_sensor(0x0112, 0x0A0A);	 // data_format
   AR1330MIPI_write_cmos_sensor(0x31AE, 0x0204);	 // serial_format
   AR1330MIPI_write_cmos_sensor(0x31B0, 0x002D);
   AR1330MIPI_write_cmos_sensor(0x31B2, 0x0011);
   AR1330MIPI_write_cmos_sensor(0x31B4, 0x3188);
   AR1330MIPI_write_cmos_sensor(0x31B6, 0x2104);
   AR1330MIPI_write_cmos_sensor(0x31B8, 0x1023);
   AR1330MIPI_write_cmos_sensor(0x31BA, 0x103A);
   AR1330MIPI_write_cmos_sensor(0x31BC, 0x8305);
   //Rev2_recommended_settings
   AR1330MIPI_write_cmos_sensor(0x3042, 0x1104);
   AR1330MIPI_write_cmos_sensor(0x3044, 0x8580);
   AR1330MIPI_write_cmos_sensor(0x30D2, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x30D4, 0x1040);
   AR1330MIPI_write_cmos_sensor(0x30DE, 0x0002);
   AR1330MIPI_write_cmos_sensor(0x3180, 0xD0FF);
   AR1330MIPI_write_cmos_sensor(0x30EE, 0x3155);
   AR1330MIPI_write_cmos_sensor(0x3ECC, 0x00AF);
   AR1330MIPI_write_cmos_sensor(0x3ED4, 0xCCAF);
   AR1330MIPI_write_cmos_sensor(0x3EDA, 0x6024);   
   AR1330MIPI_write_cmos_sensor(0x3ED6, 0x8A20);
   AR1330MIPI_write_cmos_sensor(0x3EDC, 0x80C1);
   AR1330MIPI_write_cmos_sensor(0x3EDE, 0x50E0);
   AR1330MIPI_write_cmos_sensor(0x316a, 0x8200);
   AR1330MIPI_write_cmos_sensor(0x3ED0, 0x8853);
   AR1330MIPI_write_cmos_sensor(0x3EFC, 0xB2B7);
   AR1330MIPI_write_cmos_sensor(0x3EE0, 0x0144);
   AR1330MIPI_write_cmos_sensor(0x3EE4, 0xA506);
   AR1330MIPI_write_cmos_sensor(0x316C, 0x8100);
   AR1330MIPI_write_cmos_sensor(0x316E, 0x8100);
   AR1330MIPI_write_cmos_sensor(0x3EFA, 0x8F8F);
   AR1330MIPI_write_cmos_sensor(0x3EFE, 0x8F80);

   //Sequencer Settings
   // @00 Jump Table
   AR1330MIPI_write_cmos_sensor(0x3D00, 0x04FF);
   AR1330MIPI_write_cmos_sensor(0x3D02, 0x5875); 
   AR1330MIPI_write_cmos_sensor(0x3D04, 0xFFFF);
   AR1330MIPI_write_cmos_sensor(0x3D06, 0xFFFF);
   
   // @04 Read
   AR1330MIPI_write_cmos_sensor(0x3D08, 0x8271);
   AR1330MIPI_write_cmos_sensor(0x3D0A, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D0C, 0x5680);
   AR1330MIPI_write_cmos_sensor(0x3D0E, 0x5480);
   AR1330MIPI_write_cmos_sensor(0x3D10, 0x0022);
   AR1330MIPI_write_cmos_sensor(0x3D12, 0x8047);
   AR1330MIPI_write_cmos_sensor(0x3D14, 0x8046);
   AR1330MIPI_write_cmos_sensor(0x3D16, 0x8020);
   AR1330MIPI_write_cmos_sensor(0x3D18, 0x05BC);
   AR1330MIPI_write_cmos_sensor(0x3D1A, 0x5D80);
   AR1330MIPI_write_cmos_sensor(0x3D1C, 0x4280);
   AR1330MIPI_write_cmos_sensor(0x3D1E, 0x4680);
   AR1330MIPI_write_cmos_sensor(0x3D20, 0x4710);
   AR1330MIPI_write_cmos_sensor(0x3D22, 0x068B);
   AR1330MIPI_write_cmos_sensor(0x3D24, 0x5FFF);
   AR1330MIPI_write_cmos_sensor(0x3D26, 0xB859);
   AR1330MIPI_write_cmos_sensor(0x3D28, 0xA650);
   AR1330MIPI_write_cmos_sensor(0x3D2A, 0x9A4F);
   AR1330MIPI_write_cmos_sensor(0x3D2C, 0x8251);
   AR1330MIPI_write_cmos_sensor(0x3D2E, 0x805F);
   AR1330MIPI_write_cmos_sensor(0x3D30, 0x804D);
   AR1330MIPI_write_cmos_sensor(0x3D32, 0x8C43);
   AR1330MIPI_write_cmos_sensor(0x3D34, 0xB643);
   AR1330MIPI_write_cmos_sensor(0x3D36, 0xFF9C);
   AR1330MIPI_write_cmos_sensor(0x3D38, 0x5188);
   AR1330MIPI_write_cmos_sensor(0x3D3A, 0x53BA);
   AR1330MIPI_write_cmos_sensor(0x3D3C, 0x539A);
   AR1330MIPI_write_cmos_sensor(0x3D3E, 0x4FA4);
   AR1330MIPI_write_cmos_sensor(0x3D40, 0x4F80);
   AR1330MIPI_write_cmos_sensor(0x3D42, 0x4280);
   AR1330MIPI_write_cmos_sensor(0x3D44, 0x5280);
   AR1330MIPI_write_cmos_sensor(0x3D46, 0x0081);
   AR1330MIPI_write_cmos_sensor(0x3D48, 0x8000);
   AR1330MIPI_write_cmos_sensor(0x3D4A, 0x4180);
   AR1330MIPI_write_cmos_sensor(0x3D4C, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D4E, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D50, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D52, 0x49AE);
   AR1330MIPI_write_cmos_sensor(0x3D54, 0x4980);
   AR1330MIPI_write_cmos_sensor(0x3D56, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D58, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D5A, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D5C, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D5E, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D60, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D62, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D64, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D66, 0x49AE);
   AR1330MIPI_write_cmos_sensor(0x3D68, 0x4980);
   AR1330MIPI_write_cmos_sensor(0x3D6A, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D6C, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D6E, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D70, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D72, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D74, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D76, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D78, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D7A, 0x49AE);
   AR1330MIPI_write_cmos_sensor(0x3D7C, 0x4980);
   AR1330MIPI_write_cmos_sensor(0x3D7E, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D80, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D82, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D84, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D86, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3D88, 0x4480);
   AR1330MIPI_write_cmos_sensor(0x3D8A, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D8C, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D8E, 0x4980);
   AR1330MIPI_write_cmos_sensor(0x3D90, 0x5980);
   AR1330MIPI_write_cmos_sensor(0x3D92, 0x5490);
   AR1330MIPI_write_cmos_sensor(0x3D94, 0x5298);
   AR1330MIPI_write_cmos_sensor(0x3D96, 0x4980);
   AR1330MIPI_write_cmos_sensor(0x3D98, 0x4A80);
   AR1330MIPI_write_cmos_sensor(0x3D9A, 0x4380);
   AR1330MIPI_write_cmos_sensor(0x3D9C, 0x0030);
   AR1330MIPI_write_cmos_sensor(0x3D9E, 0x8000);
   AR1330MIPI_write_cmos_sensor(0x3DA0, 0x4180);
   AR1330MIPI_write_cmos_sensor(0x3DA2, 0x4780);
   AR1330MIPI_write_cmos_sensor(0x3DA4, 0x4180);
   AR1330MIPI_write_cmos_sensor(0x3DA6, 0x2025);
   AR1330MIPI_write_cmos_sensor(0x3DA8, 0x8256);
   AR1330MIPI_write_cmos_sensor(0x3DAA, 0x8010);
   AR1330MIPI_write_cmos_sensor(0x3DAC, 0x1C82);
   AR1330MIPI_write_cmos_sensor(0x3DAE, 0x7000);
   AR1330MIPI_write_cmos_sensor(0x3DB0, 0x837A);
   AR1330MIPI_write_cmos_sensor(0x3DB2, 0x8E20);
   AR1330MIPI_write_cmos_sensor(0x3DB4, 0x0992);
   AR1330MIPI_write_cmos_sensor(0x3DB6, 0x5C92);
   AR1330MIPI_write_cmos_sensor(0x3DB8, 0x0012);
   AR1330MIPI_write_cmos_sensor(0x3DBA, 0x8278);
   AR1330MIPI_write_cmos_sensor(0x3DBC, 0x8E43);
   AR1330MIPI_write_cmos_sensor(0x3DBE, 0x8004);
   AR1330MIPI_write_cmos_sensor(0x3DC0, 0x8080);
   AR1330MIPI_write_cmos_sensor(0x3DC2, 0x0240);
   AR1330MIPI_write_cmos_sensor(0x3DC4, 0x80AF);
   AR1330MIPI_write_cmos_sensor(0x3DC6, 0x0900);
   AR1330MIPI_write_cmos_sensor(0x3DC8, 0x8002);
   AR1330MIPI_write_cmos_sensor(0x3DCA, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3DCC, 0x0480);
   AR1330MIPI_write_cmos_sensor(0x3DCE, 0xB004);
   AR1330MIPI_write_cmos_sensor(0x3DD0, 0x8080);
   AR1330MIPI_write_cmos_sensor(0x3DD2, 0x0240);
   AR1330MIPI_write_cmos_sensor(0x3DD4, 0x8009);
   AR1330MIPI_write_cmos_sensor(0x3DD6, 0x0080);
   AR1330MIPI_write_cmos_sensor(0x3DD8, 0x7CC5);
   AR1330MIPI_write_cmos_sensor(0x3DDA, 0x9C43);
   AR1330MIPI_write_cmos_sensor(0x3DDC, 0xE202);
   AR1330MIPI_write_cmos_sensor(0x3DDE, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3DE0, 0x0490);
   AR1330MIPI_write_cmos_sensor(0x3DE2, 0x8279);
   AR1330MIPI_write_cmos_sensor(0x3DE4, 0xD741);
   AR1330MIPI_write_cmos_sensor(0x3DE6, 0x2019);
   AR1330MIPI_write_cmos_sensor(0x3DE8, 0x8070);
   AR1330MIPI_write_cmos_sensor(0x3DEA, 0x8271);
   AR1330MIPI_write_cmos_sensor(0x3DEC, 0x4080);
   AR1330MIPI_write_cmos_sensor(0x3DEE, 0x5680);
   AR1330MIPI_write_cmos_sensor(0x3DF0, 0x5480);
   AR1330MIPI_write_cmos_sensor(0x3DF2, 0x0022);
   AR1330MIPI_write_cmos_sensor(0x3DF4, 0x8047);
   AR1330MIPI_write_cmos_sensor(0x3DF6, 0x8046);
   AR1330MIPI_write_cmos_sensor(0x3DF8, 0x8020);
   AR1330MIPI_write_cmos_sensor(0x3DFA, 0x05BC);
   AR1330MIPI_write_cmos_sensor(0x3DFC, 0x5D80);
   AR1330MIPI_write_cmos_sensor(0x3DFE, 0x4280);
   AR1330MIPI_write_cmos_sensor(0x3E00, 0x4680);
   AR1330MIPI_write_cmos_sensor(0x3E02, 0x4710);
   AR1330MIPI_write_cmos_sensor(0x3E04, 0x06FF);
   AR1330MIPI_write_cmos_sensor(0x3E06, 0xC559);
   AR1330MIPI_write_cmos_sensor(0x3E08, 0xA650);
   AR1330MIPI_write_cmos_sensor(0x3E0A, 0x9A4F);
   AR1330MIPI_write_cmos_sensor(0x3E0C, 0x8251);
   AR1330MIPI_write_cmos_sensor(0x3E0E, 0x824D);
   AR1330MIPI_write_cmos_sensor(0x3E10, 0x8C43);
   AR1330MIPI_write_cmos_sensor(0x3E12, 0xB643);
   AR1330MIPI_write_cmos_sensor(0x3E14, 0xCB5F);
   AR1330MIPI_write_cmos_sensor(0x3E16, 0x805F);
   AR1330MIPI_write_cmos_sensor(0x3E18, 0xCD51);
   AR1330MIPI_write_cmos_sensor(0x3E1A, 0x8853);
   AR1330MIPI_write_cmos_sensor(0x3E1C, 0xBA53);
   AR1330MIPI_write_cmos_sensor(0x3E1E, 0x9A4F);
   AR1330MIPI_write_cmos_sensor(0x3E20, 0xA44F);
   AR1330MIPI_write_cmos_sensor(0x3E22, 0x8042);
   AR1330MIPI_write_cmos_sensor(0x3E24, 0x8052);
   AR1330MIPI_write_cmos_sensor(0x3E26, 0x8000);
   AR1330MIPI_write_cmos_sensor(0x3E28, 0x8180);
   AR1330MIPI_write_cmos_sensor(0x3E2A, 0x0041);
   AR1330MIPI_write_cmos_sensor(0x3E2C, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E2E, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E30, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E32, 0x8049);
   AR1330MIPI_write_cmos_sensor(0x3E34, 0xAE49);
   AR1330MIPI_write_cmos_sensor(0x3E36, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E38, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E3A, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E3C, 0x8040);
   AR1330MIPI_write_cmos_sensor(0x3E3E, 0x8040);
   AR1330MIPI_write_cmos_sensor(0x3E40, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E42, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E44, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E46, 0x8049);
   AR1330MIPI_write_cmos_sensor(0x3E48, 0xAE49);
   AR1330MIPI_write_cmos_sensor(0x3E4A, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E4C, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E4E, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E50, 0x8040);
   AR1330MIPI_write_cmos_sensor(0x3E52, 0x8040);
   AR1330MIPI_write_cmos_sensor(0x3E54, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E56, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E58, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E5A, 0x8049);
   AR1330MIPI_write_cmos_sensor(0x3E5C, 0x855F);
   AR1330MIPI_write_cmos_sensor(0x3E5E, 0x805F);
   AR1330MIPI_write_cmos_sensor(0x3E60, 0xA549);
   AR1330MIPI_write_cmos_sensor(0x3E62, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E64, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E66, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E68, 0x8040);
   AR1330MIPI_write_cmos_sensor(0x3E6A, 0x8040);
   AR1330MIPI_write_cmos_sensor(0x3E6C, 0x8044);
   AR1330MIPI_write_cmos_sensor(0x3E6E, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E70, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E72, 0x8049);
   AR1330MIPI_write_cmos_sensor(0x3E74, 0x9452);
   AR1330MIPI_write_cmos_sensor(0x3E76, 0x9849);
   AR1330MIPI_write_cmos_sensor(0x3E78, 0x804A);
   AR1330MIPI_write_cmos_sensor(0x3E7A, 0x8043);
   AR1330MIPI_write_cmos_sensor(0x3E7C, 0x8000);
   AR1330MIPI_write_cmos_sensor(0x3E7E, 0x3080);
   AR1330MIPI_write_cmos_sensor(0x3E80, 0x0041);
   AR1330MIPI_write_cmos_sensor(0x3E82, 0x8047);
   AR1330MIPI_write_cmos_sensor(0x3E84, 0x8041);
   AR1330MIPI_write_cmos_sensor(0x3E86, 0x8020);
   AR1330MIPI_write_cmos_sensor(0x3E88, 0x2580);
   AR1330MIPI_write_cmos_sensor(0x3E8A, 0x5980);
   AR1330MIPI_write_cmos_sensor(0x3E8C, 0x1500);
   AR1330MIPI_write_cmos_sensor(0x3E8E, 0x8010);
   AR1330MIPI_write_cmos_sensor(0x3E90, 0x1C82);
   AR1330MIPI_write_cmos_sensor(0x3E92, 0x7000);
   AR1330MIPI_write_cmos_sensor(0x3E94, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3E96, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3E98, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3E9A, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3E9C, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3E9E, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EA0, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EA2, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EA4, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EA6, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EA8, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EAA, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EAC, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EAE, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EB0, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EB2, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EB4, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EB6, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EB8, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EBA, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EBC, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EBE, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EC0, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EC2, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EC4, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EC6, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3EC8, 0x0000);
   AR1330MIPI_write_cmos_sensor(0x3ECA, 0x0000);

   // PLL_Configuration
   AR1330MIPI_write_cmos_sensor(0x0300, 0x0009);	 // vt_pix_clk_div=9
   AR1330MIPI_write_cmos_sensor(0x0302, 0x0001);	 // vt_sys_clk_div=1
   AR1330MIPI_write_cmos_sensor(0x0304, 0x0003);	 // pre_pll_clk_div=3
   AR1330MIPI_write_cmos_sensor(0x0306, 0x003F);	 // pll_multipler=63
   AR1330MIPI_write_cmos_sensor(0x0308, 0x000A);	 // op_pix_clk_div=10
   AR1330MIPI_write_cmos_sensor(0x030A, 0x0001);	 // op_sys_clk_div=1

   mDELAY(1);

   

   //Lens_correction

   //Defect_correction
   AR1330MIPI_write_cmos_sensor(0x31E0, 0x17C1);
   AR1330MIPI_write_cmos_sensor(0x31E6, 0x1000);
   AR1330MIPI_write_cmos_sensor(0x3F00, 0x0086);
   AR1330MIPI_write_cmos_sensor(0x3F02, 0x03AF);
   AR1330MIPI_write_cmos_sensor(0x3F04, 0x0020);
   AR1330MIPI_write_cmos_sensor(0x3F06, 0x0040);
   AR1330MIPI_write_cmos_sensor(0x3F08, 0x0070);
   AR1330MIPI_write_cmos_sensor(0x3F0A, 0x0102);
   AR1330MIPI_write_cmos_sensor(0x3F0C, 0x0407);
   AR1330MIPI_write_cmos_sensor(0x3F10, 0x0101);
   AR1330MIPI_write_cmos_sensor(0x3F12, 0x0101);
   AR1330MIPI_write_cmos_sensor(0x3F14, 0x0101);
   AR1330MIPI_write_cmos_sensor(0x3F16, 0x0409);
   AR1330MIPI_write_cmos_sensor(0x3F18, 0x0409);
   AR1330MIPI_write_cmos_sensor(0x3F1A, 0x0409);
   AR1330MIPI_write_cmos_sensor(0x3F1E, 0x0027);
   AR1330MIPI_write_cmos_sensor(0x3F40, 0x0101); 
   AR1330MIPI_write_cmos_sensor(0x3F42, 0x0101);
   AR1330MIPI_write_cmos_sensor(0x3F44, 0x0101);

   //Preview size
   AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);   // GROUPED_PARAMETER_HOLD
   AR1330MIPI_write_cmos_sensor(0x3004, 0x0008);   // X_ADDR_START_
   AR1330MIPI_write_cmos_sensor(0x3008, 0x1075);   // X_ADDR_END_
   AR1330MIPI_write_cmos_sensor(0x3002, 0x0008);   // Y_ADDR_START_
   AR1330MIPI_write_cmos_sensor(0x3006, 0x0C35);   // Y_ADDR_END_
   AR1330MIPI_write_cmos_sensor(0x3040, 0x00C1);   // READ_MODE
   AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
   AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
   AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
   AR1330MIPI_write_cmos_sensor(0x317A, 0x416E);   // ANALOG_CONTROL6
   AR1330MIPI_write_cmos_sensor(0x0400, 0x0002);   // SCALING_MODE
   AR1330MIPI_write_cmos_sensor(0x0404, 0x0010);   // SCALE_M
   AR1330MIPI_write_cmos_sensor(0x0408, 0x1010);   // SECOND_RESIDUAL
   AR1330MIPI_write_cmos_sensor(0x040A, 0x0210);   // SECOND_CROP
   AR1330MIPI_write_cmos_sensor(0x306E, 0xF010);   // DATAPATH_SELECT
   AR1330MIPI_write_cmos_sensor(0x034C, 0x0838);   // X_OUTPUT_SIZE
   AR1330MIPI_write_cmos_sensor(0x034E, 0x0618);   // Y_OUTPUT_SIZE
   AR1330MIPI_write_cmos_sensor(0x300C, 0x1188);   // LINE_LENGTH_PCK_
   AR1330MIPI_write_cmos_sensor(0x300A, 0x0674);   // FRAME_LENGTH_LINES_
   AR1330MIPI_write_cmos_sensor(0x3012, 0x0673);   // COARSE_INTEGRATION_TIME_
   AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);   // GROUPED_PARAMETER_HOLD
   AR1330MIPI_write_cmos_sensor_8(0x0100, 0x01);   // MODE_SELECT

#endif
#else
int totalCnt = 0, len = 0;
	int transfer_len, transac_len=4;
	kal_uint8* pBuf=NULL;
	dma_addr_t dmaHandle;
	pBuf = (kal_uint8*)kmalloc(2048, GFP_KERNEL);
	AR1330MIPI_write_cmos_sensor_8(0x0103, 0x01);	//SOFTWARE_RESET (clears itself)
    mDELAY(50);		//Initialization Time
	memset(pBuf,0,2048);
    totalCnt = ARRAY_SIZE(AR1330_init_part1);
	transfer_len = totalCnt / transac_len;
	len = (transfer_len<<8)|transac_len;    
	SENSORDB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	memcpy(pBuf, &AR1330_init_part1, totalCnt );   
	dmaHandle = dma_map_single(NULL, pBuf, 2048, DMA_TO_DEVICE);	
	AR1330_multi_write_cmos_sensor(dmaHandle, len); 
	dma_unmap_single(NULL, dmaHandle, 2048, DMA_TO_DEVICE);
	mdelay(10);
	memset(pBuf,0,2048);
    totalCnt = ARRAY_SIZE(AR1330_init_part2);
	transfer_len = totalCnt / transac_len;
	len = (transfer_len<<8)|transac_len;    
	SENSORDB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	memcpy(pBuf, &AR1330_init_part2, totalCnt );   
	dmaHandle = dma_map_single(NULL, pBuf, 2048, DMA_TO_DEVICE);	
	AR1330_multi_write_cmos_sensor(dmaHandle, len); 
	dma_unmap_single(NULL, dmaHandle, 2048, DMA_TO_DEVICE);
	kfree(pBuf);
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);	// GROUPED_PARAMETER_HOLD
	AR1330MIPI_write_cmos_sensor(0x3004, 0x0008);	// X_ADDR_START_
	AR1330MIPI_write_cmos_sensor(0x3008, 0x1075);	// X_ADDR_END_
	AR1330MIPI_write_cmos_sensor(0x3002, 0x0008);	// Y_ADDR_START_
	AR1330MIPI_write_cmos_sensor(0x3006, 0x0C35);	// Y_ADDR_END_
	AR1330MIPI_write_cmos_sensor(0x3040, 0x00C1);	// READ_MODE
	AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);	// READ_MODE
	AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);	// READ_MODE
	AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);	// READ_MODE
	AR1330MIPI_write_cmos_sensor(0x317A, 0x416E);	// ANALOG_CONTROL6
	AR1330MIPI_write_cmos_sensor(0x0400, 0x0002);	// SCALING_MODE
	AR1330MIPI_write_cmos_sensor(0x0404, 0x0010);	// SCALE_M
	AR1330MIPI_write_cmos_sensor(0x0408, 0x1010);	// SECOND_RESIDUAL
	AR1330MIPI_write_cmos_sensor(0x040A, 0x0210);	// SECOND_CROP
	AR1330MIPI_write_cmos_sensor(0x306E, 0xF010);	// DATAPATH_SELECT
	AR1330MIPI_write_cmos_sensor(0x034C, 0x0838);	// X_OUTPUT_SIZE
	AR1330MIPI_write_cmos_sensor(0x034E, 0x0618);	// Y_OUTPUT_SIZE
	AR1330MIPI_write_cmos_sensor(0x300C, 0x1188);	// LINE_LENGTH_PCK_
	AR1330MIPI_write_cmos_sensor(0x300A, 0x0674);	// FRAME_LENGTH_LINES_
	AR1330MIPI_write_cmos_sensor(0x3012, 0x0673);	// COARSE_INTEGRATION_TIME_
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);	// GROUPED_PARAMETER_HOLD
	AR1330MIPI_write_cmos_sensor_8(0x0100, 0x01);	// MODE_SELECT

#endif
    SENSORDB( "Exit!");
}   /*  AR1330MIPI_Sensor_Init  */


/*************************************************************************
* FUNCTION
*   AR1330MIPIGetSensorID
*
* DESCRIPTION
*   This function get the sensor ID 
*
* PARAMETERS
*   *sensorID : return the sensor ID 
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 AR1330MIPIGetSensorID(UINT32 *sensorID) 
{
    const kal_uint8 i2c_addr[] = {AR1330MIPI_WRITE_ID_1,AR1330MIPI_WRITE_ID_2 }; 
    kal_uint16 sensor_id = 0xFFFF;
    kal_uint16 i;

    // AR1330 may have different i2c address
    for(i = 0; i < sizeof(i2c_addr) / sizeof(i2c_addr[0]); i++)
    {
        spin_lock(&ar1330mipi_drv_lock);
        Salve_addr = i2c_addr[i];
        spin_unlock(&ar1330mipi_drv_lock);

        SENSORDB( "i2c address is %x ", Salve_addr);
        
        sensor_id = AR1330MIPI_read_cmos_sensor(0x3000);
        
        if(sensor_id == AR1330MIPI_SENSOR_ID)
            break;
    }

    *sensorID  = sensor_id;
        
    SENSORDB("sensor_id is %x ", *sensorID );
 
    if (*sensorID != AR1330MIPI_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    
    return ERROR_NONE;
}

void AR1330MIPI_Init_Parameters(void)
{

		spin_lock(&ar1330mipi_drv_lock);
		AR1330MIPI_sensor.preview_vt_clk = 2240;
		AR1330MIPI_sensor.capture_vt_clk = 3947;
		AR1330MIPI_sensor.line_length= 4488;
		AR1330MIPI_sensor.frame_length=1652;
		AR1330MIPI_sensor.sensor_mode=AR1330MIPI_MODE_INIT;
		AR1330MIPI_sensor.dummy_pixels=0;
		AR1330MIPI_sensor.dummy_lines=0;
		spin_unlock(&ar1330mipi_drv_lock);

}


/*************************************************************************
* FUNCTION
*   AR1330MIPIOpen
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 AR1330MIPIOpen(void)
{
    kal_uint16 sensor_id = 0;

    AR1330MIPIGetSensorID((UINT32 *)(&sensor_id));
    
    SENSORDB("sensor_id is %x ", sensor_id);
    
    if (sensor_id != AR1330MIPI_SENSOR_ID){
        return ERROR_SENSOR_CONNECT_FAIL;
    }

	AR1330MIPI_Init_Parameters();
    AR1330MIPI_Init_setting();

    return ERROR_NONE;
}





/*************************************************************************
* FUNCTION
*   AR1330MIPIClose
*
* DESCRIPTION
*   This function is to turn off sensor module power.
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 AR1330MIPIClose(void)
{
    return ERROR_NONE;
}   /* AR1330MIPIClose() */


void AR1330MIPI_Set_Mirror_Flip(kal_uint8 image_mirror)
{
    SENSORDB("image_mirror = %d", image_mirror);
	kal_uint8 mirror=0;
	mirror=AR1330MIPI_read_cmos_sensor_8(0x0101);
   
    switch (image_mirror)
    {
        case IMAGE_NORMAL:
            AR1330MIPI_write_cmos_sensor_8(0x0101,(mirror&0xFC));
        break;
        case IMAGE_H_MIRROR:
            AR1330MIPI_write_cmos_sensor_8(0x0101,(mirror|0x01));
        break;
        case IMAGE_V_MIRROR:
            AR1330MIPI_write_cmos_sensor_8(0x0101,(mirror|0x02));
        break;
        case IMAGE_HV_MIRROR:
            AR1330MIPI_write_cmos_sensor_8(0x0101,(mirror|0x03));
        break;
    }
}


static void AR1330MIPI_preview_setting(void)
{
    SENSORDB( "Enter!");

    //[Preview 2104 x 1560 30.2fps]
    //STATE= Master Clock, 224000000 
    //IMAGE= 2104,1560, BAYER-10
    
    AR1330MIPI_write_cmos_sensor_8(0x0100, 0x00);     // mode_select
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);     // group_parameter_hold

    // PLL_Configuration
    AR1330MIPI_write_cmos_sensor(0x0300, 0x0009);     // vt_pix_clk_div=9
    AR1330MIPI_write_cmos_sensor(0x0302, 0x0001);     // vt_sys_clk_div=1
    AR1330MIPI_write_cmos_sensor(0x0304, 0x0003);     // pre_pll_clk_div=3
    AR1330MIPI_write_cmos_sensor(0x0306, 0x003F);     // pll_multipler=63
    AR1330MIPI_write_cmos_sensor(0x0308, 0x000A);     // op_pix_clk_div=10
    AR1330MIPI_write_cmos_sensor(0x030A, 0x0001);     // op_sys_clk_div=1

    mDELAY(10);
#if 0
	AR1330MIPI_write_cmos_sensor(0x31B0, 0x002D);
    AR1330MIPI_write_cmos_sensor(0x31B2, 0x0011);
    AR1330MIPI_write_cmos_sensor(0x31B4, 0x3188);
    AR1330MIPI_write_cmos_sensor(0x31B6, 0x2104);
    AR1330MIPI_write_cmos_sensor(0x31B8, 0x1023);
    AR1330MIPI_write_cmos_sensor(0x31BA, 0x103A);
    AR1330MIPI_write_cmos_sensor(0x31BC, 0x8305);
#endif
    AR1330MIPI_write_cmos_sensor(0x3004, 0x0008);   // X_ADDR_START_
    AR1330MIPI_write_cmos_sensor(0x3008, 0x1075);   // X_ADDR_END_
    AR1330MIPI_write_cmos_sensor(0x3002, 0x0008);   // Y_ADDR_START_
    AR1330MIPI_write_cmos_sensor(0x3006, 0x0C35);   // Y_ADDR_END_
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C1);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x3040, 0x00C3);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x317A, 0x416E);   // ANALOG_CONTROL6
    AR1330MIPI_write_cmos_sensor(0x0400, 0x0002);   // SCALING_MODE
    AR1330MIPI_write_cmos_sensor(0x0404, 0x0010);   // SCALE_M
    AR1330MIPI_write_cmos_sensor(0x0408, 0x1010);   // SECOND_RESIDUAL
    AR1330MIPI_write_cmos_sensor(0x040A, 0x0210);   // SECOND_CROP
    AR1330MIPI_write_cmos_sensor(0x306E, 0xF010);   // DATAPATH_SELECT
    AR1330MIPI_write_cmos_sensor(0x034C, 0x0838);   // X_OUTPUT_SIZE
    AR1330MIPI_write_cmos_sensor(0x034E, 0x0618);   // Y_OUTPUT_SIZE
    AR1330MIPI_write_cmos_sensor(0x300C, 0x1188);   // LINE_LENGTH_PCK_ 0x1188
    AR1330MIPI_write_cmos_sensor(0x300A, 0x0674);   // FRAME_LENGTH_LINES_
    AR1330MIPI_write_cmos_sensor(0x3012, 0x0673);   // COARSE_INTEGRATION_TIME_
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);   // GROUPED_PARAMETER_HOLD
    AR1330MIPI_write_cmos_sensor_8(0x0100, 0x01);   // MODE_SELECT

    spin_lock(&ar1330mipi_drv_lock);
    AR1330MIPI_sensor.preview_vt_clk = 2240;
    spin_unlock(&ar1330mipi_drv_lock); 

    SENSORDB( "Exit!");
}

static void AR1330MIPI_capture_setting(void)
{
    SENSORDB( "AR1330 capture setting Enter!");

    //[Capture 4208 x 3120@24fps]
    //STATE= Master Clock, 394666667
    //IMAGE= 4208,3120, BAYER-10
    
    AR1330MIPI_write_cmos_sensor_8(0x0100, 0x00);     // mode_select
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01);     // group_parameter_hold
#if 0
	AR1330MIPI_write_cmos_sensor(0x31B0, 0x003E);
    AR1330MIPI_write_cmos_sensor(0x31B2, 0x0018);
    AR1330MIPI_write_cmos_sensor(0x31B4, 0x428C);
    AR1330MIPI_write_cmos_sensor(0x31B6, 0x42E7);
    AR1330MIPI_write_cmos_sensor(0x31B8, 0x1C25);
    AR1330MIPI_write_cmos_sensor(0x31BA, 0x185B);
    AR1330MIPI_write_cmos_sensor(0x31BC, 0x8489);
#endif

    // PLL_Configuration
    AR1330MIPI_write_cmos_sensor(0x0300, 0x0009);     // vt_pix_clk_div=9
    AR1330MIPI_write_cmos_sensor(0x0302, 0x0001);     // vt_sys_clk_div=1
    AR1330MIPI_write_cmos_sensor(0x0304, 0x0003);     // pre_pll_clk_div=3
    AR1330MIPI_write_cmos_sensor(0x0306, 0x006E);     // pll_multipler=111
    AR1330MIPI_write_cmos_sensor(0x0308, 0x000A);     // op_pix_clk_div=10
    AR1330MIPI_write_cmos_sensor(0x030A, 0x0001);     // op_sys_clk_div=1
    
    mDELAY(10);
    AR1330MIPI_write_cmos_sensor(0x3004, 0x0008);   // X_ADDR_START_
    AR1330MIPI_write_cmos_sensor(0x3008, 0x1077);   // X_ADDR_END_
    AR1330MIPI_write_cmos_sensor(0x3002, 0x0008);   // Y_ADDR_START_
    AR1330MIPI_write_cmos_sensor(0x3006, 0x0C37);   // Y_ADDR_END_
    AR1330MIPI_write_cmos_sensor(0x3040, 0x0041);   // READ_MODE
    AR1330MIPI_write_cmos_sensor(0x0400, 0x0002);   // SCALING_MODE
    AR1330MIPI_write_cmos_sensor(0x0404, 0x0010);   // SCALE_M
    AR1330MIPI_write_cmos_sensor(0x0408, 0x1010);   // SECOND_RESIDUAL
    AR1330MIPI_write_cmos_sensor(0x040A, 0x0210);   // SECOND_CROP
    AR1330MIPI_write_cmos_sensor(0x306E, 0xF010);   // DATAPATH_SELECT
    AR1330MIPI_write_cmos_sensor(0x034C, 0x1070);   // X_OUTPUT_SIZE
    AR1330MIPI_write_cmos_sensor(0x034E, 0x0C30);   // Y_OUTPUT_SIZE
    AR1330MIPI_write_cmos_sensor(0x300C, 0x1408);   // LINE_LENGTH_PCK_
    AR1330MIPI_write_cmos_sensor(0x300A, 0x0C8B);   // FRAME_LENGTH_LINES_
    AR1330MIPI_write_cmos_sensor(0x3012, 0x0C8A);   // COARSE_INTEGRATION_TIME_
    AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00);   // GROUPED_PARAMETER_HOLD
    AR1330MIPI_write_cmos_sensor_8(0x0100, 0x01);   // MODE_SELECT
    
    spin_lock(&ar1330mipi_drv_lock);
    AR1330MIPI_sensor.capture_vt_clk = 3947;
    spin_unlock(&ar1330mipi_drv_lock);

    SENSORDB( "Exit!");
}


/*************************************************************************
* FUNCTION
*   AR1330MIPI_SetDummy
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   mode  ture : preview mode
*             false : capture mode
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void AR1330MIPI_SetDummy(const kal_uint16 iDummyPixels, const kal_uint16 iDummyLines)
{

	SENSORDB("Enter!");
	spin_lock(&ar1330mipi_drv_lock);
    AR1330MIPI_sensor.dummy_pixels=iDummyPixels;
	AR1330MIPI_sensor.dummy_lines=iDummyLines;
	spin_unlock(&ar1330mipi_drv_lock);
	if(AR1330MIPI_sensor.sensor_mode==AR1330MIPI_MODE_PREVIEW)
	{
		spin_lock(&ar1330mipi_drv_lock);
		AR1330MIPI_sensor.line_length=AR1330MIPI_PV_PERIOD_PIXEL_NUMS+iDummyPixels;
		AR1330MIPI_sensor.frame_length=AR1330MIPI_PV_PERIOD_LINE_NUMS+iDummyLines;
		spin_unlock(&ar1330mipi_drv_lock);
	}
	else if(AR1330MIPI_sensor.sensor_mode==AR1330MIPI_MODE_CAPTURE)
	{
		spin_lock(&ar1330mipi_drv_lock);
		AR1330MIPI_sensor.line_length=AR1330MIPI_FULL_PERIOD_PIXEL_NUMS+iDummyPixels;
		AR1330MIPI_sensor.frame_length=AR1330MIPI_FULL_PERIOD_LINE_NUMS+iDummyLines;
		spin_unlock(&ar1330mipi_drv_lock);
	}
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x01); 
	AR1330MIPI_write_cmos_sensor(0x300C, AR1330MIPI_sensor.line_length);
	AR1330MIPI_write_cmos_sensor(0x300A, AR1330MIPI_sensor.frame_length);
	AR1330MIPI_write_cmos_sensor_8(0x0104, 0x00); 
	SENSORDB("linelength=%0x,framelength=%0x.",AR1330MIPI_sensor.line_length,AR1330MIPI_sensor.frame_length);
    SENSORDB("Exit!");

}   /*  AR1330MIPI_SetDummy */


/*************************************************************************
* FUNCTION
*   AR1330MIPIPreview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 AR1330MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("AR1330 preview Enter!");
   
    spin_lock(&ar1330mipi_drv_lock);
    AR1330MIPI_sensor.sensor_mode= AR1330MIPI_MODE_PREVIEW;
    spin_unlock(&ar1330mipi_drv_lock);
    AR1330MIPI_preview_setting();

   AR1330MIPI_SetDummy(0,20);
	mdelay(40);
    SENSORDB( "Exit!");
    
    return ERROR_NONE;
}   /* AR1330MIPIPreview() */


/*******************************************************************************
*
********************************************************************************/
UINT32 AR1330MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("AR1330 capture Enter!");
	
	spin_lock(&ar1330mipi_drv_lock);
    AR1330MIPI_sensor.sensor_mode= AR1330MIPI_MODE_CAPTURE; 
    spin_unlock(&ar1330mipi_drv_lock);
    AR1330MIPI_capture_setting();
	AR1330MIPI_SetDummy(0,20);
	mdelay(40);
      SENSORDB( "Exit!");
    
    return ERROR_NONE;
}   /* AR1330MIPICapture() */


UINT32 AR1330MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    pSensorResolution->SensorFullWidth     =  AR1330MIPI_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight    =  AR1330MIPI_IMAGE_SENSOR_FULL_HEIGHT;
    
    pSensorResolution->SensorPreviewWidth  =  AR1330MIPI_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight =  AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT;
    
    pSensorResolution->SensorVideoWidth     =  AR1330MIPI_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorVideoHeight    =  AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT;        

    return ERROR_NONE;
}   /* AR1330MIPIGetResolution() */


UINT32 AR1330MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    
	switch(ScenarioId)
		{
		  case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		  case MSDK_SCENARIO_ID_CAMERA_ZSD:
		  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			   pSensorInfo->SensorPreviewResolutionX=AR1330MIPI_IMAGE_SENSOR_FULL_WIDTH;
			   pSensorInfo->SensorPreviewResolutionY=AR1330MIPI_IMAGE_SENSOR_FULL_HEIGHT;
			   pSensorInfo->SensorFullResolutionX=AR1330MIPI_IMAGE_SENSOR_FULL_WIDTH;
			   pSensorInfo->SensorFullResolutionY=AR1330MIPI_IMAGE_SENSOR_FULL_HEIGHT;				   
			   pSensorInfo->SensorCameraPreviewFrameRate=15;
			   break;
		  case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			   pSensorInfo->SensorPreviewResolutionX=AR1330MIPI_IMAGE_SENSOR_PV_WIDTH;
			   pSensorInfo->SensorPreviewResolutionY=AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT;
			   pSensorInfo->SensorFullResolutionX=AR1330MIPI_IMAGE_SENSOR_PV_WIDTH;
			   pSensorInfo->SensorFullResolutionY=AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT;				   
			   pSensorInfo->SensorCameraPreviewFrameRate=15;		  
			  break;
		  default:
	
			   pSensorInfo->SensorPreviewResolutionX=AR1330MIPI_IMAGE_SENSOR_PV_WIDTH;
			   pSensorInfo->SensorPreviewResolutionY=AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT;
			   pSensorInfo->SensorFullResolutionX=AR1330MIPI_IMAGE_SENSOR_PV_WIDTH;
			   pSensorInfo->SensorFullResolutionY=AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT;				   
			   pSensorInfo->SensorCameraPreviewFrameRate=15;
			   break;
				   
		}

    pSensorInfo->SensorVideoFrameRate = 30; /* not use */
    pSensorInfo->SensorStillCaptureFrameRate= 15; /* not use */
    pSensorInfo->SensorWebCamCaptureFrameRate= 15; /* not use */

    pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 1; /* not use */
    pSensorInfo->SensorResetActiveHigh = FALSE; /* not use */
    pSensorInfo->SensorResetDelayCount = 5; /* not use */

    pSensorInfo->SensroInterfaceType        = SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->SensorOutputDataFormat     = AR1330MIPI_DATA_FORMAT;

    pSensorInfo->CaptureDelayFrame = 1; 
    pSensorInfo->PreviewDelayFrame = 2; 
    pSensorInfo->VideoDelayFrame = 3; 
    pSensorInfo->SensorMasterClockSwitch = 0; /* not use */
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;      
    pSensorInfo->AEShutDelayFrame = 0;          /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 1;    /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;
       
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount= 3; /* not use */
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2; /* not use */
            pSensorInfo->SensorPixelClockCount= 3; /* not use */
            pSensorInfo->SensorDataLatchCount= 2; /* not use */
            pSensorInfo->SensorGrabStartX = AR1330MIPI_FULL_START_X; 
            pSensorInfo->SensorGrabStartY = AR1330MIPI_FULL_START_Y;   
            
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->SensorWidthSampling = 0;
            pSensorInfo->SensorHightSampling = 0;
            pSensorInfo->SensorPacketECCOrder = 1;

            break;
        default:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount= 3; /* not use */
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2; /* not use */
            pSensorInfo->SensorPixelClockCount= 3; /* not use */
            pSensorInfo->SensorDataLatchCount= 2; /* not use */
            pSensorInfo->SensorGrabStartX = AR1330MIPI_PV_START_X; 
            pSensorInfo->SensorGrabStartY = AR1330MIPI_PV_START_Y;    
            
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;
            pSensorInfo->SensorHightSampling = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            
            break;
    }

    //memcpy(pSensorConfigData, &AR1330MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* AR1330MIPIGetInfo() */


UINT32 AR1330MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    SENSORDB( "Enter!");

    spin_lock(&ar1330mipi_drv_lock);
    AR1330_CurrentScenarioId = ScenarioId;
    spin_unlock(&ar1330mipi_drv_lock);
    
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            AR1330MIPIPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            AR1330MIPICapture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;  
    }
    
    return ERROR_NONE;
} /* AR1330MIPIControl() */


UINT32 AR1330MIPISetVideoMode(UINT16 u2FrameRate)
{
	kal_int16 dummy_line;
	SENSORDB("Enter,u2FrameRate=%d",u2FrameRate);
    if (u2FrameRate==0)
		return ERROR_NONE;
	
    dummy_line=(AR1330MIPI_sensor.preview_vt_clk*100000/AR1330MIPI_sensor.line_length/u2FrameRate)-AR1330MIPI_PV_PERIOD_LINE_NUMS;
	if(dummy_line<20)
		dummy_line=20;
	AR1330MIPI_SetDummy(0,dummy_line);
    return ERROR_NONE;
}

UINT32 AR1330MIPISetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
    if(bEnable)
    {
    	spin_lock(&ar1330mipi_drv_lock);
    	AR1330MIPI_sensor.autoflicker=KAL_TRUE;
		spin_unlock(&ar1330mipi_drv_lock);
    }
	else
	{
		spin_lock(&ar1330mipi_drv_lock);
    	AR1330MIPI_sensor.autoflicker=KAL_FALSE;
		spin_unlock(&ar1330mipi_drv_lock);
	}
 	SENSORDB("autoflicker=%d",AR1330MIPI_sensor.autoflicker);
    return ERROR_NONE;
}


UINT32 AR1330MIPI_SetTestPatternMode(kal_bool bEnable)
{
    
    SENSORDB("Test pattern enable: %d \n", bEnable);
	if(bEnable)
	{
		AR1330MIPI_write_cmos_sensor(0x0600,0x0203);
	}
	else
	{
		AR1330MIPI_write_cmos_sensor(0x0600,0x0200);
		AR1330MIPI_write_cmos_sensor(0x0600,0x0000);
	}
	
    return ERROR_NONE;
}




UINT32 AR1330MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                                                                UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{    
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    UINT32 SensorRegNumber;
    UINT32 i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++=AR1330MIPI_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16=AR1330MIPI_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++= AR1330MIPI_sensor.line_length;
			*pFeatureReturnPara16++= AR1330MIPI_sensor.frame_length;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            switch(AR1330_CurrentScenarioId)
            {
                case MSDK_SCENARIO_ID_CAMERA_ZSD:
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    *pFeatureReturnPara32 = AR1330MIPI_sensor.capture_vt_clk * 100000;
                    *pFeatureParaLen=4;
                    break;
                default:
                    *pFeatureReturnPara32 = AR1330MIPI_sensor.preview_vt_clk * 100000;
                    *pFeatureParaLen=4;
                    break;
            }
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            AR1330MIPI_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            break;
        case SENSOR_FEATURE_SET_GAIN:
            AR1330MIPI_Set_gain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            // AR1330MIPI_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
			if((pSensorRegData->RegData>>8)!=0)
            	AR1330MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
			else
				AR1330MIPI_write_cmos_sensor_8(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = AR1330MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;   
            for (i=0;i<SensorRegNumber;i++)
            {
                spin_lock(&ar1330mipi_drv_lock);
                AR1330MIPISensorCCT[i].Addr=*pFeatureData32++;
                AR1330MIPISensorCCT[i].Para=*pFeatureData32++;
                spin_unlock(&ar1330mipi_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            
            for (i=0;i<SensorRegNumber;i++)
            {
                spin_lock(&ar1330mipi_drv_lock);
                *pFeatureData32++=AR1330MIPISensorCCT[i].Addr;
                *pFeatureData32++=AR1330MIPISensorCCT[i].Para;
                spin_unlock(&ar1330mipi_drv_lock);
            }
            
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
                spin_lock(&ar1330mipi_drv_lock);
                AR1330MIPISensorReg[i].Addr=*pFeatureData32++;
                AR1330MIPISensorReg[i].Para=*pFeatureData32++;
                spin_unlock(&ar1330mipi_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                spin_lock(&ar1330mipi_drv_lock);
                *pFeatureData32++=AR1330MIPISensorReg[i].Addr;
                *pFeatureData32++=AR1330MIPISensorReg[i].Para;
                spin_unlock(&ar1330mipi_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=AR1330MIPI_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, AR1330MIPISensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, AR1330MIPISensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &AR1330MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            AR1330MIPI_camera_para_to_sensor();
            break;
            
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            AR1330MIPI_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=AR1330MIPI_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            AR1330MIPI_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            AR1330MIPI_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            AR1330MIPI_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 221;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat = AR1330MIPI_DATA_FORMAT;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            AR1330MIPISetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            AR1330MIPIGetSensorID(pFeatureReturnPara32); 
            break; 
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            AR1330MIPISetAutoFlickerMode((BOOL) *pFeatureData16, *(pFeatureData16 + 1));
            break; 
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            AR1330MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            AR1330MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
            break;      

        case SENSOR_FEATURE_SET_TEST_PATTERN:
            AR1330MIPI_SetTestPatternMode((BOOL)*pFeatureData16);
             break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing             
            *pFeatureReturnPara32= AR1330MIPI_TEST_PATTERN_CHECKSUM;
            *pFeatureParaLen=4;                             
             break;  
        default:
            break;
    }
    
    return ERROR_NONE;
}   /* AR1330MIPIFeatureControl() */


SENSOR_FUNCTION_STRUCT  SensorFuncAR1330MIPI=
{
    AR1330MIPIOpen,
    AR1330MIPIGetInfo,
    AR1330MIPIGetResolution,
    AR1330MIPIFeatureControl,
    AR1330MIPIControl,
    AR1330MIPIClose
};

UINT32 AR1330_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncAR1330MIPI;

    return ERROR_NONE;
}/* SensorInit() */

