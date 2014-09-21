/* drivers/hwmon/mt6516/amit/epl2182.c - EPL2182 ALS/PS driver
 * 
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

// last modified in 2012.12.12 by renato pan
//lenovo
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include <linux/hwmsen_helper.h>
#include "epl2182.h"


#include <linux/earlysuspend.h> 
#include <linux/wakelock.h>



/******************************************************************************
 * extern functions
*******************************************************************************/

extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);

extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

/*-------------------------MT6516&MT6573 define-------------------------------*/
#define DYNAMIC_INTT //ices add by 20131216 /*DYNAMIC_INTT*/
/******************************************************************************
 * configuration
*******************************************************************************/

// TODO: change ps/als integrationtime
#define PS_INTT 					4
#define ALS_INTT					7

#define TXBYTES 				2
#define RXBYTES 				2
#define PACKAGE_SIZE 			2
#define I2C_RETRY_COUNT 		10



#ifdef DYNAMIC_INTT //ices add by 20131216 /*DYNAMIC_INTT*/
// TODO: change delay time
#define PS_DELAY 			120
#define ALS_DELAY 			100

int dynamic_intt_idx;
int dynamic_intt_init_idx = 2;	//initial dynamic_intt_idx
/*Lenovo-sw caoyi1 modify for als real lux 2014-04-15 start*/
int c_gain = 660; // 10.0/1000 *10000 //0.66
/*Lenovo-sw caoyi1 modify for als real lux 2014-04-15 end*/

uint8_t dynamic_intt_intt;
uint8_t dynamic_intt_gain;
/*Lenovo-sw caoyi1 modify for cycle als lux 2014-04-16 start*/
uint8_t dynamic_intt_cycle;
/*Lenovo-sw caoyi1 modify for cycle als lux 2014-04-16 end*/
uint16_t dynamic_intt_high_thr;
uint16_t dynamic_intt_low_thr;
uint32_t dynamic_intt_max_lux = 87000;  //8700
uint32_t dynamic_intt_min_lux = 5;      //0.5
uint32_t dynamic_intt_min_unit = 1000;  //1000: float, 1000:integer

static int als_dynamic_intt_intt[] = {EPL_ALS_INTT_4096, EPL_ALS_INTT_256, EPL_ALS_INTT_16, EPL_ALS_INTT_16};
static int als_dynamic_intt_intt_value[] = {4096, 256, 16, 16};
static int als_dynamic_intt_gain[] = {EPL_M_GAIN, EPL_M_GAIN, EPL_M_GAIN, EPL_L_GAIN};
static int als_dynamic_intt_high_thr[] = {900, 800, 400, 3200};
static int als_dynamic_intt_low_thr[] = {32, 32, 32, 256};

static int als_dynamic_intt_intt_num =  sizeof(als_dynamic_intt_intt_value)/sizeof(int);

int als_report_idx = 0;

#else

// TODO: change delay time
#define PS_DELAY 			120
#define ALS_DELAY 			70

#endif //ices end by 20131216  /*DYNAMIC_INTT*/


typedef struct _epl_raw_data
{
    u8 raw_bytes[PACKAGE_SIZE];
    u16 ps_raw;
    u16 ps_state;
	u16 ps_int_state;
    u16 als_ch0_raw;
    u16 als_ch1_raw;
    int  als_lux;
} epl_raw_data;


#define EPL2182_DEV_NAME     "EPL2182"


/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO fmt, ##args)                 
#define FTM_CUST_ALSPS "/data/epl2182"


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static struct i2c_client *epl2182_i2c_client = NULL;
static struct mutex sensor_mutex;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id epl2182_i2c_id[] = {{"EPL2182",0},{}};
static struct i2c_board_info __initdata i2c_EPL2182={ I2C_BOARD_INFO("EPL2182", (0X92>>1))};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short epl2182_force[] = {0x00, 0x92, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const epl2182_forces[] = { epl2182_force, NULL };
//static struct i2c_client_address_data epl2182_addr_data = { .forces = epl2182_forces,};


/*----------------------------------------------------------------------------*/
static int epl2182_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int epl2182_i2c_remove(struct i2c_client *client);
static int epl2182_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);


/*----------------------------------------------------------------------------*/
static int epl2182_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int epl2182_i2c_resume(struct i2c_client *client);
static void epl2182_eint_func(void);
static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd);
	
static struct epl2182_priv *g_epl2182_ptr = NULL;
static bool isInterrupt = false;

static void polling_do_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(polling_work, polling_do_work);

/*----------------------------------------------------------------------------*/
typedef enum 
{
    CMC_TRC_ALS_DATA = 0x0001,
    CMC_TRC_PS_DATA = 0X0002,
    CMC_TRC_EINT    = 0x0004,
    CMC_TRC_IOCTL   = 0x0008,
    CMC_TRC_I2C     = 0x0010,
    CMC_TRC_CVT_ALS = 0x0020,
    CMC_TRC_CVT_PS  = 0x0040,
    CMC_TRC_DEBUG   = 0x0800,
} CMC_TRC;

/*----------------------------------------------------------------------------*/
typedef enum 
{
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;

/*----------------------------------------------------------------------------*/
struct epl2182_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};

/*----------------------------------------------------------------------------*/
struct epl2182_priv 
{
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct delayed_work  eint_work;
	  struct workqueue_struct *epl_wq;

    /*i2c address group*/
   struct epl2182_i2c_addr  addr;
    
    int enable_pflag;
    int enable_lflag;

    /*misc*/
    atomic_t    trace;
    atomic_t    i2c_retry;
    atomic_t    als_suspend;
    atomic_t    als_debounce;   /*debounce time after enabling als*/
    atomic_t    als_deb_on;     /*indicates if the debounce is on*/
    atomic_t    als_deb_end;    /*the jiffies representing the end of debounce*/
    atomic_t    ps_mask;        /*mask ps: always return far away*/
    atomic_t    ps_debounce;    /*debounce time after enabling ps*/
    atomic_t    ps_deb_on;      /*indicates if the debounce is on*/
    atomic_t    ps_deb_end;     /*the jiffies representing the end of debounce*/
    atomic_t    ps_suspend;
    atomic_t    ps_thd_val_high;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val_low;     /*the cmd value can't be read, stored in ram*/
	
    /*data*/
    u16         als;
    u16          ps;
    u8          _align;
    u8   		als_thd_level;
    bool   		als_enable;    /*record current als status*/
    bool    	ps_enable;     /*record current ps status*/
    ulong       enable;         /*record HAL enalbe status*/
    ulong       pending_intr;   /*pending interrupt*/
    //ulong        first_read;   // record first read ps and als

	/*data*/
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

	/*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif
};



/*----------------------------------------------------------------------------*/
static struct i2c_driver epl2182_i2c_driver =
{
	.probe      = epl2182_i2c_probe,
	.remove     = epl2182_i2c_remove,
	.detect     = epl2182_i2c_detect,
	.suspend    = epl2182_i2c_suspend,
	.resume     = epl2182_i2c_resume,
	.id_table   = epl2182_i2c_id,
	//.address_data = &epl2182_addr_data,
	.driver = {
		//.owner          = THIS_MODULE,
		.name           = EPL2182_DEV_NAME,
	},
};

static struct epl2182_priv *epl2182_obj = NULL;
static struct platform_driver epl2182_alsps_driver;
static struct wake_lock ps_lock; /* Bob.chen add for if ps run, the system forbid to goto sleep mode. */
static epl_raw_data	gRawData;

//static struct wake_lock als_lock; /* Bob.chen add for if ps run, the system forbid to goto sleep mode. */


/*
//====================I2C write operation===============//
//regaddr: ELAN epl2182 Register Address.
//bytecount: How many bytes to be written to epl2182 register via i2c bus.
//txbyte: I2C bus transmit byte(s). Single byte(0X01) transmit only slave address.
//data: setting value.
//
// Example: If you want to write single byte to 0x1D register address, show below
//	      elan_epl2182_I2C_Write(client,0x1D,0x01,0X02,0xff);
//
*/
static int elan_epl2182_I2C_Write(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount, uint8_t txbyte, uint8_t data)
{
    uint8_t buffer[2];
    int ret = 0;
    int retry;

   // printk("[ELAN epl2182] %s\n", __func__);

    buffer[0] = (regaddr<<3) | bytecount ;
    buffer[1] = data;


    //printk("---buffer data (%x) (%x)---\n",buffer[0],buffer[1]);

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_send(client, buffer, txbyte);

        if (ret == txbyte)
        {
            break;
        }

        printk("i2c write error,TXBYTES %d\r\n",ret);
        mdelay(10);
    }


    if(retry>=I2C_RETRY_COUNT)
    {
        printk(KERN_ERR "[ELAN epl2182 error] %s i2c write retry over %d\n",__func__, I2C_RETRY_COUNT);
        return -EINVAL;
    }

    return ret;
}




/*
//====================I2C read operation===============//
*/
static int elan_epl2182_I2C_Read(struct i2c_client *client)
{
    uint8_t buffer[RXBYTES];
    int ret = 0, i =0;
    int retry;

    //printk("[ELAN epl2182] %s\n", __func__);

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_recv(client, buffer, RXBYTES);

        if (ret == RXBYTES)
            break;

        APS_ERR("i2c read error,RXBYTES %d\r\n",ret);
        mdelay(10);
    }

    if(retry>=I2C_RETRY_COUNT)
    {
        APS_ERR(KERN_ERR "[ELAN epl2182 error] %s i2c read retry over %d\n",__func__, I2C_RETRY_COUNT);
        return -EINVAL;
    }

    for(i=0; i<PACKAGE_SIZE; i++)
        gRawData.raw_bytes[i] = buffer[i];

    //printk("-----Receive data byte1 (%x) byte2 (%x)-----\n",buffer[0],buffer[1]);


    return ret;
}




static int elan_epl2182_psensor_enable(struct epl2182_priv *epl_data, int enable)
{
    int ret = 0;
    uint8_t regdata;
    struct i2c_client *client = epl_data->client;
    struct epl2182_priv *epld = g_epl2182_ptr;

    APS_LOG("[ELAN epl2182] %s enable = %d\n", __func__, enable);

    epl_data->enable_pflag = enable;
    //modify bye ELAN Robert at 2013/4/23, set LED current from 60uA to 120uA
    ret = elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);
    //ret = elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE | 0x10);

    clear_bit(CMC_BIT_PS, &epld->pending_intr);
	
    if(enable)
    {    
    	//Modify by ELAN Robert at 2013/5/10
        //regdata = EPL_SENSING_8_TIME | EPL_PS_MODE | EPL_H_GAIN ;
regdata = EPL_SENSING_8_TIME | EPL_PS_MODE | EPL_M_GAIN ;
        //regdata = EPL_SENSING_2_TIME | EPL_PS_MODE | EPL_M_GAIN ;
        regdata = regdata | (isInterrupt ? EPL_C_SENSING_MODE : EPL_S_SENSING_MODE);
        ret = elan_epl2182_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

		//Modify by ELAN Robert at 2013/5/10
        //regdata = PS_INTT<<4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
        //regdata = PS_INTT<<4 | EPL_PST_1_TIME | EPL_12BIT_ADC;
regdata = PS_INTT<<4 | EPL_PST_1_TIME | EPL_12BIT_ADC;
        ret = elan_epl2182_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

	//set_psensor_intr_threshold(epl_data->hw ->ps_threshold_low,epl_data->hw ->ps_threshold_high);
	set_psensor_intr_threshold(atomic_read(&epl_data->ps_thd_val_low),atomic_read(&epl_data->ps_thd_val_high));
	
	
        ret = elan_epl2182_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_RESET);
        ret = elan_epl2182_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);
				
		msleep(PS_DELAY);

       // bool enable_ps = test_bit(CMC_BIT_PS, &epl_data->enable) && atomic_read(&epl_data->ps_suspend)==0;
       // bool enable_als = test_bit(CMC_BIT_ALS, &epl_data->enable) && atomic_read(&epl_data->als_suspend)==0;

        if(isInterrupt)
        {
 #if 0
            if(enable_ps && enable_als){
                APS_LOG("[ELAN epl2182] >>>>>>>>>>>enable_ps && enable_als \r\n");
                elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
            }
            else{
                elan_epl2182_I2C_Write(client,REG_13,R_SINGLE_BYTE,0x01,0);
    			elan_epl2182_I2C_Read(client);
    			gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);

                APS_LOG("[ELAN epl2182]: >>>>>>>>>>>>>>>>>>>>>> gRawData.ps_state=%d, gRawData.ps_int_state=%d \r\n", gRawData.ps_state, gRawData.ps_int_state);

    			if(gRawData.ps_state != gRawData.ps_int_state)
    			{
        			//modify bye ELAN Robert at 2013/4/23, set LED current from 60uA to 120uA
        			//elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE | 0x10);
        			elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
    			    APS_LOG("[ELAN epl2182] >>>>>>>>>>>EPL_INT_FRAME_ENABLE \r\n");
    			}
    			else
    			{
    			    //modify bye ELAN Robert at 2013/4/23, set LED current from 60uA to 120uA
            		//elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW | 0x10);
            		APS_LOG("[ELAN epl2182] >>>>>>>>>>>EPL_INT_ACTIVE_LOW \r\n");
            		elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
    			}
            }
#else
            elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
#endif
	    }

    }
    else
    {
        //clear_bit(CMC_BIT_PS, &epld->pending_intr);
	ret = elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);
	ret = elan_epl2182_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_P_DOWN);
	#if 0
        regdata = EPL_SENSING_2_TIME | EPL_PS_MODE | EPL_L_GAIN ;
        regdata = regdata | EPL_S_SENSING_MODE;
        ret = elan_epl2182_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);
        #endif

    }

    if(ret<0)
    {
        APS_ERR("[ELAN epl2182 error]%s: ps enable %d fail\n",__func__,ret);
    }
    else
    {
    	ret = 0;
    }

    return ret;
}


static int elan_epl2182_lsensor_enable(struct epl2182_priv *epl_data, int enable)
{
    int ret = 0;
    uint8_t regdata;
	int mode;
    struct i2c_client *client = epl_data->client;

    APS_LOG("[ELAN epl2182] %s enable = %d\n", __func__, enable);
	
    epl_data->enable_lflag = enable;

    if(enable)
    {
        regdata = EPL_INT_DISABLE;
        ret = elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02, regdata);
#ifdef DYNAMIC_INTT //ices add by 20131222 /*DYNAMIC_INTT*/
/*Lenovo-sw caoyi1 modify for cycle als lux 2014-04-16 start*/
    dynamic_intt_cycle = EPL_SENSING_16_TIME;
    if(dynamic_intt_idx >= 2)
    {
        dynamic_intt_cycle = EPL_SENSING_64_TIME;
    }

    regdata = EPL_S_SENSING_MODE | dynamic_intt_cycle | EPL_ALS_MODE | dynamic_intt_gain;
/*Lenovo-sw caoyi1 modify for cycle als lux 2014-04-16 end*/
    ret = elan_epl2182_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

    regdata = dynamic_intt_intt<<4 | EPL_PST_1_TIME | EPL_8BIT_ADC;
    ret = elan_epl2182_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

#else
        regdata = EPL_S_SENSING_MODE | EPL_SENSING_16_TIME | EPL_ALS_MODE | EPL_AUTO_GAIN;
        ret = elan_epl2182_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

        regdata = ALS_INTT<<4 | EPL_PST_1_TIME | EPL_8BIT_ADC;
        ret = elan_epl2182_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);
#endif
        ret = elan_epl2182_I2C_Write(client,REG_10,W_SINGLE_BYTE,0X02,0x3e);
        ret = elan_epl2182_I2C_Write(client,REG_11,W_SINGLE_BYTE,0x02,0x3e);

		
        ret = elan_epl2182_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_RESET);
        ret = elan_epl2182_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);
		msleep(ALS_DELAY);
    }


    if(ret<0)
    {
        APS_ERR("[ELAN epl2182 error]%s: als_enable %d fail\n",__func__,ret);
    }
    else
    {
    	ret = 0;
    }
	
    return ret;
}

#ifdef DYNAMIC_INTT //ices add by 20131222 /*DYNAMIC_INTT*/
long raw_convert_to_lux(u16 raw_data)
{
    long lux = 0;

    /* lux = C*count*(4096/intt)*(64/Gain), Gain:H=64, M=8, L=1 */
#if 0
    if(dynamic_intt_gain == EPL_H_GAIN){
        lux = c_gain * raw_data * (4096 / als_dynamic_intt_intt_value[dynamic_intt_idx]) * (64 / 64);
    }
    else if(dynamic_intt_gain == EPL_M_GAIN){
        lux = c_gain * raw_data * (4096 / als_dynamic_intt_intt_value[dynamic_intt_idx]) * (64 / 8);
    }
    else if(dynamic_intt_gain == EPL_L_GAIN){
        lux = c_gain * raw_data * (4096 / als_dynamic_intt_intt_value[dynamic_intt_idx]) * (64 / 1);
    }
#else/* lux = C*count*(4096/intt) for epl2182*/
    lux = c_gain * raw_data * (4096 / als_dynamic_intt_intt_value[dynamic_intt_idx]);
    if(lux >= (dynamic_intt_max_lux*1000)){
        APS_LOG("raw_convert_to_lux: change max lux\r\n");
        lux = dynamic_intt_max_lux * 1000;
    }
    else if(lux <= (dynamic_intt_min_lux*1000)){
        APS_LOG("raw_convert_to_lux: change min lux\r\n");
        lux = dynamic_intt_min_lux * 1000;
    }
#endif
    return lux;
}
#endif //ices end by 20131216 /*DYNAMIC_INTT*/


static int epl2182_get_als_value(struct epl2182_priv *obj, u16 als)
{

#ifdef DYNAMIC_INTT //ices add by 20131222 /*DYNAMIC_INTT*/
				uint32_t now_lux, lux_tmp;
        int als_com = 0;
	   // struct epl2182_priv *epld = epl2182_obj;
	    //struct i2c_client *client = epld->client;
        elan_epl2182_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
        elan_epl2182_I2C_Read(obj->client);
        als_com = (gRawData.raw_bytes[0]&0x04)>>2;
        APS_LOG("dynamic_intt_idx=%d, als_dynamic_intt_intt_value=%d, dynamic_intt_gain=%d, als_raw=%d, als_com=%d\r\n",
                                                    dynamic_intt_idx, als_dynamic_intt_intt_value[dynamic_intt_idx], dynamic_intt_gain, als, als_com);

        if(als_com == 1){
            if(dynamic_intt_idx == (als_dynamic_intt_intt_num - 1)){
                lux_tmp = dynamic_intt_max_lux * 1000;
            }
            else{
                als  = dynamic_intt_high_thr;
          		obj->als  = dynamic_intt_high_thr;
				gRawData.als_ch1_raw = dynamic_intt_high_thr;
          		lux_tmp = raw_convert_to_lux(als);
                dynamic_intt_idx++;
            }
            APS_LOG(">>>>>>>>>>>>>>>>>>>>>>>> channel output has saturated! \r\n");

        }
        else{
            if(als > dynamic_intt_high_thr)
        	{
          		if(dynamic_intt_idx == (als_dynamic_intt_intt_num - 1)){
        	      	lux_tmp = dynamic_intt_max_lux * 1000;
        	      	APS_LOG(">>>>>>>>>>>>>>>>>>>>>>>> INTT_MAX_LUX\r\n");
          		}
                else{
					als  = dynamic_intt_high_thr;
              		obj->als  = dynamic_intt_high_thr;
					gRawData.als_ch1_raw = dynamic_intt_high_thr;
              		lux_tmp = raw_convert_to_lux(als);
                    dynamic_intt_idx++;
                    APS_LOG(">>>>>>>>>>>>>>>>>>>>>>>>change INTT high: %d, raw: %d \r\n", dynamic_intt_idx, als);
                }
            }
            else if(als < dynamic_intt_low_thr)
            {
                if(dynamic_intt_idx == 0){
                    lux_tmp = dynamic_intt_min_lux * 1000;
                    APS_LOG(">>>>>>>>>>>>>>>>>>>>>>>> INTT_MIN_LUX\r\n");
                }
                else{
					als  = dynamic_intt_low_thr;
                	obj->als = dynamic_intt_low_thr;
					gRawData.als_ch1_raw = dynamic_intt_low_thr;
                	lux_tmp = raw_convert_to_lux(als);
                    dynamic_intt_idx--;
                    APS_LOG(">>>>>>>>>>>>>>>>>>>>>>>>change INTT low: %d, raw: %d \r\n", dynamic_intt_idx, als);
                }
            }
            else
            {
            	lux_tmp = raw_convert_to_lux(als);
            }
        }

            now_lux = lux_tmp / dynamic_intt_min_unit;

            //now_lux = raw_convert_to_lux(gRawData.als_ch1_raw);

            APS_LOG("-------------------  ALS raw = %d, now_lux = %d   \r\n",  als, now_lux);

            dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
            dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
            dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
            dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];

            return now_lux;
#else
    int idx;
    int invalid = 0;
    for(idx = 0; idx < obj->als_level_num; idx++)
    {
        if(als < obj->hw->als_level[idx])
        {
            break;
        }
    }

    if(idx >= obj->als_value_num)
    {
        APS_ERR("exceed range\n");
        idx = obj->als_value_num - 1;
    }

    if(1 == atomic_read(&obj->als_deb_on))
    {
        unsigned long endt = atomic_read(&obj->als_deb_end);
        if(time_after(jiffies, endt))
        {
            atomic_set(&obj->als_deb_on, 0);
        }

        if(1 == atomic_read(&obj->als_deb_on))
        {
            invalid = 1;
        }
    }

    if(!invalid)
    {
        APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
        return obj->hw->als_value[idx];
    }
    else
    {
        APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
        return -1;
    }
#endif //ices end by 20131216 /*DYNAMIC_INTT*/
}

#ifdef DYNAMIC_INTT
static int epl2182_get_als_value_end(struct epl2182_priv *obj, int als)
{
	int idx;
    int invalid = 0;
    for(idx = 0; idx < obj->als_level_num; idx++)
    {
        if(als <= obj->hw->als_level[idx])
        {
            break;
        }
    }

    if(idx >= obj->als_level_num)
    {
        APS_ERR("exceed range\n");
        idx = obj->als_level_num - 1;
    }

    if(1 == atomic_read(&obj->als_deb_on))
    {
        unsigned long endt = atomic_read(&obj->als_deb_end);
        if(time_after(jiffies, endt))
        {
            atomic_set(&obj->als_deb_on, 0);
        }

        if(1 == atomic_read(&obj->als_deb_on))
        {
            invalid = 1;
        }
    }

    if(!invalid)
    {
        APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
        return obj->hw->als_value[idx];
    }
    else
    {
        APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
        return -1;
    }	

}
#endif

static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
    int ret = 0;
    struct epl2182_priv *epld = epl2182_obj;
    struct i2c_client *client = epld->client;

    uint8_t high_msb ,high_lsb, low_msb, low_lsb;

    APS_LOG("%s\n", __func__);

    high_msb = (uint8_t) (high_thd >> 8);
    high_lsb = (uint8_t) (high_thd & 0x00ff);
    low_msb  = (uint8_t) (low_thd >> 8);
    low_lsb  = (uint8_t) (low_thd & 0x00ff);

    APS_LOG("%s: low_thd = 0x%X, high_thd = 0x%x \n",__func__, low_thd, high_thd);

    elan_epl2182_I2C_Write(client,REG_2,W_SINGLE_BYTE,0x02,high_lsb);
    elan_epl2182_I2C_Write(client,REG_3,W_SINGLE_BYTE,0x02,high_msb);
    elan_epl2182_I2C_Write(client,REG_4,W_SINGLE_BYTE,0x02,low_lsb);
    elan_epl2182_I2C_Write(client,REG_5,W_SINGLE_BYTE,0x02,low_msb);

    return ret;
}



/*----------------------------------------------------------------------------*/
static void epl2182_dumpReg(struct i2c_client *client)
{
  APS_LOG("chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
  APS_LOG("chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
  APS_LOG("chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
  APS_LOG("chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
  APS_LOG("chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
  APS_LOG("chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
  APS_LOG("chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
  APS_LOG("chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
  APS_LOG("chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
  APS_LOG("chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
  APS_LOG("chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
  APS_LOG("chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
  APS_LOG("chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
  APS_LOG("chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
  APS_LOG("chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));

}


/*----------------------------------------------------------------------------*/
int hw8k_init_device(struct i2c_client *client)
{
	APS_LOG("hw8k_init_device.........\r\n");

	epl2182_i2c_client=client;
	
	APS_LOG(" I2C Addr==[0x%x],line=%d\n",epl2182_i2c_client->addr,__LINE__);

	return 0;
}

/*----------------------------------------------------------------------------*/
int epl2182_get_addr(struct alsps_hw *hw, struct epl2182_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	return 0;
}


/*----------------------------------------------------------------------------*/
static void epl2182_power(struct alsps_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	//APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "EPL2182")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "EPL2182")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}



/*----------------------------------------------------------------------------*/
static int epl2182_check_intr(struct i2c_client *client) 
{
	struct epl2182_priv *obj = i2c_get_clientdata(client);
	int mode;

	APS_LOG("int pin = %d\n", mt_get_gpio_in(GPIO_ALS_EINT_PIN));
	
	//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/  
	 //   return 0;

	elan_epl2182_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
	elan_epl2182_I2C_Read(obj->client);
	mode = gRawData.raw_bytes[0]&(3<<4);
	APS_LOG("mode %d\n", mode);

	if(mode==0x10)// PS
	{
		set_bit(CMC_BIT_PS, &obj->pending_intr);
	}
	else
	{
	   	clear_bit(CMC_BIT_PS, &obj->pending_intr);
	}


	if(atomic_read(&obj->trace) & CMC_TRC_DEBUG)
	{
		APS_LOG("check intr: 0x%08X\n", obj->pending_intr);
	}

	return 0;

}



/*----------------------------------------------------------------------------*/

int epl2182_read_als(struct i2c_client *client, u16 *data)
{
    struct epl2182_priv *obj = i2c_get_clientdata(client);
int mode;
    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
        return -1;
    }

	elan_epl2182_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
	elan_epl2182_I2C_Read(obj->client);
	mode = gRawData.raw_bytes[0]&(3<<4);
	if(mode!=0x00){
	printk("read als  in ps  mode %d\n", mode);
	return 0;
		}
	
    elan_epl2182_I2C_Write(obj->client,REG_14,R_TWO_BYTE,0x01,0x00);
    elan_epl2182_I2C_Read(obj->client);
    gRawData.als_ch0_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

    elan_epl2182_I2C_Write(obj->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl2182_I2C_Read(obj->client);
    gRawData.als_ch1_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
    *data =  gRawData.als_ch1_raw;
 epl2182_obj->als=gRawData.als_ch1_raw;
 
	APS_LOG("read als raw data = %d\n", gRawData.als_ch1_raw);
    return 0;
}


/*----------------------------------------------------------------------------*/
long epl2182_read_ps(struct i2c_client *client, u16 *data)
{
    struct epl2182_priv *obj = i2c_get_clientdata(client);
	
    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
        return -1;
    }

    elan_epl2182_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
    elan_epl2182_I2C_Read(obj->client);
    gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);

    elan_epl2182_I2C_Write(obj->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl2182_I2C_Read(obj->client);
    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

    *data = gRawData.ps_raw ;
    APS_LOG("read ps raw data = %d\n", gRawData.ps_raw);
	APS_LOG("read ps binary data = %d\n", gRawData.ps_state);

    return 0;
}



/*----------------------------------------------------------------------------*/
void epl2182_eint_func(void)
{
	struct epl2182_priv *obj = g_epl2182_ptr;
	
	// APS_LOG(" interrupt fuc\n");

	if(!obj)
	{
		return;
	}

    mt_eint_mask(CUST_EINT_ALS_NUM);
	schedule_delayed_work(&obj->eint_work, 0);
}



/*----------------------------------------------------------------------------*/
static void epl2182_eint_work(struct work_struct *work)
{
	struct epl2182_priv *epld = g_epl2182_ptr;
	int err;
	hwm_sensor_data sensor_data;
    //mutex_lock(&sensor_mutex);
    bool enable_als=test_bit(CMC_BIT_ALS, &epld->enable) && atomic_read(&epld->als_suspend)==0;
    bool enable_ps = test_bit(CMC_BIT_PS, &epld->enable) && atomic_read(&epld->ps_suspend)==0;

	APS_LOG("xxxxx eint work\n");
	
	// 0816
set_bit(CMC_BIT_PS, &epld->pending_intr);

	if(test_bit(CMC_BIT_PS, &epld->enable)==0)
		goto exit;

	if((err = epl2182_check_intr(epld->client)))
	{
		APS_ERR("check intrs: %d\n", err);
                goto exit;
	}

	if((1<<CMC_BIT_PS) & epld->pending_intr)
	{
		elan_epl2182_I2C_Write(epld->client,REG_13,R_SINGLE_BYTE,0x01,0);
		elan_epl2182_I2C_Read(epld->client);
		gRawData.ps_int_state= !((gRawData.raw_bytes[0]&0x04)>>2);
		APS_LOG("ps_int_state = %d;ps_state=%d\n", gRawData.ps_int_state, gRawData.ps_state);

	    elan_epl2182_I2C_Write(epld->client,REG_16,R_TWO_BYTE,0x01,0x00);
	    elan_epl2182_I2C_Read(epld->client);
	    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
		APS_LOG("ps raw_data = %d\n", gRawData.ps_raw);

		sensor_data.values[0] = gRawData.ps_int_state;
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;

		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
			APS_ERR("get interrupt data failed\n");
			APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}

#if 0
    //modify bye ELAN Robert at 2013/4/23, set LED current from 60uA to 110uA
	elan_epl2182_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
	//elan_epl2182_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW | 0x10);
	elan_epl2182_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);
#else
/*
    if(enable_ps && enable_als){
        elan_epl2182_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
    }
    else{
        elan_epl2182_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
    }
    elan_epl2182_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);
*/
    elan_epl2182_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
    //elan_epl2182_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
    elan_epl2182_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);

#endif
   // mutex_unlock(&sensor_mutex);
exit:		
   //0816
   clear_bit(CMC_BIT_PS, &epld->pending_intr);
   
	#ifdef MT6516
	MT6516_EINTIRQUnmask(CUST_EINT_ALS_NUM);      
	#endif     

	#ifdef MT6573
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
	#endif

	#ifdef MT6513
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
	#endif
	
	#ifdef MT6575
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
	#endif

	#ifdef MT6577
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
	#endif
	mt_eint_unmask(CUST_EINT_ALS_NUM);
}



/*----------------------------------------------------------------------------*/
int epl2182_setup_eint(struct i2c_client *client)
{
	struct epl2182_priv *obj = i2c_get_clientdata(client);        

	APS_LOG("epl2182_setup_eint\n");
	

	g_epl2182_ptr = obj;

	/*configure to GPIO function, external interrupt*/

	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

#ifdef MT6516
	MT6516_EINT_Set_Sensitivity(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	MT6516_EINT_Set_Polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	MT6516_EINT_Set_HW_Debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	MT6516_EINT_Registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2182_eint_func, 0);
	MT6516_EINTIRQUnmask(CUST_EINT_ALS_NUM);  
#endif

#ifdef MT6513
    mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2182_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
#endif 	

#ifdef MT6573
   	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2182_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
#endif 	

#ifdef  MT6575
    mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2182_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
#endif 	

#ifdef  MT6577
	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2182_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);	
#endif 	
	//mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	//mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM,  CUST_EINTF_TRIGGER_FALLING, epl2182_eint_func, 0);
	mt_eint_unmask(CUST_EINT_ALS_NUM);

    return 0;
}




/*----------------------------------------------------------------------------*/
static int epl2182_init_client(struct i2c_client *client)
{
	struct epl2182_priv *obj = i2c_get_clientdata(client);
	int err=0;

	APS_LOG("[Agold spl] I2C Addr==[0x%x],line=%d\n",epl2182_i2c_client->addr,__LINE__);
	
/*  interrupt mode */


	APS_FUN();

	if(obj->hw->polling_mode_ps == 0)
	{
		mt_eint_mask(CUST_EINT_ALS_NUM);  

		if((err = epl2182_setup_eint(client)))
		{
			APS_ERR("setup eint: %d\n", err);
			return err;
		}
		APS_LOG("epl2182 interrupt setup\n");
	}


	if((err = hw8k_init_device(client)) != 0)
	{
		APS_ERR("init dev: %d\n", err);
		return err;
	}


	if((err = epl2182_check_intr(client)))
	{
		APS_ERR("check/clear intr: %d\n", err);
		return err;
	}


/*  interrupt mode */
 //if(obj->hw->polling_mode_ps == 0)
   //     mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  	

	return err;
}


/*----------------------------------------------------------------------------*/
static ssize_t epl2182_show_reg(struct device_driver *ddri, char *buf)
{
	if(!epl2182_obj)
	{
		APS_ERR("epl2182_obj is null!!\n");
		return 0;
	}
	ssize_t len = 0;
	struct i2c_client *client = epl2182_obj->client;

	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));
	
    return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t epl2182_show_status(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;
    struct epl2182_priv *epld = epl2182_obj;

    if(!epl2182_obj)
    {
        APS_ERR("epl2182_obj is null!!\n");
        return 0;
    }
    elan_epl2182_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_LOCK);

    elan_epl2182_I2C_Write(epld->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl2182_I2C_Read(epld->client);
    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
    APS_LOG("ch1 raw_data = %d\n", gRawData.ps_raw);

    elan_epl2182_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);
    len += snprintf(buf+len, PAGE_SIZE-len, "ch1 raw is %d\n",gRawData.ps_raw);
    return len;
}



/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, epl2182_show_status,  NULL);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, epl2182_show_reg,   NULL);

/*----------------------------------------------------------------------------*/
static struct device_attribute * epl2182_attr_list[] =
{
    &driver_attr_status,
    &driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int epl2182_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(epl2182_attr_list)/sizeof(epl2182_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, epl2182_attr_list[idx]))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", epl2182_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}



/*----------------------------------------------------------------------------*/
static int epl2182_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(epl2182_attr_list)/sizeof(epl2182_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, epl2182_attr_list[idx]);
	}

	return err;
}



/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int epl2182_open(struct inode *inode, struct file *file)
{
	file->private_data = epl2182_i2c_client;

	APS_FUN();

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int epl2182_release(struct inode *inode, struct file *file)
{
        APS_FUN();
	file->private_data = NULL;
	return 0;
}

static int epl2182_ps_average_val = 0;

static void epl2182_WriteCalibration(struct epl2182_priv *obj, HWMON_PS_STRUCT *data_cali)
{
   // struct PS_CALI_DATA_STRUCT *ps_data_cali;
	APS_LOG("le_WriteCalibration  1 %d," ,data_cali->close);
	APS_LOG("le_WriteCalibration  2 %d," ,data_cali->far_away);
	APS_LOG("le_WriteCalibration  3 %d,", data_cali->valid);
	//APS_LOG("le_WriteCalibration  4 %d,", data_cali->pulse);

	if(data_cali->valid == 1)
	{
		atomic_set(&obj->ps_thd_val_high, data_cali->close);
		atomic_set(&obj->ps_thd_val_low, data_cali->far_away);
	}

}
static int epl2182_read_data_for_cali(struct i2c_client *client, HWMON_PS_STRUCT *ps_data_cali)
{
	struct rpr400_priv *obj = i2c_get_clientdata(client);
        u8 databuf[2];
        u8 buffer[2];  
	int i=0 ,res = 0;
	/*Lenovo huangdra 20130713 modify u16 will overflow,change to u32*/
	u16 data[32];
	u32 sum=0, data_cali=0;

	ps_data_cali->valid = 0;

	for(i = 0; i < 10; i++)
	{
		mdelay(40);//50
		res = epl2182_read_ps(client,&data[i]);
		if(res != 0)
		{
			APS_ERR("epl2182_read_data_for_cali fail: %d\n", i); 
			break;
		}
		else
		{
			APS_LOG("[%d]sample = %d\n", i, data[i]);
			sum += data[i];
		}
	 }
	 
				
	if(i < 10)
	{
		res=  -1;

		return res;
	}
	else
	{
		data_cali = sum / 10;
		epl2182_ps_average_val = data_cali;
		APS_LOG("epl2182_read_data_for_cali data = %d",data_cali);
/*Lenovo-sw caoyi1 modify for alston p-sensor calibration 2014-05-08 start*/
#if defined(LENOVO_ALSTON_FDD_P_SENSOR)//A806
		 if( data_cali>3000)
		{
			APS_ERR("epl2182_read_data_for_cali fail value to high: %d\n", data_cali);
			return -2;
		}
		 if((data_cali>=0)&&(data_cali <=250))
		{
		    ps_data_cali->close =data_cali +112;
		    ps_data_cali->far_away =data_cali +47;
		 }
		 else
		{
		     APS_ERR("epl2182_read_data_for_cali IR current 0\n");
			return -2;
		}
		ps_data_cali->valid = 1;
#elif defined(LENOVO_ALSTON_TDD_P_SENSOR)//A808t
		 if( data_cali>3000)
		{
			APS_ERR("epl2182_read_data_for_cali fail value to high: %d\n", data_cali);
			return -2;
		}
/*Lenovo-sw caoyi1 modify for PVT alston p-sensor calibration 2014-06-08 start*/
		 if((data_cali>=0)&&(data_cali <11))//A808T White colour PVT
		{
		    ps_data_cali->close =data_cali +161;
		    ps_data_cali->far_away =data_cali +64;
		 }
               else if((data_cali>=11)&&(data_cali <250))//A808T Black colour PVT
              {
		    ps_data_cali->close =data_cali +172;
		    ps_data_cali->far_away =data_cali +70;
               }
/*Lenovo-sw caoyi1 modify for alston PVT p-sensor calibration 2014-06-08 end*/
		 else
		{
		     APS_ERR("epl2182_read_data_for_cali IR current 0\n");
		     return -2;
		}
		ps_data_cali->valid = 1;
#else
		 if( data_cali>8000)
		{
			APS_ERR("epl2182_read_data_for_cali fail value to high: %d\n", data_cali);
			return -2;
		}
		 if((data_cali>0)&&(data_cali <=10))
		{
		    ps_data_cali->close =data_cali +89;
		    ps_data_cali->far_away =data_cali +38;
		 }
                else if(data_cali <=100)
                {
		    //ps_data_cali->close =data_cali +118;
		    //ps_data_cali->far_away =data_cali +88;
    		    ps_data_cali->close =data_cali +56;
		    ps_data_cali->far_away =data_cali +36;
                }
		 else if(data_cali <=200)
                {
		    //ps_data_cali->close =data_cali +118;
		    //ps_data_cali->far_away =data_cali +88;
    		    ps_data_cali->close =data_cali +102;
		    ps_data_cali->far_away =data_cali +45;
                }
		else if(data_cali<1000)
		{	
		//lenovo-sw molg1 change for anchi 20131119	
		    ps_data_cali->close =data_cali +180;
		    ps_data_cali->far_away =data_cali +86;
		}
		else
		{
		     APS_ERR("epl2182_read_data_for_cali IR current 0\n");
			return -2;
		}
		ps_data_cali->valid = 1;
#endif
/*Lenovo-sw caoyi1 modify for alston p-sensor calibration 2014-05-08 end*/
		APS_LOG("rpr400_read_data_for_cali close = %d,far_away = %d,valid = %d\n",ps_data_cali->close,ps_data_cali->far_away,ps_data_cali->valid);
		//APS_LOG("rpr400_read_data_for_cali avg=%d max=%d min=%d\n",data_cali, max, min);
	}

	return res;
}

static void polling_do_work(struct work_struct *work)
{
    struct epl2182_priv *epld = epl2182_obj;
    struct i2c_client *client = epld->client;

    bool enable_als=test_bit(CMC_BIT_ALS, &epld->enable);
    bool enable_ps=test_bit(CMC_BIT_PS, &epld->enable);

    printk("polling als enable %d, ps enable %d\n\n",enable_als , enable_ps);

    cancel_delayed_work(&polling_work);
#if 0
    if((enable_als && enable_ps) || (enable_ps && isInterrupt == 0) || (enable_als && enable_ps == false)){
        queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(PS_DELAY * 2 + ALS_DELAY));
        //queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(350));
    }
#else
    queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(PS_DELAY * 2 + ALS_DELAY));
    //queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(350));
#endif

//0816
    if(enable_als && atomic_read(&epld->als_suspend)==0 && test_bit(CMC_BIT_PS, &epld->pending_intr)==0)
    {
        elan_epl2182_lsensor_enable(epld, 1);
        epl2182_read_als(client, &gRawData.als_ch1_raw);
        gRawData.als_lux=epl2182_get_als_value(epld,gRawData.als_ch1_raw);
    }
//0816
    if(enable_ps && atomic_read(&epld->ps_suspend)==0  && test_bit(CMC_BIT_PS, &epld->pending_intr)==0)
    {
        elan_epl2182_psensor_enable(epld, 1);
		if(isInterrupt==0)
			{
			epl2182_read_ps(epl2182_obj->client, &epl2182_obj->ps);
			}
    }

    if(!enable_als &&  !enable_ps )
    {
        cancel_delayed_work(&polling_work);
        APS_LOG("disable sensor\n");
        elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);
        elan_epl2182_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,EPL_S_SENSING_MODE);
    }

}
/*----------------------------------------------------------------------------*/
static long epl2182_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct epl2182_priv *obj = i2c_get_clientdata(client);  
	int err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	/*for ps cali work mode support -- by liaoxl.lenovo 2.08.2011 start*/
	HWMON_PS_STRUCT ps_cali_temp;
	/*for ps cali work mode support -- by liaoxl.lenovo 2.08.2011 end*/

	//APS_LOG("---epl2182_ioctll- ALSPS_SET_PS_CALIBRATION  = %x, cmd = %x........\r\n", ALSPS_SET_PS_CALIBRATION, cmd);
	
	switch (cmd)
	{
		case ALSPS_SET_PS_MODE:
          //  mutex_lock(&sensor_mutex);
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
			}

			if(enable)
			{
				if(isInterrupt)
				{
					if((err = elan_epl2182_psensor_enable(obj, 1))!=0)
                    {
                        APS_ERR("enable ps fail: %d\n", err);
                        err= -1;
                    }
				}
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if(isInterrupt)
				{
					if((err = elan_epl2182_psensor_enable(obj, 0))!=0)
                    {
                        APS_ERR("disable ps fail: %d\n", err);
                        err= -1;
                    }
				}
				clear_bit(CMC_BIT_PS, &obj->enable);
			}
          //  mutex_unlock(&sensor_mutex);
			break;


		case ALSPS_GET_PS_MODE:
			enable=test_bit(CMC_BIT_PS, &obj->enable);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;
			

		case ALSPS_GET_PS_DATA: 
         //   mutex_lock(&sensor_mutex);
			if((err = elan_epl2182_psensor_enable(obj, 1))!=0)
            {
                APS_ERR("enable ps fail: %d\n", err);
                err = -1;
            }
	        epl2182_read_ps(obj->client, &obj->ps);
			dat = gRawData.ps_state;

            APS_LOG("ioctl ps state value = %d \n", dat);

			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
			}  
         //   mutex_unlock(&sensor_mutex);
			break;


		case ALSPS_GET_PS_RAW_DATA:  
          //  mutex_lock(&sensor_mutex);
			if((err = elan_epl2182_psensor_enable(obj, 1))!=0)
            {
                APS_ERR("enable ps fail: %d\n", err);
                err= -1;
            }
            epl2182_read_ps(obj->client, &obj->ps);
			dat = gRawData.ps_raw;

            APS_LOG("ioctl ps raw value = %d \n", dat);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
			}  
          //  mutex_unlock(&sensor_mutex);
			break;            


		case ALSPS_SET_ALS_MODE:
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
			}
			if(enable)
			{
#ifdef DYNAMIC_INTT //ices add by 20131222 /*DYNAMIC_INTT*/
				dynamic_intt_idx = dynamic_intt_init_idx;
        dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
        dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
        dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
        dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
#endif //ices end by 20131222 /*DYNAMIC_INTT*/
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				clear_bit(CMC_BIT_ALS, &obj->enable);
			}
			break;



		case ALSPS_GET_ALS_MODE:
			enable=test_bit(CMC_BIT_ALS, &obj->enable);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
			}
			break;



		case ALSPS_GET_ALS_DATA: 
         //   mutex_lock(&sensor_mutex);
            if((err = elan_epl2182_lsensor_enable(obj, 1))!=0)
            {
                APS_ERR("disable als fail: %d\n", err);
                err= -1;
            }

            epl2182_read_als(obj->client, &obj->als);
            dat = epl2182_get_als_value(obj, obj->als);
            APS_LOG("ioctl get als data = %d\n", dat);


            if(obj->enable_pflag && isInterrupt)
            {
                if((err = elan_epl2182_psensor_enable(obj, 1))!=0)
                {
                    APS_ERR("disable ps fail: %d\n", err);
                    err= -1;
                }
            }

             if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
			}              
         //   mutex_unlock(&sensor_mutex);
			break;


		case ALSPS_GET_ALS_RAW_DATA:    
            //mutex_lock(&sensor_mutex);
            if((err = elan_epl2182_lsensor_enable(obj, 1))!=0)
            {
                APS_ERR("disable als fail: %d\n", err);
                err= -1;
            }
			 // mutex_unlock(&sensor_mutex);

            epl2182_read_als(obj->client, &obj->als);
			dat = epl2182_get_als_value(obj, obj->als);//obj->als;
            printk("ioctl get als raw data = %d\n", dat);


            if(obj->enable_pflag && isInterrupt)
            {
           //  mutex_lock(&sensor_mutex);
                if((err = elan_epl2182_psensor_enable(obj, 1))!=0)
                {
                    APS_ERR("disable ps fail: %d\n", err);
                    err= -1;
                }
            }
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
			}              
         
			break;

            case ALSPS_SET_PS_CALI:
			dat = (void __user*)arg;
			if(dat == NULL)
			{
				APS_LOG("dat == NULL\n");
				err = -EINVAL;
				break;	  
			}
			if(copy_from_user(&ps_cali_temp,dat, sizeof(ps_cali_temp)))
			{
				APS_LOG("copy_from_user\n");
				err = -EFAULT;
				break;	  
			}
			epl2182_WriteCalibration(obj, &ps_cali_temp);
			APS_LOG(" ALSPS_SET_PS_CALI %d,%d,%d\t",ps_cali_temp.close,ps_cali_temp.far_away,ps_cali_temp.valid);
			break;

		/*for ps cali work mode support -- by liaoxl.lenovo 2.08.2011 start*/
		case ALSPS_GET_PS_RAW_DATA_FOR_CALI:
		{
			
				cancel_work_sync(&obj->eint_work);
				mt_eint_mask(CUST_EINT_ALS_NUM);

				elan_epl2182_psensor_enable(obj, 1);
				msleep(200);
				err = epl2182_read_data_for_cali(obj->client,&ps_cali_temp);
				if(err < 0 ){
					goto err_out;
				}	

				
				epl2182_WriteCalibration(obj, &ps_cali_temp);

				elan_epl2182_psensor_enable(obj, 0);
				if(copy_to_user(ptr, &ps_cali_temp, sizeof(ps_cali_temp)))
				{
						err = -EFAULT;
						goto err_out;
				}
		}
			break;
			/*for ps cali work mode support -- by liaoxl.lenovo 2.08.2011 end*/
		//lenovo-sw, shanghai, add by chenlj2, for geting ps average val, 2012-05-14 begin
		case ALSPS_GET_PS_AVERAGE:
			enable = epl2182_ps_average_val;
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;
		//lenovo-sw, shanghai, add by chenlj2, for geting ps average val, 2012-05-14 end
                //lenovo-sw, shanghai, add by chenlj2, for AVAGO project, 2012-10-09 start         
                  case ALSPS_GET_PS_FAR_THRESHOLD:
			enable = atomic_read(&obj->ps_thd_val_low);	
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;
	        case ALSPS_GET_PS_CLOSE_THRESHOLD:
			enable = atomic_read(&obj->ps_thd_val_high);	
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;
              //lenovo-sw, shanghai, add by chenlj2, for AVAGO project, 2012-10-09 end

		default:
			APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	err_out:
	return err;    
}


/*----------------------------------------------------------------------------*/
static struct file_operations epl2182_fops =
{
	.owner = THIS_MODULE,
	.open = epl2182_open,
	.release = epl2182_release,
	.unlocked_ioctl = epl2182_unlocked_ioctl,
};


/*----------------------------------------------------------------------------*/
static struct miscdevice epl2182_device =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &epl2182_fops,
};


/*----------------------------------------------------------------------------*/
static int epl2182_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
	APS_FUN();    

	return 0;

}



/*----------------------------------------------------------------------------*/
static int epl2182_i2c_resume(struct i2c_client *client)
{
	APS_FUN();
/*Lenovo-sw caoyi1 modify for light sensor value 2014-05-12 start*/
    als_report_idx = 0;
/*Lenovo-sw caoyi1 modify for light sensor value 2014-05-12 end*/
	return 0;
}



/*----------------------------------------------------------------------------*/
static void epl2182_early_suspend(struct early_suspend *h)
{
    struct epl2182_priv *obj = container_of(h, struct epl2182_priv, early_drv);
	APS_FUN();

    if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

	//atomic_set(&obj->ps_suspend, 1);
    //atomic_set(&obj->als_suspend, 1);
}



/*----------------------------------------------------------------------------*/
static void epl2182_late_resume(struct early_suspend *h)
{  
	/*late_resume is only applied for ALS*/
	struct epl2182_priv *obj = container_of(h, struct epl2182_priv, early_drv); 
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}
/*Lenovo-sw caoyi1 modify for light sensor value 2014-05-12 start*/
	als_report_idx = 0;
/*Lenovo-sw caoyi1 modify for light sensor value 2014-05-12 end*/
/*
	atomic_set(&obj->ps_suspend, 0);
    atomic_set(&obj->als_suspend, 0);

    if(test_bit(CMC_BIT_ALS, &obj->enable) || test_bit(CMC_BIT_PS, &obj->enable)){
        queue_delayed_work(obj->epl_wq, &polling_work,msecs_to_jiffies(5));
    }
*/
}


/*----------------------------------------------------------------------------*/
int epl2182_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct epl2182_priv *obj = (struct epl2182_priv *)self;

	APS_FUN();


	APS_LOG("epl2182_ps_operate command = %x\n",command);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;


		case SENSOR_ENABLE:
          //  mutex_lock(&sensor_mutex);
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				APS_LOG("ps enable = %d\n", value);



	            if(value)
                {
                	//if(isInterrupt)
				//	{
                                        if(isInterrupt)
					{
                                                //modify by chenlj2 at 2013/7/10
						gRawData.ps_int_state = 2;
                                        }
					queue_delayed_work(obj->epl_wq, &polling_work,msecs_to_jiffies(5));

	                //}
                    set_bit(CMC_BIT_PS, &obj->enable);
                }
                else
                {
                                        #if 0
					if(isInterrupt)
					{
                                                //modify by chenlj2 at 2013/7/10
						gRawData.ps_int_state = 2;
						if((err = elan_epl2182_psensor_enable(obj, 0))!=0)
						{
							APS_ERR("disable ps fail: %d\n", err);
							return -1;
						}
					}
                                        #endif
					clear_bit(CMC_BIT_PS, &obj->enable);
                }
			}
          //  mutex_unlock(&sensor_mutex);
			break;



		case SENSOR_GET_DATA:
          //  mutex_lock(&sensor_mutex);
			APS_LOG(" get ps data !!!!!!\n");
            if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
            {
                APS_ERR("get sensor data parameter error!\n");
                err = -EINVAL;
            }
            else
            {
            /*
                if((err = elan_epl2182_psensor_enable(epl2182_obj, 1))!=0)
                {
                    APS_ERR("enable ps fail: %d\n", err);
                    err= -1;
                }

                epl2182_read_ps(epl2182_obj->client, &epl2182_obj->ps);
        */
                APS_LOG("---SENSOR_GET_DATA---\n\n");

                sensor_data = (hwm_sensor_data *)buff_out;
                sensor_data->values[0] =gRawData.ps_state;;
                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
        

            }
            //mutex_unlock(&sensor_mutex);
			break;


		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;



	}

	return err;



}



int epl2182_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	int ret_value;
	hwm_sensor_data* sensor_data;
	struct epl2182_priv *obj = (struct epl2182_priv *)self;

	APS_FUN();
	APS_LOG("epl2182_als_operate command = %x\n",command);

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;
			

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
#ifdef DYNAMIC_INTT //ices add by 20131222 /*DYNAMIC_INTT*/
		dynamic_intt_idx = dynamic_intt_init_idx;
        dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
        dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
        dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
        dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
        als_report_idx = 0;
#endif //ices end by 20131222 /*DYNAMIC_INTT*/
				value = *(int *)buff_in;
				if(value)
				{
				queue_delayed_work(obj->epl_wq, &polling_work,msecs_to_jiffies(5));
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
			}
			break;


		case SENSOR_GET_DATA:
           // mutex_lock(&sensor_mutex);
			APS_LOG("get als data !!!!!!\n");
            if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
            {
                APS_ERR("get sensor data parameter error!\n");
                err = -EINVAL;
            }
            else
            	{


 //if(isInterrupt){
 	
				sensor_data = (hwm_sensor_data *)buff_out;
				//sensor_data->values[0] = epl2182_obj->als;
#ifdef DYNAMIC_INTT
                als_report_idx++;
/*Lenovo-sw caoyi1 modify for light sensor value 2014-05-12 start*/
                if(als_report_idx >= (12*(als_dynamic_intt_intt_num - 1) + 1)){ //als_operate update rate= 9 time/count, 9*3=27
                    sensor_data->values[0] = gRawData.als_lux/10;//epl2182_get_als_value_end(epl2182_obj, gRawData.als_lux);
                    als_report_idx = (12*(als_dynamic_intt_intt_num - 1) + 1);
/*Lenovo-sw caoyi1 modify for light sensor value 2014-05-12 end*/
                }
		else
		{
			sensor_data->values[0] = -1; 
		}
#else
                sensor_data->values[0] = gRawData.als_lux/10; //epl2182_get_als_value(epl2182_obj, epl2182_obj->als)/10;

#endif


                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                APS_LOG("get als data->values[0] = %d /%d \n", sensor_data->values[0], gRawData.als_lux);
				/*
 	}
 else
 	{
 	                if((err = elan_epl2182_lsensor_enable(epl2182_obj, 1))!=0)
                {
                    APS_ERR("disable als fail: %d\n", err);
                    err= -1;
                }



                epl2182_read_als(epl2182_obj->client, &epl2182_obj->als);
		 APS_ERR("chenl333  xxxx ps enable=%d; isinterrupt=%d\n",epl2182_obj->enable_pflag,isInterrupt);
                if(epl2182_obj->enable_pflag && isInterrupt)
                {
                     APS_ERR("chenlj333xxxx ps enable\n");
			
                    if((err = elan_epl2182_psensor_enable(epl2182_obj, 1))!=0)
                    {
                        APS_ERR("disable ps fail: %d\n", err);
                        err= -1;
                    }
			
                }
				                sensor_data = (hwm_sensor_data *)buff_out;
				//sensor_data->values[0] = epl2182_obj->als;
                sensor_data->values[0] = epl2182_get_als_value(epl2182_obj, epl2182_obj->als);
                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                APS_LOG("get als data->values[0] = %d\n", sensor_data->values[0]);
 	}*/
				

				
            }
          //  mutex_unlock(&sensor_mutex);
            break;

		default:
			APS_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;



	}

	return err;

}


/*----------------------------------------------------------------------------*/

static int epl2182_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{    
	strcpy(info->type, EPL2182_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int epl2182_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct epl2182_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;
	APS_FUN();

	epl2182_dumpReg(client);

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));

	epl2182_obj = obj;
	obj->hw = get_cust_alsps_hw();
    mutex_init(&sensor_mutex);

	epl2182_get_addr(obj->hw, &obj->addr);

    epl2182_obj->als_level_num = sizeof(epl2182_obj->hw->als_level)/sizeof(epl2182_obj->hw->als_level[0]);
    epl2182_obj->als_value_num = sizeof(epl2182_obj->hw->als_value)/sizeof(epl2182_obj->hw->als_value[0]);
    BUG_ON(sizeof(epl2182_obj->als_level) != sizeof(epl2182_obj->hw->als_level));
    memcpy(epl2182_obj->als_level, epl2182_obj->hw->als_level, sizeof(epl2182_obj->als_level));
    BUG_ON(sizeof(epl2182_obj->als_value) != sizeof(epl2182_obj->hw->als_value));
    memcpy(epl2182_obj->als_value, epl2182_obj->hw->als_value, sizeof(epl2182_obj->als_value));

	INIT_DELAYED_WORK(&obj->eint_work, epl2182_eint_work);
obj->epl_wq = create_singlethread_workqueue("elan_sensor_wq");
	obj->client = client;

	i2c_set_clientdata(client, obj);	

	atomic_set(&obj->als_debounce, 2000);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 1000);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);

	obj->ps_enable = 0;
	obj->als_enable = 0;
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_thd_level = 2;

	atomic_set(&obj->i2c_retry, 3);

	epl2182_i2c_client = client;

    elan_epl2182_I2C_Write(client,REG_0,W_SINGLE_BYTE,0x02, EPL_S_SENSING_MODE);
	elan_epl2182_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);

	if(err = epl2182_init_client(client))
	{
		goto exit_init_failed;
	}

	/*Add by Elan FAE for Factorymod->####1111# invalid 20130905*/
	obj->pending_intr = 0;

	if(err = misc_register(&epl2182_device))
	{
		APS_ERR("epl2182_device register failed\n");
		goto exit_misc_device_register_failed;
	}
#if 0
	if(err = epl2182_create_attr(&epl2182_alsps_driver.driver))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
#endif
	obj_ps.self = epl2182_obj;

	if( obj->hw->polling_mode_ps == 1)
	{
	  obj_ps.polling = 1;
	  APS_LOG("isInterrupt == false\n");
	}
	else
	{
	  obj_ps.polling = 0;//interrupt mode
	  isInterrupt=true;
	  APS_LOG("isInterrupt == true\n");
	}



	obj_ps.sensor_operate = epl2182_ps_operate;



	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


	obj_als.self = epl2182_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = epl2182_als_operate;
	APS_LOG("als polling mode\n");


	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}



#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = epl2182_early_suspend,
	obj->early_drv.resume   = epl2182_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

     if(isInterrupt)
            epl2182_setup_eint(client);

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&epl2182_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
//	exit_kfree:
	kfree(obj);
	exit:
	epl2182_i2c_client = NULL; 
	#ifdef MT6516        
	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	#endif

	APS_ERR("%s: err = %d\n", __func__, err);
	return err;



}



/*----------------------------------------------------------------------------*/
static int epl2182_i2c_remove(struct i2c_client *client)
{
	int err;	
//	int data;

	if(err = epl2182_delete_attr(&epl2182_i2c_driver.driver))
	{
		APS_ERR("epl2182_delete_attr fail: %d\n", err);
	} 

	if(err = misc_deregister(&epl2182_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}

	epl2182_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}



/*----------------------------------------------------------------------------*/
static int epl2182_probe(struct platform_device *pdev) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	epl2182_power(hw, 1);    

	/* Bob.chen add for if ps run, the system forbid to goto sleep mode. */
	//wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");
	//wake_lock_init(&als_lock, WAKE_LOCK_SUSPEND, "als wakelock");
	//wake_lock_init(&als_lock, WAKE_LOCK_SUSPEND, "als wakelock");
	/* Bob.chen add end. */

	//epl2182_force[0] = hw->i2c_num;

	if(i2c_add_driver(&epl2182_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}



/*----------------------------------------------------------------------------*/
static int epl2182_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	epl2182_power(hw, 0);    

	APS_ERR("EPL2182 remove \n");
	i2c_del_driver(&epl2182_i2c_driver);
	return 0;
}



/*----------------------------------------------------------------------------*/
static struct platform_driver epl2182_alsps_driver = {
	.probe      = epl2182_probe,
	.remove     = epl2182_remove,    
	.driver     = {
		.name  = "als_ps",
		//.owner = THIS_MODULE,
	}
};

/*----------------------------------------------------------------------------*/
static int __init epl2182_init(void)
{
	APS_FUN();
	struct alsps_hw *hw = get_cust_alsps_hw();
	i2c_register_board_info(hw->i2c_num, &i2c_EPL2182, 1);

	if(platform_driver_register(&epl2182_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit epl2182_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&epl2182_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(epl2182_init);
module_exit(epl2182_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Hivemotion haifeng@hivemotion.com");
MODULE_DESCRIPTION("EPL2182 ALPsr driver");
MODULE_LICENSE("GPL");
