#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

/* Pre-defined definition */
#define TPD_TYPE_CAPACITIVE
#define TPD_TYPE_RESISTIVE
#define TPD_POWER_SOURCE
//#defing TPD_CLODE_POWER_IN_SLEEP
//#define	TPD_ESD_CHECK			//if the CTP use the esd feature
#define	TPD_HIGH_SPEED_DMA		//if it use the dma mode, please define it

#define TPD_I2C_NUMBER			(0)
#define TPD_WAKEUP_TRIAL		(60)
#define TPD_WAKEUP_DELAY		(100)
#define REPORT_PACKET_LENGTH	(8)
#define MAX_TOUCH_FINGER		(2)
#define SWAP_X_Y 				(1)
#define REVERSE_X				(1)
#define REVERSE_Y				(1)

#define TPD_VELOCITY_CUSTOM_X	(12)
#define TPD_VELOCITY_CUSTOM_Y	(16)

#define TPD_DELAY                (2*HZ/100)
#define TPD_CALIBRATION_MATRIX  {962,0,0,0,1600,0,0,0};

#define TPD_RES_MAX_X			(1080)	//you can modify this part to set resolution
#define TPD_RES_MAX_Y			(1920)

#define TPD_HAVE_BUTTON			//If the CTP have the virtual key, please define it

#define TPD_BUTTON_Y			(1970)	//virtual key button y coordinate
#define TPD_BUTTON_WIDTH		(200)	//button width
#define TPD_BUTTON_HEIGH		(100)	//button height
#define TPD_KEY_COUNT           (3)		//button number
#define TPD_KEYS                {KEY_BACK, KEY_HOME, KEY_MENU}
#define TPD_KEYS_DIM            {{80,TPD_BUTTON_Y,TPD_BUTTON_WIDTH,TPD_BUTTON_HEIGH}, {440,TPD_BUTTON_Y,TPD_BUTTON_WIDTH,TPD_BUTTON_HEIGH}, {800,TPD_BUTTON_Y,TPD_BUTTON_WIDTH,TPD_BUTTON_HEIGH}}

#endif /* TOUCHPANEL_H__ */
