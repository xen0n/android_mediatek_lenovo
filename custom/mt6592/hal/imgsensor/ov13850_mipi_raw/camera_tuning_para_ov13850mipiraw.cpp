#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_ov13850mipiraw.h"
#include "camera_info_ov13850mipiraw.h"
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
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        }
    },
    ISPPca:{
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
        79800,    // i4R_AVG
        17214,    // i4R_STD
        93550,    // i4B_AVG
        18105,    // i4B_STD
        {  // i4P00[9]
            5260000, -2692500, -5000, -770000, 3872500, -540000, -142500, -2547500, 5262500
        },
        {  // i4P10[9]
            1875785, -2334012, 446125, -85715, -278656, 379487, 235254, 246532, -472697
        },
        {  // i4P01[9]
            960615, -1281413, 309004, -208480, -396641, 620089, 43045, -484208, 449782
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
            90,    // u4MiniISOGain, ISOxx  
            32,    // u4GainStepUnit, 1x/8 
            11,    // u4PreExpUnit 
            30,    // u4PreMaxFrameRate
            11,    // u4VideoExpUnit  
            30,    // u4VideoMaxFrameRate 
            1024,    // u4Video2PreRatio, 1024 base = 1x 
            13,    // u4CapExpUnit 
            24,    // u4CapMaxFrameRate
            1024,    // u4Cap2PreRatio, 1024 base = 1x
            22,    // u4LensFno, Fno = 2.8
            385    // u4FocusLength_100x
        },
        // rHistConfig
        {
            4,    // u4HistHighThres
            40,    // u4HistLowThres
            2,    // u4MostBrightRatio
            1,    // u4MostDarkRatio
            160,    // u4CentralHighBound
            20,    // u4CentralLowBound
            {240, 230, 220, 210, 200},    // u4OverExpThres[AE_CCT_STRENGTH_NUM] 
            {82, 108, 128, 148, 170},    // u4HistStretchThres[AE_CCT_STRENGTH_NUM] 
            {18, 22, 26, 30, 34}    // u4BlackLightThres[AE_CCT_STRENGTH_NUM] 
        },
        // rCCTConfig
        {
            TRUE,    // bEnableBlackLight
            TRUE,    // bEnableHistStretch
            TRUE,    // bEnableAntiOverExposure
            TRUE,    // bEnableTimeLPF
            TRUE,    // bEnableCaptureThres
            TRUE,    // bEnableVideoThres
            TRUE,    // bEnableStrobeThres
            50,    // u4AETarget
            50,    // u4StrobeAETarget
            80,    // u4InitIndex
            4,    // u4BackLightWeight
            32,    // u4HistStretchWeight
            4,    // u4AntiOverExpWeight
            2,    // u4BlackLightStrengthIndex
            2,    // u4HistStretchStrengthIndex
            2,    // u4AntiOverExpStrengthIndex
            2,    // u4TimeLPFStrengthIndex
            {1, 3, 5, 7, 8},    // u4LPFConvergeTable[AE_CCT_STRENGTH_NUM] 
            90,    // u4InDoorEV = 9.0, 10 base 
            -5,    // i4BVOffset delta BV = value/10 
            64,    // u4PreviewFlareOffset
            64,    // u4CaptureFlareOffset
            4,    // u4CaptureFlareThres
            64,    // u4VideoFlareOffset
            4,    // u4VideoFlareThres
            32,    // u4StrobeFlareOffset
            2,    // u4StrobeFlareThres
            160,    // u4PrvMaxFlareThres
            4,    // u4PrvMinFlareThres
            160,    // u4VideoMaxFlareThres
            4,    // u4VideoMinFlareThres
            18,    // u4FlatnessThres    // 10 base for flatness condition.
            60    // u4FlatnessStrength
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
                1000,    // i4R
                512,    // i4G
                706    // i4B
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
                129,    // i4X
                -366    // i4Y
            },
            // Horizon
            {
                -367,    // i4X
                -329    // i4Y
            },
            // A
            {
                -246,    // i4X
                -350    // i4Y
            },
            // TL84
            {
                -77,    // i4X
                -365    // i4Y
            },
            // CWF
            {
                -51,    // i4X
                -419    // i4Y
            },
            // DNP
            {
                -5,    // i4X
                -364    // i4Y
            },
            // D65
            {
                129,    // i4X
                -366    // i4Y
            },
            // DF
            {
                0,    // i4X
                0    // i4Y
            }
        },
        // Rotated XY coordinate of AWB light source
        {
            // Strobe
            {
                129,    // i4X
                -366    // i4Y
            },
            // Horizon
            {
                -367,    // i4X
                -329    // i4Y
            },
            // A
            {
                -246,    // i4X
                -350    // i4Y
            },
            // TL84
            {
                -77,    // i4X
                -365    // i4Y
            },
            // CWF
            {
                -51,    // i4X
                -419    // i4Y
            },
            // DNP
            {
                -5,    // i4X
                -364    // i4Y
            },
            // D65
            {
                129,    // i4X
                -366    // i4Y
            },
            // DF
            {
                0,    // i4X
                0    // i4Y
            }
        },
        // AWB gain of AWB light source
        {
            // Strobe 
            {
                1000,    // i4R
                512,    // i4G
                706    // i4B
            },
            // Horizon 
            {
                512,    // i4R
                539,    // i4G
                1384    // i4B
            },
            // A 
            {
                590,    // i4R
                512,    // i4G
                1148    // i4B
            },
            // TL84 
            {
                756,    // i4R
                512,    // i4G
                932    // i4B
            },
            // CWF 
            {
                842,    // i4R
                512,    // i4G
                968    // i4B
            },
            // DNP 
            {
                832,    // i4R
                512,    // i4G
                844    // i4B
            },
            // D65 
            {
                1000,    // i4R
                512,    // i4G
                706    // i4B
            },
            // DF 
            {
                512,    // i4R
                512,    // i4G
                512    // i4B
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
            -145,    // i4RightBound
            -777,    // i4LeftBound
            -180,    // i4UpperBound
            -389    // i4LowerBound
            },
            // Warm fluorescent
            {
            -145,    // i4RightBound
            -777,    // i4LeftBound
            -389,    // i4UpperBound
            -509    // i4LowerBound
            },
            // Fluorescent
            {
            -50,    // i4RightBound
            -145,    // i4LeftBound
            -287,    // i4UpperBound
            -392    // i4LowerBound
            },
            // CWF
            {
            -50,    // i4RightBound
            -145,    // i4LeftBound
            -392,    // i4UpperBound
            -500    // i4LowerBound
            },
            // Daylight
            {
            154,    // i4RightBound
            -50,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Shade
            {
            514,    // i4RightBound
            154,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Daylight Fluorescent
            {
            154,    // i4RightBound
            -50,    // i4LeftBound
            -460,    // i4UpperBound
            -546    // i4LowerBound
            }
        },
        // PWB light area
        {
            // Reference area
            {
            514,    // i4RightBound
            -777,    // i4LeftBound
            0,    // i4UpperBound
            -546    // i4LowerBound
            },
            // Daylight
            {
            179,    // i4RightBound
            -50,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Cloudy daylight
            {
            279,    // i4RightBound
            104,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Shade
            {
            379,    // i4RightBound
            104,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Twilight
            {
            -50,    // i4RightBound
            -210,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Fluorescent
            {
            179,    // i4RightBound
            -177,    // i4LeftBound
            -315,    // i4UpperBound
            -469    // i4LowerBound
            },
            // Warm fluorescent
            {
            -146,    // i4RightBound
            -346,    // i4LeftBound
            -315,    // i4UpperBound
            -469    // i4LowerBound
            },
            // Incandescent
            {
            -146,    // i4RightBound
            -346,    // i4LeftBound
            -286,    // i4UpperBound
            -460    // i4LowerBound
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
            926,    // i4R
            512,    // i4G
            777    // i4B
            },
            // Cloudy daylight
            {
            1099,    // i4R
            512,    // i4G
            655    // i4B
            },
            // Shade
            {
            1176,    // i4R
            512,    // i4G
            612    // i4B
            },
            // Twilight
            {
            711,    // i4R
            512,    // i4G
            1012    // i4B
            },
            // Fluorescent
            {
            872,    // i4R
            512,    // i4G
            869    // i4B
            },
            // Warm fluorescent
            {
            624,    // i4R
            512,    // i4G
            1214    // i4B
            },
            // Incandescent
            {
            608,    // i4R
            512,    // i4G
            1184    // i4B
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
            6036    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            0,    // i4SliderValue
            4739    // i4OffsetThr
            },
            // Shade
            {
            0,    // i4SliderValue
            1341    // i4OffsetThr
            },
            // Daylight WB gain
            {
            835,    // i4R
            512,    // i4G
            846    // i4B
            },
            // Preference gain: strobe
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: tungsten
            {
            525,    // i4R
            525,    // i4G
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
            520,    // i4G
            522    // i4B
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
                -496,    // i4RotatedXCoordinate[0]
                -375,    // i4RotatedXCoordinate[1]
                -206,    // i4RotatedXCoordinate[2]
                -134,    // i4RotatedXCoordinate[3]
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
}}; // NSFeature


