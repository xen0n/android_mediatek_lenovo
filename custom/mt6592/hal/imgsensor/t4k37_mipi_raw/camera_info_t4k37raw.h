#ifndef _CAMERA_INFO_T4K37RAW_H
#define _CAMERA_INFO_T4K37RAW_H

/*******************************************************************************
*   Configuration
********************************************************************************/
#define SENSOR_ID                           T4K37_SENSOR_ID
#define SENSOR_DRVNAME                      SENSOR_DRVNAME_T4K37_MIPI_RAW
#define INCLUDE_FILENAME_ISP_REGS_PARAM     "camera_isp_regs_t4k37raw.h"
#define INCLUDE_FILENAME_ISP_PCA_PARAM      "camera_isp_pca_t4k37raw.h"
#define INCLUDE_FILENAME_ISP_LSC_PARAM      "camera_isp_lsc_t4k37raw.h"
#define INCLUDE_FILENAME_TSF_PARA           "camera_tsf_para_t4k37raw.h"
#define INCLUDE_FILENAME_TSF_DATA           "camera_tsf_data_t4k37raw.h"
/*******************************************************************************
*
********************************************************************************/

#if defined(ISP_SUPPORT)

#define T4K37RAW_CAMERA_AUTO_DSC CAM_AUTO_DSC
#define T4K37RAW_CAMERA_PORTRAIT CAM_PORTRAIT
#define T4K37RAW_CAMERA_LANDSCAPE CAM_LANDSCAPE
#define T4K37RAW_CAMERA_SPORT CAM_SPORT
#define T4K37RAW_CAMERA_FLOWER CAM_FLOWER
#define T4K37RAW_CAMERA_NIGHTSCENE CAM_NIGHTSCENE
#define T4K37RAW_CAMERA_DOCUMENT CAM_DOCUMENT
#define T4K37RAW_CAMERA_ISO_ANTI_HAND_SHAKE CAM_ISO_ANTI_HAND_SHAKE
#define T4K37RAW_CAMERA_ISO100 CAM_ISO100
#define T4K37RAW_CAMERA_ISO200 CAM_ISO200
#define T4K37RAW_CAMERA_ISO400 CAM_ISO400
#define T4K37RAW_CAMERA_ISO800 CAM_ISO800
#define T4K37RAW_CAMERA_ISO1600 CAM_ISO1600
#define T4K37RAW_CAMERA_VIDEO_AUTO CAM_VIDEO_AUTO
#define T4K37RAW_CAMERA_VIDEO_NIGHT CAM_VIDEO_NIGHT
#define T4K37RAW_CAMERA_NO_OF_SCENE_MODE CAM_NO_OF_SCENE_MODE

#endif
#endif
