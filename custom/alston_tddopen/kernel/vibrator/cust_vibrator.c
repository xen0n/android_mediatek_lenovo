#include <cust_vibrator.h>
#include <linux/types.h>

static struct vibrator_hw cust_vibrator_hw = {
	/*lenovo-sw heww3 20140606 modify for haptic feedback begin*/
	.vib_timer = 0,
	
  #ifdef CUST_VIBR_LIMIT
	.vib_limit = 0,
  #endif
  #ifdef CUST_VIBR_VOL
	.vib_vol = 0x5,//2.8V for vibr
  #endif
	/*lenovo-sw heww3 20140606 modify end*/
};

struct vibrator_hw *get_cust_vibrator_hw(void)
{
    return &cust_vibrator_hw;
}

