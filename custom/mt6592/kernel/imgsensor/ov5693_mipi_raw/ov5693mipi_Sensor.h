/*****************************************************************************
 *
 * Filename:
 * ---------
 *   OV5693mipi_Sensor.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _OV5693MIPI_SENSOR_H
#define _OV5693MIPI_SENSOR_H

#define OV5693MIPI_FACTORY_START_ADDR 0
#define OV5693MIPI_ENGINEER_START_ADDR 10
 
typedef enum OV5693MIPI_group_enum
{
  OV5693MIPI_PRE_GAIN = 0,
  OV5693MIPI_CMMCLK_CURRENT,
  OV5693MIPI_FRAME_RATE_LIMITATION,
  OV5693MIPI_REGISTER_EDITOR,
  OV5693MIPI_GROUP_TOTAL_NUMS
} OV5693MIPI_FACTORY_GROUP_ENUM;

typedef enum OV5693MIPI_register_index
{
  OV5693MIPI_SENSOR_BASEGAIN = OV5693MIPI_FACTORY_START_ADDR,
  OV5693MIPI_PRE_GAIN_R_INDEX,
  OV5693MIPI_PRE_GAIN_Gr_INDEX,
  OV5693MIPI_PRE_GAIN_Gb_INDEX,
  OV5693MIPI_PRE_GAIN_B_INDEX,
  OV5693MIPI_FACTORY_END_ADDR
} OV5693MIPI_FACTORY_REGISTER_INDEX;

typedef enum OV5693MIPI_engineer_index
{
  OV5693MIPI_CMMCLK_CURRENT_INDEX = OV5693MIPI_ENGINEER_START_ADDR,
  OV5693MIPI_ENGINEER_END
} OV5693MIPI_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[OV5693MIPI_ENGINEER_END];
  SENSOR_REG_STRUCT cct[OV5693MIPI_FACTORY_END_ADDR];
} sensor_data_struct;


/* Video default full size@30fps
     If video use 720P@30fps or 1080P@30fps, Please open define VIDEO_720P or VIDEO_1080P
*/
//#define VIDEO_720P  
//#define VIDEO_1080P

/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define OV5693MIPI_PREVIEW_CLK                      160000000
#define OV5693MIPI_CAPTURE_CLK                      160000000
#define OV5693MIPI_VIDEO_CLK                        160000000


/* Data Format */
#define OV5693MIPI_COLOR_FORMAT                     SENSOR_OUTPUT_FORMAT_RAW_B


#define OV5693MIPI_MIN_ANALOG_GAIN                  1   /* 1x */
#define OV5693MIPI_MAX_ANALOG_GAIN                  32  /* 32x */

#define OV5693MIPI_FULL_START_X                    (2)
#define OV5693MIPI_FULL_START_Y                    (2)
#define OV5693MIPI_IMAGE_SENSOR_FULL_WIDTH         (2560)
#define OV5693MIPI_IMAGE_SENSOR_FULL_HEIGHT        (1920)

#define OV5693MIPI_PV_START_X                      (2)
#define OV5693MIPI_PV_START_Y                      (2)
#define OV5693MIPI_IMAGE_SENSOR_PV_WIDTH           (1280)
#define OV5693MIPI_IMAGE_SENSOR_PV_HEIGHT          (960)


#ifdef VIDEO_720P
    #define OV5693MIPI_VIDEO_START_X                   (2)
    #define OV5693MIPI_VIDEO_START_Y                   (2)
    #define OV5693MIPI_IMAGE_SENSOR_VIDEO_WIDTH        (1280 - 8)
    #define OV5693MIPI_IMAGE_SENSOR_VIDEO_HEIGHT       (720 - 6)
#elif defined VIDEO_1080P
    #define OV5693MIPI_VIDEO_START_X                   (2)
    #define OV5693MIPI_VIDEO_START_Y                   (2)
    #define OV5693MIPI_IMAGE_SENSOR_VIDEO_WIDTH        (1920 - 8)
    #define OV5693MIPI_IMAGE_SENSOR_VIDEO_HEIGHT       (1080 - 6)
#else
    #define OV5693MIPI_VIDEO_START_X                   (2)
    #define OV5693MIPI_VIDEO_START_Y                   (2)
    #define OV5693MIPI_IMAGE_SENSOR_VIDEO_WIDTH        (2560)
    #define OV5693MIPI_IMAGE_SENSOR_VIDEO_HEIGHT       (1920)
#endif


/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
#define OV5693MIPI_FULL_PERIOD_PIXEL_NUMS          (2688) /* 30 fps */
#define OV5693MIPI_FULL_PERIOD_LINE_NUMS           (1984)

#define OV5693MIPI_PV_PERIOD_PIXEL_NUMS            (2688) /* 30 fps */
#define OV5693MIPI_PV_PERIOD_LINE_NUMS             (1984)

#define OV5693MIPI_VIDEO_PERIOD_PIXEL_NUMS         (2688) /* 30 fps */
#define OV5693MIPI_VIDEO_PERIOD_LINE_NUMS          (1984)

/* SENSOR READ/WRITE ID */
#define OV5693MIPI_WRITE_ID_1 (0x6c)
#define OV5693MIPI_READ_ID_1  (0x6d)
#define OV5693MIPI_WRITE_ID_2 (0x21)
#define OV5693MIPI_READ_ID_2  (0x20)

/* FRAME RATE UNIT */
#define OV5693MIPI_FPS(x)                          (10 * (x))


/* EXPORT FUNCTIONS */
UINT32 OV5693MIPIOpen(void);
UINT32 OV5693MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV5693MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 OV5693MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV5693MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 OV5693MIPIClose(void);

#define Sleep(ms) mdelay(ms)

#endif 
