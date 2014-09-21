#ifndef __CUST_KEY_H__
#define __CUST_KEY_H__

#include<cust_kpd.h>

//lenovo_sw liaohj delete for use dct define 2014-03-14
//#define MT65XX_RECOVERY_KEY	32
#define MT65XX_META_KEY		42	/* KEY_2 */
#define MT65XX_PMIC_RST_KEY	32 /*for pmic key use*/
#define MT_CAMERA_KEY 		10

#define MT65XX_BOOT_MENU_KEY       32   /* KEY_VOLUMEUP */
#define MT65XX_MENU_SELECT_KEY     MT65XX_BOOT_MENU_KEY   
#define MT65XX_MENU_OK_KEY         0 /* KEY_VOLUMEDOWN */

/*lenovo-sw chuyq 2014.3.18 add begin*/
#define LENOVO_META_KEY MT65XX_FACTORY_KEY
#define LENOVO_FACTORY_KEY MT65XX_RECOVERY_KEY
/*lenovo-sw chuyq 2014.3.18 add end*/
#endif /* __CUST_KEY_H__ */
