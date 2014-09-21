#include <utils/Log.h>
#include <fcntl.h>
#include <math.h>

#include "camera_custom_nvram.h"
#include "camera_custom_sensor.h"
#include "image_sensor.h"
#include "kd_imgsensor_define.h"
#include "camera_AE_PLineTable_ov8835mipiraw.h"
#include "camera_info_ov8835mipiraw.h"
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
        77225,    // i4R_AVG
        19806,    // i4R_STD
        117325,    // i4B_AVG
        30738,    // i4B_STD
        {  // i4P00[9]
            5607500, -2997500, -50000, -802500, 3865000, -502500, -112500, -2167500, 4845000
        },
        {  // i4P10[9]
            1108520, -1388364, 279844, -27029, -77362, 99012, -119193, -261315, 385814
        },
        {  // i4P01[9]
            788467, -1085442, 296974, -168095, -215900, 381771, -178895, -898810, 1081765
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
            1640,    // u4MinGain, 1024 base = 1x
            15872,    // u4MaxGain, 16x
            118,    // u4MiniISOGain, ISOxx  
            128,    // u4GainStepUnit, 1x/8 
            17346,    // u4PreExpUnit 
            30,    // u4PreMaxFrameRate
            13876,    // u4VideoExpUnit  
            30,    // u4VideoMaxFrameRate 
            1024,    // u4Video2PreRatio, 1024 base = 1x 
            13876,    // u4CapExpUnit 
            28,    // u4CapMaxFrameRate
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
            {75, 108, 128, 148, 170},    // u4HistStretchThres[AE_CCT_STRENGTH_NUM] 
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
            0,    // u4StrobeAETarget
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
            -8,    // i4BVOffset delta BV = value/10 
            64,    // u4PreviewFlareOffset
            64,    // u4CaptureFlareOffset
            3,    // u4CaptureFlareThres
            64,    // u4VideoFlareOffset
            3,    // u4VideoFlareThres
            64,    // u4StrobeFlareOffset
            3,    // u4StrobeFlareThres
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
                810,    // i4R
                512,    // i4G
                669    // i4B
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
                -409,    // i4X
                -291    // i4Y
            },
            // A
            {
                -291,    // i4X
                -307    // i4Y
            },
            // TL84
            {
                -191,    // i4X
                -299    // i4Y
            },
            // CWF
            {
                -135,    // i4X
                -360    // i4Y
            },
            // DNP
            {
                -51,    // i4X
                -314    // i4Y
            },
            // D65
            {
                70,    // i4X
                -268    // i4Y
            },
            // DF
            {
                9,    // i4X
                -303    // i4Y
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
                -428,    // i4X
                -261    // i4Y
            },
            // A
            {
                -311,    // i4X
                -285    // i4Y
            },
            // TL84
            {
                -211,    // i4X
                -284    // i4Y
            },
            // CWF
            {
                -160,    // i4X
                -349    // i4Y
            },
            // DNP
            {
                -73,    // i4X
                -309    // i4Y
            },
            // D65
            {
                51,    // i4X
                -272    // i4Y
            },
            // DF
            {
                -12,    // i4X
                -302    // i4Y
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
                601,    // i4G
                1549    // i4B
            },
            // A 
            {
                524,    // i4R
                512,    // i4G
                1150    // i4B
            },
            // TL84 
            {
                593,    // i4R
                512,    // i4G
                993    // i4B
            },
            // CWF 
            {
                695,    // i4R
                512,    // i4G
                1000    // i4B
            },
            // DNP 
            {
                731,    // i4R
                512,    // i4G
                839    // i4B
            },
            // D65 
            {
                810,    // i4R
                512,    // i4G
                669    // i4B
            },
            // DF 
            {
                781,    // i4R
                512,    // i4G
                762    // i4B
            }
        },
        // Rotation matrix parameter
        {
            4,    // i4RotationAngle
            255,    // i4Cos
            18    // i4Sin
        },
        // Daylight locus parameter
        {
            -149,    // i4SlopeNumerator
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
            -261,    // i4RightBound
            -911,    // i4LeftBound
            -223,    // i4UpperBound
            -323    // i4LowerBound
            },
            // Warm fluorescent
            {
            -261,    // i4RightBound
            -911,    // i4LeftBound
            -323,    // i4UpperBound
            -443    // i4LowerBound
            },
            // Fluorescent
            {
            -123,    // i4RightBound
            -261,    // i4LeftBound
            -207,    // i4UpperBound
            -316    // i4LowerBound
            },
            // CWF
            {
            -123,    // i4RightBound
            -261,    // i4LeftBound
            -316,    // i4UpperBound
            -399    // i4LowerBound
            },
            // Daylight
            {
            76,    // i4RightBound
            -123,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
            },
            // Shade
            {
            436,    // i4RightBound
            76,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
            },
            // Daylight Fluorescent
            {
            76,    // i4RightBound
            -123,    // i4LeftBound
            -399,    // i4UpperBound
            -352    // i4LowerBound
            }
        },
        // PWB light area
        {
            // Reference area
            {
            436,    // i4RightBound
            -911,    // i4LeftBound
            0,    // i4UpperBound
            -443    // i4LowerBound
            },
            // Daylight
            {
            101,    // i4RightBound
            -123,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
            },
            // Cloudy daylight
            {
            201,    // i4RightBound
            26,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
            },
            // Shade
            {
            301,    // i4RightBound
            26,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
            },
            // Twilight
            {
            -123,    // i4RightBound
            -283,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
            },
            // Fluorescent
            {
            101,    // i4RightBound
            -311,    // i4LeftBound
            -222,    // i4UpperBound
            -399    // i4LowerBound
            },
            // Warm fluorescent
            {
            -211,    // i4RightBound
            -411,    // i4LeftBound
            -222,    // i4UpperBound
            -399    // i4LowerBound
            },
            // Incandescent
            {
            -211,    // i4RightBound
            -411,    // i4LeftBound
            -192,    // i4UpperBound
            -352    // i4LowerBound
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
            749,    // i4R
            512,    // i4G
            732    // i4B
            },
            // Cloudy daylight
            {
            876,    // i4R
            512,    // i4G
            611    // i4B
            },
            // Shade
            {
            932,    // i4R
            512,    // i4G
            569    // i4B
            },
            // Twilight
            {
            588,    // i4R
            512,    // i4G
            967    // i4B
            },
            // Fluorescent
            {
            703,    // i4R
            512,    // i4G
            881    // i4B
            },
            // Warm fluorescent
            {
            543,    // i4R
            512,    // i4G
            1187    // i4B
            },
            // Incandescent
            {
            513,    // i4R
            512,    // i4G
            1131    // i4B
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
            5402    // i4OffsetThr
            },
            // Warm fluorescent	
            {
            0,    // i4SliderValue
            5263    // i4OffsetThr
            },
            // Shade
            {
            0,    // i4SliderValue
            1342    // i4OffsetThr
            },
            // Daylight WB gain
            {
            693,    // i4R
            512,    // i4G
            801    // i4B
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
                -479,    // i4RotatedXCoordinate[0]
                -362,    // i4RotatedXCoordinate[1]
                -262,    // i4RotatedXCoordinate[2]
                -124,    // i4RotatedXCoordinate[3]
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
}}; // NSFeature


