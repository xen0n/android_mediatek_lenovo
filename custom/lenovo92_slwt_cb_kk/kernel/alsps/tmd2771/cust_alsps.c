#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>
#include <mach/upmu_common.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
	.polling_mode_ps =0,
	.polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    //.i2c_addr   = {0x0C, 0x48, 0x78, 0x00},
    //.als_level  = { 0,  1,  1,   7,  15,  15,  100, 1000, 2000,  3000,  6000, 10000, 14000, 18000, 20000},
    //.als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240},
    /* MTK: modified to support AAL */
    .als_level  = { 308, 2470, 3957, 8182, 22063, 27806, 44537, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535},
    .als_value  = { 132, 303, 503, 1003, 2004, 3011, 5006,  8001,  8001, 8001, 8001, 8001,  8001,  8001, 8001, 8001},
    .ps_threshold_high = 65535,
    .ps_threshold_low = 0,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}

int pmic_ldo_suspend_enable(int enable)
{
	//0 for disable suspend, 1 for enable suspend
	upmu_set_vio18_lp_sel(enable);
	upmu_set_vio28_lp_sel(enable);
	return 0;
}

int TMD2771_CMM_PPCOUNT_VALUE = 0x0B;
int ZOOM_TIME = 4;
int TMD2771_CMM_CONTROL_VALUE = 0x20;


