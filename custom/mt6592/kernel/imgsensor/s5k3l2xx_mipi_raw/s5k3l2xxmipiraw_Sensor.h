/*******************************************************************************************/
  
      
/*******************************************************************************************/

/* SENSOR FULL SIZE */
#ifndef __SENSOR_H
#define __SENSOR_H

typedef enum group_enum {
    PRE_GAIN=0,
    CMMCLK_CURRENT,
    FRAME_RATE_LIMITATION,
    REGISTER_EDITOR,
    GROUP_TOTAL_NUMS
} FACTORY_GROUP_ENUM;
 

#define ENGINEER_START_ADDR 10
#define FACTORY_START_ADDR 0

typedef enum engineer_index
{
    CMMCLK_CURRENT_INDEX=ENGINEER_START_ADDR,
    ENGINEER_END
} FACTORY_ENGINEER_INDEX;

typedef enum register_index
{
	SENSOR_BASEGAIN=FACTORY_START_ADDR,
	PRE_GAIN_R_INDEX,
	PRE_GAIN_Gr_INDEX,
	PRE_GAIN_Gb_INDEX,
	PRE_GAIN_B_INDEX,
	FACTORY_END_ADDR
} FACTORY_REGISTER_INDEX;

typedef struct
{
    SENSOR_REG_STRUCT	Reg[ENGINEER_END];
    SENSOR_REG_STRUCT	CCT[FACTORY_END_ADDR];
} SENSOR_DATA_STRUCT, *PSENSOR_DATA_STRUCT;

typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE
} S5K3L2XX_SENSOR_MODE;


typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	
	kal_uint32 pvShutter;
	kal_uint32 pvGain;
	
	kal_uint32 shutter;
	kal_uint32 maxExposureLines;

	kal_uint16 sensorGlobalGain;//sensor gain read from 0x350a 0x350b;
	kal_uint16 ispBaseGain;//64
	kal_uint16 realGain;//ispBaseGain as 1x

	kal_int16 imgMirror;

	S5K3L2XX_SENSOR_MODE sensorMode;

	kal_bool S5K3L2XXAutoFlickerMode;
	kal_bool S5K3L2XXVideoMode;
	
	
}S5K3L2XX_PARA_STRUCT,*PS5K3L2XX_PARA_STRUCT;

#define s5k3l2xx_master_clock								24   //mhz

#define s5k3l2xx_max_analog_gain							16
#define s5k3l2xx_min_analog_gain							1

#define s5k3l2xx_preview_frame_length   					1844 //1872
#define s5k3l2xx_preview_line_length  						7808
#define s5k3l2xx_preview_pixelclock 					 	432000000

#define s5k3l2xx_capture_frame_length 						3254
#define s5k3l2xx_capture_line_length 						8850 //7808 //9112
#define s5k3l2xx_capture_pixelclock 					 	432000000


#define s5k3l2xx_video_frame_length  						1844 //1872
#define s5k3l2xx_video_line_length   						7808
#define s5k3l2xx_video_pixelclock 					 		432000000

#define s5k3l2xx_sensor_output_preview_width				2104
#define s5k3l2xx_sensor_output_preview_height				1560
#define s5k3l2xx_preview_width								s5k3l2xx_sensor_output_preview_width
#define s5k3l2xx_preview_height								s5k3l2xx_sensor_output_preview_height
#define s5k3l2xx_preview_startx								0
#define s5k3l2xx_preview_starty								0
#define s5k3l2xx_preview_max_framerate						300								

#define s5k3l2xx_sensor_output_capture_width				4208
#define s5k3l2xx_sensor_output_capture_height				3120
#define s5k3l2xx_capture_width								s5k3l2xx_sensor_output_capture_width 
#define s5k3l2xx_capture_height								s5k3l2xx_sensor_output_capture_height 
#define s5k3l2xx_capture_startx								0
#define s5k3l2xx_capture_starty								0
#define s5k3l2xx_capture_max_framerate						150

#define s5k3l2xx_sensor_output_video_width					2104
#define s5k3l2xx_sensor_output_video_height					1560
#define s5k3l2xx_video_width								s5k3l2xx_sensor_output_video_width
#define s5k3l2xx_video_height								s5k3l2xx_sensor_output_video_height
#define s5k3l2xx_video_startx								0
#define s5k3l2xx_video_starty								0
#define s5k3l2xx_video_max_framerate						300

#define S5K3L2XXMIPI_WRITE_ID 	(0x20)
#define S5K3L2XXMIPI_READ_ID	(0x21)

// SENSOR CHIP VERSION

#define S5K3L2XXMIPI_SENSOR_ID            S5K3L2XX_SENSOR_ID

#define S5K3L2XXMIPI_PAGE_SETTING_REG    (0xFF)

struct S5K3L2XX_MIPI_otp_struct
{
    kal_uint16 customer_id;
	kal_uint16 module_integrator_id;
	kal_uint16 lens_id;
	kal_uint16 r_ratio;
	kal_uint16 b_ratio;
	kal_uint16 user_data[5];
	kal_uint16 R_to_G;
	kal_uint16 B_to_G;
	kal_uint16 G_to_G;
	kal_uint16 R_Gain;
	kal_uint16 G_Gain;
	kal_uint16 B_Gain;
};

//export functions
UINT32 S5K3L2XXMIPIOpen(void);
UINT32 S5K3L2XXMIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 S5K3L2XXMIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K3L2XXMIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K3L2XXMIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 S5K3L2XXMIPIClose(void);

#endif /* __SENSOR_H */

