#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_s5k3h7ymipiraw.h"
#include "camera_info_s5k3h7ymipiraw.h"
#include "camera_custom_AEPlinetable.h"
#include "camera_custom_tsf_tbl.h"


const NVRAM_CAMERA_ISP_PARAM_STRUCT CAMERA_ISP_DEFAULT_VALUE =
{{
    //Version
    Version: NVRAM_CAMERA_PARA_FILE_VERSION,

    //SensorId
    SensorId: SENSOR_ID,
    ISPComm:{
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    	}
    },
    ISPPca: {
        #include INCLUDE_FILENAME_ISP_PCA_PARAM
    },
    ISPRegs:{
        #include INCLUDE_FILENAME_ISP_REGS_PARAM
    },
    ISPMfbMixer:{{
        {//00: MFB mixer for ISO 100
            0x00000000, 0x00000000
        },
        {//01: MFB mixer for ISO 200
            0x00000000, 0x00000000
        },
        {//02: MFB mixer for ISO 400
            0x00000000, 0x00000000
        },
        {//03: MFB mixer for ISO 800
            0x00000000, 0x00000000
        },
        {//04: MFB mixer for ISO 1600
            0x00000000, 0x00000000
        },
        {//05: MFB mixer for ISO 2400
            0x00000000, 0x00000000
        },
        {//06: MFB mixer for ISO 3200
            0x00000000, 0x00000000
        }
    }},
    ISPCcmPoly22:{
        75170, // i4R_AVG
        13190, // i4R_STD
        79140, // i4B_AVG
        26270, // i4B_STD
        { // i4P00[9]
            4448648, -1494813, -393843, -604477, 3414513, -250036,  85095, -1385454, 3860283
        },
        { // i4P10[9]
            933698,  -628943, -304758, -247520,  -22220,  269740, -73861,   196166, -122555
        },
        { // i4P01[9]
            814367,  -494023, -320352, -358410, -180556,  538966, -57406,  -190454,  247689
        },
        { // i4P20[9]
            394007,  -491950,   98031,  -21525,   59812,  -38287, 140879,  -521951,  381045
        },
        { // i4P11[9]
            -35750,  -344806,  380738,  121574,   59500, -181074, 143388,  -309535,  166309
        },
        { // i4P02[9]
            -315751,    65233,  250618,  151463,   34149, -185612,  21808,    -8637,  -12997
        }
    }
}};

const NVRAM_CAMERA_3A_STRUCT CAMERA_3A_NVRAM_DEFAULT_VALUE =
{
    NVRAM_CAMERA_3A_FILE_VERSION, // u4Version
    SENSOR_ID, // SensorId

    // AE NVRAM
    {
        // rDevicesInfo
        {
            1136,   // u4MinGain, 1024 base =  1x
            10240,  // u4MaxGain, 16x
            96,     // u4MiniISOGain, ISOxx
            128,    // u4GainStepUnit, 1x/8
            13172,     // u4PreExpUnit
            30,     // u4PreMaxFrameRate
            13172,     // u4VideoExpUnit
            30,     // u4VideoMaxFrameRate
            1024,   // u4Video2PreRatio, 1024 base = 1x
            13172,     // u4CapExpUnit
            30,     // u4CapMaxFrameRate
            1024,   // u4Cap2PreRatio, 1024 base = 1x
            24,      // u4LensFno, Fno = 2.8
            350     // u4FocusLength_100x
         },
         // rHistConfig
        {
            4, // 2,   // u4HistHighThres
            40,  // u4HistLowThres
            2,   // u4MostBrightRatio
            1,   // u4MostDarkRatio
            160, // u4CentralHighBound
            20,  // u4CentralLowBound
            {240, 230, 220, 210, 200}, // u4OverExpThres[AE_CCT_STRENGTH_NUM]
            {62, 70, 82, 108, 141},  // u4HistStretchThres[AE_CCT_STRENGTH_NUM]
            {18, 22, 26, 30, 34}       // u4BlackLightThres[AE_CCT_STRENGTH_NUM]
        },
        // rCCTConfig
        {
            TRUE,            // bEnableBlackLight
            TRUE,            // bEnableHistStretch
            FALSE,           // bEnableAntiOverExposure
            TRUE,            // bEnableTimeLPF
            TRUE,            // bEnableCaptureThres
            TRUE,            // bEnableVideoThres
            TRUE,            // bEnableStrobeThres
            47,                // u4AETarget
            47,                // u4StrobeAETarget

            50,                // u4InitIndex
            4,                 // u4BackLightWeight
            32,                // u4HistStretchWeight
            4,                 // u4AntiOverExpWeight
            2,                 // u4BlackLightStrengthIndex
            2,                 // u4HistStretchStrengthIndex
            2,                 // u4AntiOverExpStrengthIndex
            2,                 // u4TimeLPFStrengthIndex
            {1, 3, 5, 7, 8}, // u4LPFConvergeTable[AE_CCT_STRENGTH_NUM]
            90,                // u4InDoorEV = 9.0, 10 base
            -2,               // i4BVOffset delta BV = -2.3
            64,                 // u4PreviewFlareOffset
            64,                 // u4CaptureFlareOffset
            3,                 // u4CaptureFlareThres
            64,                 // u4VideoFlareOffset
            3,                 // u4VideoFlareThres
            64,                 // u4StrobeFlareOffset //12 bits
            3,                 // u4StrobeFlareThres // 0.5%
            160,                 // u4PrvMaxFlareThres //12 bit
            0,                 // u4PrvMinFlareThres
            160,                 // u4VideoMaxFlareThres // 12 bit
            0,                 // u4VideoMinFlareThres
            18,                // u4FlatnessThres              // 10 base for flatness condition.
            75                 // u4FlatnessStrength
         }
    },

    // AWB NVRAM
    {
        // AWB calibration data
        {
            // rUnitGain (unit gain: 1.0 = 512)
            {
			0,	// UnitGain_R		
			0,	// UnitGain_G				
			0	// UnitGain_B				
            },
            // rGoldenGain (golden sample gain: 1.0 = 512)
            {
			0,	// GoldenGain_R				
			0,	// GoldenGain_G				
			0	// GoldenGain_B				
            },
            // rTuningUnitGain (Tuning sample unit gain: 1.0 = 512)
            {
			0,	// TuningUnitGain_R				
			0,	// TuningUnitGain_G				
			0	// TuningUnitGain_B				
            },
            // rD65Gain (D65 WB gain: 1.0 = 512)
            {
			776,	// D65Gain_R				
			512,	// D65Gain_G				
			634	// D65Gain_B				
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
			-64,	// OriX_Strobe	
			-441	// OriY_Strobe		
            },
            // Horizon
            {
		-490,	// OriX_Hor
		-308	// OriY_Hor	
            },
            // A
            {
			-365,	// OriX_A	
			-306	    // OriY_A	
            },
            // TL84
            {
			-208,	// OriX_TL84		
			-319	// OriY_TL84		
            },
            // CWF
            {
			-163,	// OriX_CWF	
			-440	// OriY_CWF	
            },
            // DNP
            {
			-124,	// OriX_DNP		
			-277	// OriY_DNP		
            },
            // D65
            {
			75,	// OriX_D65			
			-232	// OriY_D65			
            },
            // DF
            {
			54, 	// OriX_DF
			-386	// OriY_DF	
            }
        },
        // Rotated XY coordinate of AWB light source
        {
            // Strobe
            {
			-139, 	// RotX_Strobe				              
			-423	 	// RotY_Strobe				              
            },
            // Horizon
            {
			-535,	// RotX_Hor				                  
			-219	// RotY_Hor				                  
            },
            // A
            {
			-412,	// RotX_A				                    
			-238	// RotY_A				                    
            },
            // TL84
            {
			-260,	// RotX_TL84				                
			-278	// RotY_TL84				                
            },
            // CWF
            {
			-236,	// RotX_CWF				                  
			-405	// RotY_CWF				                  
            },
            // DNP
            {
			-170,	// RotX_DNP				                  
			-251	// RotY_DNP				                  
            },
            // D65
            {
			34,	// RotX_D65				                    
			-241	// RotY_D65				                  
            },
            // DF
            {
			-13,	// RotX_DF				                    
			-389	// RotY_DF				                      
            }
        },
        // AWB gain of AWB light source
        {
            // Strobe
            {
			854,	// AWBGAIN_STROBE_R				          
			512,	// AWBGAIN_STROBE_G				          
			1014	// AWBGAIN_STROBE_B				            
            },
            // Horizon
            {
			512,	// AWBGAIN_HOR_R				            
			655,	// AWBGAIN_HOR_G				            
			1928	// AWBGAIN_HOR_B				            
            },
            // A
            {
			512,	// AWBGAIN_A_R				              
			554,	// AWBGAIN_A_G				              
			1375	// AWBGAIN_A_B				              
            },
            // TL84
            {
			595,	// AWBGAIN_TL84_R				            
			512,	// AWBGAIN_TL84_G				            
			1046	// AWBGAIN_TL84_B				            
            },
            // CWF
            {
			745,	// AWBGAIN_CWF_R				            
			512,	// AWBGAIN_CWF_G				            
			1159	// AWBGAIN_CWF_B				            
            },
            // DNP
            {
			630,	// AWBGAIN_DNP_R				            
			512,	// AWBGAIN_DNP_G				            
			880	// AWBGAIN_DNP_B				              
            },
            // D65
            {
			776,	// AWBGAIN_D65_R				            
			512,	// AWBGAIN_D65_G				            
			634	// AWBGAIN_D65_B				              
            },
            // DF
            {
			929,	// AWBGAIN_DF_R				              
			512,	// AWBGAIN_DF_G				              
			803	// AWBGAIN_DF_B				                
            }
        },
        // Rotation matrix parameter
        {
		10,	// RotationAngle					              
		252,	// Cos					                      
		44,	// Sin					                        
        },
        // Daylight locus parameter
        {
		-178,	// SlopeNumerator					            
		128	// SlopeDenominator					            
        },
        // AWB light area
        {
		// Strobe						                        
            {
			-110,	// StrobeRightBound				            
			-220,	// StrobeLeftBound				          
			-386,	// StrobeUpperBound				          
			-455	// StrobeLowerBound				          
            },
            // Tungsten
            {
			-380,	// TungRightBound				            
			-960,	// TungLeftBound				            
			-178,	// TungUpperBound				            
			-278	// TungLowerBound				            
            },
            // Warm fluorescent
            {
			-380,	// WFluoRightBound				          
			-960,	// WFluoLeftBound				            
			-278,	// WFluoUpperBound				          
			-398	// WFluoLowerBound				          
            },
            // Fluorescent
            {
			-220,	// FluoRightBound				            
			-380,	// FluoLeftBound				            
			-170,	// FluoUpperBound				            
			-341	// FluoLowerBound				            
            },
            // CWF
            {
			-70,	// CWFRightBound				            
			-380,	// CWFLeftBound				              
			-341,	// CWFUpperBound				            
			-455	// CWFLowerBound				            
            },
            // Daylight
            {
			59,	// DayRightBound				              
			-220,	// DayLeftBound				              
			-161,	// DayUpperBound				            
			-341	// DayLowerBound				            
            },
            // Shade
            {
			419,	// ShadeRightBound				          
			59,	// ShadeLeftBound				              
			-161,	// ShadeUpperBound				          
			-259	// ShadeLowerBound				          
            },
            // Daylight Fluorescent
            {
			59,	// DFRightBound				                
			-70,	// DFLeftBound				                
			-341,	// DFUpperBound				                
			-450	// DFLowerBound				                  
            }
        },
        // PWB light area
        {
            // Reference area
            {
			419,	// PRefRightBound				            
			-960,	// PRefLeftBound				            
			-136,	// PRefUpperBound				            
			-455	// PRefLowerBound				            
            },
            // Daylight
            {
			84,	// PDayRightBound				            
			-220,	// PDayLeftBound				            
			-161,	// PDayUpperBound				            
			-321	// PDayLowerBound				            
            },
            // Cloudy daylight
            {
			184,	// PCloudyRightBound				        
			9,	// PCloudyLeftBound				            
			-161,	// PCloudyUpperBound				        
			-321	// PCloudyLowerBound				        
            },
            // Shade
            {
			284,	// PShadeRightBound				          
			9,	// PShadeLeftBound				          
			-161,	// PShadeUpperBound				          
			-321	// PShadeLowerBound				          
            },
            // Twilight
            {
			-220,	// PTwiRightBound				            
			-380,	// PTwiLeftBound				            
			-161,	// PTwiUpperBound				            
			-321	// PTwiLowerBound				            
            },
            // Fluorescent
            {
			84,	// PFluoRightBound				          
			-360,	// PFluoLeftBound				            
			-191,	// PFluoUpperBound				          
			-455	// PFluoLowerBound				          
            },
            // Warm fluorescent
            {
			-312,	// PWFluoRightBound				          
			-512,	// PWFluoLeftBound				          
			-191,	// PWFluoUpperBound				          
			-455	// PWFluoLowerBound				          
            },
            // Incandescent
            {
			-312,	// PIncaRightBound				          
			-512,	// PIncaLeftBound				            
			-161,	// PIncaUpperBound				          
			-321	// PIncaLowerBound				          
            },
            // Gray World
            {
			5000,	// PGWRightBound				            
			-5000,	// PGWLeftBound				            
			5000,	// PGWUpperBound				            
			-5000	// PGWLowerBound				            
            }
        },
        // PWB default gain
        {
            // Daylight
            {
			693,	// PWB_Day_R				                
			512,	// PWB_Day_G				                
			743	// PWB_Day_B				                  
            },
            // Cloudy daylight
            {
			831,	// PWB_Cloudy_R				              
			512,	// PWB_Cloudy_G				              
			574	// PWB_Cloudy_B				                
            },
            // Shade
            {
			878,	// PWB_Shade_R				              
			512,	// PWB_Shade_G				              
			531	// PWB_Shade_B				                
            },
            // Twilight
            {
			537,	// PWB_Twi_R				                
			512,	// PWB_Twi_G				                
			1069	// PWB_Twi_B				                
            },
            // Fluorescent
            {
			730,	// PWB_Fluo_R				                
			512,	// PWB_Fluo_G				                
			907	// PWB_Fluo_B				                  
            },
            // Warm fluorescent
            {
			540,	// PWB_WFluo_R				              
			512,	// PWB_WFluo_G				              
			1394	// PWB_WFluo_B				              
            },
            // Incandescent
            {
			475,	// PWB_Inca_R				                
			512,	// PWB_Inca_G				                
			1274	// PWB_Inca_B				                
            },
            // Gray World
            {
			512,	// PWB_GW_R				                  
			512,	// PWB_GW_G				                  
			512	// PWB_GW_B				                    
            }
        },
        // AWB preference color
        {
            // Tungsten
            {
			50,	// TUNG_SLIDER				                
			4753	// TUNG_OFFS				                
            },
            // Warm fluorescent
            {
			50,	// WFluo_SLIDER				                
			4753	// WFluo_OFFS				                
            },
            // Shade
            {
			50,	// Shade_SLIDER				                
			845	// Shade_OFFS				                  
            },
            // Daylight WB gain
            {
			723,	// Day_GAIN_R				                
			512,	// Day_GAIN_G				                
			696	// Day_GAIN_B				                  
            },
            // Preference gain: strobe
            {
			512,	// PRF_STROBE_R				              
			512,	// PRF_STROBE_G				              
			512	// PRF_STROBE_B				                
            },
            // Preference gain: tungsten
            {
			512,	// PRF_TUNG_R				                
			512,	// PRF_TUNG_G				                
			512	// PRF_TUNG_B				                  
            },
            // Preference gain: warm fluorescent
            {
			512,	// PRF_WFluo_R				              
			512,	// PRF_WFluo_G				              
			512	// PRF_WFluo_B				                
            },
            // Preference gain: fluorescent
            {
			512,	// PRF_Fluo_R				                
			512,	// PRF_Fluo_G				                
			512	// PRF_Fluo_B				                  
            },
            // Preference gain: CWF
            {
			512,	// PRF_CWF_R				                
			512,	// PRF_CWF_G				                
			512	// PRF_CWF_B				                  
            },
            // Preference gain: daylight
            {
			512,	// PRF_Day_R				                
			512,	// PRF_Day_G				                
			512	// PRF_Day_B				                  
            },
            // Preference gain: shade
            {
			512,	// PRF_Shade_R				              
			512,	// PRF_Shade_G				              
			512	// PRF_Shade_B				                
            },
            // Preference gain: daylight fluorescent
            {
			512,	// PRF_DF_R				                  
			512,	// PRF_DF_G				                  
			512	// PRF_DF_B				                    
            }
        },
	// CCT estimation							                
	{							                                
		// CCT						                          
		{						                                
			2300,	// CCT0				                      
			2850,	// CCT1				                      
			4100,	// CCT2				                      
			5100,	// CCT3				                      
			6500 	// CCT4				                      
            },
		// Rotated X coordinate						          
		{						                                
			-569,	// RotatedXCoordinate0				      
			-446,	// RotatedXCoordinate1				      
			-294,	// RotatedXCoordinate2				      
			-204,	// RotatedXCoordinate3				      
			0 	// RotatedXCoordinate4				        
            }
        }
    },
                                               
	{0}  //FIXED                                  
};

#include INCLUDE_FILENAME_ISP_LSC_PARAM
//};  //  namespace

const CAMERA_TSF_TBL_STRUCT CAMERA_TSF_DEFAULT_VALUE =
{
    #include INCLUDE_FILENAME_TSF_PARA
    #include INCLUDE_FILENAME_TSF_DATA
};


typedef NSFeature::RAWSensorInfo<SENSOR_ID> SensorInfoSingleton_T;


namespace NSFeature {
template <>
UINT32
SensorInfoSingleton_T::
impGetDefaultData(CAMERA_DATA_TYPE_ENUM const CameraDataType, VOID*const pDataBuf, UINT32 const size) const
{
    UINT32 dataSize[CAMERA_DATA_TYPE_NUM] = {sizeof(NVRAM_CAMERA_ISP_PARAM_STRUCT),
                                             sizeof(NVRAM_CAMERA_3A_STRUCT),
                                             sizeof(NVRAM_CAMERA_SHADING_STRUCT),
                                             sizeof(NVRAM_LENS_PARA_STRUCT),
                                             sizeof(AE_PLINETABLE_T),
                                             0,
                                             sizeof(CAMERA_TSF_TBL_STRUCT)};

    if (CameraDataType > CAMERA_DATA_TSF_TABLE || NULL == pDataBuf || (size < dataSize[CameraDataType]))
    {
        return 1;
    }

    switch(CameraDataType)
    {
        case CAMERA_NVRAM_DATA_ISP:
            memcpy(pDataBuf,&CAMERA_ISP_DEFAULT_VALUE,sizeof(NVRAM_CAMERA_ISP_PARAM_STRUCT));
            break;
        case CAMERA_NVRAM_DATA_3A:
            memcpy(pDataBuf,&CAMERA_3A_NVRAM_DEFAULT_VALUE,sizeof(NVRAM_CAMERA_3A_STRUCT));
            break;
        case CAMERA_NVRAM_DATA_SHADING:
            memcpy(pDataBuf,&CAMERA_SHADING_DEFAULT_VALUE,sizeof(NVRAM_CAMERA_SHADING_STRUCT));
            break;
        case CAMERA_DATA_AE_PLINETABLE:
            memcpy(pDataBuf,&g_PlineTableMapping,sizeof(AE_PLINETABLE_T));
            break;
        case CAMERA_DATA_TSF_TABLE:
            memcpy(pDataBuf,&CAMERA_TSF_DEFAULT_VALUE,sizeof(CAMERA_TSF_TBL_STRUCT));
            break;
        default:
            break;
    }
    return 0;
}};  //  NSFeature


