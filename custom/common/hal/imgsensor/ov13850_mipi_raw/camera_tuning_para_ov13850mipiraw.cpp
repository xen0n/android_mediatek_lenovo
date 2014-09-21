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
        78625,    // i4R_AVG
        16326,    // i4R_STD
        92050,    // i4B_AVG
        17486,    // i4B_STD
        {  // i4P00[9]
            5415000, -2715000, -140000, -842500, 3955000, -550000, -200000, -2582500, 5347500
        },
        {  // i4P10[9]
            2282244, -2692191, 409946, -149399, -332132, 499199, 212878, 413842, -591383
        },
        {  // i4P01[9]
            1394874, -1672148, 277274, -276385, -456691, 750551, 15805, -166905, 186050
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
            7936,    // u4MaxGain, 16x
            53,    // u4MiniISOGain, ISOxx  
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
            TRUE,    // bEnableCaptureThres
            TRUE,    // bEnableVideoThres
            TRUE,    // bEnableStrobeThres
            47,    // u4AETarget
            47,    // u4StrobeAETarget
            50,    // u4InitIndex
            4,    // u4BackLightWeight
            32,    // u4HistStretchWeight
            4,    // u4AntiOverExpWeight
            2,    // u4BlackLightStrengthIndex
            2,    // u4HistStretchStrengthIndex
            2,    // u4AntiOverExpStrengthIndex
            2,    // u4TimeLPFStrengthIndex
            {1, 3, 5, 7, 8},    // u4LPFConvergeTable[AE_CCT_STRENGTH_NUM] 
            90,    // u4InDoorEV = 9.0, 10 base 
            -6,    // i4BVOffset delta BV = value/10 
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
                995,    // i4R
                512,    // i4G
                706    // i4B
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
                126,    // i4X
                -364    // i4Y
            },
            // Horizon
            {
                -364,    // i4X
                -324    // i4Y
            },
            // A
            {
                -247,    // i4X
                -350    // i4Y
            },
            // TL84
            {
                -72,    // i4X
                -362    // i4Y
            },
            // CWF
            {
                -46,    // i4X
                -410    // i4Y
            },
            // DNP
            {
                -7,    // i4X
                -351    // i4Y
            },
            // D65
            {
                126,    // i4X
                -364    // i4Y
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
                126,    // i4X
                -364    // i4Y
            },
            // Horizon
            {
                -364,    // i4X
                -324    // i4Y
            },
            // A
            {
                -247,    // i4X
                -350    // i4Y
            },
            // TL84
            {
                -72,    // i4X
                -362    // i4Y
            },
            // CWF
            {
                -46,    // i4X
                -410    // i4Y
            },
            // DNP
            {
                -7,    // i4X
                -351    // i4Y
            },
            // D65
            {
                126,    // i4X
                -364    // i4Y
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
                995,    // i4R
                512,    // i4G
                706    // i4B
            },
            // Horizon 
            {
                512,    // i4R
                540,    // i4G
                1370    // i4B
            },
            // A 
            {
                588,    // i4R
                512,    // i4G
                1149    // i4B
            },
            // TL84 
            {
                759,    // i4R
                512,    // i4G
                921    // i4B
            },
            // CWF 
            {
                837,    // i4R
                512,    // i4G
                950    // i4B
            },
            // DNP 
            {
                816,    // i4R
                512,    // i4G
                831    // i4B
            },
            // D65 
            {
                995,    // i4R
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
            -113,    // i4SlopeNumerator
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
            -122,    // i4RightBound
            -772,    // i4LeftBound
            -200,    // i4UpperBound
            -387    // i4LowerBound
            },
            // Warm fluorescent
            {
            -122,    // i4RightBound
            -772,    // i4LeftBound
            -387,    // i4UpperBound
            -507    // i4LowerBound
            },
            // Fluorescent
            {
            -40,    // i4RightBound
            -122,    // i4LeftBound
            -285,    // i4UpperBound
            -386    // i4LowerBound
            },
            // CWF
            {
            -40,    // i4RightBound
            -122,    // i4LeftBound
            -386,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Daylight
            {
            151,    // i4RightBound
            -40,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
            },
            // Shade
            {
            511,    // i4RightBound
            151,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
            },
            // Daylight Fluorescent
            {
            151,    // i4RightBound
            -40,    // i4LeftBound
            -444,    // i4UpperBound
            -544    // i4LowerBound
            }
        },
        // PWB light area
        {
            // Reference area
            {
            511,    // i4RightBound
            -772,    // i4LeftBound
            0,    // i4UpperBound
            -544    // i4LowerBound
            },
            // Daylight
            {
            176,    // i4RightBound
            -40,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
            },
            // Cloudy daylight
            {
            276,    // i4RightBound
            101,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
            },
            // Shade
            {
            376,    // i4RightBound
            101,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
            },
            // Twilight
            {
            -40,    // i4RightBound
            -200,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
            },
            // Fluorescent
            {
            176,    // i4RightBound
            -172,    // i4LeftBound
            -312,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Warm fluorescent
            {
            -147,    // i4RightBound
            -347,    // i4LeftBound
            -312,    // i4UpperBound
            -460    // i4LowerBound
            },
            // Incandescent
            {
            -147,    // i4RightBound
            -347,    // i4LeftBound
            -284,    // i4UpperBound
            -444    // i4LowerBound
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
            919,    // i4R
            512,    // i4G
            764    // i4B
            },
            // Cloudy daylight
            {
            1082,    // i4R
            512,    // i4G
            649    // i4B
            },
            // Shade
            {
            1157,    // i4R
            512,    // i4G
            607    // i4B
            },
            // Twilight
            {
            712,    // i4R
            512,    // i4G
            986    // i4B
            },
            // Fluorescent
            {
            866,    // i4R
            512,    // i4G
            861    // i4B
            },
            // Warm fluorescent
            {
            618,    // i4R
            512,    // i4G
            1206    // i4B
            },
            // Incandescent
            {
            600,    // i4R
            512,    // i4G
            1171    // i4B
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
            6228    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            0,    // i4SliderValue
            4385    // i4OffsetThr
            },
            // Shade
            {
            0,    // i4SliderValue
            1341    // i4OffsetThr
            },
            // Daylight WB gain
            {
            830,    // i4R
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
                -490,    // i4RotatedXCoordinate[0]
                -373,    // i4RotatedXCoordinate[1]
                -198,    // i4RotatedXCoordinate[2]
                -133,    // i4RotatedXCoordinate[3]
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


