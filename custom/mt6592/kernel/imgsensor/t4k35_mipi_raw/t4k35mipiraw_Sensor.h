/*******************************************************************************************/


/******************************************************************************************/

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
    SENSOR_MODE_CAPTURE,
} T4K35_SENSOR_MODE;


typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	
	kal_uint32 pvShutter;
	kal_uint32 pvGain;
	
	kal_uint32 pvPclk;  // x10 480 for 48MHZ
	kal_uint32 videoPclk;
	kal_uint32 capPclk; // x10
	
	kal_uint32 shutter;
	kal_uint32 maxExposureLines;

	kal_uint16 sensorGlobalGain;//sensor gain read from 0x350a 0x350b;
	kal_uint16 ispBaseGain;//64
	kal_uint16 realGain;//ispBaseGain as 1x

	kal_int16 imgMirror;

	T4K35_SENSOR_MODE sensorMode;

	kal_bool T4K35AutoFlickerMode;
	kal_bool T4K35VideoMode;
	
}T4K35_PARA_STRUCT,*PT4K35_PARA_STRUCT;


    #define CURRENT_MAIN_SENSOR                T4K35_MICRON
   //if define RAW10, MIPI_INTERFACE must be defined
   //if MIPI_INTERFACE is marked, RAW10 must be marked too
    #define MIPI_INTERFACE

   #define T4K35_IMAGE_SENSOR_FULL_HACTIVE    3264
   #define T4K35_IMAGE_SENSOR_FULL_VACTIVE    2448
   #define T4K35_IMAGE_SENSOR_PV_HACTIVE      1632
   #define T4K35_IMAGE_SENSOR_PV_VACTIVE      1224
   #define T4K35_IMAGE_SENSOR_VIDEO_HACTIVE   3264
   #define T4K35_IMAGE_SENSOR_VIDEO_VACTIVE   1836


   #define T4K35_IMAGE_SENSOR_FULL_WIDTH				(T4K35_IMAGE_SENSOR_FULL_HACTIVE)	
   #define T4K35_IMAGE_SENSOR_FULL_HEIGHT 				(T4K35_IMAGE_SENSOR_FULL_VACTIVE)

	/* SENSOR PV SIZE */

   #define T4K35_IMAGE_SENSOR_PV_WIDTH        (T4K35_IMAGE_SENSOR_PV_HACTIVE)
   #define T4K35_IMAGE_SENSOR_PV_HEIGHT       (T4K35_IMAGE_SENSOR_PV_VACTIVE)

   #define T4K35_IMAGE_SENSOR_VIDEO_WIDTH     (T4K35_IMAGE_SENSOR_VIDEO_HACTIVE)
   #define T4K35_IMAGE_SENSOR_VIDEO_HEIGHT    (T4K35_IMAGE_SENSOR_VIDEO_VACTIVE)


	/* SENSOR SCALER FACTOR */
	#define T4K35_PV_SCALER_FACTOR					    	3
	#define T4K35_FULL_SCALER_FACTOR					    1
	                                        	
	/* SENSOR START/EDE POSITION */         	
	#define T4K35_FULL_X_START						    	(0)
	#define T4K35_FULL_Y_START						    	(0)
	#define T4K35_PV_X_START						    	(0)
	#define T4K35_PV_Y_START						    	(0)	
	#define T4K35_VIDEO_X_START								(0)
	#define T4K35_VIDEO_Y_START								(0)

	#define T4K35_MAX_ANALOG_GAIN					(16)
	#define T4K35_MIN_ANALOG_GAIN					(1)
	#define T4K35_ANALOG_GAIN_1X						(0x0020)


	/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
    #define	T4K35_IMAGE_SENSOR_FULL_HBLANKING  244
    #define T4K35_IMAGE_SENSOR_FULL_VBLANKING  40


    #define	T4K35_IMAGE_SENSOR_PV_HBLANKING    1876
    #define T4K35_IMAGE_SENSOR_PV_VBLANKING    1264
	
    #define	T4K35_IMAGE_SENSOR_VIDEO_HBLANKING    244
    #define T4K35_IMAGE_SENSOR_VIDEO_VBLANKING    652


	#define T4K35_FULL_PERIOD_PIXEL_NUMS	    (T4K35_IMAGE_SENSOR_FULL_HACTIVE + T4K35_IMAGE_SENSOR_FULL_HBLANKING)  //2592+1200= 3792
    #define T4K35_FULL_PERIOD_LINE_NUMS	    (T4K35_IMAGE_SENSOR_FULL_VACTIVE + T4K35_IMAGE_SENSOR_FULL_VBLANKING)  //1944+150 = 2094
    #define T4K35_PV_PERIOD_PIXEL_NUMS	        (T4K35_IMAGE_SENSOR_PV_HACTIVE + T4K35_IMAGE_SENSOR_PV_HBLANKING)     //1296 +1855 =3151
    #define T4K35_PV_PERIOD_LINE_NUMS	        (T4K35_IMAGE_SENSOR_PV_VACTIVE + T4K35_IMAGE_SENSOR_PV_VBLANKING)    //972 + 128 =1100
    #define T4K35_VIDEO_PERIOD_PIXEL_NUMS 		(T4K35_IMAGE_SENSOR_VIDEO_HACTIVE + T4K35_IMAGE_SENSOR_VIDEO_HBLANKING) 	//1296 +1855 =3151
    #define T4K35_VIDEO_PERIOD_LINE_NUMS		(T4K35_IMAGE_SENSOR_VIDEO_VACTIVE + T4K35_IMAGE_SENSOR_VIDEO_VBLANKING)    //972 + 128 =1100 


	/* DUMMY NEEDS TO BE INSERTED */
	/* SETUP TIME NEED TO BE INSERTED */
	#define T4K35_IMAGE_SENSOR_PV_INSERTED_PIXELS			2
	#define T4K35_IMAGE_SENSOR_PV_INSERTED_LINES			2

	#define T4K35_IMAGE_SENSOR_FULL_INSERTED_PIXELS		    4
	#define T4K35_IMAGE_SENSOR_FULL_INSERTED_LINES		    4

#define T4K35MIPI_WRITE_ID 	(0x6E)
#define T4K35MIPI_READ_ID	(0x6F)

// SENSOR CHIP VERSION

#define T4K35MIPI_SENSOR_ID            T4K35_SENSOR_ID

#define T4K35MIPI_PAGE_SETTING_REG    (0xFF)

struct T4K35_SENSOR_STRUCT
{
    kal_uint8 i2c_write_id;
    kal_uint8 i2c_read_id;
    kal_uint16 pvPclk;
    kal_uint16 capPclk;
	kal_uint16 videoPclk;
};


//s_add for porting
//s_add for porting
//s_add for porting

//export functions
UINT32 T4K35MIPIOpen(void);
UINT32 T4K35MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 T4K35MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 T4K35MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 T4K35MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 T4K35MIPIClose(void);

//#define Sleep(ms) mdelay(ms)
//#define RETAILMSG(x,...)
//#define TEXT

//e_add for porting
//e_add for porting
//e_add for porting

#endif /* __SENSOR_H */

