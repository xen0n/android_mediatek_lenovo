#ifndef _CAMERA_CUSTOM_EXIF_
#define _CAMERA_CUSTOM_EXIF_
//
#include "camera_custom_types.h"

//
namespace NSCamCustom
{
/*******************************************************************************
* Custom EXIF: Imgsensor-related.
*******************************************************************************/
typedef struct SensorExifInfo_S
{
    MUINT32 uFLengthNum;
    MUINT32 uFLengthDenom;
    
} SensorExifInfo_T;

SensorExifInfo_T const&
getParamSensorExif()
{
    static SensorExifInfo_T inst = { 
        uFLengthNum     : 35, // Numerator of Focal Length. Default is 35.
        uFLengthDenom   : 10, // Denominator of Focal Length, it should not be 0.  Default is 10.
    };
    return inst;
}


/*******************************************************************************
* Custom EXIF
******************************************************************************/
/*Lenovo-sw sunliang add for exif info 2014_3_14 begin*/
#define EN_CUSTOM_EXIF_INFO 
/*Lenovo-sw sunliang add for exif info 2014_3_14 end*/
#define SET_EXIF_TAG_STRING(tag,str) \
    if (strlen((const char*)str) <= 32) { \
        strcpy((char *)pexifApp1Info->tag, (const char*)str); }
        
typedef struct customExifInfo_s {
    unsigned char strMake[PROPERTY_VALUE_MAX];
    unsigned char strModel[PROPERTY_VALUE_MAX];
    unsigned char strSoftware[PROPERTY_VALUE_MAX];
} customExifInfo_t;

MINT32 custom_SetExif(void **ppCustomExifTag)
{
#ifdef EN_CUSTOM_EXIF_INFO
#define CUSTOM_EXIF_STRING_MAKE  "custom make"
#define CUSTOM_EXIF_STRING_MODEL "custom model"
#define CUSTOM_EXIF_STRING_SOFTWARE "custom software"
static customExifInfo_t exifTag = {CUSTOM_EXIF_STRING_MAKE,CUSTOM_EXIF_STRING_MODEL,CUSTOM_EXIF_STRING_SOFTWARE};
  /*Lenovo-sw sunliang add for exif info 2014_3_14 begin*/
  property_get("ro.product.brand", (char *)exifTag.strMake, "Lenovo");
  property_get("ro.product.model", (char *)exifTag.strModel, "Phone");
  property_get("ro.build.product", (char *)exifTag.strSoftware, "Lenovo");
   /*Lenovo-sw sunliang add for exif info 2014_3_14 end*/
	if (0 != ppCustomExifTag) {
        *ppCustomExifTag = (void*)&exifTag;
    }
    return 0;
#else
    return -1;
#endif
}


/*******************************************************************************
* Custom EXIF: Exposure Program
******************************************************************************/
typedef struct customExif_s
{
    MBOOL   bEnCustom;
    MUINT32 u4ExpProgram;
    
} customExif_t;

customExif_t const&
getCustomExif()
{
    static customExif_t inst = {
        bEnCustom       :   false,  // default value: false.
        u4ExpProgram    :   0,      // default value: 0.    '0' means not defined, '1' manual control, '2' program normal
    };
    return inst;
}


};  //NSCamCustom
#endif  //  _CAMERA_CUSTOM_EXIF_

