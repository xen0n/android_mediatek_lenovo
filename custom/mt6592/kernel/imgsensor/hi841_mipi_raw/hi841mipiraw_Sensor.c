/*******************************************************************************************/
// schedule
//   getsensorid ok
//   open ok
//   setting(pv,cap,video) ok

/*******************************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "hi841mipiraw_Sensor.h"
#include "hi841mipiraw_Camera_Sensor_para.h"
#include "hi841mipiraw_CameraCustomized.h"
static DEFINE_SPINLOCK(HI841mipiraw_drv_lock);

#define HI841_TEST_PATTERN_CHECKSUM (0xd6ac9045)//do rotate will change this value

#define HI841_DEBUG
//#define HI841_DEBUG_SOFIA

#ifdef HI841_DEBUG
	#define HI841DB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[HI841Raw] ",  fmt, ##arg)
#else
	#define HI841DB(fmt, arg...)
#endif

#ifdef HI841_DEBUG_SOFIA
	#define HI841DBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[HI841Raw] ",  fmt, ##arg)
#else
	#define HI841DBSOFIA(fmt, arg...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 HI841_FeatureControl_PERIOD_PixelNum=HI841_PV_PERIOD_PIXEL_NUMS;
kal_uint32 HI841_FeatureControl_PERIOD_LineNum=HI841_PV_PERIOD_LINE_NUMS;

UINT16 VIDEO_MODE_TARGET_FPS = 30;
static BOOL ReEnteyCamera = KAL_FALSE;


MSDK_SENSOR_CONFIG_STRUCT HI841SensorConfigData;

kal_uint32 HI841_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM HI841CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT HI841SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT HI841SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static HI841_PARA_STRUCT HI841;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define HI841_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, HI841MIPI_WRITE_ID)

kal_uint16 HI841_read_cmos_sensor(kal_uint32 addr)
{
kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,HI841MIPI_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)

void HI841_write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = HI841_PV_PERIOD_PIXEL_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;

	HI841DBSOFIA("!!shutter=%d!!!!!\n", shutter);

	if(HI841.HI841AutoFlickerMode == KAL_TRUE)
	{

		if ( SENSOR_MODE_PREVIEW == HI841.sensorMode )  //(g_iHI841_Mode == HI841_MODE_PREVIEW)	//SXGA size output
		{
			line_length = HI841_PV_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
			max_shutter = HI841_PV_PERIOD_LINE_NUMS + HI841.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == HI841.sensorMode ) //add for video_6M setting
		{
			line_length = HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
			max_shutter = HI841_VIDEO_PERIOD_LINE_NUMS + HI841.DummyLines ;
		}
		else
		{
			line_length = HI841_FULL_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
			max_shutter = HI841_FULL_PERIOD_LINE_NUMS + HI841.DummyLines ;
		}

		switch(HI841CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				HI841DBSOFIA("AutoFlickerMode!!! MSDK_SCENARIO_ID_CAMERA_ZSD  0!!\n");
				min_framelength = max_shutter;// capture max_fps 24,no need calculate
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				if(VIDEO_MODE_TARGET_FPS==30)
				{
					min_framelength = (HI841.videoPclk*10000) /(HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/304*10 ;
				}
				else if(VIDEO_MODE_TARGET_FPS==15)
				{
					min_framelength = (HI841.videoPclk*10000) /(HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/148*10 ;
				}
				else
				{
					min_framelength = max_shutter;
				}
				break;
			default:
				min_framelength = (HI841.pvPclk*10000) /(HI841_PV_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/296*10 ;
    			break;
		}

		HI841DBSOFIA("AutoFlickerMode!!! min_framelength for AutoFlickerMode = %d (0x%x)\n",min_framelength,min_framelength);
		HI841DBSOFIA("max framerate(10 base) autofilker = %d\n",(HI841.pvPclk*10000)*10 /line_length/min_framelength);

		if (shutter < 4)
			shutter = 4;

		if (shutter > (max_shutter-4) )
			extra_lines = shutter - max_shutter + 4;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == HI841.sensorMode )	//SXGA size output
		{
			frame_length = HI841_PV_PERIOD_LINE_NUMS+ HI841.DummyLines + extra_lines ;
		}
		else if(SENSOR_MODE_VIDEO == HI841.sensorMode)
		{
			frame_length = HI841_VIDEO_PERIOD_LINE_NUMS+ HI841.DummyLines + extra_lines ;
		}
		else				//QSXGA size output
		{
			frame_length = HI841_FULL_PERIOD_LINE_NUMS + HI841.DummyLines + extra_lines ;
		}
		HI841DBSOFIA("frame_length 0= %d\n",frame_length);

		if (frame_length < min_framelength)
		{
			//shutter = min_framelength - 4;

			switch(HI841CurrentScenarioId)
			{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				extra_lines = min_framelength- (HI841_FULL_PERIOD_LINE_NUMS+ HI841.DummyLines);
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				extra_lines = min_framelength- (HI841_VIDEO_PERIOD_LINE_NUMS+ HI841.DummyLines);
				break;
			default:
				extra_lines = min_framelength- (HI841_PV_PERIOD_LINE_NUMS+ HI841.DummyLines);
    			break;
			}
			frame_length = min_framelength;
		}

		HI841DBSOFIA("frame_length 1= %d\n",frame_length);

		ASSERT(line_length < HI841_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < HI841_MAX_FRAME_LENGTH); 	//0xFFFF
		
		spin_lock_irqsave(&HI841mipiraw_drv_lock,flags);
		HI841.maxExposureLines = frame_length - 4;
		HI841_FeatureControl_PERIOD_PixelNum = line_length;
		HI841_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&HI841mipiraw_drv_lock,flags);


		HI841_write_cmos_sensor(0x0104, 0x1);
		//Set total frame length
		HI841_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		HI841_write_cmos_sensor(0x0341, frame_length & 0xFF);

		//Set shutter (Coarse integration time, uint: lines.)
		HI841_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		HI841_write_cmos_sensor(0x0203, (shutter) & 0xFF);
		
		HI841_write_cmos_sensor(0x0104, 0x0);

		HI841DBSOFIA("frame_length 2= %d\n",frame_length);
		HI841DB("framerate(10 base) = %d\n",(HI841.pvPclk*10000)*10 /line_length/frame_length);

		HI841DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);

	}
	else
	{
		if ( SENSOR_MODE_PREVIEW == HI841.sensorMode )  //(g_iHI841_Mode == HI841_MODE_PREVIEW)	//SXGA size output
		{
			max_shutter = HI841_PV_PERIOD_LINE_NUMS + HI841.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == HI841.sensorMode ) //add for video_6M setting
		{
			max_shutter = HI841_VIDEO_PERIOD_LINE_NUMS + HI841.DummyLines ;
		}
		else
		{
			max_shutter = HI841_FULL_PERIOD_LINE_NUMS + HI841.DummyLines ;
		}

		if (shutter < 4)
			shutter = 4;

		if (shutter > (max_shutter-4) )
			extra_lines = shutter - max_shutter + 4;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == HI841.sensorMode )	//SXGA size output
		{
			line_length = HI841_PV_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
			frame_length = HI841_PV_PERIOD_LINE_NUMS+ HI841.DummyLines + extra_lines ;
		}
		else if( SENSOR_MODE_VIDEO == HI841.sensorMode )
		{
			line_length = HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
			frame_length = HI841_VIDEO_PERIOD_LINE_NUMS + HI841.DummyLines + extra_lines ;
		}
		else				//QSXGA size output
		{
			line_length = HI841_FULL_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
			frame_length = HI841_FULL_PERIOD_LINE_NUMS + HI841.DummyLines + extra_lines ;
		}

		ASSERT(line_length < HI841_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < HI841_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		HI841_write_cmos_sensor(0x0104, 0x1);		
		HI841_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		HI841_write_cmos_sensor(0x0341, frame_length & 0xFF);
		HI841_write_cmos_sensor(0x0104, 0x0);		

		spin_lock_irqsave(&HI841mipiraw_drv_lock,flags);
		HI841.maxExposureLines = frame_length -4;
		HI841_FeatureControl_PERIOD_PixelNum = line_length;
		HI841_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&HI841mipiraw_drv_lock,flags);


		//Set shutter (Coarse integration time, uint: lines.)
		HI841_write_cmos_sensor(0x0104, 0x1);		
		HI841_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		HI841_write_cmos_sensor(0x0203, (shutter) & 0xFF);
		HI841_write_cmos_sensor(0x0104, 0x0);		
		
		//HI841DB("framerate(10 base) = %d\n",(HI841.pvPclk*10000)*10 /line_length/frame_length);
		HI841DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}

}   /* write_HI841_shutter */

/*******************************************************************************
*
********************************************************************************/
static kal_uint16 HI841Reg2Gain(const kal_uint8 iReg)
{
	kal_uint16 iGain = 64;

	iGain = iGain * 256 / (iReg + 32);

	return iGain;
}

/*******************************************************************************
*
********************************************************************************/
static kal_uint16 HI841Gain2Reg(const kal_uint16 Gain)
{
	kal_uint16 iReg;
	kal_uint8 iBaseGain = 64;
	
	iReg = 256 * iBaseGain / Gain - 32;
    return iReg;//HI841. sensorGlobalGain

}


void write_HI841_gain(kal_uint8 gain)
{
#if 1
	HI841_write_cmos_sensor(0x0205, gain);
#endif
	return;
}

/*************************************************************************
* FUNCTION
*    HI841_SetGain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    gain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HI841_SetGain(UINT16 iGain)
{
	unsigned long flags;
    kal_uint16 HI841GlobalGain=0;
	kal_uint16 DigitalGain = 0;

    
	// AG = 256/(B[7:0] + 32)
	// hi841離댕連넣8굡친콰gain,(7 * 255 / 256)굡鑒俚gain
	if(iGain > 4080)  
	{
		iGain = 4080;
	}
	if(iGain < 58)  // gain돨reg離댕令角255
	{
		iGain = 58;
	}

	if(iGain <= 8 * BASEGAIN)
	{
		if(HI841.realGain > 512)
		{
			HI841_write_cmos_sensor(0x020e, 0x1);
			HI841_write_cmos_sensor(0x020f, 0x0);
			HI841_write_cmos_sensor(0x0210, 0x1);
			HI841_write_cmos_sensor(0x0211, 0x0);
			HI841_write_cmos_sensor(0x0212, 0x1);
			HI841_write_cmos_sensor(0x0213, 0x0);
			HI841_write_cmos_sensor(0x0214, 0x1);
			HI841_write_cmos_sensor(0x0215, 0x0);
		}
		
		HI841GlobalGain = HI841Gain2Reg(iGain); 
		spin_lock(&HI841mipiraw_drv_lock);
		HI841.realGain = iGain;
		HI841.sensorGlobalGain =HI841GlobalGain;
		spin_unlock(&HI841mipiraw_drv_lock);

		HI841DB("[HI841_SetGain]HI841.sensorGlobalGain=0x%x,HI841.realGain=%d\n",HI841.sensorGlobalGain,HI841.realGain);

		HI841_write_cmos_sensor(0x0104, 0x1);
		write_HI841_gain(HI841.sensorGlobalGain);	
		HI841_write_cmos_sensor(0x0104, 0x0);
		
	}
	else
	{
		spin_lock(&HI841mipiraw_drv_lock);
		HI841.realGain = iGain;
		spin_unlock(&HI841mipiraw_drv_lock);

	
		DigitalGain = iGain / 2 ;  // DigitalGain = iGain * 256 / 64 / 8

		HI841_write_cmos_sensor(0x0104, 0x1);
		
		HI841_write_cmos_sensor(0x0205, 0);   // 8굡친콰gain


		HI841_write_cmos_sensor(0x020e,(DigitalGain >> 8 ) & 0x7);
		HI841_write_cmos_sensor(0x020f, DigitalGain & 0xff);
		HI841_write_cmos_sensor(0x0210,(DigitalGain >> 8 ) & 0x7);
		HI841_write_cmos_sensor(0x0211, DigitalGain & 0xff);
		HI841_write_cmos_sensor(0x0212,(DigitalGain >> 8 ) & 0x7);
		HI841_write_cmos_sensor(0x0213, DigitalGain & 0xff);	
		HI841_write_cmos_sensor(0x0214,(DigitalGain >> 8 ) & 0x7);
		HI841_write_cmos_sensor(0x0215, DigitalGain & 0xff);	

		HI841_write_cmos_sensor(0x0104, 0x0);

	}
	

	
	return;	
}   /*  HI841_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_HI841_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 read_HI841_gain(void)
{
	kal_uint16 read_gain_anolog=0;
	kal_uint16 read_gain_digital=0;	
	kal_uint16 HI841RealGain_anolog =0;
	kal_uint16 HI841RealGain_digital =0;
	kal_uint16 HI841RealGain =0;

	read_gain_anolog=HI841_read_cmos_sensor(0x0205);
	read_gain_digital= ((HI841_read_cmos_sensor(0x020e) & 0x7 ) << 8) + HI841_read_cmos_sensor(0x020f);
	
	HI841RealGain_anolog = HI841Reg2Gain(read_gain_anolog);
	HI841RealGain_digital = (read_gain_digital >> 2);
	
	HI841RealGain = HI841RealGain_anolog + HI841RealGain_digital;

	spin_lock(&HI841mipiraw_drv_lock);
	HI841.sensorGlobalGain = read_gain_anolog;
	HI841.realGain = HI841RealGain;
	spin_unlock(&HI841mipiraw_drv_lock);
	HI841DB("[read_HI841_gain]HI841RealGain_anolog=0x%x,HI841RealGain_digital=%d\n",HI841RealGain_anolog,HI841RealGain_digital);
	HI841DB("[read_HI841_gain]HI841.sensorGlobalGain=0x%x,HI841.realGain=%d\n",HI841.sensorGlobalGain,HI841.realGain);

	return HI841.sensorGlobalGain;
}  /* read_HI841_gain */


void HI841_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=HI841SensorReg[i].Addr; i++)
    {
        HI841_write_cmos_sensor(HI841SensorReg[i].Addr, HI841SensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=HI841SensorReg[i].Addr; i++)
    {
        HI841_write_cmos_sensor(HI841SensorReg[i].Addr, HI841SensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        HI841_write_cmos_sensor(HI841SensorCCT[i].Addr, HI841SensorCCT[i].Para);
    }
}


/*************************************************************************
* FUNCTION
*    HI841_sensor_to_camera_para
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
void HI841_sensor_to_camera_para(void)
{
    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=HI841SensorReg[i].Addr; i++)
    {
         temp_data = HI841_read_cmos_sensor(HI841SensorReg[i].Addr);
		 spin_lock(&HI841mipiraw_drv_lock);
		 HI841SensorReg[i].Para =temp_data;
		 spin_unlock(&HI841mipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=HI841SensorReg[i].Addr; i++)
    {
        temp_data = HI841_read_cmos_sensor(HI841SensorReg[i].Addr);
		spin_lock(&HI841mipiraw_drv_lock);
		HI841SensorReg[i].Para = temp_data;
		spin_unlock(&HI841mipiraw_drv_lock);
    }
}

/*************************************************************************
* FUNCTION
*    HI841_get_sensor_group_count
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
kal_int32  HI841_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void HI841_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{
   switch (group_idx)
   {
        case PRE_GAIN:
            sprintf((char *)group_name_ptr, "CCT");
            *item_count_ptr = 2;
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
}

void HI841_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
    kal_int16 temp_reg=0;
    kal_uint16 temp_gain=0, temp_addr=0, temp_para=0;

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

            temp_para= HI841SensorCCT[temp_addr].Para;
			//temp_gain= (temp_para/HI841.sensorBaseGain) * 1000;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= HI841_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= HI841_MAX_ANALOG_GAIN * 1000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");

                    //temp_reg=MT9P017SensorReg[CMMCLK_CURRENT_INDEX].Para;
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
                    info_ptr->ItemValue=    111;  //MT9P017_MAX_EXPOSURE_LINES;
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
}



kal_bool HI841_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
//   kal_int16 temp_reg;
   kal_uint16  temp_gain=0,temp_addr=0, temp_para=0;

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

		 temp_gain=((ItemValue*BASEGAIN+500)/1000);			//+500:get closed integer value

		  if(temp_gain>=1*BASEGAIN && temp_gain<=16*BASEGAIN)
          {
//             temp_para=(temp_gain * HI841.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
          }
          else
			  ASSERT(0);

			 HI841DBSOFIA("HI841????????????????????? :\n ");
		  spin_lock(&HI841mipiraw_drv_lock);
          HI841SensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&HI841mipiraw_drv_lock);
          HI841_write_cmos_sensor(HI841SensorCCT[temp_addr].Addr,temp_para);

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
					spin_lock(&HI841mipiraw_drv_lock);
                    HI841_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&HI841mipiraw_drv_lock);
                    break;
                case 1:
                    HI841_write_cmos_sensor(HI841_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
    return KAL_TRUE;
}

static void HI841_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	if ( SENSOR_MODE_PREVIEW == HI841.sensorMode )	//SXGA size output
	{
		line_length = HI841_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = HI841_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_VIDEO== HI841.sensorMode )
	{
		line_length = HI841_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = HI841_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else//QSXGA size output
	{
		line_length = HI841_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = HI841_FULL_PERIOD_LINE_NUMS + iLines;
	}

	//if(HI841.maxExposureLines > frame_length -4 )
	//	return;

	//ASSERT(line_length < HI841_MAX_LINE_LENGTH);		//0xCCCC
	//ASSERT(frame_length < HI841_MAX_FRAME_LENGTH);	//0xFFFF

	//Set total frame length
	HI841_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
	HI841_write_cmos_sensor(0x0341, frame_length & 0xFF);

	spin_lock(&HI841mipiraw_drv_lock);
	HI841.maxExposureLines = frame_length -4;
	HI841_FeatureControl_PERIOD_PixelNum = line_length;
	HI841_FeatureControl_PERIOD_LineNum = frame_length;
	spin_unlock(&HI841mipiraw_drv_lock);

	//Set total line length
	HI841_write_cmos_sensor(0x0342, (line_length >> 8) & 0xFF);
	HI841_write_cmos_sensor(0x0343, line_length & 0xFF);

}   /*  HI841_SetDummy */

void HI841PreviewSetting(void)
{	
	HI841DB("HI841PreviewSetting_4lane_30fps:\n ");

	// Preview Mode Setting
	//============================================================
	// MIPI Setting (380Mbps x 4lane)
	HI841_write_cmos_sensor(0x0100, 0x00);	// sleep on
	
	HI841_write_cmos_sensor(0x0800, 0x57);	// tclk_post 234ns
	HI841_write_cmos_sensor(0x0801, 0x1F);	// ths_prepare 85ns
	HI841_write_cmos_sensor(0x0802, 0x1f);	// ths prepare + ths_zero_min 257ns
	HI841_write_cmos_sensor(0x0803, 0x26);	// ths_trail 99ns
	HI841_write_cmos_sensor(0x0804, 0x1F);	// tclk_trail_min 70ns
	HI841_write_cmos_sensor(0x0805, 0x17);	// tclk_prepare 64ns
	HI841_write_cmos_sensor(0x0806, 0x70);	// tclk prepare + tclk_zero 370ns
	HI841_write_cmos_sensor(0x0807, 0x1F);	// tlpx 84ns
					 // clk pre 90ns
	HI841_write_cmos_sensor(0x8006, 0x07);
	HI841_write_cmos_sensor(0x8041, 0x00);
	HI841_write_cmos_sensor(0x8042, 0x00);
	HI841_write_cmos_sensor(0x8043, 0xA0);
	
	// 4 Lane Setting
//	HI841_write_cmos_sensor(0x8405, 0x09);// 08);  // Ramp Clk : B[4]=ramp_clk_sel (/2), B[3:2]=ramp2_div (/1.5), B[1:0] = ramp1_div (/2) ->20121226

	HI841_write_cmos_sensor(0x0307, 0x30);
	// Preview Mode Setting
//	HI841_write_cmos_sensor(0x0303, 0x01);	// vt_sys_clk_div ( 0 : 1/1, 1 : 1/2, 2 : 1/4, 3 : 1/8 )
//	HI841_write_cmos_sensor(0x030B, 0x01);	// op_sys_clk_div ( 0 : 1/1, 1 : 1/2, 2 : 1/4, 3 : 1/8 )
//	HI841_write_cmos_sensor(0x4000, 0x0C);	// tg_ctl1 (variable frame rate)
	HI841_write_cmos_sensor(0x0340, 0x04); // frame_length_lines_Hi
	HI841_write_cmos_sensor(0x0341, 0xea);//e9); //DE); // frame_length_lines_Lo
	HI841_write_cmos_sensor(0x0900, 0x01);	// Binning mode enable
	HI841_write_cmos_sensor(0x034C, 0x06);  // X_output_size_Hi (3264/2 = 1632)
	HI841_write_cmos_sensor(0x034D, 0x60);  // X_output_size_Lo 
	HI841_write_cmos_sensor(0x034E, 0x04);  // Y_output_size_Hi (2448/2 = 1224)
	HI841_write_cmos_sensor(0x034F, 0xC8);  // Y_output_size_Lo 
	HI841_write_cmos_sensor(0x0387, 0x03);  // y_odd_inc 
	
	HI841_write_cmos_sensor(0x8401, 0x30);
	HI841_write_cmos_sensor(0x8401, 0x31);
	HI841_write_cmos_sensor(0x0100, 0x01);	// sleep on

	HI841_write_cmos_sensor(0x8401, 0x11);

	ReEnteyCamera = KAL_FALSE;

    HI841DB("HI841PreviewSetting_4lane exit :\n ");
}


void HI841VideoSetting(void)
{

	HI841DB("HI841VideoSetting:\n ");

	// MIPI Setting (380Mbps x 4lane)
	HI841_write_cmos_sensor(0x0100, 0x00);	// sleep on
	
	HI841_write_cmos_sensor(0x0800, 0x57);	// tclk_post 234ns
	HI841_write_cmos_sensor(0x0801, 0x1F);	// ths_prepare 85ns
	HI841_write_cmos_sensor(0x0802, 0x1f);	// ths prepare + ths_zero_min 257ns
	HI841_write_cmos_sensor(0x0803, 0x26);	// ths_trail 99ns
	HI841_write_cmos_sensor(0x0804, 0x1F);	// tclk_trail_min 70ns
	HI841_write_cmos_sensor(0x0805, 0x17);	// tclk_prepare 64ns
	HI841_write_cmos_sensor(0x0806, 0x70);	// tclk prepare + tclk_zero 370ns
	HI841_write_cmos_sensor(0x0807, 0x1F);	// tlpx 84ns
					 // clk pre 90ns
	HI841_write_cmos_sensor(0x8006, 0x07);
	HI841_write_cmos_sensor(0x8041, 0x00);
	HI841_write_cmos_sensor(0x8042, 0x00);
	HI841_write_cmos_sensor(0x8043, 0xA0);
	
	// 4 Lane Setting
	//HI841_write_cmos_sensor(0x0114, 0x03);  // 4 lane setting
//	HI841_write_cmos_sensor(0x8405, 0x09);	// Ramp Clk : B[4]=ramp_clk_sel (/2), B[3:2]=ramp2_div (/1.5), B[1:0] = ramp1_div (/2) ->20121226
	
	// Video Mode Setting
	HI841_write_cmos_sensor(0x0307, 0x30);
//	HI841_write_cmos_sensor(0x0303, 0x01);	// vt_sys_clk_div ( 0 : 1/1, 1 : 1/2, 2 : 1/4, 3 : 1/8 )
//	HI841_write_cmos_sensor(0x030B, 0x01);	// op_sys_clk_div ( 0 : 1/1, 1 : 1/2, 2 : 1/4, 3 : 1/8 )
//	HI841_write_cmos_sensor(0x4000, 0x0C);	// tg_ctl1 (variable frame rate)
	HI841_write_cmos_sensor(0x0340, 0x04); // frame_length_lines_Hi
	HI841_write_cmos_sensor(0x0341, 0xEa); // frame_length_lines_Lo
	HI841_write_cmos_sensor(0x0900, 0x01);	// Binning mode enable
	HI841_write_cmos_sensor(0x034C, 0x06);  // X_output_size_Hi (3264/2 = 1632)
	HI841_write_cmos_sensor(0x034D, 0x60);  // X_output_size_Lo 
	HI841_write_cmos_sensor(0x034E, 0x04);  // Y_output_size_Hi (2448/2 = 1224)
	HI841_write_cmos_sensor(0x034F, 0xC8);  // Y_output_size_Lo 
	HI841_write_cmos_sensor(0x0387, 0x03);  // y_odd_inc 
	
	HI841_write_cmos_sensor(0x8401, 0x30);
	HI841_write_cmos_sensor(0x8401, 0x31);
	HI841_write_cmos_sensor(0x0100, 0x01);	// sleep on

	HI841_write_cmos_sensor(0x8401, 0x11);

	ReEnteyCamera = KAL_FALSE;

	HI841DB("HI841VideoSetting_4:3 exit :\n ");
}


void HI841CaptureSetting(void)
{

    if(ReEnteyCamera == KAL_TRUE)
    {
		HI841DB("HI841CaptureSetting_4lane_SleepIn :\n ");
    }
	else
	{
		HI841DB("HI841CaptureSetting_4lane_streamOff :\n ");
	}

	HI841DB("HI841CaptureSetting_4lane_OB:\n ");
	// Capture Mode Setting
	HI841_write_cmos_sensor(0x0100, 0x00);	// sleep on
	
	// MIPI Setting
	HI841_write_cmos_sensor(0x0800, 0x77);	// tclk_post 156ns
	HI841_write_cmos_sensor(0x0801, 0x2F);	// ths_prepare 64ns
	HI841_write_cmos_sensor(0x0802, 0x3f);	// ths prepare + ths_zero_min 195ns
	HI841_write_cmos_sensor(0x0803, 0x3f);	// ths_trail 81ns
	HI841_write_cmos_sensor(0x0804, 0x3c);	// tclk_trail_min 67ns
	HI841_write_cmos_sensor(0x0805, 0x2f);	// tclk_prepare 65ns
	HI841_write_cmos_sensor(0x0806, 0xc0);	// tclk prepare + tclk_zero 325ns
	HI841_write_cmos_sensor(0x0807, 0x2F);	// tlpx 62ns
	
	HI841_write_cmos_sensor(0x8006, 0x07);
	HI841_write_cmos_sensor(0x8041, 0x00);
	HI841_write_cmos_sensor(0x8042, 0x00);
	HI841_write_cmos_sensor(0x8043, 0xA0);
	
	// 4 Lane Setting
	//HI841_write_cmos_sensor(0x0114, 0x03);  // 4 lane setting
//	HI841_write_cmos_sensor(0x8405, 0x08);	// Ramp Clk : B[4]=ramp_clk_sel (/2), B[3:2]=ramp2_div (/1.5), B[1:0] = ramp1_div (/2) ->20121226
	HI841_write_cmos_sensor(0x0307, 0x5f);
	// Capture Mode Setting
//	HI841_write_cmos_sensor(0x0303, 0x00);	// vt_sys_clk_div ( 0 : 1/1, 1 : 1/2, 2 : 1/4, 3 : 1/8 )
//	HI841_write_cmos_sensor(0x030B, 0x00);	// op_sys_clk_div ( 0 : 1/1, 1 : 1/2, 2 : 1/4, 3 : 1/8 )
//	HI841_write_cmos_sensor(0x4000, 0x0C);	// tg_ctl1 (B[1] = Line OBP 값 출력, B[0] = Frame OBP 값 출력)
	HI841_write_cmos_sensor(0x0900, 0x00);	//	Binning mode enable
	HI841_write_cmos_sensor(0x0340, 0x09);	//	frame_length_lines_Hi
	HI841_write_cmos_sensor(0x0341, 0xd4);//B8);  //	frame_length_lines_Lo
	HI841_write_cmos_sensor(0x034C, 0x0C);	//	X_output_size_Hi (3264)
	HI841_write_cmos_sensor(0x034D, 0xC0);	//	X_output_size_Lo 
	HI841_write_cmos_sensor(0x034E, 0x09);	//	Y_output_size_Hi (2448)
	HI841_write_cmos_sensor(0x034F, 0x90);	//	Y_output_size_Lo 
	HI841_write_cmos_sensor(0x0387, 0x01);	//	y_odd_inc 

	HI841_write_cmos_sensor(0x8401, 0x30);
	HI841_write_cmos_sensor(0x8401, 0x31);
	HI841_write_cmos_sensor(0x0100, 0x01);	// sleep on

	HI841_write_cmos_sensor(0x8401, 0x11);

	ReEnteyCamera = KAL_FALSE;
	
	HI841DB("HI841CaptureSetting_4lane exit :\n ");
}

static void HI841_Sensor_Init(void)
{

	HI841DB("HI841_Sensor_Init 4lane_OB:\n ");	
	
    ReEnteyCamera = KAL_TRUE;

	mDELAY(20);
	// Initial setting ********
	HI841_write_cmos_sensor(0x8400, 0x03);	// system_enable (Parallel mode enable, System clock enable)
	HI841_write_cmos_sensor(0x0101, 0x01);	// image_orientation : B[1] = 0 (no Yflip), B[0] = 0 (no Xflip)
	
	HI841_write_cmos_sensor(0x0200, 0x00);	// Fine Integration Time
	HI841_write_cmos_sensor(0x0201, 0x80);	// Fine Integration Time
	
	HI841_write_cmos_sensor(0x0342, 0x0F); // line_length_pck_Hi
	HI841_write_cmos_sensor(0x0343, 0xA8); // line_length_pck_Lo
	
	HI841_write_cmos_sensor(0x0344, 0x00); // x_addr_start_Hi
	HI841_write_cmos_sensor(0x0345, 0x28); // x_addr_start_Lo (20, Display시에 20-23 은 잘리고 24 부터 출력)
	HI841_write_cmos_sensor(0x0346, 0x00); // y_addr_start_Hi
	HI841_write_cmos_sensor(0x0347, 0x28); // y_addr_start_Lo (22, Display시에 22-23 은 잘리고 24 부터 출력)
	HI841_write_cmos_sensor(0x0348, 0x0C); // x_addr_end_Hi
	HI841_write_cmos_sensor(0x0349, 0xe7); // x_addr_end_Lo (3323, Display시에 3320-3323 은 잘림)
	HI841_write_cmos_sensor(0x034A, 0x09); // y_addr_end_Hi
	HI841_write_cmos_sensor(0x034B, 0xb7); // y_addr_end_Lo (2505, Display시에 2504-2505 은 잘림)
	
	HI841_write_cmos_sensor(0x7406, 0x00); // fmt_x_start_Hi
	HI841_write_cmos_sensor(0x7407, 0x00); // fmt_x_start_Lo
	
	HI841_write_cmos_sensor(0x0901, 0x20); // binning_type
	
	HI841_write_cmos_sensor(0x0B04, 0x01);	// black_level_correction_enable (B[0] = Frame BLC en, off 시에는 이 레지스터와 함께 0x40A6 B[0] 도 control 필요)
	
	HI841_write_cmos_sensor(0x4098, 0x80); // col_dark_width (128 line)
	HI841_write_cmos_sensor(0x4099, 0x08); // c_drk_addr_s (Line OBP start addr) 
	HI841_write_cmos_sensor(0x409A, 0x08); // row_dark_height (8 line)
	HI841_write_cmos_sensor(0x400A, 0x00); // r_drk_addr_s (Frame OBP start addr)
	HI841_write_cmos_sensor(0x400B, 0x07); // r_drk_addr_e (Frame OBP end addr)
	
	HI841_write_cmos_sensor(0x40A0, 0x01);	// blc_ctl1 (B[0] = Line BLC en, B[2] = Digital offset en 적용하기 위해서는 B[0] 이 enable 되어있어야 함, B[4] = 2x digital offset)
	
	HI841_write_cmos_sensor(0x40A1, 0x3D);	// blc_ctl2 (OBP 영역의 DPC 관련 setting)
	HI841_write_cmos_sensor(0x40A2, 0x80);	// blc_ctl3 (B[7] = dithering en)
	HI841_write_cmos_sensor(0x40A3, 0x00);	// blc_ctl4 
	HI841_write_cmos_sensor(0x40A4, 0x06);	// blc_ctl5 (B[7:6] = 0, 좌우 Line OBPs 모두 사용)
	HI841_write_cmos_sensor(0x40A5, 0x15);	// blc_ctl6 
	HI841_write_cmos_sensor(0x40A6, 0x12);	// blc_ctl7 (B[0] = Frame BLC en, off 시에는 이 레지스터와 함께 0x0B04 B[0] 도 control 필요) 
	HI841_write_cmos_sensor(0x40A7, 0x00);	// blc_ctl8 
	HI841_write_cmos_sensor(0x40AC, 0x03);	// dig_blc_offset_Hi  ->20121224 reset count 증가
	HI841_write_cmos_sensor(0x40AD, 0x30);	// dig_blc_offset_Lo  ->20121224 reset count 증가
	HI841_write_cmos_sensor(0x40AE, 0x08);	// col_blc_dead_pxl_th
	HI841_write_cmos_sensor(0x40BC, 0x10);	// row_blc_dead_pxl_th
	
	HI841_write_cmos_sensor(0x4001, 0x0A);	// tg_ctl2 
	HI841_write_cmos_sensor(0x4002, 0x10);	// tg_ctl3 
	HI841_write_cmos_sensor(0x4003, 0x40);	// tg_ctl4 (B[1] = Min. Coarse Texp (0x1004, 0x1005) 에 의해 Coarse Texp 가 제한되는 것을 풀어줌)
	HI841_write_cmos_sensor(0x4004, 0x00);	// tg_ctl5 
	HI841_write_cmos_sensor(0x4005, 0x00);	// tg_ctl6 (Operation mode control)
	HI841_write_cmos_sensor(0x4007, 0x00);	// data_c_time_opt
	
	HI841_write_cmos_sensor(0x400C, 0x43);	// pd_flush_ctl1 (frame start 시의 flush operation 관련 세팅)
	HI841_write_cmos_sensor(0x400D, 0x88);	// pd_flush_ctl2 (B[4] = flush en after even row readout)
	HI841_write_cmos_sensor(0x400E, 0x37);	// pd_flush_ctl3 (B[2] = rx latch en, B[1] = tx latch en, B[0] = row_idle_ctrl_en)
	HI841_write_cmos_sensor(0x407B, 0x00);	// r_row_dec_ctl (B[3] = pxl_pwr_ctl_n, B[2] = pxl_pwr_ctl_p, B[1:0] = row_sr_clk)
	
	HI841_write_cmos_sensor(0x4100, 0x01);	// 
	HI841_write_cmos_sensor(0x4101, 0x22);	// 
	
	HI841_write_cmos_sensor(0x4033, 0x10);	// analog_func_ctl1 (B[7] = Analog Test Mode en)
	HI841_write_cmos_sensor(0x4034, 0x01);	// analog_func_ctl2 
	HI841_write_cmos_sensor(0x4035, 0x20);	// analog_func_ctl3 
	HI841_write_cmos_sensor(0x4036, 0x00);	// analog_func_ctl4 
	HI841_write_cmos_sensor(0x4037, 0x08);	// r_a_sig_flx_ctl1 (B[3:2] = cds_s1)
	HI841_write_cmos_sensor(0x4038, 0x00);	// r_a_sig_flx_ctl2 (B[1:0] = cds_sxb)
	HI841_write_cmos_sensor(0x4039, 0x03);	// r_a_sig_flx_ctl3 (B[7:6] = cds_rst_clp, B[3:2] = cds_vbias_sample2, B[1:0] = cds_vbias_sample1)
	HI841_write_cmos_sensor(0x403A, 0x00);	// r_a_sig_flx_ctl4 (B[5:4] = col_rst_time_flag)
	HI841_write_cmos_sensor(0x403B, 0x00);	// r_a_sig_flx_ctl5 (B[5:4] = tx0_exp, B[3:2] = tx1_exp, B[1:0] =rx_exp)
	HI841_write_cmos_sensor(0x403C, 0x00);	// r_a_sig_flx_ctl6 (B[3:2] = pxl_pwr_ctl, B[1:0] = tx0_read)
	HI841_write_cmos_sensor(0x403D, 0x00);	// r_a_sig_flx_ctl7 (B[7:6] = tx1_read, B[5:4] = rx_read, B[3:2] = sx_read)
	HI841_write_cmos_sensor(0x40CB, 0x00);	// r_a_sig_flx_ctl11 (B[7:6] = cds_pxbias_sample)
	
	HI841_write_cmos_sensor(0x4048, 0x08);	// r_ldo_ctl (B[6] = d2a_ldo_en)
	HI841_write_cmos_sensor(0x4078, 0x07);	// r_cds_ldo_ctl (B[2:0] = CDS_LDO 의 voltage level 을 control)
	
	///////// PX Setting /////////
	HI841_write_cmos_sensor(0x4049, 0xC2);	// PX Bias En
	HI841_write_cmos_sensor(0x404A, 0x01);	// r_cds_ctl2
	HI841_write_cmos_sensor(0x404B, 0x03);	// r_bias_ctl1 (B[1/0] = pxbias_exp/read control en)
	HI841_write_cmos_sensor(0x404E, 0x22);	// pxl_bias_ctl1 (가능하면, 0x404F/0x4050 도 동일세팅해줄 것, 0x404B 도 0x1F 로 바꾸고.)
	HI841_write_cmos_sensor(0x40CB, 0xC0);	// PX Bias Sampling On/Off -> 20130215
	
	///////// DCDC Setting /////////
	HI841_write_cmos_sensor(0x4058, 0x10);	// dcdc_ctl1 (B[4] = dcdc_en_comp, B[0] = PCP/NCP 를 exp/read 에서 따로 control 가능)
	HI841_write_cmos_sensor(0x4059, 0x13);	// dcdc_ctl2 (B[0] = dcdc_vbb_test_force...)
	HI841_write_cmos_sensor(0x405A, 0xAA);	// dcdc_ctl3 (PCP control)
	HI841_write_cmos_sensor(0x405B, 0xFF);	// dcdc_ctl4 (NCP control)
	HI841_write_cmos_sensor(0x405C, 0x00);	// dcdc_ctl5 (drv_pwr_hv control, rx 에 PCP 공급하려면 0x1B)
	HI841_write_cmos_sensor(0x405D, 0xFF);	// dcdc_ctl6 (drv_pwr_lv control)
	HI841_write_cmos_sensor(0x405E, 0x55);	// dcdc_bias_ctl
	HI841_write_cmos_sensor(0x407C, 0xF1);	// dcdc_ctl7 
	
	///////// CDS Setting /////////
	HI841_write_cmos_sensor(0x4051, 0x00);	// Comp1 Bias Ctrl	
	HI841_write_cmos_sensor(0x4053, 0x55);	// Comp2 Bias Ctrl 
	HI841_write_cmos_sensor(0x4054, 0x54);	// Blacksun Level
	HI841_write_cmos_sensor(0x407D, 0x05);	// Comp2 Input Common
	
	///////// RAMP Setting /////////
	HI841_write_cmos_sensor(0x4041, 0x07);	// ramp_gen_ctl1 (B[5] = ramp_probe_out_en, B[4:0] = ADC input range)
	HI841_write_cmos_sensor(0x4042, 0x09);	// ramp_gen_ctl2 
	HI841_write_cmos_sensor(0x4043, 0x6B);	// ramp_gen_ctl3 (ramp dc offset)
	HI841_write_cmos_sensor(0x4079, 0x0B);	//r_ramp_clt1
	HI841_write_cmos_sensor(0x40CA, 0xCC);	//r_a_sig_flx_ctl10
	
	HI841_write_cmos_sensor(0x40C0, 0x01);	// ramp_ini_pofs_Hi (511, 511 - 511 = 0)
	HI841_write_cmos_sensor(0x40C1, 0xFF);	// ramp_ini_pofs_Lo
	HI841_write_cmos_sensor(0x40C2, 0x01);	// ramp_rst_pofs_Hi (383, 511 - 383 = 128)
	HI841_write_cmos_sensor(0x40C3, 0x4F);	// ramp_rst_pofs_Lo
	HI841_write_cmos_sensor(0x40C4, 0x01);	// ramp_sig_pofs_dark_Hi (256, 511 - 256 = 255) 
	HI841_write_cmos_sensor(0x40C5, 0x00);	// ramp_sig_pofs_dark_Lo
	HI841_write_cmos_sensor(0x40C6, 0x01);	// ramp_sig_pofs_nor_Hi (256, 511 - 256 = 255)
	HI841_write_cmos_sensor(0x40C7, 0x00);	// ramp_sig_pofs_nor_Lo
	HI841_write_cmos_sensor(0x40C8, 0x01);	// ramp_sig_pofs_obp_Hi (256, 511 - 256 = 255)
	HI841_write_cmos_sensor(0x40C9, 0x00);	// ramp_sig_pofs_obp_Lo
	
	HI841_write_cmos_sensor(0x4072, 0x01);	// int_bin_half_p_Hi (650)
	HI841_write_cmos_sensor(0x4073, 0x8A);	// int_bin_half_p_Lo
	HI841_write_cmos_sensor(0x4074, 0x02);	// scn_bin_half_p_Hi (650)
	HI841_write_cmos_sensor(0x4075, 0x8A);	// scn_bin_half_p_Lo
	
	///////////////// PLL Setting /////////////////
	HI841_write_cmos_sensor(0x0301, 0x03);	// vt_pix_clk_div ( 0 : 1/2, 1 : 1/3, 2 : 1/4, 3 : 1/5 )
	HI841_write_cmos_sensor(0x0305, 0x05);	// pre pll divider set
//	HI841_write_cmos_sensor(0x0307, 0x5F);	// pll multiplier

	HI841_write_cmos_sensor(0x0309, 0x03);	// op_pix_clk_div ( 				  2 : 1/4, 3 : 1/5 )
	HI841_write_cmos_sensor(0x8405, 0x08);
		
	HI841_write_cmos_sensor(0x8401, 0x11);	// clk_ctrl (B[0]=1 : PLL_CLK use, B[0]=0 : EXT_CLK use)
	HI841_write_cmos_sensor(0x8404, 0x3A);	// B[5] = PLL select, B[4] = Pll enable

	// PAD REGISTERS
	HI841_write_cmos_sensor(0x7400, 0x60);	// fmt_ctrl1 
	HI841_write_cmos_sensor(0x5810, 0x0C);	// bnr_flt_ctl (Bayer Noise Reduction...)
	HI841_write_cmos_sensor(0x8410, 0x00);	// PAD_ctrl_high_z
	HI841_write_cmos_sensor(0x8411, 0x18);	// PAD_ctrl_strobe
	HI841_write_cmos_sensor(0x8412, 0x18);	// PAD_ctrl_pclk
	HI841_write_cmos_sensor(0x8413, 0x18);	// PAD_ctrl_vsync
	HI841_write_cmos_sensor(0x8414, 0x18);	// PAD_ctrl_hsync
	HI841_write_cmos_sensor(0x8416, 0x3F);	// PAD_ctrl_data_b
	HI841_write_cmos_sensor(0x8417, 0x03);	// PAD_ctrl_sda
	
	/// 20121226 Ramp 190MHz setting /// ->20121226
	HI841_write_cmos_sensor(0x4003, 0x42);	// r_tg_ctl4 (Minimum int. time 제약을 풀어준다)
	
	HI841_write_cmos_sensor(0x4130, 0x00);	// ramp_poffset_n_pos_Hi
	HI841_write_cmos_sensor(0x4131, 0x58);	// ramp_poffset_n_pos_Lo
	HI841_write_cmos_sensor(0x4132, 0x01);	// ramp_poffset_n_neg_Hi
	HI841_write_cmos_sensor(0x4133, 0x0A);	// ramp_poffset_n_neg_Lo
	HI841_write_cmos_sensor(0x4134, 0x03);	// ramp_sig_ofs_ful_neg_Hi
	HI841_write_cmos_sensor(0x4135, 0xDE);	// ramp_sig_ofs_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x4136, 0x00);	// col_rst_time_flag_pos_Hi
	HI841_write_cmos_sensor(0x4137, 0x58);	// col_rst_time_flag_pos_Lo
	HI841_write_cmos_sensor(0x4138, 0x01);	// col_rst_time_flag_neg_Hi
	HI841_write_cmos_sensor(0x4139, 0x0A);	// col_rst_time_flag_neg_Lo
	
	HI841_write_cmos_sensor(0x410E, 0x00);	// ramp_clk_msk1_ful_neg_Hi
	HI841_write_cmos_sensor(0x410F, 0x76);	// ramp_clk_msk1_ful_neg_Lo
	HI841_write_cmos_sensor(0x410C, 0x01);	// ramp_clk_msk1_ful_pos_Hi
	HI841_write_cmos_sensor(0x410D, 0x08);	// ramp_clk_msk1_ful_pos_Lo
	
	HI841_write_cmos_sensor(0x4112, 0x01);	// ramp_clk_msk2_ful_neg_Hi
	HI841_write_cmos_sensor(0x4113, 0x74);	// ramp_clk_msk2_ful_neg_Lo
	HI841_write_cmos_sensor(0x4110, 0x03);	// ramp_clk_msk2_ful_pos_Hi
	HI841_write_cmos_sensor(0x4111, 0xDC);	// ramp_clk_msk2_ful_pos_Lo
	
	HI841_write_cmos_sensor(0x4106, 0x00);	// ramp_preset1_ful_neg_Hi
	HI841_write_cmos_sensor(0x4107, 0x76);	// ramp_preset1_ful_neg_Lo
	HI841_write_cmos_sensor(0x4104, 0x01);	// ramp_preset1_ful_pos_Hi
	HI841_write_cmos_sensor(0x4105, 0x0A);	// ramp_preset1_ful_pos_Lo
	
	HI841_write_cmos_sensor(0x410A, 0x01);	// ramp_preset2_ful_neg_Hi
	HI841_write_cmos_sensor(0x410B, 0x74);	// ramp_preset2_ful_neg_Lo
	HI841_write_cmos_sensor(0x4108, 0x03);	// ramp_preset2_ful_pos_Hi
	HI841_write_cmos_sensor(0x4109, 0xDE);	// ramp_preset2_ful_pos_Lo
	
	HI841_write_cmos_sensor(0x41D0, 0x01);	//ramp_dn_1_pos_Hi
	HI841_write_cmos_sensor(0x41D1, 0x39);	//ramp_dn_1_pos_Lo
	HI841_write_cmos_sensor(0x41D2, 0x01);	//ramp_dn_1_neg_Hi
	HI841_write_cmos_sensor(0x41D3, 0x39);	//ramp_dn_1_neg_Lo
	
	HI841_write_cmos_sensor(0x41D6, 0x00);	// ramp_dn_2_pos_Hi
	HI841_write_cmos_sensor(0x41D7, 0xBF);	// ramp_dn_2_pos_Lo
	HI841_write_cmos_sensor(0x41D4, 0x01);	// ramp_dn_2_neg_Hi
	HI841_write_cmos_sensor(0x41D5, 0x0A);	// ramp_dn_2_neg_Lo
	
	HI841_write_cmos_sensor(0x41D8, 0x03);	//ramp_dn_3_pos_Hi
	HI841_write_cmos_sensor(0x41D9, 0xE1);	//ramp_dn_3_pos_Lo
	HI841_write_cmos_sensor(0x41DA, 0x03);	//ramp_dn_3_neg_Hi
	HI841_write_cmos_sensor(0x41DB, 0xE1);	//ramp_dn_3_neg_Lo
	
	HI841_write_cmos_sensor(0x41DC, 0x03);	// ramp_dn_4_pos_Hi
	HI841_write_cmos_sensor(0x41DD, 0xDE);	// ramp_dn_4_pos_Lo
	HI841_write_cmos_sensor(0x41DE, 0x02);	// ramp_dn_4_neg_Hi
	HI841_write_cmos_sensor(0x41DF, 0xA8);	// ramp_dn_4_neg_Lo
	
	HI841_write_cmos_sensor(0x41E8, 0x00);	// ramp_lsb2c_1_pos_Hi
	HI841_write_cmos_sensor(0x41E9, 0xBF);	// ramp_lsb2c_1_pos_Lo
	HI841_write_cmos_sensor(0x41EA, 0x01);	// ramp_lsb2c_1_neg_Hi
	HI841_write_cmos_sensor(0x41EB, 0x0A);	// ramp_lsb2c_1_neg_Lo
	
	HI841_write_cmos_sensor(0x41EC, 0x02);	// ramp_lsb2c_2_pos_Hi
	HI841_write_cmos_sensor(0x41ED, 0xA8);	// ramp_lsb2c_2_pos_Lo
	HI841_write_cmos_sensor(0x41EE, 0x03);	// ramp_lsb2c_2_neg_Hi
	HI841_write_cmos_sensor(0x41EF, 0xDE);	// ramp_lsb2c_2_neg_Lo
	
	HI841_write_cmos_sensor(0x41B0, 0x00);	// col_init_pos_Hi
	HI841_write_cmos_sensor(0x41B1, 0x0A);	// col_init_pos_Lo
	HI841_write_cmos_sensor(0x41B2, 0x00);	// col_init_neg_Hi
	HI841_write_cmos_sensor(0x41B3, 0x58);	// col_init_neg_Lo
	
	HI841_write_cmos_sensor(0x413E, 0x00);	// int_addr_ful_pos_Hi
	HI841_write_cmos_sensor(0x413F, 0x08);	// int_addr_ful_pos_Lo 
	HI841_write_cmos_sensor(0x4140, 0x00);	// int_addr_ful_neg_Hi
	HI841_write_cmos_sensor(0x4141, 0x2C);	// int_addr_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x4142, 0x00);	// int_en_ful_pos_Hi
	HI841_write_cmos_sensor(0x4143, 0x0A);	// int_en_ful_pos_Lo
	HI841_write_cmos_sensor(0x4144, 0x00);	// int_en_ful_neg_Hi
	HI841_write_cmos_sensor(0x4145, 0x2A);	// int_en_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x414A, 0x00);	// int_rx1_ful_pos_Hi
	HI841_write_cmos_sensor(0x414B, 0x0E);	// int_rx1_ful_pos_Lo
	HI841_write_cmos_sensor(0x414C, 0x00);	// int_rx1_ful_neg_Hi
	HI841_write_cmos_sensor(0x414D, 0x26);	// int_rx1_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x4156, 0x00);	// int_tx1_ful_pos_Hi
	HI841_write_cmos_sensor(0x4157, 0x12);	// int_tx1_ful_pos_Lo
	HI841_write_cmos_sensor(0x4158, 0x00);	// int_tx1_ful_neg_Hi
	HI841_write_cmos_sensor(0x4159, 0x22);	// int_tx1_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x415E, 0x00);	// scn_addr_ful_pos_Hi
	HI841_write_cmos_sensor(0x415F, 0x02);	// scn_addr_ful_pos_Lo 
	HI841_write_cmos_sensor(0x4160, 0x03);	// scn_addr_ful_neg_Hi
	HI841_write_cmos_sensor(0x4161, 0xE6);	// scn_addr_ful_neg_Lo 
	
	HI841_write_cmos_sensor(0x4166, 0x00);	// scn_en_ful_pos_Hi
	HI841_write_cmos_sensor(0x4167, 0x04);	// scn_en_ful_pos_Lo
	HI841_write_cmos_sensor(0x4168, 0x03);	// scn_en_ful_neg_Hi
	HI841_write_cmos_sensor(0x4169, 0xE2);	// scn_en_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x416A, 0x00);	// scn_sx_ful_pos_Hi
	HI841_write_cmos_sensor(0x416B, 0x02);	// scn_sx_ful_pos_Lo   ////////////// 
	HI841_write_cmos_sensor(0x416C, 0x03);	// scn_sx_ful_neg_Hi
	HI841_write_cmos_sensor(0x416D, 0xDE);	// scn_sx_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x41BA, 0x00);	// sxb_ful_neg_Hi
	HI841_write_cmos_sensor(0x41BB, 0x06);	// sxb_ful_neg_Lo
	HI841_write_cmos_sensor(0x41B8, 0x03);	// sxb_ful_pos_Hi
	HI841_write_cmos_sensor(0x41B9, 0xDE);	// sxb_ful_pos_Lo
	
	HI841_write_cmos_sensor(0x4162, 0x00);	// scn_pxl_pwr_ful_pos_Hi
	HI841_write_cmos_sensor(0x4163, 0x2c);	// scn_pxl_pwr_ful_pos_Lo ->20130115 floating node attack 제거
	HI841_write_cmos_sensor(0x4164, 0x00);	// scn_pxl_pwr_ful_neg_Hi
	HI841_write_cmos_sensor(0x4165, 0x2c);	// scn_pxl_pwr_ful_neg_Lo ->20130115 floating node attack 제거
	
	HI841_write_cmos_sensor(0x417A, 0x00);	// scn_rx1_ful_pos_Hi
	HI841_write_cmos_sensor(0x417B, 0x06);	// scn_rx1_ful_pos_Lo	 ///////////
	HI841_write_cmos_sensor(0x417C, 0x00);	// scn_rx1_ful_neg_Hi
	HI841_write_cmos_sensor(0x417D, 0x10);	// scn_rx1_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x4182, 0x03);	// scn_rx3_ful_pos_Hi
	HI841_write_cmos_sensor(0x4183, 0xE0);	// scn_rx3_ful_pos_Lo
	HI841_write_cmos_sensor(0x4184, 0x03);	// scn_rx3_ful_neg_Hi
	HI841_write_cmos_sensor(0x4185, 0xE4);	// scn_rx3_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x416E, 0x01);	// scn_tx1_ful_pos_Hi
	HI841_write_cmos_sensor(0x416F, 0x0A);	// scn_tx1_ful_pos_Lo
	HI841_write_cmos_sensor(0x4170, 0x01);	// scn_tx1_ful_neg_Hi
	HI841_write_cmos_sensor(0x4171, 0x1A);	// scn_tx1_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x4176, 0x03);	// scn_tx3_ful_pos_Hi
	HI841_write_cmos_sensor(0x4177, 0xE0);	// scn_tx3_ful_pos_Lo
	HI841_write_cmos_sensor(0x4178, 0x03);	// scn_tx3_ful_neg_Hi
	HI841_write_cmos_sensor(0x4179, 0xE4);	// scn_tx3_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x418A, 0x00);	// cds_rst_ful_pos_Hi
	HI841_write_cmos_sensor(0x418B, 0x08);	// cds_rst_ful_pos_Lo
	HI841_write_cmos_sensor(0x418C, 0x01);	// cds_rst_ful_neg_Hi
	HI841_write_cmos_sensor(0x418D, 0x0A);	// cds_rst_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x418E, 0x01);	// cds_sig_ful_pos_Hi
	HI841_write_cmos_sensor(0x418F, 0x10);	// cds_sig_ful_pos_Lo
	HI841_write_cmos_sensor(0x4190, 0x03);	// cds_sig_ful_neg_Hi
	HI841_write_cmos_sensor(0x4191, 0xDE);	// cds_sig_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x4192, 0x00);	// cds_s1_1_pos_Hi
	HI841_write_cmos_sensor(0x4193, 0x12);	// cds_s1_1_pos_Lo
	HI841_write_cmos_sensor(0x4194, 0x00);	// cds_s1_1_neg_Hi
	HI841_write_cmos_sensor(0x4195, 0x56);	// cds_s1_1_neg_Lo
	
	HI841_write_cmos_sensor(0x41AA, 0x01);	// cds_s1_2_pos_Hi
	HI841_write_cmos_sensor(0x41AB, 0x1C);	// cds_s1_2_pos_Lo
	HI841_write_cmos_sensor(0x41AC, 0x01);	// cds_s1_2_neg_Hi
	HI841_write_cmos_sensor(0x41AD, 0x72);	// cds_s1_2_neg_Lo
	
	HI841_write_cmos_sensor(0x4196, 0x00);	// cds_s2_ful_pos_Hi
	HI841_write_cmos_sensor(0x4197, 0x30);	// cds_s2_ful_pos_Lo   ////////
	HI841_write_cmos_sensor(0x4198, 0x00);	// cds_s2_ful_neg_Hi
	HI841_write_cmos_sensor(0x4199, 0x50);	// cds_s2_ful_neg_Lo   ////////
	
	HI841_write_cmos_sensor(0x419A, 0x00);	// cds_s3_ful_pos_Hi
	HI841_write_cmos_sensor(0x419B, 0x30);	// cds_s3_ful_pos_Lo   ////////
	HI841_write_cmos_sensor(0x419C, 0x00);	// cds_s3_ful_neg_Hi
	HI841_write_cmos_sensor(0x419D, 0x58);	// cds_s3_ful_neg_Lo   ////////
	
	HI841_write_cmos_sensor(0x4186, 0x00);	// cds_rst_clp_ful_pos_Hi
	HI841_write_cmos_sensor(0x4187, 0x11);	// cds_rst_clp_ful_pos_Lo
	HI841_write_cmos_sensor(0x4188, 0x00);	// cds_rst_clp_ful_neg_Hi
	HI841_write_cmos_sensor(0x4189, 0x58);	// cds_rst_clp_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x419E, 0x00);	// cds_s4_ful_pos_Hi
	HI841_write_cmos_sensor(0x419F, 0x60);	// cds_s4_ful_pos_Lo
	HI841_write_cmos_sensor(0x41A0, 0x03);	// cds_s4_ful_neg_Hi
	HI841_write_cmos_sensor(0x41A1, 0xDE);	// cds_s4_ful_neg_Lo
	
	HI841_write_cmos_sensor(0x41C2, 0x00);	// cds_pxbias_sample_neg_Hi
	HI841_write_cmos_sensor(0x41C3, 0x02);	// cds_pxbias_sample_neg_Lo
	HI841_write_cmos_sensor(0x41C0, 0x00);	// cds_pxbias_sample_pos_Hi
	HI841_write_cmos_sensor(0x41C1, 0x0C);	// cds_pxbias_sample_pos_Lo
	
	HI841_write_cmos_sensor(0x41A4, 0x00);	// cds_cds_vbias_sample1_neg_Hi
	HI841_write_cmos_sensor(0x41A5, 0x02);	// cds_cds_vbias_sample1_neg_Lo
	HI841_write_cmos_sensor(0x41A2, 0x00);	// cds_cds_vbias_sample1_pos_Hi
	HI841_write_cmos_sensor(0x41A3, 0x0C);	// cds_cds_vbias_sample1_pos_Lo
	
	HI841_write_cmos_sensor(0x41A8, 0x00);	// cds_cds_vbias_sample2_neg_Hi
	HI841_write_cmos_sensor(0x41A9, 0x02);	// cds_cds_vbias_sample2_neg_Lo
	HI841_write_cmos_sensor(0x41A6, 0x00);	// cds_cds_vbias_sample2_pos_Hi
	HI841_write_cmos_sensor(0x41A7, 0x0C);	// cds_cds_vbias_sample2_pos_Lo
	
	HI841_write_cmos_sensor(0x41B4, 0x03);	// col_load_ful_pos_Hi
	HI841_write_cmos_sensor(0x41B5, 0xDE);	// col_load_ful_pos_Lo
	HI841_write_cmos_sensor(0x41B6, 0x03);	// col_load_ful_neg_Hi
	HI841_write_cmos_sensor(0x41B7, 0xE6);	// col_load_ful_neg_Lo
	///////////////////////////////////////////
	
	// OTP2ADPC DMA
	HI841_write_cmos_sensor(0x9c02, 0x03);
	HI841_write_cmos_sensor(0x9c03, 0xff);
	HI841_write_cmos_sensor(0x9c04, 0x00);
	HI841_write_cmos_sensor(0x9c05, 0x20);
	HI841_write_cmos_sensor(0x9c06, 0x00);
	HI841_write_cmos_sensor(0x9c07, 0x00);
	HI841_write_cmos_sensor(0x9c08, 0x00);
	HI841_write_cmos_sensor(0x9c00, 0x17);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	HI841_write_cmos_sensor(0x8400, 0x03);
	
	//ADPC Enable
	HI841_write_cmos_sensor(0x5400, 0x01);

	HI841_write_cmos_sensor(0x6000, 0x03);
	
	// SET BNR
	HI841_write_cmos_sensor(0x5810, 0x5b); //bnr_flt_ctl 
	HI841_write_cmos_sensor(0x5811, 0x12); //prewit_rate
	HI841_write_cmos_sensor(0x5812, 0x07); //prewit_rate
	HI841_write_cmos_sensor(0x5813, 0x0C); //nr_rate
	HI841_write_cmos_sensor(0x5814, 0x20); //line_rate
	HI841_write_cmos_sensor(0x5815, 0x07); //med_rate
	HI841_write_cmos_sensor(0x5820, 0x00); //full_drange_div
	HI841_write_cmos_sensor(0x5821, 0x80); //full_drange_cent
	HI841_write_cmos_sensor(0x5822, 0x00); //full_drange_offset
	HI841_write_cmos_sensor(0x5823, 0x00); //full_drange_limit
	HI841_write_cmos_sensor(0x5824, 0x00); //lint_th_div
	HI841_write_cmos_sensor(0x5825, 0x80); //lint_th_cent
	HI841_write_cmos_sensor(0x5826, 0x01); //lint_th_offset
	HI841_write_cmos_sensor(0x5827, 0x0f); //lint_th_limit
	HI841_write_cmos_sensor(0x5830, 0x88); //center_g_edge
	HI841_write_cmos_sensor(0x5831, 0x8c); //slope_l_edge
	HI841_write_cmos_sensor(0x5832, 0x0c); //slope_r_edge
	HI841_write_cmos_sensor(0x5833, 0x20); //offset_1_edge
	HI841_write_cmos_sensor(0x5834, 0x3f); //scala_ratio
	HI841_write_cmos_sensor(0x5835, 0x7f); //limit_g_edge
	HI841_write_cmos_sensor(0x5840, 0x0c); //dark_level
	HI841_write_cmos_sensor(0x5841, 0x0c); //bright_level
	HI841_write_cmos_sensor(0x5842, 0x10); //dark_std_gain
	HI841_write_cmos_sensor(0x5843, 0x1a); //room_std_gain
	HI841_write_cmos_sensor(0x5844, 0x0f); //bright_std_gain
	HI841_write_cmos_sensor(0x5845, 0x0e); //imp_nlevel
	HI841_write_cmos_sensor(0x5846, 0x08); //dark_imp_std
	HI841_write_cmos_sensor(0x5847, 0x0f); //room_imp_std
	HI841_write_cmos_sensor(0x5848, 0x0f); //bright_imp_std
	HI841_write_cmos_sensor(0x5850, 0x06); //gbgr_ctl
	HI841_write_cmos_sensor(0x5851, 0x32); //gbgr_cutoff
	HI841_write_cmos_sensor(0x5852, 0xff); //gbgr_init
	HI841_write_cmos_sensor(0x5853, 0x80); //gbgr_Limit_cent
	HI841_write_cmos_sensor(0x5854, 0x01); //gbgr_limit_rate
	HI841_write_cmos_sensor(0x5855, 0x00); //gbgr_limit_offset
	HI841_write_cmos_sensor(0x5856, 0x0a); //gbgr_limit_max
	
	HI841_write_cmos_sensor(0x0114, 0x03);//3);  // 4 lane setting

	
	HI841DB("HI841_Sensor_Init exit :\n ");
}   /*  HI841_Sensor_Init  */

/*************************************************************************
* FUNCTION
*   HI841Open
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

UINT32 HI841Open(void)
{

	volatile signed int i;
	kal_uint16 sensor_id = 0;

	HI841DB("HI841Open enter :\n ");

	//  Read sensor ID to adjust I2C is OK?
	HI841_write_cmos_sensor(0x8408, 0x0a); //Set I2C Config
	HI841_write_cmos_sensor(0x0103, 0x01); //SW reset
	HI841_write_cmos_sensor(0x0103, 0x00); //SW reset
	HI841_write_cmos_sensor(0x8400, 0x03); //system enable 
	for(i=0;i<3;i++)
	{
		sensor_id = (HI841_read_cmos_sensor(0x0000)<<8)|HI841_read_cmos_sensor(0x0001);
		HI841DB("OHI841 READ ID :%x",sensor_id);
		if(sensor_id != HI841MIPI_SENSOR_ID)
		{
			return ERROR_SENSOR_CONNECT_FAIL;
		}
		else
		{
			break;
		}
	}
	spin_lock(&HI841mipiraw_drv_lock);
	HI841.sensorMode = SENSOR_MODE_INIT;
	HI841.HI841AutoFlickerMode = KAL_FALSE;
	HI841.HI841VideoMode = KAL_FALSE;
	spin_unlock(&HI841mipiraw_drv_lock);
	HI841_Sensor_Init();

	spin_lock(&HI841mipiraw_drv_lock);
	HI841.DummyLines= 0;
	HI841.DummyPixels= 0;
	HI841.pvPclk =  ( 15360); 
	HI841.videoPclk = ( 15360);
	HI841.capPclk = (30400);

	HI841.shutter = 0x4EA;
	HI841.pvShutter = 0x4EA;
	HI841.maxExposureLines =HI841_PV_PERIOD_LINE_NUMS -4;

	HI841.ispBaseGain = BASEGAIN;//0x40
	HI841.sensorGlobalGain = 0x1f;//sensor gain read from 0x350a 0x350b; 0x1f as 3.875x
	HI841.pvGain = 0x1f;
	HI841.realGain = 0x1f;//ispBaseGain as 1x
	spin_unlock(&HI841mipiraw_drv_lock);


	HI841DB("HI841Open exit :\n ");

    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   HI841GetSensorID
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
UINT32 HI841GetSensorID(UINT32 *sensorID)
{
    int  retry = 3;

	HI841DB("HI841GetSensorID enter :\n ");

//	mDELAY(100);
	
	HI841_write_cmos_sensor(0x8408, 0x0a); //Set I2C Config
	HI841_write_cmos_sensor(0x0103, 0x01); //SW reset
	HI841_write_cmos_sensor(0x0103, 0x00); //SW reset
	HI841_write_cmos_sensor(0x8400, 0x03); //system enable 

    // check if sensor ID correct
    do {
        *sensorID = (HI841_read_cmos_sensor(0x0000)<<8)|HI841_read_cmos_sensor(0x0001);
        if (*sensorID == HI841MIPI_SENSOR_ID)
        	{
        		HI841DB("Sensor ID = 0x%04x\n", *sensorID);
            	break;
        	}
        HI841DB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
        retry--;
    } while (retry > 0);
// end
    if (*sensorID != HI841MIPI_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   HI841_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of HI841 to change exposure time.
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
void HI841_SetShutter(kal_uint32 iShutter)
{

//   if(HI841.shutter == iShutter)
//   		return;

   spin_lock(&HI841mipiraw_drv_lock);
   HI841.shutter= iShutter;
   spin_unlock(&HI841mipiraw_drv_lock);

   HI841_write_shutter(iShutter);
   return;
 
}   /*  HI841_SetShutter   */



/*************************************************************************
* FUNCTION
*   HI841_read_shutter
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
UINT32 HI841_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2;
	UINT32 shutter =0;
	temp_reg1 = HI841_read_cmos_sensor(0x0202);  
	temp_reg2 = HI841_read_cmos_sensor(0x0203);    
	//read out register value and divide 16;
	shutter  = (temp_reg1 <<8)| (temp_reg2);

	return shutter;
}

/*************************************************************************
* FUNCTION
*   HI841_night_mode
*
* DESCRIPTION
*   This function night mode of HI841.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HI841_NightMode(kal_bool bEnable)
{
}/*	HI841_NightMode */



/*************************************************************************
* FUNCTION
*   HI841Close
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
UINT32 HI841Close(void)
{
    //  CISModulePowerOn(FALSE);
    //s_porting
    //  DRV_I2CClose(HI841hDrvI2C);
    //e_porting
    ReEnteyCamera = KAL_FALSE;
    return ERROR_NONE;
}	/* HI841Close() */

void HI841SetFlipMirror(kal_int32 imgMirror)
{
#if 1
	kal_int8 iTemp;
	iTemp = HI841_read_cmos_sensor(0x0101);
	
	if((iTemp & 0x3) == imgMirror)
		return;
	iTemp &= 0xFC;
	
    switch (imgMirror)
    {
        case IMAGE_NORMAL://IMAGE_NOMAL:
            HI841_write_cmos_sensor(0x0101, (iTemp | 0x00));//Set normal
            break;    
        case IMAGE_V_MIRROR://IMAGE_V_MIRROR:
            HI841_write_cmos_sensor(0x0101, (iTemp |0x02));	//Set flip
            break;
        case IMAGE_H_MIRROR://IMAGE_H_MIRROR:
            HI841_write_cmos_sensor(0x0101, (iTemp | 0x01));//Set mirror
            break;
        case IMAGE_HV_MIRROR://IMAGE_H_MIRROR:
            HI841_write_cmos_sensor(0x0101, (iTemp |0x03));	//Set mirror & flip
            break;            
    }
#endif	
}


/*************************************************************************
* FUNCTION
*   HI841Preview
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
UINT32 HI841Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	HI841DB("HI841Preview enter:");

	// preview size
	if(HI841.sensorMode == SENSOR_MODE_PREVIEW)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		//HI841DB("HI841Preview setting!!\n");
		HI841PreviewSetting();
	}
	
	spin_lock(&HI841mipiraw_drv_lock);
	HI841.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	HI841.DummyPixels = 0;//define dummy pixels and lines
	HI841.DummyLines = 0 ;
	HI841_FeatureControl_PERIOD_PixelNum=HI841_PV_PERIOD_PIXEL_NUMS+ HI841.DummyPixels;
	HI841_FeatureControl_PERIOD_LineNum=HI841_PV_PERIOD_LINE_NUMS+HI841.DummyLines;
	spin_unlock(&HI841mipiraw_drv_lock);

	spin_lock(&HI841mipiraw_drv_lock);
	HI841.imgMirror = sensor_config_data->SensorImageMirror;
	//HI841.imgMirror =IMAGE_NORMAL; //by module layout
	spin_unlock(&HI841mipiraw_drv_lock);
	
	//HI841SetFlipMirror(sensor_config_data->SensorImageMirror);
	HI841SetFlipMirror(IMAGE_H_MIRROR);
	

	HI841DBSOFIA("[HI841Preview]frame_len=%x\n", ((HI841_read_cmos_sensor(0x380e)<<8)+HI841_read_cmos_sensor(0x380f)));

 //   mDELAY(40);
	HI841DB("HI841Preview exit:\n");
    return ERROR_NONE;
}	/* HI841Preview() */



UINT32 HI841Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	HI841DB("HI841Video enter:");

	if(HI841.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
	{
		HI841VideoSetting();

	}
	spin_lock(&HI841mipiraw_drv_lock);
	HI841.sensorMode = SENSOR_MODE_VIDEO;
	HI841_FeatureControl_PERIOD_PixelNum=HI841_VIDEO_PERIOD_PIXEL_NUMS+ HI841.DummyPixels;
	HI841_FeatureControl_PERIOD_LineNum=HI841_VIDEO_PERIOD_LINE_NUMS+HI841.DummyLines;
	spin_unlock(&HI841mipiraw_drv_lock);


	spin_lock(&HI841mipiraw_drv_lock);
	HI841.imgMirror = sensor_config_data->SensorImageMirror;
	//HI841.imgMirror =IMAGE_NORMAL; //by module layout
	spin_unlock(&HI841mipiraw_drv_lock);
	//HI841SetFlipMirror(sensor_config_data->SensorImageMirror);
	HI841SetFlipMirror(IMAGE_H_MIRROR);

//    mDELAY(40);
	HI841DB("HI841Video exit:\n");
    return ERROR_NONE;
}


UINT32 HI841Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = HI841.shutter;
	kal_uint32 temp_data;
	//kal_uint32 pv_line_length , cap_line_length,

	if( SENSOR_MODE_CAPTURE== HI841.sensorMode)
	{
		HI841DB("HI841Capture BusrtShot!!!\n");
	}
	else
	{
		HI841DB("HI841Capture enter:\n");
#if 0
		//Record Preview shutter & gain
		shutter=HI841_read_shutter();
		temp_data =  read_HI841_gain();
		spin_lock(&HI841mipiraw_drv_lock);
		HI841.pvShutter =shutter;
		HI841.sensorGlobalGain = temp_data;
		HI841.pvGain =HI841.sensorGlobalGain;
		spin_unlock(&HI841mipiraw_drv_lock);

		HI841DB("[HI841Capture]HI841.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",HI841.shutter, shutter,HI841.sensorGlobalGain);
#endif
		// Full size setting
		HI841CaptureSetting();
	//    mDELAY(20);

		spin_lock(&HI841mipiraw_drv_lock);
		HI841.sensorMode = SENSOR_MODE_CAPTURE;
		HI841.imgMirror = sensor_config_data->SensorImageMirror;
		//HI841.imgMirror =IMAGE_NORMAL; //by module layout
		HI841.DummyPixels = 0;//define dummy pixels and lines
		HI841.DummyLines = 0 ;
		HI841_FeatureControl_PERIOD_PixelNum = HI841_FULL_PERIOD_PIXEL_NUMS + HI841.DummyPixels;
		HI841_FeatureControl_PERIOD_LineNum = HI841_FULL_PERIOD_LINE_NUMS + HI841.DummyLines;
		spin_unlock(&HI841mipiraw_drv_lock);

		//HI841SetFlipMirror(sensor_config_data->SensorImageMirror);

		HI841SetFlipMirror(IMAGE_H_MIRROR);

		HI841DB("HI841Capture exit:\n");
	}

    return ERROR_NONE;
}	/* HI841Capture() */

UINT32 HI841GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    HI841DB("HI841GetResolution!!\n");

	pSensorResolution->SensorPreviewWidth	= HI841_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= HI841_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorFullWidth		= HI841_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= HI841_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorVideoWidth		= HI841_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = HI841_IMAGE_SENSOR_VIDEO_HEIGHT;
//    HI841DB("SensorPreviewWidth:  %d.\n", pSensorResolution->SensorPreviewWidth);
//    HI841DB("SensorPreviewHeight: %d.\n", pSensorResolution->SensorPreviewHeight);
//    HI841DB("SensorFullWidth:  %d.\n", pSensorResolution->SensorFullWidth);
//    HI841DB("SensorFullHeight: %d.\n", pSensorResolution->SensorFullHeight);
    return ERROR_NONE;
}   /* HI841GetResolution() */

UINT32 HI841GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	pSensorInfo->SensorPreviewResolutionX= HI841_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY= HI841_IMAGE_SENSOR_PV_HEIGHT;

	pSensorInfo->SensorFullResolutionX= HI841_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= HI841_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&HI841mipiraw_drv_lock);
	HI841.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&HI841mipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gb;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->CaptureDelayFrame = 4;  // from 2 to 3 for test shutter linearity
    pSensorInfo->PreviewDelayFrame = 4;
    pSensorInfo->VideoDelayFrame = 2;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0 ;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = HI841_PV_X_START;
            pSensorInfo->SensorGrabStartY = HI841_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = MIPI_DELAY_COUNT;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = HI841_VIDEO_X_START;
            pSensorInfo->SensorGrabStartY = HI841_VIDEO_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = MIPI_DELAY_COUNT;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = HI841_FULL_X_START;	//2*HI841_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = HI841_FULL_Y_START;	//2*HI841_IMAGE_SENSOR_PV_STARTY;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = MIPI_DELAY_COUNT;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = HI841_PV_X_START;
            pSensorInfo->SensorGrabStartY = HI841_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = MIPI_DELAY_COUNT;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }

    memcpy(pSensorConfigData, &HI841SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* HI841GetInfo() */


UINT32 HI841Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&HI841mipiraw_drv_lock);
		HI841CurrentScenarioId = ScenarioId;
		spin_unlock(&HI841mipiraw_drv_lock);
		//HI841DB("ScenarioId=%d\n",ScenarioId);
		HI841DB("HI841CurrentScenarioId=%d\n",HI841CurrentScenarioId);

	switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            HI841Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			HI841Video(pImageWindow, pSensorConfigData);
			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            HI841Capture(pImageWindow, pSensorConfigData);
            break;

        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
} /* HI841Control() */


UINT32 HI841SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MIN_Frame_length =0,frameRate=0,extralines=0;
    HI841DB("[HI841SetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&HI841mipiraw_drv_lock);
	VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&HI841mipiraw_drv_lock);

	if(u2FrameRate==0)
	{
		HI841DB("Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    HI841DB("error frame rate seting\n");

    if(HI841.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(HI841.HI841AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 304;
			else if(u2FrameRate==15)
				frameRate= 148;//148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (HI841.videoPclk*10000)/(HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/frameRate*10;
    	}
		else
		{
    		if (u2FrameRate==30)
				MIN_Frame_length= HI841_VIDEO_PERIOD_LINE_NUMS;
			else	
				MIN_Frame_length = (HI841.videoPclk*10000) /(HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/u2FrameRate;
		}

		if((MIN_Frame_length <=HI841_VIDEO_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = HI841_VIDEO_PERIOD_LINE_NUMS;
			HI841DB("[HI841SetVideoMode]current fps = %d\n", (HI841.videoPclk*10000)  /(HI841_VIDEO_PERIOD_PIXEL_NUMS)/HI841_VIDEO_PERIOD_LINE_NUMS);
		}
		HI841DB("[HI841SetVideoMode]current fps (10 base)= %d\n", (HI841.videoPclk*10000)*10/(HI841_VIDEO_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/MIN_Frame_length);

		if(HI841.shutter + 4 > MIN_Frame_length)
				MIN_Frame_length = HI841.shutter + 4;

		extralines = MIN_Frame_length - HI841_VIDEO_PERIOD_LINE_NUMS;
		
		spin_lock(&HI841mipiraw_drv_lock);
		HI841.DummyPixels = 0;//define dummy pixels and lines
		HI841.DummyLines = extralines ;
		spin_unlock(&HI841mipiraw_drv_lock);
		
		HI841_SetDummy(HI841.DummyPixels,extralines);
    }
	else if(HI841.sensorMode == SENSOR_MODE_CAPTURE)
	{
		HI841DB("-------[HI841SetVideoMode]ZSD???---------\n");
		if(HI841.HI841AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==15)
			    frameRate= 148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (HI841.capPclk*10000) /(HI841_FULL_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (HI841.capPclk*10000) /(HI841_FULL_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=HI841_FULL_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = HI841_FULL_PERIOD_LINE_NUMS;
			HI841DB("[HI841SetVideoMode]current fps = %d\n", (HI841.capPclk*10000) /(HI841_FULL_PERIOD_PIXEL_NUMS)/HI841_FULL_PERIOD_LINE_NUMS);

		}
		HI841DB("[HI841SetVideoMode]current fps (10 base)= %d\n", (HI841.capPclk*10000)*10/(HI841_FULL_PERIOD_PIXEL_NUMS + HI841.DummyPixels)/MIN_Frame_length);

		if(HI841.shutter + 4 > MIN_Frame_length)
				MIN_Frame_length = HI841.shutter + 4;


		extralines = MIN_Frame_length - HI841_FULL_PERIOD_LINE_NUMS;

		spin_lock(&HI841mipiraw_drv_lock);
		HI841.DummyPixels = 0;//define dummy pixels and lines
		HI841.DummyLines = extralines ;
		spin_unlock(&HI841mipiraw_drv_lock);

		HI841_SetDummy(HI841.DummyPixels,extralines);
	}
	HI841DB("[HI841SetVideoMode]MIN_Frame_length=%d,HI841.DummyLines=%d\n",MIN_Frame_length,HI841.DummyLines);

    return KAL_TRUE;
}

UINT32 HI841SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	//return ERROR_NONE;

    //HI841DB("[HI841SetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
	if(bEnable) {   // enable auto flicker
		spin_lock(&HI841mipiraw_drv_lock);
		HI841.HI841AutoFlickerMode = KAL_TRUE;
		spin_unlock(&HI841mipiraw_drv_lock);
    } else {
    	spin_lock(&HI841mipiraw_drv_lock);
        HI841.HI841AutoFlickerMode = KAL_FALSE;
		spin_unlock(&HI841mipiraw_drv_lock);
        HI841DB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 HI841SetTestPatternMode(kal_bool bEnable)
{
    HI841DB("[HI841SetTestPatternMode] Test pattern enable:%d\n", bEnable);
#if 1
    if(bEnable) 
    {
        HI841_write_cmos_sensor(0x0600,0x00);
        HI841_write_cmos_sensor(0x0601,0x02);        
    }
    else
    {
        HI841_write_cmos_sensor(0x0600,0x00);
        HI841_write_cmos_sensor(0x0601,0x00);  

    }
#endif
    return ERROR_NONE;
}

UINT32 HI841MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	HI841DB("HI841MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk =  153600000;
			lineLength = HI841_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - HI841_PV_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&HI841mipiraw_drv_lock);
			HI841.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&HI841mipiraw_drv_lock);
			HI841_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk =  153600000;
			lineLength = HI841_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - HI841_VIDEO_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&HI841mipiraw_drv_lock);
			HI841.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&HI841mipiraw_drv_lock);
			HI841_SetDummy(0, dummyLine);			
			break;			
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = 304000000;
			lineLength = HI841_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - HI841_FULL_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&HI841mipiraw_drv_lock);
			HI841.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&HI841mipiraw_drv_lock);
			HI841_SetDummy(0, dummyLine);			
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			break;		
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 HI841MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 234;
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			 *pframeRate = 300;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 HI841FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= HI841_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= HI841_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
				*pFeatureReturnPara16++= HI841_FeatureControl_PERIOD_PixelNum;
				*pFeatureReturnPara16= HI841_FeatureControl_PERIOD_LineNum;
				*pFeatureParaLen=4;
				break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(HI841CurrentScenarioId)
			{
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 =  153600000;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 =  153600000;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = 304000000;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 =  153600000;
					*pFeatureParaLen=4;
					break;
			}
		    break;

        case SENSOR_FEATURE_SET_ESHUTTER:
            HI841_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            HI841_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            HI841_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //HI841_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            HI841_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = HI841_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&HI841mipiraw_drv_lock);
                HI841SensorCCT[i].Addr=*pFeatureData32++;
                HI841SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&HI841mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=HI841SensorCCT[i].Addr;
                *pFeatureData32++=HI841SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&HI841mipiraw_drv_lock);
                HI841SensorReg[i].Addr=*pFeatureData32++;
                HI841SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&HI841mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=HI841SensorReg[i].Addr;
                *pFeatureData32++=HI841SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=HI841MIPI_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, HI841SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, HI841SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &HI841SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            HI841_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            HI841_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=HI841_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            HI841_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            HI841_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            HI841_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_Gb;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;

        case SENSOR_FEATURE_INITIALIZE_AF:
            break;
        case SENSOR_FEATURE_CONSTANT_AF:
            break;
        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            HI841SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            HI841GetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            HI841SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            HI841SetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			HI841MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			HI841MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
            break;       
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing             
            *pFeatureReturnPara32=HI841_TEST_PATTERN_CHECKSUM;           
            *pFeatureParaLen=4;                             
        break;     
        default:
            break;
    }
    return ERROR_NONE;
}	/* HI841FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncHI841=
{
    HI841Open,
    HI841GetInfo,
    HI841GetResolution,
    HI841FeatureControl,
    HI841Control,
    HI841Close
};

UINT32 HI841_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncHI841;

    return ERROR_NONE;
}   /* SensorInit() */

