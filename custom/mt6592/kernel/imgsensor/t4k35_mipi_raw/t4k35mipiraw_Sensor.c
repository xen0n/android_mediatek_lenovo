/*******************************************************************************************/
   

/******************************************************************************************/

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

#include "t4k35mipiraw_Sensor.h"
#include "t4k35mipiraw_Camera_Sensor_para.h"
#include "t4k35mipiraw_CameraCustomized.h"
static DEFINE_SPINLOCK(t4k35mipiraw_drv_lock);

#define T4K35_DEBUG
//#define T4K35_DEBUG_SOFIA

#ifdef T4K35_DEBUG
	#define T4K35DB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[T4K35Raw] ",  fmt, ##arg)
#else
	#define T4K35DB(fmt, arg...)
#endif

#ifdef T4K35_DEBUG_SOFIA
	#define T4K35DBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[T4K35Raw] ",  fmt, ##arg)
#else
	#define T4K35DBSOFIA(fmt, arg...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 T4K35_FeatureControl_PERIOD_PixelNum=T4K35_PV_PERIOD_PIXEL_NUMS;
kal_uint32 T4K35_FeatureControl_PERIOD_LineNum=T4K35_PV_PERIOD_LINE_NUMS;

static UINT16 VIDEO_MODE_TARGET_FPS = 30;
static BOOL ReEnteyCamera = KAL_FALSE;
kal_bool T4K35DuringTestPattern = KAL_FALSE;
#define T4K35_TEST_PATTERN_CHECKSUM (0xeab06ccb)


MSDK_SENSOR_CONFIG_STRUCT T4K35SensorConfigData;

kal_uint32 T4K35_FAC_SENSOR_REG;
kal_bool flag_video=0;

MSDK_SCENARIO_ID_ENUM T4K35CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT T4K35SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT T4K35SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/
struct T4K35_SENSOR_STRUCT T4K35_sensor=
{
    .i2c_write_id = 0x6E,
    .i2c_read_id  = 0x6F,
};

static T4K35_PARA_STRUCT t4k35;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

extern int iMultiWriteReg(u8 *pData, u16 lens, u16 i2cId);

#define T4K35_multi_write_cmos_sensor(pData, lens) iMultiWriteReg((u8*) pData, (u16) lens, T4K35MIPI_WRITE_ID)

#define T4K35_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, T4K35MIPI_WRITE_ID)

kal_uint16 T4K35_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,T4K35MIPI_WRITE_ID);
    return get_byte;
}


#define Sleep(ms) mdelay(ms)


/*******************************************************************************
*
********************************************************************************/
//#define OTP_TEST

#ifdef OTP_TEST

#define SET_T4K35_REG(addr, para)  T4K35_write_cmos_sensor((u16)addr, (u8)(para))
#define GET_T4K35_REG(addr, para)  para=T4K35_read_cmos_sensor((u16)addr)

typedef struct t4k35_otp_struct 
{
  uint8_t LSC[53];              /* LSC */
  uint8_t AWB[8];               /* AWB */
  uint8_t Module_Info[9];
  uint8_t AF_Macro[2];
  uint8_t AF_Inifity[5];
} st_t4k35_otp;

#define T4K35_OTP_PSEL 0x3502
#define T4K35_OTP_CTRL 0x3500
#define T4K35_OTP_DATA_BEGIN_ADDR 0x3504
#define T4K35_OTP_DATA_END_ADDR 0x3543

static uint16_t t4k35_otp_data[T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR + 1] = {0x00};
static uint16_t t4k35_otp_data_backup[T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR + 1] = {0x00};

static uint16_t t4k35_r_golden_value=0x50; //0x91
static uint16_t t4k35_g_golden_value=0x90; //0xA6
static uint16_t t4k35_b_golden_value=0x5d; //0x81

static void t4k35_otp_set_page(uint16_t page)
{
   	//set page
    SET_T4K35_REG(T4K35_OTP_PSEL, page);
}
static void t4k35_otp_access(void)
{
	uint16_t reg_val;
	//OTP access
	GET_T4K35_REG(T4K35_OTP_CTRL, reg_val);
	SET_T4K35_REG(T4K35_OTP_CTRL, reg_val | 0x80);
	Sleep(30);
}
static void t4k35_otp_read_enble(uint8_t enble)
{
    if (enble)
        SET_T4K35_REG(T4K35_OTP_CTRL, 0x01);
    else
        SET_T4K35_REG(T4K35_OTP_CTRL, 0x00);
}



static int32_t t4k35_otp_read_data(uint16_t* otp_data)
{
    uint16_t i = 0;
    //uint16_t data = 0;
	
    for (i = 0; i <= (T4K35_OTP_DATA_END_ADDR - T4K35_OTP_DATA_BEGIN_ADDR); i++)
	{
        GET_T4K35_REG(T4K35_OTP_DATA_BEGIN_ADDR+i,otp_data[i]);
    }

    return 0;
}

static void t4k35_update_awb(struct t4k35_otp_struct *p_otp)
{
  //return;
  uint16_t rg,bg,r_otp,g_otp,b_otp;

  r_otp=p_otp->AWB[1]+(p_otp->AWB[0]<<8);
  g_otp=(p_otp->AWB[3]+(p_otp->AWB[2]<<8)+p_otp->AWB[5]+(p_otp->AWB[4]<<8))/2;
  b_otp=p_otp->AWB[7]+(p_otp->AWB[6]<<8);
  
  rg = 256*(t4k35_r_golden_value *g_otp)/(r_otp*t4k35_g_golden_value);
  bg = 256*(t4k35_b_golden_value*g_otp)/(b_otp*t4k35_g_golden_value);

  printk("r_golden=0x%x,g_golden=0x%x, b_golden=0x%0x\n", t4k35_r_golden_value,t4k35_g_golden_value,t4k35_b_golden_value);
  printk("r_otp=0x%x,g_opt=0x%x, b_otp=0x%0x\n", r_otp,g_otp,b_otp);
  printk("rg=0x%x, bg=0x%0x\n", rg,bg);

  //R
  SET_T4K35_REG(0x0212, rg >> 8);
  SET_T4K35_REG(0x0213, rg & 0xff);

  //B
  SET_T4K35_REG(0x0214, bg >> 8);
  SET_T4K35_REG(0x0215, bg & 0xff);
  T4K35DB("T4K35_OTP_12=%x \n ",(T4K35_read_cmos_sensor(0x0212)));
  T4K35DB("T4K35_OTP_13=%x \n ",(T4K35_read_cmos_sensor(0x0213)));
  T4K35DB("T4K35_OTP_14=%x \n ",(T4K35_read_cmos_sensor(0x0214)));
  T4K35DB("T4K35_OTP_15=%x \n ",(T4K35_read_cmos_sensor(0x0215)));
}

static void t4k35_update_lsc(struct t4k35_otp_struct *p_otp)
{
  uint16_t addr;
  int i;

  //set lsc parameters
  addr = 0x323A;
  SET_T4K35_REG(addr, p_otp->LSC[0]);
  addr = 0x323E;
  for(i = 1; i < 53; i++) 
  {
    printk("SET LSC[%d], addr:0x%0x, val:0x%0x\n", i, addr, p_otp->LSC[i]);
    SET_T4K35_REG(addr++, p_otp->LSC[i]);
  }

  //turn on lsc
  SET_T4K35_REG(0x3237,0x80);
}

static int32_t t4k35_otp_init_lsc_awb(struct t4k35_otp_struct *p_otp)
{
  int i,j;
  //uint16_t reg_val;
  uint16_t check_sum=0x00;

  //read OTP LSC and AWB data
  for(i = 3; i >= 0; i--) 
  {
  	//otp enable
  	t4k35_otp_read_enble(1);
  	//read data area
  	//set page
	t4k35_otp_set_page(i);
	//OTP access
    t4k35_otp_access();

	printk("otp lsc data area data:%d\n",i);
    t4k35_otp_read_data(t4k35_otp_data);


	//read data backup area
	printk("otp lsc backup area data:%d\n",i+6);
	//set page
	t4k35_otp_set_page(i+6);
	//OTP access
    t4k35_otp_access();
	
	t4k35_otp_read_data(t4k35_otp_data_backup);
	//otp disable
  	t4k35_otp_read_enble(0);

	//get final OTP data;
    for(j = 0; j < 64; j++) 
	{
        t4k35_otp_data[j]=t4k35_otp_data[j]|t4k35_otp_data_backup[j];
    }

	//check program flag
    if (0 == t4k35_otp_data[0]) 
	{
      continue;
    } 
	else 
	{
	  //checking check sum
	  for(j = 2; j < 64; j++) 
	  {
        check_sum=check_sum+t4k35_otp_data[j];
      }
	  
	  if((check_sum&0xFF)==t4k35_otp_data[1])
	  {
	  	printk("otp lsc checksum ok!\n");
		for(j=3;j<=55;j++)
		{
			p_otp->LSC[j-3]=t4k35_otp_data[j];
		}
		for(j=56;j<=63;j++)
		{
			p_otp->AWB[j-56]=t4k35_otp_data[j];
		}
		return 0;
	  }
	  else
	  {
		printk("otp lsc checksum error!\n");
		return -1;
	  }
    }
  }

  if (i < 0) 
  {
    return -1;
    printk("No otp lsc data on sensor t4k35\n");
  }
  else 
  {
    return 0;
  }
}


static int32_t t4k35_otp_init_module_info(struct t4k35_otp_struct *p_otp)
{
  int i,pos;
  uint16_t check_sum=0x00;

  //otp enable
  t4k35_otp_read_enble(1);
  //set page
  t4k35_otp_set_page(4);
  //OTP access
  t4k35_otp_access();
  printk("data area data:\n");
  t4k35_otp_read_data(t4k35_otp_data);


  //set page
  t4k35_otp_set_page(10);
  //OTP access
  t4k35_otp_access();
  t4k35_otp_read_data(t4k35_otp_data_backup);
  //otp disable
  t4k35_otp_read_enble(0);		
  
  //get final OTP data;
  for(i = 0; i < 64; i++) 
  {
	  t4k35_otp_data[i]=t4k35_otp_data[i]|t4k35_otp_data_backup[i];
  }

  //check flag
  if(t4k35_otp_data[32])
  {
	  pos=32;
  }
  else if(t4k35_otp_data[0])
  {
  	  pos=0;
  }
  else
  {
  	  printk("otp no module information!\n");
  	  return -1;
  }
  

  //checking check sum
  for(i = pos+2; i <pos+32; i++) 
  {
     check_sum=check_sum+t4k35_otp_data[i];
  }
	  
  if((check_sum&0xFF)==t4k35_otp_data[pos+1])
  {
	  	printk("otp module info checksum ok!\n");
		if((t4k35_otp_data[pos+15]==0x00)&&(t4k35_otp_data[pos+16]==0x00)
			&&(t4k35_otp_data[pos+17]==0x00)&&(t4k35_otp_data[pos+18]==0x00)
			&&(t4k35_otp_data[pos+19]==0x00)&&(t4k35_otp_data[pos+20]==0x00)
			&&(t4k35_otp_data[pos+21]==0x00)&&(t4k35_otp_data[pos+22]==0x00))
			return 0;
		
			
		t4k35_r_golden_value=t4k35_otp_data[pos+16]+(t4k35_otp_data[pos+15]<<8);
		t4k35_g_golden_value=(t4k35_otp_data[pos+18]+(t4k35_otp_data[pos+17]<<8)+t4k35_otp_data[pos+20]+(t4k35_otp_data[pos+19]<<8))/2;
		t4k35_b_golden_value=t4k35_otp_data[pos+22]+(t4k35_otp_data[pos+21]<<8);
		return 0;
  }
  else
  {
	printk("otp module info checksum error!\n");
	return -1;
  }

}

static int32_t t4k35_otp_init_setting(void)
{
    int32_t rc = 0;
	st_t4k35_otp t4k35_data;

	rc=t4k35_otp_init_module_info(&t4k35_data);
	if(rc==0x00)
	{
		//check module information
	}
	
    rc=t4k35_otp_init_lsc_awb(&t4k35_data);
	if(rc==0x00)
	{
		t4k35_update_lsc(&t4k35_data);
		t4k35_update_awb(&t4k35_data);
	}

	
    return rc;
}

#endif
static kal_uint16 T4K35MIPIReg2Gain(const kal_uint8 iReg)
{

    kal_uint16 iGain;
	kal_uint8  Senosr_base=0x2E;

	iGain=(T4K35_read_cmos_sensor(0x0204)<<8)|T4K35_read_cmos_sensor(0x0205);
    iGain=(iGain*64)/Senosr_base;
	
	return iGain;


}
	
static kal_uint16 T4K35MIPIGain2Reg(kal_uint16 iGain)
{
    kal_uint16 Gain;
	kal_uint8  Senosr_base=0x2E;
	T4K35DB("[T4K35MIPI_SetGain] iGain is :%d \n ",iGain);
	Gain=(iGain*Senosr_base)/64;
	//T4K35DB("[T4K35MIPI_SetGain] Gain is :%d \n ",Gain);
	//T4K35_write_cmos_sensor(0x0104, 1);      	
   
    T4K35_write_cmos_sensor(0x0204, (Gain >> 8) & 0x0F);
    T4K35_write_cmos_sensor(0x0205, Gain  & 0xFF);
	
    //T4K35_write_cmos_sensor(0x0104, 0); 


}

/*************************************************************************
* FUNCTION
*    T4K35MIPI_SetGain
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


kal_uint16 read_T4K35MIPI_gain(void)
{   
    return (kal_uint16)(T4K35_read_cmos_sensor(0x0204)<<8)|T4K35_read_cmos_sensor(0x0205);

}  /* read_A8141MIPI_gain */

void T4K35MIPI_SetGain(UINT16 iGain)
{
	T4K35DB("[T4K35MIPI_SetGain] iGain is :%d \n ",iGain);
	
    T4K35MIPIGain2Reg(iGain);

}   /*  T4K35MIPI_SetGain  */

static void T4K35_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	if ( SENSOR_MODE_PREVIEW == t4k35.sensorMode )	//SXGA size output
	{
		line_length = T4K35_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K35_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_VIDEO== t4k35.sensorMode )
	{
		line_length = T4K35_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K35_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_CAPTURE== t4k35.sensorMode )
	{
		line_length = T4K35_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K35_FULL_PERIOD_LINE_NUMS + iLines;
	}

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.maxExposureLines = frame_length -6;
	T4K35_FeatureControl_PERIOD_PixelNum = line_length;
	T4K35_FeatureControl_PERIOD_LineNum = frame_length;
	spin_unlock(&t4k35mipiraw_drv_lock);

	//Set total line length
	T4K35_write_cmos_sensor(0x0104, 1); 
	T4K35_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
	T4K35_write_cmos_sensor(0x0341, frame_length& 0xFF);
	T4K35_write_cmos_sensor(0x0342, (line_length >>8) & 0xFF);
	T4K35_write_cmos_sensor(0x0343, line_length& 0xFF);
	T4K35_write_cmos_sensor(0x0104, 0); 

	T4K35DB("[T4K35MIPI_SetDummy] frame_length is :%d \n ",frame_length);
	T4K35DB("[T4K35MIPI_SetDummy] line_lengthis :%d \n ",line_length);

}   /*  T4K35_SetDummy */
static kal_uint8 T4K35_init[]={

        0x01,0x01,0x00,
		0x01,0x03,0x00,
		0x01,0x04,0x00,
		0x01,0x05,0x01,//01
		0x01,0x10,0x00,
		0x01,0x11,0x02,
		0x01,0x12,0x0A,
		0x01,0x13,0x0A,
		0x01,0x14,0x03,
		0x01,0x30,0x18,
		0x01,0x31,0x00,
		0x01,0x41,0x00,
		0x01,0x42,0x00,
		0x01,0x43,0x00,
		0x02,0x02,0x09,
		0x02,0x03,0xB2,
		0x02,0x04,0x00,
		0x02,0x05,0x2E,
		0x02,0x10,0x01,
		0x02,0x11,0x00,
		0x02,0x12,0x01,
		0x02,0x13,0x00,
		0x02,0x14,0x01,
		0x02,0x15,0x00,
		0x02,0x16,0x01,
		0x02,0x17,0x00,
		0x02,0x30,0x00,
		0x03,0x01,0x01,
		0x03,0x03,0x05,
		0x03,0x05,0x03,
		0x03,0x06,0x00,
		0x03,0x07,0x6E,
		0x03,0x0B,0x00,
		0x03,0x0D,0x03,
		0x03,0x0E,0x00,
		0x03,0x0F,0x6E,
		0x03,0x10,0x00,
		0x03,0x40,0x09,
		0x03,0x41,0xB8,
		0x03,0x42,0x0D,
		0x03,0x43,0xB4,
		0x03,0x44,0x00,
		0x03,0x46,0x00,
		0x03,0x47,0x08,
		0x03,0x4A,0x09,
		0x03,0x4B,0x98,
		0x03,0x4C,0x0C,
		0x03,0x4D,0xC0,
		0x03,0x4E,0x09,
		0x03,0x4F,0x90,
		0x04,0x01,0x00,
		0x04,0x03,0x00,
		0x04,0x04,0x10,
		0x04,0x08,0x00,
		0x04,0x09,0x00,
		0x04,0x0A,0x00,
		0x04,0x0B,0x00,
		0x04,0x0C,0x0C,
		0x04,0x0D,0xD0,
		0x04,0x0E,0x09,
		0x04,0x0F,0xA0,
		0x06,0x01,0x00,
		0x08,0x00,0x88,
		0x08,0x01,0x38,
		0x08,0x02,0x78,
		0x08,0x03,0x48,
		0x08,0x04,0x48,
		0x08,0x05,0x40,
		0x08,0x06,0x00,
		0x08,0x07,0x48,
		0x08,0x08,0x01,
		0x08,0x20,0x0A,
		0x08,0x21,0x20,
		0x08,0x22,0x00,
		0x08,0x23,0x00,
		0x09,0x00,0x00,
		0x09,0x01,0x00,
		0x09,0x02,0x00,
		0x0A,0x05,0x01,
		0x0A,0x06,0x01,
		0x0A,0x07,0x98,
		0x0A,0x0A,0x01,
		0x0A,0x0B,0x98,
		0x0F,0x00,0x00,
		0x30,0x0A,0x00,
		0x30,0x1A,0x44,
		0x30,0x1B,0x44,
		0x30,0x24,0x01,
		0x30,0x25,0x83,
		0x30,0x37,0x06,
		0x30,0x38,0x06,
		0x30,0x39,0x06,
		0x30,0x3A,0x06,
		0x30,0x3B,0x0B,
		0x30,0x3C,0x03,
		0x30,0x3D,0x03,
		0x30,0x53,0xC0,
		0x30,0x5D,0x10,
		0x30,0x5E,0x06,
		0x30,0x6B,0x08,
		0x30,0x73,0x1C,
		0x30,0x74,0x0F,
		0x30,0x75,0x03,
		0x30,0x76,0x7F,
		0x30,0x7E,0x02,
		0x30,0x8D,0x03,
		0x30,0x8E,0x20,
		0x30,0x91,0x04,
		0x30,0x96,0x75,
		0x30,0x97,0x7E,
		0x30,0x98,0x20,
		0x30,0xA0,0x82,
		0x30,0xAB,0x30,
		0x30,0xCE,0x65,
		0x30,0xCF,0x75,
		0x30,0xD2,0xB3,
		0x30,0xD5,0x09,
		0x31,0x34,0x01,
		0x31,0x4D,0x80,
		0x31,0x5B,0x22,
		0x31,0x5C,0x22,
		0x31,0x5D,0x02,
		0x31,0x65,0x67,
		0x31,0x68,0xF1,
		0x31,0x69,0x77,
		0x31,0x6A,0x77,
		0x31,0x73,0x30,
		0x31,0xC1,0x27,
		0x31,0xDB,0x15,
		0x31,0xDC,0xE0,
		0x32,0x04,0x00,
		0x32,0x31,0x00,
		0x32,0x32,0x00,
		0x32,0x33,0x00,
		0x32,0x34,0x00,
		0x32,0x37,0x00,
		0x32,0x38,0x00,
		0x32,0x39,0x80,
		0x32,0x3A,0x80,
		0x32,0x3B,0x00,
		0x32,0x3C,0x81,
		0x32,0x3D,0x00,
		0x32,0x3E,0x02,
		0x32,0x3F,0x00,
		0x32,0x40,0x00,
		0x32,0x41,0x00,
		0x32,0x42,0x00,
		0x32,0x43,0x00,
		0x32,0x44,0x00,
		0x32,0x45,0x00,
		0x32,0x46,0x00,
		0x32,0x47,0x00,
		0x32,0x48,0x00,
		0x32,0x49,0x00,
		0x32,0x4A,0x00,
		0x32,0x4B,0x00,
		0x32,0x4C,0x00,
		0x32,0x4D,0x00,
		0x32,0x4E,0x00,
		0x32,0x4F,0x00,
		0x32,0x50,0x00,
		0x32,0x51,0x80,
		0x32,0x52,0x80,
		0x32,0x53,0x80,
		0x32,0x54,0x80,
		0x32,0x55,0x80,
		0x32,0x56,0x80,
		0x32,0x57,0x80,
		0x32,0x58,0x80,
		0x32,0x59,0x80,
		0x32,0x5A,0x80,
		0x32,0x5B,0x80,
		0x32,0x5C,0x80,
		0x32,0x5D,0x80,
		0x32,0x5E,0x80,
		0x32,0x5F,0x80,
		0x32,0x60,0x80,
		0x32,0x61,0x00,
		0x32,0x62,0x00,
		0x32,0x63,0x00,
		0x32,0x64,0x00,
		0x32,0x65,0x00,
		0x32,0x66,0x00,
		0x32,0x67,0x00,
		0x32,0x68,0x00,
		0x32,0x69,0x00,
		0x32,0x6A,0x00,
		0x32,0x6B,0x00,
		0x32,0x6C,0x00,
		0x32,0x6D,0x00,
		0x32,0x6E,0x00,
		0x32,0x6F,0x00,
		0x32,0x70,0x00,
		0x32,0x71,0x80,
		0x32,0x72,0x00,
		0x32,0x73,0x80,
		0x32,0x74,0x01,
		0x32,0x75,0x00,
		0x32,0x76,0x00,
		0x32,0x82,0xC0,
		0x32,0x84,0x06,
		0x32,0x85,0x03,
		0x32,0x86,0x02,
		0x32,0x8A,0x03,
		0x32,0x8B,0x02,
		0x32,0x90,0x20,
		0x32,0x94,0x10,
		0x32,0xA8,0x84,
		0x32,0xA9,0x02,
		0x32,0xB3,0x10,
		0x32,0xB4,0x1F,
		0x32,0xB7,0x3B,
		0x32,0xBB,0x0F,
		0x32,0xBC,0x0F,
		0x32,0xBE,0x04,
		0x32,0xBF,0x0F,
		0x32,0xC0,0x0F,
		0x32,0xC6,0x50,
		0x32,0xC8,0x0E,
		0x32,0xC9,0x0E,
		0x32,0xCA,0x0E,
		0x32,0xCB,0x0E,
		0x32,0xCC,0x0E,
		0x32,0xCD,0x0E,
		0x32,0xCE,0x08,
		0x32,0xCF,0x08,
		0x32,0xD0,0x08,
		0x32,0xD1,0x0F,
		0x32,0xD2,0x0F,
		0x32,0xD3,0x0F,
		0x32,0xD4,0x08,
		0x32,0xD5,0x08,
		0x32,0xD6,0x08,
		0x32,0xD8,0x00,
		0x32,0xD9,0x00,
		0x32,0xDA,0x00,
		0x32,0xDD,0x02,
		0x32,0xE0,0x20,
		0x32,0xE1,0x20,
		0x32,0xE2,0x20,
		0x32,0xF2,0x04,
		0x32,0xF3,0x04,
		0x33,0x07,0x2E,
		0x33,0x08,0x2D,
		0x33,0x09,0x0D,
		0x33,0x84,0x10,
		0x34,0x24,0x00,
		0x34,0x25,0x78,
		0x34,0x27,0xC0,
		0x34,0x30,0xA7,
		0x34,0x31,0x60,
		0x34,0x32,0x11,
		0x01,0x00,0x01};
	

static void T4K35_Sensor_Init(void)
{

	  T4K35DB("T4K35_Sensor_Init 4lane_OB:\n ");			
	#if 0	
		T4K35_write_cmos_sensor(0x0101,0x00);
		T4K35_write_cmos_sensor(0x0103,0x00);
		T4K35_write_cmos_sensor(0x0104,0x00);
		T4K35_write_cmos_sensor(0x0105,0x01);
		T4K35_write_cmos_sensor(0x0110,0x00);
		T4K35_write_cmos_sensor(0x0111,0x02);
		T4K35_write_cmos_sensor(0x0112,0x0A);
		T4K35_write_cmos_sensor(0x0113,0x0A);
		T4K35_write_cmos_sensor(0x0114,0x03);
		T4K35_write_cmos_sensor(0x0130,0x18);
		T4K35_write_cmos_sensor(0x0131,0x00);
		T4K35_write_cmos_sensor(0x0141,0x00);
		T4K35_write_cmos_sensor(0x0142,0x00);
		T4K35_write_cmos_sensor(0x0143,0x00);
		T4K35_write_cmos_sensor(0x0202,0x09);
		T4K35_write_cmos_sensor(0x0203,0xB2);
		T4K35_write_cmos_sensor(0x0204,0x00);
		T4K35_write_cmos_sensor(0x0205,0x2E);
		T4K35_write_cmos_sensor(0x0210,0x01);
		T4K35_write_cmos_sensor(0x0211,0x00);
		T4K35_write_cmos_sensor(0x0212,0x01);
		T4K35_write_cmos_sensor(0x0213,0x00);
		T4K35_write_cmos_sensor(0x0214,0x01);
		T4K35_write_cmos_sensor(0x0215,0x00);
		T4K35_write_cmos_sensor(0x0216,0x01);
		T4K35_write_cmos_sensor(0x0217,0x00);
		T4K35_write_cmos_sensor(0x0230,0x00);
		T4K35_write_cmos_sensor(0x0301,0x01);
		T4K35_write_cmos_sensor(0x0303,0x05);
		T4K35_write_cmos_sensor(0x0305,0x03);
		T4K35_write_cmos_sensor(0x0306,0x00);
		T4K35_write_cmos_sensor(0x0307,0x6E);
		T4K35_write_cmos_sensor(0x030B,0x00);
		T4K35_write_cmos_sensor(0x030D,0x03);
		T4K35_write_cmos_sensor(0x030E,0x00);
		T4K35_write_cmos_sensor(0x030F,0x6E);
		T4K35_write_cmos_sensor(0x0310,0x00);
		T4K35_write_cmos_sensor(0x0340,0x09);
		T4K35_write_cmos_sensor(0x0341,0xB8);
		T4K35_write_cmos_sensor(0x0342,0x0D);
		T4K35_write_cmos_sensor(0x0343,0xB4);
		T4K35_write_cmos_sensor(0x0344,0x00);
		T4K35_write_cmos_sensor(0x0346,0x00);
		T4K35_write_cmos_sensor(0x0347,0x08);
		T4K35_write_cmos_sensor(0x034A,0x09);
		T4K35_write_cmos_sensor(0x034B,0x98);
		T4K35_write_cmos_sensor(0x034C,0x0C);
		T4K35_write_cmos_sensor(0x034D,0xC0);
		T4K35_write_cmos_sensor(0x034E,0x09);
		T4K35_write_cmos_sensor(0x034F,0x90);
		T4K35_write_cmos_sensor(0x0401,0x00);
		T4K35_write_cmos_sensor(0x0403,0x00);
		T4K35_write_cmos_sensor(0x0404,0x10);
		T4K35_write_cmos_sensor(0x0408,0x00);
		T4K35_write_cmos_sensor(0x0409,0x00);
		T4K35_write_cmos_sensor(0x040A,0x00);
		T4K35_write_cmos_sensor(0x040B,0x00);
		T4K35_write_cmos_sensor(0x040C,0x0C);
		T4K35_write_cmos_sensor(0x040D,0xD0);
		T4K35_write_cmos_sensor(0x040E,0x09);
		T4K35_write_cmos_sensor(0x040F,0xA0);
		T4K35_write_cmos_sensor(0x0601,0x00);
		T4K35_write_cmos_sensor(0x0800,0x88);
		T4K35_write_cmos_sensor(0x0801,0x38);
		T4K35_write_cmos_sensor(0x0802,0x78);
		T4K35_write_cmos_sensor(0x0803,0x48);
		T4K35_write_cmos_sensor(0x0804,0x48);
		T4K35_write_cmos_sensor(0x0805,0x40);
		T4K35_write_cmos_sensor(0x0806,0x00);
		T4K35_write_cmos_sensor(0x0807,0x48);
		T4K35_write_cmos_sensor(0x0808,0x01);
		T4K35_write_cmos_sensor(0x0820,0x0A);
		T4K35_write_cmos_sensor(0x0821,0x20);
		T4K35_write_cmos_sensor(0x0822,0x00);
		T4K35_write_cmos_sensor(0x0823,0x00);
		T4K35_write_cmos_sensor(0x0900,0x00);
		T4K35_write_cmos_sensor(0x0901,0x00);
		T4K35_write_cmos_sensor(0x0902,0x00);
		T4K35_write_cmos_sensor(0x0A05,0x01);
		T4K35_write_cmos_sensor(0x0A06,0x01);
		T4K35_write_cmos_sensor(0x0A07,0x98);
		T4K35_write_cmos_sensor(0x0A0A,0x01);
		T4K35_write_cmos_sensor(0x0A0B,0x98);
		T4K35_write_cmos_sensor(0x0F00,0x00);
		T4K35_write_cmos_sensor(0x300A,0x00);
		T4K35_write_cmos_sensor(0x301A,0x44);
		T4K35_write_cmos_sensor(0x301B,0x44);
		T4K35_write_cmos_sensor(0x3024,0x01);
		T4K35_write_cmos_sensor(0x3025,0x83);
		T4K35_write_cmos_sensor(0x3037,0x06);
		T4K35_write_cmos_sensor(0x3038,0x06);
		T4K35_write_cmos_sensor(0x3039,0x06);
		T4K35_write_cmos_sensor(0x303A,0x06);
		T4K35_write_cmos_sensor(0x303B,0x0B);
		T4K35_write_cmos_sensor(0x303C,0x03);
		T4K35_write_cmos_sensor(0x303D,0x03);
		T4K35_write_cmos_sensor(0x3053,0xC0);
		T4K35_write_cmos_sensor(0x305D,0x10);
		T4K35_write_cmos_sensor(0x305E,0x06);
		T4K35_write_cmos_sensor(0x306B,0x08);
		T4K35_write_cmos_sensor(0x3073,0x1C);
		T4K35_write_cmos_sensor(0x3074,0x0F);
		T4K35_write_cmos_sensor(0x3075,0x03);
		T4K35_write_cmos_sensor(0x3076,0x7F);
		T4K35_write_cmos_sensor(0x307E,0x02);
		T4K35_write_cmos_sensor(0x308D,0x03);
		T4K35_write_cmos_sensor(0x308E,0x20);
		T4K35_write_cmos_sensor(0x3091,0x04);
		T4K35_write_cmos_sensor(0x3096,0x75);
		T4K35_write_cmos_sensor(0x3097,0x7E);
		T4K35_write_cmos_sensor(0x3098,0x20);
		T4K35_write_cmos_sensor(0x30A0,0x82);
		T4K35_write_cmos_sensor(0x30AB,0x30);
		T4K35_write_cmos_sensor(0x30CE,0x65);
		T4K35_write_cmos_sensor(0x30CF,0x75);
		T4K35_write_cmos_sensor(0x30D2,0xB3);
		T4K35_write_cmos_sensor(0x30D5,0x09);
		T4K35_write_cmos_sensor(0x3134,0x01);
		T4K35_write_cmos_sensor(0x314D,0x80);
		T4K35_write_cmos_sensor(0x315B,0x22);
		T4K35_write_cmos_sensor(0x315C,0x22);
		T4K35_write_cmos_sensor(0x315D,0x02);
		T4K35_write_cmos_sensor(0x3165,0x67);
		T4K35_write_cmos_sensor(0x3168,0xF1);
		T4K35_write_cmos_sensor(0x3169,0x77);
		T4K35_write_cmos_sensor(0x316A,0x77);
		T4K35_write_cmos_sensor(0x3173,0x30);
		T4K35_write_cmos_sensor(0x31C1,0x27);
		T4K35_write_cmos_sensor(0x31DB,0x15);
		T4K35_write_cmos_sensor(0x31DC,0xE0);
		T4K35_write_cmos_sensor(0x3204,0x00);
		T4K35_write_cmos_sensor(0x3231,0x00);
		T4K35_write_cmos_sensor(0x3232,0x00);
		T4K35_write_cmos_sensor(0x3233,0x00);
		T4K35_write_cmos_sensor(0x3234,0x00);
		T4K35_write_cmos_sensor(0x3237,0x00);
		T4K35_write_cmos_sensor(0x3238,0x00);
		T4K35_write_cmos_sensor(0x3239,0x80);
		T4K35_write_cmos_sensor(0x323A,0x80);
		T4K35_write_cmos_sensor(0x323B,0x00);
		T4K35_write_cmos_sensor(0x323C,0x81);
		T4K35_write_cmos_sensor(0x323D,0x00);
		T4K35_write_cmos_sensor(0x323E,0x02);
		T4K35_write_cmos_sensor(0x323F,0x00);
		T4K35_write_cmos_sensor(0x3240,0x00);
		T4K35_write_cmos_sensor(0x3241,0x00);
		T4K35_write_cmos_sensor(0x3242,0x00);
		T4K35_write_cmos_sensor(0x3243,0x00);
		T4K35_write_cmos_sensor(0x3244,0x00);
		T4K35_write_cmos_sensor(0x3245,0x00);
		T4K35_write_cmos_sensor(0x3246,0x00);
		T4K35_write_cmos_sensor(0x3247,0x00);
		T4K35_write_cmos_sensor(0x3248,0x00);
		T4K35_write_cmos_sensor(0x3249,0x00);
		T4K35_write_cmos_sensor(0x324A,0x00);
		T4K35_write_cmos_sensor(0x324B,0x00);
		T4K35_write_cmos_sensor(0x324C,0x00);
		T4K35_write_cmos_sensor(0x324D,0x00);
		T4K35_write_cmos_sensor(0x324E,0x00);
		T4K35_write_cmos_sensor(0x324F,0x00);
		T4K35_write_cmos_sensor(0x3250,0x00);
		T4K35_write_cmos_sensor(0x3251,0x80);
		T4K35_write_cmos_sensor(0x3252,0x80);
		T4K35_write_cmos_sensor(0x3253,0x80);
		T4K35_write_cmos_sensor(0x3254,0x80);
		T4K35_write_cmos_sensor(0x3255,0x80);
		T4K35_write_cmos_sensor(0x3256,0x80);
		T4K35_write_cmos_sensor(0x3257,0x80);
		T4K35_write_cmos_sensor(0x3258,0x80);
		T4K35_write_cmos_sensor(0x3259,0x80);
		T4K35_write_cmos_sensor(0x325A,0x80);
		T4K35_write_cmos_sensor(0x325B,0x80);
		T4K35_write_cmos_sensor(0x325C,0x80);
		T4K35_write_cmos_sensor(0x325D,0x80);
		T4K35_write_cmos_sensor(0x325E,0x80);
		T4K35_write_cmos_sensor(0x325F,0x80);
		T4K35_write_cmos_sensor(0x3260,0x80);
		T4K35_write_cmos_sensor(0x3261,0x00);
		T4K35_write_cmos_sensor(0x3262,0x00);
		T4K35_write_cmos_sensor(0x3263,0x00);
		T4K35_write_cmos_sensor(0x3264,0x00);
		T4K35_write_cmos_sensor(0x3265,0x00);
		T4K35_write_cmos_sensor(0x3266,0x00);
		T4K35_write_cmos_sensor(0x3267,0x00);
		T4K35_write_cmos_sensor(0x3268,0x00);
		T4K35_write_cmos_sensor(0x3269,0x00);
		T4K35_write_cmos_sensor(0x326A,0x00);
		T4K35_write_cmos_sensor(0x326B,0x00);
		T4K35_write_cmos_sensor(0x326C,0x00);
		T4K35_write_cmos_sensor(0x326D,0x00);
		T4K35_write_cmos_sensor(0x326E,0x00);
		T4K35_write_cmos_sensor(0x326F,0x00);
		T4K35_write_cmos_sensor(0x3270,0x00);
		T4K35_write_cmos_sensor(0x3271,0x80);
		T4K35_write_cmos_sensor(0x3272,0x00);
		T4K35_write_cmos_sensor(0x3273,0x80);
		T4K35_write_cmos_sensor(0x3274,0x01);
		T4K35_write_cmos_sensor(0x3275,0x00);
		T4K35_write_cmos_sensor(0x3276,0x00);
		T4K35_write_cmos_sensor(0x3282,0xC0);
		T4K35_write_cmos_sensor(0x3284,0x06);
		T4K35_write_cmos_sensor(0x3285,0x03);
		T4K35_write_cmos_sensor(0x3286,0x02);
		T4K35_write_cmos_sensor(0x328A,0x03);
		T4K35_write_cmos_sensor(0x328B,0x02);
		T4K35_write_cmos_sensor(0x3290,0x20);
		T4K35_write_cmos_sensor(0x3294,0x10);
		T4K35_write_cmos_sensor(0x32A8,0x84);
		T4K35_write_cmos_sensor(0x32A9,0x02);
		T4K35_write_cmos_sensor(0x32B3,0x10);
		T4K35_write_cmos_sensor(0x32B4,0x1F);
		T4K35_write_cmos_sensor(0x32B7,0x3B);
		T4K35_write_cmos_sensor(0x32BB,0x0F);
		T4K35_write_cmos_sensor(0x32BC,0x0F);
		T4K35_write_cmos_sensor(0x32BE,0x04);
		T4K35_write_cmos_sensor(0x32BF,0x0F);
		T4K35_write_cmos_sensor(0x32C0,0x0F);
		T4K35_write_cmos_sensor(0x32C6,0x50);
		T4K35_write_cmos_sensor(0x32C8,0x0E);
		T4K35_write_cmos_sensor(0x32C9,0x0E);
		T4K35_write_cmos_sensor(0x32CA,0x0E);
		T4K35_write_cmos_sensor(0x32CB,0x0E);
		T4K35_write_cmos_sensor(0x32CC,0x0E);
		T4K35_write_cmos_sensor(0x32CD,0x0E);
		T4K35_write_cmos_sensor(0x32CE,0x08);
		T4K35_write_cmos_sensor(0x32CF,0x08);
		T4K35_write_cmos_sensor(0x32D0,0x08);
		T4K35_write_cmos_sensor(0x32D1,0x0F);
		T4K35_write_cmos_sensor(0x32D2,0x0F);
		T4K35_write_cmos_sensor(0x32D3,0x0F);
		T4K35_write_cmos_sensor(0x32D4,0x08);
		T4K35_write_cmos_sensor(0x32D5,0x08);
		T4K35_write_cmos_sensor(0x32D6,0x08);
		T4K35_write_cmos_sensor(0x32D8,0x00);
		T4K35_write_cmos_sensor(0x32D9,0x00);
		T4K35_write_cmos_sensor(0x32DA,0x00);
		T4K35_write_cmos_sensor(0x32DD,0x02);
		T4K35_write_cmos_sensor(0x32E0,0x20);
		T4K35_write_cmos_sensor(0x32E1,0x20);
		T4K35_write_cmos_sensor(0x32E2,0x20);
		T4K35_write_cmos_sensor(0x32F2,0x04);
		T4K35_write_cmos_sensor(0x32F3,0x04);
		T4K35_write_cmos_sensor(0x3307,0x2E);
		T4K35_write_cmos_sensor(0x3308,0x2D);
		T4K35_write_cmos_sensor(0x3309,0x0D);
		T4K35_write_cmos_sensor(0x3384,0x10);
		T4K35_write_cmos_sensor(0x3424,0x00);
		T4K35_write_cmos_sensor(0x3425,0x78);
		T4K35_write_cmos_sensor(0x3427,0xC0);
		T4K35_write_cmos_sensor(0x3430,0xA7);
		T4K35_write_cmos_sensor(0x3431,0x60);
		T4K35_write_cmos_sensor(0x3432,0x11);
		T4K35_write_cmos_sensor(0x0100,0x01);
#else

	  int totalCnt = 0, len = 0;
	  int transfer_len, transac_len=3;
	  kal_uint8* pBuf=NULL;
	  dma_addr_t dmaHandle;
	  pBuf = (kal_uint8*)kmalloc(1024, GFP_KERNEL);
	
      totalCnt = ARRAY_SIZE(T4K35_init);
	  transfer_len = totalCnt / transac_len;
      len = (transfer_len<<8)|transac_len;

	  T4K35DB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	  memcpy(pBuf, &T4K35_init, totalCnt );   
	  dmaHandle = dma_map_single(NULL, pBuf, 1024, DMA_TO_DEVICE);	
	  T4K35_multi_write_cmos_sensor(dmaHandle, len); 
	  dma_unmap_single(NULL, dmaHandle, 1024, DMA_TO_DEVICE);

#endif
	  
      T4K35DB("T4K35_Sensor_Init exit :\n ");
	
}   /*  T4K35_Sensor_Init  */
static kal_uint8 T4K35_Preview[]={
	
    0x01,0x04,0x01,
    0x03,0x40,0x09,
    0x03,0x41,0xB8,
    0x03,0x42,0x0D,
    0x03,0x43,0xB4,
	0x03,0x46,0x00,
	0x03,0x47,0x08,
	0x03,0x4A,0x09,
	0x03,0x4B,0xA0,//98
    0x03,0x4C,0x06,
    0x03,0x4D,0x60,
    0x03,0x4E,0x04,
    0x03,0x4F,0xC8,
    0x03,0x01,0x01,
    0x04,0x01,0x02,
    0x04,0x04,0x10,
    0x09,0x00,0x01,
	0x09,0x01,0x01,
    0x01,0x04,0x00
};

void T4K35PreviewSetting(void)
{
    T4K35DB("T4K35Preview setting:");
#if 0
    T4K35_write_cmos_sensor(0x0104,0x01);
    T4K35_write_cmos_sensor(0x0340,0x09);
    T4K35_write_cmos_sensor(0x0341,0xB8);
    T4K35_write_cmos_sensor(0x0342,0x0D);
    T4K35_write_cmos_sensor(0x0343,0xB4);
	
	T4K35_write_cmos_sensor(0x0346,0x00);
	T4K35_write_cmos_sensor(0x0347,0x08);
	T4K35_write_cmos_sensor(0x034A,0x09);
	T4K35_write_cmos_sensor(0x034B,0x98);
	
    T4K35_write_cmos_sensor(0x034C,0x06);
    T4K35_write_cmos_sensor(0x034D,0x60);
    T4K35_write_cmos_sensor(0x034E,0x04);
    T4K35_write_cmos_sensor(0x034F,0xC8);
    T4K35_write_cmos_sensor(0x0301,0x01);
    T4K35_write_cmos_sensor(0x0401,0x02);
    T4K35_write_cmos_sensor(0x0404,0x10);
    T4K35_write_cmos_sensor(0x0900,0x01);
	T4K35_write_cmos_sensor(0x0901,0x01);
    T4K35_write_cmos_sensor(0x0104,0x00);
#else
      int totalCnt = 0, len = 0;
	  int transfer_len, transac_len=3;
	  kal_uint8* pBuf=NULL;
	  dma_addr_t dmaHandle;
	  pBuf = (kal_uint8*)kmalloc(1024, GFP_KERNEL);
	
      totalCnt = ARRAY_SIZE(T4K35_Preview);
	  transfer_len = totalCnt / transac_len;
      len = (transfer_len<<8)|transac_len;

	  T4K35DB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	  memcpy(pBuf, &T4K35_Preview, totalCnt );   
	  dmaHandle = dma_map_single(NULL, pBuf, 1024, DMA_TO_DEVICE);	
	  T4K35_multi_write_cmos_sensor(dmaHandle, len); 
	  dma_unmap_single(NULL, dmaHandle, 1024, DMA_TO_DEVICE);
#endif

}

static kal_uint8 T4K35_Video[]={
	
	0x01,0x04,0x01,
	0x03,0x40,0x09,
	0x03,0x41,0xB8,
	0x03,0x42,0x0D,
	0x03,0x43,0xB4,
	0x03,0x46,0x01,
	0x03,0x47,0x3A,
	0x03,0x4A,0x08,
	0x03,0x4B,0x66,
	0x03,0x4C,0x0C,
	0x03,0x4D,0xC0,
	0x03,0x4E,0x07,
	0x03,0x4F,0x2C,
	0x03,0x01,0x01,
	0x04,0x01,0x02,
	0x04,0x04,0x10,
	0x09,0x00,0x00,
	0x09,0x01,0x00,
	0x01,0x04,0x00,
};

void T4K35VideoSetting(void)
{
	T4K35DB("T4K35VideoSetting_16:9 exit :\n ");
#if 0
	T4K35_write_cmos_sensor(0x0104,0x01);
	T4K35_write_cmos_sensor(0x0340,0x09);
	T4K35_write_cmos_sensor(0x0341,0xB8);
	T4K35_write_cmos_sensor(0x0342,0x0D);
	T4K35_write_cmos_sensor(0x0343,0xB4);
	
	T4K35_write_cmos_sensor(0x0346,0x01);
	T4K35_write_cmos_sensor(0x0347,0x3A);
	T4K35_write_cmos_sensor(0x034A,0x08);
	T4K35_write_cmos_sensor(0x034B,0x66);
	
	T4K35_write_cmos_sensor(0x034C,0x0C);//0A
	T4K35_write_cmos_sensor(0x034D,0xC0);//BC
	T4K35_write_cmos_sensor(0x034E,0x07);//08
	T4K35_write_cmos_sensor(0x034F,0x2C);//0D
	
	T4K35_write_cmos_sensor(0x0301,0x01);
	T4K35_write_cmos_sensor(0x0401,0x02);
	T4K35_write_cmos_sensor(0x0404,0x10);
	T4K35_write_cmos_sensor(0x0900,0x00);
	T4K35_write_cmos_sensor(0x0901,0x00);
	T4K35_write_cmos_sensor(0x0104,0x00);
 #else
      int totalCnt = 0, len = 0;
	  int transfer_len, transac_len=3;
	  kal_uint8* pBuf=NULL;
	  dma_addr_t dmaHandle;
	  pBuf = (kal_uint8*)kmalloc(1024, GFP_KERNEL);
	
      totalCnt = ARRAY_SIZE(T4K35_Video);
	  transfer_len = totalCnt / transac_len;
      len = (transfer_len<<8)|transac_len;

	  T4K35DB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	  memcpy(pBuf, &T4K35_Video, totalCnt );   
	  dmaHandle = dma_map_single(NULL, pBuf, 1024, DMA_TO_DEVICE);	
	  T4K35_multi_write_cmos_sensor(dmaHandle, len); 
	  dma_unmap_single(NULL, dmaHandle, 1024, DMA_TO_DEVICE);
 #endif
		     
}

static kal_uint8 T4K35_Capture[]={

      0x01,0x04,0x01,
      0x03,0x40,0x09,
      0x03,0x41,0xB8,
	  0x03,0x42,0x0D,
      0x03,0x43,0xB4,
      0x03,0x46,0x00,
      0x03,0x47,0x0A,//08
      0x03,0x4A,0x09,
      0x03,0x4B,0xA0,//98
      0x03,0x4C,0x0C,
      0x03,0x4D,0xC0,
      0x03,0x4E,0x09,
      0x03,0x4F,0x90,
      0x03,0x01,0x01,
      0x04,0x01,0x00,
      0x04,0x04,0x10,
      0x09,0x00,0x00,
      0x09,0x01,0x00,
      0x01,0x04,0x00};

void T4K35CaptureSetting(void)
{
      T4K35DB("T4K35capture setting:");
 #if 0     
	  T4K35_write_cmos_sensor(0x0104,0x01);
	  T4K35_write_cmos_sensor(0x0340,0x09);
	  T4K35_write_cmos_sensor(0x0341,0xB8);
	  T4K35_write_cmos_sensor(0x0342,0x0D);
	  T4K35_write_cmos_sensor(0x0343,0xB4);

	  T4K35_write_cmos_sensor(0x0346,0x00);
	  T4K35_write_cmos_sensor(0x0347,0x08);
	  T4K35_write_cmos_sensor(0x034A,0x09);
	  T4K35_write_cmos_sensor(0x034B,0x98);
	  
	  T4K35_write_cmos_sensor(0x034C,0x0C);
	  T4K35_write_cmos_sensor(0x034D,0xC0);
	  T4K35_write_cmos_sensor(0x034E,0x09);
	  T4K35_write_cmos_sensor(0x034F,0x90);
	  T4K35_write_cmos_sensor(0x0301,0x01);
	  T4K35_write_cmos_sensor(0x0401,0x00);
	  T4K35_write_cmos_sensor(0x0404,0x10);
	  T4K35_write_cmos_sensor(0x0900,0x00);
	  T4K35_write_cmos_sensor(0x0901,0x00);
	  T4K35_write_cmos_sensor(0x0104,0x00);
 #else
      int totalCnt = 0, len = 0;
	  int transfer_len, transac_len=3;
	  kal_uint8* pBuf=NULL;
	  dma_addr_t dmaHandle;
	  pBuf = (kal_uint8*)kmalloc(1024, GFP_KERNEL);
	
      totalCnt = ARRAY_SIZE(T4K35_Capture);
	  transfer_len = totalCnt / transac_len;
      len = (transfer_len<<8)|transac_len;

	  T4K35DB("Total Count = %d, Len = 0x%x\n", totalCnt,len);    
	  memcpy(pBuf, &T4K35_Capture, totalCnt );   
	  dmaHandle = dma_map_single(NULL, pBuf, 1024, DMA_TO_DEVICE);	
	  T4K35_multi_write_cmos_sensor(dmaHandle, len); 
	  dma_unmap_single(NULL, dmaHandle, 1024, DMA_TO_DEVICE);
  #endif
	  
}

/*************************************************************************
* FUNCTION
*   T4K35Open
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

UINT32 T4K35Open(void)
{

	volatile signed int i;
	kal_uint16 sensor_id = 0;

	T4K35DB("T4K35Open enter :\n ");

    mDELAY(2);

	//  Read sensor ID to adjust I2C is OK?
	for(i=0;i<3;i++)
	{
		sensor_id = (T4K35_read_cmos_sensor(0x0000)<<8)|T4K35_read_cmos_sensor(0x0001);
		T4K35DB("OT4K35 READ ID :%x",sensor_id);
		if(sensor_id != T4K35_SENSOR_ID)
		{
			return ERROR_SENSOR_CONNECT_FAIL;
		}else
			break;
	}
	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.sensorMode = SENSOR_MODE_INIT;
	t4k35.T4K35AutoFlickerMode = KAL_FALSE;
	t4k35.T4K35VideoMode = KAL_FALSE;
	spin_unlock(&t4k35mipiraw_drv_lock);
	T4K35_Sensor_Init();
	
	#ifdef OTP_TEST
	t4k35_otp_init_setting();
	#endif

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.DummyLines= 0;
	t4k35.DummyPixels= 0;
	t4k35.pvPclk =  (2640); 
	t4k35.videoPclk = (2640);
	t4k35.capPclk = (2640);

	t4k35.shutter = 0x4EA;
	t4k35.pvShutter = 0x4EA;
	t4k35.maxExposureLines =T4K35_PV_PERIOD_LINE_NUMS;

	t4k35.ispBaseGain = BASEGAIN;//0x40
	t4k35.sensorGlobalGain = 0x1000;//sensor gain 1x
	t4k35.pvGain = 0x1000;
	T4K35DuringTestPattern = KAL_FALSE;
	spin_unlock(&t4k35mipiraw_drv_lock);

	T4K35DB("T4K35Open exit :\n ");

    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   T4K35GetSensorID
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
UINT32 T4K35GetSensorID(UINT32 *sensorID)
{
    int  retry = 1;

	T4K35DB("T4K35GetSensorID enter :\n ");
    // check if sensor ID correct
    do {
        *sensorID =(T4K35_read_cmos_sensor(0x0000)<<8)|T4K35_read_cmos_sensor(0x0001);
        if (*sensorID == T4K35_SENSOR_ID)
        	{
        		T4K35DB("Sensor ID = 0x%04x\n", *sensorID);
            	break;
        	}
        T4K35DB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
        retry--;
    } while (retry > 0);

    if (*sensorID != T4K35_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   T4K35_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of T4K35 to change exposure time.
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
void T4K35_write_shutter(kal_uint32 shutter)
{

}	/* write_T4K35_shutter */

void T4K35_SetShutter(kal_uint32 iShutter)
{
   T4K35DB("T4K35MIPI_SetShutter =%d \n ",iShutter);
   kal_uint16 frame_length=0 ;
   if (iShutter < 1)
	   iShutter = 1; 
   //T4K35DB("T4K35.sensorMode=%d \n ",t4k35.sensorMode);
   
   if ( SENSOR_MODE_PREVIEW== t4k35.sensorMode ) 
   {
	   frame_length = T4K35_PV_PERIOD_LINE_NUMS+t4k35.DummyLines;
   }
   else if( SENSOR_MODE_VIDEO== t4k35.sensorMode )
   {
	   frame_length = T4K35_VIDEO_PERIOD_LINE_NUMS+t4k35.DummyLines;
   }
   else if( SENSOR_MODE_CAPTURE== t4k35.sensorMode)
   {
	   frame_length = T4K35_FULL_PERIOD_LINE_NUMS + t4k35.DummyLines;
   }
   
   //T4K35_write_cmos_sensor(0x0104, 1);
   
   if(iShutter>frame_length-6)
   	{   
   	    frame_length=iShutter+6;
	    T4K35_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
        T4K35_write_cmos_sensor(0x0341, frame_length& 0xFF);	        
   	}
   else
   	{
	    T4K35_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
        T4K35_write_cmos_sensor(0x0341, frame_length& 0xFF);	        
  
   	}
   spin_lock(&t4k35mipiraw_drv_lock);
   t4k35.shutter= iShutter;
   T4K35_FeatureControl_PERIOD_LineNum=frame_length;
   spin_unlock(&t4k35mipiraw_drv_lock);

   T4K35_write_cmos_sensor(0x0202, (iShutter >> 8) & 0xFF);
   T4K35_write_cmos_sensor(0x0203, iShutter	& 0xFF);
   
   //T4K35_write_cmos_sensor(0x0104, 0);	 

}   /*  T4K35_SetShutter   */


/*************************************************************************
* FUNCTION
*   T4K35_read_shutter
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
UINT32 T4K35_read_shutter(void)
{

    kal_uint16 ishutter;

    ishutter =(T4K35_read_cmos_sensor(0x0202)<<8)|T4K35_read_cmos_sensor(0x0203);

    T4K35DB("T4K35_read_shutter (0x%x)\n",ishutter);

    return ishutter;

}

/*************************************************************************
* FUNCTION
*   T4K35_night_mode
*
* DESCRIPTION
*   This function night mode of T4K35.
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
void T4K35_NightMode(kal_bool bEnable)
{
}/*	T4K35_NightMode */



/*************************************************************************
* FUNCTION
*   T4K35Close
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
UINT32 T4K35Close(void)
{
    //  CISModulePowerOn(FALSE);
    //s_porting
    //  DRV_I2CClose(T4K35hDrvI2C);
    //e_porting
    ReEnteyCamera = KAL_FALSE;
    return ERROR_NONE;
}	/* T4K35Close() */

void T4K35SetFlipMirror(kal_int32 imgMirror)
{

}


/*************************************************************************
* FUNCTION
*   T4K35Preview
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
UINT32 T4K35Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	T4K35DB("T4K35Preview enter:");

	// preview size
	if(t4k35.sensorMode == SENSOR_MODE_PREVIEW)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		//T4K35DB("T4K35Preview setting!!\n");
		T4K35PreviewSetting();
	}
	
	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	t4k35.DummyPixels = 0;//define dummy pixels and lines
	t4k35.DummyLines = 0 ;
	T4K35_FeatureControl_PERIOD_PixelNum=T4K35_PV_PERIOD_PIXEL_NUMS+ t4k35.DummyPixels;
	T4K35_FeatureControl_PERIOD_LineNum=T4K35_PV_PERIOD_LINE_NUMS+t4k35.DummyLines;
	spin_unlock(&t4k35mipiraw_drv_lock);

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k35mipiraw_drv_lock);
	//T4K35SetFlipMirror(sensor_config_data->SensorImageMirror);

	T4K35DBSOFIA("[T4K35Preview]frame_len=%x\n", T4K35_read_cmos_sensor(0x0340));

    mDELAY(40);
	T4K35DB("T4K35Preview exit:\n");
    return ERROR_NONE;
}	/* T4K35Preview() */



UINT32 T4K35Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	T4K35DB("T4K35Video enter:");

	if(t4k35.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
	{
		T4K35VideoSetting();

	}
	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.sensorMode = SENSOR_MODE_VIDEO;
	t4k35.DummyPixels = 0;//define dummy pixels and lines
	t4k35.DummyLines = 0 ;
	T4K35_FeatureControl_PERIOD_PixelNum=T4K35_VIDEO_PERIOD_PIXEL_NUMS+ t4k35.DummyPixels;
	T4K35_FeatureControl_PERIOD_LineNum=T4K35_VIDEO_PERIOD_LINE_NUMS+t4k35.DummyLines;
	spin_unlock(&t4k35mipiraw_drv_lock);

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k35mipiraw_drv_lock);
	//T4K35SetFlipMirror(sensor_config_data->SensorImageMirror);

	T4K35DBSOFIA("[T4K35Video]frame_len=%x\n", T4K35_read_cmos_sensor(0x0340));

    mDELAY(40);
	T4K35DB("T4K35Video exit:\n");
    return ERROR_NONE;
}


UINT32 T4K35Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	if( SENSOR_MODE_CAPTURE== t4k35.sensorMode)
	{
		T4K35DB("T4K35Capture BusrtShot!!!\n");
	}else{
	T4K35DB("T4K35Capture enter:\n");
	
	T4K35CaptureSetting();

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.sensorMode = SENSOR_MODE_CAPTURE;
	t4k35.imgMirror = sensor_config_data->SensorImageMirror;
	t4k35.DummyPixels = 0;//define dummy pixels and lines
	t4k35.DummyLines = 0 ;
	T4K35_FeatureControl_PERIOD_PixelNum = T4K35_FULL_PERIOD_PIXEL_NUMS + t4k35.DummyPixels;
	T4K35_FeatureControl_PERIOD_LineNum = T4K35_FULL_PERIOD_LINE_NUMS + t4k35.DummyLines;
	spin_unlock(&t4k35mipiraw_drv_lock);

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k35mipiraw_drv_lock);
	//T4K35SetFlipMirror(sensor_config_data->SensorImageMirror);
  }
    return ERROR_NONE;

}	/* T4K35Capture() */

UINT32 T4K35GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    T4K35DB("T4K35GetResolution!!\n");

	pSensorResolution->SensorPreviewWidth	= T4K35_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= T4K35_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorFullWidth		= T4K35_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= T4K35_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorVideoWidth		= T4K35_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = T4K35_IMAGE_SENSOR_VIDEO_HEIGHT;
	
    return ERROR_NONE;
}   /* T4K35GetResolution() */

UINT32 T4K35GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{      switch(ScenarioId)
	   {
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorPreviewResolutionX=T4K35_IMAGE_SENSOR_FULL_WIDTH;
			pSensorInfo->SensorPreviewResolutionY=T4K35_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  T4K35_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  T4K35_IMAGE_SENSOR_FULL_HEIGHT; 			
			pSensorInfo->SensorCameraPreviewFrameRate=30;
		break;

        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	pSensorInfo->SensorPreviewResolutionX=T4K35_IMAGE_SENSOR_PV_WIDTH;
       		pSensorInfo->SensorPreviewResolutionY=T4K35_IMAGE_SENSOR_PV_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  T4K35_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  T4K35_IMAGE_SENSOR_FULL_HEIGHT;       		
			pSensorInfo->SensorCameraPreviewFrameRate=30;            
            break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        	pSensorInfo->SensorPreviewResolutionX=T4K35_IMAGE_SENSOR_VIDEO_WIDTH;
       		pSensorInfo->SensorPreviewResolutionY=T4K35_IMAGE_SENSOR_VIDEO_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  T4K35_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  T4K35_IMAGE_SENSOR_FULL_HEIGHT;       		
			pSensorInfo->SensorCameraPreviewFrameRate=30;  
			break;
		default:
        	pSensorInfo->SensorPreviewResolutionX=T4K35_IMAGE_SENSOR_PV_WIDTH;
       		pSensorInfo->SensorPreviewResolutionY=T4K35_IMAGE_SENSOR_PV_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  T4K35_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  T4K35_IMAGE_SENSOR_FULL_HEIGHT;       		
			pSensorInfo->SensorCameraPreviewFrameRate=30;
		break;
	}

	spin_lock(&t4k35mipiraw_drv_lock);
	t4k35.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&t4k35mipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gr;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
	pSensorInfo->MIPIsensorType=MIPI_OPHY_CSI2;

    pSensorInfo->CaptureDelayFrame = 2;
    pSensorInfo->PreviewDelayFrame = 2;
    pSensorInfo->VideoDelayFrame = 2;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame =1;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;
	  

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K35_PV_X_START;
            pSensorInfo->SensorGrabStartY = T4K35_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K35_VIDEO_X_START;
            pSensorInfo->SensorGrabStartY = T4K35_VIDEO_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K35_FULL_X_START;	//2*T4K35_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = T4K35_FULL_Y_START;	//2*T4K35_IMAGE_SENSOR_PV_STARTY;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;

            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K35_PV_X_START;
            pSensorInfo->SensorGrabStartY = T4K35_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;

            break;
    }

    memcpy(pSensorConfigData, &T4K35SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* T4K35GetInfo() */


UINT32 T4K35Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&t4k35mipiraw_drv_lock);
		T4K35CurrentScenarioId = ScenarioId;
		spin_unlock(&t4k35mipiraw_drv_lock);
		T4K35DB("T4K35CurrentScenarioId=%d\n",T4K35CurrentScenarioId);

	switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            T4K35Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			T4K35Video(pImageWindow, pSensorConfigData);
			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            T4K35Capture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
} /* T4K35Control() */


UINT32 T4K35SetVideoMode(UINT16 u2FrameRate)
{
    kal_uint32 MIN_Frame_length =0,frameRate=0,extralines=0;
    T4K35DB("[T4K35SetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&t4k35mipiraw_drv_lock);
	VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&t4k35mipiraw_drv_lock);

	if(u2FrameRate==0)
	{
		T4K35DB("Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    T4K35DB("error frame rate seting\n");

    if(t4k35.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(t4k35.T4K35AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 306;
			else if(u2FrameRate==15)
				frameRate= 148;//148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (t4k35.videoPclk*100000)/(T4K35_VIDEO_PERIOD_PIXEL_NUMS + t4k35.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (t4k35.videoPclk*100000) /(T4K35_VIDEO_PERIOD_PIXEL_NUMS + t4k35.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=T4K35_VIDEO_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = T4K35_VIDEO_PERIOD_LINE_NUMS;
			T4K35DB("[T4K35SetVideoMode]current fps = %d\n", (t4k35.pvPclk*100000)  /(T4K35_VIDEO_PERIOD_PIXEL_NUMS)/T4K35_VIDEO_PERIOD_LINE_NUMS);
		}
		T4K35DB("[T4K35SetVideoMode]current fps (10 base)= %d\n", (t4k35.pvPclk*100000)*10/(T4K35_VIDEO_PERIOD_PIXEL_NUMS + t4k35.DummyPixels)/MIN_Frame_length);
		extralines = MIN_Frame_length - T4K35_VIDEO_PERIOD_LINE_NUMS;
		T4K35DB("[T4K35SetVideoMode]extralines= %d\n", extralines);
		spin_lock(&t4k35mipiraw_drv_lock);
		t4k35.DummyPixels = 0;//define dummy pixels and lines
		t4k35.DummyLines = extralines ;
		spin_unlock(&t4k35mipiraw_drv_lock);
		
        //T4K35_write_cmos_sensor(0x0104, 1); 
	    T4K35_write_cmos_sensor(0x0340, (MIN_Frame_length >>8) & 0xFF);
	    T4K35_write_cmos_sensor(0x0341, MIN_Frame_length& 0xFF);

		if(t4k35.shutter>MIN_Frame_length-6)
			{
			  t4k35.shutter=MIN_Frame_length-6;
			  T4K35_write_cmos_sensor(0x0202, (t4k35.shutter >> 8) & 0xFF);
              T4K35_write_cmos_sensor(0x0203, t4k35.shutter  & 0xFF);
			}
	    //T4K35_write_cmos_sensor(0x0104, 0); 
    }
	T4K35DB("[T4K35SetVideoMode]MIN_Frame_length=%d,t4k35.DummyLines=%d\n",MIN_Frame_length,t4k35.DummyLines);

    return KAL_TRUE;
}

UINT32 T4K35SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
   
    if (KAL_TRUE == T4K35DuringTestPattern) return ERROR_NONE;
	if(bEnable) {   // enable auto flicker
		spin_lock(&t4k35mipiraw_drv_lock);
		t4k35.T4K35AutoFlickerMode = KAL_TRUE;
		spin_unlock(&t4k35mipiraw_drv_lock);
    } else {
    	spin_lock(&t4k35mipiraw_drv_lock);
        t4k35.T4K35AutoFlickerMode = KAL_FALSE;
		spin_unlock(&t4k35mipiraw_drv_lock);
        T4K35DB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 T4K35SetTestPatternMode(kal_bool bEnable)
{
    T4K35DB("[T4K35SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	kal_uint16 temp;

	if(bEnable) 
	{
        spin_lock(&t4k35mipiraw_drv_lock);
	    T4K35DuringTestPattern = KAL_TRUE;
	    spin_unlock(&t4k35mipiraw_drv_lock);
		T4K35_write_cmos_sensor(0x0601,0x02);
	}
   else		
	{
		T4K35_write_cmos_sensor(0x0601,0x00);	
	}

    return ERROR_NONE;
}

UINT32 T4K35MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {


	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	T4K35DB("T4K35MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = 264000000;
			lineLength = T4K35_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K35_PV_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k35mipiraw_drv_lock);
			t4k35.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&t4k35mipiraw_drv_lock);
			T4K35_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = 264000000;
			lineLength = T4K35_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K35_VIDEO_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k35mipiraw_drv_lock);
			t4k35.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&t4k35mipiraw_drv_lock);
			T4K35_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = 264000000;
			lineLength = T4K35_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K35_FULL_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k35mipiraw_drv_lock);
			t4k35.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&t4k35mipiraw_drv_lock);
			T4K35_SetDummy(0, dummyLine);			
			break;			
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 T4K35MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 300;
			break;			
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 T4K35FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
	   MSDK_SENSOR_ENG_INFO_STRUCT	  *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;
	
	   T4K35DB("T4K35_FeatureControl FeatureId(%d)\n",FeatureId);
	
	   switch (FeatureId)
	   {
		   case SENSOR_FEATURE_GET_RESOLUTION:
			   *pFeatureReturnPara16++=T4K35_IMAGE_SENSOR_FULL_WIDTH;
			   *pFeatureReturnPara16=T4K35_IMAGE_SENSOR_FULL_HEIGHT;
			   *pFeatureParaLen=4;
			   break;
		   case SENSOR_FEATURE_GET_PERIOD:
			   *pFeatureReturnPara16++= T4K35_FeatureControl_PERIOD_PixelNum;
			   *pFeatureReturnPara16= T4K35_FeatureControl_PERIOD_LineNum;
			   *pFeatureParaLen=4;
			   break;
		   case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			   switch(T4K35CurrentScenarioId)
			   {   
			       case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					   *pFeatureReturnPara32 = t4k35.videoPclk * 100000;
					      *pFeatureParaLen=4;
					   break;
				   case MSDK_SCENARIO_ID_CAMERA_ZSD:
				   case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					   *pFeatureReturnPara32 = t4k35.capPclk * 100000; 
						  *pFeatureParaLen=4;
					   break;
				   default:
					   *pFeatureReturnPara32 = t4k35.pvPclk * 100000;
						  *pFeatureParaLen=4;
					   break;
			   }
			   break;
		   case SENSOR_FEATURE_SET_ESHUTTER:
			   T4K35_SetShutter(*pFeatureData16);
			   break;
		   case SENSOR_FEATURE_SET_NIGHTMODE:
			   T4K35_NightMode((BOOL) *pFeatureData16);
			   break;
		   case SENSOR_FEATURE_SET_GAIN:
			   T4K35MIPI_SetGain((UINT16) *pFeatureData16);
			   break;
		   case SENSOR_FEATURE_SET_FLASHLIGHT:
			   break;
		   case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			  // T4K35_isp_master_clock=*pFeatureData32;
			   break;
		   case SENSOR_FEATURE_SET_REGISTER:
			   T4K35_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
			   break;
		   case SENSOR_FEATURE_GET_REGISTER:
			   pSensorRegData->RegData = T4K35_read_cmos_sensor(pSensorRegData->RegAddr);
			   break;
		   case SENSOR_FEATURE_SET_CCT_REGISTER:
			   SensorRegNumber=FACTORY_END_ADDR;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   T4K35SensorCCT[i].Addr=*pFeatureData32++;
				   T4K35SensorCCT[i].Para=*pFeatureData32++;
			   }
			   break;
		   case SENSOR_FEATURE_GET_CCT_REGISTER:
			   SensorRegNumber=FACTORY_END_ADDR;
			   if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
				   return FALSE;
			   *pFeatureData32++=SensorRegNumber;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   *pFeatureData32++=T4K35SensorCCT[i].Addr;
				   *pFeatureData32++=T4K35SensorCCT[i].Para;
			   }
			   break;
		   case SENSOR_FEATURE_SET_ENG_REGISTER:
			   SensorRegNumber=ENGINEER_END;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   T4K35SensorReg[i].Addr=*pFeatureData32++;
				   T4K35SensorReg[i].Para=*pFeatureData32++;
			   }
			   break;
		   case SENSOR_FEATURE_GET_ENG_REGISTER:
			   SensorRegNumber=ENGINEER_END;
			   if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
				   return FALSE;
			   *pFeatureData32++=SensorRegNumber;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   *pFeatureData32++=T4K35SensorReg[i].Addr;
				   *pFeatureData32++=T4K35SensorReg[i].Para;
			   }
			   break;
		   case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
			   if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
			   {
				   pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
				   pSensorDefaultData->SensorId=T4K35_SENSOR_ID;
				   memcpy(pSensorDefaultData->SensorEngReg, T4K35SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
				   memcpy(pSensorDefaultData->SensorCCTReg, T4K35SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
			   }
			   else
				   return FALSE;
			   *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
			   break;
		   case SENSOR_FEATURE_GET_CONFIG_PARA:
			   break;
		   case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
			   //T4K35_camera_para_to_sensor();
			   break;
	
		   case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
			   //T4K35_sensor_to_camera_para();
			   break;
		   case SENSOR_FEATURE_GET_GROUP_COUNT:

			   break;
		   case SENSOR_FEATURE_GET_GROUP_INFO:

			   break;
		   case SENSOR_FEATURE_GET_ITEM_INFO:

			   break;
	
		   case SENSOR_FEATURE_SET_ITEM_INFO:

			   break;
	
		   case SENSOR_FEATURE_GET_ENG_INFO:
			   pSensorEngInfo->SensorId = 221;
			   pSensorEngInfo->SensorType = CMOS_SENSOR;
	
			   pSensorEngInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_Gr;
	
			   *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
			   break;
		   case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			   // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			   // if EEPROM does not exist in camera module.
			   *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			   *pFeatureParaLen=4;
			   break;
	
		   case SENSOR_FEATURE_SET_VIDEO_MODE:
			   T4K35SetVideoMode(*pFeatureData16);
			   break;
		   case SENSOR_FEATURE_CHECK_SENSOR_ID:
			   T4K35GetSensorID(pFeatureReturnPara32);
			   break;
	       case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
               T4K35SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	           break;
           case SENSOR_FEATURE_SET_TEST_PATTERN:
               T4K35SetTestPatternMode((BOOL)*pFeatureData16);
               break;
		   case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing 			
               *pFeatureReturnPara32= T4K35_TEST_PATTERN_CHECKSUM;
			   *pFeatureParaLen=4; 							
			break;	
		   case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			   T4K35MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			   break;
		   case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			   T4K35MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			  break;

		   default:
			   break;

    }
    return ERROR_NONE;
}	/* T4K35FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncT4K35=
{
    T4K35Open,
    T4K35GetInfo,
    T4K35GetResolution,
    T4K35FeatureControl,
    T4K35Control,
    T4K35Close
};

UINT32 T4K35_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncT4K35;

    return ERROR_NONE;
}   /* SensorInit() */

