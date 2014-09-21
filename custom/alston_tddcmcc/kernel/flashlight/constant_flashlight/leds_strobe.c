#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <cust_gpio_usage.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/version.h>
#include <mach/mt_pwm.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#include <linux/mutex.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#endif

#include <linux/i2c.h>
#include <linux/leds.h>
/*
#if defined(LENOVO_PROJECT_STELLA) || defined(LENOVO_PROJECT_STELLAP)
#define FLASH_BD7710
#else
#define FLASH_RT9387
#endif
*/
/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE
#define TAG_NAME "leds_strobe.c"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        xlog_printk(ANDROID_LOG_WARNING, TAG_NAME, KERN_WARNING  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_NOTICE  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        xlog_printk(ANDROID_LOG_INFO   , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME,  "<%s>\n", __FUNCTION__);
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	#define PK_DBG printk
	#define PK_VER printk
	#define PK_ERR printk
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif



/******************************************************************************
 * local variables
******************************************************************************/

static DEFINE_SPINLOCK(g_strobeSMPLock); /* cotta-- SMP proection */


static u32 strobe_Res = 0;
static u32 strobe_Timeus = 0;
static BOOL g_strobe_On = 0;

static int g_duty=-1;
static int g_timeOutTimeMs=0;
static int g_timeTorchTimeMs=0;



#define FLASHLIGHT_DEVNAME            "SGM3785"
#define FLASH_SGM3785
struct flash_chip_data {
	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;

	struct mutex lock;    

	int mode;
	int torch_level;
};

static struct flash_chip_data chipconf;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
static DEFINE_MUTEX(g_strobeSem);
#else
static DECLARE_MUTEX(g_strobeSem);
#endif


#define STROBE_DEVICE_ID 0x60


static struct work_struct workTimeOut;
static struct work_struct workTimeTorch;

/*****************************************************************************
Functions
*****************************************************************************/
#define GPIO_ENF (GPIO12 | 0x80000000)
#define GPIO_ENT (GPIO13 | 0x80000000)


    /*CAMERA-FLASH-EN */


extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
static void work_timeOutFunc(struct work_struct *data);

#if defined(FLASH_BD7710)
static struct i2c_client *BD7710_i2c_client = NULL;

#define GPIO_ENF GPIO_CAMERA_FLASH_MODE_PIN
#define GPIO_ENT GPIO_CAMERA_FLASH_TX1_PIN
#define EN_PIN GPIO_CAMERA_FLASH_EN_PIN
#define RESET_PIN GPIO_CAMERA_FLASH_RESET_PIN
struct BD7710_platform_data {
	u8 torch_pin_enable;    // 1:  TX1/TORCH pin isa hardware TORCH enable
	u8 pam_sync_pin_enable; // 1:  TX2 Mode The ENVM/TX2 is a PAM Sync. on input
	u8 thermal_comp_mode_enable;// 1: LEDI/NTC pin in Thermal Comparator Mode
	u8 strobe_pin_disable;  // 1 : STROBE Input disabled
	u8 vout_mode_enable;  // 1 : Voltage Out Mode enable
};

struct BD7710_chip_data {
	struct i2c_client *client;

	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;
	struct led_classdev cdev_indicator;

	struct BD7710_platform_data *pdata;
	struct mutex lock;    

	u8 last_flag; 
	u8 no_pdata; 
};

/* i2c access*/
static int BD7710_read_reg(struct i2c_client *client, u8 reg,u8 *val)
{
	int ret;
	struct BD7710_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (ret < 0) {
		PK_ERR("failed reading at 0x%02x error %d\n",reg, ret);
		return ret;
	}
	*val = ret&0xff; 

	return 0;
}

static int BD7710_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret=0;
	struct BD7710_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret =  i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);
	
	if (ret < 0)
		PK_ERR("failed writting at 0x%02x\n", reg);
	return ret;
}

static int BD7710_chip_init(struct BD7710_chip_data *chip)
{
	int ret =0;
	struct i2c_client *client = chip->client;
	struct BD7710_platform_data *pdata = chip->pdata; 
	PK_DBG("BD7710_chip_init start--->.\n");

    mt_set_gpio_mode(EN_PIN, 0);
    mt_set_gpio_dir(EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(EN_PIN, GPIO_OUT_ONE);

    mt_set_gpio_mode(GPIO_ENF, 0);
    mt_set_gpio_dir(GPIO_ENF, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ZERO);

    mt_set_gpio_mode(GPIO_ENT, 0);
    mt_set_gpio_dir(GPIO_ENT, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_ENT, GPIO_OUT_ZERO);
	mdelay(1);
    mt_set_gpio_mode(RESET_PIN, 0);
    mt_set_gpio_dir(RESET_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ONE);
	mdelay(5);
    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ZERO);
	mdelay(5);
    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ONE);
	mdelay(10);
	do {
		ret = BD7710_write_reg(client, 0x00, 0x0A);
	}while(0);

	if(ret >= 0)
	{
		ret = 0;
	}
	else
	{
		ret = -1;
	}
	PK_DBG("BD7710_chip_init end ret=%d.\n", ret);

	return ret;
}

static int BD7710_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct BD7710_chip_data *chip;
	struct BD7710_platform_data *pdata = client->dev.platform_data;

	int err = -1;

	PK_DBG("BD7710_probe start--->.\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printk(KERN_ERR  "BD7710 i2c functionality check fail.\n");
		return err; 
	}

	chip = kzalloc(sizeof(struct BD7710_chip_data), GFP_KERNEL);
	chip->client = client;	

	mutex_init(&chip->lock);	
	i2c_set_clientdata(client, chip);

	if(pdata == NULL){ //values are set to Zero. 
		PK_ERR("BD7710 Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct BD7710_platform_data),GFP_KERNEL);
		chip->pdata  = pdata;
		chip->no_pdata = 1; 
	}
	
	chip->pdata  = pdata;
	if(BD7710_chip_init(chip)<0)
		goto err_chip_init;

	BD7710_i2c_client = client;
	PK_DBG("BD7710 Initializing is done \n");

	return 0;

err_chip_init:	
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	PK_ERR("BD7710 probe is failed \n");
	return -ENODEV;
}

static int BD7710_remove(struct i2c_client *client)
{
	struct BD7710_chip_data *chip = i2c_get_clientdata(client);

    if(chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);
	return 0;
}


#define BD7710_NAME "leds-BD7710"
static const struct i2c_device_id BD7710_id[] = {
	{BD7710_NAME, 0},
	{}
};

static struct i2c_driver BD7710_i2c_driver = {
	.driver = {
		.name  = BD7710_NAME,
	},
	.probe	= BD7710_probe,
	.remove   = __devexit_p(BD7710_remove),
	.id_table = BD7710_id,
};

struct BD7710_platform_data BD7710_pdata = {0, 0, 0, 0, 0};
static struct i2c_board_info __initdata i2c_BD7710={ I2C_BOARD_INFO(BD7710_NAME, 0x66>>1), \
													.platform_data = &BD7710_pdata,};

static int __init BD7710_init(void)
{
	printk("BD7710_init\n");
	i2c_register_board_info(2, &i2c_BD7710, 1);

	return i2c_add_driver(&BD7710_i2c_driver);
}

static void __exit BD7710_exit(void)
{
	i2c_del_driver(&BD7710_i2c_driver);
}


module_init(BD7710_init);
module_exit(BD7710_exit);

MODULE_DESCRIPTION("Flash Lighting driver for BD7710");
MODULE_AUTHOR("zhangjiano <zhangjiano@lenovo.com>");
MODULE_LICENSE("GPL v2");

#define TORCH_BRIGHTNESS 0
#define FLASH_BRIGHTNESS 15


int FL_Enable(void)
{
	struct flash_chip_data *chip = &chipconf;
	int brightness = 0;
	u8 tmp4,tmp5;
	PK_DBG("FL_enable g_duty=%d\n",g_duty);
    mt_set_gpio_out(EN_PIN, GPIO_OUT_ONE);
	mdelay(1);
    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ONE);
	mdelay(1);
	if(g_duty == 0)//torch
	{
		BD7710_write_reg(BD7710_i2c_client, 0x02, 0x40);
		BD7710_write_reg(BD7710_i2c_client, 0x00, 0x14); //75ma torch output_en'
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ONE);
		
		PK_DBG("FL_enable  torch brightness=%d\n",brightness);
		
		chip->torch_level = brightness;
		chip->mode = 1;
	}
	else if((g_duty >= (TORCH_BRIGHTNESS+1))&&(g_duty <= FLASH_BRIGHTNESS))
	{
		brightness =(g_duty + 3)|0x80;//mix = 225,max = 1350
		PK_DBG("FL_enable flash brightness=%d\n",brightness);
		BD7710_write_reg(BD7710_i2c_client, 0x02,brightness); //75ma torch output_en'
		BD7710_write_reg(BD7710_i2c_client, 0x01, brightness); //750ma flash output_en
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ZERO);

		chip->torch_level = 0;
		chip->mode = 2;
	}
    return 0;
}

int FL_Disable(void)
{
	struct flash_chip_data *chip = &chipconf;
	PK_DBG("FL_disable g_duty=%d\n",g_duty);
	if((chip->mode == 2)||(chip->mode == 1))
	{
		BD7710_write_reg(BD7710_i2c_client, 0x02,0x00); //75ma torch output_en'
		BD7710_write_reg(BD7710_i2c_client, 0x01, 0x00); //750ma flash output_en
		BD7710_write_reg(BD7710_i2c_client, 0x00, 0x80); //750ma flash output_en
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ONE);
	    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ZERO);
		mdelay(1);
	    mt_set_gpio_out(EN_PIN, GPIO_OUT_ZERO);
		
		//mt_set_gpio_out(GPIO_ENT, GPIO_OUT_ZERO);
		//udelay(50);
		chip->torch_level = 0;
		chip->mode = 0;
	}

    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	g_duty=duty;

	PK_DBG("FL_dim_duty\n");
    return 0;
}


int FL_Init(void)
{
	PK_DBG("FL_init\n");
	if(BD7710_i2c_client == NULL)
    {
    	return 0;
    }
	BD7710_write_reg(BD7710_i2c_client, 0x01, 0xC0); //750ma flash output_en
	mt_set_gpio_out(GPIO_ENT, GPIO_OUT_ZERO);
	INIT_WORK(&workTimeOut, work_timeOutFunc);

    return 0;
}


int FL_Uninit(void)
{
	PK_DBG("FL_uninit\n");
	FL_Disable();
    return 0;
}

#elif defined(FLASH_RT9387)
#define GPIO_ENF GPIO_CAMERA_FLASH_EN_PIN
#define GPIO_ENT GPIO_CAMERA_FLASH_MODE_PIN

int FL_Enable(void)
{
	if(g_duty==0)
	{
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ONE);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		PK_DBG(" FL_Enable line=%d\n",__LINE__);
	}
	else
	{
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		PK_DBG(" FL_Enable line=%d\n",__LINE__);
	}

    return 0;
}

int FL_Disable(void)
{

	mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
	mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
	PK_DBG(" FL_Disable line=%d\n",__LINE__);
    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	g_duty=duty;
	PK_DBG(" FL_dim_duty line=%d\n",__LINE__);
    return 0;
}


int FL_Init(void)
{


   if(mt_set_gpio_mode(GPIO_ENF,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_ENF,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    /*Init. to disable*/
    if(mt_set_gpio_mode(GPIO_ENT,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_ENT,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}

	INIT_WORK(&workTimeOut, work_timeOutFunc);
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


int FL_Uninit(void)
{
	FL_Disable();
    return 0;
}
#elif		defined(FLASH_SGM3785)

#define GPIO_ENF 				(GPIO12|0x80000000)
#define GPIO_ENT				(GPIO106|0x80000000)
#define TORCH_BRIGHTNESS 		0
#define FLASH_BRIGHTNESS 		15
#define FLASH_BRIGHTNESS_MAX	15
#define FLASH_ENT_PIN_M_GPIO		GPIO_MODE_00
#define FLASH_ENT_PIN_M_PWM		GPIO_MODE_05
#define FLASH_ENT_PIN_GPIO_H		(1)
#define FLASH_ENT_PIN_GPIO_L		(0)
struct 	pwm_spec_config	pwm_setting={
						.pwm_no=PWM4,
						.mode=PWM_MODE_OLD,
						.clk_div=CLK_DIV1,
						.clk_src=PWM_CLK_OLD_MODE_32K,
						.pmic_pad=false,
						.PWM_MODE_OLD_REGS.IDLE_VALUE=0,
						.PWM_MODE_OLD_REGS.GUARD_VALUE=0,
						.PWM_MODE_OLD_REGS.GDURATION=0,
						.PWM_MODE_OLD_REGS.WAVE_NUM=0,
						.PWM_MODE_OLD_REGS.DATA_WIDTH=255,
						.PWM_MODE_OLD_REGS.THRESH=0};

/*
int FL_PWM_SetTorchBegin(){

	pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE=1;
	pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM=1;
	pwm_setting.PWM_MODE_OLD_REGS.THRESH=255;
	pwm_set_spec_config(&pwm_setting);
	msleep(5);
}
int FL_PWM_SetFlashBegin(){

	pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE=0;
	pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM=0;
	pwm_setting.PWM_MODE_OLD_REGS.THRESH=0;
}
int FL_PWM_SetTorchEnd(){

	pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE=0;
	pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM=1;
	pwm_setting.PWM_MODE_OLD_REGS.THRESH=1;
	pwm_set_spec_config(&pwm_setting);
}
*/
int  	FL_ENT_SETMODE_GPIO(UINT8  mLevele){
	mt_set_gpio_mode(GPIO_ENT,FLASH_ENT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_ENT,GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_ENT,mLevele);	
}

int 	FL_ENT_SETMODE_PWM(){
	mt_set_gpio_mode(GPIO_ENT,FLASH_ENT_PIN_M_PWM);
}
int FL_PWM_SetBrightness(UINT32 mBrightness){

	pwm_setting.PWM_MODE_OLD_REGS.THRESH=mBrightness;
	pwm_set_spec_config(&pwm_setting);
	return 0;
}
static void workTimeTorchFunc(struct work_struct *data)
{
	FL_ENT_SETMODE_PWM();
	FL_PWM_SetBrightness(125);
	PK_DBG("%s\n",__func__);
}
enum hrtimer_restart TorchTimeOutCallback(struct hrtimer *timer)
{
   
    schedule_work(&workTimeTorch); 
    PK_DBG("%s\n",__func__);
    return HRTIMER_NORESTART;
}
static struct hrtimer g_timeTorchTimer;
void Torch_timerInit(void)
{
	g_timeTorchTimeMs=6; //1s
	hrtimer_init( &g_timeTorchTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeTorchTimer.function=TorchTimeOutCallback;

}
int FL_Enable(void)
{
	int brightness = 0;
	PK_DBG("Suny duty=%d\r\n",g_duty);
	if(g_duty == 0)//torch
	{
		brightness=220;            	
		//ktime_t kTorchtime;
		//g_timeTorchTimeMs=6;
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		FL_ENT_SETMODE_GPIO(FLASH_ENT_PIN_GPIO_H);
		mdelay(4);
		FL_ENT_SETMODE_PWM();
		FL_PWM_SetBrightness(brightness);
		//kTorchtime = ktime_set( 0, g_timeTorchTimeMs*1000000 );
		//hrtimer_start( &g_timeTorchTimer, kTorchtime, HRTIMER_MODE_REL );
		PK_DBG(" Suny FL_enable  torch brightness=%d\n",brightness);
	}
	else if((g_duty >= (TORCH_BRIGHTNESS+1))&&(g_duty <= FLASH_BRIGHTNESS))
	{
		brightness=(g_duty*255)/(FLASH_BRIGHTNESS+1);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		FL_ENT_SETMODE_PWM();
		FL_PWM_SetBrightness(brightness);
		udelay(50);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		PK_DBG("Suny FL_enable flash brightness=%d\n",brightness);
	}
    	return 0;
}

int FL_Disable(void)
{
	int brightness=0;
	PK_DBG("Suny SGM %s\r\n",__func__);
	mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
	FL_ENT_SETMODE_PWM();
	mt_pwm_disable(pwm_setting.pwm_no,pwm_setting.pmic_pad);

    	return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	g_duty=duty;
	PK_DBG(" Suny FL_dim_duty=%d line=%d\n",duty,__LINE__);
    	return 0;
}


int FL_Init(void)
{
	PK_DBG("Suny SGM %s\r\n",__func__);
	mt_set_gpio_out(GPIO_ENT, GPIO_OUT_ZERO);
	INIT_WORK(&workTimeOut, work_timeOutFunc);	
	//INIT_WORK(&workTimeTorch,workTimeTorchFunc);
   	return 0;
}


int FL_Uninit(void)
{
	PK_DBG("Suny SGM %s\r\n",__func__);
	FL_Disable();
    	return 0;
}
#else
int FL_Enable(void)
{
	if(g_duty==0)
	{
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ONE);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		PK_DBG(" FL_Enable line=%d\n",__LINE__);
	}
	else
	{
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		PK_DBG(" FL_Enable line=%d\n",__LINE__);
	}

    return 0;
}

int FL_Disable(void)
{

	mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
	mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
	PK_DBG(" FL_Disable line=%d\n",__LINE__);
    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	g_duty=duty;
	PK_DBG(" FL_dim_duty line=%d\n",__LINE__);
    return 0;
}


int FL_Init(void)
{


	if(mt_set_gpio_mode(GPIO_ENF,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_ENF,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    /*Init. to disable*/
    if(mt_set_gpio_mode(GPIO_ENT,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_ENT,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}

	INIT_WORK(&workTimeOut, work_timeOutFunc);
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


int FL_Uninit(void)
{
	FL_Disable();
    return 0;
}
#endif
/*****************************************************************************
User interface
*****************************************************************************/

static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
    PK_DBG("ledTimeOut_callback\n");
    //printk(KERN_ALERT "work handler function./n");
}



enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}
static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;

}



static int constant_flashlight_ioctl(MUINT32 cmd, MUINT32 arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift, arg);
    switch(cmd)
    {

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
			g_timeOutTimeMs=arg;
		break;


    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
    		FL_dim_duty(arg);
    		break;


    	case FLASH_IOC_SET_STEP:
    		PK_DBG("FLASH_IOC_SET_STEP: %d\n",arg);

    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
    		if(arg==1)
    		{
				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( 0, g_timeOutTimeMs*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
    			FL_Enable();
    		}
    		else
    		{
    			FL_Disable();
				hrtimer_cancel( &g_timeOutTimer );
    		}
    		break;
		default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
	    FL_Init();
		timerInit();
		//Torch_timerInit();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }


    spin_unlock_irq(&g_strobeSMPLock);
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

    return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);

        strobe_Res = 0;
        strobe_Timeus = 0;

        /* LED On Status */
        g_strobe_On = FALSE;

        spin_unlock_irq(&g_strobeSMPLock);

    	FL_Uninit();
    }

    PK_DBG(" Done\n");

    return 0;

}


FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &constantFlashlightFunc;
    }
    return 0;
}



/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);


/***************                   *******************/
#if 1//def CONFIG_LENOVO
static void chip_torch_brightness_set(struct led_classdev *cdev,
				  enum led_brightness brightness)
{
	int i, cc;
	struct flash_chip_data *chip = &chipconf;
	u8 tmp4,tmp5;
	PK_ERR("[flashchip] torch brightness = %d\n",brightness);
	#if defined(FLASH_BD7710)
	if(brightness == 0)
	{
		BD7710_write_reg(BD7710_i2c_client, 0x02,0x88); //75ma torch output_en'
		BD7710_write_reg(BD7710_i2c_client, 0x01, 0xC0); //750ma flash output_en
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ONE);
	    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ZERO);
		mdelay(1);
	    mt_set_gpio_out(EN_PIN, GPIO_OUT_ZERO);

		return;
	}
	else
	{
		mt_set_gpio_out(EN_PIN, GPIO_OUT_ONE);
		mdelay(1);
		mt_set_gpio_out(RESET_PIN, GPIO_OUT_ONE);
		mdelay(1);
		BD7710_write_reg(BD7710_i2c_client, 0x02, 0x88);
		BD7710_write_reg(BD7710_i2c_client, 0x01, 0x50); //75ma torch output_en'
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ZERO);
	}
	#elif defined(FLASH_RT9387)
	if(brightness == 0)
	{
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
		mdelay(4);
		chip->torch_level = 0;
		chip->mode = 0;
		PK_ERR("[flashchip] level = 0\n");

		return;
	}
	else
	{
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ONE);
		mdelay(4);
		chip->torch_level = 0;
		chip->mode = 0;	
	}
	/*lenovo-sw sunliang add for SGM3785 2014_3_31 begin*/
	#elif defined(FLASH_SGM3785)
	if(brightness == 0)
	{
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		FL_ENT_SETMODE_PWM();
		mt_pwm_disable(pwm_setting.pwm_no,pwm_setting.pmic_pad);
		chip->torch_level = 0;
		chip->mode = 0;
		PK_ERR("[flashchip] level = 0\n");
	}
	else
	{
		brightness=220;
		//ktime_t kTorchtime;
		//g_timeTorchTimeMs=6;
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		FL_ENT_SETMODE_GPIO(FLASH_ENT_PIN_GPIO_H);
		mdelay(5);
		FL_ENT_SETMODE_PWM();
		FL_PWM_SetBrightness(brightness);
		chip->torch_level = 0;
		chip->mode = 0;	
	}
	/*lenovo-sw sunliang add for SGM3785 2014_3_31 end*/
	#endif
}


static void chip_flash_brightness_set(struct led_classdev *cdev,
				  enum led_brightness brightness)
{
	struct flash_chip_data *chip = &chipconf;
	u8 tmp4,tmp5;
	PK_ERR("[flashchip] flash brightness = %d\n",brightness);
	#if defined(FLASH_BD7710)
	if(brightness == 0)
	{
		BD7710_write_reg(BD7710_i2c_client, 0x02,0x88); //75ma torch output_en'
		BD7710_write_reg(BD7710_i2c_client, 0x01, 0xC0); //750ma flash output_en
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ONE);
	    mt_set_gpio_out(RESET_PIN, GPIO_OUT_ZERO);
		mdelay(1);
	    mt_set_gpio_out(EN_PIN, GPIO_OUT_ZERO);
	}
	else
	{
		mt_set_gpio_out(EN_PIN, GPIO_OUT_ONE);
		mdelay(1);
		mt_set_gpio_out(RESET_PIN, GPIO_OUT_ONE);
		mdelay(1);
		BD7710_write_reg(BD7710_i2c_client, 0x02,0x88); //75ma torch output_en'
		BD7710_write_reg(BD7710_i2c_client, 0x01, 0x9A); //750ma flash output_en
		udelay(50);
		mt_set_gpio_out(GPIO_ENF, GPIO_OUT_ZERO);
		PK_ERR("[flashchip] flash level = 1\n");
	}
	#elif defined(FLASH_RT9387)
	if(brightness == 0)
	{
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
		mdelay(4);
		chip->torch_level = 0;
		chip->mode = 0;
		PK_ERR("[flashchip] flash level = 0\n");
	}
	else
	{
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
		//mdelay(4);
		chip->torch_level = 0;
		chip->mode = 2;
		PK_ERR("[flashchip] flash level = 1\n");
	}
	/*lenovo-sw sunliang add for SGM3785 2014_3_31 begin*/
	#elif defined(FLASH_SGM3785)
	if(brightness == 0)
	{
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
		FL_ENT_SETMODE_PWM();
		mt_pwm_disable(pwm_setting.pwm_no,pwm_setting.pmic_pad);
		chip->torch_level = 0;
		chip->mode = 0;
		PK_ERR("[flashchip] level = 0\n");
	}
	else
	{
		FL_ENT_SETMODE_PWM();
		FL_PWM_SetBrightness(125);
		udelay(50);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		chip->torch_level = 0;
		chip->mode = 2;	
	}
	/*lenovo-sw sunliang add for SGM3785 2014_3_31 end*/
	#endif
	return;
}


static int flashchip_probe(struct platform_device *dev)
{
	struct flash_chip_data *chip;

	PK_ERR("[flashchip_probe] start\n");
	chip = &chipconf;
	chip->mode = 0;
	chip->torch_level = 0;
	mutex_init(&chip->lock);

	//flash
	chip->cdev_flash.name="flash";
	chip->cdev_flash.max_brightness = 1;
	chip->cdev_flash.brightness_set = chip_flash_brightness_set;
	if(led_classdev_register((struct device *)&dev->dev,&chip->cdev_flash)<0)
		goto err_create_flash_file;	
	//torch
	chip->cdev_torch.name="torch";
	chip->cdev_torch.max_brightness = 16;
	chip->cdev_torch.brightness_set = chip_torch_brightness_set;
	if(led_classdev_register((struct device *)&dev->dev,&chip->cdev_torch)<0)
		goto err_create_torch_file;

    PK_ERR("[flashchip_probe] Done\n");
    return 0;

err_create_torch_file:
	led_classdev_unregister(&chip->cdev_flash);
err_create_flash_file:
err_chip_init:	
	printk(KERN_ERR "[flashchip_probe] is failed !\n");
	return -ENODEV;



}

static int flashchip_remove(struct platform_device *dev)
{
	struct flash_chip_data *chip = &chipconf;
    PK_DBG("[flashchip_remove] start\n");

	led_classdev_unregister(&chip->cdev_torch);
	led_classdev_unregister(&chip->cdev_flash);


    PK_DBG("[flashchip_remove] Done\n");
    return 0;
}


static struct platform_driver flashchip_platform_driver =
{
    .probe      = flashchip_probe,
    .remove     = flashchip_remove,
    .driver     = {
        .name = FLASHLIGHT_DEVNAME,
		.owner	= THIS_MODULE,
    },
};



static struct platform_device flashchip_platform_device = {
    .name = FLASHLIGHT_DEVNAME,
    .id = 0,
    .dev = {
//    	.platform_data	= &chip,
    }
};

static int __init flashchip_init(void)
{
    int ret = 0;
    printk("[flashchip_init] start\n");

	ret = platform_device_register (&flashchip_platform_device);
	if (ret) {
        PK_ERR("[flashchip_init] platform_device_register fail\n");
        return ret;
	}

    ret = platform_driver_register(&flashchip_platform_driver);
	if(ret){
		PK_ERR("[flashchip_init] platform_driver_register fail\n");
		return ret;
	}

	printk("[flashchip_init] done!\n");
    return ret;
}

static void __exit flashchip_exit(void)
{
    printk("[flashchip_exit] start\n");
    platform_driver_unregister(&flashchip_platform_driver);
    printk("[flashchip_exit] done!\n");
}

/*****************************************************************************/
module_init(flashchip_init);
module_exit(flashchip_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhangjiano@lenovo.com>");
MODULE_DESCRIPTION("Factory mode flash control Driver");
#endif


