#ifndef TPD_CUSTOM_FTS2A052_H
#define TPD_CUSTOM_FTS2A052_H


#define TPD_HAVE_BUTTON

#define TPD_KEY_COUNT		3
#define TPD_KEYS                {KEY_BACK, KEY_HOMEPAGE ,KEY_MENU}
#define TPD_KEYS_DIM            {{270,1980,100,80},\
				 {540,1980,100,80},\
				 {810,1980,100,80}}

#define TPD_I2C_NUMBER 0
#define TPD_POWER_SOURCE_CUSTOM MT6322_POWER_LDO_VGP1
#define TPD_POWER_SOURCE_1800 MT6322_POWER_LDO_VRF18

#define X_AXIS_MAX                          1080
#define X_AXIS_MIN                          0
#define Y_AXIS_MAX                          1920
#define Y_AXIS_MIN                          0

#endif