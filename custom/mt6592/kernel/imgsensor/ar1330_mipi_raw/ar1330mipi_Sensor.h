/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ar1330mipi_Sensor.h
 *
 * Project:
 * --------
 *   YUSU
 *
 * Description:
 * ------------
 *   Header file of Sensor driver
 *
 *
 * Author:
 * -------
 *   Guangye Yang (mtk70662)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#ifndef _AR1330MIPI_SENSOR_H
#define _AR1330MIPI_SENSOR_H

typedef enum group_enum {
    PRE_GAIN=0,
    CMMCLK_CURRENT,
    FRAME_RATE_LIMITATION,
    REGISTER_EDITOR,
    GROUP_TOTAL_NUMS
} FACTORY_GROUP_ENUM;


#define ENGINEER_START_ADDR 10
#define FACTORY_START_ADDR 0


typedef enum register_index
{
    SENSOR_BASEGAIN=FACTORY_START_ADDR,
    PRE_GAIN_R_INDEX,
    PRE_GAIN_Gr_INDEX,
    PRE_GAIN_Gb_INDEX,
    PRE_GAIN_B_INDEX,
    FACTORY_END_ADDR
} FACTORY_REGISTER_INDEX;


typedef enum engineer_index
{
    CMMCLK_CURRENT_INDEX=ENGINEER_START_ADDR,
    ENGINEER_END
} FACTORY_ENGINEER_INDEX;


typedef struct
{
    SENSOR_REG_STRUCT   Reg[ENGINEER_END];
    SENSOR_REG_STRUCT   CCT[FACTORY_END_ADDR];
} SENSOR_DATA_STRUCT, *PSENSOR_DATA_STRUCT;


#define AR1330MIPI_DATA_FORMAT SENSOR_OUTPUT_FORMAT_RAW_Gr


#define AR1330MIPI_IMAGE_SENSOR_FULL_HACTIVE     (4208)
#define AR1330MIPI_IMAGE_SENSOR_FULL_VACTIVE     (3120)
#define AR1330MIPI_IMAGE_SENSOR_PV_HACTIVE       (2104)
#define AR1330MIPI_IMAGE_SENSOR_PV_VACTIVE       (1560)


#define AR1330MIPI_FULL_START_X                  (0)
#define AR1330MIPI_FULL_START_Y                  (0)
#define AR1330MIPI_IMAGE_SENSOR_FULL_WIDTH       (AR1330MIPI_IMAGE_SENSOR_FULL_HACTIVE)  //4208-8 4200
#define AR1330MIPI_IMAGE_SENSOR_FULL_HEIGHT      (AR1330MIPI_IMAGE_SENSOR_FULL_VACTIVE)  //3120 -6 3114

#define AR1330MIPI_PV_START_X                    (0)
#define AR1330MIPI_PV_START_Y                    (0)
#define AR1330MIPI_IMAGE_SENSOR_PV_WIDTH         (AR1330MIPI_IMAGE_SENSOR_PV_HACTIVE)    //2104 -16 1280
#define AR1330MIPI_IMAGE_SENSOR_PV_HEIGHT        (AR1330MIPI_IMAGE_SENSOR_PV_VACTIVE)    //1560 -12 960


#define AR1330MIPI_FULL_PERIOD_PIXEL_NUMS        (5128)
#define AR1330MIPI_FULL_PERIOD_LINE_NUMS         (3211)
#define AR1330MIPI_PV_PERIOD_PIXEL_NUMS          (4488)
#define AR1330MIPI_PV_PERIOD_LINE_NUMS           (1652)


#define AR1330MIPI_WRITE_ID_1    0x6C
#define AR1330MIPI_READ_ID_1     0x6D
#define AR1330MIPI_WRITE_ID_2    0x6E
#define AR1330MIPI_READ_ID_2     0x6F

typedef enum
{
    AR1330MIPI_MODE_INIT,
    AR1330MIPI_MODE_PREVIEW,
    AR1330MIPI_MODE_VIDEO,
    AR1330MIPI_MODE_CAPTURE
} AR1330MIPI_MODE;


/* SENSOR PRIVATE STRUCT */
struct AR1330MIPI_SENSOR_STRUCT
{
    kal_uint16 preview_vt_clk;
    kal_uint16 capture_vt_clk;
	kal_uint16 line_length;
	kal_uint16 frame_length;
	AR1330MIPI_MODE sensor_mode;
	kal_uint16 dummy_pixels;
	kal_uint16 dummy_lines;
	kal_uint8 autoflicker;
	kal_uint16 gain;
	kal_uint16 shutter;
	
};

#endif /* _AR1330MIPI_SENSOR_H */

