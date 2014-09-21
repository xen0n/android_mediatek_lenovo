/*****************************************************************************
 *
 * Filename:
 * ---------
 *   OV5693mipi_Sensor.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
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
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov5693mipi_Sensor.h"
#include "ov5693mipi_Camera_Sensor_para.h"
#include "ov5693mipi_CameraCustomized.h"

#define OV5693MIPI_DRIVER_TRACE
#define OV5693MIPI_DEBUG
#ifdef OV5693MIPI_DEBUG
#define SENSORDB(fmt, arg...) printk("%s: " fmt "\n", __FUNCTION__ ,##arg)
#else
#define SENSORDB(x,...)
#endif

typedef enum
{
    OV5693MIPI_SENSOR_MODE_INIT,
    OV5693MIPI_SENSOR_MODE_PREVIEW,  
    OV5693MIPI_SENSOR_MODE_CAPTURE,
    OV5693MIPI_SENSOR_MODE_VIDEO,
} OV5693MIPI_SENSOR_MODE;

/* SENSOR PRIVATE STRUCT */
typedef struct OV5693MIPI_sensor_STRUCT
{
    MSDK_SENSOR_CONFIG_STRUCT cfg_data;
    sensor_data_struct eng; /* engineer mode */
    MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
    kal_uint8 mirror;

    OV5693MIPI_SENSOR_MODE sensor_mode;
    
    kal_bool video_mode;
    kal_bool NightMode;
    kal_uint16 normal_fps; /* video normal mode max fps */
    kal_uint16 night_fps; /* video night mode max fps */
    kal_uint16 FixedFps;
    kal_uint16 shutter;
    kal_uint16 gain;
    kal_uint32 pclk;
    kal_uint16 frame_length;
    kal_uint16 line_length;

    kal_uint16 dummy_pixel;
    kal_uint16 dummy_line;

	kal_uint8 i2c_write_id;
	kal_uint8 i2c_read_id;
} OV5693MIPI_sensor_struct;


static MSDK_SCENARIO_ID_ENUM mCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
static kal_bool OV5693MIPIAutoFlickerMode = KAL_FALSE;

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

void OV5693MIPISetMaxFrameRate(UINT16 u2FrameRate);

static DEFINE_SPINLOCK(ov5693mipi_drv_lock);


static OV5693MIPI_sensor_struct OV5693MIPI_sensor =
{
    .eng =
    {
        .reg = CAMERA_SENSOR_REG_DEFAULT_VALUE,
        .cct = CAMERA_SENSOR_CCT_DEFAULT_VALUE,
    },
    .eng_info =
    {
        .SensorId = 128,
        .SensorType = CMOS_SENSOR,
        .SensorOutputDataFormat = OV5693MIPI_COLOR_FORMAT,
    },
    .sensor_mode = OV5693MIPI_SENSOR_MODE_INIT,
    .shutter = 0x3D0,  
    .gain = 0x100,
    .pclk = OV5693MIPI_PREVIEW_CLK,
    .frame_length = OV5693MIPI_PV_PERIOD_LINE_NUMS,
    .line_length = OV5693MIPI_PV_PERIOD_PIXEL_NUMS,
    .dummy_pixel = 0,
    .dummy_line = 0,

    .i2c_write_id = 0x6c,
    .i2c_read_id = 0x6d,
};

kal_bool OV5693MIPIDuringTestPattern = KAL_FALSE;
#define OV5693MIPI_TEST_PATTERN_CHECKSUM (0xf7375923)


kal_uint16 OV5693MIPI_read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;

    char puSendCmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(puSendCmd, 2, (u8*)&get_byte, 1, OV5693MIPI_sensor.i2c_write_id);

    return get_byte;
}

void OV5693MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char puSendCmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(puSendCmd, 3, OV5693MIPI_sensor.i2c_write_id);
}

static void OV5693MIPI_Write_Shutter(kal_uint16 iShutter)
{
#if 0
    kal_uint16 extra_line = 0;
#endif
    
    kal_uint16 frame_length = 0;

    #ifdef OV5693MIPI_DRIVER_TRACE
        SENSORDB("iShutter =  %d", iShutter);
    #endif
    
    /* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
    /* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
    if (!iShutter) iShutter = 1;

    if(OV5693MIPIAutoFlickerMode){
        if(OV5693MIPI_sensor.video_mode == KAL_FALSE){
            if(mCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
                OV5693MIPISetMaxFrameRate(296);
            else
                OV5693MIPISetMaxFrameRate(296);
        } 
        else
        {
            if(OV5693MIPI_sensor.FixedFps == 30)
                OV5693MIPISetMaxFrameRate(300);
            else if (OV5693MIPI_sensor.FixedFps == 15)
                OV5693MIPISetMaxFrameRate(150);
        }
    }
    else
    {
        if(OV5693MIPI_sensor.video_mode == KAL_FALSE){
            if(mCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
                OV5693MIPISetMaxFrameRate(300);
            else
                OV5693MIPISetMaxFrameRate(300);
        } 
        else
        {
            if(OV5693MIPI_sensor.FixedFps == 30)
                OV5693MIPISetMaxFrameRate(300);
            else if (OV5693MIPI_sensor.FixedFps == 15)
                OV5693MIPISetMaxFrameRate(150);
        }    
    }

    // OV Recommend Solution
    // if shutter bigger than frame_length, should extend frame length first
#if 0

	if(iShutter > (OV5693MIPI_sensor.frame_length - 4))
		extra_line = iShutter - (OV5693MIPI_sensor.frame_length - 4);
	else
	    extra_line = 0;

	// Update Extra shutter
	OV5693MIPI_write_cmos_sensor(0x350c, (extra_line >> 8) & 0xFF);	
	OV5693MIPI_write_cmos_sensor(0x350d, (extra_line) & 0xFF); 
	
#endif

#if 1  

    if(iShutter > OV5693MIPI_sensor.frame_length - 4)
        frame_length = iShutter + 4;
    else
        frame_length = OV5693MIPI_sensor.frame_length;

    // Extend frame length
    OV5693MIPI_write_cmos_sensor(0x380e, frame_length >> 8);
    OV5693MIPI_write_cmos_sensor(0x380f, frame_length & 0xFF);
    
#endif

    // Update Shutter
    OV5693MIPI_write_cmos_sensor(0x3502, (iShutter << 4) & 0xFF);
    OV5693MIPI_write_cmos_sensor(0x3501, (iShutter >> 4) & 0xFF);     
    OV5693MIPI_write_cmos_sensor(0x3500, (iShutter >> 12) & 0x0F);  

    //SENSORDB("frame_length = %d ", frame_length);
    
}   /*  OV5693MIPI_Write_Shutter  */

static void OV5693MIPI_Set_Dummy(const kal_uint16 iDummyPixels, const kal_uint16 iDummyLines)
{
    kal_uint16 line_length, frame_length;

    #ifdef OV5693MIPI_DRIVER_TRACE
        SENSORDB("iDummyPixels = %d, iDummyLines = %d ", iDummyPixels, iDummyLines);
    #endif

    if (OV5693MIPI_SENSOR_MODE_PREVIEW == OV5693MIPI_sensor.sensor_mode)
    {
        line_length = OV5693MIPI_PV_PERIOD_PIXEL_NUMS + iDummyPixels;
        frame_length = OV5693MIPI_PV_PERIOD_LINE_NUMS + iDummyLines;
    }
    else if(OV5693MIPI_SENSOR_MODE_VIDEO == OV5693MIPI_sensor.sensor_mode)
    {
        line_length = OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS + iDummyPixels;
        frame_length = OV5693MIPI_VIDEO_PERIOD_LINE_NUMS + iDummyLines;
    }    
    else
    {
        line_length = OV5693MIPI_FULL_PERIOD_PIXEL_NUMS + iDummyPixels;
        frame_length = OV5693MIPI_FULL_PERIOD_LINE_NUMS + iDummyLines;
    }

    OV5693MIPI_sensor.dummy_pixel = iDummyPixels;
    OV5693MIPI_sensor.dummy_line = iDummyLines;
    OV5693MIPI_sensor.line_length = line_length;
    OV5693MIPI_sensor.frame_length = frame_length;
    
    OV5693MIPI_write_cmos_sensor(0x380e, frame_length >> 8);
    OV5693MIPI_write_cmos_sensor(0x380f, frame_length & 0xFF);     
    OV5693MIPI_write_cmos_sensor(0x380c, line_length >> 8);
    OV5693MIPI_write_cmos_sensor(0x380d, line_length & 0xFF);
  
}   /*  OV5693MIPI_Set_Dummy  */


void OV5693MIPISetMaxFrameRate(UINT16 u2FrameRate)
{
    kal_int16 dummy_line;
    kal_uint16 frame_length = OV5693MIPI_sensor.frame_length;
    unsigned long flags;

    #ifdef OV5693MIPI_DRIVER_TRACE
        SENSORDB("u2FrameRate = %d ", u2FrameRate);
    #endif

    frame_length= (10 * OV5693MIPI_sensor.pclk) / u2FrameRate / OV5693MIPI_sensor.line_length;

    spin_lock_irqsave(&ov5693mipi_drv_lock, flags);
    OV5693MIPI_sensor.frame_length = frame_length;
    spin_unlock_irqrestore(&ov5693mipi_drv_lock, flags);

    if (OV5693MIPI_SENSOR_MODE_PREVIEW == OV5693MIPI_sensor.sensor_mode)
    {
        dummy_line = frame_length - OV5693MIPI_PV_PERIOD_LINE_NUMS;
    }
    else if(OV5693MIPI_SENSOR_MODE_VIDEO == OV5693MIPI_sensor.sensor_mode)
    {
        dummy_line = frame_length - OV5693MIPI_VIDEO_PERIOD_LINE_NUMS;
    }    
    else
    {
        dummy_line = frame_length - OV5693MIPI_FULL_PERIOD_LINE_NUMS;
    }

#if 0
    if(mCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
        dummy_line = frame_length - OV5693MIPI_FULL_PERIOD_LINE_NUMS;
    else
        dummy_line = frame_length - OV5693MIPI_PV_PERIOD_LINE_NUMS;
#endif

    if (dummy_line < 0) dummy_line = 0;

    OV5693MIPI_Set_Dummy(OV5693MIPI_sensor.dummy_pixel, dummy_line); /* modify dummy_pixel must gen AE table again */   
}   /*  OV5693MIPISetMaxFrameRate  */


/*************************************************************************
* FUNCTION
*   OV5693MIPI_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of OV5693MIPI to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void set_OV5693MIPI_shutter(kal_uint16 iShutter)
{
    unsigned long flags;
    
    spin_lock_irqsave(&ov5693mipi_drv_lock, flags);
    OV5693MIPI_sensor.shutter = iShutter;
    spin_unlock_irqrestore(&ov5693mipi_drv_lock, flags);
    
    OV5693MIPI_Write_Shutter(iShutter);
}   /*  Set_OV5693MIPI_Shutter */

#if 0
static kal_uint16 OV5693MIPI_Reg2Gain(const kal_uint8 iReg)
{
    kal_uint16 iGain ;
    /* Range: 1x to 32x */
    iGain = (iReg >> 4) * BASEGAIN + (iReg & 0xF) * BASEGAIN / 16; 
    return iGain ;
}
#endif


 kal_uint16 OV5693MIPI_Gain2Reg(const kal_uint16 iGain)
{
    kal_uint16 iReg = 0x0000;
    
    iReg = ((iGain / BASEGAIN) << 4) + ((iGain % BASEGAIN) * 16 / BASEGAIN);
    iReg = iReg & 0xFFFF;
    return (kal_uint16)iReg;
}

/*************************************************************************
* FUNCTION
*   OV5693MIPI_SetGain
*
* DESCRIPTION
*   This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*   the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 OV5693MIPI_SetGain(kal_uint16 iGain)
{
    kal_uint16 iRegGain;

    OV5693MIPI_sensor.gain = iGain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X  */
    /* [4:9] = M meams M X       */
    /* Total gain = M + N /16 X   */

    //
    if(iGain < BASEGAIN || iGain > 32 * BASEGAIN){
        SENSORDB("Error gain setting");

        if(iGain < BASEGAIN) iGain = BASEGAIN;
        if(iGain > 32 * BASEGAIN) iGain = 32 * BASEGAIN;        
    }
 
    iRegGain = OV5693MIPI_Gain2Reg(iGain);

    #ifdef OV5693MIPI_DRIVER_TRACE
        SENSORDB("iGain = %d , iRegGain = 0x%x ", iGain, iRegGain);
    #endif

    OV5693MIPI_write_cmos_sensor(0x350a, iRegGain >> 8);
    OV5693MIPI_write_cmos_sensor(0x350b, iRegGain & 0xFF);    
    
    return iGain;
}   /*  OV5693MIPI_SetGain  */



void OV5693MIPI_Set_Mirror_Flip(kal_uint8 image_mirror)
{
    SENSORDB("image_mirror = %d", image_mirror);

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    
	switch (image_mirror)
	{
		case IMAGE_NORMAL:
		    OV5693MIPI_write_cmos_sensor(0x3820,((OV5693MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		    OV5693MIPI_write_cmos_sensor(0x3821,((OV5693MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		    break;
		case IMAGE_H_MIRROR:
		    OV5693MIPI_write_cmos_sensor(0x3820,((OV5693MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		    OV5693MIPI_write_cmos_sensor(0x3821,((OV5693MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		    break;
		case IMAGE_V_MIRROR:
		    OV5693MIPI_write_cmos_sensor(0x3820,((OV5693MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		    OV5693MIPI_write_cmos_sensor(0x3821,((OV5693MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x06));		
		    break;
		case IMAGE_HV_MIRROR:
		    OV5693MIPI_write_cmos_sensor(0x3820,((OV5693MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		    OV5693MIPI_write_cmos_sensor(0x3821,((OV5693MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		    break;
		default:
		    SENSORDB("Error image_mirror setting");
	}

}

/*************************************************************************
* FUNCTION
*   OV5693MIPI_NightMode
*
* DESCRIPTION
*   This function night mode of OV5693MIPI.
*
* PARAMETERS
*   bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV5693MIPI_night_mode(kal_bool enable)
{
/*No Need to implement this function*/ 
}   /*  OV5693MIPI_night_mode  */


/* write camera_para to sensor register */
static void OV5693MIPI_camera_para_to_sensor(void)
{
    kal_uint32 i;

    SENSORDB("OV5693MIPI_camera_para_to_sensor\n");

    for (i = 0; 0xFFFFFFFF != OV5693MIPI_sensor.eng.reg[i].Addr; i++)
    {
        OV5693MIPI_write_cmos_sensor(OV5693MIPI_sensor.eng.reg[i].Addr, OV5693MIPI_sensor.eng.reg[i].Para);
    }
    for (i = OV5693MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV5693MIPI_sensor.eng.reg[i].Addr; i++)
    {
        OV5693MIPI_write_cmos_sensor(OV5693MIPI_sensor.eng.reg[i].Addr, OV5693MIPI_sensor.eng.reg[i].Para);
    }
    OV5693MIPI_SetGain(OV5693MIPI_sensor.gain); /* update gain */
}

/* update camera_para from sensor register */
static void OV5693MIPI_sensor_to_camera_para(void)
{
    kal_uint32 i,temp_data;

    SENSORDB("OV5693MIPI_sensor_to_camera_para\n");

    for (i = 0; 0xFFFFFFFF != OV5693MIPI_sensor.eng.reg[i].Addr; i++)
    {
        temp_data =OV5693MIPI_read_cmos_sensor(OV5693MIPI_sensor.eng.reg[i].Addr);
     
        spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPI_sensor.eng.reg[i].Para = temp_data;
        spin_unlock(&ov5693mipi_drv_lock);
    }
    for (i = OV5693MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV5693MIPI_sensor.eng.reg[i].Addr; i++)
    {
        temp_data =OV5693MIPI_read_cmos_sensor(OV5693MIPI_sensor.eng.reg[i].Addr);
    
        spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPI_sensor.eng.reg[i].Para = temp_data;
        spin_unlock(&ov5693mipi_drv_lock);
    }
}

/* ------------------------ Engineer mode ------------------------ */
inline static void OV5693MIPI_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{

    SENSORDB("OV5693MIPI_get_sensor_group_count\n");

    *sensor_count_ptr = OV5693MIPI_GROUP_TOTAL_NUMS;
}

inline static void OV5693MIPI_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{

    SENSORDB("OV5693MIPI_get_sensor_group_info\n");

    switch (para->GroupIdx)
    {
    case OV5693MIPI_PRE_GAIN:
        sprintf(para->GroupNamePtr, "CCT");
        para->ItemCount = 5;
        break;
    case OV5693MIPI_CMMCLK_CURRENT:
        sprintf(para->GroupNamePtr, "CMMCLK Current");
        para->ItemCount = 1;
        break;
    case OV5693MIPI_FRAME_RATE_LIMITATION:
        sprintf(para->GroupNamePtr, "Frame Rate Limitation");
        para->ItemCount = 2;
        break;
    case OV5693MIPI_REGISTER_EDITOR:
        sprintf(para->GroupNamePtr, "Register Editor");
        para->ItemCount = 2;
        break;
    default:
        ASSERT(0);
  }
}

inline static void OV5693MIPI_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

    const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
    const static kal_char *editer_item_name[] = {"REG addr", "REG value"};
  
    SENSORDB("OV5693MIPI_get_sensor_item_info");

    switch (para->GroupIdx)
    {
    case OV5693MIPI_PRE_GAIN:
        switch (para->ItemIdx)
        {
        case OV5693MIPI_SENSOR_BASEGAIN:
        case OV5693MIPI_PRE_GAIN_R_INDEX:
        case OV5693MIPI_PRE_GAIN_Gr_INDEX:
        case OV5693MIPI_PRE_GAIN_Gb_INDEX:
        case OV5693MIPI_PRE_GAIN_B_INDEX:
            break;
        default:
            ASSERT(0);
        }
        sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - OV5693MIPI_SENSOR_BASEGAIN]);
        para->ItemValue = OV5693MIPI_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
        para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
        para->Min = OV5693MIPI_MIN_ANALOG_GAIN * 1000;
        para->Max = OV5693MIPI_MAX_ANALOG_GAIN * 1000;
        break;
    case OV5693MIPI_CMMCLK_CURRENT:
        switch (para->ItemIdx)
        {
        case 0:
            sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
            switch (OV5693MIPI_sensor.eng.reg[OV5693MIPI_CMMCLK_CURRENT_INDEX].Para)
            {
            case ISP_DRIVING_2MA:
                para->ItemValue = 2;
                break;
            case ISP_DRIVING_4MA:
                para->ItemValue = 4;
                break;
            case ISP_DRIVING_6MA:
                para->ItemValue = 6;
                break;
            case ISP_DRIVING_8MA:
                para->ItemValue = 8;
                break;
            default:
                ASSERT(0);
            }
            para->IsTrueFalse = para->IsReadOnly = KAL_FALSE;
            para->IsNeedRestart = KAL_TRUE;
            para->Min = 2;
            para->Max = 8;
            break;
        default:
            ASSERT(0);
        }
        break;
    case OV5693MIPI_FRAME_RATE_LIMITATION:
        switch (para->ItemIdx)
        {
        case 0:
            sprintf(para->ItemNamePtr, "Max Exposure Lines");
            para->ItemValue = 5998;
            break;
        case 1:
            sprintf(para->ItemNamePtr, "Min Frame Rate");
            para->ItemValue = 5;
            break;
        default:
            ASSERT(0);
        }
        para->IsTrueFalse = para->IsNeedRestart = KAL_FALSE;
        para->IsReadOnly = KAL_TRUE;
        para->Min = para->Max = 0;
        break;
    case OV5693MIPI_REGISTER_EDITOR:
        switch (para->ItemIdx)
        {
        case 0:
        case 1:
            sprintf(para->ItemNamePtr, editer_item_name[para->ItemIdx]);
            para->ItemValue = 0;
            para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
            para->Min = 0;
            para->Max = (para->ItemIdx == 0 ? 0xFFFF : 0xFF);
            break;
        default:
            ASSERT(0);
        }
        break;
    default:
        ASSERT(0);
  }
}

inline static kal_bool OV5693MIPI_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
    kal_uint16 temp_para;

    SENSORDB("OV5693MIPI_set_sensor_item_info\n");

    switch (para->GroupIdx)
    {
    case OV5693MIPI_PRE_GAIN:
        switch (para->ItemIdx)
        {
        case OV5693MIPI_SENSOR_BASEGAIN:
        case OV5693MIPI_PRE_GAIN_R_INDEX:
        case OV5693MIPI_PRE_GAIN_Gr_INDEX:
        case OV5693MIPI_PRE_GAIN_Gb_INDEX:
        case OV5693MIPI_PRE_GAIN_B_INDEX:
            spin_lock(&ov5693mipi_drv_lock);
            OV5693MIPI_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
            spin_unlock(&ov5693mipi_drv_lock);
            OV5693MIPI_SetGain(OV5693MIPI_sensor.gain); /* update gain */
            break;
        default:
            ASSERT(0);
        }
        break;
    case OV5693MIPI_CMMCLK_CURRENT:
        switch (para->ItemIdx)
        {
        case 0:
            switch (para->ItemValue)
            {
            case 2:
                temp_para = ISP_DRIVING_2MA;
                break;
            case 3:
            case 4:
                temp_para = ISP_DRIVING_4MA;
                break;
            case 5:
            case 6:
                temp_para = ISP_DRIVING_6MA;
                break;
            default:
                temp_para = ISP_DRIVING_8MA;
                break;
            }
            spin_lock(&ov5693mipi_drv_lock);
            //OV5693MIPI_set_isp_driving_current(temp_para);
            OV5693MIPI_sensor.eng.reg[OV5693MIPI_CMMCLK_CURRENT_INDEX].Para = temp_para;
            spin_unlock(&ov5693mipi_drv_lock);
            break;
        default:
            ASSERT(0);
        }
        break;
    case OV5693MIPI_FRAME_RATE_LIMITATION:
        ASSERT(0);
        break;
    case OV5693MIPI_REGISTER_EDITOR:
        switch (para->ItemIdx)
        {
        static kal_uint32 fac_sensor_reg;
        case 0:
            if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
            fac_sensor_reg = para->ItemValue;
            break;
        case 1:
            if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
            OV5693MIPI_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
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

static void OV5693MIPI_Sensor_Init(void)
{
    SENSORDB("Enter!");

   /*****************************************************************************
    0x3098[0:1] pll3_prediv
    pll3_prediv_map[] = {2, 3, 4, 6} 
    
    0x3099[0:4] pll3_multiplier
    pll3_multiplier
    
    0x309C[0] pll3_rdiv
    pll3_rdiv + 1
    
    0x309A[0:3] pll3_sys_div
    pll3_sys_div + 1
    
    0x309B[0:1] pll3_div
    pll3_div[] = {2, 2, 4, 5}
    
    VCO = XVCLK * 2 / pll3_prediv * pll3_multiplier * pll3_rdiv
    sysclk = VCO * 2 * 2 / pll3_sys_div / pll3_div

    XVCLK = 24 MHZ
    0x3098, 0x03
    0x3099, 0x1e
    0x309a, 0x02
    0x309b, 0x01
    0x309c, 0x00


    VCO = 24 * 2 / 6 * 31 * 1
    sysclk = VCO * 2  * 2 / 3 / 2
    sysclk = 160 MHZ
    */

    OV5693MIPI_write_cmos_sensor(0x0103,0x01);  // Software Reset

    OV5693MIPI_write_cmos_sensor(0x3001,0x0a);  // FSIN output, FREX input
    OV5693MIPI_write_cmos_sensor(0x3002,0x80);  // Vsync output, Href, Frex, strobe, fsin, ilpwm input
    OV5693MIPI_write_cmos_sensor(0x3006,0x00);  // output value of GPIO
    
    OV5693MIPI_write_cmos_sensor(0x3011,0x21);  //MIPI 2 lane enable
    OV5693MIPI_write_cmos_sensor(0x3012,0x09);  //MIPI 10-bit
    OV5693MIPI_write_cmos_sensor(0x3013,0x10);  //Drive 1x
    OV5693MIPI_write_cmos_sensor(0x3014,0x00);
    OV5693MIPI_write_cmos_sensor(0x3015,0x08);  //MIPI on
    OV5693MIPI_write_cmos_sensor(0x3016,0xf0);
    OV5693MIPI_write_cmos_sensor(0x3017,0xf0);
    OV5693MIPI_write_cmos_sensor(0x3018,0xf0);
    OV5693MIPI_write_cmos_sensor(0x301b,0xb4);
    OV5693MIPI_write_cmos_sensor(0x301d,0x02);
    OV5693MIPI_write_cmos_sensor(0x3021,0x00);  //internal regulator on
    OV5693MIPI_write_cmos_sensor(0x3022,0x01);
    OV5693MIPI_write_cmos_sensor(0x3028,0x44);
    OV5693MIPI_write_cmos_sensor(0x3098,0x03);  //PLL
    OV5693MIPI_write_cmos_sensor(0x3099,0x1e);  //PLL
    OV5693MIPI_write_cmos_sensor(0x309a,0x02);  //PLL
    OV5693MIPI_write_cmos_sensor(0x309b,0x01);  //PLL
    OV5693MIPI_write_cmos_sensor(0x309c,0x00);  //PLL
    OV5693MIPI_write_cmos_sensor(0x30a0,0xd2);
    OV5693MIPI_write_cmos_sensor(0x30a2,0x01);
    OV5693MIPI_write_cmos_sensor(0x30b2,0x00);
    OV5693MIPI_write_cmos_sensor(0x30b3,0x6a);  //PLL 50 64 6c
    OV5693MIPI_write_cmos_sensor(0x30b4,0x03);  //PLL
    OV5693MIPI_write_cmos_sensor(0x30b5,0x04);  //PLL
    OV5693MIPI_write_cmos_sensor(0x30b6,0x01);  //PLL
    OV5693MIPI_write_cmos_sensor(0x3104,0x21);  //sclk from dac_pll
    OV5693MIPI_write_cmos_sensor(0x3106,0x00);  //sclk_sel from pll_clk
    OV5693MIPI_write_cmos_sensor(0x3400,0x04);  //MWB R H
    OV5693MIPI_write_cmos_sensor(0x3401,0x00);  //MWB R L
    OV5693MIPI_write_cmos_sensor(0x3402,0x04);  //MWB G H
    OV5693MIPI_write_cmos_sensor(0x3403,0x00);  //MWB G L
    OV5693MIPI_write_cmos_sensor(0x3404,0x04);  //MWB B H
    OV5693MIPI_write_cmos_sensor(0x3405,0x00);  //MWB B L
    OV5693MIPI_write_cmos_sensor(0x3406,0x01);  //MWB gain enable
    OV5693MIPI_write_cmos_sensor(0x3500,0x00);  //exposure HH
    OV5693MIPI_write_cmos_sensor(0x3501,0x3d);  //exposure H   3d
    OV5693MIPI_write_cmos_sensor(0x3502,0x00);  //exposure L
    OV5693MIPI_write_cmos_sensor(0x3503,0x07);  //gain 1 frame latch, VTS manual, AGC manual, AEC manual
    OV5693MIPI_write_cmos_sensor(0x3504,0x00);  //manual gain H
    OV5693MIPI_write_cmos_sensor(0x3505,0x00);  //manual gain L
    OV5693MIPI_write_cmos_sensor(0x3506,0x00);  //short exposure HH
    OV5693MIPI_write_cmos_sensor(0x3507,0x02);  //short exposure H
    OV5693MIPI_write_cmos_sensor(0x3508,0x00);  //short exposure L
    OV5693MIPI_write_cmos_sensor(0x3509,0x10);  //use real ggain
    OV5693MIPI_write_cmos_sensor(0x350a,0x00);  //gain H 
    OV5693MIPI_write_cmos_sensor(0x350b,0x40);  //gain L
    OV5693MIPI_write_cmos_sensor(0x3600,0xbc);  //(20140110)
    OV5693MIPI_write_cmos_sensor(0x3601,0x0a);  //analog
    OV5693MIPI_write_cmos_sensor(0x3602,0x38);
    OV5693MIPI_write_cmos_sensor(0x3612,0x80);
    OV5693MIPI_write_cmos_sensor(0x3620,0x44);  //0x54 (20140110)
    OV5693MIPI_write_cmos_sensor(0x3621,0xb5);  //0xc7 (20140110)
    OV5693MIPI_write_cmos_sensor(0x3622,0x0c);  //0x0f (20140110)

    OV5693MIPI_write_cmos_sensor(0x3625,0x10);
    OV5693MIPI_write_cmos_sensor(0x3630,0x55);
    OV5693MIPI_write_cmos_sensor(0x3631,0xf4);
    OV5693MIPI_write_cmos_sensor(0x3632,0x00);
    OV5693MIPI_write_cmos_sensor(0x3633,0x34);
    OV5693MIPI_write_cmos_sensor(0x3634,0x02);
    OV5693MIPI_write_cmos_sensor(0x364d,0x0d);
    OV5693MIPI_write_cmos_sensor(0x364f,0xdd);
    OV5693MIPI_write_cmos_sensor(0x3660,0x04);
    OV5693MIPI_write_cmos_sensor(0x3662,0x10);
    OV5693MIPI_write_cmos_sensor(0x3663,0xf1);
    OV5693MIPI_write_cmos_sensor(0x3665,0x00);
    OV5693MIPI_write_cmos_sensor(0x3666,0x20);
    OV5693MIPI_write_cmos_sensor(0x3667,0x00);
    OV5693MIPI_write_cmos_sensor(0x366a,0x80);
    OV5693MIPI_write_cmos_sensor(0x3680,0xe0);
    OV5693MIPI_write_cmos_sensor(0x3681,0x00);  //analog
    OV5693MIPI_write_cmos_sensor(0x3700,0x42);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3701,0x14);
    OV5693MIPI_write_cmos_sensor(0x3702,0xa0);
    OV5693MIPI_write_cmos_sensor(0x3703,0xd8);
    OV5693MIPI_write_cmos_sensor(0x3704,0x78);
    OV5693MIPI_write_cmos_sensor(0x3705,0x02);
    OV5693MIPI_write_cmos_sensor(0x3708,0xe6);  //e6 //e2
    OV5693MIPI_write_cmos_sensor(0x3709,0xc7);  //c7 //c3
    OV5693MIPI_write_cmos_sensor(0x370a,0x00);
    OV5693MIPI_write_cmos_sensor(0x370b,0x20);
    OV5693MIPI_write_cmos_sensor(0x370c,0x0c);
    OV5693MIPI_write_cmos_sensor(0x370d,0x11);
    OV5693MIPI_write_cmos_sensor(0x370e,0x00);
    OV5693MIPI_write_cmos_sensor(0x370f,0x40);
    OV5693MIPI_write_cmos_sensor(0x3710,0x00);
    OV5693MIPI_write_cmos_sensor(0x371a,0x1c);
    OV5693MIPI_write_cmos_sensor(0x371b,0x05);
    OV5693MIPI_write_cmos_sensor(0x371c,0x01);
    OV5693MIPI_write_cmos_sensor(0x371e,0xa1);
    OV5693MIPI_write_cmos_sensor(0x371f,0x0c);
    OV5693MIPI_write_cmos_sensor(0x3721,0x00);
    OV5693MIPI_write_cmos_sensor(0x3724,0x10);
    OV5693MIPI_write_cmos_sensor(0x3726,0x00);
    OV5693MIPI_write_cmos_sensor(0x372a,0x01);
    OV5693MIPI_write_cmos_sensor(0x3730,0x10);
    OV5693MIPI_write_cmos_sensor(0x3738,0x22);
    OV5693MIPI_write_cmos_sensor(0x3739,0xe5);
    OV5693MIPI_write_cmos_sensor(0x373a,0x50);
    OV5693MIPI_write_cmos_sensor(0x373b,0x02);
    OV5693MIPI_write_cmos_sensor(0x373c,0x41);
    OV5693MIPI_write_cmos_sensor(0x373f,0x02);
    OV5693MIPI_write_cmos_sensor(0x3740,0x42);
    OV5693MIPI_write_cmos_sensor(0x3741,0x02);
    OV5693MIPI_write_cmos_sensor(0x3742,0x18);
    OV5693MIPI_write_cmos_sensor(0x3743,0x01);
    OV5693MIPI_write_cmos_sensor(0x3744,0x02);
    OV5693MIPI_write_cmos_sensor(0x3747,0x10);
    OV5693MIPI_write_cmos_sensor(0x374c,0x04);
    OV5693MIPI_write_cmos_sensor(0x3751,0xf0);
    OV5693MIPI_write_cmos_sensor(0x3752,0x00);
    OV5693MIPI_write_cmos_sensor(0x3753,0x00);
    OV5693MIPI_write_cmos_sensor(0x3754,0xc0);
    OV5693MIPI_write_cmos_sensor(0x3755,0x00);
    OV5693MIPI_write_cmos_sensor(0x3756,0x1a);
    OV5693MIPI_write_cmos_sensor(0x3758,0x00);
    OV5693MIPI_write_cmos_sensor(0x3759,0x0f);
    OV5693MIPI_write_cmos_sensor(0x376b,0x44);
    OV5693MIPI_write_cmos_sensor(0x375c,0x04);
    OV5693MIPI_write_cmos_sensor(0x3774,0x10);
    OV5693MIPI_write_cmos_sensor(0x3776,0x00);
    OV5693MIPI_write_cmos_sensor(0x377f,0x08);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3780,0x22);  //PSRAM control
    OV5693MIPI_write_cmos_sensor(0x3781,0x0c);
    OV5693MIPI_write_cmos_sensor(0x3784,0x2c);
    OV5693MIPI_write_cmos_sensor(0x3785,0x1e);
    OV5693MIPI_write_cmos_sensor(0x378f,0xf5);
    OV5693MIPI_write_cmos_sensor(0x3791,0xb0);
    OV5693MIPI_write_cmos_sensor(0x3795,0x00);
    OV5693MIPI_write_cmos_sensor(0x3796,0x64);
    OV5693MIPI_write_cmos_sensor(0x3797,0x11);
    OV5693MIPI_write_cmos_sensor(0x3798,0x30);
    OV5693MIPI_write_cmos_sensor(0x3799,0x41);
    OV5693MIPI_write_cmos_sensor(0x379a,0x07);
    OV5693MIPI_write_cmos_sensor(0x379b,0xb0);
    OV5693MIPI_write_cmos_sensor(0x379c,0x0c);  //PSRAM control                       
    OV5693MIPI_write_cmos_sensor(0x37c5,0x00);  //sensor FREX exp HH                  
    OV5693MIPI_write_cmos_sensor(0x37c6,0x00);  //sensor FREX exp H                   
    OV5693MIPI_write_cmos_sensor(0x37c7,0x00);  //sensor FREX exp L                   
    OV5693MIPI_write_cmos_sensor(0x37c9,0x00);  //strobe Width HH                     
    OV5693MIPI_write_cmos_sensor(0x37ca,0x00);  //strobe Width H                      
    OV5693MIPI_write_cmos_sensor(0x37cb,0x00);  //strobe Width L                      
    OV5693MIPI_write_cmos_sensor(0x37de,0x00);  //sensor FREX PCHG Width H            
    OV5693MIPI_write_cmos_sensor(0x37df,0x00);  //sensor FREX PCHG Width L
    
    OV5693MIPI_write_cmos_sensor(0x3800,0x00);  //X start H                           
    OV5693MIPI_write_cmos_sensor(0x3801,0x00);  //X start L                           
    OV5693MIPI_write_cmos_sensor(0x3802,0x00);  //Y start H                           
    OV5693MIPI_write_cmos_sensor(0x3803,0x00);  //Y start L                           
    OV5693MIPI_write_cmos_sensor(0x3804,0x0a);  //X end H                             
    OV5693MIPI_write_cmos_sensor(0x3805,0x3f);  //X end L                             
    OV5693MIPI_write_cmos_sensor(0x3806,0x07);  //Y end H                             
    OV5693MIPI_write_cmos_sensor(0x3807,0xa3);  //Y end L                             
    OV5693MIPI_write_cmos_sensor(0x3808,0x05);  //X output size H  2592             //05  0a
    OV5693MIPI_write_cmos_sensor(0x3809,0x10);  //X output size L                       //10  20
    OV5693MIPI_write_cmos_sensor(0x380a,0x03);  //Y output size H  1944             //03  07
    OV5693MIPI_write_cmos_sensor(0x380b,0xcc);  //Y output size L                       //cc   98
    OV5693MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H  2688                             
    OV5693MIPI_write_cmos_sensor(0x380d,0x80);  //HTS H                               
    OV5693MIPI_write_cmos_sensor(0x380e,0x07);  //HTS L  1984                           //05
    OV5693MIPI_write_cmos_sensor(0x380f,0xc0);  //HTS L                                     //d0
    
    OV5693MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H                  
    OV5693MIPI_write_cmos_sensor(0x3811,0x08);  //timing ISP x win L     //02             
    OV5693MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H                  
    OV5693MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L                  
    OV5693MIPI_write_cmos_sensor(0x3814,0x31);  //timing x inc                                        //31  11
    OV5693MIPI_write_cmos_sensor(0x3815,0x31);  //timing y ing                                        //31  11

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5693MIPI_write_cmos_sensor(0x3820,0x04);  //v fast bin on, v flip off, v bin off           //04  00
    OV5693MIPI_write_cmos_sensor(0x3821,0x1f);  //hsync on, mirror on, hbin on              //1f   1e
    
    OV5693MIPI_write_cmos_sensor(0x3823,0x00);
    OV5693MIPI_write_cmos_sensor(0x3824,0x00);
    OV5693MIPI_write_cmos_sensor(0x3825,0x00);
    OV5693MIPI_write_cmos_sensor(0x3826,0x00);
    OV5693MIPI_write_cmos_sensor(0x3827,0x00);
    OV5693MIPI_write_cmos_sensor(0x382a,0x04);
    OV5693MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5693MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5693MIPI_write_cmos_sensor(0x3a06,0x00);
    OV5693MIPI_write_cmos_sensor(0x3a07,0xfe);
    OV5693MIPI_write_cmos_sensor(0x3b00,0x00);  //strobe off          
    OV5693MIPI_write_cmos_sensor(0x3b02,0x00);  //strobe dummy line H     
    OV5693MIPI_write_cmos_sensor(0x3b03,0x00);  //strobe dummy line L     
    OV5693MIPI_write_cmos_sensor(0x3b04,0x00);  //strobe at next frame    
    OV5693MIPI_write_cmos_sensor(0x3b05,0x00);  //strobe pulse width      
    OV5693MIPI_write_cmos_sensor(0x3e07,0x20);
    OV5693MIPI_write_cmos_sensor(0x4000,0x08);
    OV5693MIPI_write_cmos_sensor(0x4001,0x04);  //BLC start line                                                                         
    OV5693MIPI_write_cmos_sensor(0x4002,0x45);  //BLC auto enable, do 5 frames                                                           
    OV5693MIPI_write_cmos_sensor(0x4004,0x08);  //8 black lines                                                                          
    OV5693MIPI_write_cmos_sensor(0x4005,0x18);  //don't output black line, apply one channel offset to all, BLC triggered by gain change,
    OV5693MIPI_write_cmos_sensor(0x4006,0x20);  //ZLINE COEF                                                                             
    OV5693MIPI_write_cmos_sensor(0x4008,0x24);                                                                                       
    OV5693MIPI_write_cmos_sensor(0x4009,0x40);  //black level target                                                                     
    OV5693MIPI_write_cmos_sensor(0x400c,0x00);  //BLC man level0 H                                                                       
    OV5693MIPI_write_cmos_sensor(0x400d,0x00);  //BLC man level0 L                                                                       
    OV5693MIPI_write_cmos_sensor(0x4058,0x00);                                                                                         
    OV5693MIPI_write_cmos_sensor(0x404e,0x37);  //BLC maximum black level                                                                
    OV5693MIPI_write_cmos_sensor(0x404f,0x8f);  //BLC stable range                                                                       
    OV5693MIPI_write_cmos_sensor(0x4058,0x00);                                                                                     
    OV5693MIPI_write_cmos_sensor(0x4101,0xb2);                                                                                   
    OV5693MIPI_write_cmos_sensor(0x4303,0x00);  //test pattern off                                                                       
    OV5693MIPI_write_cmos_sensor(0x4304,0x08);  //test pattern option                                                                    
    OV5693MIPI_write_cmos_sensor(0x4307,0x30);                                                                                       
    OV5693MIPI_write_cmos_sensor(0x4311,0x04);  //Vsync width H                                                                          
    OV5693MIPI_write_cmos_sensor(0x4315,0x01);  //Vsync width L                                                                          
    OV5693MIPI_write_cmos_sensor(0x4511,0x05);                                                                                      
    OV5693MIPI_write_cmos_sensor(0x4512,0x01); 

    OV5693MIPI_write_cmos_sensor(0x4800,0x14);  // MIPI line sync enable
    
    OV5693MIPI_write_cmos_sensor(0x4806,0x00);                                                                                      
    OV5693MIPI_write_cmos_sensor(0x4816,0x52);  //embedded line data type                                                                
    OV5693MIPI_write_cmos_sensor(0x481f,0x30);  //max clk_prepare                                                                        
    OV5693MIPI_write_cmos_sensor(0x4826,0x2c);  //hs prepare min                                                                         
    OV5693MIPI_write_cmos_sensor(0x4831,0x64);  //UI hs prepare     
    OV5693MIPI_write_cmos_sensor(0x4d00,0x04);  //temprature sensor 
    OV5693MIPI_write_cmos_sensor(0x4d01,0x71);                 
    OV5693MIPI_write_cmos_sensor(0x4d02,0xfd);                  
    OV5693MIPI_write_cmos_sensor(0x4d03,0xf5);                  
    OV5693MIPI_write_cmos_sensor(0x4d04,0x0c);                  
    OV5693MIPI_write_cmos_sensor(0x4d05,0xcc);  //temperature sensor
    OV5693MIPI_write_cmos_sensor(0x4837,0x09);  //MIPI global timing  //0d 0a
    OV5693MIPI_write_cmos_sensor(0x5000,0x06);  //BPC on, WPC on    
    OV5693MIPI_write_cmos_sensor(0x5001,0x01);  //MWB on            
    OV5693MIPI_write_cmos_sensor(0x5002,0x00);  //scal off          
    OV5693MIPI_write_cmos_sensor(0x5003,0x20);
    OV5693MIPI_write_cmos_sensor(0x5046,0x0a);  //SOF auto mode 
    OV5693MIPI_write_cmos_sensor(0x5013,0x00);            
    OV5693MIPI_write_cmos_sensor(0x5046,0x0a);  //SOF auto mode 

    //update DPC
    OV5693MIPI_write_cmos_sensor(0x5780,0xfc); //DPC
    OV5693MIPI_write_cmos_sensor(0x5781,0x13);
    OV5693MIPI_write_cmos_sensor(0x5782,0x03);
    OV5693MIPI_write_cmos_sensor(0x5786,0x20);
    OV5693MIPI_write_cmos_sensor(0x5787,0x40);
    OV5693MIPI_write_cmos_sensor(0x5788,0x08);
    OV5693MIPI_write_cmos_sensor(0x5789,0x08);
    OV5693MIPI_write_cmos_sensor(0x578a,0x02);
    OV5693MIPI_write_cmos_sensor(0x578b,0x01);
    OV5693MIPI_write_cmos_sensor(0x578c,0x01);
    OV5693MIPI_write_cmos_sensor(0x578d,0x0c);
    OV5693MIPI_write_cmos_sensor(0x578e,0x02);
    OV5693MIPI_write_cmos_sensor(0x578f,0x01);
    OV5693MIPI_write_cmos_sensor(0x5790,0x01);
    OV5693MIPI_write_cmos_sensor(0x5791,0xff); //DPC
    
    OV5693MIPI_write_cmos_sensor(0x5842,0x01);  //LENC BR Hscale H
    OV5693MIPI_write_cmos_sensor(0x5843,0x2b);  //LENC BR Hscale L
    OV5693MIPI_write_cmos_sensor(0x5844,0x01);  //LENC BR Vscale H
    OV5693MIPI_write_cmos_sensor(0x5845,0x92);  //LENC BR Vscale L
    OV5693MIPI_write_cmos_sensor(0x5846,0x01);  //LENC G Hscale H 
    OV5693MIPI_write_cmos_sensor(0x5847,0x8f);  //LENC G Hscale L 
    OV5693MIPI_write_cmos_sensor(0x5848,0x01);  //LENC G Vscale H 
    OV5693MIPI_write_cmos_sensor(0x5849,0x0c);  //LENC G Vscale L 
    OV5693MIPI_write_cmos_sensor(0x5e00,0x00);  //test pattern off
    OV5693MIPI_write_cmos_sensor(0x5e10,0x0c);
    OV5693MIPI_write_cmos_sensor(0x0100,0x01);  //wake up
}   /*  OV5693MIPI_Sensor_Init  */


static void OV5693MIPI_Preview_Setting(void)
{
    //5.1.2 FQPreview 1296x972 30fps 24M MCLK 2lane 864Mbps/lane
    OV5693MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep

    OV5693MIPI_write_cmos_sensor(0x3708,0xe6);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3709,0xc7);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3803,0x00);  //timing Y start L
    OV5693MIPI_write_cmos_sensor(0x3806,0x07);  //timing Y end H
    OV5693MIPI_write_cmos_sensor(0x3807,0xa3);  //timing Y end L
    
    OV5693MIPI_write_cmos_sensor(0x3808,0x05);  //X output size H  1296
    OV5693MIPI_write_cmos_sensor(0x3809,0x10);  //X output size L
    OV5693MIPI_write_cmos_sensor(0x380a,0x03);  //Y output size H  972
    OV5693MIPI_write_cmos_sensor(0x380b,0xcc);  //Y output size L
    
    //OV5693MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H  2688
    //OV5693MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5693MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H  1984
    //OV5693MIPI_write_cmos_sensor(0x380f,0xc0);  //VTS L
    OV5693MIPI_write_cmos_sensor(0x380c, ((OV5693MIPI_PV_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5693MIPI_write_cmos_sensor(0x380d, (OV5693MIPI_PV_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5693MIPI_write_cmos_sensor(0x380e, ((OV5693MIPI_PV_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5693MIPI_write_cmos_sensor(0x380f, (OV5693MIPI_PV_PERIOD_LINE_NUMS & 0xFF));         // vts    
    
    OV5693MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5693MIPI_write_cmos_sensor(0x3811,0x08);  //timing ISP x win L
    OV5693MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5693MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L
    OV5693MIPI_write_cmos_sensor(0x3814,0x31);  //timing X inc
    OV5693MIPI_write_cmos_sensor(0x3815,0x31);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/    
    OV5693MIPI_write_cmos_sensor(0x3820,0x04);  //v fast bin on, v flip off, v bin off
    OV5693MIPI_write_cmos_sensor(0x3821,0x1f);  //hsync on, mirror on, hbin on
    
    OV5693MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5693MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5693MIPI_write_cmos_sensor(0x5002,0x00);  //scale off

    OV5693MIPI_write_cmos_sensor(0x0100,0x01);  //wake up   
}   /*  OV5693MIPI_Capture_Setting  */

static void OV5693MIPI_Capture_Setting(void)
{
    //5.1.3 Capture 2592x1944 30fps 24M MCLK 2lane 864Mbps/lane
    OV5693MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep
    
    OV5693MIPI_write_cmos_sensor(0x3708,0xe2);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3803,0x00);  //timing Y start L
    OV5693MIPI_write_cmos_sensor(0x3806,0x07);  //timing Y end H
    OV5693MIPI_write_cmos_sensor(0x3807,0xa3);  //timing Y end L
    
    OV5693MIPI_write_cmos_sensor(0x3808,0x0a);  //X output size H  2592
    OV5693MIPI_write_cmos_sensor(0x3809,0x20);  //X output size L
    OV5693MIPI_write_cmos_sensor(0x380a,0x07);  //Y output size H  1944
    OV5693MIPI_write_cmos_sensor(0x380b,0x98);  //Y output size L
    
    //OV5693MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H 2688
    //OV5693MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5693MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H 1984
    //OV5693MIPI_write_cmos_sensor(0x380f,0xc0);  //VTS L
    OV5693MIPI_write_cmos_sensor(0x380c, ((OV5693MIPI_FULL_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5693MIPI_write_cmos_sensor(0x380d, (OV5693MIPI_FULL_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5693MIPI_write_cmos_sensor(0x380e, ((OV5693MIPI_FULL_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5693MIPI_write_cmos_sensor(0x380f, (OV5693MIPI_FULL_PERIOD_LINE_NUMS & 0xFF));         // vts      
    
    OV5693MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5693MIPI_write_cmos_sensor(0x3811,0x10);  //timing ISP x win L
    OV5693MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5693MIPI_write_cmos_sensor(0x3813,0x06);  //timing ISP y win L
    OV5693MIPI_write_cmos_sensor(0x3814,0x11);  //timing X inc
    OV5693MIPI_write_cmos_sensor(0x3815,0x11);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5693MIPI_write_cmos_sensor(0x3820,0x00);  //v fast vbin on, flip off, v bin on
    OV5693MIPI_write_cmos_sensor(0x3821,0x1e);  //hsync on, h mirror on, h bin on
    
    OV5693MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5693MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5693MIPI_write_cmos_sensor(0x5002,0x00);  //scale off
    
    OV5693MIPI_write_cmos_sensor(0x0100,0x01);  //wake up
}

#ifdef VIDEO_720P

static void OV5693MIPI_Video_720P_Setting(void)
{
    SENSORDB("Enter!");

    //5.1.4 Video BQ720p Full FOV 30fps 24M MCLK 2lane 864Mbps/lane
    OV5693MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep
    
    OV5693MIPI_write_cmos_sensor(0x3708,0xe6);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3803,0xf4);  //timing Y start L
    OV5693MIPI_write_cmos_sensor(0x3806,0x06);  //timing Y end H
    OV5693MIPI_write_cmos_sensor(0x3807,0xab);  //timing Y end L
    
    OV5693MIPI_write_cmos_sensor(0x3808,0x05);  //X output size H  1280
    OV5693MIPI_write_cmos_sensor(0x3809,0x00);  //X output size L
    OV5693MIPI_write_cmos_sensor(0x380a,0x02);  //Y output size H  720
    OV5693MIPI_write_cmos_sensor(0x380b,0xd0);  //Y output size L
    
    //OV5693MIPI_write_cmos_sensor(0x380c,0x0d);  //HTS H  2688
    //OV5693MIPI_write_cmos_sensor(0x380d,0xb0);  //HTS L
    //OV5693MIPI_write_cmos_sensor(0x380e,0x05);  //VTS H  1984
    //OV5693MIPI_write_cmos_sensor(0x380f,0xf0);  //VTS L
    OV5693MIPI_write_cmos_sensor(0x380c, ((OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5693MIPI_write_cmos_sensor(0x380d, (OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5693MIPI_write_cmos_sensor(0x380e, ((OV5693MIPI_VIDEO_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5693MIPI_write_cmos_sensor(0x380f, (OV5693MIPI_VIDEO_PERIOD_LINE_NUMS & 0xFF));         // vts     
    
    OV5693MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5693MIPI_write_cmos_sensor(0x3811,0x08);  //timing ISP x win L
    OV5693MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5693MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L
    OV5693MIPI_write_cmos_sensor(0x3814,0x31);  //timing X inc
    OV5693MIPI_write_cmos_sensor(0x3815,0x31);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5693MIPI_write_cmos_sensor(0x3820,0x01);  //v fast vbin of, flip off, v bin on
    OV5693MIPI_write_cmos_sensor(0x3821,0x1f);  //hsync on, h mirror on, h bin on
    
    OV5693MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5693MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5693MIPI_write_cmos_sensor(0x5002,0x00);  //scale off
    
    OV5693MIPI_write_cmos_sensor(0x0100,0x01);  //wake up

    SENSORDB("Exit!");
}

#elif defined VIDEO_1080P

static void OV5693MIPI_Video_1080P_Setting(void)
{
    SENSORDB("Enter!");
    
    //5.1.5 Video 1080p 30fps 24M MCLK 2lane 864Mbps/lane
    OV5693MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep

    OV5693MIPI_write_cmos_sensor(0x3708,0xe2);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3803,0xf8);  //timing Y start L
    OV5693MIPI_write_cmos_sensor(0x3806,0x06);  //timing Y end H
    OV5693MIPI_write_cmos_sensor(0x3807,0xab);  //timing Y end L

    OV5693MIPI_write_cmos_sensor(0x3808,0x07);  //X output size H  1920
    OV5693MIPI_write_cmos_sensor(0x3809,0x80);  //X output size L
    OV5693MIPI_write_cmos_sensor(0x380a,0x04);  //Y output size H  1080
    OV5693MIPI_write_cmos_sensor(0x380b,0x38);  //Y output size L

    //OV5693MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H  2688
    //OV5693MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5693MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H  1984
    //OV5693MIPI_write_cmos_sensor(0x380f,0xc0);  //VYS L
    OV5693MIPI_write_cmos_sensor(0x380c, ((OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5693MIPI_write_cmos_sensor(0x380d, (OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5693MIPI_write_cmos_sensor(0x380e, ((OV5693MIPI_VIDEO_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5693MIPI_write_cmos_sensor(0x380f, (OV5693MIPI_VIDEO_PERIOD_LINE_NUMS & 0xFF));         // vts       

    OV5693MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5693MIPI_write_cmos_sensor(0x3811,0x02);  //timing ISP x win L
    OV5693MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5693MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L
    OV5693MIPI_write_cmos_sensor(0x3814,0x11);  //timing X inc
    OV5693MIPI_write_cmos_sensor(0x3815,0x11);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5693MIPI_write_cmos_sensor(0x3820,0x00);  //v bin off
    OV5693MIPI_write_cmos_sensor(0x3821,0x1e);  //hsync on, h mirror on, h bin off
    
    OV5693MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5693MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5693MIPI_write_cmos_sensor(0x5002,0x80);  //scale on

    OV5693MIPI_write_cmos_sensor(0x0100,0x01);  //wake up

    SENSORDB("Exit!");
}

#else

static void OV5693MIPI_Video_Setting(void)
{
    SENSORDB("Enter!");
    
    //5.1.3 Video 2592x1944 30fps 24M MCLK 2lane 864Mbps/lane
    OV5693MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep

    OV5693MIPI_write_cmos_sensor(0x3708,0xe2);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5693MIPI_write_cmos_sensor(0x3803,0x00);  //timing Y start L
    OV5693MIPI_write_cmos_sensor(0x3806,0x07);  //timing Y end H
    OV5693MIPI_write_cmos_sensor(0x3807,0xa3);  //timing Y end L

    OV5693MIPI_write_cmos_sensor(0x3808,0x0a);  //X output size H  2592
    OV5693MIPI_write_cmos_sensor(0x3809,0x20);  //X output size L
    OV5693MIPI_write_cmos_sensor(0x380a,0x07);  //Y output size H  1944
    OV5693MIPI_write_cmos_sensor(0x380b,0x98);  //Y output size L

    //OV5693MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H 2688
    //OV5693MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5693MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H 1984
    //OV5693MIPI_write_cmos_sensor(0x380f,0xc0);  //VTS L
    OV5693MIPI_write_cmos_sensor(0x380c, ((OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5693MIPI_write_cmos_sensor(0x380d, (OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5693MIPI_write_cmos_sensor(0x380e, ((OV5693MIPI_VIDEO_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5693MIPI_write_cmos_sensor(0x380f, (OV5693MIPI_VIDEO_PERIOD_LINE_NUMS & 0xFF));         // vts       

    OV5693MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5693MIPI_write_cmos_sensor(0x3811,0x10);  //timing ISP x win L
    OV5693MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5693MIPI_write_cmos_sensor(0x3813,0x06);  //timing ISP y win L
    OV5693MIPI_write_cmos_sensor(0x3814,0x11);  //timing X inc
    OV5693MIPI_write_cmos_sensor(0x3815,0x11);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5693MIPI_write_cmos_sensor(0x3820,0x00);  //v fast vbin on, flip off, v bin on
    OV5693MIPI_write_cmos_sensor(0x3821,0x1e);  //hsync on, h mirror on, h bin on
    
    OV5693MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5693MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5693MIPI_write_cmos_sensor(0x5002,0x00);  //scale off

    OV5693MIPI_write_cmos_sensor(0x0100,0x01);  //wake up

    SENSORDB("Exit!");
}

#endif


/*************************************************************************
* FUNCTION
*   OV5693MIPIOpen
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
UINT32 OV5693MIPIOpen(void)
{
    const kal_uint8 i2c_addr[] = {OV5693MIPI_WRITE_ID_1, OV5693MIPI_WRITE_ID_2};
	kal_uint16 i;
    kal_uint16 sensor_id = 0; 

    //ov5693 have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	for(i = 0; i < sizeof(i2c_addr) / sizeof(i2c_addr[0]); i++)
	{
		spin_lock(&ov5693mipi_drv_lock);
		OV5693MIPI_sensor.i2c_write_id = i2c_addr[i];
		spin_unlock(&ov5693mipi_drv_lock);

		sensor_id = ((OV5693MIPI_read_cmos_sensor(0x300A) << 8) | OV5693MIPI_read_cmos_sensor(0x300B));
		if(sensor_id == OV5693_SENSOR_ID)
			break;
	}   
    
    SENSORDB("i2c write address is %x", OV5693MIPI_sensor.i2c_write_id);  
    SENSORDB("sensor_id = 0x%x ", sensor_id);
    
    if (sensor_id != OV5693_SENSOR_ID){
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    
    /* initail sequence write in  */
    OV5693MIPI_Sensor_Init();

    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPIDuringTestPattern = KAL_FALSE;
    OV5693MIPIAutoFlickerMode = KAL_FALSE;
    OV5693MIPI_sensor.sensor_mode = OV5693MIPI_SENSOR_MODE_INIT;
    OV5693MIPI_sensor.shutter = 0x3D0;
    OV5693MIPI_sensor.gain = 0x100;
    OV5693MIPI_sensor.pclk = OV5693MIPI_PREVIEW_CLK;
    OV5693MIPI_sensor.frame_length = OV5693MIPI_PV_PERIOD_LINE_NUMS;
    OV5693MIPI_sensor.line_length = OV5693MIPI_PV_PERIOD_PIXEL_NUMS;
    OV5693MIPI_sensor.dummy_pixel = 0;
    OV5693MIPI_sensor.dummy_line = 0;
    spin_unlock(&ov5693mipi_drv_lock);

    return ERROR_NONE;
}   /*  OV5693MIPIOpen  */


/*************************************************************************
* FUNCTION
*   OV5693GetSensorID
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
UINT32 OV5693GetSensorID(UINT32 *sensorID) 
{
    const kal_uint8 i2c_addr[] = {OV5693MIPI_WRITE_ID_1, OV5693MIPI_WRITE_ID_2};
	kal_uint16 i;

    //ov5693 have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	for(i = 0; i < sizeof(i2c_addr) / sizeof(i2c_addr[0]); i++)
	{
		spin_lock(&ov5693mipi_drv_lock);
		OV5693MIPI_sensor.i2c_write_id = i2c_addr[i];
		spin_unlock(&ov5693mipi_drv_lock);

		*sensorID = ((OV5693MIPI_read_cmos_sensor(0x300A) << 8) | OV5693MIPI_read_cmos_sensor(0x300B));
		if(*sensorID == OV5693_SENSOR_ID)
			break;
	}   
    
    SENSORDB("i2c write address is %x", OV5693MIPI_sensor.i2c_write_id);
    SENSORDB("Sensor ID: 0x%x ", *sensorID);
        
    if (*sensorID != OV5693_SENSOR_ID) {
        // if Sensor ID is not correct, Must set *sensorID to 0xFFFFFFFF 
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
 
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   OV5693MIPIClose
*
* DESCRIPTION
*   
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
UINT32 OV5693MIPIClose(void)
{

    /*No Need to implement this function*/ 
    
    return ERROR_NONE;
}   /*  OV5693MIPIClose  */


/*************************************************************************
* FUNCTION
* OV5693MIPIPreview
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
UINT32 OV5693MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

	if(OV5693MIPI_SENSOR_MODE_PREVIEW == OV5693MIPI_sensor.sensor_mode)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		OV5693MIPI_Preview_Setting();
	}

    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPI_sensor.sensor_mode = OV5693MIPI_SENSOR_MODE_PREVIEW;
    OV5693MIPI_sensor.pclk = OV5693MIPI_PREVIEW_CLK;
    OV5693MIPI_sensor.video_mode = KAL_FALSE;
    OV5693MIPI_sensor.line_length = OV5693MIPI_PV_PERIOD_PIXEL_NUMS;
    OV5693MIPI_sensor.frame_length = OV5693MIPI_PV_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5693mipi_drv_lock);

    SENSORDB("Exit!");
    
    return ERROR_NONE;
}   /*  OV5693MIPIPreview   */

UINT32 OV5693MIPIZSDPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

    OV5693MIPI_Capture_Setting();

    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPI_sensor.sensor_mode = OV5693MIPI_SENSOR_MODE_CAPTURE;
    OV5693MIPI_sensor.pclk = OV5693MIPI_CAPTURE_CLK;
    OV5693MIPI_sensor.video_mode = KAL_FALSE;
    OV5693MIPI_sensor.line_length = OV5693MIPI_FULL_PERIOD_PIXEL_NUMS;
    OV5693MIPI_sensor.frame_length = OV5693MIPI_FULL_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5693mipi_drv_lock);

    SENSORDB("Exit!");

    return ERROR_NONE;
}   /*  OV5693MIPIZSDPreview   */

/*************************************************************************
* FUNCTION
*   OV5693MIPICapture
*
* DESCRIPTION
*   This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV5693MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

    if(OV5693MIPI_SENSOR_MODE_CAPTURE != OV5693MIPI_sensor.sensor_mode)
    {
        OV5693MIPI_Capture_Setting();
    }
        
    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPI_sensor.sensor_mode = OV5693MIPI_SENSOR_MODE_CAPTURE;
    OV5693MIPI_sensor.pclk = OV5693MIPI_CAPTURE_CLK;
    OV5693MIPI_sensor.video_mode = KAL_FALSE;
    OV5693MIPIAutoFlickerMode = KAL_FALSE; 
    OV5693MIPI_sensor.line_length = OV5693MIPI_FULL_PERIOD_PIXEL_NUMS;
    OV5693MIPI_sensor.frame_length = OV5693MIPI_FULL_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5693mipi_drv_lock);

    SENSORDB("Exit!");
    
    return ERROR_NONE;
}   /* OV5693MIPI_Capture() */


UINT32 OV5693MIPIVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

    #ifdef VIDEO_720P
        OV5693MIPI_Video_720P_Setting();
    #elif defined VIDEO_1080P
        OV5693MIPI_Video_1080P_Setting();
    #else
        OV5693MIPI_Video_Setting();
    #endif

    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPI_sensor.sensor_mode = OV5693MIPI_SENSOR_MODE_VIDEO;
    OV5693MIPI_sensor.pclk = OV5693MIPI_VIDEO_CLK;
    OV5693MIPI_sensor.video_mode = KAL_TRUE;
    OV5693MIPI_sensor.line_length = OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS;
    OV5693MIPI_sensor.frame_length = OV5693MIPI_VIDEO_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5693mipi_drv_lock);

    SENSORDB("Exit!");
    
    return ERROR_NONE;
}   /*  OV5693MIPIPreview   */



UINT32 OV5693MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
#ifdef OV5693MIPI_DRIVER_TRACE
        SENSORDB("Enter");
#endif

    pSensorResolution->SensorFullWidth = OV5693MIPI_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight = OV5693MIPI_IMAGE_SENSOR_FULL_HEIGHT;
    
    pSensorResolution->SensorPreviewWidth = OV5693MIPI_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight = OV5693MIPI_IMAGE_SENSOR_PV_HEIGHT;

    pSensorResolution->SensorVideoWidth = OV5693MIPI_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight = OV5693MIPI_IMAGE_SENSOR_VIDEO_HEIGHT;		
    
    return ERROR_NONE;
}   /*  OV5693MIPIGetResolution  */

UINT32 OV5693MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                      MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                      MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
#ifdef OV5693MIPI_DRIVER_TRACE
    SENSORDB("ScenarioId = %d", ScenarioId);
#endif

    switch(ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorPreviewResolutionX = OV5693MIPI_IMAGE_SENSOR_FULL_WIDTH; /* not use */
            pSensorInfo->SensorPreviewResolutionY = OV5693MIPI_IMAGE_SENSOR_FULL_HEIGHT; /* not use */
            pSensorInfo->SensorCameraPreviewFrameRate = 15; /* not use */
            break;

        default:
            pSensorInfo->SensorPreviewResolutionX = OV5693MIPI_IMAGE_SENSOR_PV_WIDTH; /* not use */
            pSensorInfo->SensorPreviewResolutionY = OV5693MIPI_IMAGE_SENSOR_PV_HEIGHT; /* not use */
            pSensorInfo->SensorCameraPreviewFrameRate = 30; /* not use */
            break;
    }

    pSensorInfo->SensorFullResolutionX = OV5693MIPI_IMAGE_SENSOR_FULL_WIDTH; /* not use */
    pSensorInfo->SensorFullResolutionY = OV5693MIPI_IMAGE_SENSOR_FULL_HEIGHT; /* not use */

    pSensorInfo->SensorVideoFrameRate = 30; /* not use */
	pSensorInfo->SensorStillCaptureFrameRate= 30; /* not use */
	pSensorInfo->SensorWebCamCaptureFrameRate= 30; /* not use */

    pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 4; /* not use */
    pSensorInfo->SensorResetActiveHigh = FALSE; /* not use */
    pSensorInfo->SensorResetDelayCount = 5; /* not use */

    pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->SensorOutputDataFormat = OV5693MIPI_COLOR_FORMAT;

    pSensorInfo->CaptureDelayFrame = 2; 
    pSensorInfo->PreviewDelayFrame = 2; 
    pSensorInfo->VideoDelayFrame = 2; 

    pSensorInfo->SensorMasterClockSwitch = 0; /* not use */
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;
    
    pSensorInfo->AEShutDelayFrame = 0;          /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;    /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;  
    
    //pSensorInfo->MIPIsensorType = MIPI_OPHY_CSI2; 

    switch (ScenarioId)
    {
	    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorClockFreq = 24;
			pSensorInfo->SensorClockDividCount = 3; /* not use */
			pSensorInfo->SensorClockRisingCount = 0;
			pSensorInfo->SensorClockFallingCount = 2; /* not use */
			pSensorInfo->SensorPixelClockCount = 3; /* not use */
			pSensorInfo->SensorDataLatchCount = 2; /* not use */
	        pSensorInfo->SensorGrabStartX = OV5693MIPI_FULL_START_X; 
	        pSensorInfo->SensorGrabStartY = OV5693MIPI_FULL_START_Y;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

	        break;
	    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq = 24;
			pSensorInfo->SensorClockDividCount = 3; /* not use */
			pSensorInfo->SensorClockRisingCount = 0;
			pSensorInfo->SensorClockFallingCount = 2; /* not use */
			pSensorInfo->SensorPixelClockCount = 3; /* not use */
			pSensorInfo->SensorDataLatchCount = 2; /* not use */
	        pSensorInfo->SensorGrabStartX = OV5693MIPI_VIDEO_START_X; 
	        pSensorInfo->SensorGrabStartY = OV5693MIPI_VIDEO_START_Y;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

	        break;            
	    default:
	        pSensorInfo->SensorClockFreq = 24;
			pSensorInfo->SensorClockDividCount = 3; /* not use */
			pSensorInfo->SensorClockRisingCount = 0;
			pSensorInfo->SensorClockFallingCount = 2; /* not use */
			pSensorInfo->SensorPixelClockCount = 3; /* not use */
			pSensorInfo->SensorDataLatchCount = 2; /* not use */
	        pSensorInfo->SensorGrabStartX = OV5693MIPI_PV_START_X; 
	        pSensorInfo->SensorGrabStartY = OV5693MIPI_PV_START_Y;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

	        break;
    }
	
	return ERROR_NONE;
}   /*  OV5693MIPIGetInfo  */


UINT32 OV5693MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                      MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    SENSORDB("ScenarioId = %d", ScenarioId);

    mCurrentScenarioId =ScenarioId;
    
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            OV5693MIPIPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            OV5693MIPIVideo(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            OV5693MIPICapture(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            OV5693MIPIZSDPreview(pImageWindow, pSensorConfigData);
            break;      
        default:
            SENSORDB("Error ScenarioId setting");
            return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}   /* OV5693MIPIControl() */



UINT32 OV5693MIPISetVideoMode(UINT16 u2FrameRate)
{
    SENSORDB("u2FrameRate = %d ", u2FrameRate);

    // SetVideoMode Function should fix framerate
    if(u2FrameRate < 5){
        // Dynamic frame rate
        return ERROR_NONE;
    }

    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPI_sensor.video_mode = KAL_TRUE;
    spin_unlock(&ov5693mipi_drv_lock);

    if(u2FrameRate == 30){
        spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPI_sensor.NightMode = KAL_FALSE;
        spin_unlock(&ov5693mipi_drv_lock);
    }else if(u2FrameRate == 15){
        spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPI_sensor.NightMode = KAL_TRUE;
        spin_unlock(&ov5693mipi_drv_lock);
    }else{
        // fixed other frame rate
    }

    spin_lock(&ov5693mipi_drv_lock);
    OV5693MIPI_sensor.FixedFps = u2FrameRate;
    spin_unlock(&ov5693mipi_drv_lock);

    if((u2FrameRate == 30)&&(OV5693MIPIAutoFlickerMode == KAL_TRUE))
        u2FrameRate = 296;
    else if ((u2FrameRate == 15)&&(OV5693MIPIAutoFlickerMode == KAL_TRUE))
        u2FrameRate = 146;
    else
        u2FrameRate = 10 * u2FrameRate;
    
    OV5693MIPISetMaxFrameRate(u2FrameRate);

    return ERROR_NONE;
}

UINT32 OV5693MIPISetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
    SENSORDB("bEnable = %d, u2FrameRate = %d ", bEnable, u2FrameRate);

    if(bEnable){
        spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPIAutoFlickerMode = KAL_TRUE;
        spin_unlock(&ov5693mipi_drv_lock);

        #if 0
        if((OV5693MIPI_sensor.FixedFps == 30)&&(OV5693MIPI_sensor.video_mode==KAL_TRUE))
            OV5693MIPISetMaxFrameRate(296);
        else if((OV5693MIPI_sensor.FixedFps == 15)&&(OV5693MIPI_sensor.video_mode==KAL_TRUE))
            OV5693MIPISetMaxFrameRate(148);
        #endif
        
    }else{ //Cancel Auto flick
        spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPIAutoFlickerMode = KAL_FALSE;
        spin_unlock(&ov5693mipi_drv_lock);
        
        #if 0
        if((OV5693MIPI_sensor.FixedFps == 30)&&(OV5693MIPI_sensor.video_mode==KAL_TRUE))
            OV5693MIPISetMaxFrameRate(300);
        else if((OV5693MIPI_sensor.FixedFps == 15)&&(OV5693MIPI_sensor.video_mode==KAL_TRUE))
            OV5693MIPISetMaxFrameRate(150);      
        #endif
    }

    return ERROR_NONE;
}


UINT32 OV5693MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
    kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
  
	SENSORDB("scenarioId = %d, frame rate = %d", scenarioId, frameRate);

    switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = OV5693MIPI_PREVIEW_CLK;
			lineLength = OV5693MIPI_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV5693MIPI_PV_PERIOD_LINE_NUMS;
            if (dummyLine < 0) dummyLine = 0;
			OV5693MIPI_Set_Dummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = OV5693MIPI_VIDEO_CLK;
			lineLength = OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV5693MIPI_VIDEO_PERIOD_LINE_NUMS;
            if (dummyLine < 0) dummyLine = 0;
			OV5693MIPI_Set_Dummy(0, dummyLine);			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = OV5693MIPI_CAPTURE_CLK;
			lineLength = OV5693MIPI_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV5693MIPI_FULL_PERIOD_LINE_NUMS;
            if (dummyLine < 0) dummyLine = 0;
			OV5693MIPI_Set_Dummy(0, dummyLine);			
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


UINT32 OV5693MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{
    SENSORDB("scenarioId = %d", scenarioId);

	switch (scenarioId) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *pframeRate = 300;
            break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            *pframeRate = 300;
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

UINT32 OV5693MIPI_SetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("bEnable: %d", bEnable);

	if(bEnable) 
	{
	    spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPIDuringTestPattern = KAL_TRUE;
        spin_unlock(&ov5693mipi_drv_lock);

        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        OV5693MIPI_write_cmos_sensor(0x5E00, 0x80);
	}
	else        
	{
	    spin_lock(&ov5693mipi_drv_lock);
        OV5693MIPIDuringTestPattern = KAL_FALSE;
        spin_unlock(&ov5693mipi_drv_lock);

        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        OV5693MIPI_write_cmos_sensor(0x5E00, 0x00);
	}
    
    return ERROR_NONE;
}

UINT32 OV5693MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                             UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    //UINT32 OV5693MIPISensorRegNumber;
    //UINT32 i;
    //PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    //MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ENG_INFO_STRUCT *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    #ifdef OV5693MIPI_DRIVER_TRACE
        //SENSORDB("FeatureId = %d", FeatureId);
    #endif

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++=OV5693MIPI_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16=OV5693MIPI_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
            switch(mCurrentScenarioId)
            {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                case MSDK_SCENARIO_ID_CAMERA_ZSD:
                    *pFeatureReturnPara16++ = OV5693MIPI_FULL_PERIOD_PIXEL_NUMS;
                    *pFeatureReturnPara16 = OV5693MIPI_sensor.frame_length;
                    *pFeatureParaLen=4;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *pFeatureReturnPara16++ = OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS;
                    *pFeatureReturnPara16 = OV5693MIPI_sensor.frame_length;
                    *pFeatureParaLen=4;
                    break;
                default:
                    *pFeatureReturnPara16++ = OV5693MIPI_PV_PERIOD_PIXEL_NUMS;
                    *pFeatureReturnPara16 = OV5693MIPI_sensor.frame_length;
                    *pFeatureParaLen=4;
                    break;
            }
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            switch(mCurrentScenarioId)
            {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                case MSDK_SCENARIO_ID_CAMERA_ZSD: 
                    *pFeatureReturnPara32 = OV5693MIPI_CAPTURE_CLK;
                    *pFeatureParaLen=4;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *pFeatureReturnPara32 = OV5693MIPI_VIDEO_CLK;
                    *pFeatureParaLen=4;
                    break;                  
                default:
                    *pFeatureReturnPara32 = OV5693MIPI_PREVIEW_CLK;
                    *pFeatureParaLen=4;
                    break;
            }
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_OV5693MIPI_shutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            OV5693MIPI_night_mode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:       
            OV5693MIPI_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            OV5693MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = OV5693MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            memcpy(&OV5693MIPI_sensor.eng.cct, pFeaturePara, sizeof(OV5693MIPI_sensor.eng.cct));
            break;
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            if (*pFeatureParaLen >= sizeof(OV5693MIPI_sensor.eng.cct) + sizeof(kal_uint32))
            {
              *((kal_uint32 *)pFeaturePara++) = sizeof(OV5693MIPI_sensor.eng.cct);
              memcpy(pFeaturePara, &OV5693MIPI_sensor.eng.cct, sizeof(OV5693MIPI_sensor.eng.cct));
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            memcpy(&OV5693MIPI_sensor.eng.reg, pFeaturePara, sizeof(OV5693MIPI_sensor.eng.reg));
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            if (*pFeatureParaLen >= sizeof(OV5693MIPI_sensor.eng.reg) + sizeof(kal_uint32))
            {
              *((kal_uint32 *)pFeaturePara++) = sizeof(OV5693MIPI_sensor.eng.reg);
              memcpy(pFeaturePara, &OV5693MIPI_sensor.eng.reg, sizeof(OV5693MIPI_sensor.eng.reg));
            }
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            ((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
            ((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = OV5693_SENSOR_ID;
            memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &OV5693MIPI_sensor.eng.reg, sizeof(OV5693MIPI_sensor.eng.reg));
            memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &OV5693MIPI_sensor.eng.cct, sizeof(OV5693MIPI_sensor.eng.cct));
            *pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pFeaturePara, &OV5693MIPI_sensor.cfg_data, sizeof(OV5693MIPI_sensor.cfg_data));
            *pFeatureParaLen = sizeof(OV5693MIPI_sensor.cfg_data);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
             OV5693MIPI_camera_para_to_sensor();
            break;
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            OV5693MIPI_sensor_to_camera_para();
            break;                          
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            OV5693MIPI_get_sensor_group_count((kal_uint32 *)pFeaturePara);
            *pFeatureParaLen = 4;
            break;    
        case SENSOR_FEATURE_GET_GROUP_INFO:
            OV5693MIPI_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            OV5693MIPI_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_SET_ITEM_INFO:
            OV5693MIPI_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ENG_INFO:
            memcpy(pFeaturePara, &OV5693MIPI_sensor.eng_info, sizeof(OV5693MIPI_sensor.eng_info));
            *pFeatureParaLen = sizeof(OV5693MIPI_sensor.eng_info);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            OV5693MIPISetVideoMode(*pFeatureData16);
            break; 
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV5693GetSensorID(pFeatureReturnPara32); 
            break; 
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            OV5693MIPISetAutoFlickerMode((BOOL)*pFeatureData16,*(pFeatureData16+1));
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV5693MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV5693MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV5693MIPI_SetTestPatternMode((BOOL)*pFeatureData16);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing             
            *pFeatureReturnPara32= OV5693MIPI_TEST_PATTERN_CHECKSUM;
            *pFeatureParaLen=4;                             
            break;            
        default:
            break;
    }
  
    return ERROR_NONE;
}   /*  OV5693MIPIFeatureControl()  */

SENSOR_FUNCTION_STRUCT  SensorFuncOV5693MIPI=
{
    OV5693MIPIOpen,
    OV5693MIPIGetInfo,
    OV5693MIPIGetResolution,
    OV5693MIPIFeatureControl,
    OV5693MIPIControl,
    OV5693MIPIClose
};

UINT32 OV5693_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncOV5693MIPI;

    return ERROR_NONE;
}   /*  OV5693MIPISensorInit  */
