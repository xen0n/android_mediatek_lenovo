/*******************************************************************************************/


/*******************************************************************************************/
     
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/kernel.h>//for printk


#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "t4k37mipiraw_Sensor.h"
#include "t4k37mipiraw_Camera_Sensor_para.h"
#include "t4k37mipiraw_CameraCustomized.h"
static DEFINE_SPINLOCK(t4k37mipiraw_drv_lock);

#define T4K37_DEBUG

#ifdef T4K37_DEBUG
	#define T4K37DB(fmt, arg...) printk( "[T4K37Raw] "  fmt, ##arg)
#else
	#define T4K37DB(x,...)
#endif


#define mDELAY(ms)  mdelay(ms)

MSDK_SENSOR_CONFIG_STRUCT T4K37SensorConfigData;

kal_uint32 T4K37_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM T4K37CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT T4K37SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT T4K37SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

#define T4K37_TEST_PATTERN_CHECKSUM 0xEE63562E


static T4K37_PARA_STRUCT t4k37;


extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define T4K37_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, T4K37MIPI_WRITE_ID)

kal_uint16 T4K37_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,T4K37MIPI_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)

void T4K37_write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = T4K37_PV_PERIOD_PIXEL_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;

	if(MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG == T4K37CurrentScenarioId)
	{
		T4K37_write_cmos_sensor(0x0104,0x0001);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 
	}

	if(t4k37.T4K37AutoFlickerMode == KAL_TRUE)
	{
		if ( SENSOR_MODE_PREVIEW == t4k37.sensorMode )
		{
			line_length = T4K37_PV_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;
			max_shutter = T4K37_PV_PERIOD_LINE_NUMS + t4k37.DummyLines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k37.sensorMode)
		{
			line_length = T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;
			max_shutter = T4K37_VIDEO_PERIOD_LINE_NUMS + t4k37.DummyLines ; 
		}
		else	
		{
			line_length = T4K37_FULL_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;
			max_shutter = T4K37_FULL_PERIOD_LINE_NUMS + t4k37.DummyLines ; 
		}

		switch(T4K37CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				min_framelength = (t4k37.capPclk*10000) /(T4K37_FULL_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/296*10 ;
				break;

			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				min_framelength = (t4k37.videoPclk*10000) /(T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/296*10 ;
				break;
			default:
				min_framelength = (t4k37.pvPclk*10000) /(T4K37_PV_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/296*10 ; 
    			break;
		}

		if (shutter < 1)
			shutter = 1;

		if (shutter > (max_shutter -6))
			extra_lines = shutter - (max_shutter -6);
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == t4k37.sensorMode )
		{
			frame_length = T4K37_PV_PERIOD_LINE_NUMS+ t4k37.DummyLines + extra_lines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k37.sensorMode)
		{
			frame_length = T4K37_VIDEO_PERIOD_LINE_NUMS+ t4k37.DummyLines + extra_lines ; 
		}
		else			
		{ 
			frame_length = T4K37_FULL_PERIOD_LINE_NUMS + t4k37.DummyLines + extra_lines ; 
		}
		
		if (frame_length < min_framelength)
		{
			switch(T4K37CurrentScenarioId)
			{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				extra_lines = min_framelength- (T4K37_FULL_PERIOD_LINE_NUMS+ t4k37.DummyLines);
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				extra_lines = min_framelength- (T4K37_VIDEO_PERIOD_LINE_NUMS+ t4k37.DummyLines);
			default:
				extra_lines = min_framelength- (T4K37_PV_PERIOD_LINE_NUMS+ t4k37.DummyLines); 
    			break;
			}
			
			frame_length = min_framelength;//redefine frame_length
		}	
		
		//Set total frame length
		T4K37_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		T4K37_write_cmos_sensor(0x0341, frame_length & 0xFF);
		spin_lock_irqsave(&t4k37mipiraw_drv_lock,flags);
		t4k37.maxExposureLines = frame_length;
		spin_unlock_irqrestore(&t4k37mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		T4K37_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		T4K37_write_cmos_sensor(0x0203,  shutter& 0xFF);
		
		//T4K37DB("framerate(10 base) = %d\n",(t4k37.pvPclk*10000)*10 /line_length/frame_length);
		
		T4K37DB("AutoFlickerMode_ON:shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	
	}
	else
	{
		if ( SENSOR_MODE_PREVIEW == t4k37.sensorMode )  
		{
			max_shutter = T4K37_PV_PERIOD_LINE_NUMS + t4k37.DummyLines; 
		}
		else if(SENSOR_MODE_VIDEO == t4k37.sensorMode)
		{
			max_shutter = T4K37_VIDEO_PERIOD_LINE_NUMS + t4k37.DummyLines; 
		}
		else	
		{
			max_shutter = T4K37_FULL_PERIOD_LINE_NUMS + t4k37.DummyLines; 
		}
		
		if (shutter < 1)
			shutter = 1;

		if (shutter > (max_shutter -6))
			extra_lines = shutter - (max_shutter -6);
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == t4k37.sensorMode )
		{
			line_length = T4K37_PV_PERIOD_PIXEL_NUMS + t4k37.DummyPixels; 
			frame_length = T4K37_PV_PERIOD_LINE_NUMS+ t4k37.DummyLines + extra_lines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k37.sensorMode)
		{
			line_length = T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels; 
			frame_length = T4K37_VIDEO_PERIOD_LINE_NUMS+ t4k37.DummyLines + extra_lines ; 
		}
		else				//QSXGA size output
		{
			line_length = T4K37_FULL_PERIOD_PIXEL_NUMS + t4k37.DummyPixels; 
			frame_length = T4K37_FULL_PERIOD_LINE_NUMS + t4k37.DummyLines + extra_lines ; 
		}


		//Set total frame length
		T4K37_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		T4K37_write_cmos_sensor(0x0341, frame_length & 0xFF);
		spin_lock_irqsave(&t4k37mipiraw_drv_lock,flags);
		t4k37.maxExposureLines = frame_length;
		spin_unlock_irqrestore(&t4k37mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		T4K37_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		T4K37_write_cmos_sensor(0x0203,  shutter& 0xFF);
		
		T4K37DB("AutoFlickerMode_OFF:shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}

	if(MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG == T4K37CurrentScenarioId)
	{
		T4K37_write_cmos_sensor(0x0104,0x0000);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 
	}
	
}   /* write_T4K37_shutter */

/*******************************************************************************
* 
********************************************************************************/
static kal_uint16 T4K37Reg2Gain(const kal_uint16 iReg)
{
	kal_uint16 sensorGain=0x0000;	

    sensorGain = iReg/T4K37_ANALOG_GAIN_1X;  //get sensor gain multiple
    return sensorGain*BASEGAIN; 
}

/*******************************************************************************
* 
********************************************************************************/
static kal_uint16 T4K37Gain2Reg(const kal_uint16 Gain)
{
    kal_uint32 iReg = 0x0000;
	kal_uint16 sensorGain=0x0000;	

	iReg= Gain*T4K37_ANALOG_GAIN_1X/BASEGAIN;
	
	T4K37DB("T4K37Gain2Reg iReg =%d",iReg);
    return (kal_uint16)iReg;
}


void write_T4K37_gain(kal_uint16 gain)
{

//	T4K37_write_cmos_sensor(0x0104,0x0001);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 

	T4K37_write_cmos_sensor(0x0204,(gain>>8));
	T4K37_write_cmos_sensor(0x0205,(gain&0xff));


//	T4K37_write_cmos_sensor(0x0104,0x0000);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 

	return;
}

/*************************************************************************
* FUNCTION
*    T4K37_SetGain
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
void T4K37_SetGain(UINT16 iGain)
{
	UINT16 gain_reg=0;
	unsigned long flags;

	
	T4K37DB("4K04:featurecontrol--iGain=%d\n",iGain);


	spin_lock_irqsave(&t4k37mipiraw_drv_lock,flags);
	t4k37.realGain = iGain;//64 Base
	t4k37.sensorGlobalGain = T4K37Gain2Reg(iGain);
	spin_unlock_irqrestore(&t4k37mipiraw_drv_lock,flags);
	
	write_T4K37_gain(t4k37.sensorGlobalGain);	
	T4K37DB("[T4K37_SetGain]t4k37.sensorGlobalGain=0x%x,t4k37.realGain=%d\n",t4k37.sensorGlobalGain,t4k37.realGain);
	
}   /*  T4K37_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_T4K37_gain
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
kal_uint16 read_T4K37_gain(void)
{
    kal_uint8  temp_reg;
	kal_uint16 sensor_gain =0, read_gain=0;

	read_gain=((T4K37_read_cmos_sensor(0x0204) << 8) | T4K37_read_cmos_sensor(0x0205));
	
	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.sensorGlobalGain = read_gain; 
	t4k37.realGain = T4K37Reg2Gain(t4k37.sensorGlobalGain);
	spin_unlock(&t4k37mipiraw_drv_lock);
	
	T4K37DB("t4k37.sensorGlobalGain=0x%x,t4k37.realGain=%d\n",t4k37.sensorGlobalGain,t4k37.realGain);
	
	return t4k37.sensorGlobalGain;
} 


void T4K37_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=T4K37SensorReg[i].Addr; i++)
    {
        T4K37_write_cmos_sensor(T4K37SensorReg[i].Addr, T4K37SensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=T4K37SensorReg[i].Addr; i++)
    {
        T4K37_write_cmos_sensor(T4K37SensorReg[i].Addr, T4K37SensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        T4K37_write_cmos_sensor(T4K37SensorCCT[i].Addr, T4K37SensorCCT[i].Para);
    }
}


/*************************************************************************
* FUNCTION
*    T4K37_sensor_to_camera_para
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
void T4K37_sensor_to_camera_para(void)
{
    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=T4K37SensorReg[i].Addr; i++)
    {
         temp_data = T4K37_read_cmos_sensor(T4K37SensorReg[i].Addr);
		 spin_lock(&t4k37mipiraw_drv_lock);
		 T4K37SensorReg[i].Para =temp_data;
		 spin_unlock(&t4k37mipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=T4K37SensorReg[i].Addr; i++)
    {
        temp_data = T4K37_read_cmos_sensor(T4K37SensorReg[i].Addr);
		spin_lock(&t4k37mipiraw_drv_lock);
		T4K37SensorReg[i].Para = temp_data;
		spin_unlock(&t4k37mipiraw_drv_lock);
    }
}

/*************************************************************************
* FUNCTION
*    T4K37_get_sensor_group_count
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
kal_int32  T4K37_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void T4K37_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
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

void T4K37_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
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

            temp_para= T4K37SensorCCT[temp_addr].Para;
			//temp_gain= (temp_para/t4k37.sensorBaseGain) * 1000;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= T4K37_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= T4K37_MAX_ANALOG_GAIN * 1000;
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



kal_bool T4K37_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
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
//             temp_para=(temp_gain * t4k37.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
          }          
          else
			  ASSERT(0);

		  spin_lock(&t4k37mipiraw_drv_lock);
          T4K37SensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&t4k37mipiraw_drv_lock);
          T4K37_write_cmos_sensor(T4K37SensorCCT[temp_addr].Addr,temp_para);

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
					spin_lock(&t4k37mipiraw_drv_lock);
                    T4K37_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&t4k37mipiraw_drv_lock);
                    break;
                case 1:
                    T4K37_write_cmos_sensor(T4K37_FAC_SENSOR_REG,ItemValue);
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

static void T4K37_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )//checkd
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

//	T4K37_write_cmos_sensor(0x0104,0x0001);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 

	if ( SENSOR_MODE_PREVIEW == t4k37.sensorMode )
	{
		line_length = T4K37_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K37_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if(SENSOR_MODE_VIDEO == t4k37.sensorMode)
	{
		line_length = T4K37_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K37_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else				
	{
		line_length = T4K37_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K37_FULL_PERIOD_LINE_NUMS + iLines;
	}
	
	
	//Set total frame length
	T4K37_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
	T4K37_write_cmos_sensor(0x0341, frame_length & 0xFF);
	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.maxExposureLines = frame_length;
	t4k37.DummyPixels =iPixels;
	t4k37.DummyLines =iLines;
	spin_unlock(&t4k37mipiraw_drv_lock);

	//Set total line length
	T4K37_write_cmos_sensor(0x0342, (line_length >> 8) & 0xFF);
	T4K37_write_cmos_sensor(0x0343, line_length & 0xFF);

//	T4K37_write_cmos_sensor(0x0104,0x0000);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 
	
}

static void T4K37_Sensor_Init(void)
{
	T4K37DB("T4K37_Sensor_Init enter_4lane :\n ");	

	T4K37_write_cmos_sensor(0x0101, 0x0003);  //  -/-/-/-/-/-/IMAGE_ORIENT[1:0];
	T4K37_write_cmos_sensor(0x0103, 0x0000);  //  -/-/-/-/-/-/MIPI_RST/SOFTWARE_RESET;
	T4K37_write_cmos_sensor(0x0104, 0x0000);  //  -/-/-/-/-/-/-/GROUP_PARA_HOLD;
	T4K37_write_cmos_sensor(0x0105, 0x0001);  //  -/-/-/-/-/-/-/MSK_CORRUPT_FR;   
	T4K37_write_cmos_sensor(0x0106, 0x0000);  //  -/-/-/-/-/-/-/FAST_STANDBY_CTRL;
	T4K37_write_cmos_sensor(0x0110, 0x0000);  //  -/-/-/-/-/CSI_CHAN_IDNTF[2:0];
	T4K37_write_cmos_sensor(0x0111, 0x0002);  //  -/-/-/-/-/-/CSI_SIGNAL_MOD[1:0];
	T4K37_write_cmos_sensor(0x0112, 0x000a);  //  CSI_DATA_FORMAT[15:8];
	T4K37_write_cmos_sensor(0x0113, 0x000a);  //  CSI_DATA_FORMAT[7:0];
	T4K37_write_cmos_sensor(0x0114, 0x0003);  //  -/-/-/-/-/-/CSI_LANE_MODE[1:0];
	T4K37_write_cmos_sensor(0x0115, 0x0030);  //  -/-/CSI_10TO8_DT[5:0];
	T4K37_write_cmos_sensor(0x0117, 0x0032);  //  -/-/CSI_10TO6_DT[5:0];
	T4K37_write_cmos_sensor(0x0130, 0x0018);  //  EXTCLK_FRQ_MHZ[15:8];
	T4K37_write_cmos_sensor(0x0131, 0x0000);  //  EXTCLK_FRQ_MHZ[7:0];
	T4K37_write_cmos_sensor(0x0141, 0x0000);  //  -/-/-/-/-/CTX_SW_CTL[2:0];
	T4K37_write_cmos_sensor(0x0142, 0x0000);  //  -/-/-/-/CONT_MDSEL_FRVAL[1:0]/CONT_FRCNT_MSK/CONT_GRHOLD_MSK;
	T4K37_write_cmos_sensor(0x0143, 0x0000);  //  R_FRAME_COUNT[7:0];
	T4K37_write_cmos_sensor(0x0202, 0x0003);  //  COAR_INTEGR_TIM[15:8];
	T4K37_write_cmos_sensor(0x0203, 0x00a6);  //  COAR_INTEGR_TIM[7:0];
	T4K37_write_cmos_sensor(0x0204, 0x0000);  //  -/-/-/-/ANA_GA_CODE_GL[11:8];
	T4K37_write_cmos_sensor(0x0205, 0x003d);  //  ANA_GA_CODE_GL[7:0];
	T4K37_write_cmos_sensor(0x0210, 0x0001);  //  -/-/-/-/-/-/DG_GA_GREENR[9:8];
	T4K37_write_cmos_sensor(0x0211, 0x0000);  //  DG_GA_GREENR[7:0];
	T4K37_write_cmos_sensor(0x0212, 0x0001);  //  -/-/-/-/-/-/DG_GA_RED[9:8];
	T4K37_write_cmos_sensor(0x0213, 0x0000);  //  DG_GA_RED[7:0];
	T4K37_write_cmos_sensor(0x0214, 0x0001);  //  -/-/-/-/-/-/DG_GA_BLUE[9:8];
	T4K37_write_cmos_sensor(0x0215, 0x0000);  //  DG_GA_BLUE[7:0];
	T4K37_write_cmos_sensor(0x0216, 0x0001);  //  -/-/-/-/-/-/DG_GA_GREENB[9:8];
	T4K37_write_cmos_sensor(0x0217, 0x0000);  //  DG_GA_GREENB[7:0];
	T4K37_write_cmos_sensor(0x0230, 0x0000);  //  -/-/-/HDR_MODE[4:0];
	T4K37_write_cmos_sensor(0x0232, 0x0004);  //  HDR_RATIO_1[7:0];
	T4K37_write_cmos_sensor(0x0234, 0x0000);  //  HDR_SHT_INTEGR_TIM[15:8];
	T4K37_write_cmos_sensor(0x0235, 0x0019);  //  HDR_SHT_INTEGR_TIM[7:0];
	T4K37_write_cmos_sensor(0x0301, 0x0003);  //  -/-/-/-/VT_PIX_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0303, 0x0004);  //  -/-/-/-/VT_SYS_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0305, 0x0007);  //  -/-/-/-/-/PRE_PLL_CLK_DIV[2:0];
	T4K37_write_cmos_sensor(0x0306, 0x0001);  //  -/-/-/-/-/-/-/PLL_MULTIPLIER[8];
	T4K37_write_cmos_sensor(0x0307, 0x0024);  //  PLL_MULTIPLIER[7:0];
	T4K37_write_cmos_sensor(0x030B, 0x0001);  //  -/-/-/-/OP_SYS_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x030D, 0x0003);  //  -/-/-/-/-/PRE_PLL_ST_CLK_DIV[2:0];
	T4K37_write_cmos_sensor(0x030E, 0x0000);  //  -/-/-/-/-/-/-/PLL_MULT_ST[8];
	T4K37_write_cmos_sensor(0x030F, 0x006c);  //  PLL_MULT_ST[7:0];
	T4K37_write_cmos_sensor(0x0310, 0x0000);  //  -/-/-/-/-/-/-/OPCK_PLLSEL;
	T4K37_write_cmos_sensor(0x0340, 0x000c);  //  FR_LENGTH_LINES[15:8];
	T4K37_write_cmos_sensor(0x0341, 0x0058);  //  FR_LENGTH_LINES[7:0];
	T4K37_write_cmos_sensor(0x0342, 0x0015);  //  LINE_LENGTH_PCK[15:8];
	T4K37_write_cmos_sensor(0x0343, 0x0000);  //  LINE_LENGTH_PCK[7:0];
	T4K37_write_cmos_sensor(0x0344, 0x0000);  //  -/-/-/-/H_CROP[3:0];
	T4K37_write_cmos_sensor(0x0346, 0x0000);  //  Y_ADDR_START[15:8];
	T4K37_write_cmos_sensor(0x0347, 0x0000);  //  Y_ADDR_START[7:0];
	T4K37_write_cmos_sensor(0x034A, 0x000c);  //  Y_ADDR_END[15:8];
	T4K37_write_cmos_sensor(0x034B, 0x002f);  //  Y_ADDR_END[7:0];
	T4K37_write_cmos_sensor(0x034C, 0x0010);  //  X_OUTPUT_SIZE[15:8];
	T4K37_write_cmos_sensor(0x034D, 0x0070);  //  X_OUTPUT_SIZE[7:0];
	T4K37_write_cmos_sensor(0x034E, 0x000c);  //  Y_OUTPUT_SIZE[15:8];
	T4K37_write_cmos_sensor(0x034F, 0x0030);  //  Y_OUTPUT_SIZE[7:0];
	T4K37_write_cmos_sensor(0x0401, 0x0000);  //  -/-/-/-/-/-/SCALING_MODE[1:0];
	T4K37_write_cmos_sensor(0x0403, 0x0000);  //  -/-/-/-/-/-/SPATIAL_SAMPLING[1:0];
	T4K37_write_cmos_sensor(0x0404, 0x0010);  //  SCALE_M[7:0];
	T4K37_write_cmos_sensor(0x0408, 0x0000);  //  DCROP_XOFS[15:8];
	T4K37_write_cmos_sensor(0x0409, 0x0000);  //  DCROP_XOFS[7:0];
	T4K37_write_cmos_sensor(0x040A, 0x0000);  //  DCROP_YOFS[15:8];
	T4K37_write_cmos_sensor(0x040B, 0x0000);  //  DCROP_YOFS[7:0];
	T4K37_write_cmos_sensor(0x040C, 0x0010);  //  DCROP_WIDTH[15:8];
	T4K37_write_cmos_sensor(0x040D, 0x0070);  //  DCROP_WIDTH[7:0];
	T4K37_write_cmos_sensor(0x040E, 0x000c);  //  DCROP_HIGT[15:8];
	T4K37_write_cmos_sensor(0x040F, 0x0030);  //  DCROP_HIGT[7:0];
	T4K37_write_cmos_sensor(0x0601, 0x0000);  //  TEST_PATT_MODE[7:0];
	T4K37_write_cmos_sensor(0x0602, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_RED[9:8];
	T4K37_write_cmos_sensor(0x0603, 0x00c0);  //  TEST_DATA_RED[7:0];
	T4K37_write_cmos_sensor(0x0604, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_GREENR[9:8];
	T4K37_write_cmos_sensor(0x0605, 0x00c0);  //  TEST_DATA_GREENR[7:0];
	T4K37_write_cmos_sensor(0x0606, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_BLUE[9:8];
	T4K37_write_cmos_sensor(0x0607, 0x00c0);  //  TEST_DATA_BLUE[7:0];
	T4K37_write_cmos_sensor(0x0608, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_GREENB[9:8];
	T4K37_write_cmos_sensor(0x0609, 0x00c0);  //  TEST_DATA_GREENB[7:0];
	T4K37_write_cmos_sensor(0x060A, 0x0000);  //  HO_CURS_WIDTH[15:8];
	T4K37_write_cmos_sensor(0x060B, 0x0000);  //  HO_CURS_WIDTH[7:0];
	T4K37_write_cmos_sensor(0x060C, 0x0000);  //  HO_CURS_POSITION[15:8];
	T4K37_write_cmos_sensor(0x060D, 0x0000);  //  HO_CURS_POSITION[7:0];
	T4K37_write_cmos_sensor(0x060E, 0x0000);  //  VE_CURS_WIDTH[15:8];
	T4K37_write_cmos_sensor(0x060F, 0x0000);  //  VE_CURS_WIDTH[7:0];
	T4K37_write_cmos_sensor(0x0610, 0x0000);  //  VE_CURS_POSITION[15:8];
	T4K37_write_cmos_sensor(0x0611, 0x0000);  //  VE_CURS_POSITION[7:0];
	T4K37_write_cmos_sensor(0x0800, 0x0088);  //  TCLK_POST[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0801, 0x0038);  //  THS_PREPARE[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0802, 0x0078);  //  THS_ZERO[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0803, 0x0048);  //  THS_TRAIL[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0804, 0x0048);  //  TCLK_TRAIL[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0805, 0x0040);  //  TCLK_PREPARE[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0806, 0x0000);  //  TCLK_ZERO[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0807, 0x0048);  //  TLPX[7:3]/-/-/-;
	T4K37_write_cmos_sensor(0x0808, 0x0001);  //  -/-/-/-/-/-/DPHY_CTRL[1:0];
	T4K37_write_cmos_sensor(0x0820, 0x0008);  //  MSB_LBRATE[31:24];
	T4K37_write_cmos_sensor(0x0821, 0x0040);  //  MSB_LBRATE[23:16];
	T4K37_write_cmos_sensor(0x0822, 0x0000);  //  MSB_LBRATE[15:8];
	T4K37_write_cmos_sensor(0x0823, 0x0000);  //  MSB_LBRATE[7:0];
	T4K37_write_cmos_sensor(0x0900, 0x0000);  //  -/-/-/-/-/-/H_BIN[1:0];
	T4K37_write_cmos_sensor(0x0901, 0x0000);  //  -/-/-/-/-/-/V_BIN_MODE[1:0];
	T4K37_write_cmos_sensor(0x0902, 0x0000);  //  -/-/-/-/-/-/BINNING_WEIGHTING[1:0];
	T4K37_write_cmos_sensor(0x0A05, 0x0001);  //  -/-/-/-/-/-/-/MAP_DEF_EN;
	T4K37_write_cmos_sensor(0x0A06, 0x0001);  //  -/-/-/-/-/-/-/SGL_DEF_EN;
	T4K37_write_cmos_sensor(0x0A07, 0x0098);  //  SGL_DEF_W[7:0];
	T4K37_write_cmos_sensor(0x0A0A, 0x0001);  //  -/-/-/-/-/-/-/COMB_CPLT_SGL_DEF_EN;
	T4K37_write_cmos_sensor(0x0A0B, 0x0098);  //  COMB_CPLT_SGL_DEF_W[7:0];
	T4K37_write_cmos_sensor(0x0C00, 0x0000);  //  -/-/-/-/-/-/GLBL_RST_CTRL1[1:0];
	T4K37_write_cmos_sensor(0x0C01, 0x0000);  //  -/-/-/-/-/-/-/GLBL_RST_CTRL2;
	T4K37_write_cmos_sensor(0x0C02, 0x0000);  //  GLBL_RST_CFG_1[7:0];
	T4K37_write_cmos_sensor(0x0C03, 0x0000);  //  -/-/-/-/-/-/-/GLBL_RST_CFG_2;
	T4K37_write_cmos_sensor(0x0C04, 0x0000);  //  TRDY_CTRL[15:8];
	T4K37_write_cmos_sensor(0x0C05, 0x0032);  //  TRDY_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C06, 0x0000);  //  TRDOUT_CTRL[15:8];
	T4K37_write_cmos_sensor(0x0C07, 0x0010);  //  TRDOUT_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C08, 0x0000);  //  TSHT_STB_DLY_CTRL[15:8];
	T4K37_write_cmos_sensor(0x0C09, 0x0049);  //  TSHT_STB_DLY_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C0A, 0x0001);  //  TSHT_STB_WDTH_CTRL[15:8];
	T4K37_write_cmos_sensor(0x0C0B, 0x0068);  //  TSHT_STB_WDTH_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C0C, 0x0000);  //  TFLSH_STB_DLY_CTRL[15:8];
	T4K37_write_cmos_sensor(0x0C0D, 0x0034);  //  TFLSH_STB_DLY_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C0E, 0x0000);  //  TFLSH_STB_WDTH_CTRL[15:8];
	T4K37_write_cmos_sensor(0x0C0F, 0x0040);  //  TFLSH_STB_WDTH_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C12, 0x0001);  //  FLASH_ADJ[7:0];
	T4K37_write_cmos_sensor(0x0C14, 0x0000);  //  FLASH_LINE[15:8];
	T4K37_write_cmos_sensor(0x0C15, 0x0001);  //  FLASH_LINE[7:0];
	T4K37_write_cmos_sensor(0x0C16, 0x0000);  //  FLASH_DELAY[15:8];
	T4K37_write_cmos_sensor(0x0C17, 0x0020);  //  FLASH_DELAY[7:0];
	T4K37_write_cmos_sensor(0x0C18, 0x0000);  //  FLASH_WIDTH[15:8];
	T4K37_write_cmos_sensor(0x0C19, 0x0040);  //  FLASH_WIDTH[7:0];
	T4K37_write_cmos_sensor(0x0C1A, 0x0000);  //  -/-/FLASH_MODE[5:0];
	T4K37_write_cmos_sensor(0x0C1B, 0x0000);  //  -/-/-/-/-/-/-/FLASH_TRG;
	T4K37_write_cmos_sensor(0x0C26, 0x0000);  //  FLASH_WIDTH2_HRCTRL[15:8];
	T4K37_write_cmos_sensor(0x0C27, 0x0004);  //  FLASH_WIDTH2_HRCTRL[7:0];
	T4K37_write_cmos_sensor(0x0C28, 0x0000);  //  FLASH_WIDTH_LRCTRL[15:8];
	T4K37_write_cmos_sensor(0x0C29, 0x0004);  //  FLASH_WIDTH_LRCTRL[7:0];
	T4K37_write_cmos_sensor(0x0C2A, 0x0001);  //  FLASH_COUNT_RCTRL[7:0];
	T4K37_write_cmos_sensor(0x0C2B, 0x0001);  //  FLASH_COUNT_CTRL[7:0];
	T4K37_write_cmos_sensor(0x0C2C, 0x0000);  //  FLASH_WIDTH2_HCTRL[15:8];
	T4K37_write_cmos_sensor(0x0C2D, 0x0004);  //  FLASH_WIDTH2_HCTRL[7:0];
	T4K37_write_cmos_sensor(0x0C2E, 0x0000);  //  FLASH_WIDTHLCTRL[15:8];
	T4K37_write_cmos_sensor(0x0C2F, 0x0004);  //  FLASH_WIDTHLCTRL[7:0];
	T4K37_write_cmos_sensor(0x0F00, 0x0000);  //  -/-/-/-/-/ABF_LUT_CTL[2:0];
	T4K37_write_cmos_sensor(0x0F01, 0x0001);  //  -/-/-/-/-/-/ABF_LUT_MODE[1:0];
	T4K37_write_cmos_sensor(0x0F02, 0x0001);  //  ABF_ES_A[15:8];
	T4K37_write_cmos_sensor(0x0F03, 0x0040);  //  ABF_ES_A[7:0];
	T4K37_write_cmos_sensor(0x0F04, 0x0000);  //  -/-/-/-/ABF_AG_A[11:8];
	T4K37_write_cmos_sensor(0x0F05, 0x0040);  //  ABF_AG_A[7:0];
	T4K37_write_cmos_sensor(0x0F06, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_GR_A[8];
	T4K37_write_cmos_sensor(0x0F07, 0x0000);  //  ABF_DG_GR_A[7:0];
	T4K37_write_cmos_sensor(0x0F08, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_R_A[8];
	T4K37_write_cmos_sensor(0x0F09, 0x0000);  //  ABF_DG_R_A[7:0];
	T4K37_write_cmos_sensor(0x0F0A, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_B_A[8];
	T4K37_write_cmos_sensor(0x0F0B, 0x0000);  //  ABF_DG_B_A[7:0];
	T4K37_write_cmos_sensor(0x0F0C, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_GB_A[8];
	T4K37_write_cmos_sensor(0x0F0D, 0x0000);  //  ABF_DG_GB_A[7:0];
	T4K37_write_cmos_sensor(0x0F0E, 0x0000);  //  -/-/-/-/-/-/-/F_ENTRY_A;
	T4K37_write_cmos_sensor(0x0F0F, 0x0001);  //  ABF_ES_B[15:8];
	T4K37_write_cmos_sensor(0x0F10, 0x0050);  //  ABF_ES_B[7:0];
	T4K37_write_cmos_sensor(0x0F11, 0x0000);  //  -/-/-/-/ABF_AG_B[11:8];
	T4K37_write_cmos_sensor(0x0F12, 0x0050);  //  ABF_AG_B[7:0];
	T4K37_write_cmos_sensor(0x0F13, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_GR_B[8];
	T4K37_write_cmos_sensor(0x0F14, 0x0000);  //  ABF_DG_GR_B[7:0];
	T4K37_write_cmos_sensor(0x0F15, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_R_B[8];
	T4K37_write_cmos_sensor(0x0F16, 0x0000);  //  ABF_DG_R_B[7:0];
	T4K37_write_cmos_sensor(0x0F17, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_B_B[8];
	T4K37_write_cmos_sensor(0x0F18, 0x0000);  //  ABF_DG_B_B[7:0];
	T4K37_write_cmos_sensor(0x0F19, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_GB_B[8];
	T4K37_write_cmos_sensor(0x0F1A, 0x0000);  //  ABF_DG_GB_B[7:0];
	T4K37_write_cmos_sensor(0x0F1B, 0x0000);  //  -/-/-/-/-/-/-/F_ENTRY_B;
	T4K37_write_cmos_sensor(0x0F1C, 0x0001);  //  ABF_ES_C[15:8];
	T4K37_write_cmos_sensor(0x0F1D, 0x0060);  //  ABF_ES_C[7:0];
	T4K37_write_cmos_sensor(0x0F1E, 0x0000);  //  -/-/-/-/ABF_AG_C[11:8];
	T4K37_write_cmos_sensor(0x0F1F, 0x0060);  //  ABF_AG_C[7:0];
	T4K37_write_cmos_sensor(0x0F20, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_GR_C[8];
	T4K37_write_cmos_sensor(0x0F21, 0x0000);  //  ABF_DG_GR_C[7:0];
	T4K37_write_cmos_sensor(0x0F22, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_R_C[8];
	T4K37_write_cmos_sensor(0x0F23, 0x0000);  //  ABF_DG_R_C[7:0];
	T4K37_write_cmos_sensor(0x0F24, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_B_C[8];
	T4K37_write_cmos_sensor(0x0F25, 0x0000);  //  ABF_DG_B_C[7:0];
	T4K37_write_cmos_sensor(0x0F26, 0x0001);  //  -/-/-/-/-/-/-/ABF_DG_GB_C[8];
	T4K37_write_cmos_sensor(0x0F27, 0x0000);  //  ABF_DG_GB_C[7:0];
	T4K37_write_cmos_sensor(0x0F28, 0x0000);  //  -/-/-/-/-/-/-/F_ENTRY_C;
	T4K37_write_cmos_sensor(0x1101, 0x0000);  //  -/-/-/-/-/-/IMAGE_ORIENT_1B[1:0];
	T4K37_write_cmos_sensor(0x1143, 0x0000);  //  R_FRAME_COUNT_1B[7:0];
	T4K37_write_cmos_sensor(0x1202, 0x0000);  //  COAR_INTEGR_TIM_1B[15:8];
	T4K37_write_cmos_sensor(0x1203, 0x0019);  //  COAR_INTEGR_TIM_1B[7:0];
	T4K37_write_cmos_sensor(0x1204, 0x0000);  //  -/-/-/-/ANA_GA_CODE_GL_1B[11:8];
	T4K37_write_cmos_sensor(0x1205, 0x0040);  //  ANA_GA_CODE_GL_1B[7:0];
	T4K37_write_cmos_sensor(0x1210, 0x0001);  //  -/-/-/-/-/-/DG_GA_GREENR_1B[9:8];
	T4K37_write_cmos_sensor(0x1211, 0x0000);  //  DG_GA_GREENR_1B[7:0];
	T4K37_write_cmos_sensor(0x1212, 0x0001);  //  -/-/-/-/-/-/DG_GA_RED_1B[9:8];
	T4K37_write_cmos_sensor(0x1213, 0x0000);  //  DG_GA_RED_1B[7:0];
	T4K37_write_cmos_sensor(0x1214, 0x0001);  //  -/-/-/-/-/-/DG_GA_BLUE_1B[9:8];
	T4K37_write_cmos_sensor(0x1215, 0x0000);  //  DG_GA_BLUE_1B[7:0];
	T4K37_write_cmos_sensor(0x1216, 0x0001);  //  -/-/-/-/-/-/DG_GA_GREENB_1B[9:8];
	T4K37_write_cmos_sensor(0x1217, 0x0000);  //  DG_GA_GREENB_1B[7:0];
	T4K37_write_cmos_sensor(0x1230, 0x0000);  //  -/-/-/HDR_MODE_1B[4:0];
	T4K37_write_cmos_sensor(0x1232, 0x0004);  //  HDR_RATIO_1_1B[7:0];
	T4K37_write_cmos_sensor(0x1234, 0x0000);  //  HDR_SHT_INTEGR_TIM_1B[15:8];
	T4K37_write_cmos_sensor(0x1235, 0x0019);  //  HDR_SHT_INTEGR_TIM_1B[7:0];
	T4K37_write_cmos_sensor(0x1340, 0x000c);  //  FR_LENGTH_LINES_1B[15:8];
	T4K37_write_cmos_sensor(0x1341, 0x0080);  //  FR_LENGTH_LINES_1B[7:0];
	T4K37_write_cmos_sensor(0x1342, 0x0015);  //  LINE_LENGTH_PCK_1B[15:8];
	T4K37_write_cmos_sensor(0x1343, 0x00e0);  //  LINE_LENGTH_PCK_1B[7:0];
	T4K37_write_cmos_sensor(0x1344, 0x0000);  //  -/-/-/-/H_CROP_1B[3:0];
	T4K37_write_cmos_sensor(0x1346, 0x0000);  //  Y_ADDR_START_1B[15:8];
	T4K37_write_cmos_sensor(0x1347, 0x0000);  //  Y_ADDR_START_1B[7:0];
	T4K37_write_cmos_sensor(0x134A, 0x000c);  //  Y_ADDR_END_1B[15:8];
	T4K37_write_cmos_sensor(0x134B, 0x002f);  //  Y_ADDR_END_1B[7:0];
	T4K37_write_cmos_sensor(0x134C, 0x0010);  //  X_OUTPUT_SIZE_1B[15:8];
	T4K37_write_cmos_sensor(0x134D, 0x0070);  //  X_OUTPUT_SIZE_1B[7:0];
	T4K37_write_cmos_sensor(0x134E, 0x000c);  //  Y_OUTPUT_SIZE_1B[15:8];
	T4K37_write_cmos_sensor(0x134F, 0x0030);  //  Y_OUTPUT_SIZE_1B[7:0];
	T4K37_write_cmos_sensor(0x1401, 0x0000);  //  -/-/-/-/-/-/SCALING_MODE_1B[1:0];
	T4K37_write_cmos_sensor(0x1403, 0x0000);  //  -/-/-/-/-/-/SPATIAL_SAMPLING_1B[1:0];
	T4K37_write_cmos_sensor(0x1404, 0x0010);  //  SCALE_M_1B[7:0];
	T4K37_write_cmos_sensor(0x1408, 0x0000);  //  DCROP_XOFS_1B[15:8];
	T4K37_write_cmos_sensor(0x1409, 0x0000);  //  DCROP_XOFS_1B[7:0];
	T4K37_write_cmos_sensor(0x140A, 0x0000);  //  DCROP_YOFS_1B[15:8];
	T4K37_write_cmos_sensor(0x140B, 0x0000);  //  DCROP_YOFS_1B[7:0];
	T4K37_write_cmos_sensor(0x140C, 0x0010);  //  DCROP_WIDTH_1B[15:8];
	T4K37_write_cmos_sensor(0x140D, 0x0070);  //  DCROP_WIDTH_1B[7:0];
	T4K37_write_cmos_sensor(0x140E, 0x000c);  //  DCROP_HIGT_1B[15:8];
	T4K37_write_cmos_sensor(0x140F, 0x0030);  //  DCROP_HIGT_1B[7:0];
	T4K37_write_cmos_sensor(0x1601, 0x0000);  //  TEST_PATT_MODE_1B[7:0];
	T4K37_write_cmos_sensor(0x1602, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_RED_1B[9:8];
	T4K37_write_cmos_sensor(0x1603, 0x00c0);  //  TEST_DATA_RED_1B[7:0];
	T4K37_write_cmos_sensor(0x1604, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_GREENR_1B[9:8];
	T4K37_write_cmos_sensor(0x1605, 0x00c0);  //  TEST_DATA_GREENR_1B[7:0];
	T4K37_write_cmos_sensor(0x1606, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_BLUE_1B[9:8];
	T4K37_write_cmos_sensor(0x1607, 0x00c0);  //  TEST_DATA_BLUE_1B[7:0];
	T4K37_write_cmos_sensor(0x1608, 0x0002);  //  -/-/-/-/-/-/TEST_DATA_GREENB_1B[9:8];
	T4K37_write_cmos_sensor(0x1609, 0x00c0);  //  TEST_DATA_GREENB_1B[7:0];
	T4K37_write_cmos_sensor(0x160A, 0x0000);  //  HO_CURS_WIDTH_1B[15:8];
	T4K37_write_cmos_sensor(0x160B, 0x0000);  //  HO_CURS_WIDTH_1B[7:0];
	T4K37_write_cmos_sensor(0x160C, 0x0000);  //  HO_CURS_POSITION_1B[15:8];
	T4K37_write_cmos_sensor(0x160D, 0x0000);  //  HO_CURS_POSITION_1B[7:0];
	T4K37_write_cmos_sensor(0x160E, 0x0000);  //  VE_CURS_WIDTH_1B[15:8];
	T4K37_write_cmos_sensor(0x160F, 0x0000);  //  VE_CURS_WIDTH_1B[7:0];
	T4K37_write_cmos_sensor(0x1610, 0x0000);  //  VE_CURS_POSITION_1B[15:8];
	T4K37_write_cmos_sensor(0x1611, 0x0000);  //  VE_CURS_POSITION_1B[7:0];
	T4K37_write_cmos_sensor(0x1900, 0x0000);  //  -/-/-/-/-/-/H_BIN_1B[1:0];
	T4K37_write_cmos_sensor(0x1901, 0x0000);  //  -/-/-/-/-/-/V_BIN_MODE_1B[1:0];
	T4K37_write_cmos_sensor(0x1902, 0x0000);  //  -/-/-/-/-/-/BINNING_WEIGHTING_1B[1:0];
	T4K37_write_cmos_sensor(0x3000, 0x0030);  //  BSC_ULMT_AGRNG3[7:0];
	T4K37_write_cmos_sensor(0x3001, 0x0014);  //  BSC_ULMT_AGRNG2[7:0];
	T4K37_write_cmos_sensor(0x3002, 0x000E);  //  BSC_ULMT_AGRNG1[7:0];
	T4K37_write_cmos_sensor(0x3007, 0x0008);  //  BSC_OFF/-/-/-/LIMIT_BSC_ON/-/-/-;
	T4K37_write_cmos_sensor(0x3008, 0x0080);  //  DRESET_HIGH/FTLSNS_HIGH/-/DRESET_LBSC_LOW/FTLSNS_HIGH_RNG4/FTLSNS_HIGH_RNG3/FTLSNS_HIGH_RNG2/FTLSNS_HIGH_RNG1;
	T4K37_write_cmos_sensor(0x3009, 0x000f);  //  -/-/-/-/LIMIT_BSC_RNG4/LIMIT_BSC_RNG3/LIMIT_BSC_RNG2/LIMIT_BSC_RNG1;
	T4K37_write_cmos_sensor(0x300A, 0x0000);  //  -/-/-/-/VSIG_CLIP_BSC_RNG4/VSIG_CLIP_BSC_RNG3/VSIG_CLIP_BSC_RNG2/VSIG_CLIP_BSC_RNG1;
	T4K37_write_cmos_sensor(0x300B, 0x0008);  //  -/-/-/DRESET_LBSC_U[4:0];
	T4K37_write_cmos_sensor(0x300C, 0x0020);  //  DRESET_LBSC_D[7:0];
	T4K37_write_cmos_sensor(0x300D, 0x0000);  //  DRESET_CONJ_U[11:8]/-/-/-/-;
	T4K37_write_cmos_sensor(0x300E, 0x001c);  //  DRESET_CONJ_U[7:0];
	T4K37_write_cmos_sensor(0x300F, 0x0005);  //  -/-/-/-/DRESET_CONJ_D[3:0];
	T4K37_write_cmos_sensor(0x3010, 0x0000);  //  -/-/-/-/-/FTLSNS_LBSC_U[10:8];
	T4K37_write_cmos_sensor(0x3011, 0x0046);  //  FTLSNS_LBSC_U[7:0];
	T4K37_write_cmos_sensor(0x3012, 0x0000);  //  -/-/-/-/FTLSNS_LBSC_D[3:0];
	T4K37_write_cmos_sensor(0x3013, 0x0018);  //  -/FTLSNS_CONJ_W[6:0];
	T4K37_write_cmos_sensor(0x3014, 0x0005);  //  -/-/-/-/FTLSNS_CONJ_D[3:0];
	T4K37_write_cmos_sensor(0x3015, 0x0000);  //  -/-/-/-/-/SADR_LBSC_U[10:8];
	T4K37_write_cmos_sensor(0x3016, 0x004e);  //  SADR_LBSC_U[7:0];
	T4K37_write_cmos_sensor(0x3017, 0x0000);  //  -/-/-/-/SADR_LBSC_D[3:0];
	T4K37_write_cmos_sensor(0x3018, 0x004e);  //  SADR_CONJ_U[7:0];
	T4K37_write_cmos_sensor(0x3019, 0x0000);  //  -/-/-/-/SADR_CONJ_D[3:0];
	T4K37_write_cmos_sensor(0x301A, 0x0044);  //  KBIASCNT_RNG4[3:0]/KBIASCNT_RNG3[3:0];
	T4K37_write_cmos_sensor(0x301B, 0x0044);  //  KBIASCNT_RNG2[3:0]/KBIASCNT_RNG1[3:0];
	T4K37_write_cmos_sensor(0x301C, 0x0088);  //  FIX_VSIGLMTEN[3:0]/VSIGLMTEN_TIM_CHG/-/-/-;
	T4K37_write_cmos_sensor(0x301D, 0x0000);  //  -/-/-/-/-/-/-/VSIGLMTEN_U[8];
	T4K37_write_cmos_sensor(0x301E, 0x0000);  //  VSIGLMTEN_U[7:0];
	T4K37_write_cmos_sensor(0x301F, 0x0000);  //  -/-/-/-/-/-/-/VSIGLMTEN_D[8];
	T4K37_write_cmos_sensor(0x3020, 0x0000);  //  VSIGLMTEN_D[7:0];
	T4K37_write_cmos_sensor(0x3021, 0x000a);  //  -/-/VSIGLMTEN_TIM_CHG_D[5:0];
	T4K37_write_cmos_sensor(0x3023, 0x0008);  //  AGADJ_CALC_MODE/-/-/-/MPS_AGADJ_MODE/-/-/-;
	T4K37_write_cmos_sensor(0x3024, 0x0002);  //  -/-/-/-/AGADJ_FIX_COEF[11:8];
	T4K37_write_cmos_sensor(0x3025, 0x0004);  //  AGADJ_FIX_COEF[7:0];
	T4K37_write_cmos_sensor(0x3027, 0x0000);  //  ADSW1_HIGH/-/ADSW1DMX_LOW/-/ADSW1LK_HIGH/-/ADSW1LKX_LOW/-;
	T4K37_write_cmos_sensor(0x3028, 0x0002);  //  ADSW2_HIGH/-/ADSW2DMX_LOW/-/-/-/FIX_ADENX[1:0];
	T4K37_write_cmos_sensor(0x3029, 0x000c);  //  ADSW1_D[7:0];
	T4K37_write_cmos_sensor(0x302A, 0x0000);  //  ADSW_U[7:0];
	T4K37_write_cmos_sensor(0x302B, 0x0006);  //  -/-/-/ADSW1DMX_U[4:0];
	T4K37_write_cmos_sensor(0x302C, 0x0018);  //  -/ADSW1LK_D[6:0];
	T4K37_write_cmos_sensor(0x302D, 0x0018);  //  -/ADSW1LKX_U[6:0];
	T4K37_write_cmos_sensor(0x302E, 0x000c);  //  -/-/ADSW2_D[5:0];
	T4K37_write_cmos_sensor(0x302F, 0x0006);  //  -/-/-/ADSW2DMX_U[4:0];
	T4K37_write_cmos_sensor(0x3030, 0x0000);  //  ADENX_U[7:0];
	T4K37_write_cmos_sensor(0x3031, 0x0080);  //  ADENX_D[8]/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3032, 0x00fc);  //  ADENX_D[7:0];
	T4K37_write_cmos_sensor(0x3033, 0x0000);  //  CMPI_GCR_CHG[7:0];
	T4K37_write_cmos_sensor(0x3034, 0x0000);  //  CMPI_NML_RET[8]/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3035, 0x0004);  //  CMPI_NML_RET[7:0];
	T4K37_write_cmos_sensor(0x3036, 0x0000);  //  -/CMPAMPCAPEN_RNG4/-/CMPAMPCAPEN_RNG3/-/CMPAMPCAPEN_RNG2/-/CMPAMPCAPEN_RNG1;
	T4K37_write_cmos_sensor(0x3037, 0x000d);  //  -/-/-/CMPI1_NML_RNG4[4:0];
	T4K37_write_cmos_sensor(0x3038, 0x000d);  //  -/-/-/CMPI1_NML_RNG3[4:0];
	T4K37_write_cmos_sensor(0x3039, 0x000d);  //  -/-/-/CMPI1_NML_RNG2[4:0];
	T4K37_write_cmos_sensor(0x303A, 0x000d);  //  -/-/-/CMPI1_NML_RNG1[4:0];
	T4K37_write_cmos_sensor(0x303B, 0x000f);  //  -/-/-/CMPI2_NML_VAL[4:0];
	T4K37_write_cmos_sensor(0x303C, 0x0003);  //  -/-/-/CMPI1_GCR_VAL[4:0];
	T4K37_write_cmos_sensor(0x303D, 0x0003);  //  -/-/-/CMPI2_GCR_VAL[4:0];
	T4K37_write_cmos_sensor(0x303E, 0x0000);  //  ADCKEN_MASK[1:0]/-/-/-/-/-/ADCKEN_LOW;
	T4K37_write_cmos_sensor(0x303F, 0x000b);  //  ADCKEN_NML_RS_U[7:0];
	T4K37_write_cmos_sensor(0x3040, 0x0015);  //  -/-/-/ADCKEN_RS_D[4:0];
	T4K37_write_cmos_sensor(0x3042, 0x000b);  //  ADCKEN_NML_AD_U[7:0];
	T4K37_write_cmos_sensor(0x3043, 0x0015);  //  -/-/-/ADCKEN_AD_D[4:0];
	T4K37_write_cmos_sensor(0x3044, 0x0000);  //  EDCONX_RS_HIGH/EDCONX_AD_HIGH/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3045, 0x0014);  //  EDCONX_RS_D[7:0];
	T4K37_write_cmos_sensor(0x3046, 0x0000);  //  -/-/-/-/-/-/-/EDCONX_AD_D[8];
	T4K37_write_cmos_sensor(0x3047, 0x0028);  //  EDCONX_AD_D[7:0];
	T4K37_write_cmos_sensor(0x3048, 0x000e);  //  -/-/-/ADCKSTT_D[4:0];
	T4K37_write_cmos_sensor(0x3049, 0x0020);  //  -/CNTEDSTP_U[6:0];
	T4K37_write_cmos_sensor(0x304A, 0x0009);  //  -/-/-/-/CNTRSTX_U[3:0];
	T4K37_write_cmos_sensor(0x304B, 0x001f);  //  -/-/CNT0RSTX_RS_D[5:0];
	T4K37_write_cmos_sensor(0x304C, 0x0009);  //  -/-/-/-/CNT0RSTX_AD_U[3:0];
	T4K37_write_cmos_sensor(0x304D, 0x0021);  //  -/-/CNT0RSTX_AD_D[5:0];
	T4K37_write_cmos_sensor(0x304E, 0x0000);  //  CNTRD_HCNT_DRV/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x304F, 0x0029);  //  CNTRD_HCNT_U[7:0];
	T4K37_write_cmos_sensor(0x3050, 0x001d);  //  -/-/-/CNTINVX_START[4:0];
	T4K37_write_cmos_sensor(0x3051, 0x0000);  //  ADTESTCK_ON/COUNTER_TEST/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3052, 0x0000);  //  -/-/-/-/ADTESTCK_INTVL[3:0];
	T4K37_write_cmos_sensor(0x3053, 0x00E0);  //  BSDIGITAL_RNG4/BSDIGITAL_RNG3/BSDIGITAL_RNG2/BSDIGITAL_RNG1/-/-/-/-;
	T4K37_write_cmos_sensor(0x3055, 0x0000);  //  TEST_ST_HREGSW/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3056, 0x0000);  //  DATA_LATCH_DLY/-/-/-/NONHEF_PS_ORDER/-/-/-;
	T4K37_write_cmos_sensor(0x3057, 0x0000);  //  HOBINV/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3059, 0x0001);  //  -/-/-/-/EN_HSFBC_PERIOD_U[3:0];
	T4K37_write_cmos_sensor(0x305A, 0x000a);  //  -/-/-/-/EN_HSFBC_PERIOD_W[3:0];
	T4K37_write_cmos_sensor(0x305C, 0x0001);  //  -/-/-/-/-/FBC_STOP_CND[2:0];
	T4K37_write_cmos_sensor(0x305D, 0x0010);  //  -/FBC_IN_RANGE[6:0];
	T4K37_write_cmos_sensor(0x305E, 0x0006);  //  FBC_AG_CONT_COEF[7:0];
	T4K37_write_cmos_sensor(0x305F, 0x0004);  //  -/-/-/FBC_IN_LINES[4:0];
	T4K37_write_cmos_sensor(0x3061, 0x0001);  //  -/-/-/-/-/-/FBC_SUSP_CND[1:0];
	T4K37_write_cmos_sensor(0x3062, 0x0001);  //  -/-/-/-/-/FBC_SUSP_RANGE[2:0];
	T4K37_write_cmos_sensor(0x3063, 0x0060);  //  FBC_SUSP_AGPARAM[7:0];
	T4K37_write_cmos_sensor(0x3065, 0x0002);  //  -/-/-/-/FBC_START_CND[3:0];
	T4K37_write_cmos_sensor(0x3066, 0x0001);  //  -/-/-/-/-/-/FBC_OUT_RANGE[1:0];
	T4K37_write_cmos_sensor(0x3067, 0x0003);  //  -/-/-/-/-/-/FBC_OUT_CNT_FRMS[1:0];
	T4K37_write_cmos_sensor(0x3068, 0x0024);  //  -/-/FBC_OUT_LINES[5:0];
	T4K37_write_cmos_sensor(0x306A, 0x0080);  //  HS_FBC_ON/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x306B, 0x0008);  //  -/-/HS_FBC_RANGE[5:0];
	T4K37_write_cmos_sensor(0x306E, 0x0010);  //  -/-/-/VSIGLMTBIASSEL/-/-/-/IREFVSEL;
	T4K37_write_cmos_sensor(0x306F, 0x0002);  //  -/-/-/-/-/-/FBC_VREF_RNG_SEL[1:0];
	T4K37_write_cmos_sensor(0x3070, 0x000e);  //  -/-/EN_PS_VREFI_FB[5:0];
	T4K37_write_cmos_sensor(0x3071, 0x0000);  //  PS_VFB_GBL_VAL[9:8]/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3072, 0x0000);  //  PS_VFB_GBL_VAL[7:0];
	T4K37_write_cmos_sensor(0x3073, 0x0026);  //  -/PS_VFB_10B_RNG4[6:0];
	T4K37_write_cmos_sensor(0x3074, 0x001A);  //  -/PS_VFB_10B_RNG3[6:0];
	T4K37_write_cmos_sensor(0x3075, 0x000F);  //  -/PS_VFB_10B_RNG2[6:0];
	T4K37_write_cmos_sensor(0x3076, 0x0003);  //  -/PS_VFB_10B_RNG1[6:0];
	T4K37_write_cmos_sensor(0x3077, 0x0001);  //  -/-/-/-/VFB_STEP_RNG4[3:0];
	T4K37_write_cmos_sensor(0x3078, 0x0001);  //  -/-/-/-/VFB_STEP_RNG3[3:0];
	T4K37_write_cmos_sensor(0x3079, 0x0001);  //  -/-/-/-/VFB_STEP_RNG2[3:0];
	T4K37_write_cmos_sensor(0x307A, 0x0001);  //  -/-/-/-/VFB_STEP_RNG1[3:0];
	T4K37_write_cmos_sensor(0x307B, 0x0004);  //  -/-/-/-/HSVFB_STEP_RNG4[3:0];
	T4K37_write_cmos_sensor(0x307C, 0x0002);  //  -/-/-/-/HSVFB_STEP_RNG3[3:0];
	T4K37_write_cmos_sensor(0x307D, 0x0001);  //  -/-/-/-/HSVFB_STEP_RNG2[3:0];
	T4K37_write_cmos_sensor(0x307E, 0x0002);  //  -/-/-/-/HSVFB_STEP_RNG1[3:0];
	T4K37_write_cmos_sensor(0x3080, 0x0000);  //  EXT_VREFI_FB_ON/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3081, 0x0000);  //  VREFI_FB_FIXVAL[9:8]/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3082, 0x0000);  //  VREFI_FB_FIXVAL[7:0];
	T4K37_write_cmos_sensor(0x3085, 0x0000);  //  EXT_VREFI_ZS_ON/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3086, 0x0000);  //  -/-/-/-/-/-/VREFI_ZS_FIXVAL[9:8];
	T4K37_write_cmos_sensor(0x3087, 0x0000);  //  VREFI_ZS_FIXVAL[7:0];
	T4K37_write_cmos_sensor(0x3089, 0x00a0);  //  ZSV_LEVEL_RNG4[7:0];
	T4K37_write_cmos_sensor(0x308A, 0x00a0);  //  ZSV_LEVEL_RNG3[7:0];
	T4K37_write_cmos_sensor(0x308B, 0x00a0);  //  ZSV_LEVEL_RNG2[7:0];
	T4K37_write_cmos_sensor(0x308C, 0x00a0);  //  ZSV_LEVEL_RNG1[7:0];
	T4K37_write_cmos_sensor(0x308D, 0x0003);  //  -/-/-/-/-/-/ZSV_STOP_CND[1:0];
	T4K37_write_cmos_sensor(0x308E, 0x0020);  //  -/-/ZSV_IN_RANGE[5:0];
	T4K37_write_cmos_sensor(0x3090, 0x0004);  //  -/-/-/-/ZSV_IN_LINES[3:0];
	T4K37_write_cmos_sensor(0x3091, 0x0004);  //  -/-/-/-/ZSV_FORCE_END[3:0];
	T4K37_write_cmos_sensor(0x3092, 0x0001);  //  -/-/-/-/-/-/ZSV_SUSP_CND[1:0];
	T4K37_write_cmos_sensor(0x3093, 0x0001);  //  -/-/-/-/-/-/ZSV_SUSP_RANGE[1:0];
	T4K37_write_cmos_sensor(0x3094, 0x0060);  //  ZSV_SUSP_AGPARAM[7:0];
	T4K37_write_cmos_sensor(0x3095, 0x000e);  //  -/-/-/EN_PS_VREFI_ZS[4:0];
	T4K37_write_cmos_sensor(0x3096, 0x0075);  //  PS_VZS_10B_COEF[7:0];
	T4K37_write_cmos_sensor(0x3097, 0x007e);  //  -/PS_VZS_10B_INTC[6:0];
	T4K37_write_cmos_sensor(0x3098, 0x0020);  //  VZS_STEP_10B[7:0];
	T4K37_write_cmos_sensor(0x3099, 0x0001);  //  -/-/MIN_VZS_STEP[5:0];
	T4K37_write_cmos_sensor(0x309C, 0x0000);  //  EXT_HCNT_MAX_ON/-/-/-/HCNT_MAX_MODE/-/-/-;
	T4K37_write_cmos_sensor(0x309D, 0x0008);  //  HCNT_MAX_FIXVAL[15:8];
	T4K37_write_cmos_sensor(0x309E, 0x002c);  //  HCNT_MAX_FIXVAL[7:0];
	T4K37_write_cmos_sensor(0x30A0, 0x0082);  //  REV_INT_MODE/-/-/-/RI_LOWVOL_MODE/-/RI_FINE_FBC/-;
	T4K37_write_cmos_sensor(0x30A1, 0x0002);  //  -/-/-/-/-/AGADJ_EXEC_MODE[2:0];
	T4K37_write_cmos_sensor(0x30A2, 0x0000);  //  -/-/-/-/-/-/-/AGADJ_REV_INT;
	T4K37_write_cmos_sensor(0x30A3, 0x000e);  //  -/-/-/ZSV_EXEC_MODE[4:0];
	T4K37_write_cmos_sensor(0x30A6, 0x0000);  //  -/-/-/-/-/-/ROUND_VREF_CODE[1:0];
	T4K37_write_cmos_sensor(0x30A7, 0x0020);  //  EXT_VCD_ADJ_ON/-/AUTO_BIT_SFT_VCD/-/AG_SEN_SHIFT/-/-/-;
	T4K37_write_cmos_sensor(0x30A8, 0x0004);  //  -/-/-/-/VCD_COEF_FIXVAL[11:8];
	T4K37_write_cmos_sensor(0x30A9, 0x0000);  //  VCD_COEF_FIXVAL[7:0];
	T4K37_write_cmos_sensor(0x30AA, 0x0000);  //  -/-/VCD_INTC_FIXVAL[5:0];
	T4K37_write_cmos_sensor(0x30AB, 0x0030);  //  -/-/VCD_RNG_TYPE_SEL[1:0]/-/-/VREFIC_MODE[1:0];
	T4K37_write_cmos_sensor(0x30AC, 0x0032);  //  -/-/VREFAD_RNG4_SEL[1:0]/-/-/VREFAD_RNG3_SEL[1:0];
	T4K37_write_cmos_sensor(0x30AD, 0x0010);  //  -/-/VREFAD_RNG2_SEL[1:0]/-/-/VREFAD_RNG1_SEL[1:0];
	T4K37_write_cmos_sensor(0x30AE, 0x0000);  //  FIX_AGADJ_VREFI/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30AF, 0x0000);  //  -/-/-/-/-/-/AGADJ1_VREFI_ZS[9:8];
	T4K37_write_cmos_sensor(0x30B0, 0x003e);  //  AGADJ1_VREFI_ZS[7:0];
	T4K37_write_cmos_sensor(0x30B1, 0x0000);  //  -/-/-/-/-/-/AGADJ2_VREFI_ZS[9:8];
	T4K37_write_cmos_sensor(0x30B2, 0x001f);  //  AGADJ2_VREFI_ZS[7:0];
	T4K37_write_cmos_sensor(0x30B3, 0x0000);  //  -/-/-/-/-/-/-/AGADJ1_VREFI_AD[8];
	T4K37_write_cmos_sensor(0x30B4, 0x003e);  //  AGADJ1_VREFI_AD[7:0];
	T4K37_write_cmos_sensor(0x30B5, 0x0000);  //  -/-/-/-/-/-/-/AGADJ2_VREFI_AD[8];
	T4K37_write_cmos_sensor(0x30B6, 0x001f);  //  AGADJ2_VREFI_AD[7:0];
	T4K37_write_cmos_sensor(0x30B7, 0x0070);  //  -/AGADJ_VREFIC[2:0]/-/AGADJ_VREFC[2:0];
	T4K37_write_cmos_sensor(0x30B9, 0x0080);  //  RI_VREFAD_COEF[7:0];
	T4K37_write_cmos_sensor(0x30BA, 0x0000);  //  EXT_VREFC_ON/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30BB, 0x0000);  //  -/-/-/-/-/VREFC_FIXVAL[2:0];
	T4K37_write_cmos_sensor(0x30BC, 0x0000);  //  EXT_VREFIC_ON/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30BD, 0x0000);  //  -/-/-/-/-/VREFIC_FIXVAL[2:0];
	T4K37_write_cmos_sensor(0x30BE, 0x0080);  //  VREFIC_CHG_SEL/-/VREFICAID_OFF[1:0]/-/FIX_VREFICAID[2:0];
	T4K37_write_cmos_sensor(0x30BF, 0x0000);  //  -/-/-/-/-/-/VREFICAID_W[9:8];
	T4K37_write_cmos_sensor(0x30C0, 0x00e6);  //  VREFICAID_W[7:0];
	T4K37_write_cmos_sensor(0x30C1, 0x0000);  //  EXT_PLLFREQ_ON/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30C2, 0x0000);  //  -/-/-/-/PLLFREQ_FIXVAL[3:0];
	T4K37_write_cmos_sensor(0x30C4, 0x0000);  //  -/-/-/-/-/-/ST_BLACK_LEVEL[9:8];
	T4K37_write_cmos_sensor(0x30C5, 0x0040);  //  ST_BLACK_LEVEL[7:0];
	T4K37_write_cmos_sensor(0x30C6, 0x0000);  //  -/-/ST_CKI[5:0];
	T4K37_write_cmos_sensor(0x30C7, 0x0000);  //  EN_TAIL_DATA/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30C8, 0x0010);  //  -/-/-/EN_REV_CNT_DATA/-/-/-/EXT_BCODEREF_ON;
	T4K37_write_cmos_sensor(0x30C9, 0x0000);  //  -/BCODEREF_FIXVAL[6:0];
	T4K37_write_cmos_sensor(0x30CC, 0x0000);  //  CNTSTTMODE/CNTSTPMODE/-/-/ADCKNUM[1:0]/-/-;
	T4K37_write_cmos_sensor(0x30CD, 0x00f0);  //  POSBSTEN/POSBSTEN2/POSLFIX/NEGLFIX/-/NEGBST2EN/NEGBST2RNG/NEGBST2CKSEL;
	T4K37_write_cmos_sensor(0x30CE, 0x0065);  //  NEGBSTCNT[3:0]/NEGBST2CNT[3:0];
	T4K37_write_cmos_sensor(0x30CF, 0x0075);  //  -/POSBSTCNT[2:0]/-/POSBST2CNT[2:0];
	T4K37_write_cmos_sensor(0x30D0, 0x0022);  //  -/POSBSTHG[2:0]/-/POSBST2HG[2:0];
	T4K37_write_cmos_sensor(0x30D1, 0x0005);  //  -/-/READVDSEL[1:0]/GDMOSBGREN/POSBSTGA[2:0];
	T4K37_write_cmos_sensor(0x30D2, 0x00B3);  //  NEGLEAKCUT/POSBSTEV/POSBSTRNG[1:0]/-/-/POSBST2RNG[1:0];
	T4K37_write_cmos_sensor(0x30D3, 0x0080);  //  KBIASSEL/-/-/BSVBPSEL[4:0];
	T4K37_write_cmos_sensor(0x30D5, 0x0009);  //  -/-/-/VREFV[4:0];
	T4K37_write_cmos_sensor(0x30D6, 0x00cc);  //  ADSW2WEAK/ADSW1WEAK/-/-/VREFAI[3:0];
	T4K37_write_cmos_sensor(0x30D7, 0x0010);  //  ADCKSEL/-/ADCKDIV[1:0]/-/-/-/-;
	T4K37_write_cmos_sensor(0x30D8, 0x0000);  //  -/-/-/-/ANAMON1_SEL[3:0];
	T4K37_write_cmos_sensor(0x30D9, 0x0027);  //  HREG_TEST/TESTCROP/BSDETSEL/HOBSIGIN/VBINVSIG/BINVSIG/BINED/BINCMP;
	T4K37_write_cmos_sensor(0x30DB, 0x0000);  //  -/-/-/-/-/-/VSIGLMTGSEL/GCR_MODE;
	T4K37_write_cmos_sensor(0x30DC, 0x0021);  //  -/-/GDMOS2CNT[1:0]/-/-/-/GDMOSCNTX4;
	T4K37_write_cmos_sensor(0x30DD, 0x0007);  //  -/-/-/-/-/GBLRSTVREFSTPEN/GBLRSTDDSTPEN/GBLRSTGDSTPEN;
	T4K37_write_cmos_sensor(0x30DE, 0x0000);  //  -/-/-/VREFALN/-/-/-/VREFIMBC;
	T4K37_write_cmos_sensor(0x30DF, 0x0010);  //  -/-/VREFTEST[1:0]/-/-/-/VREFIMX4_SEL;
	T4K37_write_cmos_sensor(0x30E0, 0x0013);  //  -/-/-/ADCMP1SRTSEL/-/SINTSELFB[2:0];
	T4K37_write_cmos_sensor(0x30E1, 0x00c0);  //  SINTLSEL2/SINTLSEL/SINTSELPH2/SINTSELPH1/-/-/-/-;
	T4K37_write_cmos_sensor(0x30E2, 0x0000);  //  VREFAMPSHSEL/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30E3, 0x0040);  //  SINTSELOUT2[3:0]/SINTSELOUT1[3:0];
	T4K37_write_cmos_sensor(0x30E4, 0x0000);  //  POSBSTCKDLYOFF/NEGBSTCKDLYOFF/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x30E5, 0x0008);  //  SENSEMODE[7:0];
	T4K37_write_cmos_sensor(0x30E6, 0x0000);  //  SPARE_BL[3:0]/SPARE_BR[3:0];
	T4K37_write_cmos_sensor(0x30E7, 0x0000);  //  SPARE_TL[3:0]/SPARE_TR[3:0];
	T4K37_write_cmos_sensor(0x30E8, 0x0010);  //  -/-/-/BGRDVSTPEN/-/-/-/TSIGEN;
	T4K37_write_cmos_sensor(0x30E9, 0x0000);  //  -/-/-/ST_VOB_VALID_TYPE/-/-/-/BLMD_VALID_ON;
	T4K37_write_cmos_sensor(0x30EA, 0x0000);  //  -/-/-/SADR_HIGH/DCLAMP_LEVEL[1:0]/DCLAMP_LINE_MODE/DCLAMP_ON;
	T4K37_write_cmos_sensor(0x3100, 0x0007);  //  -/-/-/-/FRAC_EXP_TIME_10B[11:8];
	T4K37_write_cmos_sensor(0x3101, 0x00ed);  //  FRAC_EXP_TIME_10B[7:0];
	T4K37_write_cmos_sensor(0x3104, 0x0000);  //  ES_1PULSE/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3105, 0x0000);  //  ESTGRESET_LOW/-/-/-/ESTGRESET_U_SEL/-/-/-;
	T4K37_write_cmos_sensor(0x3106, 0x00a0);  //  AUTO_READ_W/-/AUTO_ESREAD_2D/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3107, 0x0006);  //  -/-/ESTGRESET_U[5:0];
	T4K37_write_cmos_sensor(0x3108, 0x0010);  //  ESTGRESET_D[7:0];
	T4K37_write_cmos_sensor(0x310A, 0x0054);  //  ESREAD_1W[7:0];
	T4K37_write_cmos_sensor(0x310B, 0x0000);  //  -/-/-/-/-/-/-/ESREAD_1D[8];
	T4K37_write_cmos_sensor(0x310C, 0x0046);  //  ESREAD_1D[7:0];
	T4K37_write_cmos_sensor(0x310E, 0x0079);  //  ESREAD_2W[7:0];
	T4K37_write_cmos_sensor(0x310F, 0x0001);  //  -/-/-/ESREAD_2D[12:8];
	T4K37_write_cmos_sensor(0x3110, 0x0004);  //  ESREAD_2D[7:0];
	T4K37_write_cmos_sensor(0x3111, 0x0000);  //  EXTD_ROTGRESET/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3112, 0x0000);  //  -/-/-/-/-/-/ROTGRESET_U[9:8];
	T4K37_write_cmos_sensor(0x3113, 0x000c);  //  ROTGRESET_U[7:0];
	T4K37_write_cmos_sensor(0x3114, 0x002a);  //  -/ROTGRESET_W[6:0];
	T4K37_write_cmos_sensor(0x3116, 0x0000);  //  ROREAD_U[7:0];
	T4K37_write_cmos_sensor(0x3118, 0x0079);  //  ROREAD_W[7:0];
	T4K37_write_cmos_sensor(0x311B, 0x0000);  //  -/-/-/-/-/-/-/ZEROSET_U[8];
	T4K37_write_cmos_sensor(0x311C, 0x000e);  //  ZEROSET_U[7:0];
	T4K37_write_cmos_sensor(0x311E, 0x0030);  //  ZEROSET_W[7:0];
	T4K37_write_cmos_sensor(0x311F, 0x001e);  //  -/-/AZSZEROSET_U[5:0];
	T4K37_write_cmos_sensor(0x3121, 0x0064);  //  AZSZEROSET_W[7:0];
	T4K37_write_cmos_sensor(0x3124, 0x00fc);  //  ESROWVD_U[7:0];
	T4K37_write_cmos_sensor(0x3126, 0x0002);  //  ESROWVD_D[7:0];
	T4K37_write_cmos_sensor(0x3127, 0x0000);  //  -/-/-/-/-/-/-/ROROWVD_U[8];
	T4K37_write_cmos_sensor(0x3128, 0x0008);  //  ROROWVD_U[7:0];
	T4K37_write_cmos_sensor(0x312A, 0x0004);  //  ROROWVD_D[7:0];
	T4K37_write_cmos_sensor(0x312C, 0x0000);  //  PDRSTDRN_HIGH/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x312D, 0x0042);  //  -/PDRSTDRN_U[6:0];
	T4K37_write_cmos_sensor(0x312E, 0x0000);  //  PDRSTDRN_D[8]/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x312F, 0x0000);  //  PDRSTDRN_D[7:0];
	T4K37_write_cmos_sensor(0x3130, 0x0042);  //  -/AZSRSTDRN_U[6:0];
	T4K37_write_cmos_sensor(0x3131, 0x0014);  //  -/AZSRSTDRN_D[6:0];
	T4K37_write_cmos_sensor(0x3132, 0x0008);  //  FIX_RSTNWEAK[1:0]/-/-/FIX_RSTNWEAK_AZS/-/-/-;
	T4K37_write_cmos_sensor(0x3133, 0x0000);  //  -/READ_VDD_MODE[2:0]/ESREAD1_VDD_MDOE/-/-/-;
	T4K37_write_cmos_sensor(0x3134, 0x0001);  //  -/-/-/-/-/-/-/ROROWVD_MODE;
	T4K37_write_cmos_sensor(0x3135, 0x0004);  //  -/-/-/VDDRD28EN_RORE_U[4:0];
	T4K37_write_cmos_sensor(0x3136, 0x0004);  //  -/-/-/VDDRD28EN_RORE_D[4:0];
	T4K37_write_cmos_sensor(0x3137, 0x0004);  //  -/-/-/VDDRD28EN_ROFE_U[4:0];
	T4K37_write_cmos_sensor(0x3138, 0x0004);  //  -/-/-/VDDRD28EN_ROFE_D[4:0];
	T4K37_write_cmos_sensor(0x3139, 0x0004);  //  -/-/-/VDDRD28EN_ES1RE_U[4:0];
	T4K37_write_cmos_sensor(0x313A, 0x0004);  //  -/-/-/VDDRD28EN_ES1RE_D[4:0];
	T4K37_write_cmos_sensor(0x313B, 0x0004);  //  -/-/-/VDDRD28EN_ES1FE_U[4:0];
	T4K37_write_cmos_sensor(0x313C, 0x0004);  //  -/-/-/VDDRD28EN_ES1FE_D[4:0];
	T4K37_write_cmos_sensor(0x313D, 0x0004);  //  -/-/-/VDDRD28EN_ES2RE_U[4:0];
	T4K37_write_cmos_sensor(0x313E, 0x0004);  //  -/-/-/VDDRD28EN_ES2RE_D[4:0];
	T4K37_write_cmos_sensor(0x313F, 0x0004);  //  -/-/-/VDDRD28EN_ES2FE_U[4:0];
	T4K37_write_cmos_sensor(0x3140, 0x0004);  //  -/-/-/VDDRD28EN_ES2FE_D[4:0];
	T4K37_write_cmos_sensor(0x3141, 0x0002);  //  -/-/-/BSTRDCUT_RORE_U[4:0];
	T4K37_write_cmos_sensor(0x3142, 0x0002);  //  -/-/-/BSTRDCUT_RORE_D[4:0];
	T4K37_write_cmos_sensor(0x3143, 0x0002);  //  -/-/-/BSTRDCUT_ROFE_U[4:0];
	T4K37_write_cmos_sensor(0x3144, 0x0002);  //  -/-/-/BSTRDCUT_ROFE_D[4:0];
	T4K37_write_cmos_sensor(0x3145, 0x0002);  //  -/-/-/BSTRDCUT_ES1RE_U[4:0];
	T4K37_write_cmos_sensor(0x3146, 0x0002);  //  -/-/-/BSTRDCUT_ES1RE_D[4:0];
	T4K37_write_cmos_sensor(0x3147, 0x0002);  //  -/-/-/BSTRDCUT_ES1FE_U[4:0];
	T4K37_write_cmos_sensor(0x3148, 0x0002);  //  -/-/-/BSTRDCUT_ES1FE_D[4:0];
	T4K37_write_cmos_sensor(0x3149, 0x0002);  //  -/-/-/BSTRDCUT_ES2RE_U[4:0];
	T4K37_write_cmos_sensor(0x314A, 0x0002);  //  -/-/-/BSTRDCUT_ES2RE_D[4:0];
	T4K37_write_cmos_sensor(0x314B, 0x0002);  //  -/-/-/BSTRDCUT_ES2FE_U[4:0];
	T4K37_write_cmos_sensor(0x314C, 0x0002);  //  -/-/-/BSTRDCUT_ES2FE_D[4:0];
	T4K37_write_cmos_sensor(0x314D, 0x0080);  //  VSIGSRT_LOW/-/VSIGSRT_U[5:0];
	T4K37_write_cmos_sensor(0x314E, 0x0000);  //  -/VSIGSRT_D_RNG4[6:0];
	T4K37_write_cmos_sensor(0x314F, 0x003d);  //  -/VSIGSRT_D_RNG3[6:0];
	T4K37_write_cmos_sensor(0x3150, 0x003b);  //  -/VSIGSRT_D_RNG2[6:0];
	T4K37_write_cmos_sensor(0x3151, 0x0036);  //  -/VSIGSRT_D_RNG1[6:0];
	T4K37_write_cmos_sensor(0x3154, 0x0000);  //  RO_DMY_CR_MODE/-/-/-/DRCUT_SIGIN/-/-/-;
	T4K37_write_cmos_sensor(0x3155, 0x0000);  //  INTERMIT_DRCUT/-/-/DRCUT_TMG_CHG/DRCUT_DMY_OFF/-/-/-;
	T4K37_write_cmos_sensor(0x3156, 0x0000);  //  DRCUT_INTERMIT_U[7:0];
	T4K37_write_cmos_sensor(0x3159, 0x0000);  //  GDMOSCNT_GCR_RET[8]/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x315A, 0x0004);  //  GDMOSCNT_GCR_RET[7:0];
	T4K37_write_cmos_sensor(0x315B, 0x0088);  //  GDMOSCNT_NML_RNG4[3:0]/GDMOSCNT_NML_RNG3[3:0];
	T4K37_write_cmos_sensor(0x315C, 0x0088);  //  GDMOSCNT_NML_RNG2[3:0]/GDMOSCNT_NML_RNG1[3:0];
	T4K37_write_cmos_sensor(0x315D, 0x0002);  //  -/-/-/-/GDMOSCNT_GCR_VAL[3:0];
	T4K37_write_cmos_sensor(0x315F, 0x0000);  //  RSTDRNAMPSTP_LOW/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3160, 0x0010);  //  -/-/RSTDRNAMPSTP_U[5:0];
	T4K37_write_cmos_sensor(0x3162, 0x007c);  //  RSTDRNAMPSTP_D[7:0];
	T4K37_write_cmos_sensor(0x3163, 0x0000);  //  BGRDVSTP_LOW/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3165, 0x0067);  //  RSTVDSEL_NML[1:0]/RSTVDSEL_AZS[1:0]/RSTDRNVDSEL_NML[1:0]/RSTDRNVDSEL_AZS[1:0];
	T4K37_write_cmos_sensor(0x3166, 0x0011);  //  VSIGLMTCNT_RNG4[3:0]/VSIGLMTCNT_RNG3[3:0];
	T4K37_write_cmos_sensor(0x3167, 0x0011);  //  VSIGLMTCNT_RNG2[3:0]/VSIGLMTCNT_RNG1[3:0];
	T4K37_write_cmos_sensor(0x3168, 0x00f1);  //  GDMOS2EN_RNG4/GDMOS2EN_RNG3/GDMOS2EN_RNG2/GDMOS2EN_RNG1/DRADRVLV_AZS[3:0];
	T4K37_write_cmos_sensor(0x3169, 0x0077);  //  DRADRVLV_RNG4[3:0]/DRADRVLV_RNG3[3:0];
	T4K37_write_cmos_sensor(0x316A, 0x0077);  //  DRADRVLV_RNG2[3:0]/DRADRVLV_RNG1[3:0];
	T4K37_write_cmos_sensor(0x316B, 0x0061);  //  -/DRADRVI_AZS[2:0]/-/DRADRVI_NML[2:0];
	T4K37_write_cmos_sensor(0x316C, 0x0001);  //  -/-/-/-/-/-/-/FIX_RSTDRNAMPSTP_GES;
	T4K37_write_cmos_sensor(0x316D, 0x000c);  //  -/-/-/-/VSIGLMTCNT_EZ_VAL[3:0];
	T4K37_write_cmos_sensor(0x316E, 0x0004);  //  -/-/VSIGLMTCNT_EZ_U[5:0];
	T4K37_write_cmos_sensor(0x316F, 0x0004);  //  -/-/VSIGLMTCNT_EZ_D[5:0];
	T4K37_write_cmos_sensor(0x3172, 0x0041);  //  -/FBC_LONG_REF/-/FBC_SUBSMPL/-/-/BIN_MODE[1:0];
	T4K37_write_cmos_sensor(0x3173, 0x0030);  //  -/-/ES_MODE[1:0]/ESREAD_ALT_OFF[3:0];
	T4K37_write_cmos_sensor(0x3174, 0x0000);  //  -/-/-/-/-/-/-/DIS_MODE;
	T4K37_write_cmos_sensor(0x3175, 0x0000);  //  -/-/-/-/-/-/-/ALLZEROSET_ON;
	T4K37_write_cmos_sensor(0x3176, 0x0011);  //  -/-/ALLZEROSET_1ST_ON[1:0]/-/-/-/ALLZEROSET_CHG_ON;
	T4K37_write_cmos_sensor(0x3177, 0x000a);  //  -/-/LTCH_POS[1:0]/DME_ON/RODATA_U/DMR_ON/ALLREAD_ON;
	T4K37_write_cmos_sensor(0x3178, 0x0001);  //  -/-/-/-/ES_LOAD_BAL[3:0];
	T4K37_write_cmos_sensor(0x3180, 0x0002);  //  -/-/-/-/-/-/GBL_HCNT_MAX_SEL[1:0];
	T4K37_write_cmos_sensor(0x3181, 0x001e);  //  -/GBLESTGRESET_U[6:0];
	T4K37_write_cmos_sensor(0x3182, 0x0000);  //  -/-/-/-/-/GBLESTGRESET_W[10:8];
	T4K37_write_cmos_sensor(0x3183, 0x00fa);  //  GBLESTGRESET_W[7:0];
	T4K37_write_cmos_sensor(0x3184, 0x0000);  //  -/-/-/-/-/-/GBLESREAD_1W[9:8];
	T4K37_write_cmos_sensor(0x3185, 0x0088);  //  GBLESREAD_1W[7:0];
	T4K37_write_cmos_sensor(0x3186, 0x0000);  //  -/-/-/-/-/-/GBLESREAD_1D[9:8];
	T4K37_write_cmos_sensor(0x3187, 0x009c);  //  GBLESREAD_1D[7:0];
	T4K37_write_cmos_sensor(0x3188, 0x0000);  //  -/-/-/-/-/-/GBLESREAD_2W[9:8];
	T4K37_write_cmos_sensor(0x3189, 0x00c2);  //  GBLESREAD_2W[7:0];
	T4K37_write_cmos_sensor(0x318A, 0x0010);  //  -/-/-/GBLESREAD_2D[4:0];
	T4K37_write_cmos_sensor(0x318B, 0x0001);  //  -/-/-/-/-/-/FIX_GBLRSTSTP_AZS/FIX_GBLRSTSTP_GES;
	T4K37_write_cmos_sensor(0x318E, 0x0088);  //  GBLVDDRD28EN_ES1RE_U[3:0]/GBLVDDRD28EN_ES1RE_D[3:0];
	T4K37_write_cmos_sensor(0x318F, 0x0088);  //  GBLVDDRD28EN_ES1FE_U[3:0]/GBLVDDRD28EN_ES1FE_D[3:0];
	T4K37_write_cmos_sensor(0x3190, 0x0088);  //  GBLVDDRD28EN_ES2RE_U[3:0]/GBLVDDRD28EN_ES2RE_D[3:0];
	T4K37_write_cmos_sensor(0x3191, 0x0088);  //  GBLVDDRD28EN_ES2FE_U[3:0]/GBLVDDRD28EN_ES2FE_D[3:0];
	T4K37_write_cmos_sensor(0x3192, 0x0044);  //  GBLBSTRDCUT_ES1RE_U[3:0]/GBLBSTRDCUT_ES1RE_D[3:0];
	T4K37_write_cmos_sensor(0x3193, 0x0044);  //  GBLBSTRDCUT_ES1FE_U[3:0]/GBLBSTRDCUT_ES1FE_D[3:0];
	T4K37_write_cmos_sensor(0x3194, 0x0044);  //  GBLBSTRDCUT_ES2RE_U[3:0]/GBLBSTRDCUT_ES2RE_D[3:0];
	T4K37_write_cmos_sensor(0x3195, 0x0044);  //  GBLBSTRDCUT_ES2FE_U[3:0]/GBLBSTRDCUT_ES2FE_D[3:0];
	T4K37_write_cmos_sensor(0x3197, 0x0000);  //  SENDUM_1U[7:0];
	T4K37_write_cmos_sensor(0x3198, 0x0000);  //  SENDUM_1W[7:0];
	T4K37_write_cmos_sensor(0x3199, 0x0000);  //  SENDUM_2U[7:0];
	T4K37_write_cmos_sensor(0x319A, 0x0000);  //  SENDUM_2W[7:0];
	T4K37_write_cmos_sensor(0x319B, 0x0000);  //  SENDUM_3U[7:0];
	T4K37_write_cmos_sensor(0x319C, 0x0000);  //  SENDUM_3W[7:0];
	T4K37_write_cmos_sensor(0x31A8, 0x0000);  //  -/-/-/ACT_TESTDAC/-/-/-/TESTDACEN;
	T4K37_write_cmos_sensor(0x31A9, 0x00ff);  //  TDAC_INT[7:0];
	T4K37_write_cmos_sensor(0x31AA, 0x0000);  //  TDAC_MIN[7:0];
	T4K37_write_cmos_sensor(0x31AB, 0x0010);  //  TDAC_STEP[3:0]/-/-/TDAC_SWD[1:0];
	T4K37_write_cmos_sensor(0x31AC, 0x0000);  //  -/-/-/-/-/-/-/AG_TEST;
	T4K37_write_cmos_sensor(0x31AD, 0x0000);  //  DACS_INT[7:0];
	T4K37_write_cmos_sensor(0x31AE, 0x00ff);  //  DACS_MAX[7:0];
	T4K37_write_cmos_sensor(0x31AF, 0x0010);  //  DACS_STEP[3:0]/-/-/DACS_SWD[1:0];
	T4K37_write_cmos_sensor(0x31B0, 0x0080);  //  TESTDAC_RSVOL[7:0];
	T4K37_write_cmos_sensor(0x31B1, 0x0070);  //  TESTDAC_ADVOL[7:0];
	T4K37_write_cmos_sensor(0x31B5, 0x0003);  //  -/-/-/-/SINT_ZS_U[3:0];
	T4K37_write_cmos_sensor(0x31B6, 0x0027);  //  -/SINT_10B_ZS_W[6:0];
	T4K37_write_cmos_sensor(0x31B7, 0x0000);  //  SINT_10B_MZ_U[7:0];
	T4K37_write_cmos_sensor(0x31B8, 0x003b);  //  -/SINT_10B_MZ_W[6:0];
	T4K37_write_cmos_sensor(0x31B9, 0x001b);  //  SINT_NML_RS_U[7:0];
	T4K37_write_cmos_sensor(0x31BA, 0x003b);  //  SINT_10B_RS_W[7:0];
	T4K37_write_cmos_sensor(0x31BB, 0x0011);  //  SINT_NML_RF_U[7:0];
	T4K37_write_cmos_sensor(0x31BC, 0x003b);  //  SINT_10B_RF_W[7:0];
	T4K37_write_cmos_sensor(0x31BD, 0x000c);  //  -/-/SINT_FF_U[5:0];
	T4K37_write_cmos_sensor(0x31BE, 0x000b);  //  -/-/SINT_10B_FF_W[5:0];
	T4K37_write_cmos_sensor(0x31BF, 0x0013);  //  SINT_FB_U[7:0];
	T4K37_write_cmos_sensor(0x31C1, 0x0027);  //  -/SINT_10B_FB_W[6:0];
	T4K37_write_cmos_sensor(0x31C2, 0x0000);  //  -/-/-/-/-/-/SINT_10B_AD_U[9:8];
	T4K37_write_cmos_sensor(0x31C3, 0x00a3);  //  SINT_10B_AD_U[7:0];
	T4K37_write_cmos_sensor(0x31C4, 0x0000);  //  -/-/-/-/-/-/SINT_10B_AD_W[9:8];
	T4K37_write_cmos_sensor(0x31C5, 0x00bb);  //  SINT_10B_AD_W[7:0];
	T4K37_write_cmos_sensor(0x31C6, 0x0011);  //  -/-/SINTX_USHIFT[1:0]/-/-/SINTX_DSHIFT[1:0];
	T4K37_write_cmos_sensor(0x31C7, 0x0000);  //  -/-/-/-/-/-/-/SRST_10B_ZS_U[8];
	T4K37_write_cmos_sensor(0x31C8, 0x0059);  //  SRST_10B_ZS_U[7:0];
	T4K37_write_cmos_sensor(0x31C9, 0x000e);  //  -/-/SRST_ZS_W[5:0];
	T4K37_write_cmos_sensor(0x31CA, 0x0000);  //  -/-/-/-/-/-/SRST_10B_RS_U[9:8];
	T4K37_write_cmos_sensor(0x31CB, 0x00ee);  //  SRST_10B_RS_U[7:0];
	T4K37_write_cmos_sensor(0x31CC, 0x0010);  //  -/-/SRST_RS_W[5:0];
	T4K37_write_cmos_sensor(0x31CD, 0x0007);  //  -/-/-/-/SRST_AD_U[3:0];
	T4K37_write_cmos_sensor(0x31CE, 0x0000);  //  -/-/-/-/-/-/-/SRST_10B_AD_D[8];
	T4K37_write_cmos_sensor(0x31CF, 0x005b);  //  SRST_10B_AD_D[7:0];
	T4K37_write_cmos_sensor(0x31D0, 0x0008);  //  FIX_VREFSHBGR[1:0]/-/-/EN_VREFC_ZERO/-/-/-;
	T4K37_write_cmos_sensor(0x31D1, 0x0000);  //  -/-/-/-/-/-/-/VREFSHBGR_U[8];
	T4K37_write_cmos_sensor(0x31D2, 0x0002);  //  VREFSHBGR_U[7:0];
	T4K37_write_cmos_sensor(0x31D3, 0x0007);  //  -/-/-/-/VREFSHBGR_D[3:0];
	T4K37_write_cmos_sensor(0x31D4, 0x0001);  //  VREF_H_START_U[7:0];
	T4K37_write_cmos_sensor(0x31D6, 0x0000);  //  ADCMP1SRT_LOW/-/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x31D7, 0x0000);  //  ADCMP1SRT_NML_RS_U[7:0];
	T4K37_write_cmos_sensor(0x31D8, 0x0000);  //  ADCMP1SRT_NML_AD_U[9:8]/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x31D9, 0x0000);  //  ADCMP1SRT_NML_AD_U[7:0];
	T4K37_write_cmos_sensor(0x31DA, 0x0015);  //  -/ADCMP1SRT_RS_D[6:0];
	T4K37_write_cmos_sensor(0x31DB, 0x0015);  //  -/ADCMP1SRT_AD_D[6:0];
	T4K37_write_cmos_sensor(0x31DC, 0x00e0);  //  CDS_STOPBST/CDS_CMP_BSTOFF/CDS_ADC_BSTOFF/CDS_ADC_BSTOFF_TEST/BSTCKLFIX_HIGH/-/BSTCKLFIX_MERGE/-;
	T4K37_write_cmos_sensor(0x31DD, 0x0010);  //  -/BSTCKLFIX_CMP_U[6:0];
	T4K37_write_cmos_sensor(0x31DE, 0x000e);  //  -/BSTCKLFIX_CMP_D[6:0];
	T4K37_write_cmos_sensor(0x31DF, 0x0009);  //  -/-/BSTCKLFIX_RS_U[5:0];
	T4K37_write_cmos_sensor(0x31E0, 0x0001);  //  -/-/-/BSTCKLFIX_RS_D[4:0];
	T4K37_write_cmos_sensor(0x31E1, 0x0009);  //  -/-/BSTCKLFIX_AD_U[5:0];
	T4K37_write_cmos_sensor(0x31E2, 0x0001);  //  -/-/-/BSTCKLFIX_AD_D[4:0];
	T4K37_write_cmos_sensor(0x3200, 0x0012);  //  -/-/-/BK_ON/-/-/BK_HOKAN_MODE/DANSA_ON;
	T4K37_write_cmos_sensor(0x3201, 0x0012);  //  -/-/BK_START_LINE[1:0]/-/-/BK_MODE[1:0];
	T4K37_write_cmos_sensor(0x3203, 0x0000);  //  BK_FRAME[7:0];
	T4K37_write_cmos_sensor(0x3204, 0x0000);  //  -/-/-/BK_LEV_AGDELTA[4:0];
	T4K37_write_cmos_sensor(0x3205, 0x0080);  //  BK_LEVEL_ALL[7:0];
	T4K37_write_cmos_sensor(0x3206, 0x0000);  //  -/-/-/-/BK_LEVEL_COL1[11:8];
	T4K37_write_cmos_sensor(0x3207, 0x0000);  //  BK_LEVEL_COL1[7:0];
	T4K37_write_cmos_sensor(0x3208, 0x0000);  //  -/-/-/-/BK_LEVEL_COL2[11:8];
	T4K37_write_cmos_sensor(0x3209, 0x0000);  //  BK_LEVEL_COL2[7:0];
	T4K37_write_cmos_sensor(0x320A, 0x0000);  //  -/-/-/-/BK_LEVEL_COL3[11:8];
	T4K37_write_cmos_sensor(0x320B, 0x0000);  //  BK_LEVEL_COL3[7:0];
	T4K37_write_cmos_sensor(0x320C, 0x0000);  //  -/-/-/-/BK_LEVEL_COL4[11:8];
	T4K37_write_cmos_sensor(0x320D, 0x0000);  //  BK_LEVEL_COL4[7:0];
	T4K37_write_cmos_sensor(0x320E, 0x0000);  //  -/-/-/-/BK_LEVEL_COL5[11:8];
	T4K37_write_cmos_sensor(0x320F, 0x0000);  //  BK_LEVEL_COL5[7:0];
	T4K37_write_cmos_sensor(0x3210, 0x0000);  //  -/-/-/-/BK_LEVEL_COL6[11:8];
	T4K37_write_cmos_sensor(0x3211, 0x0000);  //  BK_LEVEL_COL6[7:0];
	T4K37_write_cmos_sensor(0x3212, 0x0000);  //  -/-/-/-/BK_LEVEL_COL7[11:8];
	T4K37_write_cmos_sensor(0x3213, 0x0000);  //  BK_LEVEL_COL7[7:0];
	T4K37_write_cmos_sensor(0x3214, 0x0000);  //  -/-/-/-/BK_LEVEL_COL8[11:8];
	T4K37_write_cmos_sensor(0x3215, 0x0000);  //  BK_LEVEL_COL8[7:0];
	T4K37_write_cmos_sensor(0x3216, 0x0000);  //  -/BK_MPY_COL1[6:0];
	T4K37_write_cmos_sensor(0x3217, 0x0000);  //  -/BK_MPY_COL2[6:0];
	T4K37_write_cmos_sensor(0x3218, 0x0000);  //  -/BK_MPY_COL3[6:0];
	T4K37_write_cmos_sensor(0x3219, 0x0000);  //  -/BK_MPY_COL4[6:0];
	T4K37_write_cmos_sensor(0x321A, 0x0000);  //  -/BK_MPY_COL5[6:0];
	T4K37_write_cmos_sensor(0x321B, 0x0000);  //  -/BK_MPY_COL6[6:0];
	T4K37_write_cmos_sensor(0x321C, 0x0000);  //  -/BK_MPY_COL7[6:0];
	T4K37_write_cmos_sensor(0x321D, 0x0000);  //  -/BK_MPY_COL8[6:0];
	T4K37_write_cmos_sensor(0x321E, 0x000c);  //  -/-/-/-/AG_LEVEL_MAX[3:0];
	T4K37_write_cmos_sensor(0x321F, 0x0084);  //  AG_LEVEL_MID[3:0]/AG_LEVEL_MIN[3:0];
	T4K37_write_cmos_sensor(0x3220, 0x0000);  //  -/-/-/VOB_DISP/-/-/-/HOB_DISP;
	T4K37_write_cmos_sensor(0x3221, 0x0000);  //  -/MPY_LIPOL/MPY_CSPOL/MPY_2LPOL/-/LVL_LIPOL/LVL_CSPOL/LVL_2LPOL;
	T4K37_write_cmos_sensor(0x3222, 0x0000);  //  BK_TEST_FCNT/TEST_CUP/-/CB_VALID_SEL/-/-/TEST_CHART_H/TEST_CHART_W;
	T4K37_write_cmos_sensor(0x3223, 0x0003);  //  -/-/VE_CURS_TEMP/VE_CURS_SCRL_MAX/-/-/VE_CURS_SPEED[1:0];
	T4K37_write_cmos_sensor(0x3224, 0x0020);  //  -/AGMAX_OB_REFLEV[6:0];
	T4K37_write_cmos_sensor(0x3225, 0x0020);  //  -/AGMIN_OB_REFLEV[6:0];
	T4K37_write_cmos_sensor(0x3226, 0x0020);  //  -/-/AGMAX_OB_WIDTH[5:0];
	T4K37_write_cmos_sensor(0x3227, 0x0018);  //  -/-/AGMIN_OB_WIDTH[5:0];
	T4K37_write_cmos_sensor(0x3230, 0x0000);  //  -/-/-/-/-/-/DAMP_LIPOL/DAMP_CSPOL;
	T4K37_write_cmos_sensor(0x3231, 0x0023);  //  PWB_RG[7:0];
	T4K37_write_cmos_sensor(0x3232, 0x0000);  //  PWB_GRG[7:0];
	T4K37_write_cmos_sensor(0x3233, 0x0000);  //  PWB_GBG[7:0];
	T4K37_write_cmos_sensor(0x3234, 0x0044);  //  PWB_BG[7:0];
	T4K37_write_cmos_sensor(0x3235, 0x0080);  //  DAMP_BLACK[7:0];
	T4K37_write_cmos_sensor(0x3237, 0x0080);  //  LSSC_EN/-/-/TEST_LSSC/LSSC_DISP/-/LSSC_LIPOL/LSSC_CSPOL;
	T4K37_write_cmos_sensor(0x3238, 0x0000);  //  LSSC_HCNT_ADJ[7:0];
	T4K37_write_cmos_sensor(0x3239, 0x0080);  //  LSSC_HCNT_MPY[7:0];
	T4K37_write_cmos_sensor(0x323A, 0x0080);  //  LSSC_HCEN_ADJ[7:0];
	T4K37_write_cmos_sensor(0x323B, 0x0000);  //  LSSC_VCNT_ADJ[7:0];
	T4K37_write_cmos_sensor(0x323C, 0x0081);  //  LSSC_VCNT_MPYSW/-/-/-/LSSC_VCNT_MPY[11:8];
	T4K37_write_cmos_sensor(0x323D, 0x0000);  //  LSSC_VCNT_MPY[7:0];
	T4K37_write_cmos_sensor(0x323E, 0x0002);  //  LSSC_VCEN_WIDTH/-/-/-/-/-/LSSC_VCEN_ADJ[9:8];
	T4K37_write_cmos_sensor(0x323F, 0x0000);  //  LSSC_VCEN_ADJ[7:0];
	T4K37_write_cmos_sensor(0x3240, 0x0010);  //  LSSC_TOPL_PM1RG[7:0];
	T4K37_write_cmos_sensor(0x3241, 0x0032);  //  LSSC_TOPL_PM1GRG[7:0];
	T4K37_write_cmos_sensor(0x3242, 0x0032);  //  LSSC_TOPL_PM1GBG[7:0];
	T4K37_write_cmos_sensor(0x3243, 0x000d);  //  LSSC_TOPL_PM1BG[7:0];
	T4K37_write_cmos_sensor(0x3244, 0x001b);  //  LSSC_TOPR_PM1RG[7:0];
	T4K37_write_cmos_sensor(0x3245, 0x0020);  //  LSSC_TOPR_PM1GRG[7:0];
	T4K37_write_cmos_sensor(0x3246, 0x0022);  //  LSSC_TOPR_PM1GBG[7:0];
	T4K37_write_cmos_sensor(0x3247, 0x000c);  //  LSSC_TOPR_PM1BG[7:0];
	T4K37_write_cmos_sensor(0x3248, 0x000e);  //  LSSC_BOTL_PM1RG[7:0];
	T4K37_write_cmos_sensor(0x3249, 0x0020);  //  LSSC_BOTL_PM1GRG[7:0];
	T4K37_write_cmos_sensor(0x324A, 0x0020);  //  LSSC_BOTL_PM1GBG[7:0];
	T4K37_write_cmos_sensor(0x324B, 0x0006);  //  LSSC_BOTL_PM1BG[7:0];
	T4K37_write_cmos_sensor(0x324C, 0x0022);  //  LSSC_BOTR_PM1RG[7:0];
	T4K37_write_cmos_sensor(0x324D, 0x001c);  //  LSSC_BOTR_PM1GRG[7:0];
	T4K37_write_cmos_sensor(0x324E, 0x001c);  //  LSSC_BOTR_PM1GBG[7:0];
	T4K37_write_cmos_sensor(0x324F, 0x000c);  //  LSSC_BOTR_PM1BG[7:0];
	T4K37_write_cmos_sensor(0x3250, 0x0000);  //  -/-/-/-/LSSC1BG_PMSW/LSSC1GBG_PMSW/LSSC1GRG_PMSW/LSSC1RG_PMSW;
	T4K37_write_cmos_sensor(0x3251, 0x0050);  //  LSSC_LEFT_P2RG[7:0];
	T4K37_write_cmos_sensor(0x3252, 0x0074);  //  LSSC_LEFT_P2GRG[7:0];
	T4K37_write_cmos_sensor(0x3253, 0x0072);  //  LSSC_LEFT_P2GBG[7:0];
	T4K37_write_cmos_sensor(0x3254, 0x0068);  //  LSSC_LEFT_P2BG[7:0];
	T4K37_write_cmos_sensor(0x3255, 0x0064);  //  LSSC_RIGHT_P2RG[7:0];
	T4K37_write_cmos_sensor(0x3256, 0x006c);  //  LSSC_RIGHT_P2GRG[7:0];
	T4K37_write_cmos_sensor(0x3257, 0x006c);  //  LSSC_RIGHT_P2GBG[7:0];
	T4K37_write_cmos_sensor(0x3258, 0x0072);  //  LSSC_RIGHT_P2BG[7:0];
	T4K37_write_cmos_sensor(0x3259, 0x0060);  //  LSSC_TOP_P2RG[7:0];
	T4K37_write_cmos_sensor(0x325A, 0x0082);  //  LSSC_TOP_P2GRG[7:0];
	T4K37_write_cmos_sensor(0x325B, 0x0082);  //  LSSC_TOP_P2GBG[7:0];
	T4K37_write_cmos_sensor(0x325C, 0x0074);  //  LSSC_TOP_P2BG[7:0];
	T4K37_write_cmos_sensor(0x325D, 0x0071);  //  LSSC_BOTTOM_P2RG[7:0];
	T4K37_write_cmos_sensor(0x325E, 0x0074);  //  LSSC_BOTTOM_P2GRG[7:0];
	T4K37_write_cmos_sensor(0x325F, 0x0074);  //  LSSC_BOTTOM_P2GBG[7:0];
	T4K37_write_cmos_sensor(0x3260, 0x0070);  //  LSSC_BOTTOM_P2BG[7:0];
	T4K37_write_cmos_sensor(0x3261, 0x0052);  //  LSSC_LEFT_PM4RG[7:0];
	T4K37_write_cmos_sensor(0x3262, 0x0004);  //  LSSC_LEFT_PM4GRG[7:0];
	T4K37_write_cmos_sensor(0x3263, 0x0004);  //  LSSC_LEFT_PM4GBG[7:0];
	T4K37_write_cmos_sensor(0x3264, 0x0000);  //  LSSC_LEFT_PM4BG[7:0];
	T4K37_write_cmos_sensor(0x3265, 0x0040);  //  LSSC_RIGHT_PM4RG[7:0];
	T4K37_write_cmos_sensor(0x3266, 0x0008);  //  LSSC_RIGHT_PM4GRG[7:0];
	T4K37_write_cmos_sensor(0x3267, 0x0008);  //  LSSC_RIGHT_PM4GBG[7:0];
	T4K37_write_cmos_sensor(0x3268, 0x0000);  //  LSSC_RIGHT_PM4BG[7:0];
	T4K37_write_cmos_sensor(0x3269, 0x0016);  //  LSSC_TOP_PM4RG[7:0];
	T4K37_write_cmos_sensor(0x326A, 0x0000);  //  LSSC_TOP_PM4GRG[7:0];
	T4K37_write_cmos_sensor(0x326B, 0x0000);  //  LSSC_TOP_PM4GBG[7:0];
	T4K37_write_cmos_sensor(0x326C, 0x0000);  //  LSSC_TOP_PM4BG[7:0];
	T4K37_write_cmos_sensor(0x326D, 0x0020);  //  LSSC_BOTTOM_PM4RG[7:0];
	T4K37_write_cmos_sensor(0x326E, 0x0000);  //  LSSC_BOTTOM_PM4GRG[7:0];
	T4K37_write_cmos_sensor(0x326F, 0x0000);  //  LSSC_BOTTOM_PM4GBG[7:0];
	T4K37_write_cmos_sensor(0x3270, 0x0000);  //  LSSC_BOTTOM_PM4BG[7:0];
	T4K37_write_cmos_sensor(0x3271, 0x0080);  //  LSSC_MGSEL[1:0]/-/-/LSSC4BG_PMSW/LSSC4GBG_PMSW/LSSC4GRG_PMSW/LSSC4RG_PMSW;
	T4K37_write_cmos_sensor(0x3272, 0x0000);  //  -/-/-/-/-/-/-/LSSC_BLACK[8];
	T4K37_write_cmos_sensor(0x3273, 0x0080);  //  LSSC_BLACK[7:0];
	T4K37_write_cmos_sensor(0x3274, 0x0001);  //  -/-/-/-/-/-/-/OBOFFSET_MPY_LSSC;
	T4K37_write_cmos_sensor(0x3275, 0x0000);  //  -/-/-/-/-/-/-/LSSC_HGINV;
	T4K37_write_cmos_sensor(0x3276, 0x0000);  //  -/-/-/-/-/-/-/LSSC_TEST_STRK;
	T4K37_write_cmos_sensor(0x3282, 0x00c0);  //  ABPC_EN/ABPC_CK_EN/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3284, 0x0006);  //  -/-/-/DM_L_GAIN_WH[4:0];
	T4K37_write_cmos_sensor(0x3285, 0x0003);  //  -/-/-/DM_M_GAIN_WH[4:0];
	T4K37_write_cmos_sensor(0x3286, 0x0002);  //  -/-/-/DM_H_GAIN_WH[4:0];
	T4K37_write_cmos_sensor(0x3287, 0x0040);  //  DM_L_THRS_WH[7:0];
	T4K37_write_cmos_sensor(0x3288, 0x0080);  //  DM_H_THRS_WH[7:0];
	T4K37_write_cmos_sensor(0x3289, 0x0006);  //  -/-/-/DM_L_GAIN_BK[4:0];
	T4K37_write_cmos_sensor(0x328A, 0x0003);  //  -/-/-/DM_M_GAIN_BK[4:0];
	T4K37_write_cmos_sensor(0x328B, 0x0002);  //  -/-/-/DM_H_GAIN_BK[4:0];
	T4K37_write_cmos_sensor(0x328C, 0x0040);  //  DM_L_THRS_BK[7:0];
	T4K37_write_cmos_sensor(0x328D, 0x0080);  //  DM_H_THRS_BK[7:0];
	T4K37_write_cmos_sensor(0x328E, 0x0013);  //  -/-/DARK_LIMIT[5:0];
	T4K37_write_cmos_sensor(0x328F, 0x0080);  //  ABPC_M_THRS[7:0];
	T4K37_write_cmos_sensor(0x3290, 0x0020);  //  ABPC_DSEL/ABPC_MSEL/ABPC_RSEL/USE_ABPC_AVE/TEST_ABPC/-/-/TEST_PPSRAM_ON;
	T4K37_write_cmos_sensor(0x3291, 0x0000);  //  -/-/-/-/-/-/ABPC_VBLK_SEL/ABPC_TEST_STRK;
	T4K37_write_cmos_sensor(0x3294, 0x0001);  //  -/-/-/MAP_MEMLESS/-/-/TEST_MAP_DISP_H/HDR_WCLIP_ON;
	T4K37_write_cmos_sensor(0x3295, 0x007f);  //  TEST_WCLIP_ON/HDR_WCLIP_W[6:0];
	T4K37_write_cmos_sensor(0x3296, 0x0010);  //  -/-/-/DFCT_XADJ[4:0];
	T4K37_write_cmos_sensor(0x3297, 0x0000);  //  -/-/-/-/-/DFCT_YDLY_VAL[1:0]/DFCT_YDLY_SW;
	T4K37_write_cmos_sensor(0x329E, 0x0000);  //  -/-/-/-/-/-/-/MEMIF_RWPOL;
	T4K37_write_cmos_sensor(0x329F, 0x0004);  //  -/-/-/-/-/MEMIF_MAX_ADR1[10:8];
	T4K37_write_cmos_sensor(0x32A0, 0x003e);  //  MEMIF_MAX_ADR1[7:0];
	T4K37_write_cmos_sensor(0x32A1, 0x0002);  //  -/-/-/-/-/MEMIF_MAX_ADR2[10:8];
	T4K37_write_cmos_sensor(0x32A2, 0x001f);  //  MEMIF_MAX_ADR2[7:0];
	T4K37_write_cmos_sensor(0x32A8, 0x0084);  //  CNR_SW/-/-/-/-/CNR_USE_FB/CNR_USE_FB_1ST/CNR_USE_FB_ABF;
	T4K37_write_cmos_sensor(0x32A9, 0x0002);  //  -/-/-/-/-/-/CNR_UPDATE_FB/CNR_UPDATE_FB_MASK;
	T4K37_write_cmos_sensor(0x32AB, 0x0000);  //  -/-/-/-/-/CNR_ABF_DGR[2:0];
	T4K37_write_cmos_sensor(0x32AC, 0x0000);  //  -/-/-/-/-/CNR_ABF_DGG[2:0];
	T4K37_write_cmos_sensor(0x32AD, 0x0000);  //  -/-/-/-/-/CNR_ABF_DGB[2:0];
	T4K37_write_cmos_sensor(0x32AF, 0x0000);  //  -/-/CNR_CNF_DARK_AG0[5:0];
	T4K37_write_cmos_sensor(0x32B0, 0x0000);  //  -/-/CNR_CNF_DARK_AG1[5:0];
	T4K37_write_cmos_sensor(0x32B1, 0x0000);  //  -/-/CNR_CNF_DARK_AG2[5:0];
	T4K37_write_cmos_sensor(0x32B3, 0x0010);  //  -/-/-/CNR_CNF_RATIO_AG0[4:0];
	T4K37_write_cmos_sensor(0x32B4, 0x001f);  //  -/-/-/CNR_CNF_RATIO_AG1[4:0];
	T4K37_write_cmos_sensor(0x32B5, 0x001f);  //  -/-/-/CNR_CNF_RATIO_AG2[4:0];
	T4K37_write_cmos_sensor(0x32B7, 0x003b);  //  -/-/CNR_CNF_CLIP_GAIN_R[1:0]/CNR_CNF_CLIP_GAIN_G[1:0]/CNR_CNF_CLIP_GAIN_B[1:0];
	T4K37_write_cmos_sensor(0x32B8, 0x00ff);  //  CNR_CNF_DITHER_LEVEL[7:0];
	T4K37_write_cmos_sensor(0x32BA, 0x0004);  //  -/-/CNR_A1L_DARK_AG0[5:0];
	T4K37_write_cmos_sensor(0x32BB, 0x000f);  //  -/-/CNR_A1L_DARK_AG1[5:0];
	T4K37_write_cmos_sensor(0x32BC, 0x000f);  //  -/-/CNR_A1L_DARK_AG2[5:0];
	T4K37_write_cmos_sensor(0x32BE, 0x0004);  //  -/-/-/CNR_A1L_RATIO_AG0[4:0];
	T4K37_write_cmos_sensor(0x32BF, 0x000f);  //  -/-/-/CNR_A1L_RATIO_AG1[4:0];
	T4K37_write_cmos_sensor(0x32C0, 0x000f);  //  -/-/-/CNR_A1L_RATIO_AG2[4:0]
	T4K37_write_cmos_sensor(0x32C1, 0x0000);  //  -/CNR_INF_ZERO_CLIP_AG0[6:0];
	T4K37_write_cmos_sensor(0x32C2, 0x0000);  //  -/CNR_INF_ZERO_CLIP_AG1[6:0];
	T4K37_write_cmos_sensor(0x32C3, 0x0000);  //  -/CNR_INF_ZERO_CLIP_AG2[6:0];
	T4K37_write_cmos_sensor(0x32C5, 0x0001);  //  CNR_TOUT_SEL[3:0]/-/-/CNR_CNF_SHIFT[1:0];
	T4K37_write_cmos_sensor(0x32C6, 0x0050);  //  -/CNR_DCOR_SW[2:0]/-/-/-/CNR_MERGE_YLSEL//from 0x0 to 0x5
	T4K37_write_cmos_sensor(0x32C8, 0x000e);  //  -/-/-/CNR_MERGE_HLBLEND_MIN_AG0[4:0]// YNR OFF
	T4K37_write_cmos_sensor(0x32C9, 0x0004);  //  -/-/-/CNR_MERGE_HLBLEND_MIN_AG1[4:0]// YNR MID
	T4K37_write_cmos_sensor(0x32CA, 0x0000);  //  -/-/-/CNR_MERGE_HLBLEND_MIN_AG2[4:0]// YNR HI
	T4K37_write_cmos_sensor(0x32CB, 0x000e);  //  -/-/-/CNR_MERGE_HLBLEND_MAX_AG0[4:0];
	T4K37_write_cmos_sensor(0x32CC, 0x000e);  //  -/-/-/CNR_MERGE_HLBLEND_MAX_AG1[4:0];
	T4K37_write_cmos_sensor(0x32CD, 0x000e);  //  -/-/-/CNR_MERGE_HLBLEND_MAX_AG2[4:0];
	T4K37_write_cmos_sensor(0x32CE, 0x0008);  //  -/-/-/-/CNR_MERGE_HLBLEND_DARK_AG0[3:0];
	T4K37_write_cmos_sensor(0x32CF, 0x0008);  //  -/-/-/-/CNR_MERGE_HLBLEND_DARK_AG1[3:0];
	T4K37_write_cmos_sensor(0x32D0, 0x0008);  //  -/-/-/-/CNR_MERGE_HLBLEND_DARK_AG2[3:0];
	T4K37_write_cmos_sensor(0x32D1, 0x000f);  //  -/-/-/-/CNR_MERGE_HLBLEND_RATIO_AG0[3:0];
	T4K37_write_cmos_sensor(0x32D2, 0x000f);  //  -/-/-/-/CNR_MERGE_HLBLEND_RATIO_AG1[3:0];
	T4K37_write_cmos_sensor(0x32D3, 0x000f);  //  -/-/-/-/CNR_MERGE_HLBLEND_RATIO_AG2[3:0];
	T4K37_write_cmos_sensor(0x32D4, 0x0008);  //  CNR_MERGE_HLBLEND_SLOPE_AG0[7:0];
	T4K37_write_cmos_sensor(0x32D5, 0x0008);  //  CNR_MERGE_HLBLEND_SLOPE_AG1[7:0];
	T4K37_write_cmos_sensor(0x32D6, 0x0008);  //  CNR_MERGE_HLBLEND_SLOPE_AG2[7:0];
	T4K37_write_cmos_sensor(0x32D8, 0x0000);  //  -/-/-/CNR_MERGE_D2BLEND_AG0[4:0];
	T4K37_write_cmos_sensor(0x32D9, 0x0000);  //  -/-/-/CNR_MERGE_D2BLEND_AG1[4:0];
	T4K37_write_cmos_sensor(0x32DA, 0x0000);  //  -/-/-/CNR_MERGE_D2BLEND_AG2[4:0];
	T4K37_write_cmos_sensor(0x32DD, 0x0002);  //  -/-/CNR_MERGE_MINDIV_AG0[5:0];
	T4K37_write_cmos_sensor(0x32DE, 0x0004);  //  -/-/CNR_MERGE_MINDIV_AG1[5:0];
	T4K37_write_cmos_sensor(0x32DF, 0x0004);  //  -/-/CNR_MERGE_MINDIV_AG2[5:0];
	T4K37_write_cmos_sensor(0x32E0, 0x0020);  //  -/CNR_MERGE_BLACK_AG0[6:0];
	T4K37_write_cmos_sensor(0x32E1, 0x0020);  //  -/CNR_MERGE_BLACK_AG1[6:0];
	T4K37_write_cmos_sensor(0x32E2, 0x0020);  //  -/CNR_MERGE_BLACK_AG2[6:0];
	T4K37_write_cmos_sensor(0x32E4, 0x0000);  //  -/-/CNR_ABCY_DARK_AG0[5:0];
	T4K37_write_cmos_sensor(0x32E5, 0x0000);  //  -/-/CNR_ABCY_DARK_AG1[5:0];
	T4K37_write_cmos_sensor(0x32E6, 0x0000);  //  -/-/CNR_ABCY_DARK_AG2[5:0];
	T4K37_write_cmos_sensor(0x32E7, 0x0000);  //  -/-/-/-/CNR_ABCY_RATIO_AG0[3:0];
	T4K37_write_cmos_sensor(0x32E8, 0x0000);  //  -/-/-/-/CNR_ABCY_RATIO_AG1[3:0];
	T4K37_write_cmos_sensor(0x32E9, 0x0000);  //  -/-/-/-/CNR_ABCY_RATIO_AG2[3:0];
	T4K37_write_cmos_sensor(0x32EA, 0x0000);  //  -/-/-/-/-/CNR_ABCY_WEIGHT_AG0[2:0];
	T4K37_write_cmos_sensor(0x32EB, 0x0000);  //  -/-/-/-/-/CNR_ABCY_WEIGHT_AG1[2:0];
	T4K37_write_cmos_sensor(0x32EC, 0x0000);  //  -/-/-/-/-/CNR_ABCY_WEIGHT_AG2[2:0];
	T4K37_write_cmos_sensor(0x32ED, 0x0000);  //  -/-/-/-/-/-/-/CNR_USE_FIXED_SIZE;
	T4K37_write_cmos_sensor(0x32EE, 0x0000);  //  -/-/-/CNR_IMAGE_WIDTH[12:8];
	T4K37_write_cmos_sensor(0x32EF, 0x0000);  //  CNR_IMAGE_WIDTH[7:0];
	T4K37_write_cmos_sensor(0x32F0, 0x0000);  //  -/-/-/-/CNR_IMAGE_HEIGHT[11:8];
	T4K37_write_cmos_sensor(0x32F1, 0x0000);  //  CNR_IMAGE_HEIGHT[7:0];
	T4K37_write_cmos_sensor(0x32F4, 0x0003);  //  -/-/-/-/-/-/PDS_SIGSEL[1:0];
	T4K37_write_cmos_sensor(0x32F5, 0x0080);  //  AGMAX_PED[7:0];
	T4K37_write_cmos_sensor(0x32F6, 0x0080);  //  AGMIN_PED[7:0];
	T4K37_write_cmos_sensor(0x32F7, 0x0000);  //  -/-/-/-/-/-/-/PP_DCROP_SW;
	T4K37_write_cmos_sensor(0x32F8, 0x0003);  //  -/PP_HCNT_MGN[2:0]/-/PP_VCNT_MGN[2:0];
	T4K37_write_cmos_sensor(0x32F9, 0x0003);  //  -/-/-/-/-/PP_VPIX_MGN[2:0];
	T4K37_write_cmos_sensor(0x32FA, 0x0001);  //  -/-/-/-/-/PP_VBLK_SEL[2:0];
	T4K37_write_cmos_sensor(0x32FD, 0x00f0);  //  PP_RESV[7:0];
	T4K37_write_cmos_sensor(0x32FE, 0x0000);  //  PP_RESV_A[7:0];
	T4K37_write_cmos_sensor(0x32FF, 0x0000);  //  PP_RESV_B[7:0];
	T4K37_write_cmos_sensor(0x3300, 0x000a);  //  -/-/-/-/WKUP_WAIT_ON/VCO_CONV[2:0];
	T4K37_write_cmos_sensor(0x3301, 0x0005);  //  ES_MARGIN[7:0];
	T4K37_write_cmos_sensor(0x3302, 0x0000);  //  -/-/-/-/-/SP_COUNT[10:8];
	T4K37_write_cmos_sensor(0x3303, 0x0070);  //  SP_COUNT[7:0];
	T4K37_write_cmos_sensor(0x3304, 0x0000);  //  -/-/-/-/-/HREG_HRST_POS[10:8];
	T4K37_write_cmos_sensor(0x3305, 0x0000);  //  HREG_HRST_POS[7:0];
	T4K37_write_cmos_sensor(0x3307, 0x003d);  //  AG_MIN[7:0];
	T4K37_write_cmos_sensor(0x3308, 0x003c);  //  AG_MAX[7:0];
	T4K37_write_cmos_sensor(0x3309, 0x000D);  //  -/-/-/-/AGCHG_BK_PERIOD[1:0]/-/INCLUDE_ESCHG;
	T4K37_write_cmos_sensor(0x330A, 0x0001);  //  -/-/-/-/-/-/BIN_MODE[1:0];
	T4K37_write_cmos_sensor(0x330B, 0x0001);  //  -/-/-/-/-/-/VLAT_GLBFRM_RO/VBLK_RGIF_2H;
	T4K37_write_cmos_sensor(0x330C, 0x0002);  //  -/-/-/-/GRST_ALLZEROSET_NUM[3:0];
	T4K37_write_cmos_sensor(0x330D, 0x0000);  //  -/-/-/-/-/HFV_ES_REF[2:0];
	T4K37_write_cmos_sensor(0x330E, 0x0000);  //  -/-/-/-/-/-/-/USE_OP_FRM_END;
	T4K37_write_cmos_sensor(0x3310, 0x0003);  //  -/-/-/-/-/ESYNC_SW/VSYNC_PH/HSYNC_PH;
	T4K37_write_cmos_sensor(0x3311, 0x0005);  //  -/HC_PRESET[14:8];
	T4K37_write_cmos_sensor(0x3312, 0x0042);  //  HC_PRESET[7:0];
	T4K37_write_cmos_sensor(0x3313, 0x0000);  //  VC_PRESET[15:8];
	T4K37_write_cmos_sensor(0x3314, 0x0000);  //  VC_PRESET[7:0];
	T4K37_write_cmos_sensor(0x3316, 0x0000);  //  TEST_TG_MODE[7:0];
	T4K37_write_cmos_sensor(0x3318, 0x0040);  //  LTCH_CYCLE[1:0]/-/-/-/-/-/-;
	T4K37_write_cmos_sensor(0x3319, 0x000a);  //  -/-/-/V_ADR_ADJ[4:0];
	T4K37_write_cmos_sensor(0x3380, 0x0000);  //  -/-/-/TEST_SCCLK_ON/-/-/TEST_ABCLK_ON/TEST_PPCLK_ON;
	T4K37_write_cmos_sensor(0x3381, 0x0001);  //  -/-/-/-/-/AD_CNTL[2:0];
	T4K37_write_cmos_sensor(0x3383, 0x0008);  //  -/-/-/-/ST_CNTL[3:0];
	T4K37_write_cmos_sensor(0x3384, 0x0000);  //  -/-/-/VTCK_PLLSEL/-/-/-/PLL_SINGLE_SW;
	T4K37_write_cmos_sensor(0x338B, 0x0000);  //  -/-/-/-/-/-/-/REGVD_SEL;
	T4K37_write_cmos_sensor(0x338C, 0x0005);  //  -/-/-/-/BST_CNTL[3:0];
	T4K37_write_cmos_sensor(0x338D, 0x0003);  //  PLL_SNR_CNTL[1:0]/PLL_SYS_CNTL[1:0]/-/VCO_EN_SEL/VCO_EN/DIVRSTX;
	T4K37_write_cmos_sensor(0x338E, 0x0000);  //  -/ICP_SEL[1:0]/LPFR_SEL[1:0]/AMON0_SEL[2:0];
	T4K37_write_cmos_sensor(0x338F, 0x0020);  //  PCMODE/-/ICP_PCH/ICP_NCH/-/-/-/VCO_TESTSEL;
	T4K37_write_cmos_sensor(0x3390, 0x0000);  //  -/-/AUTO_ICP_R_SEL_LG/AUTO_ICP_R_SEL_ST/-/-/PLLEV_EN/PLLEV_SEL;
	T4K37_write_cmos_sensor(0x3391, 0x0000);  //  -/-/-/-/-/-/-/ADCK_DIV2_CNTL;
	T4K37_write_cmos_sensor(0x339B, 0x0000);  //  -/-/-/-/-/SC_MPYOFFSET[2:0];
	T4K37_write_cmos_sensor(0x339C, 0x0000);  //  IPHASE_DIR_INPUT/IPHASE_EVN[6:0];
	T4K37_write_cmos_sensor(0x339D, 0x0000);  //  -/IPHASE_ODD[6:0];
	T4K37_write_cmos_sensor(0x33B2, 0x0000);  //  -/-/-/-/-/-/-/PISO_MSKN;
	T4K37_write_cmos_sensor(0x33B3, 0x0000);  //  SLEEP_SW/VCO_STP_SW/PHY_PWRON_SW/SLEEP_CKSP_SW/SLEEP_MN/VCO_STP_MN/PHY_PWRON_MN/SLEEP_CKSP_MN;
	T4K37_write_cmos_sensor(0x33B4, 0x0001);  //  -/-/-/-/-/-/-/SMIAPP_SW;
	T4K37_write_cmos_sensor(0x33B5, 0x0003);  //  -/-/-/-/-/-/CLR_TRG/B_FLS_MSKGPH;
	T4K37_write_cmos_sensor(0x33B6, 0x0000);  //  SENSTP_SW/-/-/-/SENSTP_MN/-/-/-;
	T4K37_write_cmos_sensor(0x33D0, 0x0000);  //  -/-/DRVUP[5:0];
	T4K37_write_cmos_sensor(0x33E0, 0x0000);  //  -/-/-/DCLK_POL/-/PARA_SW[2:0];
	T4K37_write_cmos_sensor(0x33E1, 0x0001);  //  -/-/-/STBIO_HZ/-/-/-/PARA_HZ;
	T4K37_write_cmos_sensor(0x33E2, 0x0000);  //  -/-/OPCK_SEL[1:0]/-/-/VTCK_SEL[1:0];
	T4K37_write_cmos_sensor(0x33E4, 0x0000);  //  -/-/-/-/TM_MODE_SEL[3:0];
	T4K37_write_cmos_sensor(0x33E5, 0x0000);  //  -/-/-/-/-/-/-/PLL_BIST_CK_EN;
	T4K37_write_cmos_sensor(0x33F1, 0x0000);  //  -/-/-/-/REG18_CNT[3:0];
	T4K37_write_cmos_sensor(0x33FF, 0x0000);  //  RG_RSVD_REG[7:0];
	T4K37_write_cmos_sensor(0x3405, 0x0000);  //  -/-/SC_TEST[1:0]/EMB_OUT_SW/HFCORROFF/EQ_MONI[1:0];
	T4K37_write_cmos_sensor(0x3420, 0x0000);  //  -/-/B_LB_LANE_SEL[1:0]/B_LBTEST_CLR/B_LB_TEST_EN/-/B_LB_MODE;
	T4K37_write_cmos_sensor(0x3424, 0x0000);  //  -/-/-/-/B_TRIG_Z5_X/B_TX_TRIGOPT/B_CLKULPS/B_ESCREQ;
	T4K37_write_cmos_sensor(0x3425, 0x0078);  //  B_ESCDATA[7:0];
	T4K37_write_cmos_sensor(0x3426, 0x00ff);  //  B_TRIG_DUMMY[7:0];
	T4K37_write_cmos_sensor(0x3427, 0x00c0);  //  B_MIPI_CLKVBLK/B_MIPI_CLK_MODE/-/-/B_HS_SR_CNT[1:0]/B_LP_SR_CNT[1:0];
	T4K37_write_cmos_sensor(0x3428, 0x0000);  //  -/-/-/-/B_LANE_OFF[3:0];
	T4K37_write_cmos_sensor(0x3429, 0x0040);  //  -/B_PHASE_ADJ[2:0]/-/-/-/-;
	T4K37_write_cmos_sensor(0x342A, 0x0000);  //  -/B_CK_DELAY[2:0]/-/-/-/-;
	T4K37_write_cmos_sensor(0x342B, 0x0000);  //  -/B_D1_DELAY[2:0]/-/B_D2_DELAY[2:0];
	T4K37_write_cmos_sensor(0x342C, 0x0000);  //  -/B_D3_DELAY[2:0]/-/B_D4_DELAY[2:0];
	T4K37_write_cmos_sensor(0x342D, 0x0000);  //  -/-/-/B_READ_TRIG/-/-/B_EN_PHASE_SEL[1:0];
	T4K37_write_cmos_sensor(0x342E, 0x0000);  //  -/-/-/-/-/-/B_FIFODLY[9:8];
	T4K37_write_cmos_sensor(0x342F, 0x0000);  //  B_FIFODLY[7:0];
	T4K37_write_cmos_sensor(0x3430, 0x00a7);  //  B_NUMWAKE[7:0];
	T4K37_write_cmos_sensor(0x3431, 0x0060);  //  B_NUMINIT[7:0];
	T4K37_write_cmos_sensor(0x3432, 0x0011);  //  -/-/-/B_CLK0_M/-/-/-/B_LNKBTWK_ON;
	T4K37_write_cmos_sensor(0x3433, 0x0000);  //  -/-/-/-/B_OP_TEST[3:0];
	T4K37_write_cmos_sensor(0x3434, 0x0000);  //  B_T_VALUE1[7:0];
	T4K37_write_cmos_sensor(0x3435, 0x0000);  //  B_T_VALUE2[7:0];
	T4K37_write_cmos_sensor(0x3436, 0x0000);  //  B_T_VALUE3[7:0];
	T4K37_write_cmos_sensor(0x3437, 0x0000);  //  B_T_VALUE4[7:0];
	T4K37_write_cmos_sensor(0x3438, 0x0000);  //  -/-/-/-/-/B_MIPI_LANE_SEL[2:0];
	T4K37_write_cmos_sensor(0x3439, 0x0000);  //  B_TLPX_LINKOFF/-/B_TCLK_ZERO_LINKOFF/B_TCLK_PREPARE_LINKOFF/B_TCLK_TRAIL_LINKOFF/B_THS_TRAIL_LINKOFF/B_THS_ZERO_LINKOFF/B_THS_PREPARE_LINKOFF;
	T4K37_write_cmos_sensor(0x343A, 0x0000);  //  -/-/-/-/-/-/-/B_HFVCOUNT_MODE;
	T4K37_write_cmos_sensor(0x3500, 0x0000);  //  OTP_STA/-/-/-/-/OTP_CLRE/OTP_WREC/OTP_ENBL;
	T4K37_write_cmos_sensor(0x3502, 0x0000);  //  OTP_PSEL[7:0];
	T4K37_write_cmos_sensor(0x3504, 0x0000);  //  OTP_DATA0[7:0];
	T4K37_write_cmos_sensor(0x3505, 0x0000);  //  OTP_DATA1[7:0];
	T4K37_write_cmos_sensor(0x3506, 0x0000);  //  OTP_DATA2[7:0];
	T4K37_write_cmos_sensor(0x3507, 0x0000);  //  OTP_DATA3[7:0];
	T4K37_write_cmos_sensor(0x3508, 0x0000);  //  OTP_DATA4[7:0];
	T4K37_write_cmos_sensor(0x3509, 0x0000);  //  OTP_DATA5[7:0];
	T4K37_write_cmos_sensor(0x350A, 0x0000);  //  OTP_DATA6[7:0];
	T4K37_write_cmos_sensor(0x350B, 0x0000);  //  OTP_DATA7[7:0];
	T4K37_write_cmos_sensor(0x350C, 0x0000);  //  OTP_DATA8[7:0];
	T4K37_write_cmos_sensor(0x350D, 0x0000);  //  OTP_DATA9[7:0];
	T4K37_write_cmos_sensor(0x350E, 0x0000);  //  OTP_DATA10[7:0];
	T4K37_write_cmos_sensor(0x350F, 0x0000);  //  OTP_DATA11[7:0];
	T4K37_write_cmos_sensor(0x3510, 0x0000);  //  OTP_DATA12[7:0];
	T4K37_write_cmos_sensor(0x3511, 0x0000);  //  OTP_DATA13[7:0];
	T4K37_write_cmos_sensor(0x3512, 0x0000);  //  OTP_DATA14[7:0];
	T4K37_write_cmos_sensor(0x3513, 0x0000);  //  OTP_DATA15[7:0];
	T4K37_write_cmos_sensor(0x3514, 0x0000);  //  OTP_DATA16[7:0];
	T4K37_write_cmos_sensor(0x3515, 0x0000);  //  OTP_DATA17[7:0];
	T4K37_write_cmos_sensor(0x3516, 0x0000);  //  OTP_DATA18[7:0];
	T4K37_write_cmos_sensor(0x3517, 0x0000);  //  OTP_DATA19[7:0];
	T4K37_write_cmos_sensor(0x3518, 0x0000);  //  OTP_DATA20[7:0];
	T4K37_write_cmos_sensor(0x3519, 0x0000);  //  OTP_DATA21[7:0];
	T4K37_write_cmos_sensor(0x351A, 0x0000);  //  OTP_DATA22[7:0];
	T4K37_write_cmos_sensor(0x351B, 0x0000);  //  OTP_DATA23[7:0];
	T4K37_write_cmos_sensor(0x351C, 0x0000);  //  OTP_DATA24[7:0];
	T4K37_write_cmos_sensor(0x351D, 0x0000);  //  OTP_DATA25[7:0];
	T4K37_write_cmos_sensor(0x351E, 0x0000);  //  OTP_DATA26[7:0];
	T4K37_write_cmos_sensor(0x351F, 0x0000);  //  OTP_DATA27[7:0];
	T4K37_write_cmos_sensor(0x3520, 0x0000);  //  OTP_DATA28[7:0];
	T4K37_write_cmos_sensor(0x3521, 0x0000);  //  OTP_DATA29[7:0];
	T4K37_write_cmos_sensor(0x3522, 0x0000);  //  OTP_DATA30[7:0];
	T4K37_write_cmos_sensor(0x3523, 0x0000);  //  OTP_DATA31[7:0];
	T4K37_write_cmos_sensor(0x3524, 0x0000);  //  OTP_DATA32[7:0];
	T4K37_write_cmos_sensor(0x3525, 0x0000);  //  OTP_DATA33[7:0];
	T4K37_write_cmos_sensor(0x3526, 0x0000);  //  OTP_DATA34[7:0];
	T4K37_write_cmos_sensor(0x3527, 0x0000);  //  OTP_DATA35[7:0];
	T4K37_write_cmos_sensor(0x3528, 0x0000);  //  OTP_DATA36[7:0];
	T4K37_write_cmos_sensor(0x3529, 0x0000);  //  OTP_DATA37[7:0];
	T4K37_write_cmos_sensor(0x352A, 0x0000);  //  OTP_DATA38[7:0];
	T4K37_write_cmos_sensor(0x352B, 0x0000);  //  OTP_DATA39[7:0];
	T4K37_write_cmos_sensor(0x352C, 0x0000);  //  OTP_DATA40[7:0];
	T4K37_write_cmos_sensor(0x352D, 0x0000);  //  OTP_DATA41[7:0];
	T4K37_write_cmos_sensor(0x352E, 0x0000);  //  OTP_DATA42[7:0];
	T4K37_write_cmos_sensor(0x352F, 0x0000);  //  OTP_DATA43[7:0];
	T4K37_write_cmos_sensor(0x3530, 0x0000);  //  OTP_DATA44[7:0];
	T4K37_write_cmos_sensor(0x3531, 0x0000);  //  OTP_DATA45[7:0];
	T4K37_write_cmos_sensor(0x3532, 0x0000);  //  OTP_DATA46[7:0];
	T4K37_write_cmos_sensor(0x3533, 0x0000);  //  OTP_DATA47[7:0];
	T4K37_write_cmos_sensor(0x3534, 0x0000);  //  OTP_DATA48[7:0];
	T4K37_write_cmos_sensor(0x3535, 0x0000);  //  OTP_DATA49[7:0];
	T4K37_write_cmos_sensor(0x3536, 0x0000);  //  OTP_DATA50[7:0];
	T4K37_write_cmos_sensor(0x3537, 0x0000);  //  OTP_DATA51[7:0];
	T4K37_write_cmos_sensor(0x3538, 0x0000);  //  OTP_DATA52[7:0];
	T4K37_write_cmos_sensor(0x3539, 0x0000);  //  OTP_DATA53[7:0];
	T4K37_write_cmos_sensor(0x353A, 0x0000);  //  OTP_DATA54[7:0];
	T4K37_write_cmos_sensor(0x353B, 0x0000);  //  OTP_DATA55[7:0];
	T4K37_write_cmos_sensor(0x353C, 0x0000);  //  OTP_DATA56[7:0];
	T4K37_write_cmos_sensor(0x353D, 0x0000);  //  OTP_DATA57[7:0];
	T4K37_write_cmos_sensor(0x353E, 0x0000);  //  OTP_DATA58[7:0];
	T4K37_write_cmos_sensor(0x353F, 0x0000);  //  OTP_DATA59[7:0];
	T4K37_write_cmos_sensor(0x3540, 0x0000);  //  OTP_DATA60[7:0];
	T4K37_write_cmos_sensor(0x3541, 0x0000);  //  OTP_DATA61[7:0];
	T4K37_write_cmos_sensor(0x3542, 0x0000);  //  OTP_DATA62[7:0];
	T4K37_write_cmos_sensor(0x3543, 0x0000);  //  OTP_DATA63[7:0];
	T4K37_write_cmos_sensor(0x3545, 0x0007);  //  OTP_RWT/OTP_RNUM[1:0]/OTP_VERIFY/OTP_VMOD/OTP_PCLK[2:0];
	T4K37_write_cmos_sensor(0x3547, 0x0000);  //  OTP_TEST[3:0]/OTP_SPBE/OTP_TOEC/OTP_VEEC/OTP_STRC;
	T4K37_write_cmos_sensor(0x354E, 0x0000);  //  -/-/-/-/-/-/-/ECC_OFF;
	T4K37_write_cmos_sensor(0x3560, 0x0000);  //  DFCT_TYP0[1:0]/-/DFCT_XADR0[12:8];
	T4K37_write_cmos_sensor(0x3561, 0x0000);  //  DFCT_XADR0[7:0];
	T4K37_write_cmos_sensor(0x3562, 0x0000);  //  -/-/-/-/DFCT_YADR0[11:8];
	T4K37_write_cmos_sensor(0x3563, 0x0000);  //  DFCT_YADR0[7:0];
	T4K37_write_cmos_sensor(0x3564, 0x0000);  //  DFCT_TYP1[1:0]/-/DFCT_XADR1[12:8];
	T4K37_write_cmos_sensor(0x3565, 0x0000);  //  DFCT_XADR1[7:0];
	T4K37_write_cmos_sensor(0x3566, 0x0000);  //  -/-/-/-/DFCT_YADR1[11:8];
	T4K37_write_cmos_sensor(0x3567, 0x0000);  //  DFCT_YADR1[7:0];
	T4K37_write_cmos_sensor(0x3568, 0x0000);  //  DFCT_TYP2[1:0]/-/DFCT_XADR2[12:8];
	T4K37_write_cmos_sensor(0x3569, 0x0000);  //  DFCT_XADR2[7:0];
	T4K37_write_cmos_sensor(0x356A, 0x0000);  //  -/-/-/-/DFCT_YADR2[11:8];
	T4K37_write_cmos_sensor(0x356B, 0x0000);  //  DFCT_YADR2[7:0];
	T4K37_write_cmos_sensor(0x356C, 0x0000);  //  DFCT_TYP3[1:0]/-/DFCT_XADR3[12:8];
	T4K37_write_cmos_sensor(0x356D, 0x0000);  //  DFCT_XADR3[7:0];
	T4K37_write_cmos_sensor(0x356E, 0x0000);  //  -/-/-/-/DFCT_YADR3[11:8];
	T4K37_write_cmos_sensor(0x356F, 0x0000);  //  DFCT_YADR3[7:0];
	T4K37_write_cmos_sensor(0x3570, 0x0000);  //  DFCT_TYP4[1:0]/-/DFCT_XADR4[12:8];
	T4K37_write_cmos_sensor(0x3571, 0x0000);  //  DFCT_XADR4[7:0];
	T4K37_write_cmos_sensor(0x3572, 0x0000);  //  -/-/-/-/DFCT_YADR4[11:8];
	T4K37_write_cmos_sensor(0x3573, 0x0000);  //  DFCT_YADR4[7:0];
	T4K37_write_cmos_sensor(0x3574, 0x0000);  //  DFCT_TYP5[1:0]/-/DFCT_XADR5[12:8];
	T4K37_write_cmos_sensor(0x3575, 0x0000);  //  DFCT_XADR5[7:0];
	T4K37_write_cmos_sensor(0x3576, 0x0000);  //  -/-/-/-/DFCT_YADR5[11:8];
	T4K37_write_cmos_sensor(0x3577, 0x0000);  //  DFCT_YADR5[7:0];
	T4K37_write_cmos_sensor(0x3578, 0x0000);  //  DFCT_TYP6[1:0]/-/DFCT_XADR6[12:8];
	T4K37_write_cmos_sensor(0x3579, 0x0000);  //  DFCT_XADR6[7:0];
	T4K37_write_cmos_sensor(0x357A, 0x0000);  //  -/-/-/-/DFCT_YADR6[11:8];
	T4K37_write_cmos_sensor(0x357B, 0x0000);  //  DFCT_YADR6[7:0];
	T4K37_write_cmos_sensor(0x357C, 0x0000);  //  DFCT_TYP7[1:0]/-/DFCT_XADR7[12:8];
	T4K37_write_cmos_sensor(0x357D, 0x0000);  //  DFCT_XADR7[7:0];
	T4K37_write_cmos_sensor(0x357E, 0x0000);  //  -/-/-/-/DFCT_YADR7[11:8];
	T4K37_write_cmos_sensor(0x357F, 0x0000);  //  DFCT_YADR7[7:0];
	T4K37_write_cmos_sensor(0x3580, 0x0000);  //  DFCT_TYP8[1:0]/-/DFCT_XADR8[12:8];
	T4K37_write_cmos_sensor(0x3581, 0x0000);  //  DFCT_XADR8[7:0];
	T4K37_write_cmos_sensor(0x3582, 0x0000);  //  -/-/-/-/DFCT_YADR8[11:8];
	T4K37_write_cmos_sensor(0x3583, 0x0000);  //  DFCT_YADR8[7:0];
	T4K37_write_cmos_sensor(0x3584, 0x0000);  //  DFCT_TYP9[1:0]/-/DFCT_XADR9[12:8];
	T4K37_write_cmos_sensor(0x3585, 0x0000);  //  DFCT_XADR9[7:0];
	T4K37_write_cmos_sensor(0x3586, 0x0000);  //  -/-/-/-/DFCT_YADR9[11:8];
	T4K37_write_cmos_sensor(0x3587, 0x0000);  //  DFCT_YADR9[7:0];
	T4K37_write_cmos_sensor(0x3588, 0x0000);  //  DFCT_TYP10[1:0]/-/DFCT_XADR10[12:8];
	T4K37_write_cmos_sensor(0x3589, 0x0000);  //  DFCT_XADR10[7:0];
	T4K37_write_cmos_sensor(0x358A, 0x0000);  //  -/-/-/-/DFCT_YADR10[11:8];
	T4K37_write_cmos_sensor(0x358B, 0x0000);  //  DFCT_YADR10[7:0];
	T4K37_write_cmos_sensor(0x358C, 0x0000);  //  DFCT_TYP11[1:0]/-/DFCT_XADR11[12:8];
	T4K37_write_cmos_sensor(0x358D, 0x0000);  //  DFCT_XADR11[7:0];
	T4K37_write_cmos_sensor(0x358E, 0x0000);  //  -/-/-/-/DFCT_YADR11[11:8];
	T4K37_write_cmos_sensor(0x358F, 0x0000);  //  DFCT_YADR11[7:0];
	T4K37_write_cmos_sensor(0x3590, 0x0000);  //  DFCT_TYP12[1:0]/-/DFCT_XADR12[12:8];
	T4K37_write_cmos_sensor(0x3591, 0x0000);  //  DFCT_XADR12[7:0];
	T4K37_write_cmos_sensor(0x3592, 0x0000);  //  -/-/-/-/DFCT_YADR12[11:8];
	T4K37_write_cmos_sensor(0x3593, 0x0000);  //  DFCT_YADR12[7:0];
	T4K37_write_cmos_sensor(0x3594, 0x0000);  //  DFCT_TYP13[1:0]/-/DFCT_XADR13[12:8];
	T4K37_write_cmos_sensor(0x3595, 0x0000);  //  DFCT_XADR13[7:0];
	T4K37_write_cmos_sensor(0x3596, 0x0000);  //  -/-/-/-/DFCT_YADR13[11:8];
	T4K37_write_cmos_sensor(0x3597, 0x0000);  //  DFCT_YADR13[7:0];
	T4K37_write_cmos_sensor(0x3598, 0x0000);  //  DFCT_TYP14[1:0]/-/DFCT_XADR14[12:8];
	T4K37_write_cmos_sensor(0x3599, 0x0000);  //  DFCT_XADR14[7:0];
	T4K37_write_cmos_sensor(0x359A, 0x0000);  //  -/-/-/-/DFCT_YADR14[11:8];
	T4K37_write_cmos_sensor(0x359B, 0x0000);  //  DFCT_YADR14[7:0];
	T4K37_write_cmos_sensor(0x359C, 0x0000);  //  DFCT_TYP15[1:0]/-/DFCT_XADR15[12:8];
	T4K37_write_cmos_sensor(0x359D, 0x0000);  //  DFCT_XADR15[7:0];
	T4K37_write_cmos_sensor(0x359E, 0x0000);  //  -/-/-/-/DFCT_YADR15[11:8];
	T4K37_write_cmos_sensor(0x359F, 0x0000);  //  DFCT_YADR15[7:0];
	T4K37_write_cmos_sensor(0x0100, 0x0001);  //  -/-/-/-/-/-/-/MODE_SELECT;


	T4K37DB("T4K37_Sensor_Init exit_4lane :\n ");	
}

void T4K37PreviewSetting(void)
{

	T4K37DB("T4K37PreviewSetting enter_4lane :\n ");
	
	// FPS 30.13 fps PCLK: 292 MHz	CSI-2 CLK: 876 MHz	VCO: 876 MHz
	// Size: 2104x1560 H-count: 5408 V-count: 1792


	T4K37_write_cmos_sensor(0x0104, 0x0001);  //  -/-/-/-/-/-/-/GROUP_PARA_HOLD
	T4K37_write_cmos_sensor(0x0301, 0x0003);  //  -/-/-/-/VT_PIX_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0303, 0x0004);  //  -/-/-/-/VT_SYS_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0305, 0x0007);  //  -/-/-/-/-/PRE_PLL_CLK_DIV[2:0];
	T4K37_write_cmos_sensor(0x0306, 0x0001);  //  -/-/-/-/-/-/-/PLL_MULTIPLIER[8];
	T4K37_write_cmos_sensor(0x0307, 0x0024);  //  PLL_MULTIPLIER[7:0];
	T4K37_write_cmos_sensor(0x030B, 0x0001);  //  -/-/-/-/OP_SYS_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0340, 0x0007);  //  FR_LENGTH_LINES[15:8]
	T4K37_write_cmos_sensor(0x0341, 0x0000);  //  FR_LENGTH_LINES[7:0]
	T4K37_write_cmos_sensor(0x0342, 0x0015);  //  LINE_LENGTH_PCK[15:8]
	T4K37_write_cmos_sensor(0x0343, 0x0020);  //  LINE_LENGTH_PCK[7:0]
	T4K37_write_cmos_sensor(0x0346, 0x0000);  //  Y_ADDR_START[15:8]
	T4K37_write_cmos_sensor(0x0347, 0x0000);  //  Y_ADDR_START[7:0]
	T4K37_write_cmos_sensor(0x0348, 0x000C);  //  Y_ADDR_END[15:8]
	T4K37_write_cmos_sensor(0x0349, 0x002F);  //  Y_ADDR_END[7:0]
	T4K37_write_cmos_sensor(0x034C, 0x0008);  //  X_OUTPUT_SIZE[15:8]
	T4K37_write_cmos_sensor(0x034D, 0x0038);  //  X_OUTPUT_SIZE[7:0]
	T4K37_write_cmos_sensor(0x034E, 0x0006);  //  Y_OUTPUT_SIZE[15:8]
	T4K37_write_cmos_sensor(0x034F, 0x0018);  //  Y_OUTPUT_SIZE[7:0]
	T4K37_write_cmos_sensor(0x0401, 0x0000);  //  -/-/-/-/-/-/SCALING_MODE[1:0]
	T4K37_write_cmos_sensor(0x0404, 0x0010);  //  SCALE_M[7:0]
	T4K37_write_cmos_sensor(0x0900, 0x0001);  //  -/-/-/-/-/-/H_BIN[1:0]
	T4K37_write_cmos_sensor(0x0901, 0x0001);  //  -/-/-/-/-/-/V_BIN_MODE[1:0]
	T4K37_write_cmos_sensor(0x0902, 0x0001);  //  -/-/-/-/-/-/BINNING_WEIGHTING[1:0]
	T4K37_write_cmos_sensor(0x0104, 0x0000);  //  -/-/-/-/-/-/-/GROUP_PARA_HOLD

	
	T4K37DB("T4K37PreviewSetting out_4lane :\n ");
}

void T4K37VideoSetting(void)
{
	// ==========Summary========
	// Extclk= 24Mhz  PCK = 261Mhz	 DCLK = 97.875MHz  MIPI Lane CLK = 391.5MHz  ES 1 line = 13.3333us
	// FPS = 30.05fps	Image output size = 3280x1846
	//sensor max support pclk is 261M
	T4K37DB("T4K37VideoSetting enter_4lane:\n ");	

	T4K37PreviewSetting();//13M video , run preview setting
	return;
	
	//T4K37_write_cmos_sensor(/////////////);/////Video 1080P_30fps				 
	//T4K37_write_cmos_sensor(			 );//									 
	T4K37_write_cmos_sensor(0x0104,0x0001);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD
	
	T4K37_write_cmos_sensor(0x0340,0x0006);// );//FR_LENGTH_LINES[15:8]
	T4K37_write_cmos_sensor(0x0341,0x0024);// );//FR_LENGTH_LINES[7:0]
	T4K37_write_cmos_sensor(0x0342,0x0011);// );//LINE_LENGTH_PCK[15:8]
	T4K37_write_cmos_sensor(0x0343,0x0080);// );//LINE_LENGTH_PCK[7:0]
	T4K37_write_cmos_sensor(0x0346,0x0000);// );//Y_ADDR_START[15:8]
	T4K37_write_cmos_sensor(0x0347,0x0000);// );//Y_ADDR_START[7:0]
	T4K37_write_cmos_sensor(0x034A,0x000A);// );//Y_ADDR_END[15:8]
	T4K37_write_cmos_sensor(0x034B,0x004F);// );//Y_ADDR_END[7:0]
	T4K37_write_cmos_sensor(0x034C,0x0007);// );//X_OUTPUT_SIZE[15:8]
	T4K37_write_cmos_sensor(0x034D,0x0080);// );//X_OUTPUT_SIZE[7:0]
	T4K37_write_cmos_sensor(0x034E,0x0004);// );//Y_OUTPUT_SIZE[15:8]
	T4K37_write_cmos_sensor(0x034F,0x0038);// );//Y_OUTPUT_SIZE[7:0]
	T4K37_write_cmos_sensor(0x0401,0x0002);// );//-/-/-/-/-/-/SCALING_MODE[1:0]
	T4K37_write_cmos_sensor(0x0404,0x0011);// );//SCALE_M[7:0]
	T4K37_write_cmos_sensor(0x0900,0x0001);//-);///-/-/-/-/-/H_BIN[1:0]
	T4K37_write_cmos_sensor(0x0901,0x0001);//-);///-/-/-/-/-/V_BIN_MODE[1:0]
	T4K37_write_cmos_sensor(0x0902,0x0000);//-);///-/-/-/-/-/BINNING_WEIGHTING[1:0]
	
	T4K37_write_cmos_sensor(0x0104,0x0000);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD



	T4K37DB("T4K37VideoSetting out_4lane:\n ");	
}
	


void T4K37CaptureSetting(void)
{

	T4K37DB("T4K37CaptureSetting enter_4lane :\n ");	


	// FPS 23.96 fps PCLK: 389.333333333333 MHz  CSI-2 CLK: 876 MHz  VCO: 876 MHz
	// Size: 4208x3120 H-count: 5168 V-count: 3144

	T4K37_write_cmos_sensor(0x0104, 0x0001);  //  -/-/-/-/-/-/-/GROUP_PARA_HOLD
	T4K37_write_cmos_sensor(0x0301, 0x0003);  //  -/-/-/-/VT_PIX_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0303, 0x0003);  //  -/-/-/-/VT_SYS_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0305, 0x0007);  //  -/-/-/-/-/PRE_PLL_CLK_DIV[2:0];
	T4K37_write_cmos_sensor(0x0306, 0x0001);  //  -/-/-/-/-/-/-/PLL_MULTIPLIER[8];
	T4K37_write_cmos_sensor(0x0307, 0x0024);  //  PLL_MULTIPLIER[7:0];
	T4K37_write_cmos_sensor(0x030B, 0x0001);  //  -/-/-/-/OP_SYS_CLK_DIV[3:0];
	T4K37_write_cmos_sensor(0x0340, 0x000C);  //  FR_LENGTH_LINES[15:8]
	T4K37_write_cmos_sensor(0x0341, 0x0048);  //  FR_LENGTH_LINES[7:0]
	T4K37_write_cmos_sensor(0x0342, 0x0014);  //  LINE_LENGTH_PCK[15:8]
	T4K37_write_cmos_sensor(0x0343, 0x0030);  //  LINE_LENGTH_PCK[7:0]
	T4K37_write_cmos_sensor(0x0346, 0x0000);  //  Y_ADDR_START[15:8]
	T4K37_write_cmos_sensor(0x0347, 0x0000);  //  Y_ADDR_START[7:0]
	T4K37_write_cmos_sensor(0x0348, 0x000C);  //  Y_ADDR_END[15:8]
	T4K37_write_cmos_sensor(0x0349, 0x002F);  //  Y_ADDR_END[7:0]
	T4K37_write_cmos_sensor(0x034C, 0x0010);  //  X_OUTPUT_SIZE[15:8]
	T4K37_write_cmos_sensor(0x034D, 0x0070);  //  X_OUTPUT_SIZE[7:0]
	T4K37_write_cmos_sensor(0x034E, 0x000C);  //  Y_OUTPUT_SIZE[15:8]
	T4K37_write_cmos_sensor(0x034F, 0x0030);  //  Y_OUTPUT_SIZE[7:0]
	T4K37_write_cmos_sensor(0x0401, 0x0000);  //  -/-/-/-/-/-/SCALING_MODE[1:0]
	T4K37_write_cmos_sensor(0x0404, 0x0010);  //  SCALE_M[7:0]
	T4K37_write_cmos_sensor(0x0900, 0x0000);  //  -/-/-/-/-/-/H_BIN[1:0]
	T4K37_write_cmos_sensor(0x0901, 0x0000);  //  -/-/-/-/-/-/V_BIN_MODE[1:0]
	T4K37_write_cmos_sensor(0x0902, 0x0000);  //  -/-/-/-/-/-/BINNING_WEIGHTING[1:0]
	T4K37_write_cmos_sensor(0x0104, 0x0000);  //  -/-/-/-/-/-/-/GROUP_PARA_HOLD

	mdelay(50);

	T4K37DB("T4K37CaptureSetting out_4lane:\n ");

}



UINT32 T4K37Open(void)
{
	kal_uint16 sensor_id = 0;
	
	T4K37DB("T4K37Open enter :\n ");
	
	sensor_id = T4K37_read_cmos_sensor(0x0000);
	T4K37DB("T4K37 READ ID :%x",sensor_id);
	
	if(sensor_id != T4K37_SENSOR_ID)
	//if(sensor_id != 0x1c)
	{
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	
	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.sensorMode = SENSOR_MODE_INIT;
	t4k37.T4K37AutoFlickerMode = KAL_FALSE;
	t4k37.T4K37VideoMode = KAL_FALSE;
	spin_unlock(&t4k37mipiraw_drv_lock);
	
	T4K37_Sensor_Init();
	
	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.DummyLines= 0;
	t4k37.DummyPixels= 0;
	
	//#define T4K37_PREVIEW_PCLK 							(211200000)
	//#define T4K37_VIDEO_PCLK							(211200000)
	//#define T4K37_CAPTURE_PCLK							(211200000)
	t4k37.pvPclk =  (T4K37_PREVIEW_PCLK/10000);//(24200);
	t4k37.capPclk = (T4K37_CAPTURE_PCLK/10000);//(24200);
	t4k37.videoPclk = (T4K37_VIDEO_PCLK/10000);//(26100);
	
	t4k37.shutter = 0x09C0;
	t4k37.maxExposureLines =T4K37_PV_PERIOD_LINE_NUMS;
	
	t4k37.ispBaseGain = BASEGAIN;//0x40
	t4k37.sensorGlobalGain = T4K37_ANALOG_GAIN_1X;//1X
	t4k37.pvGain = T4K37_ANALOG_GAIN_1X;
	t4k37.realGain = BASEGAIN;//1x
	spin_unlock(&t4k37mipiraw_drv_lock);
	
	T4K37DB("T4K37Open exit :\n ");
	
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   T4K37GetSensorID
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
UINT32 T4K37GetSensorID(UINT32 *sensorID)
{
    int  retry = 3; 

	T4K37DB("T4K37GetSensorID enter :\n ");
    mDELAY(5);
	
    do {
        *sensorID = T4K37_read_cmos_sensor(0x0000);

        if (*sensorID == T4K37_SENSOR_ID)
        	{
        		T4K37DB("Sensor ID = 0x%04x\n", *sensorID);
            	break; 
        	}
        T4K37DB("Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
    } while (retry > 0);
	//*sensorID = 0x1C;//for test

    if (*sensorID != T4K37_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   T4K37_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of T4K37 to change exposure time.
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
void T4K37_SetShutter(kal_uint32 iShutter)
{

	T4K37DB("4K04:featurecontrol:iShutter=%d\n",iShutter);

//	if(t4k37.shutter == iShutter)
//		return;	
	
   spin_lock(&t4k37mipiraw_drv_lock);
   t4k37.shutter= iShutter;
   spin_unlock(&t4k37mipiraw_drv_lock);
   
   T4K37_write_shutter(iShutter);
   return;
}   /*  T4K37_SetShutter   */



/*************************************************************************
* FUNCTION
*   T4K37_read_shutter
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
UINT32 T4K37_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2;
	UINT32 shutter =0;

	temp_reg1 = T4K37_read_cmos_sensor(0x0202);
	temp_reg2 = T4K37_read_cmos_sensor(0x0203);
	
	shutter  = ((temp_reg1 <<8)|temp_reg2);

	return shutter;
}

/*************************************************************************
* FUNCTION
*   T4K37_night_mode
*
* DESCRIPTION
*   This function night mode of T4K37.
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
void T4K37_NightMode(kal_bool bEnable)
{
}



/*************************************************************************
* FUNCTION
*   T4K37Close
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
UINT32 T4K37Close(void)
{
    //work-around for Power-on sequence[SW-SleepIn]
	//T4K37_write_cmos_sensor(0x0100,0x00);

    return ERROR_NONE;
}

void T4K37SetFlipMirror(kal_int32 imgMirror)
{

	T4K37DB("T4K37SetFlipMirror  imgMirror =%d:\n ",imgMirror);	

	T4K37_write_cmos_sensor(0x0104,0x0001);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 

    switch (imgMirror)
    {
        case IMAGE_NORMAL:
            T4K37_write_cmos_sensor(0x0101,0x00);
            break;
        case IMAGE_H_MIRROR:
            T4K37_write_cmos_sensor(0x0101,0x01);
            break;
        case IMAGE_V_MIRROR:
            T4K37_write_cmos_sensor(0x0101,0x02);
            break;
        case IMAGE_HV_MIRROR:
            T4K37_write_cmos_sensor(0x0101,0x03);
            break;
    }
	
	T4K37_write_cmos_sensor(0x0104,0x0000);// );//-/-/-/-/-/-/-/GROUP_PARA_HOLD 
}


/*************************************************************************
* FUNCTION
*   T4K37Preview
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
UINT32 T4K37Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	T4K37DB("T4K37Preview enter:");
	
	T4K37PreviewSetting();
 
	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.sensorMode = SENSOR_MODE_PREVIEW;
	t4k37.DummyPixels =0;
    t4k37.DummyLines  =0;
	spin_unlock(&t4k37mipiraw_drv_lock);

	//T4K37DB("[T4K37Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&t4k37mipiraw_drv_lock);
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	
	t4k37.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k37mipiraw_drv_lock);
	//T4K37_SetDummy(0, 0);
	
	T4K37SetFlipMirror(sensor_config_data->SensorImageMirror);
		mdelay(50);
	T4K37DB("T4K37Preview exit:\n");
    return ERROR_NONE;
}

UINT32 T4K37Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	T4K37DB("T4K37Video enter:");


	T4K37VideoSetting();
	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.sensorMode = SENSOR_MODE_VIDEO;
	t4k37.DummyPixels =0;
    t4k37.DummyLines  =0;
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	t4k37.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k37mipiraw_drv_lock);
	//T4K37_SetDummy(0, 0);

	T4K37SetFlipMirror(sensor_config_data->SensorImageMirror);
		mdelay(50);
	T4K37DB("T4K37Video exit:\n");
    return ERROR_NONE;
}



UINT32 T4K37Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = t4k37.shutter;
	kal_uint32 pv_line_length , cap_line_length,temp_data;
	
	T4K37DB("T4K37Capture enter:\n");
	
	if( SENSOR_MODE_CAPTURE != t4k37.sensorMode)
	{
		T4K37CaptureSetting();
	}
	
	
	spin_lock(&t4k37mipiraw_drv_lock);		
	t4k37.sensorMode = SENSOR_MODE_CAPTURE;
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	t4k37.imgMirror = sensor_config_data->SensorImageMirror;
	t4k37.DummyPixels =0;
	t4k37.DummyLines  =0;
	spin_unlock(&t4k37mipiraw_drv_lock);
	//T4K37_SetDummy(0, 0);
	
	T4K37SetFlipMirror(sensor_config_data->SensorImageMirror);

	//T4K37_SetDummy( t4k37.DummyPixels, t4k37.DummyLines);
	mdelay(50);
	

	T4K37DB("T4K37Capture exit:\n");
    return ERROR_NONE;
}	/* T4K37Capture() */


UINT32 T4K37GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    T4K37DB("T4K37GetResolution!!\n");
	pSensorResolution->SensorPreviewWidth	= T4K37_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= T4K37_IMAGE_SENSOR_PV_HEIGHT;
	
	pSensorResolution->SensorVideoWidth		= T4K37_IMAGE_SENSOR_VDO_WIDTH;
    pSensorResolution->SensorVideoHeight	= T4K37_IMAGE_SENSOR_VDO_HEIGHT;
	
    pSensorResolution->SensorFullWidth		= T4K37_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= T4K37_IMAGE_SENSOR_FULL_HEIGHT; 
	
    return ERROR_NONE;
} 

UINT32 T4K37GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{	
	pSensorConfigData->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	
	switch(pSensorConfigData->SensorImageMirror)
	{
		case IMAGE_NORMAL:
			 pSensorInfo->SensorOutputDataFormat   = SENSOR_OUTPUT_FORMAT_RAW_Gr;
             break;
		case IMAGE_H_MIRROR:
			pSensorInfo->SensorOutputDataFormat	 = SENSOR_OUTPUT_FORMAT_RAW_R;
			break;
		case IMAGE_V_MIRROR:
			pSensorInfo->SensorOutputDataFormat	  = SENSOR_OUTPUT_FORMAT_RAW_B;
			break;
		case IMAGE_HV_MIRROR:
			pSensorInfo->SensorOutputDataFormat    = SENSOR_OUTPUT_FORMAT_RAW_Gb;
			break;
	}
	pSensorInfo->SensorOutputDataFormat	  = SENSOR_OUTPUT_FORMAT_FIRST_PIXEL;

	spin_lock(&t4k37mipiraw_drv_lock);
	t4k37.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&t4k37mipiraw_drv_lock);
	T4K37DB("[T4K37GetInfo]SensorImageMirror:%d\n", t4k37.imgMirror );

	pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW; 
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->CaptureDelayFrame = 4;  
	
	
    pSensorInfo->PreviewDelayFrame = 4; 
    pSensorInfo->VideoDelayFrame = 4;  

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;      
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 1 ;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;	
	
	   
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K37_PV_X_START; 
            pSensorInfo->SensorGrabStartY = T4K37_PV_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;

        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K37_VIDEO_X_START; 
            pSensorInfo->SensorGrabStartY = T4K37_VIDEO_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;

        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;
			
            pSensorInfo->SensorGrabStartX = T4K37_FULL_X_START;	
            pSensorInfo->SensorGrabStartY = T4K37_FULL_Y_START;	
            
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;
			
            pSensorInfo->SensorGrabStartX = T4K37_PV_X_START; 
            pSensorInfo->SensorGrabStartY = T4K37_PV_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;            
            break;
    }

    memcpy(pSensorConfigData, &T4K37SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}


UINT32 T4K37Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&t4k37mipiraw_drv_lock);
		T4K37CurrentScenarioId = ScenarioId;
		spin_unlock(&t4k37mipiraw_drv_lock);
		T4K37DB("T4K37CurrentScenarioId=%d\n",T4K37CurrentScenarioId);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            T4K37Preview(pImageWindow, pSensorConfigData);
            break;

        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            T4K37Video(pImageWindow, pSensorConfigData);
			break;
			
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            T4K37Capture(pImageWindow, pSensorConfigData);
            break;

        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
}


UINT32 T4K37SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MAX_Frame_length =0,frameRate=0,extralines=0;
    T4K37DB("[T4K37SetVideoMode] frame rate = %d\n", u2FrameRate);

	if(u2FrameRate==0)
	{
		T4K37DB("T4K37SetVideoMode FPS:0\n");

		//for dynamic fps
	//	t4k37.DummyPixels = 0;
	//	t4k37.DummyLines = 0;
	//	T4K37_SetDummy(t4k37.DummyPixels,t4k37.DummyLines);
		
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    T4K37DB("error frame rate seting,u2FrameRate =%d\n",u2FrameRate);
	//u2FrameRate = 15;

    if(t4k37.sensorMode == SENSOR_MODE_VIDEO)
    {
    	if(t4k37.T4K37AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 296;
			else if(u2FrameRate==15)
				frameRate= 148;
			else
				frameRate=u2FrameRate*10;
			
			MAX_Frame_length = (t4k37.videoPclk*10000)/(T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/frameRate*10;
    	}
		else
			MAX_Frame_length = (t4k37.videoPclk*10000) /(T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/u2FrameRate;
				
		if((MAX_Frame_length <=T4K37_VIDEO_PERIOD_LINE_NUMS))
		{
			MAX_Frame_length = T4K37_VIDEO_PERIOD_LINE_NUMS;
			T4K37DB("[T4K37SetVideoMode]current fps = %d\n", (t4k37.videoPclk*10000)/(T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/T4K37_PV_PERIOD_LINE_NUMS);	
		}
		T4K37DB("[T4K37SetVideoMode]current fps (10 base)= %d\n", (t4k37.videoPclk*10000)*10/(T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels)/MAX_Frame_length);
		extralines = MAX_Frame_length - T4K37_VIDEO_PERIOD_LINE_NUMS;
	
		T4K37_SetDummy(t4k37.DummyPixels,extralines);
    }

	T4K37DB("[T4K37SetVideoMode]MAX_Frame_length=%d,t4k37.DummyLines=%d\n",MAX_Frame_length,t4k37.DummyLines);
	
    return KAL_TRUE;
}

UINT32 T4K37SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{

    T4K37DB("[T4K37SetAutoFlickerMode] enable =%d, frame rate(10base) =  %d\n", bEnable, u2FrameRate);
	if(bEnable) {
		spin_lock(&t4k37mipiraw_drv_lock);
		t4k37.T4K37AutoFlickerMode = KAL_TRUE; 
		spin_unlock(&t4k37mipiraw_drv_lock);
    } else {
    	spin_lock(&t4k37mipiraw_drv_lock);
        t4k37.T4K37AutoFlickerMode = KAL_FALSE; 
		spin_unlock(&t4k37mipiraw_drv_lock);
        T4K37DB("Disable Auto flicker\n");    
    }

    return ERROR_NONE;
}

UINT32 T4K37SetTestPatternMode(kal_bool bEnable)
{
    T4K37DB("[T4K37SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	if(bEnable)
		T4K37_write_cmos_sensor(0x0601,2);
	else
		T4K37_write_cmos_sensor(0x0601,0);
		
    return ERROR_NONE;
}


UINT32 T4K37MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	T4K37DB("T4K37MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = T4K37_PREVIEW_PCLK;
			lineLength = T4K37_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K37_PV_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k37mipiraw_drv_lock);
			t4k37.DummyLines = dummyLine;
			t4k37.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&t4k37mipiraw_drv_lock);
			T4K37_SetDummy(t4k37.DummyPixels,t4k37.DummyLines);
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = T4K37_VIDEO_PCLK;
			lineLength = T4K37_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K37_VIDEO_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k37mipiraw_drv_lock);
			t4k37.DummyLines = dummyLine;
			t4k37.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&t4k37mipiraw_drv_lock);
			T4K37_SetDummy(t4k37.DummyPixels,t4k37.DummyLines);
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = T4K37_CAPTURE_PCLK;
			lineLength = T4K37_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K37_FULL_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k37mipiraw_drv_lock);
			t4k37.DummyLines = dummyLine;
			t4k37.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&t4k37mipiraw_drv_lock);
			T4K37_SetDummy(t4k37.DummyPixels,t4k37.DummyLines);
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


UINT32 T4K37MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 278;
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

UINT32 T4K37FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
            *pFeatureReturnPara16++= T4K37_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= T4K37_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			switch(T4K37CurrentScenarioId)
			{
				//T4K37DB("T4K37FeatureControl:T4K37CurrentScenarioId:%d\n",T4K37CurrentScenarioId);
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
            		*pFeatureReturnPara16++= T4K37_FULL_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;  
            		*pFeatureReturnPara16= T4K37_FULL_PERIOD_LINE_NUMS + t4k37.DummyLines;	
            		T4K37DB("capture_Sensor period:%d ,%d\n", T4K37_FULL_PERIOD_PIXEL_NUMS + t4k37.DummyPixels, T4K37_FULL_PERIOD_LINE_NUMS + t4k37.DummyLines); 
            		*pFeatureParaLen=4;        				
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            		*pFeatureReturnPara16++= T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;  
            		*pFeatureReturnPara16= T4K37_VIDEO_PERIOD_LINE_NUMS + t4k37.DummyLines;	
            		T4K37DB("video_Sensor period:%d ,%d\n", T4K37_VIDEO_PERIOD_PIXEL_NUMS + t4k37.DummyPixels, T4K37_VIDEO_PERIOD_LINE_NUMS + t4k37.DummyLines); 
            		*pFeatureParaLen=4;  
        			break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            		*pFeatureReturnPara16++= T4K37_PV_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;  
            		*pFeatureReturnPara16= T4K37_PV_PERIOD_LINE_NUMS + t4k37.DummyLines;
            		T4K37DB("preview_Sensor period:%d ,%d\n", T4K37_PV_PERIOD_PIXEL_NUMS  + t4k37.DummyPixels, T4K37_PV_PERIOD_LINE_NUMS + t4k37.DummyLines); 
            		*pFeatureParaLen=4;
        			break;
				default:	
            		*pFeatureReturnPara16++= T4K37_PV_PERIOD_PIXEL_NUMS + t4k37.DummyPixels;  
            		*pFeatureReturnPara16= T4K37_PV_PERIOD_LINE_NUMS + t4k37.DummyLines;
            		T4K37DB("Sensor period:%d ,%d\n", T4K37_PV_PERIOD_PIXEL_NUMS  + t4k37.DummyPixels, T4K37_PV_PERIOD_LINE_NUMS + t4k37.DummyLines); 
            		*pFeatureParaLen=4;
        			break;
  				}
          	break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(T4K37CurrentScenarioId)
			{
				//T4K37DB("T4K37FeatureControl:T4K37CurrentScenarioId:%d\n",T4K37CurrentScenarioId);
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = T4K37_PREVIEW_PCLK;
					*pFeatureParaLen=4;
				break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = T4K37_VIDEO_PCLK;
					*pFeatureParaLen=4;
				break;
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = T4K37_CAPTURE_PCLK;
					*pFeatureParaLen=4;
				break;
				default:
					*pFeatureReturnPara32 = T4K37_PREVIEW_PCLK;
					*pFeatureParaLen=4;
				break;
			}
		    break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            T4K37_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            T4K37_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            T4K37_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //T4K37_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            T4K37_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = T4K37_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&t4k37mipiraw_drv_lock);
                T4K37SensorCCT[i].Addr=*pFeatureData32++;
                T4K37SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&t4k37mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=T4K37SensorCCT[i].Addr;
                *pFeatureData32++=T4K37SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&t4k37mipiraw_drv_lock);
                T4K37SensorReg[i].Addr=*pFeatureData32++;
                T4K37SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&t4k37mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=T4K37SensorReg[i].Addr;
                *pFeatureData32++=T4K37SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=T4K37_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, T4K37SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, T4K37SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &T4K37SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            T4K37_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            T4K37_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=T4K37_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            T4K37_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            T4K37_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            T4K37_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
			pSensorEngInfo->SensorOutputDataFormat	  = SENSOR_OUTPUT_FORMAT_FIRST_PIXEL;
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
            T4K37SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            T4K37GetSensorID(pFeatureReturnPara32); 
            break;             
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            T4K37SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));            
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            T4K37SetTestPatternMode((BOOL)*pFeatureData16);        	
            break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:           
            *pFeatureReturnPara32= T4K37_TEST_PATTERN_CHECKSUM;           
            *pFeatureParaLen=4;                             
            break;			
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			T4K37MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			T4K37MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
			
        default:
            break;
    }
    return ERROR_NONE;
}	/* T4K37FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncT4K37=
{
    T4K37Open,  
    T4K37GetInfo,
    T4K37GetResolution,
    T4K37FeatureControl,
    T4K37Control,
    T4K37Close
};

UINT32 T4K37_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncT4K37;

    return ERROR_NONE;
}   /* SensorInit() */

