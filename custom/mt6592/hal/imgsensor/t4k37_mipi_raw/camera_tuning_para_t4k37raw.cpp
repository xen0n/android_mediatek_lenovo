#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_t4k37raw.h"
#include "camera_info_t4k37raw.h"
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
        63475,    // i4R_AVG
        13025,    // i4R_STD
        97850,    // i4B_AVG
        18801,    // i4B_STD
        {  // i4P00[9]
            5547500, -2632500, -337500, -817500, 4237500, -852500, -120000, -2415000, 5097500
        },
        {  // i4P10[9]
            1067140, -1240393, 172868, -106309, 29168, 69389, -48355, 31140, -3342
        },
        {  // i4P01[9]
            761853, -762091, -3306, -215290, 18439, 188673, -107154, -568678, 654629
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
            1136,    // u4MinGain, 1024 base = 1x
            10240,    // u4MaxGain, 16x
            72,    // u4MiniISOGain, ISOxx  
            256,    // u4GainStepUnit, 1x/8 
            19,    // u4PreExpUnit 
            30,    // u4PreMaxFrameRate
            19,    // u4VideoExpUnit  
            30,    // u4VideoMaxFrameRate 
            1024,    // u4Video2PreRatio, 1024 base = 1x 
            13,    // u4CapExpUnit 
            23,    // u4CapMaxFrameRate
            512,    // u4Cap2PreRatio, 1024 base = 1x
            22,    // u4LensFno, Fno = 2.8
            350    // u4FocusLength_100x
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
            FALSE,    // bEnableAntiOverExposure
            TRUE,    // bEnableTimeLPF
            FALSE,    // bEnableCaptureThres
            FALSE,    // bEnableVideoThres
            FALSE,    // bEnableStrobeThres
            47,    // u4AETarget
            0,    // u4StrobeAETarget
            50,    // u4InitIndex
            4,    // u4BackLightWeight
            32,    // u4HistStretchWeight
            4,    // u4AntiOverExpWeight
            2,    // u4BlackLightStrengthIndex
            0,    // u4HistStretchStrengthIndex
            2,    // u4AntiOverExpStrengthIndex
            2,    // u4TimeLPFStrengthIndex
            {1, 3, 5, 7, 8},    // u4LPFConvergeTable[AE_CCT_STRENGTH_NUM] 
            90,    // u4InDoorEV = 9.0, 10 base 
            -7,    // i4BVOffset delta BV = value/10 
            64,    // u4PreviewFlareOffset
            64,    // u4CaptureFlareOffset
            4,    // u4CaptureFlareThres
            64,    // u4VideoFlareOffset
            4,    // u4VideoFlareThres
            32,    // u4StrobeFlareOffset
            2,    // u4StrobeFlareThres
            160,    // u4PrvMaxFlareThres
            0,    // u4PrvMinFlareThres
            160,    // u4VideoMaxFlareThres
            0,    // u4VideoMinFlareThres
            18,    // u4FlatnessThres    // 10 base for flatness condition.
            75    // u4FlatnessStrength
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
                991,    // i4R
                666,    // i4G
                512    // i4B
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
                244,    // i4X
                -50    // i4Y
            },
            // Horizon
            {
                -196,    // i4X
                -49    // i4Y
            },
            // A
            {
                -196,    // i4X
                -49    // i4Y
            },
            // TL84
            {
                -1,    // i4X
                -99    // i4Y
            },
            // CWF
            {
                16,    // i4X
                -178    // i4Y
            },
            // DNP
            {
                244,    // i4X
                -50    // i4Y
            },
            // D65
            {
                244,    // i4X
                -50    // i4Y
            },
            // DF
            {
                244,    // i4X
                -50    // i4Y
            }
        },
        // Rotated XY coordinate of AWB light source
        {
            // Strobe
            {
                244,    // i4X
                -50    // i4Y
            },
            // Horizon
            {
                -196,    // i4X
                -49    // i4Y
            },
            // A
            {
                -196,    // i4X
                -49    // i4Y
            },
            // TL84
            {
                -1,    // i4X
                -99    // i4Y
            },
            // CWF
            {
                16,    // i4X
                -178    // i4Y
            },
            // DNP
            {
                244,    // i4X
                -50    // i4Y
            },
            // D65
            {
                244,    // i4X
                -50    // i4Y
            },
            // DF
            {
                244,    // i4X
                -50    // i4Y
            }
        },
        // AWB gain of AWB light source
        {
            // Strobe 
            {
                991,    // i4R
                666,    // i4G
                512    // i4B
            },
            // Horizon 
            {
                512,    // i4R
                625,    // i4G
                871    // i4B
            },
            // A 
            {
                512,    // i4R
                625,    // i4G
                871    // i4B
            },
            // TL84 
            {
                585,    // i4R
                512,    // i4G
                587    // i4B
            },
            // CWF 
            {
                666,    // i4R
                512,    // i4G
                638    // i4B
            },
            // DNP 
            {
                991,    // i4R
                666,    // i4G
                512    // i4B
            },
            // D65 
            {
                991,    // i4R
                666,    // i4G
                512    // i4B
            },
            // DF 
            {
                991,    // i4R
                666,    // i4G
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
            -134,    // i4SlopeNumerator
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
            -51,    // i4RightBound
            -701,    // i4LeftBound
            1,    // i4UpperBound
            -99    // i4LowerBound
            },
            // Warm fluorescent
            {
            -51,    // i4RightBound
            -701,    // i4LeftBound
            -99,    // i4UpperBound
            -219    // i4LowerBound
            },
            // Fluorescent
            {
            194,    // i4RightBound
            -51,    // i4LeftBound
            16,    // i4UpperBound
            -138    // i4LowerBound
            },
            // CWF
            {
            194,    // i4RightBound
            -51,    // i4LeftBound
            -138,    // i4UpperBound
            -228    // i4LowerBound
            },
            // Daylight
            {
            269,    // i4RightBound
            194,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
            },
            // Shade
            {
            629,    // i4RightBound
            269,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
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
            629,    // i4RightBound
            -701,    // i4LeftBound
            55,    // i4UpperBound
            -228    // i4LowerBound
            },
            // Daylight
            {
            294,    // i4RightBound
            194,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
            },
            // Cloudy daylight
            {
            394,    // i4RightBound
            219,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
            },
            // Shade
            {
            494,    // i4RightBound
            219,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
            },
            // Twilight
            {
            194,    // i4RightBound
            34,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
            },
            // Fluorescent
            {
            294,    // i4RightBound
            -101,    // i4LeftBound
            0,    // i4UpperBound
            -228    // i4LowerBound
            },
            // Warm fluorescent
            {
            -96,    // i4RightBound
            -296,    // i4LeftBound
            0,    // i4UpperBound
            -228    // i4LowerBound
            },
            // Incandescent
            {
            -96,    // i4RightBound
            -296,    // i4LeftBound
            30,    // i4UpperBound
            -130    // i4LowerBound
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
            762,    // i4R
            512,    // i4G
            394    // i4B
            },
            // Cloudy daylight
            {
            830,    // i4R
            512,    // i4G
            362    // i4B
            },
            // Shade
            {
            888,    // i4R
            512,    // i4G
            338    // i4B
            },
            // Twilight
            {
            639,    // i4R
            512,    // i4G
            470    // i4B
            },
            // Fluorescent
            {
            681,    // i4R
            512,    // i4G
            524    // i4B
            },
            // Warm fluorescent
            {
            458,    // i4R
            512,    // i4G
            779    // i4B
            },
            // Incandescent
            {
            420,    // i4R
            512,    // i4G
            714    // i4B
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
            50,    // i4SliderValue
            4020    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            50,    // i4SliderValue
            4020    // i4OffsetThr
            },
            // Shade
            {
            50,    // i4SliderValue
            341    // i4OffsetThr
            },
            // Daylight WB gain
            {
            991,    // i4R
            666,    // i4G
            512    // i4B
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
                -440,    // i4RotatedXCoordinate[0]
                -440,    // i4RotatedXCoordinate[1]
                -245,    // i4RotatedXCoordinate[2]
                0,    // i4RotatedXCoordinate[3]
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


