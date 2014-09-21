#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_ov5648mipiraw.h"
#include "camera_info_ov5648mipiraw.h"
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
        63275,    // i4R_AVG
        13505,    // i4R_STD
        83350,    // i4B_AVG
        22041,    // i4B_STD
        {  // i4P00[9]
            5242500, -1980000, -695000, -800000, 3527500, -162500, 32500, -1957500, 4477500
        },
        {  // i4P10[9]
            1129477, -1242550, 113202, -106725, -21168, 136300, 27805, 231084, -263248
        },
        {  // i4P01[9]
            568480, -750325, 194321, -149205, -103871, 256591, 60081, -375479, 303364
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
            51,    // u4MiniISOGain, ISOxx  
            64,    // u4GainStepUnit, 1x/8 
            35,    // u4PreExpUnit 
            30,    // u4PreMaxFrameRate
            35,    // u4VideoExpUnit  
            30,    // u4VideoMaxFrameRate 
            1024,    // u4Video2PreRatio, 1024 base = 1x 
            35,    // u4CapExpUnit 
            15,    // u4CapMaxFrameRate
            1024,    // u4Cap2PreRatio, 1024 base = 1x
            24,    // u4LensFno, Fno = 2.8
            339    // u4FocusLength_100x
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
            {86, 108, 128, 148, 170},    // u4HistStretchThres[AE_CCT_STRENGTH_NUM] 
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
            -16,    // i4BVOffset delta BV = value/10 
            4,    // u4PreviewFlareOffset
            4,    // u4CaptureFlareOffset
            5,    // u4CaptureFlareThres
            4,    // u4VideoFlareOffset
            5,    // u4VideoFlareThres
            2,    // u4StrobeFlareOffset
            2,    // u4StrobeFlareThres
            8,    // u4PrvMaxFlareThres
            0,    // u4PrvMinFlareThres
            8,    // u4VideoMaxFlareThres
            0,    // u4VideoMinFlareThres
            18,    // u4FlatnessThres    // 10 base for flatness condition.
            57    // u4FlatnessStrength
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
                729,    // i4R
                512,    // i4G
                580    // i4B
            }
        },
        // Original XY coordinate of AWB light source
        {
           // Strobe
            {
                0,    // i4X
                0    // i4Y
            },
            // Horizon
            {
                -465,    // i4X
                -235    // i4Y
            },
            // A
            {
                -345,    // i4X
                -239    // i4Y
            },
            // TL84
            {
                -170,    // i4X
                -238    // i4Y
            },
            // CWF
            {
                -103,    // i4X
                -333    // i4Y
            },
            // DNP
            {
                -56,    // i4X
                -191    // i4Y
            },
            // D65
            {
                85,    // i4X
                -176    // i4Y
            },
            // DF
            {
                -17,    // i4X
                -251    // i4Y
            }
        },
        // Rotated XY coordinate of AWB light source
        {
            // Strobe
            {
                0,    // i4X
                0    // i4Y
            },
            // Horizon
            {
                -494,    // i4X
                -168    // i4Y
            },
            // A
            {
                -376,    // i4X
                -189    // i4Y
            },
            // TL84
            {
                -202,    // i4X
                -212    // i4Y
            },
            // CWF
            {
                -149,    // i4X
                -316    // i4Y
            },
            // DNP
            {
                -82,    // i4X
                -182    // i4Y
            },
            // D65
            {
                60,    // i4X
                -187    // i4Y
            },
            // DF
            {
                -52,    // i4X
                -247    // i4Y
            }
        },
        // AWB gain of AWB light source
        {
            // Strobe 
            {
                512,    // i4R
                512,    // i4G
                512    // i4B
            },
            // Horizon 
            {
                512,    // i4R
                699,    // i4G
                1804    // i4B
            },
            // A 
            {
                512,    // i4R
                591,    // i4G
                1305    // i4B
            },
            // TL84 
            {
                561,    // i4R
                512,    // i4G
                890    // i4B
            },
            // CWF 
            {
                700,    // i4R
                512,    // i4G
                924    // i4B
            },
            // DNP 
            {
                615,    // i4R
                512,    // i4G
                716    // i4B
            },
            // D65 
            {
                729,    // i4R
                512,    // i4G
                580    // i4B
            },
            // DF 
            {
                702,    // i4R
                512,    // i4G
                736    // i4B
            }
        },
        // Rotation matrix parameter
        {
            8,    // i4RotationAngle
            254,    // i4Cos
            36    // i4Sin
        },
        // Daylight locus parameter
        {
            -167,    // i4SlopeNumerator
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
            -252,    // i4RightBound
            -902,    // i4LeftBound
            -128,    // i4UpperBound
            -228    // i4LowerBound
            },
            // Warm fluorescent
            {
            -252,    // i4RightBound
            -902,    // i4LeftBound
            -228,    // i4UpperBound
            -348    // i4LowerBound
            },
            // Fluorescent
            {
            -92,    // i4RightBound
            -252,    // i4LeftBound
            -117,    // i4UpperBound
            -264    // i4LowerBound
            },
            // CWF
            {
            -92,    // i4RightBound
            -252,    // i4LeftBound
            -264,    // i4UpperBound
            -366    // i4LowerBound
            },
            // Daylight
            {
            115,    // i4RightBound
            -92,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
            },
            // Shade
            {
            445,    // i4RightBound
            115,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
            },
            // Daylight Fluorescent
            {
            115,    // i4RightBound
            -92,    // i4LeftBound
            -267,    // i4UpperBound
            -366    // i4LowerBound
            }
        },
        // PWB light area
        {
            // Reference area
            {
            445,    // i4RightBound
            -902,    // i4LeftBound
            0,    // i4UpperBound
            -366    // i4LowerBound
            },
            // Daylight
            {
            140,    // i4RightBound
            -92,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
            },
            // Cloudy daylight
            {
            240,    // i4RightBound
            65,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
            },
            // Shade
            {
            340,    // i4RightBound
            65,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
            },
            // Twilight
            {
            -92,    // i4RightBound
            -252,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
            },
            // Fluorescent
            {
            110,    // i4RightBound
            -302,    // i4LeftBound
            -137,    // i4UpperBound
            -366    // i4LowerBound
            },
            // Warm fluorescent
            {
            -276,    // i4RightBound
            -476,    // i4LeftBound
            -137,    // i4UpperBound
            -366    // i4LowerBound
            },
            // Incandescent
            {
            -276,    // i4RightBound
            -476,    // i4LeftBound
            -107,    // i4UpperBound
            -267    // i4LowerBound
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
            700,    // i4R
            512,    // i4G
            612    // i4B
            },
            // Cloudy daylight
            {
            812,    // i4R
            512,    // i4G
            503    // i4B
            },
            // Shade
            {
            860,    // i4R
            512,    // i4G
            466    // i4B
            },
            // Twilight
            {
            559,    // i4R
            512,    // i4G
            825    // i4B
            },
            // Fluorescent
            {
            673,    // i4R
            512,    // i4G
            791    // i4B
            },
            // Warm fluorescent
            {
            488,    // i4R
            512,    // i4G
            1214    // i4B
            },
            // Incandescent
            {
            442,    // i4R
            512,    // i4G
            1127    // i4B
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
            7131    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            0,    // i4SliderValue
            5290    // i4OffsetThr
            },
            // Shade
            {
            0,    // i4SliderValue
            1756    // i4OffsetThr
            },
            // Daylight WB gain
            {
            620,    // i4R
            512,    // i4G
            719    // i4B
            },
            // Preference gain: strobe
            {
            512,    // i4R
            512,    // i4G
            512    // i4B
            },
            // Preference gain: tungsten
            {
            500,    // i4R
            512,    // i4G
            520    // i4B
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
                -554,    // i4RotatedXCoordinate[0]
                -436,    // i4RotatedXCoordinate[1]
                -262,    // i4RotatedXCoordinate[2]
                -142,    // i4RotatedXCoordinate[3]
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


