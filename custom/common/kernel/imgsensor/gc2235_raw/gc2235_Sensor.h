/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 sensor.c
 *
 * Project:
 * --------
 *	 RAW
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   Leo Lee
 *
 *============================================================================
 *             HISTORY
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 03 01 2013
 * First release GC2235 driver Version 1.0
 *
 *------------------------------------------------------------------------------
 *============================================================================
 ****************************************************************************/

#ifndef _GC2235_SENSOR_H
#define _GC2235_SENSOR_H

#define GC2235_DEBUG
#define GC2235_DRIVER_TRACE
//#define GC2235_TEST_PATTEM

#define GC2235_FACTORY_START_ADDR 0
#define GC2235_ENGINEER_START_ADDR 10
 
typedef enum GC2235_group_enum
{
  GC2235_PRE_GAIN = 0,
  GC2235_CMMCLK_CURRENT,
  GC2235_FRAME_RATE_LIMITATION,
  GC2235_REGISTER_EDITOR,
  GC2235_GROUP_TOTAL_NUMS
} GC2235_FACTORY_GROUP_ENUM;

typedef enum GC2235_register_index
{
  GC2235_SENSOR_BASEGAIN = GC2235_FACTORY_START_ADDR,
  GC2235_PRE_GAIN_R_INDEX,
  GC2235_PRE_GAIN_Gr_INDEX,
  GC2235_PRE_GAIN_Gb_INDEX,
  GC2235_PRE_GAIN_B_INDEX,
  GC2235_FACTORY_END_ADDR
} GC2235_FACTORY_REGISTER_INDEX;

typedef enum GC2235_engineer_index
{
  GC2235_CMMCLK_CURRENT_INDEX = GC2235_ENGINEER_START_ADDR,
  GC2235_ENGINEER_END
} GC2235_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[GC2235_ENGINEER_END];
  SENSOR_REG_STRUCT cct[GC2235_FACTORY_END_ADDR];
} sensor_data_struct;

/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define GC2235_PREVIEW_CLK                   24000000//48000000
#define GC2235_CAPTURE_CLK                    24000000//48000000

#define GC2235_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_B //SENSOR_OUTPUT_FORMAT_RAW_Gb //SENSOR_OUTPUT_FORMAT_RAW_R

#define GC2235_MIN_ANALOG_GAIN				1	/* 1x */
#define GC2235_MAX_ANALOG_GAIN				6	/* 6x */


/* FRAME RATE UNIT */
#define GC2235_FPS(x)                          (10 * (x))

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
//#define GC2235_FULL_PERIOD_PIXEL_NUMS          2700 /* 9 fps */
#define GC2235_FULL_PERIOD_PIXEL_NUMS          1030//1974 /* 8 fps */
#define GC2235_FULL_PERIOD_LINE_NUMS           1258

#define GC2235_VIDEO_PERIOD_PIXEL_NUMS          1030//1974 /* 8 fps */
#define GC2235_VIDEO_PERIOD_LINE_NUMS           1258

#define GC2235_PV_PERIOD_PIXEL_NUMS            1030//1974 /* 30 fps */
#define GC2235_PV_PERIOD_LINE_NUMS             1258

/* SENSOR START/END POSITION */
#define GC2235_FULL_X_START                    8   //(1+16+6)
#define GC2235_FULL_Y_START                    6  //(1+12+4)
#define GC2235_IMAGE_SENSOR_FULL_WIDTH         (1600 - 16) //(2592 - 16) /* 2560 */
#define GC2235_IMAGE_SENSOR_FULL_HEIGHT        (1200 - 12) //(1944 - 12) /* 1920 */

#define GC2235_VIDEO_X_START                      8
#define GC2235_VIDEO_Y_START                      6
#define GC2235_IMAGE_SENSOR_VIDEO_WIDTH           (1600 - 16) /* 1264 */
#define GC2235_IMAGE_SENSOR_VIDEO_HEIGHT          (1200  - 12) /* 948 */

#define GC2235_PV_X_START                      16
#define GC2235_PV_Y_START                      12
#define GC2235_IMAGE_SENSOR_PV_WIDTH           (1600 - 32) /* 1264 */
#define GC2235_IMAGE_SENSOR_PV_HEIGHT          (1200  - 24) /* 948 */

/* SENSOR READ/WRITE ID */
#define GC2235_WRITE_ID (0x78)
#define GC2235_READ_ID  (0x79)

/* SENSOR ID */
//#define GC2235_SENSOR_ID						(0x2235)

// test pattern's checksum
// it can be got by the log by factory mode's camera test
// keywords: crc
#define GC2235_TEST_PATTERN_CHECKSUM                    (0xca4003aa)


/* SENSOR PRIVATE STRUCT */
typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE
} GC2235_SENSOR_MODE;

typedef enum{
	GC2235_IMAGE_NORMAL = 0,
	GC2235_IMAGE_H_MIRROR,
	GC2235_IMAGE_V_MIRROR,
	GC2235_IMAGE_HV_MIRROR
}GC2235_IMAGE_MIRROR;

typedef struct GC2235_sensor_STRUCT
{
	MSDK_SENSOR_CONFIG_STRUCT cfg_data;
	sensor_data_struct eng; /* engineer mode */
	MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
	GC2235_SENSOR_MODE sensorMode;
	GC2235_IMAGE_MIRROR Mirror;
	kal_bool pv_mode;
	kal_bool video_mode;
	kal_bool NightMode;
	kal_uint16 normal_fps; /* video normal mode max fps */
	kal_uint16 night_fps; /* video night mode max fps */
	kal_uint16 FixedFps;
	kal_uint16 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint16 frame_height;
	kal_uint16 frame_height_BackUp;
	kal_uint16 line_length;  
	kal_uint16 Prv_line_length;
} GC2235_sensor_struct;

typedef enum GC2235_GainMode_Index
{
	GC2235_Analogic_Gain = 0,
	GC2235_Digital_Gain
}GC2235_GainMode_Index;
//export functions
UINT32 GC2235Open(void);
UINT32 GC2235Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 GC2235FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 GC2235GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 GC2235GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 GC2235Close(void);

#define Sleep(ms) mdelay(ms)

#endif 
