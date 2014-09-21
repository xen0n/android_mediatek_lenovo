/*******************************************************************************************/
  

/*******************************************************************************************/

/* SENSOR FULL SIZE */
#ifndef __SENSOR_H_T4K37
#define __SENSOR_H_T4K37
  
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
} T4K37_SENSOR_MODE;


typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	
	kal_uint32 pvShutter;
	kal_uint32 pvGain;
	
	kal_uint32 pvPclk;  // x10 480 for 48MHZ
	kal_uint32 capPclk; // x10
	kal_uint32 videoPclk;
	
	kal_uint32 shutter;
	kal_uint32 maxExposureLines;

	kal_uint16 sensorGlobalGain;//sensor gain read from 0x350a 0x350b;
	kal_uint16 ispBaseGain;//64
	kal_uint16 realGain;//ispBaseGain as 1x

	kal_int16 imgMirror;

	T4K37_SENSOR_MODE sensorMode;

	kal_bool T4K37AutoFlickerMode;
	kal_bool T4K37VideoMode;
	
}T4K37_PARA_STRUCT,*PT4K37_PARA_STRUCT;

#define SENSOR_OUTPUT_FORMAT_FIRST_PIXEL SENSOR_OUTPUT_FORMAT_RAW_Gb

#if 0
    //grab windows
	#define T4K37_IMAGE_SENSOR_PV_WIDTH					(2104-8)//1632   
	#define T4K37_IMAGE_SENSOR_PV_HEIGHT				(1560-4)//1224  
	
	//#define T4K37_IMAGE_SENSOR_VDO_WIDTH 				(1920-8)//3264  
	//#define T4K37_IMAGE_SENSOR_VDO_HEIGHT				(1080-4)//1836
	#define T4K37_IMAGE_SENSOR_VDO_WIDTH					(2104-8)//1632   
	#define T4K37_IMAGE_SENSOR_VDO_HEIGHT				(1560-4)//1224  
	
	#define T4K37_IMAGE_SENSOR_FULL_WIDTH				(4208-8)//3264
	#define T4K37_IMAGE_SENSOR_FULL_HEIGHT				(3120-4)//2448

    //total nums
	#define T4K37_PV_PERIOD_PIXEL_NUMS					4480 //0x0D54	//3412
	#define T4K37_PV_PERIOD_LINE_NUMS					1580//0x0932	//2354	
	
	#define T4K37_VIDEO_PERIOD_PIXEL_NUMS				4480//0x0D98	//3480
	#define T4K37_VIDEO_PERIOD_LINE_NUMS				1580//0x09C0	//2496
	//#define T4K37_VIDEO_PERIOD_LINE_NUMS				0x0973	//

	
	
	#define T4K37_FULL_PERIOD_PIXEL_NUMS				(5420)//(4480)//0x0D98	//3480
	#define T4K37_FULL_PERIOD_LINE_NUMS 				(3144)//0x09C0	//2496
	#endif
	
    //grab windows
	//#define T4K37_IMAGE_SENSOR_PV_WIDTH					(2104)//1632 
	#define T4K37_IMAGE_SENSOR_PV_WIDTH					(2080)
	#define T4K37_IMAGE_SENSOR_PV_HEIGHT				(1560)//1224  
	
	//#define T4K37_IMAGE_SENSOR_VDO_WIDTH 				(1920-8)//3264  
	//#define T4K37_IMAGE_SENSOR_VDO_HEIGHT				(1080-4)//1836
	//#define T4K37_IMAGE_SENSOR_VDO_WIDTH				(2104)//1632
	#define T4K37_IMAGE_SENSOR_VDO_WIDTH				(2080)	
	#define T4K37_IMAGE_SENSOR_VDO_HEIGHT				(1560)//1224  
	
	//#define T4K37_IMAGE_SENSOR_FULL_WIDTH				(4208)//3264
	#define T4K37_IMAGE_SENSOR_FULL_WIDTH				(4160)
	#define T4K37_IMAGE_SENSOR_FULL_HEIGHT				(3120)//2448


#define T4K37_PCLK_259200000	
#ifdef T4K37_PCLK_259200000

    //total nums
	//#define T4K37_PV_PERIOD_PIXEL_NUMS					(5420)//(5248)//4352//4480 //0x0D54	//3412
	#define T4K37_PV_PERIOD_PIXEL_NUMS					(5408)//(5248)//4352//4480 //0x0D54	//3412
	#define T4K37_PV_PERIOD_LINE_NUMS					(1792)// 1580 -> 3144	
	
	#define T4K37_VIDEO_PERIOD_PIXEL_NUMS				(5408)//(5248)//4352//4480//0x0D98	//3480
	#define T4K37_VIDEO_PERIOD_LINE_NUMS				(1792)//)(1580)//0x09C0	//2496
	//#define T4K37_VIDEO_PERIOD_LINE_NUMS				0x0973	//

	
	
	//#define T4K37_FULL_PERIOD_PIXEL_NUMS				(5420)//(5248)//(4480)//0x0D98	//3480
	#define T4K37_FULL_PERIOD_PIXEL_NUMS				(5168)//  5420 -> 5168
	#define T4K37_FULL_PERIOD_LINE_NUMS 				(3144)//  3160 -> 3144
	
	#define T4K37_PREVIEW_PCLK							(292000000)  // 259200000 -> 390000000
	#define T4K37_VIDEO_PCLK							(292000000)
	#define T4K37_CAPTURE_PCLK							(389333333)  //
#endif	   
                               	
	/* SENSOR START/EDE POSITION */         	
	#define T4K37_PV_X_START						    (0)//(2)
	#define T4K37_PV_Y_START						    (0)//(2)
	#define T4K37_VIDEO_X_START                         (0)//(2)
	#define T4K37_VIDEO_Y_START 						(0)//(2)
	#define T4K37_FULL_X_START							(0)//(2)
	#define T4K37_FULL_Y_START							(0)//(2)

	#define T4K37_MAX_ANALOG_GAIN						(10)
	#define T4K37_MIN_ANALOG_GAIN						(1)
	#define T4K37_ANALOG_GAIN_1X						(0x3d)//(0x20)//(0x3d)

//	#define T4K37_PREVIEW_PCLK 							(211200000)
//	#define T4K37_VIDEO_PCLK							(211200000)
//	#define T4K37_CAPTURE_PCLK							(211200000)
	

	#define T4K37MIPI_WRITE_ID 	(0x6C)//0x6e
	#define T4K37MIPI_READ_ID	(0x6D)//0x6f


	#define T4K37MIPI_SENSOR_ID            T4K37_SENSOR_ID

	#define T4K37MIPI_PAGE_SETTING_REG    (0xFF)


//export functions
UINT32 T4K37MIPIOpen(void);
UINT32 T4K37MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 T4K37MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 T4K37MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 T4K37MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 T4K37MIPIClose(void);


#endif /* __SENSOR_H_T4K37 */

