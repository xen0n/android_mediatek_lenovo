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
#include <linux/xlog.h>
#include <asm/system.h>

#include <linux/proc_fs.h> 


#include <linux/dma-mapping.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov13850mipiraw_Sensor.h"
#include "ov13850mipiraw_Camera_Sensor_para.h"
#include "ov13850mipiraw_CameraCustomized.h"

#include "ov13850mipiraw_Settings.h"

//xb.pang for lenovo otp test
#define OV13850_OTP_PROG_TEST   1
#ifdef OV13850_OTP_PROG_TEST
#include "sunny_ov13850_otp.h"
#endif

#define OV13850_DEBUG   
#ifdef OV13850_DEBUG
	#define OV13850DB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[OV13850Raw] ",  fmt, ##arg)
#else
	#define OV13850DB(fmt, arg...)
#endif



#define OV13850_TEST_PATTERN_CHECKSUM (0xf86cfdf4) //V_MIRROR
static DEFINE_SPINLOCK(ov13850mipiraw_drv_lock);


#define mDELAY(ms)  mdelay(ms)
kal_uint8 OV13850_WRITE_ID = OV13850MIPI_WRITE_ID;
kal_uint32 OV13850_FeatureControl_PERIOD_PixelNum=OV13850_PV_PERIOD_PIXEL_NUMS;
kal_uint32 OV13850_FeatureControl_PERIOD_LineNum=OV13850_PV_PERIOD_LINE_NUMS;
UINT16  ov13850VIDEO_MODE_TARGET_FPS = 30;
MSDK_SENSOR_CONFIG_STRUCT OV13850SensorConfigData;
MSDK_SCENARIO_ID_ENUM OV13850CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT OV13850SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT OV13850SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static OV13850_PARA_STRUCT ov13850;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);
extern int iMultiWriteReg(u8 *pData, u16 lens, u16 i2cId);

#define OV13850_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, OV13850_WRITE_ID)

#define OV13850_multi_write_cmos_sensor(pData, lens) iMultiWriteReg((u8*) pData, (u16) lens, OV13850_WRITE_ID)

#define OV13850_ORIENTATION IMAGE_H_MIRROR//IMAGE_NORMAL //IMAGE_V_MIRROR

kal_uint16 OV13850_read_cmos_sensor(kal_uint32 addr)
{
kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,OV13850_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)

#define REG_OPT_IMPROVE
#ifdef REG_OPT_IMPROVE
extern int iBurstWriteRegEx(u32 phyAddr, u32 bytes, u16 i2cId);
extern int iBurstReadRegEx(u32 phyAddr, u16 regAddr, u32 bytes, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
u8 *g_pdma_buf;
u32 g_phyAddr;
int ov8865_write_continuously(u8 *a_pSendData , u16 a_sizeSendData)
{
	return iWriteRegI2C(a_pSendData, a_sizeSendData, OV13850_WRITE_ID);
}

int ov8865_write_burst(u16 a_sizeSendData)
{
	return iBurstWriteRegEx(g_phyAddr, a_sizeSendData, OV13850_WRITE_ID);
}

int ov8865_read_burst(u8 *buf, u16 regAddr, u8 len)
{
	int ret = iBurstReadRegEx(g_phyAddr, regAddr, len, OV13850_WRITE_ID);
	memcpy(buf, g_pdma_buf, len);
	return ret;
}

int ov8865_read_coninuously(u16 a_u2Addr, u8 * a_pRecvData, u16 a_sizeRecvData)
{
	u8 cmd[2] = {a_u2Addr >>8, a_u2Addr & 0xff};
	return iReadRegI2C(cmd, 2, a_pRecvData, a_sizeRecvData, OV13850_WRITE_ID);
}

int ov8865_write_reg_settings(reg_setting *pRegSettings, int items)
{
	int inum = 0;
	int ret = 0;
	u8 buf[128];
	u8 *ptr;
	u8 len;
	
	reg_setting *preg, *pend;
	preg = pRegSettings;
	pend = pRegSettings + items;

	while (preg < pend) {
		buf[0] = preg->addr >> 8;
		buf[1] = preg->addr & 0xff;
		buf[2] = preg->val;

		ptr = &buf[3];
		while ((preg < pend) && ((preg+1)->addr == preg->addr + 1)) {
			preg ++;
			*ptr++ = preg->val;
		}
		len = ptr - buf;
		if (len > 8) {
			memcpy(g_pdma_buf, buf, len);
			ret |= ov8865_write_burst(len);
		} else {
			ret |= ov8865_write_continuously(buf, len);
		}
		//OV13850DB("[ADD]:%x, len=%d", (u16)buf[0]<<8 | buf[1], len);
		preg++;
	}

	return (ret ? -1 : 0);
}

int ov8865_clear_buffer(u16 a_u2Addr, u16 a_sizeSendData)
{
	u8 sendData[128];
	if (a_sizeSendData > 126) {
		OV13850DB("[ERROR FATAL] %s size too long\n\n", __func__);
		return -1;
	}
	g_pdma_buf[0] = (u8)(a_u2Addr >>8);
	g_pdma_buf[1] = (u8)(a_u2Addr & 0xff);
	memset(g_pdma_buf+2, 0, a_sizeSendData);
	
	return ov8865_write_burst(a_sizeSendData+2);
}

#endif

void OV13850_write_shutter(kal_uint32 shutter)
{

#if 1
	kal_uint32 min_framelength = OV13850_PV_PERIOD_PIXEL_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;

	OV13850DB("!!shutter=%d!!!!!\n", shutter);

	if(ov13850.OV13850AutoFlickerMode == KAL_TRUE)
	{
		if ( SENSOR_MODE_PREVIEW == ov13850.sensorMode )  //(g_iOV13850_Mode == OV13850_MODE_PREVIEW)	//SXGA size output
		{
			line_length = OV13850_PV_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
			max_shutter = OV13850_PV_PERIOD_LINE_NUMS + ov13850.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == ov13850.sensorMode ) //add for video_6M setting
		{
			line_length = OV13850_VIDEO_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
			max_shutter = OV13850_VIDEO_PERIOD_LINE_NUMS + ov13850.DummyLines ;
		}
		else
		{
			line_length = OV13850_FULL_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
			max_shutter = OV13850_FULL_PERIOD_LINE_NUMS + ov13850.DummyLines ;
		}
		//OV13850DB("linelength %d, max_shutter %d!!\n",line_length,max_shutter);

		switch(OV13850CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				//OV13850DB("AutoFlickerMode!!! MSDK_SCENARIO_ID_CAMERA_ZSD  0!!\n");
				min_framelength = max_shutter;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				
				//OV13850DB("AutoFlickerMode!!! MSDK_SCENARIO_ID_VIDEO_PREVIEW  0!!\n");
				if( ov13850VIDEO_MODE_TARGET_FPS==30)
				{
					min_framelength = (OV13850MIPI_VIDEO_CLK) /(OV13850_VIDEO_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/306*10 ;
				}
				else if( ov13850VIDEO_MODE_TARGET_FPS==15)
				{
					min_framelength = (OV13850MIPI_VIDEO_CLK) /(OV13850_VIDEO_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/148*10 ;
				}
				else
				{
					min_framelength = max_shutter;
				}
				break;
			default:
				//min_framelength = (OV13850MIPI_PREVIEW_CLK) /(OV13850_PV_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/294*10 ;
				min_framelength = (OV13850MIPI_PREVIEW_CLK) /(OV13850_PV_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/303*10 ;
    			break;
		}

		//OV13850DB("AutoFlickerMode!!! min_framelength for AutoFlickerMode = %d (0x%x)\n",min_framelength,min_framelength);
		//OV13850DB("max framerate(10 base) autofilker = %d\n",(OV13850MIPI_PREVIEW_CLK)*10 /line_length/min_framelength);

		if (shutter < 3)
			shutter = 3;

		if (shutter > (max_shutter-8))
			extra_lines = shutter -( max_shutter - 8);
		else
			extra_lines = 0;
		//OV13850DB("extra_lines 0=%d!!\n",extra_lines);

		if ( SENSOR_MODE_PREVIEW == ov13850.sensorMode )	//SXGA size output
		{
			frame_length = OV13850_PV_PERIOD_LINE_NUMS+ ov13850.DummyLines + extra_lines ;
		}
		else if(SENSOR_MODE_VIDEO == ov13850.sensorMode)
		{
			frame_length = OV13850_VIDEO_PERIOD_LINE_NUMS+ ov13850.DummyLines + extra_lines ;
		}
		else				//QSXGA size output
		{
			frame_length = OV13850_FULL_PERIOD_LINE_NUMS + ov13850.DummyLines + extra_lines ;
		}
		//OV13850DB("frame_length 0= %d\n",frame_length);

		if (frame_length < min_framelength)
		{
			//shutter = min_framelength - 4;

			switch(OV13850CurrentScenarioId)
			{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				extra_lines = min_framelength- (OV13850_FULL_PERIOD_LINE_NUMS+ ov13850.DummyLines);
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				extra_lines = min_framelength- (OV13850_VIDEO_PERIOD_LINE_NUMS+ ov13850.DummyLines);
				break;//need check
			default:
				extra_lines = min_framelength- (OV13850_PV_PERIOD_LINE_NUMS+ ov13850.DummyLines);
    			break;
			}
			frame_length = min_framelength;
		}
		//Set total frame length
		if (frame_length >= 0x8000)
			frame_length = 0x7fff;

		OV13850_write_cmos_sensor(0x380e, (frame_length >> 8) & 0x7F);
		OV13850_write_cmos_sensor(0x380f, frame_length & 0xFF);

		spin_lock_irqsave(&ov13850mipiraw_drv_lock,flags);
		ov13850.maxExposureLines = frame_length-8;
		OV13850_FeatureControl_PERIOD_PixelNum = line_length;
		OV13850_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&ov13850mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		if (shutter > 0x7ff8)
			shutter = 0x7ff8;

		OV13850_write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
		OV13850_write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		OV13850_write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	/* Don't use the fraction part. */

	    //OV13850DB("frame_length %d,shutter %d!!\n",frame_length,shutter);
		//OV13850DB("framerate(10 base) = %d\n",(OV13850MIPI_PREVIEW_CLK)*10 /line_length/frame_length);
		//OV13850DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);

	}
	else
	{
		if ( SENSOR_MODE_PREVIEW == ov13850.sensorMode )  //(g_iOV13850_Mode == OV13850_MODE_PREVIEW)	//SXGA size output
		{
			max_shutter = OV13850_PV_PERIOD_LINE_NUMS + ov13850.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == ov13850.sensorMode ) //add for video_6M setting
		{
			max_shutter = OV13850_VIDEO_PERIOD_LINE_NUMS + ov13850.DummyLines ;
		}
		else
		{
			max_shutter = OV13850_FULL_PERIOD_LINE_NUMS + ov13850.DummyLines ;
		}
		//OV13850DB(" max_shutter %d!!\n",max_shutter);

		if (shutter < 3)
			shutter = 3;

		if (shutter > (max_shutter-8))
			extra_lines = shutter - (max_shutter -8);
		else
			extra_lines = 0;
		//OV13850DB("extra_lines 0=%d!!\n",extra_lines);

		if ( SENSOR_MODE_PREVIEW == ov13850.sensorMode )	//SXGA size output
		{
			line_length = OV13850_PV_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
			frame_length = OV13850_PV_PERIOD_LINE_NUMS+ ov13850.DummyLines + extra_lines ;
		}
		else if( SENSOR_MODE_VIDEO == ov13850.sensorMode )
		{
			line_length = OV13850_VIDEO_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
			frame_length = OV13850_VIDEO_PERIOD_LINE_NUMS + ov13850.DummyLines + extra_lines ;
		}
		else				//QSXGA size output
		{
			line_length = OV13850_FULL_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
			frame_length = OV13850_FULL_PERIOD_LINE_NUMS + ov13850.DummyLines + extra_lines ;
		}

		ASSERT(line_length < OV13850_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < OV13850_MAX_FRAME_LENGTH); 	//0xFFFF
		
		//Set total frame length
		if (frame_length >= 0x8000)
			frame_length = 0x7fff;
		
		OV13850_write_cmos_sensor(0x380e, (frame_length >> 8) & 0x7F);
		OV13850_write_cmos_sensor(0x380f, frame_length & 0xFF);
		
		spin_lock_irqsave(&ov13850mipiraw_drv_lock,flags);
		ov13850.maxExposureLines = frame_length -8;
		OV13850_FeatureControl_PERIOD_PixelNum = line_length;
		OV13850_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&ov13850mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		if (shutter > 0x7ff8)
			shutter = 0x7ff8;
		
		OV13850_write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
		OV13850_write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		OV13850_write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	/* Don't use the fraction part. */
		//OV13850DB("frame_length %d,shutter %d!!\n",frame_length,shutter);
		//OV13850DB("framerate(10 base) = %d\n",(OV13850MIPI_PREVIEW_CLK)*10 /line_length/frame_length);
		//OV13850DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}
#endif
}   /* write_OV13850_shutter */

static kal_uint16 OV13850Reg2Gain(const kal_uint16 iReg)
{
    kal_uint8 iI;
    kal_uint16 iGain = ov13850.ispBaseGain;    // 1x-gain base

    // Range: 1x to 32x
    // Gain = (GAIN[9] + 1) *(GAIN[8] + 1) *(GAIN[7] + 1) * (GAIN[6] + 1) * (GAIN[5] + 1) * (GAIN[4] + 1) * (1 + GAIN[3:0] / 16)
    //for (iI = 8; iI >= 4; iI--) {
    //    iGain *= (((iReg >> iI) & 0x01) + 1);
    //}
    iGain = iReg * ov13850.ispBaseGain / 32;
    return iGain; //ov13850.realGain
}

static kal_uint16 OV13850Gain2Reg(const kal_uint16 Gain)
{
    kal_uint16 iReg = 0x0000;
	kal_uint16 iGain=Gain;
	//if(iGain <  ov13850.ispBaseGain) 
	//{
		iReg = Gain*32/ov13850.ispBaseGain;
		if(iReg < 0x20)
		{
			iReg = 0x20;
		}
		if(iReg > 0xfc)
		{
			iReg = 0xfc;
		}
	//}
	//else
	//{
	//	OV13850DB("out of range!\n");
	//}
	OV13850DB("[OV13850Gain2Reg]: isp gain:%d,sensor gain:0x%x\n",iGain,iReg);

    return iReg;//ov13850. sensorGlobalGain

}

void write_OV13850_gain(kal_uint16 gain)
{
	OV13850_write_cmos_sensor(0x350a,(gain>>8));
	OV13850_write_cmos_sensor(0x350b,(gain&0xff));
	return;
}
void OV13850_SetGain(UINT16 iGain)
{
	unsigned long flags;
	spin_lock_irqsave(&ov13850mipiraw_drv_lock,flags);
	ov13850.realGain = iGain;
	ov13850.sensorGlobalGain = OV13850Gain2Reg(iGain);
	spin_unlock_irqrestore(&ov13850mipiraw_drv_lock,flags);
	write_OV13850_gain(ov13850.sensorGlobalGain);
	OV13850DB("[OV13850_SetGain]ov13850.sensorGlobalGain=0x%x,ov13850.realGain=%d\n",ov13850.sensorGlobalGain,ov13850.realGain);

}   /*  OV13850_SetGain_SetGain  */

kal_uint16 read_OV13850_gain(void)
{
	kal_uint16 read_gain=0;
	read_gain=(((OV13850_read_cmos_sensor(0x350a)&0x01) << 8) | OV13850_read_cmos_sensor(0x350b));
	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.sensorGlobalGain = read_gain;
	ov13850.realGain = OV13850Reg2Gain(ov13850.sensorGlobalGain);
	spin_unlock(&ov13850mipiraw_drv_lock);
	OV13850DB("ov13850.sensorGlobalGain=0x%x,ov13850.realGain=%d\n",ov13850.sensorGlobalGain,ov13850.realGain);
	return ov13850.sensorGlobalGain;
}  /* read_OV13850_gain */


/*==============================================================
*
*Description:for history but not use now functions
*
*xxx_camera_para_to_sensor(void)
*
*xxx_sensor_to_camera_para(void)
*
*xxx_get_sensor_group_info(kal_uint16 group_idx, kal_int8* 
*	group_name_ptr, kal_int32* item_count_ptr)
*
*xxx_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 
*	item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
*
*xxx_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 
*	item_idx, kal_int32 ItemValue)
*
===============================================================*/
void OV13850_camera_para_to_sensor(void)
{}

void OV13850_sensor_to_camera_para(void)
{}
void OV13850_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{}
void OV13850_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{}
kal_bool OV13850_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{    
	return KAL_TRUE;
}


/*==============================================================

*function name:	xxx_get_sensor_group_count(void)
*
*Description: 	use for cct connect
*
*para:void
*
===============================================================*/

kal_int32  OV13850_get_sensor_group_count(void)
{
	return GROUP_TOTAL_NUMS;
}


/*==============================================================

*function name:	xxx_SetDummy( const kal_uint32 iPixels, 
*										const kal_uint32 iLines )
*
*Description: 	use for cct connect
*
*para:iPixels:dummy pixel(affect expose time?);
*para:iLines: dummy lines(affect frame rate?);
*
===============================================================*/

static void OV13850_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
 	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	OV13850DB("OV13850_SetDummy iPixels = %d,	iLines = %d \n",iPixels,iLines);

	if ( SENSOR_MODE_PREVIEW == ov13850.sensorMode )	//SXGA size output
	{
		line_length = OV13850_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV13850_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_VIDEO== ov13850.sensorMode )
	{
		line_length = OV13850_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV13850_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else//QSXGA size output
	{
		line_length = OV13850_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV13850_FULL_PERIOD_LINE_NUMS + iLines;
	}

	//if(ov13850.maxExposureLines > frame_length -4 )
	//	return;

	//ASSERT(line_length < OV13850_MAX_LINE_LENGTH);		//0xCCCC
	//ASSERT(frame_length < OV13850_MAX_FRAME_LENGTH);	//0xFFFF

	//Set total frame length
	if (frame_length >= 0x8000)
			frame_length = 0x7fff;
	OV13850_write_cmos_sensor(0x380e, (frame_length >> 8) & 0x7F);
	OV13850_write_cmos_sensor(0x380f, frame_length & 0xFF);

	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.maxExposureLines = frame_length -8;
	OV13850_FeatureControl_PERIOD_PixelNum = line_length;
	OV13850_FeatureControl_PERIOD_LineNum = frame_length;
	spin_unlock(&ov13850mipiraw_drv_lock);

	//Set total line length
	OV13850_write_cmos_sensor(0x380c, (line_length >> 8) & 0x7F);
	OV13850_write_cmos_sensor(0x380d, line_length & 0xFF);

	
	OV13850DB("OV13850_SetDummy linelength %d,  frame_length= %d \n",line_length,frame_length);

}   /*  OV13850_SetDummy */



static kal_uint8 ov13850_init[] = {
	//for PCLK :384M or fast:pclk:240M
	/*
	
	@@ RES_4208x3120 24fps
	;24Mhz Xclk
	;SCLK 96Mhz, Pclk 384MHz
	;4Lane, MIPI datarate 960Mbps/Lane
	;24fps
	;pixels per line=4800(0x12c0) 
	;lines per frame=3328(0xD00)
	
	;102 2630 960
	;88 e7 3f
	
	;100 99 4208 3120
	;100 98 1 0
	;102 81 0
	;102 3601 964
	
	;102 910 31
	;102 84 1
	
	;c8 0300 62
	*/
 //  0x01, 0x03, 0x01,
   0x03, 0x00, 0x01,
   0x03, 0x01, 0x00,
   0x03, 0x02, 0x28,
   0x03, 0x03, 0x00,
   0x03, 0x0a, 0x00,
   0x30, 0x0f, 0x11,
   0x30, 0x10, 0x01,
   0x30, 0x11, 0x76,
   0x30, 0x12, 0x41,
   0x30, 0x13, 0x12,
   0x30, 0x14, 0x11,
   0x30, 0x1f, 0x03,
   0x31, 0x06, 0x00,
   0x32, 0x10, 0x47,
   0x35, 0x00, 0x00,
   0x35, 0x01, 0x60,
   0x35, 0x02, 0x00,
   0x35, 0x06, 0x00,
   0x35, 0x07, 0x02,
   0x35, 0x08, 0x00,
   0x35, 0x0a, 0x00,
   0x35, 0x0b, 0x80,
   0x35, 0x0e, 0x00,
   0x35, 0x0f, 0x10,
   0x36, 0x00, 0x40,
   0x36, 0x01, 0xfc,
   0x36, 0x02, 0x02,
   0x36, 0x03, 0x48,
   0x36, 0x04, 0xa5,
   0x36, 0x05, 0x9f,
   0x36, 0x07, 0x00,
   0x36, 0x0a, 0x40,
   0x36, 0x0b, 0x91,
   0x36, 0x0c, 0x49,
   0x36, 0x0f, 0x8a,
   0x36, 0x11, 0x10,
   0x36, 0x12, 0x27,
   0x36, 0x13, 0x33,
   0x36, 0x15, 0x08,
   0x36, 0x41, 0x02,
   0x36, 0x60, 0x82,
   0x36, 0x68, 0x54,
   0x36, 0x69, 0x00,
   0x36, 0x67, 0xa0,
   0x37, 0x02, 0x40,
   0x37, 0x03, 0x44,
   0x37, 0x04, 0x2c,
   0x37, 0x05, 0x24,
   0x37, 0x06, 0x50,
   0x37, 0x07, 0x44,
   0x37, 0x08, 0x3c,
   0x37, 0x09, 0x1f,
   0x37, 0x0a, 0x26,
   0x37, 0x0b, 0x3c,
   0x37, 0x20, 0x66,
   0x37, 0x22, 0x84,
   0x37, 0x28, 0x40,
   0x37, 0x2a, 0x00,
   0x37, 0x2e, 0x22,
   0x37, 0x2f, 0xa0,
   0x37, 0x30, 0x00,
   0x37, 0x31, 0x00,
   0x37, 0x32, 0x00,
   0x37, 0x33, 0x00,
   0x37, 0x48, 0x00,
   0x37, 0x10, 0x28,
   0x37, 0x16, 0x03,
   0x37, 0x18, 0x1C,
   0x37, 0x19, 0x08,
   0x37, 0x1c, 0xfc,
   0x37, 0x60, 0x13,
   0x37, 0x61, 0x34,
   0x37, 0x62, 0x86,
   0x37, 0x63, 0x16,
   0x37, 0x67, 0x24,
   0x37, 0x68, 0x06,
   0x37, 0x69, 0x45,
   0x37, 0x6c, 0x23,
   0x3d, 0x84, 0x00,
   0x3d, 0x85, 0x17,
   0x3d, 0x8c, 0x73,
   0x3d, 0x8d, 0xbf,
   0x38, 0x00, 0x00,
   0x38, 0x01, 0x00,
   0x38, 0x02, 0x00,
   0x38, 0x03, 0x04,
   0x38, 0x04, 0x10,
   0x38, 0x05, 0x9f,
   0x38, 0x06, 0x0c,
   0x38, 0x07, 0x4b,
   0x38, 0x08, 0x08,
   0x38, 0x09, 0x40,
   0x38, 0x0a, 0x06,
   0x38, 0x0b, 0x20,
   0x38, 0x0c, 0x09,
   0x38, 0x0d, 0x60,
   0x38, 0x0e, 0x0d,
   0x38, 0x0f, 0x00,
   0x38, 0x10, 0x00,
   0x38, 0x11, 0x08,
   0x38, 0x12, 0x00,
   0x38, 0x13, 0x02,
   0x38, 0x14, 0x31,
   0x38, 0x15, 0x31,
   0x38, 0x20, 0x01,
   0x38, 0x21, 0x06,
   0x38, 0x34, 0x00,
   0x38, 0x35, 0x1c,
   0x38, 0x36, 0x08,
   0x38, 0x37, 0x02,
   0x40, 0x00, 0xf1,
   0x40, 0x01, 0x00,
   0x40, 0x06, 0x1f,
   0x40, 0x07, 0x1f,
   0x40, 0x0b, 0x0c,
   0x40, 0x11, 0x00,
   0x40, 0x1a, 0x00,
   0x40, 0x1b, 0x00,
   0x40, 0x1c, 0x00,
   0x40, 0x1d, 0x00,
   0x40, 0x20, 0x00,
   0x40, 0x21, 0xe4,
   0x40, 0x22, 0x04,
   0x40, 0x23, 0xd7,
   0x40, 0x24, 0x05,
   0x40, 0x25, 0xbc,
   0x40, 0x26, 0x05,
   0x40, 0x27, 0xbf,
   0x40, 0x28, 0x00,
   0x40, 0x29, 0x02,
   0x40, 0x2a, 0x04,
   0x40, 0x2b, 0x08,
   0x40, 0x2c, 0x02,
   0x40, 0x2d, 0x02,
   0x40, 0x2e, 0x0c,
   0x40, 0x2f, 0x08,
   0x40, 0x3d, 0x2c,
   0x40, 0x3f, 0x7f,
   0x45, 0x00, 0x82,
   0x45, 0x01, 0x3c,
   0x46, 0x01, 0x83,
   0x46, 0x02, 0x22,
   0x46, 0x03, 0x01,
   0x48, 0x37, 0x19,
   0x4d, 0x00, 0x04,
   0x4d, 0x01, 0x42,
   0x4d, 0x02, 0xd1,
   0x4d, 0x03, 0x90,
   0x4d, 0x04, 0x66,
   0x4d, 0x05, 0x65,
   0x50, 0x00, 0x0e,
   0x50, 0x01, 0x01,
   0x50, 0x02, 0x07,
   0x50, 0x13, 0x40,
   0x50, 0x1c, 0x00,
   0x50, 0x1d, 0x10,
   0x52, 0x42, 0x00,
   0x52, 0x43, 0xb8,
   0x52, 0x44, 0x00,
   0x52, 0x45, 0xf9,
   0x52, 0x46, 0x00,
   0x52, 0x47, 0xf6,
   0x52, 0x48, 0x00,
   0x52, 0x49, 0xa6,
   0x53, 0x00, 0xfc,
   0x53, 0x01, 0xdf,
   0x53, 0x02, 0x3f,
   0x53, 0x03, 0x08,
   0x53, 0x04, 0x0c,
   0x53, 0x05, 0x10,
   0x53, 0x06, 0x20,
   0x53, 0x07, 0x40,
   0x53, 0x08, 0x08,
   0x53, 0x09, 0x08,
   0x53, 0x0a, 0x02,
   0x53, 0x0b, 0x01,
   0x53, 0x0c, 0x01,
   0x53, 0x0d, 0x0c,
   0x53, 0x0e, 0x02,
   0x53, 0x0f, 0x01,
   0x53, 0x10, 0x01,
   0x54, 0x00, 0x00,
   0x54, 0x01, 0x61,
   0x54, 0x02, 0x00,
   0x54, 0x03, 0x00,
   0x54, 0x04, 0x00,
   0x54, 0x05, 0x40,
   0x54, 0x0c, 0x05,
   0x5b, 0x00, 0x00,
   0x5b, 0x01, 0x00,
   0x5b, 0x02, 0x01,
   0x5b, 0x03, 0xff,
   0x5b, 0x04, 0x02,
   0x5b, 0x05, 0x6c,
   0x5b, 0x09, 0x02,
   0x5e, 0x00, 0x00,
   0x5e, 0x10, 0x1c,
   0x52,0x3e,  0x18,
   0x52,0x3f,  0x0c,
   0x52,0x40,  0x08,
   0x52,0x41,  0x0d,
   0x01, 0x00, 0x01, 
};

void OV13850PreviewSetting(void)
{
	OV13850DB("OV13850PreviewSetting \n");
    /*
	@@ 0 20 RES_2112x1568 30fps_key setting
	;24Mhz Xclk
	;SCLK 60Mhz, Pclk 384MHz
	;4Lane, MIPI datarate 960Mbps/Lane
	;30fps
	;pixels per line=4800 
	;lines per frame=2664

	;100 99 2112 1568
	;102 3601 BBD
	*/
#ifndef REG_OPT_IMPROVE
	OV13850_write_cmos_sensor(0x0100, 0x00);
	mdelay(2);
  OV13850_write_cmos_sensor(0x0300, 0x01);
  OV13850_write_cmos_sensor(0x0302, 0x28);
  OV13850_write_cmos_sensor(0x0303, 0x00);
  OV13850_write_cmos_sensor(0x3612, 0x27); 
 // OV13850_write_cmos_sensor(0x3613, 0x33);
 // OV13850_write_cmos_sensor(0x3614, 0x28);
  OV13850_write_cmos_sensor(0x370a, 0x26);
  OV13850_write_cmos_sensor(0x372a, 0x00); 
  OV13850_write_cmos_sensor(0x3718, 0x1C);  
  OV13850_write_cmos_sensor(0x3800, 0x00); 
  OV13850_write_cmos_sensor(0x3801, 0x00); 
  OV13850_write_cmos_sensor(0x3802, 0x00); 
  OV13850_write_cmos_sensor(0x3803, 0x04); 
  OV13850_write_cmos_sensor(0x3804, 0x10); 
  OV13850_write_cmos_sensor(0x3805, 0x9f); 
  OV13850_write_cmos_sensor(0x3806, 0x0c); 
  OV13850_write_cmos_sensor(0x3807, 0x4b); 
  OV13850_write_cmos_sensor(0x3808, 0x08); 
  OV13850_write_cmos_sensor(0x3809, 0x40); 
  OV13850_write_cmos_sensor(0x380a, 0x06); 
  OV13850_write_cmos_sensor(0x380b, 0x20); 
  OV13850_write_cmos_sensor(0x380c, 0x09); 
  OV13850_write_cmos_sensor(0x380d, 0x60); 
  OV13850_write_cmos_sensor(0x380e, 0x0d); 
  OV13850_write_cmos_sensor(0x380f, 0x00); 
  OV13850_write_cmos_sensor(0x3810, 0x00); 
  OV13850_write_cmos_sensor(0x3811, 0x08); 
  OV13850_write_cmos_sensor(0x3812, 0x00); 
  OV13850_write_cmos_sensor(0x3813, 0x02); 
  OV13850_write_cmos_sensor(0x3814, 0x31); 
  OV13850_write_cmos_sensor(0x3815, 0x31); 
  OV13850_write_cmos_sensor(0x3820, 0x01); 
  OV13850_write_cmos_sensor(0x3821, 0x06); 
  OV13850_write_cmos_sensor(0x3836, 0x08); 
  OV13850_write_cmos_sensor(0x3837, 0x02); 
  OV13850_write_cmos_sensor(0x4020, 0x00); 
  OV13850_write_cmos_sensor(0x4021, 0xe4); 
  OV13850_write_cmos_sensor(0x4022, 0x04); 
  OV13850_write_cmos_sensor(0x4023, 0xd7); 
  OV13850_write_cmos_sensor(0x4024, 0x05); 
  OV13850_write_cmos_sensor(0x4025, 0xbc); 
  OV13850_write_cmos_sensor(0x4026, 0x05); 
  OV13850_write_cmos_sensor(0x4027, 0xbf); 
  OV13850_write_cmos_sensor(0x4501, 0x3c); 
  OV13850_write_cmos_sensor(0x4601, 0x83); 
  OV13850_write_cmos_sensor(0x4603, 0x01); 
  OV13850_write_cmos_sensor(0x4837, 0x19); 
  OV13850_write_cmos_sensor(0x5401, 0x61); 
  OV13850_write_cmos_sensor(0x5405, 0x40); 
	mdelay(2);
	OV13850_write_cmos_sensor(0x0100, 0x01);
	mdelay(20);
#else
	ov8865_write_reg_settings(ov13850mipiraw_preview_seq, sizeof(ov13850mipiraw_preview_seq)/sizeof(ov13850mipiraw_preview_seq[0]));
#endif
	mdelay(20);	
	
}


void OV13850VideoSetting(void)
{
	OV13850DB("OV13850VideoSetting \n");
	 /*
	@@ 0 20 RES_2112x1568 30fps_key setting
	;24Mhz Xclk
	;SCLK 60Mhz, Pclk 480MHz
	;4Lane, MIPI datarate 640Mbps/Lane
	;30fps
	;pixels per line=4800
	;lines per frame=3328

	;100 99 2112 1568
	;102 3601 BBD
	*/
#ifndef REG_OPT_IMPROVE
	OV13850_write_cmos_sensor(0x0100, 0x00);
	mdelay(2);
  OV13850_write_cmos_sensor(0x0300, 0x01);
  OV13850_write_cmos_sensor(0x0302, 0x28);
  OV13850_write_cmos_sensor(0x0303, 0x00);
  OV13850_write_cmos_sensor(0x3612, 0x27);
 // OV13850_write_cmos_sensor(0x3613, 0x33);
 // OV13850_write_cmos_sensor(0x3614, 0x28);
  OV13850_write_cmos_sensor(0x370a, 0x26);
  OV13850_write_cmos_sensor(0x372a, 0x00);
  OV13850_write_cmos_sensor(0x3718, 0x1C);
  OV13850_write_cmos_sensor(0x3800, 0x00);
  OV13850_write_cmos_sensor(0x3801, 0x00);
  OV13850_write_cmos_sensor(0x3802, 0x00);
  OV13850_write_cmos_sensor(0x3803, 0x04);
  OV13850_write_cmos_sensor(0x3804, 0x10);
  OV13850_write_cmos_sensor(0x3805, 0x9f);
  OV13850_write_cmos_sensor(0x3806, 0x0c);
  OV13850_write_cmos_sensor(0x3807, 0x4b);
  OV13850_write_cmos_sensor(0x3808, 0x08);
  OV13850_write_cmos_sensor(0x3809, 0x40);
  OV13850_write_cmos_sensor(0x380a, 0x06);
  OV13850_write_cmos_sensor(0x380b, 0x20);
  OV13850_write_cmos_sensor(0x380c, 0x09);
  OV13850_write_cmos_sensor(0x380d, 0x60);
  OV13850_write_cmos_sensor(0x380e, 0x0d);
  OV13850_write_cmos_sensor(0x380f, 0x00);
  OV13850_write_cmos_sensor(0x3810, 0x00);
  OV13850_write_cmos_sensor(0x3811, 0x08);
  OV13850_write_cmos_sensor(0x3812, 0x00);
  OV13850_write_cmos_sensor(0x3813, 0x02);
  OV13850_write_cmos_sensor(0x3814, 0x31);
  OV13850_write_cmos_sensor(0x3815, 0x31);
  OV13850_write_cmos_sensor(0x3820, 0x01);
  OV13850_write_cmos_sensor(0x3821, 0x06);
  OV13850_write_cmos_sensor(0x3836, 0x08);
  OV13850_write_cmos_sensor(0x3837, 0x02);
  OV13850_write_cmos_sensor(0x4020, 0x00);
  OV13850_write_cmos_sensor(0x4021, 0xe4);
  OV13850_write_cmos_sensor(0x4022, 0x04);
  OV13850_write_cmos_sensor(0x4023, 0xd7);
  OV13850_write_cmos_sensor(0x4024, 0x05);
  OV13850_write_cmos_sensor(0x4025, 0xbc);
  OV13850_write_cmos_sensor(0x4026, 0x05);
  OV13850_write_cmos_sensor(0x4027, 0xbf);
  OV13850_write_cmos_sensor(0x4501, 0x3c);
  OV13850_write_cmos_sensor(0x4601, 0x83);
  OV13850_write_cmos_sensor(0x4603, 0x01);
  OV13850_write_cmos_sensor(0x4837, 0x19);
  OV13850_write_cmos_sensor(0x5401, 0x61);
  OV13850_write_cmos_sensor(0x5405, 0x40);
	mdelay(2);
	OV13850_write_cmos_sensor(0x0100, 0x01);
	mdelay(20);
#else
	ov8865_write_reg_settings(ov13850mipiraw_video_seq, sizeof(ov13850mipiraw_video_seq)/sizeof(ov13850mipiraw_video_seq[0]));
#endif
	mdelay(20);

}




void OV13850CaptureSetting(void)
{
	/*
	@@ RES_4208x3120 24fps-Key setting
	;24Mhz Xclk
	;SCLK 96Mhz, Pclk 384MHz
	;4Lane, MIPI datarate 960Mbps/Lane
	;24fps
	;pixels per line=4800(0x12c0) 
	;lines per frame=3328(0xD00)

	;100 99 4208 3120
	;102 3601 964
	*/
#ifndef REG_OPT_IMPROVE
	#if 1
	//for PCLK :384M
	OV13850_write_cmos_sensor(0x0100, 0x00);  
	mdelay(2);
  OV13850_write_cmos_sensor(0x0300, 0x00);
  OV13850_write_cmos_sensor(0x0302, 0x28);
  OV13850_write_cmos_sensor(0x0303, 0x00);
  OV13850_write_cmos_sensor(0x3612, 0x07);
  //OV13850_write_cmos_sensor(0x3613, 0x22);
 // OV13850_write_cmos_sensor(0x3614, 0x1e);
  OV13850_write_cmos_sensor(0x370a, 0x24);
  OV13850_write_cmos_sensor(0x372a, 0x04);
  OV13850_write_cmos_sensor(0x3718, 0x10);
  OV13850_write_cmos_sensor(0x3800, 0x00);
  OV13850_write_cmos_sensor(0x3801, 0x0C);
  OV13850_write_cmos_sensor(0x3802, 0x00);
  OV13850_write_cmos_sensor(0x3803, 0x04);
  OV13850_write_cmos_sensor(0x3804, 0x10);
  OV13850_write_cmos_sensor(0x3805, 0x93);
  OV13850_write_cmos_sensor(0x3806, 0x0c);
  OV13850_write_cmos_sensor(0x3807, 0x4B);
  OV13850_write_cmos_sensor(0x3808, 0x10);
  OV13850_write_cmos_sensor(0x3809, 0x80);
  OV13850_write_cmos_sensor(0x380a, 0x0c);
  OV13850_write_cmos_sensor(0x380b, 0x40);
  OV13850_write_cmos_sensor(0x380c, 0x17);
  OV13850_write_cmos_sensor(0x380d, 0x7a);
  OV13850_write_cmos_sensor(0x380e, 0x0d);
  OV13850_write_cmos_sensor(0x380f, 0x00);
  OV13850_write_cmos_sensor(0x3810, 0x00);
  OV13850_write_cmos_sensor(0x3811, 0x04);
  OV13850_write_cmos_sensor(0x3812, 0x00);
  OV13850_write_cmos_sensor(0x3813, 0x04);
  OV13850_write_cmos_sensor(0x3814, 0x11);
  OV13850_write_cmos_sensor(0x3815, 0x11);
  OV13850_write_cmos_sensor(0x3820, 0x00);
  OV13850_write_cmos_sensor(0x3821, 0x04);
  OV13850_write_cmos_sensor(0x3836, 0x04);
  OV13850_write_cmos_sensor(0x3837, 0x01);
  OV13850_write_cmos_sensor(0x4020, 0x02);
  OV13850_write_cmos_sensor(0x4021, 0x4C);
  OV13850_write_cmos_sensor(0x4022, 0x0E);
  OV13850_write_cmos_sensor(0x4023, 0x37);
  OV13850_write_cmos_sensor(0x4024, 0x0F);
  OV13850_write_cmos_sensor(0x4025, 0x1C);
  OV13850_write_cmos_sensor(0x4026, 0x0F);
  OV13850_write_cmos_sensor(0x4027, 0x1F);
  OV13850_write_cmos_sensor(0x4501, 0x38);
  OV13850_write_cmos_sensor(0x4601, 0x04);
  OV13850_write_cmos_sensor(0x4603, 0x01); //NEIL
  OV13850_write_cmos_sensor(0x4837, 0x11);
  OV13850_write_cmos_sensor(0x5401, 0x71);
  OV13850_write_cmos_sensor(0x5405, 0x80);
	mdelay(2);
	OV13850_write_cmos_sensor(0x0100, 0x01);  
	mdelay(30);
    #else
	////for PCLK :240M
	//OV13850_write_cmos_sensor(0x0100, 0x00);
	OV13850_write_cmos_sensor(0x3208 , 0xa2);
	//OV13850_write_cmos_sensor(0x0100, 0x01);
	#endif
#else
	ov8865_write_reg_settings(ov13850mipiraw_capture_seq, sizeof(ov13850mipiraw_capture_seq)/sizeof(ov13850mipiraw_capture_seq[0]));
#endif
	mdelay(30);

}



static void OV13850_Sensor_Init(void)
{

#ifndef REG_OPT_IMPROVE
    int totalCnt = 0, len = 0;
	int transfer_len, transac_len=3;
	kal_uint8* pBuf=NULL;
	dma_addr_t dmaHandle;
	pBuf = (kal_uint8*)kmalloc(1024, GFP_KERNEL);
	

    totalCnt = ARRAY_SIZE(ov13850_init);
	transfer_len = totalCnt / transac_len;
	len = (transfer_len<<8)|transac_len;    
	OV13850DB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	memcpy(pBuf, &ov13850_init, totalCnt );   
	dmaHandle = dma_map_single(NULL, pBuf, 1024, DMA_TO_DEVICE);	
	OV13850_multi_write_cmos_sensor(dmaHandle, len); 

	dma_unmap_single(NULL, dmaHandle, 1024, DMA_TO_DEVICE);
	kfree(pBuf);
#else
	ov8865_write_reg_settings(ov13850mipiraw_init_seq, sizeof(ov13850mipiraw_init_seq)/sizeof(ov13850mipiraw_init_seq[0]));
#endif	
}   /*  OV13850_Sensor_Init  */

UINT32 OV13850Open(void)
{
	volatile signed int i;
	int  retry = 1;
	kal_uint16 sensor_id = 0;
	OV13850DB("OV13850Open enter :\n ");
	OV13850_WRITE_ID = OV13850MIPI_WRITE_ID_1;
	OV13850_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(10);

    // check if sensor ID correct
    do {
        sensor_id = (OV13850_read_cmos_sensor(0x300A)<<8)|OV13850_read_cmos_sensor(0x300B);
        if (sensor_id == OV13850_SENSOR_ID)
        	{
        		OV13850DB("write id=%x, Sensor ID = 0x%04x\n", OV13850_WRITE_ID,sensor_id);
            	break;
        	}
        OV13850DB("Read Sensor ID Fail = 0x%04x\n", sensor_id);
        retry--;
    } while (retry > 0);

    if (sensor_id != OV13850_SENSOR_ID) {
		OV13850_WRITE_ID=OV13850MIPI_WRITE_ID;
		OV13850_write_cmos_sensor(0x0103,0x01);// Reset sensor
	    mDELAY(10);
        retry = 1;
	    // check if sensor ID correct
	    do {
	        sensor_id = (OV13850_read_cmos_sensor(0x300A)<<8)|OV13850_read_cmos_sensor(0x300B);
	        if (sensor_id == OV13850_SENSOR_ID)
	        	{
	        		OV13850DB("write id=%x,Sensor ID = 0x%04x\n",OV13850_WRITE_ID, sensor_id);
	            	break;
	        	}
	        OV13850DB("Read Sensor ID Fail = 0x%04x\n", sensor_id);
	        retry--;
	    } while (retry > 0);
		 if (sensor_id != OV13850_SENSOR_ID) 
		 {
           return ERROR_SENSOR_CONNECT_FAIL;
		 	}
    }
	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.sensorMode = SENSOR_MODE_INIT;
	ov13850.OV13850AutoFlickerMode = KAL_FALSE;
	ov13850.OV13850VideoMode = KAL_FALSE;
	spin_unlock(&ov13850mipiraw_drv_lock);

	#ifdef REG_OPT_IMPROVE
	if (!g_pdma_buf) {
		g_pdma_buf = dma_alloc_coherent(0, 1024, &g_phyAddr, GFP_KERNEL);
		if (!g_pdma_buf) {
			OV13850DB("FATAL: DMA BUFFER ALLOC FAIL\n ");
			//return -1;
		}
	}
	OV13850DB("%s + g_pdma_buf=%p", __func__, g_pdma_buf);
	#endif

	OV13850_Sensor_Init();

	#ifdef OV13850_OTP_PROG_TEST
	update_otp_wb();
    update_otp_lenc();
	#endif

	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.DummyLines= 0;
	ov13850.DummyPixels= 0;
	ov13850.shutter = 0x4EA;
	ov13850.pvShutter = 0x4EA;
	ov13850.maxExposureLines =OV13850_PV_PERIOD_LINE_NUMS -4;
	ov13850.ispBaseGain = BASEGAIN;//0x40
	ov13850.sensorGlobalGain = 0x1f;//sensor gain read from 0x350a 0x350b; 0x1f as 3.875x
	ov13850.pvGain = 0x1f;
	ov13850.realGain = OV13850Reg2Gain(0x1f);//ispBaseGain as 1x
	spin_unlock(&ov13850mipiraw_drv_lock);

	OV13850DB("OV13850Open exit :\n ");

    return ERROR_NONE;
}

UINT32 OV13850GetSensorID(UINT32 *sensorID)
{
    int  retry = 1;

	OV13850DB("OV13850GetSensorID enter :\n ");
	OV13850_WRITE_ID = OV13850MIPI_WRITE_ID;
	OV13850_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(10);

    // check if sensor ID correct
    do {
        *sensorID = (OV13850_read_cmos_sensor(0x300A)<<8)|OV13850_read_cmos_sensor(0x300B);
        if (*sensorID == OV13850_SENSOR_ID)
        	{
        		OV13850DB("write id=%x, Sensor ID = 0x%04x\n", OV13850_WRITE_ID,*sensorID);
            	break;
        	}
        OV13850DB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
        retry--;
    } while (retry > 0);

    if (*sensorID != OV13850_SENSOR_ID) {
		OV13850_WRITE_ID=OV13850MIPI_WRITE_ID_1;
		OV13850_write_cmos_sensor(0x0103,0x01);// Reset sensor
	    mDELAY(10);
        retry = 1;
	    // check if sensor ID correct
	    do {
	        *sensorID = (OV13850_read_cmos_sensor(0x300A)<<8)|OV13850_read_cmos_sensor(0x300B);
	        if (*sensorID == OV13850_SENSOR_ID)
	        	{
	        		OV13850DB("write id=%x,Sensor ID = 0x%04x\n",OV13850_WRITE_ID, *sensorID);
	            	break;
	        	}
	        OV13850DB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
	        retry--;
	    } while (retry > 0);
		 if (*sensorID != OV13850_SENSOR_ID) 
		 {
		 
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
		 	}
    }
    return ERROR_NONE;
}


void OV13850_SetShutter(kal_uint32 iShutter)
{
	if(MSDK_SCENARIO_ID_CAMERA_ZSD == OV13850CurrentScenarioId )
	{
		//OV13850DB("always UPDATE SHUTTER when ov13850.sensorMode == SENSOR_MODE_CAPTURE\n");
	}
	else{
		if(ov13850.sensorMode == SENSOR_MODE_CAPTURE)
		{
			//OV13850DB("capture!!DONT UPDATE SHUTTER!!\n");
			//return;
		}
	}
	//if(ov13850.shutter == iShutter)
		//return;
   spin_lock(&ov13850mipiraw_drv_lock);
   ov13850.shutter= iShutter;
   spin_unlock(&ov13850mipiraw_drv_lock);
   OV13850_write_shutter(iShutter);
   return;
}   /*  OV13850_SetShutter   */

UINT32 OV13850_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2 ,temp_reg3;
	UINT32 shutter =0;
	temp_reg1 = OV13850_read_cmos_sensor(0x3500);    // AEC[b19~b16]
	temp_reg2 = OV13850_read_cmos_sensor(0x3501);    // AEC[b15~b8]
	temp_reg3 = OV13850_read_cmos_sensor(0x3502);    // AEC[b7~b0]
	shutter  = (temp_reg1 <<12)| (temp_reg2<<4)|(temp_reg3>>4);

	return shutter;
}

void OV13850_NightMode(kal_bool bEnable)
{}


UINT32 OV13850Close(void)
{
	#ifdef REG_OPT_IMPROVE
	OV13850DB("%s + g_pdma_buf=%p", __func__, g_pdma_buf);
	if (g_pdma_buf) {
		dma_free_coherent(0, 1024, g_pdma_buf, g_phyAddr);
		g_pdma_buf= NULL;
	}
	#endif
	return ERROR_NONE;
}

void OV13850SetFlipMirror(kal_int32 imgMirror)
{
	kal_int16 mirror=0,flip=0;
    flip = OV13850_read_cmos_sensor(0x3820);
	mirror   = OV13850_read_cmos_sensor(0x3821);
    switch (imgMirror)
    {
        case IMAGE_NORMAL://IMAGE_NORMAL:
            OV13850_write_cmos_sensor(0x3820, (flip & (0xFB)));//Set normal 0xBD--->0xbc  for capture size
            OV13850_write_cmos_sensor(0x3821, (mirror  & (0xFB)));	//Set normal  0xf9-->0xf8  for capture size
            break;
        case IMAGE_H_MIRROR://IMAGE_H_MIRROR:
            OV13850_write_cmos_sensor(0x3820, (flip & (0xFB)));//Set normal  0xbd--->0xbc  for capture size
            OV13850_write_cmos_sensor(0x3821, (mirror  | (0x04)));	//Set mirror
            break;
        case IMAGE_V_MIRROR://IMAGE_V_MIRROR:
            OV13850_write_cmos_sensor(0x3820, (flip |(0x04)));	//Set flip
            OV13850_write_cmos_sensor(0x3821, (mirror  & (0xFB)));	//Set normal //0xf9-->0xf8  for capture size
            break;
        case IMAGE_HV_MIRROR://IMAGE_HV_MIRROR:
            OV13850_write_cmos_sensor(0x3820, (flip |(0x4)));	//Set flip
            OV13850_write_cmos_sensor(0x3821, (mirror  |(0x04)));	//Set mirror
            break;
    }
}

UINT32 OV13850Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	OV13850DB("OV13850Preview enter:");

	// preview size
	if(ov13850.sensorMode == SENSOR_MODE_PREVIEW)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		OV13850DB("OV13850Preview setting!!\n");
		OV13850PreviewSetting();
		//mdelay(30);
	}
	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	ov13850.DummyPixels = 0;//define dummy pixels and lines
	ov13850.DummyLines = 0 ;
	OV13850_FeatureControl_PERIOD_PixelNum=OV13850_PV_PERIOD_PIXEL_NUMS+ ov13850.DummyPixels;
	OV13850_FeatureControl_PERIOD_LineNum=OV13850_PV_PERIOD_LINE_NUMS+ov13850.DummyLines;
	spin_unlock(&ov13850mipiraw_drv_lock);

	//OV13850_write_shutter(ov13850.shutter);
	//write_OV13850_gain(ov13850.pvGain);
	//set mirror & flip
	//OV13850DB("[OV13850Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&ov13850mipiraw_drv_lock);
	OV13850SetFlipMirror(OV13850_ORIENTATION);
	OV13850DB("OV13850Preview exit: \n");
    return ERROR_NONE;
}	/* OV13850Preview() */


UINT32 OV13850Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV13850DB("OV13850Video enter:");
	if(ov13850.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
		OV13850VideoSetting();
	
	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.sensorMode = SENSOR_MODE_VIDEO;
	OV13850_FeatureControl_PERIOD_PixelNum=OV13850_VIDEO_PERIOD_PIXEL_NUMS+ ov13850.DummyPixels;
	OV13850_FeatureControl_PERIOD_LineNum=OV13850_VIDEO_PERIOD_LINE_NUMS+ov13850.DummyLines;
	spin_unlock(&ov13850mipiraw_drv_lock);

	//OV13850_write_shutter(ov13850.shutter);
	//write_OV13850_gain(ov13850.pvGain);

	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&ov13850mipiraw_drv_lock);
	OV13850SetFlipMirror(OV13850_ORIENTATION);

	OV13850DB("[OV13850Video]frame_len=%x\n", ((OV13850_read_cmos_sensor(0x380e)<<8)+OV13850_read_cmos_sensor(0x380f)));

	OV13850DB("OV13850Video exit:\n");
    return ERROR_NONE;
}


UINT32 OV13850Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
 	kal_uint32 shutter = ov13850.shutter;
	kal_uint32 temp_data;
	if( SENSOR_MODE_CAPTURE== ov13850.sensorMode)
	{
		OV13850DB("OV13850Capture BusrtShot!!!\n");
	}else{
		OV13850DB("OV13850Capture enter:\n");
		//Record Preview shutter & gain
		shutter=OV13850_read_shutter();
		temp_data =  read_OV13850_gain();
		spin_lock(&ov13850mipiraw_drv_lock);
		ov13850.pvShutter =shutter;
		ov13850.sensorGlobalGain = temp_data;
		ov13850.pvGain =ov13850.sensorGlobalGain;
		spin_unlock(&ov13850mipiraw_drv_lock);

		OV13850DB("[OV13850Capture]ov13850.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",ov13850.shutter, shutter,ov13850.sensorGlobalGain);

		// Full size setting
		OV13850CaptureSetting();
		spin_lock(&ov13850mipiraw_drv_lock);
		ov13850.sensorMode = SENSOR_MODE_CAPTURE;
		ov13850.imgMirror = sensor_config_data->SensorImageMirror;
		ov13850.DummyPixels = 0;//define dummy pixels and lines                                                                                                         
		ov13850.DummyLines = 0 ;    
		OV13850_FeatureControl_PERIOD_PixelNum = OV13850_FULL_PERIOD_PIXEL_NUMS + ov13850.DummyPixels;
		OV13850_FeatureControl_PERIOD_LineNum = OV13850_FULL_PERIOD_LINE_NUMS + ov13850.DummyLines;
		spin_unlock(&ov13850mipiraw_drv_lock);

		//OV13850DB("[OV13850Capture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);
		OV13850SetFlipMirror(OV13850_ORIENTATION);

	    if(OV13850CurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
	    {
			OV13850DB("OV13850Capture exit ZSD!!\n");
			return ERROR_NONE;
	    }
		OV13850DB("OV13850Capture exit:\n");
	}

    return ERROR_NONE;
}	/* OV13850Capture() */

UINT32 OV13850GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    OV13850DB("OV13850GetResolution!!\n");
	pSensorResolution->SensorPreviewWidth	= OV13850_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= OV13850_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorFullWidth		= OV13850_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= OV13850_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorVideoWidth		= OV13850_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = OV13850_IMAGE_SENSOR_VIDEO_HEIGHT;
    return ERROR_NONE;
}   /* OV13850GetResolution() */

UINT32 OV13850GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	pSensorInfo->SensorPreviewResolutionX= OV13850_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY= OV13850_IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX= OV13850_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= OV13850_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&ov13850mipiraw_drv_lock);
	ov13850.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&ov13850mipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_B;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
    //pSensorInfo->MIPIsensorType = MIPI_OPHY_CSI2;
    pSensorInfo->CaptureDelayFrame = 3;
    pSensorInfo->PreviewDelayFrame = 2;//2;
    pSensorInfo->VideoDelayFrame = 2;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=  24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV13850_PV_X_START;
            pSensorInfo->SensorGrabStartY = OV13850_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 32;//23;for 82
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV13850_VIDEO_X_START;
            pSensorInfo->SensorGrabStartY = OV13850_VIDEO_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 32;//23;for 82
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV13850_FULL_X_START;	//2*OV13850_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = OV13850_FULL_Y_START;	//2*OV13850_IMAGE_SENSOR_PV_STARTY;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 32;//23;for 82
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV13850_PV_X_START;
            pSensorInfo->SensorGrabStartY = OV13850_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 32;//23;for 82
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }

    memcpy(pSensorConfigData, &OV13850SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* OV13850GetInfo() */


UINT32 OV13850Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&ov13850mipiraw_drv_lock);
		OV13850CurrentScenarioId = ScenarioId;
		spin_unlock(&ov13850mipiraw_drv_lock);
		OV13850DB("OV13850CurrentScenarioId=%d\n",OV13850CurrentScenarioId);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            OV13850Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			OV13850Video(pImageWindow, pSensorConfigData);
			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            OV13850Capture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
} /* OV13850Control() */


UINT32 OV13850SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MIN_Frame_length =0,frameRate=0,extralines=0;
    OV13850DB("[OV13850SetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&ov13850mipiraw_drv_lock);
	 ov13850VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&ov13850mipiraw_drv_lock);

	if(u2FrameRate==0)
	{
		OV13850DB("Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    OV13850DB("error frame rate seting\n");

    if(ov13850.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(ov13850.OV13850AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 306;
			else if(u2FrameRate==15)
				frameRate= 148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (OV13850MIPI_VIDEO_CLK)/(OV13850_VIDEO_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (OV13850MIPI_VIDEO_CLK) /(OV13850_VIDEO_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=OV13850_VIDEO_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = OV13850_VIDEO_PERIOD_LINE_NUMS;
			OV13850DB("[OV13850SetVideoMode]current fps = %d\n", (OV13850MIPI_PREVIEW_CLK)  /(OV13850_PV_PERIOD_PIXEL_NUMS)/OV13850_PV_PERIOD_LINE_NUMS);
		}
		//OV13850DB("[OV13850SetVideoMode]current fps (10 base)= %d\n", (OV13850MIPI_PREVIEW_CLK)*10/(OV13850_PV_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/MIN_Frame_length);
		if(ov13850.shutter+4 > MIN_Frame_length) 
			MIN_Frame_length = ov13850.shutter + 4;
		extralines = MIN_Frame_length - OV13850_VIDEO_PERIOD_LINE_NUMS;

		spin_lock(&ov13850mipiraw_drv_lock);
		ov13850.DummyPixels = 0;//define dummy pixels and lines
		ov13850.DummyLines = extralines ;
		spin_unlock(&ov13850mipiraw_drv_lock);
		
		OV13850_SetDummy(ov13850.DummyPixels,extralines);
    }
	else if(ov13850.sensorMode == SENSOR_MODE_CAPTURE)
	{
		OV13850DB("-------[OV13850SetVideoMode]ZSD???---------\n");
		if(ov13850.OV13850AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==15)
			    frameRate= 148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (OV13850MIPI_CAPTURE_CLK) /(OV13850_FULL_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (OV13850MIPI_CAPTURE_CLK) /(OV13850_FULL_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=OV13850_FULL_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = OV13850_FULL_PERIOD_LINE_NUMS;
			//OV13850DB("[OV13850SetVideoMode]current fps = %d\n", (OV13850MIPI_CAPTURE_CLK) /(OV13850_FULL_PERIOD_PIXEL_NUMS)/OV13850_FULL_PERIOD_LINE_NUMS);

		}
		//OV13850DB("[OV13850SetVideoMode]current fps (10 base)= %d\n", (OV13850MIPI_CAPTURE_CLK)*10/(OV13850_FULL_PERIOD_PIXEL_NUMS + ov13850.DummyPixels)/MIN_Frame_length);
		if(ov13850.shutter+4 > MIN_Frame_length) 
			MIN_Frame_length = ov13850.shutter + 4;
		extralines = MIN_Frame_length - OV13850_FULL_PERIOD_LINE_NUMS;

		spin_lock(&ov13850mipiraw_drv_lock);
		ov13850.DummyPixels = 0;//define dummy pixels and lines
		ov13850.DummyLines = extralines ;
		spin_unlock(&ov13850mipiraw_drv_lock);

		OV13850_SetDummy(ov13850.DummyPixels,extralines);
	}
	OV13850DB("[OV13850SetVideoMode]MIN_Frame_length=%d,ov13850.DummyLines=%d\n",MIN_Frame_length,ov13850.DummyLines);

    return KAL_TRUE;
}

UINT32 OV13850SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	//return ERROR_NONE;
    //OV13850DB("[OV13850SetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
	if(bEnable) {   // enable auto flicker
		spin_lock(&ov13850mipiraw_drv_lock);
		ov13850.OV13850AutoFlickerMode = KAL_TRUE;
		spin_unlock(&ov13850mipiraw_drv_lock);
    } else {
    	spin_lock(&ov13850mipiraw_drv_lock);
        ov13850.OV13850AutoFlickerMode = KAL_FALSE;
		spin_unlock(&ov13850mipiraw_drv_lock);
        OV13850DB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 OV13850SetTestPatternMode(kal_bool bEnable)
{
    OV13850DB("[OV13850SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	if(bEnable)
		{
		   OV13850_write_cmos_sensor(0x5E00, 0x80);
		}
		else
		{
		
			OV13850_write_cmos_sensor(0x5E00, 0x00);
		}

    return ERROR_NONE;
}


UINT32 OV13850MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		//scenarioId = OV13850CurrentScenarioId;
	OV13850DB("OV13850MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = OV13850MIPI_PREVIEW_CLK;
			lineLength = OV13850_PV_PERIOD_PIXEL_NUMS;
			//frameHeight = (10 * pclk)/frameRate/lineLength;
			frameHeight = ((pclk/frameRate)*10)/lineLength;
			dummyLine = frameHeight - OV13850_PV_PERIOD_LINE_NUMS;
			ov13850.sensorMode = SENSOR_MODE_PREVIEW;
			OV13850_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = OV13850MIPI_VIDEO_CLK; 
			lineLength = OV13850_VIDEO_PERIOD_PIXEL_NUMS;
			//frameHeight = (10 * pclk)/frameRate/lineLength;
			frameHeight = ((pclk/frameRate)*10)/lineLength;
			dummyLine = frameHeight - OV13850_VIDEO_PERIOD_LINE_NUMS;
			ov13850.sensorMode = SENSOR_MODE_VIDEO;
			OV13850_SetDummy(0, dummyLine);			
			break;			
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = OV13850MIPI_CAPTURE_CLK;
			lineLength = OV13850_FULL_PERIOD_PIXEL_NUMS;
			//frameHeight = (10 * pclk)/frameRate/lineLength;
			frameHeight = ((pclk/frameRate)*10)/lineLength;
			dummyLine = frameHeight - OV13850_FULL_PERIOD_LINE_NUMS;
			ov13850.sensorMode = SENSOR_MODE_CAPTURE;
			OV13850_SetDummy(0, dummyLine);			
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


UINT32 OV13850MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{ 

	//scenarioId = OV13850CurrentScenarioId;
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 240;
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


UINT32 OV13850FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
	OV13850DB(" OV13850FeatureControl is %d \n", FeatureId);

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= OV13850_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= OV13850_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
				*pFeatureReturnPara16++= OV13850_FeatureControl_PERIOD_PixelNum;
				*pFeatureReturnPara16= OV13850_FeatureControl_PERIOD_LineNum;
				*pFeatureParaLen=4;
				break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(OV13850CurrentScenarioId)
			{
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = OV13850MIPI_PREVIEW_CLK;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = OV13850MIPI_VIDEO_CLK;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = OV13850MIPI_CAPTURE_CLK;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = OV13850MIPI_CAPTURE_CLK;
					*pFeatureParaLen=4;
					break;
			}
		      break;

        case SENSOR_FEATURE_SET_ESHUTTER:
            OV13850_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            OV13850_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            OV13850_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //OV13850_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            OV13850_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = OV13850_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&ov13850mipiraw_drv_lock);
                OV13850SensorCCT[i].Addr=*pFeatureData32++;
                OV13850SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&ov13850mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=OV13850SensorCCT[i].Addr;
                *pFeatureData32++=OV13850SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&ov13850mipiraw_drv_lock);
                OV13850SensorReg[i].Addr=*pFeatureData32++;
                OV13850SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&ov13850mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=OV13850SensorReg[i].Addr;
                *pFeatureData32++=OV13850SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=OV13850_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, OV13850SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, OV13850SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &OV13850SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            OV13850_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            OV13850_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
			//no this item,cct may be connect fail.
            *pFeatureReturnPara32++=OV13850_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            OV13850_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            OV13850_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            OV13850_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
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
            OV13850SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV13850GetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            OV13850SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV13850SetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
			*pFeatureReturnPara32=OV13850_TEST_PATTERN_CHECKSUM;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV13850MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV13850MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
        default:
            break;
    }
    return ERROR_NONE;
}	/* OV13850FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncOV13850=
{
    OV13850Open,
    OV13850GetInfo,
    OV13850GetResolution,
    OV13850FeatureControl,
    OV13850Control,
    OV13850Close
};

UINT32 OV13850_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncOV13850;

    return ERROR_NONE;
}   /* SensorInit() */
