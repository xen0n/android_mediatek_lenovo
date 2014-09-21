#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_ar1330mipiraw.h"
#include "camera_info_ar1330mipiraw.h"
#include "camera_custom_AEPlinetable.h"

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
        76100,    // i4R_AVG
        16005,    // i4R_STD
        88950,    // i4B_AVG
        21483,    // i4B_STD
        {  // i4P00[9]
            4512500, -1550000, -397500, -585000, 3025000, 117500, 157500, -1932500, 4330000
        },
        {  // i4P10[9]
            968437, -948433, -21649, -66038, 110072, -36092, -21086, 361301, -323951
        },
        {  // i4P01[9]
            502752, -398286, -105414, -222854, 103016, 126615, -109484, -212265, 328205
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
            1195,   // u4MinGain, 1024 base =  1x
            16384,  // u4MaxGain, 16x
            55,     // u4MiniISOGain, ISOxx
            128,    // u4GainStepUnit, 1x/8
            33,     // u4PreExpUnit
            30,     // u4PreMaxFrameRate
            33,     // u4VideoExpUnit
            30,     // u4VideoMaxFrameRate
            1024,   // u4Video2PreRatio, 1024 base = 1x
            58,     // u4CapExpUnit
            15,     // u4CapMaxFrameRate
            1024,   // u4Cap2PreRatio, 1024 base = 1x
            28,      // u4LensFno, Fno = 2.8
            350     // u4FocusLength_100x
         },
         // rHistConfig
        {
            2,   // u4HistHighThres
            40,  // u4HistLowThres
            2,   // u4MostBrightRatio
            1,   // u4MostDarkRatio
            160, // u4CentralHighBound
            20,  // u4CentralLowBound
            {240, 230, 220, 210, 200}, // u4OverExpThres[AE_CCT_STRENGTH_NUM]
            {86, 108, 128, 148, 170},  // u4HistStretchThres[AE_CCT_STRENGTH_NUM]
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
            -7,    // i4BVOffset delta BV = value/10 
            64,    // u4PreviewFlareOffset
            64,    // u4CaptureFlareOffset
            5,    // u4CaptureFlareThres
            64,    // u4VideoFlareOffset
            5,    // u4VideoFlareThres
            64,    // u4StrobeFlareOffset
            2,    // u4StrobeFlareThres
            8,                 // u4PrvMaxFlareThres
            0,                 // u4PrvMinFlareThres
            8,                 // u4VideoMaxFlareThres
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
                997,    // i4R
                512,    // i4G
                640    // i4B
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
                50,    // i4X
                -355    // i4Y
            },
            // Horizon
            {
                -320,    // i4X
                -271    // i4Y
            },
            // A
            {
                -205,    // i4X
                -305    // i4Y
            },
            // TL84
            {
                -43,    // i4X
                -367    // i4Y
            },
            // CWF
            {
                -15,    // i4X
                -406    // i4Y
            },
            // DNP
            {
                50,    // i4X
                -355    // i4Y
            },
            // D65
            {
                164,    // i4X
                -329    // i4Y
            },
            // DF
            {
                141,    // i4X
                -406    // i4Y
            }
        },
        // Rotated XY coordinate of AWB light source
        {
            // Strobe
            {
                50,    // i4X
                -355    // i4Y
            },
            // Horizon
            {
                -320,    // i4X
                -271    // i4Y
            },
            // A
            {
                -205,    // i4X
                -305    // i4Y
            },
            // TL84
            {
                -43,    // i4X
                -367    // i4Y
            },
            // CWF
            {
                -15,    // i4X
                -406    // i4Y
            },
            // DNP
            {
                50,    // i4X
                -355    // i4Y
            },
            // D65
            {
                164,    // i4X
                -329    // i4Y
            },
            // DF
            {
                141,    // i4X
                -406    // i4Y
            }
        },
        // AWB gain of AWB light source
        {
            // Strobe 
            {
                885,    // i4R
                512,    // i4G
                774    // i4B
            },
            // Horizon 
            {
                512,    // i4R
                547,    // i4G
                1219    // i4B
            },
            // A 
            {
                586,    // i4R
                512,    // i4G
                1021    // i4B
            },
            // TL84 
            {
                794,    // i4R
                512,    // i4G
                892    // i4B
            },
            // CWF 
            {
                869,    // i4R
                512,    // i4G
                905    // i4B
            },
            // DNP 
            {
                885,    // i4R
                512,    // i4G
                774    // i4B
            },
            // D65 
            {
                997,    // i4R
                512,    // i4G
                640    // i4B
            },
            // DF 
            {
                1073,    // i4R
                512,    // i4G
                733    // i4B
            }
        },
        // Rotation matrix parameter
        {
            0,    // i4RotationAngle
            256,    // i4Cos
            0    // i4Sin
        },
        // Daylight locus parameter
        {
            -110,    // i4SlopeNumerator
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
            -93,    // i4RightBound
            -743,    // i4LeftBound
            -238,    // i4UpperBound
            -338    // i4LowerBound
            },
            // Warm fluorescent
            {
            -93,    // i4RightBound
            -743,    // i4LeftBound
            -338,    // i4UpperBound
            -458    // i4LowerBound
            },
            // Fluorescent
            {
            0,    // i4RightBound
            -93,    // i4LeftBound
            -243,    // i4UpperBound
            -386    // i4LowerBound
            },
            // CWF
            {
            0,    // i4RightBound
            -93,    // i4LeftBound
            -386,    // i4UpperBound
            -456    // i4LowerBound
            },
            // Daylight
            {
            189,    // i4RightBound
            0,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
            },
            // Shade
            {
            549,    // i4RightBound
            189,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
            },
            // Daylight Fluorescent
            {
            0,    // i4RightBound
            0,    // i4LeftBound
            0,    // i4UpperBound
            0    // i4LowerBound
            }
        },
        // PWB light area
        {
            // Reference area
            {
            549,    // i4RightBound
            -743,    // i4LeftBound
            0,    // i4UpperBound
            -458    // i4LowerBound
            },
            // Daylight
            {
            214,    // i4RightBound
            0,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
            },
            // Cloudy daylight
            {
            314,    // i4RightBound
            139,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
            },
            // Shade
            {
            414,    // i4RightBound
            139,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
            },
            // Twilight
            {
            0,    // i4RightBound
            -160,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
            },
            // Fluorescent
            {
            214,    // i4RightBound
            -143,    // i4LeftBound
            -279,    // i4UpperBound
            -456    // i4LowerBound
            },
            // Warm fluorescent
            {
            -105,    // i4RightBound
            -305,    // i4LeftBound
            -279,    // i4UpperBound
            -456    // i4LowerBound
            },
            // Incandescent
            {
            -105,    // i4RightBound
            -305,    // i4LeftBound
            -249,    // i4UpperBound
            -409    // i4LowerBound
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
            924,    // i4R
            512,    // i4G
            692    // i4B
            },
            // Cloudy daylight
            {
            1086,    // i4R
            512,    // i4G
            588    // i4B
            },
            // Shade
            {
            1162,    // i4R
            512,    // i4G
            550    // i4B
            },
            // Twilight
            {
            717,    // i4R
            512,    // i4G
            891    // i4B
            },
            // Fluorescent
            {
            884,    // i4R
            512,    // i4G
            803    // i4B
            },
            // Warm fluorescent
            {
            638,    // i4R
            512,    // i4G
            1111    // i4B
            },
            // Incandescent
            {
            606,    // i4R
            512,    // i4G
            1055    // i4B
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
            0,    // i4SliderValue
            6059    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            0,    // i4SliderValue
            4510    // i4OffsetThr
            },
            // Shade
            {
            0,    // i4SliderValue
            1341    // i4OffsetThr
            },
            // Daylight WB gain
            {
            855,    // i4R
            512,    // i4G
            747    // i4B
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
            550,    // i4R
            512,    // i4G
            480    // i4B
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
                -484,    // i4RotatedXCoordinate[0]
                -369,    // i4RotatedXCoordinate[1]
                -207,    // i4RotatedXCoordinate[2]
                -114,    // i4RotatedXCoordinate[3]
                0    // i4RotatedXCoordinate[4]
            }
        }
    },
    {0}
};

#include INCLUDE_FILENAME_ISP_LSC_PARAM
//};  //  namespace


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
                                             sizeof(AE_PLINETABLE_T)};

    if (CameraDataType > CAMERA_DATA_AE_PLINETABLE || NULL == pDataBuf || (size < dataSize[CameraDataType]))
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
        default:
            break;
    }
    return 0;
}};  //  NSFeature


