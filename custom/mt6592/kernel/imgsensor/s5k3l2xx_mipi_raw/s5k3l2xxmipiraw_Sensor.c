#include <linux/videodev2.h> 
#include <linux/i2c.h>  
#include <linux/platform_device.h>
#include <linux/delay.h> 
#include <linux/cdev.h>
#include <linux/uaccess.h>    
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/xlog.h> 

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3l2xxmipiraw_Sensor.h"
#include "s5k3l2xxmipiraw_Camera_Sensor_para.h"
#include "s5k3l2xxmipiraw_CameraCustomized.h"

static DEFINE_SPINLOCK(s5k3l2xxmipiraw_drv_lock);

#define S5K3L2XX_DEBUG
#define S5K3L2XX_TEST_PATTERN_CHECKSUM (0xe1b26f6c)

#ifdef S5K3L2XX_DEBUG
	#define S5K3L2XXDB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[S5K3L2XXMIPI]" , fmt, ##arg)
#else
	#define S5K3L2XXDB(x,...)
#endif

kal_uint32 S5K3L2XX_FeatureControl_PERIOD_PixelNum;
kal_uint32 S5K3L2XX_FeatureControl_PERIOD_LineNum;
kal_uint32 S5K3L2XX_FAC_SENSOR_REG;
kal_uint32 S5K3L2XX_VIDEO_FRAMELENGTH = s5k3l2xx_video_frame_length;

MSDK_SENSOR_CONFIG_STRUCT S5K3L2XXSensorConfigData;
MSDK_SCENARIO_ID_ENUM S5K3L2XXCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

SENSOR_REG_STRUCT S5K3L2XXSensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT S5K3L2XXSensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;

static S5K3L2XX_PARA_STRUCT s5k3l2xx;

extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

#ifdef MULTIPLE_I2C_SUPPORT
extern int iMultiWriteReg(u8 *pData, u16 lens, u16 i2cId);
#define S5K3L2XX_multi_write_cmos_sensor(pData, lens) iMultiWriteReg((u8*) pData, (u16) lens, S5K3L2XXMIPI_WRITE_ID)
#endif

#define Sleep(ms) mdelay(ms)
#define mDELAY(ms)  mdelay(ms)

inline void S5K3L2XX_write_cmos_sensor1(u16 addr, u32 para)
{
	char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,  (char)(para >> 8),	(char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd , 4, S5K3L2XXMIPI_WRITE_ID);
}



inline void S5K3L2XX_write_cmos_sensor(u16 addr, u32 para)
{
	char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF)  ,	(char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd , 3, S5K3L2XXMIPI_WRITE_ID);
}



inline kal_uint16 S5K3L2XX_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,S5K3L2XXMIPI_WRITE_ID);
	return get_byte&0x00ff;
}

//#define S5K3L2XX_USE_AWB_OTP

#if defined(S5K3L2XX_USE_AWB_OTP)

#define GAIN_DEFAULT       0x0100
#define S5K3L2XXOTP_WRITE_ID         0xB0

inline kal_uint16 S5K3L2XX_read_cmos_sensor1(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	
	char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte, 1, S5K3L2XXOTP_WRITE_ID);
	return get_byte&0x00ff;
}



void S5K3L2XX_MIPI_read_otp_wb(struct S5K3L2XX_MIPI_otp_struct *otp)
{
   	kal_uint16 golden_R, golden_G, golden_Gr, golden_Gb, golden_B, current_R, current_G, current_Gr, current_Gb, current_B, r_ratio, b_ratio, FLG = 0x00;
   
   	FLG = S5K3L2XX_read_cmos_sensor1(0x0000); 
   
  	if(FLG==0)
		S5K3L2XXDB("No OTP Data or OTP data is invalid");
   	else
   	{
		golden_R = 0x86;
		golden_Gr = 0xA1;
 		golden_Gb = 0xA1;
		golden_B = 0x6D;
  		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_MIPI_read_otp_wb] golden_R=0x%x, golden_Gr=0x%x, golden_Gb=0x%x, golden_B=0x%x\n", golden_R, golden_Gr, golden_Gb, golden_B);
  		current_R = S5K3L2XX_read_cmos_sensor1(0x0012);
  		current_Gr = S5K3L2XX_read_cmos_sensor1(0x0013);
  		current_Gb = S5K3L2XX_read_cmos_sensor1(0x0014);
  		current_B = S5K3L2XX_read_cmos_sensor1(0x0015); 
  		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_MIPI_read_otp_wb] current_R=0x%x, current_Gr=0x%x, current_Gb=0x%x, current_B=0x%x\n", current_R, current_Gr, current_Gb, current_B);
   		golden_G = (golden_Gr + golden_Gb) / 2;
   		current_G = (current_Gr + current_Gb) / 2;

   		if(!golden_G || !current_G || !golden_R || !golden_B || !current_R || !current_B)
     		S5K3L2XXDB("WB update error!");
   
   		r_ratio = 512 * golden_R * current_G /( golden_G * current_R );
   		b_ratio = 512 * golden_B * current_G /( golden_G * current_B );

   		otp->r_ratio = r_ratio;
   		otp->b_ratio = b_ratio;
	
   		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_MIPI_read_otp_wb] r_ratio=0x%x, b_ratio=0x%x\n", otp->r_ratio, otp->b_ratio);      
   	}
}



void S5K3L2XX_MIPI_algorithm_otp_wb1(struct S5K3L2XX_MIPI_otp_struct *otp)
{
   kal_uint16 R_GAIN, B_GAIN, Gr_GAIN, Gb_GAIN, G_GAIN, r_ratio, b_ratio;
   
   r_ratio = otp->r_ratio;
   b_ratio = otp->b_ratio;

	if(r_ratio >= 512 )
   {
    	if(b_ratio >= 512) 
        {
        	R_GAIN = GAIN_DEFAULT * r_ratio / 512;
            G_GAIN = GAIN_DEFAULT;     
            B_GAIN = GAIN_DEFAULT * b_ratio / 512;
   		}
        else
        {
           	R_GAIN = GAIN_DEFAULT * 512 / b_ratio  * r_ratio / 512;
           	G_GAIN = GAIN_DEFAULT * 512 / b_ratio;
           	B_GAIN = GAIN_DEFAULT;    
        }
   }
	else                      
   {
   		if(b_ratio >= 512)
    	{
      		R_GAIN = GAIN_DEFAULT;    
      	  	G_GAIN = GAIN_DEFAULT * 512/ r_ratio ;
           	B_GAIN = GAIN_DEFAULT * 512 / r_ratio * b_ratio / 512;
        } 
        else 
        {
           	Gr_GAIN = GAIN_DEFAULT * 512 / r_ratio;
           	Gb_GAIN = GAIN_DEFAULT * 512 / b_ratio;

			if(Gr_GAIN >= Gb_GAIN)
            {
            	R_GAIN = GAIN_DEFAULT;
              	G_GAIN = GAIN_DEFAULT * 512 / r_ratio;
                B_GAIN = GAIN_DEFAULT * 512 / r_ratio * b_ratio / 512;
        	} 
        	else
            {
       			R_GAIN = GAIN_DEFAULT * 512 / b_ratio * r_ratio / 512;
              	G_GAIN = GAIN_DEFAULT * 512 / b_ratio;
              	B_GAIN = GAIN_DEFAULT;
            }
   		}        
	}

   	otp->R_Gain = R_GAIN;
   	otp->B_Gain = B_GAIN;
   	otp->G_Gain = G_GAIN;

   S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_MIPI_algorithm_otp_wb1] R_gain=0x%x, B_gain=0x%x, G_gain=0x%x\n", otp->R_Gain, otp->B_Gain, otp->G_Gain);    
}



void S5K3L2XX_MIPI_write_otp_wb(struct S5K3L2XX_MIPI_otp_struct *otp)
{
   kal_uint16 R_GAIN, B_GAIN, G_GAIN;

   R_GAIN = otp->R_Gain;
   B_GAIN = otp->B_Gain;
   G_GAIN = otp->G_Gain;

   S5K3L2XX_write_cmos_sensor1(0x020e, G_GAIN);
   S5K3L2XX_write_cmos_sensor1(0x0210, R_GAIN);
   S5K3L2XX_write_cmos_sensor1(0x0212, B_GAIN);
   S5K3L2XX_write_cmos_sensor1(0x0214, G_GAIN);
}



void S5K3L2XX_MIPI_update_wb_register_from_otp(void)
{
   struct S5K3L2XX_MIPI_otp_struct current_otp;
   
   S5K3L2XX_MIPI_read_otp_wb(&current_otp);
   S5K3L2XX_MIPI_algorithm_otp_wb1(&current_otp);
   S5K3L2XX_MIPI_write_otp_wb(&current_otp);
}
#endif



kal_uint16 S5K3L2XX_get_framelength(void)
{
	kal_uint16 Framelength = s5k3l2xx_preview_frame_length;

	if(s5k3l2xx.sensorMode == SENSOR_MODE_PREVIEW)
		Framelength = s5k3l2xx_preview_frame_length;
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_CAPTURE)
		Framelength = s5k3l2xx_capture_frame_length;
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_VIDEO)
		Framelength = s5k3l2xx_capture_frame_length;
	
	return Framelength;
}



kal_uint16 S5K3L2XX_get_linelength(void)
{
	kal_uint16 Linelength = s5k3l2xx_preview_line_length;

	if(s5k3l2xx.sensorMode == SENSOR_MODE_PREVIEW)
		Linelength = s5k3l2xx_preview_line_length;
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_CAPTURE)
		Linelength = s5k3l2xx_capture_line_length;
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_VIDEO)
		Linelength = s5k3l2xx_video_line_length;
	
	return Linelength;
}



kal_uint32 S5K3L2XX_get_pixelclock(void)
{
	kal_uint32 PixelClock = s5k3l2xx_preview_pixelclock;

	if(s5k3l2xx.sensorMode == SENSOR_MODE_PREVIEW)
		PixelClock = s5k3l2xx_preview_pixelclock;
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_CAPTURE)
		PixelClock = s5k3l2xx_capture_pixelclock;
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_VIDEO)
		PixelClock = s5k3l2xx_video_pixelclock;
	
	return PixelClock;
}



void S5K3L2XX_write_shutter(kal_uint32 shutter)
{
	kal_uint16 frame_length = 0, line_length = 0, framerate = 0 , frame_length_min = 0, read_register0005_value  = 0;
	kal_uint32 pixelclock = 0;
	unsigned long flags;

	#define SHUTTER_FRAMELENGTH_MARGIN 16
	
	if (shutter < 3)
		shutter = 3;

	frame_length = shutter + SHUTTER_FRAMELENGTH_MARGIN; 
	frame_length_min = S5K3L2XX_get_framelength();
	
	if(s5k3l2xx.sensorMode == SENSOR_MODE_VIDEO)
		frame_length_min = S5K3L2XX_VIDEO_FRAMELENGTH;
	
	if(frame_length < frame_length_min)
		frame_length = frame_length_min;
	
	if(s5k3l2xx.S5K3L2XXAutoFlickerMode == KAL_TRUE)
	{
		line_length = S5K3L2XX_get_linelength();
		pixelclock = S5K3L2XX_get_pixelclock();
		framerate = (10 * pixelclock) / (frame_length * line_length);
		  
		if(framerate > 290)
		{
		  	framerate = 290;
		  	frame_length = (10 * pixelclock) / (framerate * line_length);
		}
		else if(framerate > 147 && framerate < 152)
		{
		  	framerate = 147;
			frame_length = (10 * pixelclock) / (framerate * line_length);
		}
	}

	spin_lock_irqsave(&s5k3l2xxmipiraw_drv_lock,flags);
	s5k3l2xx.maxExposureLines= frame_length;
	s5k3l2xx.shutter= shutter;
	spin_unlock_irqrestore(&s5k3l2xxmipiraw_drv_lock,flags);	

	S5K3L2XX_write_cmos_sensor(0x0104, 0x01);  
	S5K3L2XX_write_cmos_sensor1(0x0340, frame_length);
 	S5K3L2XX_write_cmos_sensor1(0x0202, shutter);
 	S5K3L2XX_write_cmos_sensor(0x0104, 0x00);   
	
	read_register0005_value = S5K3L2XX_read_cmos_sensor(0x0005);
	S5K3L2XXDB("shutter=%d, frame_length=%d, frame_length_min=%d, framerate=%d, read_register0005_value=0x%x\n", shutter,frame_length, frame_length_min, framerate, read_register0005_value);
} 



void write_S5K3L2XX_gain(kal_uint16 gain)
{
	S5K3L2XXDB("[S5K3L2XX] [write_S5K3L2XX_gain] gain=%d\n", gain);
	
	gain = gain / 2;
	
	S5K3L2XX_write_cmos_sensor(0x0104, 0x01);	
	S5K3L2XX_write_cmos_sensor(0x0204,(gain>>8));
	S5K3L2XX_write_cmos_sensor(0x0205,(gain&0xff));
	S5K3L2XX_write_cmos_sensor(0x0104, 0x00);
}



void S5K3L2XX_SetGain(UINT16 iGain)
{
	unsigned long flags;

	spin_lock_irqsave(&s5k3l2xxmipiraw_drv_lock,flags);
	s5k3l2xx.realGain = iGain;
	spin_unlock_irqrestore(&s5k3l2xxmipiraw_drv_lock,flags);

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_SetGain] gain=%d\n", iGain);

	write_S5K3L2XX_gain(iGain);
} 



kal_uint16 read_S5K3L2XX_gain(void)
{
	kal_uint16 read_gain=0;

	read_gain=((S5K3L2XX_read_cmos_sensor(0x0204) << 8) | S5K3L2XX_read_cmos_sensor(0x0205));
	
	S5K3L2XXDB("[S5K3L2XX] [read_S5K3L2XX_gain] gain=%d\n", read_gain);

	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	s5k3l2xx.sensorGlobalGain = read_gain ;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);

	return s5k3l2xx.sensorGlobalGain;
}  



void S5K3L2XX_camera_para_to_sensor(void)
{
    kal_uint32 i;

	for(i=0; 0xFFFFFFFF!=S5K3L2XXSensorReg[i].Addr; i++)
        S5K3L2XX_write_cmos_sensor(S5K3L2XXSensorReg[i].Addr, S5K3L2XXSensorReg[i].Para);
    
	for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K3L2XXSensorReg[i].Addr; i++)
        S5K3L2XX_write_cmos_sensor(S5K3L2XXSensorReg[i].Addr, S5K3L2XXSensorReg[i].Para);

	for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
        S5K3L2XX_write_cmos_sensor(S5K3L2XXSensorCCT[i].Addr, S5K3L2XXSensorCCT[i].Para);
}



void S5K3L2XX_sensor_to_camera_para(void)
{
    kal_uint32 i, temp_data;

	for(i=0; 0xFFFFFFFF!=S5K3L2XXSensorReg[i].Addr; i++)
    {
         temp_data = S5K3L2XX_read_cmos_sensor(S5K3L2XXSensorReg[i].Addr);

		 spin_lock(&s5k3l2xxmipiraw_drv_lock);
		 S5K3L2XXSensorReg[i].Para =temp_data;
		 spin_unlock(&s5k3l2xxmipiraw_drv_lock);
    }

	for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K3L2XXSensorReg[i].Addr; i++)
    {
        temp_data = S5K3L2XX_read_cmos_sensor(S5K3L2XXSensorReg[i].Addr);

		spin_lock(&s5k3l2xxmipiraw_drv_lock);
		S5K3L2XXSensorReg[i].Para = temp_data;
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);
    }
}



kal_int32  S5K3L2XX_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}



void S5K3L2XX_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
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
			break;
	}
}



void S5K3L2XX_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
    kal_int16 temp_reg=0, temp_gain=0, temp_addr=0, temp_para=0;

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
			  	break;
          }

       		temp_para= S5K3L2XXSensorCCT[temp_addr].Para;
            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= s5k3l2xx_min_analog_gain * 1000;
            info_ptr->Max= s5k3l2xx_max_analog_gain * 1000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");

                    temp_reg = ISP_DRIVING_2MA;
                    if(temp_reg==ISP_DRIVING_2MA)
                        info_ptr->ItemValue=2;
                    else if(temp_reg==ISP_DRIVING_4MA)
                        info_ptr->ItemValue=4;
                    else if(temp_reg==ISP_DRIVING_6MA)
                        info_ptr->ItemValue=6;
                    else if(temp_reg==ISP_DRIVING_8MA)
                        info_ptr->ItemValue=8;

                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_TRUE;
                    info_ptr->Min=2;
                    info_ptr->Max=8;
                    break;
                default:
					break;
            }
            break;
        case FRAME_RATE_LIMITATION:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Max Exposure Lines");
                    info_ptr->ItemValue=    111; 
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
					break;
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
					break;
            }
            break;
        default:
			break;
    }
}



kal_bool S5K3L2XX_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
   kal_uint16  temp_gain=0, temp_addr=0, temp_para=0;

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
              	break;
          }

		  temp_gain=((ItemValue*BASEGAIN+500)/1000);			

		  spin_lock(&s5k3l2xxmipiraw_drv_lock);
          S5K3L2XXSensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&s5k3l2xxmipiraw_drv_lock);
		  
          S5K3L2XX_write_cmos_sensor(S5K3L2XXSensorCCT[temp_addr].Addr,temp_para);
        	break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    break;
                default:
					break;
            }
            break;
        case FRAME_RATE_LIMITATION:
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
					spin_lock(&s5k3l2xxmipiraw_drv_lock);
                    S5K3L2XX_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&s5k3l2xxmipiraw_drv_lock);
                    break;
                case 1:
                    S5K3L2XX_write_cmos_sensor(S5K3L2XX_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
					break;
            }
            break;
        default:
			break;
    }
    return KAL_TRUE;
}



static void S5K3L2XX_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint16 line_length = 0, frame_length = 0;

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_SetDummy] iPixels=%d, iLines=%d\n", iPixels, iLines);

	frame_length = S5K3L2XX_get_framelength() + iLines;
	line_length = S5K3L2XX_get_linelength() + iPixels;

	if(s5k3l2xx.maxExposureLines > frame_length - 16)
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_SetDummy] maxExposureLines > frame_length - 16\n");

	S5K3L2XX_write_cmos_sensor(0x0104, 0x01);	
	S5K3L2XX_write_cmos_sensor1(0x0340, frame_length);
	S5K3L2XX_write_cmos_sensor1(0x0342, line_length);
	S5K3L2XX_write_cmos_sensor(0x0104, 0x00);
	
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_SetDummy] frame_length=%d, line_length=%d\n", frame_length, line_length);
}



void S5K3L2XXInitSetting(void)
{
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXInitSetting]\n");

	S5K3L2XX_write_cmos_sensor1(0x6028,0xD000);//modify page d000/7000/0000
	S5K3L2XX_write_cmos_sensor1(0x602A,0x6010);//602a write 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); //6010 0001
	Sleep(5);				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x6214);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7970); 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x6218);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7150); 
	S5K3L2XX_write_cmos_sensor1(0x6028,0x7000);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x2200); 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2DE9);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x90E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2C20);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x42E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0110);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB410);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2501);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7C50);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7C10);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC5E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2201);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7410);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x85E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB240);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB440);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB640);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5410);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1201);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC5E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8020);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0A00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC2E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA540);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF3A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFAFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBDE8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2FE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1EFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB835);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x6018);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2826);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA436);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5027);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD025);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x08C5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7424);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF004);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xE023);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD022);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7CBC);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0035);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2DE9);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA043);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFA00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9403);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x522F);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8C03);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF700);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8853);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD5E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x001A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD5E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB2E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7803);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB420);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5EE3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0A00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA083);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0AE0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB430);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x52E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8301);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA093);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x009A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0C00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB630);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x52E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8301);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x002A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x85E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xDCE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA530);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x53E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x001A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0F00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0130);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0D00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB830);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x52E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8301);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x008A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0130);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x85E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0700);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBA30);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x52E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8301);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA023);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0230);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF2A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF9FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x85E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xDCE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA530);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x53E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF0A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xEFFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xCCE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA430);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x85E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD3E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA4C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC3E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA5C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8C30);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBC10);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD3E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBE30);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0D12);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0E00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x002A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0300);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x85E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0012);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBC30);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x53E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF1A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBDE8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0110);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBB00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2DE9);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3840);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9DE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8DE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBB00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9802);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB420);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x90E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x83E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8101);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x250E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB0C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5CE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00CA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1300);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB2C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5CE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF450);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF610);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x41E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0530);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB210);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x41E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x42E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9300);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB610);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2008);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBDE8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3840);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2FE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1EFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0110);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x51E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF3A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xE3FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFFEA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF9FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2DE9);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1052);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x95E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x020C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB012);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x51E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9F15);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0012);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD115);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0B10);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5113);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3E00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF411);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB210);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB222);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xEC41);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x51E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA023);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA033);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0110);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB410);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB402);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8A00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0060);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x95E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x020C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB602);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xAC11);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0120);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB811);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x51E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC491);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB220);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x009A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x51E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC481);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB230);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x95E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x91E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3002);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x001A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0B00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8D0F);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x90E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB1E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8401);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x84C1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5EE3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD001);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB0E6);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD001);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB206);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD011);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB4E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD011);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB605);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8EE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8C05);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2C00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8C15);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x91E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2CC2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5CE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC4D1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB020);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00DA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x91E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x28C2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5CE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC4C1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD411);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5013);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD411);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5013);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9F15);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3001);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9011);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC411);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4012);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC411);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9F15);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1801);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC011);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB030);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC011);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB820);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5700);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0401);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x54E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4480);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBD88);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA081);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA081);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2118);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA083);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x008A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBDE8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2FE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1EFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2DE9);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4F00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000B);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4F00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4E00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4F00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF0A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFAFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0020);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0810);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4B00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA410);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB020);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB421);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC1E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB801);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBDE8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2FE1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1EFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2DE9);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4850);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7840);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD5E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2300);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4210);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3B00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD4E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB612);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x80E0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5810);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x81E5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EB);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3600);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD5E1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xB000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xBDE8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7040);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0E3);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0110);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5027);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9018);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1018);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0035);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x901F);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4005);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00C2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5427);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0096);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x801E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x3602);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00A6);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xE018);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1662);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0062);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0070);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xD0DD);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x44DD);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x68DC);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7CBC);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA0B1);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x7CE4);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5055);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x9090);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x50B8);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5C9D);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1402);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x187F);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00C0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x98C4);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2C4B);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xDC0C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFCE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FE5);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x04F0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xA436);
	S5K3L2XX_write_cmos_sensor1(0x6028,0xD000);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3690);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3692);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0060);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x369A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF446);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5176); 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5C76);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5C76);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x36AE);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF40E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0B00); 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4B00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4B00);
	S5K3L2XX_write_cmos_sensor1(0x602A,0xF412);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3664);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0BB8); 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x35E4);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4776);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3675);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x76);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3672);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0520); 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x367A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00ED); 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x372A);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);	
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3246);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0092);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x324A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x009C);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3256);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01BD);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x325A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01C7);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3248);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0092);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x324C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x009C);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3258);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01B2);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x325C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01BC);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3792);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); //D0003792						// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); //D0003794						// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); //D0003796
	S5K3L2XX_write_cmos_sensor1(0x602A,0x379A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); //D000379A			// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); //D000379C			// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); //D000379E			// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x602A,0x33DA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BC); //D00033DA		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BC); //D00033DC		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BE); //D00033DE		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BE); //D00033E0		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x602A,0x33EA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BC); //D00033EA		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00BC); //D00033EC		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D8); //D00033EE		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D8); //D00033F0		// 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x602A,0x36A2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x5176); //2   D00036A2 // 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2F76); //2   D00036A4 // 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2F76); //2   D00036A6 // 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x602A,0x36B6);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0B00); //2   D00036B6 // 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4B00); //2   D00036B8 // 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4B00); //2   D00036BA // 20130419 Modified
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3690);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);	// Full 01, WVGA 02
	S5K3L2XX_write_cmos_sensor1(0x602A,0x372C);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x48);	// Full 48, WVGA 4C
	S5K3L2XX_write_cmos_sensor1(0x602A,0x372D);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x48);	// Full 48, WVGA 4C
	S5K3L2XX_write_cmos_sensor1(0x602A,0x372E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); // Full 0002, WVGA 0000
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1106); // Full 1106, WVGA 1104
	S5K3L2XX_write_cmos_sensor1(0x6028,0x7000);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0300);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x350C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x080B);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3504);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4700); // AGain 2.24 => 71 = 2.24 * 32 => 0x47
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); // AGain 8 => 256 = 8 * 32 => 0x100
	S5K3L2XX_write_cmos_sensor1(0x602A,0x350E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x351C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xE63B);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3514);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4700); // AGain 2.24 => 71 = 2.24 * 32 => 0x47
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); // AGain 8 => 256 = 8 * 32 => 0x100
	S5K3L2XX_write_cmos_sensor1(0x602A,0x351E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x352C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFA3B);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3524);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4500);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4700); // AGain 2.24 => 71 = 2.24 * 32 => 0x47
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xF800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); // AGain 8 => 256 = 8 * 32 => 0x100
	S5K3L2XX_write_cmos_sensor1(0x602A,0x352E);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6028,0xD000);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0C00);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0438);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x03FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0415);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x045B);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0400);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x07FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0421);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x043B);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0800);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0FFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0423);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x043B);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1000);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1FFF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0426);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0426);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B80);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B86);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0020);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B84);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0020);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B82);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B8C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B88);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B96);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0005);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B94);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0602);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x03FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x03FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x03FF);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x03FF);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3BE0);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x03DA); //D0003BE0						3BE0		03DA
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01D6); //D0003BE2			  3BE2	  01D6
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x01BF); //D0003BE4			  3BE4	  01BF
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); //D0003BE6			  3BE6	  0001
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003BE8			  3BE8	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003BEA			  3BEA	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003BEC			  3BEC	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003BEE			  3BEE	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003BF0			  3BF0	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003BF2			  3BF2	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003BF4			  3BF4	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003BF6			  3BF6	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003BF8			  3BF8	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); //D0003BFA			  3BFA	  0001
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003BFC			  3BFC	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003BFE			  3BFE	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003C00			  3C00	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003C02			  3C02	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003C04			  3C04	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003C06			  3C06	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003C08			  3C08	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x000A); //D0003C0A			  3C0A	  000A
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0022); //D0003C0C			  3C0C	  0022
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C0E			  3C0E	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C10			  3C10	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C12			  3C12	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C14			  3C14	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C16			  3C16	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C18			  3C18	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C1A			  3C1A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C1C			  3C1C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C1E			  3C1E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C20			  3C20	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C22			  3C22	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C24			  3C24	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C26			  3C26	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C28			  3C28	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C2A			  3C2A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C2C			  3C2C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C2E			  3C2E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C30			  3C30	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C32			  3C32	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C34			  3C34	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C36			  3C36	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C38			  3C38	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C3A			  3C3A	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C3C			  3C3C	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C3E			  3C3E	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C40			  3C40	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C42			  3C42	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C44			  3C44	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C46			  3C46	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C48			  3C48	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C4A			  3C4A	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C4C			  3C4C	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C4E			  3C4E	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C50			  3C50	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C52			  3C52	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C54			  3C54	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C56			  3C56	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C58			  3C58	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C5A			  3C5A	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C5C			  3C5C	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C5E			  3C5E	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C60			  3C60	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C62			  3C62	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C64			  3C64	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C66			  3C66	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C68			  3C68	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C6A			  3C6A	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C6C			  3C6C	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C6E			  3C6E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C70			  3C70	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C72			  3C72	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C74			  3C74	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C76			  3C76	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C78			  3C78	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C7A			  3C7A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C7C			  3C7C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C7E			  3C7E	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C80			  3C80	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C82			  3C82	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C84			  3C84	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C86			  3C86	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C88			  3C88	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C8A			  3C8A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C8C			  3C8C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003C8E			  3C8E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003C90			  3C90	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C92			  3C92	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003C94			  3C94	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003C96			  3C96	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003C98			  3C98	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C9A			  3C9A	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003C9C			  3C9C	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003C9E			  3C9E	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CA0			  3CA0	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CA2			  3CA2	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CA4			  3CA4	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CA6			  3CA6	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CA8			  3CA8	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003CAA			  3CAA	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003CAC			  3CAC	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CAE			  3CAE	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CB0			  3CB0	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CB2			  3CB2	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CB4			  3CB4	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CB6			  3CB6	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CB8			  3CB8	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CBA			  3CBA	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CBC			  3CBC	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003CBE			  3CBE	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003CC0			  3CC0	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003CC2			  3CC2	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003CC4			  3CC4	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CC6			  3CC6	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CC8			  3CC8	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CCA			  3CCA	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CCC			  3CCC	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CCE			  3CCE	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CD0			  3CD0	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CD2			  3CD2	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CD4			  3CD4	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003CD6			  3CD6	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003CD8			  3CD8	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CDA			  3CDA	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CDC			  3CDC	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CDE			  3CDE	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CE0			  3CE0	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CE2			  3CE2	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CE4			  3CE4	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003CE6			  3CE6	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003CE8			  3CE8	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003CEA			  3CEA	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003CEC			  3CEC	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003CEE			  3CEE	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003CF0			  3CF0	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CF2			  3CF2	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CF4			  3CF4	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CF6			  3CF6	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CF8			  3CF8	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CFA			  3CFA	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003CFC			  3CFC	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003CFE			  3CFE	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D00			  3D00	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D02			  3D02	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D04			  3D04	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D06			  3D06	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D08			  3D08	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D0A			  3D0A	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D0C			  3D0C	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D0E			  3D0E	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D10			  3D10	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D12			  3D12	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D14			  3D14	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D16			  3D16	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D18			  3D18	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D1A			  3D1A	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D1C			  3D1C	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D1E			  3D1E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D20			  3D20	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D22			  3D22	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D24			  3D24	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D26			  3D26	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D28			  3D28	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D2A			  3D2A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D2C			  3D2C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D2E			  3D2E	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D30			  3D30	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D32			  3D32	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D34			  3D34	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D36			  3D36	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D38			  3D38	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D3A			  3D3A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D3C			  3D3C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D3E			  3D3E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D40			  3D40	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D42			  3D42	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D44			  3D44	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D46			  3D46	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D48			  3D48	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D4A			  3D4A	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D4C			  3D4C	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D4E			  3D4E	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D50			  3D50	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D52			  3D52	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D54			  3D54	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D56			  3D56	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D58			  3D58	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D5A			  3D5A	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D5C			  3D5C	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D5E			  3D5E	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D60			  3D60	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D62			  3D62	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D64			  3D64	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D66			  3D66	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D68			  3D68	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0014); //D0003D6A			  3D6A	  0014
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0040); //D0003D6C			  3D6C	  0040
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D6E			  3D6E	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D70			  3D70	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D72			  3D72	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D74			  3D74	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D76			  3D76	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D78			  3D78	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D7A			  3D7A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D7C			  3D7C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D7E			  3D7E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D80			  3D80	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D82			  3D82	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D84			  3D84	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); //D0003D86			  3D86	  0002
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003); //D0003D88			  3D88	  0003
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D8A			  3D8A	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D8C			  3D8C	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D8E			  3D8E	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D90			  3D90	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D92			  3D92	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D94			  3D94	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008); //D0003D96			  3D96	  0008
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0018); //D0003D98			  3D98	  0018
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D9A			  3D9A	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0200); //D0003D9C			  3D9C	  0200
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0F00); //D0003D9E			  3D9E	  0F00
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x4B00); //D0003DA0			  3DA0	  4B00
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x8700); //D0003DA2			  3DA2	  8700
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xC300); //D0003DA4			  3DA4	  C300
	S5K3L2XX_write_cmos_sensor1(0x6F12,0xFF00); //D0003DA6			  3DA6	  FF00
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0032); //D0003DA8			  3DA8	  0032
	S5K3L2XX_write_cmos_sensor(0x6F12,0x42);	//D0003DAA			  3DAA	  42FF
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DAB);
	S5K3L2XX_write_cmos_sensor(0x6F12,0xFF);	//D0003DAB
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DAC);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x08);	//D0003DAC			  3DAC	  0800
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DAE);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001); //D0003DAE			  3DAE	  0001
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);	//D0003DB0			  3DB0	  0008
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DB1);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x08);	//D0003DB1
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DB2);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00FF); //D0003DB2			  3DB2	  00FF
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);	//D0003DB4			  3DB4	  0008
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DB5);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x08);	//D0003DB5
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DB6);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0100); //D0003DB6			  3DB6	  0100
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);	//D0003DB8			  3DB8	  0100
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3DBA);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0F00); //D0003DBA			  3DBA	  0F00
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0136);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1800);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0304);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0006);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00D8);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0302);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0300);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x030C);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0006);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x00A2);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x030A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0308);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0008);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x35D9);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);	// Full size 00h, FHD : 00h, HD 03h
	S5K3L2XX_write_cmos_sensor1(0x602A,0x303A);
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x02BC);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B05);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0B08);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x380E);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3027);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x10);

	mdelay(30);
}




void S5K3L2XXPreviewSetting(void)
{
   	kal_uint16 read_count = 0, read_register0005_value = 0;

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXPreviewSetting]\n");
	
 	S5K3L2XX_write_cmos_sensor1(0x602A,0x0100);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);
	Sleep(200);
	for(read_count = 0; read_count <= 4; read_count++)
	{
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXPreviewSetting]read_count= %d\n",read_count);
		read_register0005_value = S5K3L2XX_read_cmos_sensor(0x0005);
		if(read_register0005_value == 0xff)
			break;
		if(read_count == 4)
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXPreviewSetting][sensor_issure] [0x0005, 0xff]\n");
		Sleep(50);
	}
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3BCE);		//gisp_hvbin_Weights_2__0_												   
	S5K3L2XX_write_cmos_sensor(0x6F12,0x30);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3BCF);   //gisp_hvbin_Weights_2__1_												   
	S5K3L2XX_write_cmos_sensor(0x6F12,0x10);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x304F);	//line start/end short packet											  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0342);	  //line length 														  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1e80);			//1198 //1e80																				
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0340);	//frame length lines													  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0734);			//0C7C	//1872 -->1800 b																			
	S5K3L2XX_write_cmos_sensor1(0x602A,0x034C);	//#smiaRegs_rw_frame_timing_x_output_size	1070						   
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0838);																			  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0618);	//#smiaRegs_rw_frame_timing_y_output_size	0C30						   
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0114);																			  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x03);		//#smiaRegs_rw_output_lane_mode 03										 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0344);	//#smiaRegs_rw_frame_timing_x_addr_start	0004							 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0004);	//#smiaRegs_rw_frame_timing_y_addr_start	0004							 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0004);																			  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1073);	//#smiaRegs_rw_frame_timing_x_addr_end	1073							   
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0C35);	//#smiaRegs_rw_frame_timing_y_addr_end									   
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3010);	//#smiaRegs_vendor_sensor_offset_x 0000 								  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);																			  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);	//#smiaRegs_vendor_sensor_offset_y 0001 								  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0386);	//#smiaRegs_rw_sub_sample_y_odd_inc 0001								 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003);																			  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0382);	//#smiaRegs_rw_sub_sample_x_odd_inc 0001								 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0003);																			  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0900);	//#smiaRegs_rw_binning_mode 00											 
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0901);	//#smiaRegs_rw_binning_type 11											 
	S5K3L2XX_write_cmos_sensor(0x6F12,0x22);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0400);	//#smiaRegs_rw_scaling_scaling_mode 0000								 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);																			  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0404);	//#smiaRegs_rw_scaling_scale_m 0010 									  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);																			  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0111);	//#smiaRegs_rw_output_signalling_mode 02	//default					  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x02);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0112);	//#smiaRegs_rw_output_data_format 0A0A			//default					   
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0A0A);																			  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x302A);	//#smiaRegs_vendor_sensor_emb_use_header 00 //default					 
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);																				  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0500);	//#smiaRegs_rw_compression_compression_mode 0000						  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);																			  
	S5K3L2XX_write_cmos_sensor1(0x602A,0x30DE);	//#skl_bHaltOnWait 01 // disable CPU during readout 					  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);	// disable CPU during readout
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0100);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);

	for(read_count = 0; read_count <= 4; read_count++)
	{
		read_register0005_value = S5K3L2XX_read_cmos_sensor(0x0005);
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXPreviewSetting]read_count= %d, read_register0005_value = 0x%x\n",read_count, read_register0005_value);
		if(read_register0005_value != 0xff)
			break;
		Sleep(50);
		if(read_count == 4)
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXPreviewSetting][sensor_issure] [0x0005, 0xff]\n");
	}
	
}



void S5K3L2XXCaptureSetting(void)
{
	kal_uint16 read_count = 0, read_register0005_value = 0;
	
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCaptureSetting]\n");

	S5K3L2XX_write_cmos_sensor1(0x602A,0x0100);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);
	Sleep(200);
	
	for(read_count = 0; read_count <= 4; read_count++)
	{
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCaptureSetting]read_count= %d\n",read_count);
		read_register0005_value = S5K3L2XX_read_cmos_sensor(0x0005);
		if(read_register0005_value == 0xff)
			break;
		if(read_count == 4)
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCaptureSetting][sensor_issure] [0x0005, 0xff]\n");
		Sleep(50);
	}
	
	S5K3L2XX_write_cmos_sensor1(0x602A,0x304F);   //line start/end short packet										 
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);																			 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0342);																		 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x2292);   //line length		//1e80			//2398							
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0340);																		 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0cb6);   //frame length lines 												 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x034C);																		 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1070);   //#smiaRegs_rw_frame_timing_x_output_size	1070	//default			  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0C30);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0114);   //#smiaRegs_rw_frame_timing_y_output_size	0C30  //default 			
	S5K3L2XX_write_cmos_sensor(0x6F12,0x03);																			 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0344);   //#smiaRegs_rw_output_lane_mode	03									  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0004);   //#smiaRegs_rw_frame_timing_x_addr_start 0004						
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0004);   //#smiaRegs_rw_frame_timing_y_addr_start 0004						
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x1073);   //#smiaRegs_rw_frame_timing_x_addr_end	1073						  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0C35);   //#smiaRegs_rw_frame_timing_y_addr_end								
	S5K3L2XX_write_cmos_sensor1(0x602A,0x3010);  //#smiaRegs_vendor_sensor_offset_x 0000								 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);																		 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);  //#smiaRegs_vendor_sensor_offset_y 0001								 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0386);  //#smiaRegs_rw_sub_sample_y_odd_inc	0001							  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0382);  //#smiaRegs_rw_sub_sample_x_odd_inc	0001							  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0001);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0900);  //#smiaRegs_rw_binning_mode	00										  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);																			 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0901);  //#smiaRegs_rw_binning_type	11										  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x11);																			 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0400);  //#smiaRegs_rw_scaling_scaling_mode	0000							  
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0404);  //#smiaRegs_rw_scaling_scale_m 0010									 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0010);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0111);  //#smiaRegs_rw_output_signalling_mode 02	  //default 				 
	S5K3L2XX_write_cmos_sensor(0x6F12,0x02);																			 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0112);  //#smiaRegs_rw_output_data_format 0A0A			//default					
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0A0A);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x302A);  //#smiaRegs_vendor_sensor_emb_use_header 00	//default				  
	S5K3L2XX_write_cmos_sensor(0x6F12,0x00);																			 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0500);  //#smiaRegs_rw_compression_compression_mode 0000						 
	S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000);																		 
	S5K3L2XX_write_cmos_sensor1(0x602A,0x30DE);  //#skl_bHaltOnWait 01 // disable CPU during readout					 
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);	// disable CPU during readout
	S5K3L2XX_write_cmos_sensor1(0x602A,0x0100);
	S5K3L2XX_write_cmos_sensor(0x6F12,0x01);
	
	for(read_count = 0; read_count <= 4; read_count++)
	{
		read_register0005_value = S5K3L2XX_read_cmos_sensor(0x0005);
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCaptureSetting]read_count= %d, read_register0005_value = 0x%x\n",read_count, read_register0005_value);
		if(read_register0005_value != 0xff)
			break;
		Sleep(50);
		if(read_count == 4)
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCaptureSetting][sensor_issure] [0x0005, 0xff]\n");
	}
}



UINT32 S5K3L2XXOpen(void)
{
	volatile signed int i;
	kal_uint16 sensor_id = 0;

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXOpen]\n");

	for(i=0;i<3;i++)
	{
		sensor_id = (S5K3L2XX_read_cmos_sensor(0x0000)<<8)|S5K3L2XX_read_cmos_sensor(0x0001);

		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXOpen] sensor_id=%x\n",sensor_id);

		if(sensor_id != S5K3L2XX_SENSOR_ID)
			return ERROR_SENSOR_CONNECT_FAIL;
		else
			break;
	}
	
	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	s5k3l2xx.sensorMode = SENSOR_MODE_INIT;
	s5k3l2xx.S5K3L2XXAutoFlickerMode = KAL_FALSE;
	s5k3l2xx.S5K3L2XXVideoMode = KAL_FALSE;
	s5k3l2xx.DummyLines= 0;
	s5k3l2xx.DummyPixels= 0;
	s5k3l2xx.shutter = 0x4EA;
	s5k3l2xx.pvShutter = 0x4EA;
	s5k3l2xx.maxExposureLines =1232 -4;
	s5k3l2xx.ispBaseGain = BASEGAIN;
	s5k3l2xx.sensorGlobalGain = 0x1f;
	s5k3l2xx.pvGain = 0x1f;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);

	S5K3L2XXInitSetting();
//	S5K3L2XX_MIPI_update_wb_register_from_otp();
    return ERROR_NONE;
}



UINT32 S5K3L2XXGetSensorID(UINT32 *sensorID)
{
    int  retry = 1;

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXGetSensorID]\n");
	
    do {
        *sensorID = (S5K3L2XX_read_cmos_sensor(0x0000)<<8)|S5K3L2XX_read_cmos_sensor(0x0001);
		
        if (*sensorID == S5K3L2XX_SENSOR_ID)
        {
        	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXGetSensorID] Sensor ID = 0x%04x\n", *sensorID);

			break;
        }
        S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXGetSensorID] Read Sensor ID Fail = 0x%04x\n", *sensorID);

		retry--;
    } while (retry > 0);

    if (*sensorID != S5K3L2XX_SENSOR_ID) 
	{
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	
    return ERROR_NONE;
}



void S5K3L2XX_SetShutter(kal_uint32 iShutter)
{
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_SetShutter] shutter = %d\n", iShutter);
	
	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	s5k3l2xx.shutter= iShutter;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);

	S5K3L2XX_write_shutter(iShutter);
} 



UINT32 S5K3L2XX_read_shutter(void)
{
	UINT32 shutter =0;
	
	shutter = (S5K3L2XX_read_cmos_sensor(0x0202) << 8) | S5K3L2XX_read_cmos_sensor(0x0203);

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XX_read_shutter] shutter = %d\n", shutter);

	return shutter;
}



UINT32 S5K3L2XXClose(void)
{
    return ERROR_NONE;
}



void S5K3L2XXSetFlipMirror(kal_int32 imgMirror)
{
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetFlipMirror] imgMirror = %d\n", imgMirror);

	switch (imgMirror)
    {
        case IMAGE_NORMAL: 
            S5K3L2XX_write_cmos_sensor(0x0101, 0x03);
            break;
        case IMAGE_V_MIRROR: 
            S5K3L2XX_write_cmos_sensor(0x0101, 0x01);	
            break;
        case IMAGE_H_MIRROR: 
            S5K3L2XX_write_cmos_sensor(0x0101, 0x02);	
            break;
        case IMAGE_HV_MIRROR: 
            S5K3L2XX_write_cmos_sensor(0x0101, 0x00);	
            break;
    }
}



UINT32 S5K3L2XXPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXPreview]\n");

	if(s5k3l2xx.sensorMode |= SENSOR_MODE_PREVIEW)
		{
		//S5K3L2XXInitSetting();
		S5K3L2XXPreviewSetting();
	}
	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	s5k3l2xx.sensorMode = SENSOR_MODE_PREVIEW; 
	s5k3l2xx.DummyPixels = 0;
	s5k3l2xx.DummyLines = 0 ;
	S5K3L2XX_FeatureControl_PERIOD_PixelNum = s5k3l2xx_preview_line_length;
	S5K3L2XX_FeatureControl_PERIOD_LineNum = s5k3l2xx_preview_frame_length;
	s5k3l2xx.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);
	
	S5K3L2XXSetFlipMirror(IMAGE_HV_MIRROR);

	return ERROR_NONE;
}	



UINT32 S5K3L2XXVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXVideo]\n");

	if(s5k3l2xx.sensorMode != SENSOR_MODE_VIDEO || s5k3l2xx.sensorMode != SENSOR_MODE_PREVIEW)
	{
		//S5K3L2XXInitSetting();
		S5K3L2XXPreviewSetting();
	}
	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	s5k3l2xx.sensorMode = SENSOR_MODE_VIDEO;
	S5K3L2XX_FeatureControl_PERIOD_PixelNum = s5k3l2xx_video_line_length;
	S5K3L2XX_FeatureControl_PERIOD_LineNum = s5k3l2xx_video_frame_length;
	s5k3l2xx.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);

	S5K3L2XXSetFlipMirror(IMAGE_HV_MIRROR);

    return ERROR_NONE;
}



UINT32 S5K3L2XXCapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = s5k3l2xx.shutter, temp_data;

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCapture]\n");

	if( SENSOR_MODE_CAPTURE == s5k3l2xx.sensorMode)
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCapture] BusrtShot!!\n");
	else
	{
		shutter=S5K3L2XX_read_shutter();
		temp_data =  read_S5K3L2XX_gain();

		spin_lock(&s5k3l2xxmipiraw_drv_lock);
		s5k3l2xx.pvShutter =shutter;
		s5k3l2xx.sensorGlobalGain = temp_data;
		s5k3l2xx.pvGain =s5k3l2xx.sensorGlobalGain;
		s5k3l2xx.sensorMode = SENSOR_MODE_CAPTURE;	
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);

		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCapture] s5k3l2xx.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",s5k3l2xx.shutter, shutter,s5k3l2xx.sensorGlobalGain);
		//S5K3L2XXInitSetting();
		S5K3L2XXCaptureSetting();

		//S5K3L2XX_SetDummy(s5k3l2xx.DummyPixels,s5k3l2xx.DummyLines);

		spin_lock(&s5k3l2xxmipiraw_drv_lock);
		s5k3l2xx.imgMirror = sensor_config_data->SensorImageMirror;
		s5k3l2xx.DummyPixels = 0;
		s5k3l2xx.DummyLines = 0 ;
		S5K3L2XX_FeatureControl_PERIOD_PixelNum = s5k3l2xx_capture_line_length;
		S5K3L2XX_FeatureControl_PERIOD_LineNum = s5k3l2xx_capture_frame_length;
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);

		S5K3L2XXSetFlipMirror(IMAGE_HV_MIRROR);

    	if(S5K3L2XXCurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
    	{
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXCapture] S5K3L2XXCapture exit ZSD!\n");

			return ERROR_NONE;
    	}
	}

    return ERROR_NONE;
}	



UINT32 S5K3L2XXGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXGetResolution]\n");

	pSensorResolution->SensorPreviewWidth	= s5k3l2xx_preview_width;
    pSensorResolution->SensorPreviewHeight	= s5k3l2xx_preview_height;
	pSensorResolution->SensorFullWidth		= s5k3l2xx_capture_width;
    pSensorResolution->SensorFullHeight		= s5k3l2xx_capture_height;
    pSensorResolution->SensorVideoWidth		= s5k3l2xx_preview_width; //s5k3l2xx_video_width;
    pSensorResolution->SensorVideoHeight    = s5k3l2xx_preview_height;//s5k3l2xx_video_height;

	return ERROR_NONE;
} 



UINT32 S5K3L2XXGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXGetInfo]\n");

	pSensorInfo->SensorPreviewResolutionX= s5k3l2xx_preview_width;
	pSensorInfo->SensorPreviewResolutionY= s5k3l2xx_preview_height;
	pSensorInfo->SensorFullResolutionX= s5k3l2xx_capture_width;
    pSensorInfo->SensorFullResolutionY= s5k3l2xx_capture_height;
	
	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	s5k3l2xx.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_B;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
    pSensorInfo->CaptureDelayFrame = 3;
    pSensorInfo->PreviewDelayFrame = 3;
    pSensorInfo->VideoDelayFrame = 2;
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;
    pSensorInfo->AESensorGainDelayFrame = 0;
    pSensorInfo->AEISPGainDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=s5k3l2xx_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorGrabStartX = s5k3l2xx_preview_startx;
            pSensorInfo->SensorGrabStartY = s5k3l2xx_preview_starty;
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=s5k3l2xx_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorGrabStartX = s5k3l2xx_video_startx;
            pSensorInfo->SensorGrabStartY = s5k3l2xx_video_starty;
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=s5k3l2xx_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorGrabStartX = s5k3l2xx_capture_startx;	
            pSensorInfo->SensorGrabStartY = s5k3l2xx_capture_starty;	
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=s5k3l2xx_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorGrabStartX = s5k3l2xx_preview_startx;
            pSensorInfo->SensorGrabStartY = s5k3l2xx_preview_starty;
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }

    memcpy(pSensorConfigData, &S5K3L2XXSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
} 



UINT32 S5K3L2XXControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	spin_lock(&s5k3l2xxmipiraw_drv_lock);
	S5K3L2XXCurrentScenarioId = ScenarioId;
	spin_unlock(&s5k3l2xxmipiraw_drv_lock);

	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXControl]\n");
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXControl] ScenarioId=%d\n",S5K3L2XXCurrentScenarioId);

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            S5K3L2XXPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			S5K3L2XXVideo(pImageWindow, pSensorConfigData);
			break;   
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            S5K3L2XXCapture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
	
    return ERROR_NONE;
}



UINT32 S5K3L2XXSetVideoMode(UINT16 u2FrameRate)
{
    kal_uint32 MIN_Frame_length =0, frameRate=0, extralines=0;
	
    S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] frame rate = %d\n", u2FrameRate);
	
	if(u2FrameRate==0)
	{
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] error frame rate seting\n");

    if(s5k3l2xx.sensorMode == SENSOR_MODE_VIDEO)
    {
    	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] SENSOR_MODE_VIDEO\n");

		if(s5k3l2xx.S5K3L2XXAutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 302;
			else if(u2FrameRate==15)
				frameRate= 148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (s5k3l2xx_video_pixelclock)/(s5k3l2xx_video_line_length+ s5k3l2xx.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (s5k3l2xx_video_pixelclock) /(s5k3l2xx_video_line_length + s5k3l2xx.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <= s5k3l2xx_video_frame_length))
			MIN_Frame_length = s5k3l2xx_video_frame_length;
		
		extralines = MIN_Frame_length - s5k3l2xx_video_frame_length;

		spin_lock(&s5k3l2xxmipiraw_drv_lock);
		s5k3l2xx.DummyPixels = 0;
		s5k3l2xx.DummyLines = extralines ;
		S5K3L2XX_VIDEO_FRAMELENGTH = MIN_Frame_length;
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);

		S5K3L2XX_SetDummy(s5k3l2xx.DummyPixels,extralines);
    }
	else if(s5k3l2xx.sensorMode == SENSOR_MODE_CAPTURE)
	{
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] SENSOR_MODE_CAPTURE\n");

		if(s5k3l2xx.S5K3L2XXAutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==15)
			    frameRate= 148;
			else
				frameRate=u2FrameRate*10;
			
			MIN_Frame_length = s5k3l2xx_capture_pixelclock /(s5k3l2xx_capture_line_length + s5k3l2xx.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = s5k3l2xx_capture_pixelclock /(s5k3l2xx_capture_line_length + s5k3l2xx.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=s5k3l2xx_capture_frame_length))
		{
			MIN_Frame_length = s5k3l2xx_capture_frame_length;
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] current fps = %d\n", (s5k3l2xx_capture_pixelclock)/(s5k3l2xx_capture_line_length)/s5k3l2xx_capture_frame_length);

		}

		extralines = MIN_Frame_length - s5k3l2xx_capture_frame_length;

		spin_lock(&s5k3l2xxmipiraw_drv_lock);
		s5k3l2xx.DummyPixels = 0;
		s5k3l2xx.DummyLines = extralines ;
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);

		S5K3L2XX_SetDummy(s5k3l2xx.DummyPixels,extralines);
	}
	
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetVideoMode] MIN_Frame_length=%d,s5k3l2xx.DummyLines=%d\n",MIN_Frame_length,s5k3l2xx.DummyLines);

    return KAL_TRUE;
}



UINT32 S5K3L2XXSetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	if(bEnable) 
	{   
		S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetAutoFlickerMode] enable\n");

		spin_lock(&s5k3l2xxmipiraw_drv_lock);
		s5k3l2xx.S5K3L2XXAutoFlickerMode = KAL_TRUE;
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);
    } 
	else 
	{
    	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetAutoFlickerMode] disable\n");
		
    	spin_lock(&s5k3l2xxmipiraw_drv_lock);
        s5k3l2xx.S5K3L2XXAutoFlickerMode = KAL_FALSE;
		spin_unlock(&s5k3l2xxmipiraw_drv_lock);
    }

    return ERROR_NONE;
}



UINT32 S5K3L2XXSetTestPatternMode(kal_bool bEnable)
{
	S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXSetTestPatternMode] bEnable = %d\n", bEnable);
	if(bEnable)
	{
		S5K3L2XXDB("[S5K3L2XX][S5K3L2XXSetTestPatternMode] enter");
		S5K3L2XX_write_cmos_sensor1(0x6028,0xD000);
		S5K3L2XX_write_cmos_sensor1(0x602A,0x0600); 
		S5K3L2XX_write_cmos_sensor1(0x6F12,0x0002); 
	}
	else
	{
		S5K3L2XX_write_cmos_sensor1(0x6028,0xD000);
		S5K3L2XX_write_cmos_sensor1(0x602A,0x0600);
		S5K3L2XX_write_cmos_sensor1(0x6F12,0x0000); 
	}
	return TRUE;
}



UINT32 S5K3L2XXMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
	kal_int32 dummyLine, lineLength, frameHeight;

	if(frameRate == 0)
		return FALSE;
		
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXMIPISetMaxFramerateByScenario] MSDK_SCENARIO_ID_CAMERA_PREVIEW\n");
			lineLength = s5k3l2xx_preview_line_length;
			frameHeight = (10 * s5k3l2xx_preview_pixelclock) / frameRate / lineLength;
			dummyLine = frameHeight - s5k3l2xx_preview_frame_length;
			S5K3L2XX_SetDummy(0, dummyLine);	

			spin_lock(&s5k3l2xxmipiraw_drv_lock);
			s5k3l2xx.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&s5k3l2xxmipiraw_drv_lock);
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXMIPISetMaxFramerateByScenario] MSDK_SCENARIO_ID_VIDEO_PREVIEW\n");
			lineLength = s5k3l2xx_video_line_length;
			frameHeight = (10*s5k3l2xx_video_pixelclock) / frameRate / lineLength;
			dummyLine = frameHeight - s5k3l2xx_video_frame_length;
			S5K3L2XX_SetDummy(0, dummyLine);

			spin_lock(&s5k3l2xxmipiraw_drv_lock);
			s5k3l2xx.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&s5k3l2xxmipiraw_drv_lock);
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:	
			S5K3L2XXDB("[S5K3L2XX] [S5K3L2XXMIPISetMaxFramerateByScenario] MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG or MSDK_SCENARIO_ID_CAMERA_ZSD\n");
			lineLength = s5k3l2xx_capture_line_length;
			frameHeight =  (10*s5k3l2xx_capture_pixelclock) / frameRate / lineLength;
			dummyLine = frameHeight - s5k3l2xx_capture_frame_length;
			S5K3L2XX_SetDummy(0, dummyLine);

			spin_lock(&s5k3l2xxmipiraw_drv_lock);
			s5k3l2xx.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&s5k3l2xxmipiraw_drv_lock);
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:   
			break;		
		default:
			break;
	}	
	
	return ERROR_NONE;
}



UINT32 S5K3L2XXMIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{
	switch (scenarioId) 
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = s5k3l2xx_video_max_framerate;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = s5k3l2xx_capture_max_framerate;
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: 
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:    
			 *pframeRate = s5k3l2xx_preview_max_framerate;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 S5K3L2XXFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara, *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara, *pFeatureData32=(UINT32 *) pFeaturePara, SensorRegNumber, i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= s5k3l2xx_capture_width;
            *pFeatureReturnPara16= s5k3l2xx_capture_height;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++= S5K3L2XX_FeatureControl_PERIOD_PixelNum;
			*pFeatureReturnPara16= S5K3L2XX_FeatureControl_PERIOD_LineNum;
			*pFeatureParaLen=4;
			break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(S5K3L2XXCurrentScenarioId)
			{
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = s5k3l2xx_preview_pixelclock;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = s5k3l2xx_video_pixelclock;
					*pFeatureParaLen=4;
					break;	 
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = s5k3l2xx_capture_pixelclock;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = s5k3l2xx_preview_pixelclock;
					*pFeatureParaLen=4;
					break;
			}
		    break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            S5K3L2XX_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            S5K3L2XX_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            S5K3L2XX_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = S5K3L2XX_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;

			for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k3l2xxmipiraw_drv_lock);
                S5K3L2XXSensorCCT[i].Addr=*pFeatureData32++;
                S5K3L2XXSensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&s5k3l2xxmipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
			
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;

			*pFeatureData32++=SensorRegNumber;

			for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K3L2XXSensorCCT[i].Addr;
                *pFeatureData32++=S5K3L2XXSensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;

			for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k3l2xxmipiraw_drv_lock);
                S5K3L2XXSensorReg[i].Addr=*pFeatureData32++;
                S5K3L2XXSensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&s5k3l2xxmipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;

			if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;

			*pFeatureData32++=SensorRegNumber;

			for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K3L2XXSensorReg[i].Addr;
                *pFeatureData32++=S5K3L2XXSensorReg[i].Para;
            }

			break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=S5K3L2XX_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, S5K3L2XXSensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, S5K3L2XXSensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;

			*pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &S5K3L2XXSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            S5K3L2XX_camera_para_to_sensor();
            break;
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            S5K3L2XX_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=S5K3L2XX_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            S5K3L2XX_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            S5K3L2XX_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_SET_ITEM_INFO:
            S5K3L2XX_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
   			pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B; 
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            S5K3L2XXSetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            S5K3L2XXGetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            S5K3L2XXSetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            S5K3L2XXSetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			S5K3L2XXMIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			S5K3L2XXMIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing 			           
			*pFeatureReturnPara32= S5K3L2XX_TEST_PATTERN_CHECKSUM;			
			*pFeatureParaLen=4; 										
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		case SENSOR_FEATURE_INITIALIZE_AF:
		case SENSOR_FEATURE_CONSTANT_AF:
		case SENSOR_FEATURE_MOVE_FOCUS_LENS:
			break;
        default:
            break;
    }
    return ERROR_NONE;
}



SENSOR_FUNCTION_STRUCT	SensorFuncS5K3L2XX=
{
    S5K3L2XXOpen,
    S5K3L2XXGetInfo,
    S5K3L2XXGetResolution,
    S5K3L2XXFeatureControl,
    S5K3L2XXControl,
    S5K3L2XXClose
};

UINT32 S5K3L2XX_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncS5K3L2XX;

    return ERROR_NONE;
} 

