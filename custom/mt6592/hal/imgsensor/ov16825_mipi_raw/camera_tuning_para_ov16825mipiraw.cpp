#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_ov16825mipiraw.h"
#include "camera_info_ov16825mipiraw.h"
#include "camera_custom_AEPlinetable.h"
#include "camera_custom_tsf_tbl.h"
//#include "camera_custom_flicker_table.h"
//#include "camera_flicker_table_ov16825mipiraw.h"


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
        79100,    // i4R_AVG
        15119,    // i4R_STD
        93975,    // i4B_AVG
        16446,    // i4B_STD
        {  // i4P00[9]
            3612500, -842500, -532500, -792500, 4440000, -1050000, -60000, -1722500, 4322500
        },
        {  // i4P10[9]
            1273285, 135597, -428100, -300276, -412170, 601827, -238239, 535021, -224922
        },
        {  // i4P01[9]
            715327, 251013, -518101, -283687, -267085, 493209, -253135, 478218, -186503
        },
        {  // i4P20[9]
            0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        {  // i4P11[9]
            0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        {  // i4P02[9]
            0, 0, 0, 0, 0, 0, 0, 0, 0
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
            1144,    // u4MinGain, 1024 base = 1x
            10240,    // u4MaxGain, 16x
            36,    // u4MiniISOGain, ISOxx  
            64,    // u4GainStepUnit, 1x/8 
            19,    // u4PreExpUnit 
            30,    // u4PreMaxFrameRate
            19,    // u4VideoExpUnit  
            30,    // u4VideoMaxFrameRate 
            1024,    // u4Video2PreRatio, 1024 base = 1x 
            20,    // u4CapExpUnit 
            15,    // u4CapMaxFrameRate
            1024,    // u4Cap2PreRatio, 1024 base = 1x
            28,    // u4LensFno, Fno = 2.8
            350    // u4FocusLength_100x
        },
        // rHistConfig
        {
            2,    // u4HistHighThres
            40,    // u4HistLowThres
            2,    // u4MostBrightRatio
            1,    // u4MostDarkRatio
            160,    // u4CentralHighBound
            20,    // u4CentralLowBound
            {240, 230, 220, 210, 200},    // u4OverExpThres[AE_CCT_STRENGTH_NUM] 
            {88, 115, 135, 155, 175},    // u4HistStretchThres[AE_CCT_STRENGTH_NUM] 
            {22, 26, 30, 364, 38}    // u4BlackLightThres[AE_CCT_STRENGTH_NUM] 
        },
        // rCCTConfig
        {
            TRUE,    // bEnableBlackLight
            TRUE,    // bEnableHistStretch
            FALSE,    // bEnableAntiOverExposure
            TRUE,    // bEnableTimeLPF
            FALSE,    // bEnableCaptureThres
            FALSE,    // bEnableVideoThres
            FALSE,    // bEnableStrobeThres
            50,    // u4AETarget
            0,    // u4StrobeAETarget
            20,    // u4InitIndex
            32,    // u4BackLightWeight
            32,    // u4HistStretchWeight
            4,    // u4AntiOverExpWeight
            4,    // u4BlackLightStrengthIndex
            4,    // u4HistStretchStrengthIndex
            2,    // u4AntiOverExpStrengthIndex
            4,    // u4TimeLPFStrengthIndex
            {1, 3, 5, 7, 8},    // u4LPFConvergeTable[AE_CCT_STRENGTH_NUM] 
            90,    // u4InDoorEV = 9.0, 10 base 
            12,    // i4BVOffset delta BV = value/10 
            64,    // u4PreviewFlareOffset
            64,    // u4CaptureFlareOffset
            5,    // u4CaptureFlareThres
            64,    // u4VideoFlareOffset
            5,    // u4VideoFlareThres
            64,    // u4StrobeFlareOffset
            2,    // u4StrobeFlareThres
            8,    // u4PrvMaxFlareThres
            0,    // u4PrvMinFlareThres
            8,    // u4VideoMaxFlareThres
            0,    // u4VideoMinFlareThres
            18,    // u4FlatnessThres    // 10 base for flatness condition.
            55    // u4FlatnessStrength
        }
    },
    // AWB NVRAM
    {
        // AWB calibration data
        {
            // rUnitGain (unit gain: 1.0 = 512)
            {
                0,    // i4R
                0,    // i4G
                0    // i4B
            },
            // rGoldenGain (golden sample gain: 1.0 = 512)
            {
                0,    // i4R
                0,    // i4G
                0    // i4B
            },
            // rTuningUnitGain (Tuning sample unit gain: 1.0 = 512)
            {
                0,    // i4R
                0,    // i4G
                0    // i4B
            },
            // rD65Gain (D65 WB gain: 1.0 = 512)
            {            
						830,    // i4R
            512,    // i4G
            840    // i4B
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
                53,    // i4X
                -441    // i4Y
            },
            // Horizon
            {
                -309,    // i4X
                -307    // i4Y
            },
            // A
            {
                -212,    // i4X
                -341    // i4Y
            },
            // TL84
            {
                -72,    // i4X
                -385    // i4Y
            },
            // CWF
            {
                -55,    // i4X
                -460    // i4Y
            },
            // DNP
            {
                39,    // i4X
                -410    // i4Y
            },
            // D65
            {
                134,    // i4X
                -353    // i4Y
            },
            // DF
            {
                101,    // i4X
                -453    // i4Y
            }
        },
        // Rotated XY coordinate of AWB light source
        {
            // Strobe
            {
                37,    // i4X
                -443    // i4Y
            },
            // Horizon
            {
                -320,    // i4X
                -296    // i4Y
            },
            // A
            {
                -224,    // i4X
                -334    // i4Y
            },
            // TL84
            {
                -86,    // i4X
                -382    // i4Y
            },
            // CWF
            {
                -71,    // i4X
                -458    // i4Y
            },
            // DNP
            {
                25,    // i4X
                -411    // i4Y
            },
            // D65
            {
                122,    // i4X
                -358    // i4Y
            },
            // DF
            {
                85,    // i4X
                -457    // i4Y
            }
        },
        // AWB gain of AWB light source
        {
            // Strobe 
            {
                1000,    // i4R
                512,    // i4G
                865    // i4B
            },
            // Horizon 
            {
                512,    // i4R
                513,    // i4G
                1182    // i4B
            },
            // A 
            {
                610,    // i4R
                512,    // i4G
                1083    // i4B
            },
            // TL84 
            {
                783,    // i4R
                512,    // i4G
                951    // i4B
            },
            // CWF 
            {            
						850,    // i4R
            512,    // i4G
            820    // i4B
            },
            // DNP 
            {
                941,    // i4R
                512,    // i4G
                846    // i4B
            },
            // D65 
            {
						830,    // i4R
            512,    // i4G
            840    // i4B
            },
            // DF 
            {
                1085,    // i4R
                512,    // i4G
                825    // i4B
            }
        },
        // Rotation matrix parameter
        {
            2,    // i4RotationAngle
            256,    // i4Cos
            9    // i4Sin
        },
        // Daylight locus parameter
        {
            -114,    // i4SlopeNumerator
            128    // i4SlopeDenominator
        },
        // AWB light area
        {
            // Strobe:FIXME
            {
            0,    // i4RightBound
            0,    // i4LeftBound
            0,    // i4UpperBound
            0    // i4LowerBound
            },
            // Tungsten
            {
            -136,    // i4RightBound
            -500,    // i4LeftBound
            -300,    // i4UpperBound
            -420    // i4LowerBound
            },
            // Warm fluorescent
            {
            -136,    // i4RightBound
            -450,    // i4LeftBound
            -420,    // i4UpperBound
            -550    // i4LowerBound
            },
            // Fluorescent
            {
            -35,    // i4RightBound
            -136,    // i4LeftBound
            -300,    // i4UpperBound
            -460    // i4LowerBound
            },
            // CWF
            {
            -35,    // i4RightBound
            -136,    // i4LeftBound
            -460,    // i4UpperBound
            -550    // i4LowerBound
            },
            // Daylight
            {
            120,    // i4RightBound
            -35,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Shade
            {
            480,    // i4RightBound
            160,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Daylight Fluorescent
            {
            210,    // i4RightBound
            -35,    // i4LeftBound
            -440,    // i4UpperBound
            -550    // i4LowerBound
            }
        },
        // PWB light area
        {
            // Reference area
            {
            480,    // i4RightBound
            -786,    // i4LeftBound
            0,    // i4UpperBound
            -550    // i4LowerBound
            },
            // Daylight
            {
            145,    // i4RightBound
            -35,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Cloudy daylight
            {
            245,    // i4RightBound
            70,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Shade
            {
            345,    // i4RightBound
            70,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Twilight
            {
            -35,    // i4RightBound
            -195,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Fluorescent
            {
            172,    // i4RightBound
            -186,    // i4LeftBound
            -308,    // i4UpperBound
            -508    // i4LowerBound
            },
            // Warm fluorescent
            {
            -124,    // i4RightBound
            -324,    // i4LeftBound
            -308,    // i4UpperBound
            -508    // i4LowerBound
            },
            // Incandescent
            {
            -124,    // i4RightBound
            -324,    // i4LeftBound
            -330,    // i4UpperBound
            -440    // i4LowerBound
            },
            // Gray World
            {
            5000,    // i4RightBound
            -5000,    // i4LeftBound
            5000,    // i4UpperBound
            -5000    // i4LowerBound
            }
        },
        // PWB default gain	
        {
            // Daylight
            {            
						830,    // i4R
            512,    // i4G
            840    // i4B
            },
            // Cloudy daylight
            {
            1078,    // i4R
            512,    // i4G
            679    // i4B
            },
            // Shade
            {
            1150,    // i4R
            512,    // i4G
            633    // i4B
            },
            // Twilight
            {
            755,    // i4R
            512,    // i4G
            994    // i4B
            },
            // Fluorescent
            {
            898,    // i4R
            512,    // i4G
            880    // i4B
            },
            // Warm fluorescent
            {
            677,    // i4R
            512,    // i4G
            1193    // i4B
            },
            // Incandescent
            {
            655,    // i4R
            512,    // i4G
            1158    // i4B
            },
            // Gray World
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            }
        },
        // AWB preference color	
        {
             // Tungsten
            {
            20,    // i4SliderValue
            4719    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            0,    // i4SliderValue
            4519    // i4OffsetThr
            },
            // Shade
            {
            0,    // i4SliderValue
            1518    // i4OffsetThr
            },
            // Daylight WB gain
            {
						830,    // i4R
            512,    // i4G
            840    // i4B
            },
            // Preference gain: strobe
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: tungsten
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: warm fluorescent
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: fluorescent
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: CWF
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: daylight
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: shade
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: daylight fluorescent
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            }
        },
        {// CCT estimation
            {// CCT
                2300,    // i4CCT[0]
                2850,    // i4CCT[1]
                4100,    // i4CCT[2]
                5100,    // i4CCT[3]
                6500    // i4CCT[4]
            },
            {// Rotated X coordinate
                -442,    // i4RotatedXCoordinate[0]
                -346,    // i4RotatedXCoordinate[1]
                -208,    // i4RotatedXCoordinate[2]
                -97,    // i4RotatedXCoordinate[3]
                0    // i4RotatedXCoordinate[4]
            }
        }
    },
    {0}
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

