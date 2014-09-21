/*****************************************************************************
*                E X T E R N A L      R E F E R E N C E S
******************************************************************************
*/
#include <asm/uaccess.h>
#include <linux/xlog.h>
#include <linux/i2c.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
//#include <platform/mt_reg_base.h>
#include <linux/delay.h>
 
#include "external_codec_driver.h"
 
#define MTK_MODIFY_AUDIODRV 
//#define MT6592_I2SOUT_GPIO
//#define CS4398_GPIO_CONTROL
//#define TPA6141_GPIO_CONTROL
//#define NB3N501_GPIO_CONTROL
 
#define GPIO_BASE (0x10005000)
 
/*****************************************************************************
*                          DEBUG INFO
******************************************************************************
*/
 
static bool ecodec_log_on = true;
 
#define ECODEC_PRINTK(fmt, arg...) \
    do { \
        if (ecodec_log_on) xlog_printk(ANDROID_LOG_INFO,"ECODEC", "%s() "fmt"\n", __func__,##arg); \
    }while (0)
 
/*****************************************************************************
*               For I2C definition
******************************************************************************
*/
 
#define ECODEC_I2C_CHANNEL     (2)        //I2CL: SDA2,SCL2
 
// device address,  write: 10011000, read: 10011001
#define ECODEC_SLAVE_ADDR_PIO2 0x48
#define ECODEC_SLAVE_ADDR_PIO3 0x49
 
#define ECODEC_I2C_DEVNAME "ES9018"
#ifdef MTK_MODIFY_AUDIODRV
#define ES9018_GPIO_STATUS (65)
#define ES9018_GPIO_STATUS_GPIO1_BIT (0)
#define ES9018_GPIO_STATUS_GPIO2_BIT (1)


#define ES9018_GPIO_CON (8)
#define ES9018_GPIO2_OUTPUT1 (15<<4)
#define ES9018_GPIO2_OUTPUT0 (7<<4)
// device address,  write: 10011000, read: 10011001
//#define ECODEC_SLAVE_ADDR_WRITE 0x90
//#define ECODEC_SLAVE_ADDR_READ  0x91
#define ECODEC_SLAVE_ADDR_WRITE 0x92    //mean 0x49, use it because ADDR high
#define ECODEC_SLAVE_ADDR_READ  0x93    


static struct i2c_board_info __initdata  ecodec_dev = {I2C_BOARD_INFO(ECODEC_I2C_DEVNAME, (ECODEC_SLAVE_ADDR_WRITE >> 1))};
#endif
 
static unsigned int default_reg[][2]={
     0,0,
     1,0x8c,
     2,0x18,
     3,0x10,
     4,0,
     5,0x7f,    //0x68 default
     6,0x42,
     7,0x80,
     8,0x10,
     9,0x22,    //0x0 default
     10,0x2d,
     11,0x2,
     12,0x5a,
     13,0,
     14,0x8a,
     15,0,
     16,0,
     17,0xff,
     18,0xff,
     19,0xff,
     20,0x3f,
     21,0,
     22,0,
     23,0,
     24,0,
     25,0xea
};
// I2C variable
static struct i2c_client *new_client = NULL;

//#define DISABLE_HIFI_TMPLY //Added by jrd.lipeng: p-sensor conflicts with hifi, so disable hifi temporarily.

static int g_i2c_probe_ret = 0;
static int g_i2c_probe_chipid = 0;

 
// new I2C register method
static const struct i2c_device_id ecodec_i2c_id[] = {{ECODEC_I2C_DEVNAME, 0}, {}};
 
//function declration
static int ecodec_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int ecodec_i2c_remove(struct i2c_client *client);
//i2c driver
struct i2c_driver ecodec_i2c_driver =
{
    .probe = ecodec_i2c_probe,
    .remove = ecodec_i2c_remove,
    .driver = {
        .owner          = THIS_MODULE,
        .name           = ECODEC_I2C_DEVNAME,
    },
    .id_table = ecodec_i2c_id,
};
 
// function implementation
//read one register
ssize_t static ecodec_read_byte(u8 addr, u8 *returnData)
{
    char        cmd_buf[1] = {0x00};
    char        readData = 0;
    int   ret = 0;
    if (!new_client)
    {
        ECODEC_PRINTK("ecodec_read_byte I2C client not initialized!! addr 0x%x",addr);
        return -1;
    }
    cmd_buf[0] = addr;
    ret = i2c_master_send(new_client, &cmd_buf[0], 1);
    if (ret < 0)
    {
        ECODEC_PRINTK("ecodec_read_byte read sends command error!! addr 0x%x",addr);
        return -1;
    }
    ret = i2c_master_recv(new_client, &readData, 1);
    if (ret < 0)
    {
        ECODEC_PRINTK("ecodec_read_byte reads recv data error!! addr 0x%x",addr);
        return -1;
    }
    *returnData = readData;    
    ECODEC_PRINTK("addr 0x%x data 0x%x", addr, readData);    
    return 0;
}
 
//read one register
static u8   I2CRead(u8 addr)
{
    u8 regvalue = 0;
    ecodec_read_byte(addr, &regvalue);
    return regvalue;
}
 
// write register
static ssize_t  I2CWrite(u8 addr, u8 writeData)
{
    char      write_data[2] = {0};
    int  ret = 0;        
    
    if (!new_client)
    {
        ECODEC_PRINTK("I2C client not initialized!!addr 0x%x data 0x%x", addr, writeData);
        return -1;
    }
    write_data[0] = addr;         // ex. 0x01
    write_data[1] = writeData;
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0)
    {
        ECODEC_PRINTK("write sends command error!!addr 0x%x data 0x%x", addr, writeData);
        return -1;
    }
    ECODEC_PRINTK("addr 0x%x data 0x%x", addr, writeData);
    return 0;
}
 
static void  ecodec_set_hw_parameters(DIGITAL_INTERFACE_FORMAT dif)
{
    //MCLKDIV, SpeedMode, format
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_set_hw_parameters (+) dif=%d", dif);
         int i;
        
         for(i=0;i<sizeof(default_reg)/(2*sizeof(int));i++){
                   I2CWrite(default_reg[i][0], default_reg[i][1]);
         }
        

    ECODEC_PRINTK("ecodec_set_hw_parameters (-)");
}
 
static void  ecodec_set_control_port(u8 enable)
{
    //CPEN
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_set_control_port (+), enable=%d ", enable);
/*
    reg = I2CRead(CS4398_MISC1);
    reg &= ~CS4398_CPEN_MASK;
 
    if (enable == 1)
    {
        reg |= (0x40 & CS4398_CPEN_MASK);
        I2CWrite(CS4398_MISC1, reg); //CPEN (6), 1: Control port enable
    }
    else
    {
        reg |= (0x00 & CS4398_CPEN_MASK);
        I2CWrite(CS4398_MISC1, reg); //CPEN (6), 0: Control port disable
    }*/
    //ECODEC_PRINTK("ecodec_set_control_port(-)");
}
 
static void  ecodec_set_power(u8 enable)
{
    //PDN
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_set_power(+) enable=%d ", enable);
/*
    reg = I2CRead(CS4398_MISC1);
 
    reg &= ~CS4398_PDN_MASK;
 
    if (enable == 1)
    {
        reg |= (0x00 & CS4398_PDN_MASK);
        I2CWrite(CS4398_MISC1, reg); //PDN (7), 0: Power down disable
    }
    else
    {
        reg |= (0x80 & CS4398_PDN_MASK);
        I2CWrite(CS4398_MISC1, reg); //PDN (7), 1: Power down enable
    }*/
    //ECODEC_PRINTK("ecodec_set_power(-)");
}
 
static void  ecodec_mute(u8 enable)
{
    //MUTE
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_mute CS4398_MUTE(+), enable=%d ", enable);
/*
    reg = I2CRead(CS4398_MUTE);
    reg &= ~CS4398_MUTEAB_MASK;
 
    if (enable == 1)
    {
        reg |= (0x18 & CS4398_MUTEAB_MASK);
        I2CWrite(CS4398_MUTE, reg); //MUTE_A/MUTEB (4:3), 1: Mute
    }
    else
    {
        reg |= (0x00 & CS4398_MUTEAB_MASK);
        I2CWrite(CS4398_MUTE, reg); //MUTE_A/MUTEB (4:3), 0: Unmute
    }*/
    //ECODEC_PRINTK("ecodec_mute CS4398_MUTE(-)");
}
 
static void  ecodec_set_volume(u8 leftright, u8 gain)
{
    //Volume
    // 0: 0dB, 6: -3dB, ..., 255: -127.5dB
    ECODEC_PRINTK("ecodec_set_volume leftright=%d, gain=%d ", leftright, gain);
/*
    if (leftright == 0) //left
    {
        I2CWrite(CS4398_VOLA, gain & CS4398_VOLUME_MASK);
    }
    else if (leftright == 1) //right
    {
        I2CWrite(CS4398_VOLB, gain & CS4398_VOLUME_MASK);
    }*/
}
 
void ecodec_dump_register(void)
{
    int i = 0;

    ECODEC_PRINTK("ecodec_dump_register");
    for(i = 0; i <= ES9018_REG_END; i++) {
        ECODEC_PRINTK("ES9018_REG#%d = 0x%x ", i, I2CRead(i));
    }
}
 
u8 ExtCodec_ReadReg(u8 addr)
{
    if(addr < ES9018_REG_FIRST || addr > ES9018_REG_END) {
        ECODEC_PRINTK("[%s]wrong addr: %u", __func__, addr);
        return -1;
    }

    return I2CRead(addr);
}

int ExtCodec_WriteReg(u8 addr, u8 val)
{
    if(addr < ES9018_REG_FIRST || addr > ES9018_REG_END) {
        ECODEC_PRINTK("[%s]wrong addr: %u", __func__, addr);
        return -1;
    }

    return I2CWrite(addr, val);
} 
 
 
static void ecodec_suspend(void)
{
    ECODEC_PRINTK("ecodec_suspend");
    //ecodec_resetRegister();
}
 
static void ecodec_resume(void)
{
    ECODEC_PRINTK("ecodec_resume");
}
 
static int ecodec_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = -1;
    volatile u8 reg;
    ECODEC_PRINTK("+ecodec_i2c_probe");
    new_client = client;
    //new_client->timing = 400;
    ECODEC_PRINTK("client timing=%dK ", new_client->timing);
 
    ret = ecodec_read_byte(64, &reg) ;
    g_i2c_probe_chipid = reg = (reg >> 2) & 0x7;
    ECODEC_PRINTK("Find ChipID 0x%x  !!", reg);
    
    //when probe done, switch off the chip  
    
    cust_extHPAmp_gpio_off();
    //hwPowerDown(MT6323_POWER_LDO_VGP1, "Audio");
    mt_set_gpio_mode(GPIO_HIFI_VCCA_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_VCCA_EN_PIN, GPIO_OUT_ZERO);//VCCA, 3,3v
    mt_set_gpio_mode(GPIO_HIFI_AVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_AVCC_EN_PIN, GPIO_OUT_ZERO);//AVCC_R & AVCC_L, 3.3v
    mt_set_gpio_mode(GPIO_HIFI_DVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_DVCC_EN_PIN, GPIO_OUT_ZERO);//DVCC, 1.8v

    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied low
        

    g_i2c_probe_ret = ret;
    ECODEC_PRINTK("-ecodec_i2c_probe");
    return ret;
}
 
static int ecodec_i2c_remove(struct i2c_client *client)
{
    ECODEC_PRINTK("+ecodec_i2c_remove");
    new_client = NULL;
    i2c_unregister_device(client);
    i2c_del_driver(&ecodec_i2c_driver);
    ECODEC_PRINTK("-ecodec_i2c_remove");
    return 0;
}

static int ecodec_check_register(unsigned short addr)
{
    int i = 0;
    struct i2c_adapter *adapter = NULL;
    struct i2c_client * client = NULL;
    struct i2c_board_info c = {I2C_BOARD_INFO(ECODEC_I2C_DEVNAME, addr)};
    
    adapter = i2c_get_adapter(ECODEC_I2C_CHANNEL);
    if (adapter) {
        client = i2c_new_device(adapter, &c);
        if(g_i2c_probe_ret < 0 || client == NULL) {
            ECODEC_PRINTK("Can not register HIFI through addr: 0x%x\n", c.addr);
            return -1;
        } else {
            ECODEC_PRINTK("HIFI registered with addr 0x%x successed.\n", c.addr);
            return 0;
        }
        i2c_put_adapter(adapter);
    } else {
        ECODEC_PRINTK("Can not get adapter %d\n", ECODEC_I2C_CHANNEL);
        return -1;
    }
}

static int ecodec_register(void)
{
    ECODEC_PRINTK("+ecodec_register");
    
    //hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_3300, "Audio");
    /* ES9018K2M needs 4 LDOs: three 3.3v for VCCA, AVCC_R, AVCC_L &  one 1.8v for DVCC. 
     * These 4 LDOs are provided by 4 voltage regulator IC.*/
    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied low
    
    mt_set_gpio_mode(GPIO_HIFI_DVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_DVCC_EN_PIN, GPIO_OUT_ONE);//DVCC, 1.8v
    mt_set_gpio_mode(GPIO_HIFI_AVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_AVCC_EN_PIN, GPIO_OUT_ONE);//AVCC_R & AVCC_L, 3.3v
    mt_set_gpio_mode(GPIO_HIFI_VCCA_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_VCCA_EN_PIN, GPIO_OUT_ONE);//VCCA, 3,3v
    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ONE); // RST tied high


    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ONE); // RST tied low
    msleep(1);
#ifdef MTK_MODIFY_AUDIODRV
    i2c_register_board_info(ECODEC_I2C_CHANNEL, &ecodec_dev, 1);
#endif
    if (i2c_add_driver(&ecodec_i2c_driver))
    {
        ECODEC_PRINTK("fail to add device into i2c");
        ECODEC_PRINTK("-ecodec_register");    
        return -1;
    }
    
#ifdef MTK_MODIFY_AUDIODRV
    //DO Nothing
#else
#if 1//Added by jrd.lipeng 
    /* 1. i2c address conflict issue of p-sensor & hifi; 
     * 2.the compatibility of different hifi i2c addresses on PIO2 & PIO3 */
    int level = 0;

    /*Judge the HW board version through the input level of GPIO8: high >= PIO3; low: < PIO3*/
    mt_set_gpio_mode(GPIO_HW_VER_CHECK_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_HW_VER_CHECK_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_HW_VER_CHECK_PIN, GPIO_PULL_DISABLE);

    level = mt_get_gpio_in(GPIO_HW_VER_CHECK_PIN);
    ECODEC_PRINTK("GPIO_HW_VER_CHECK_PIN(0x%x) input: %d", GPIO_HW_VER_CHECK_PIN, level);
    if(level == GPIO_IN_ZERO) {//HW version < PIO3
        if(ecodec_check_register(ECODEC_SLAVE_ADDR_PIO2)) {
            if(ecodec_check_register(ECODEC_SLAVE_ADDR_PIO3)) {
                goto err;
            }
        } 
    } else {//HW version >= PIO3
        if(ecodec_check_register(ECODEC_SLAVE_ADDR_PIO3)) {
            if(ecodec_check_register(ECODEC_SLAVE_ADDR_PIO2)) {
                goto err;
            }
        } 
    }
#endif
#endif
    ECODEC_PRINTK("-ecodec_register");
    return 0;
err:
    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied low

    //hwPowerDown(MT6323_POWER_LDO_VGP1, "Audio");
    mt_set_gpio_mode(GPIO_HIFI_VCCA_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_VCCA_EN_PIN, GPIO_OUT_ZERO);//VCCA, 3,3v
    mt_set_gpio_mode(GPIO_HIFI_AVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_AVCC_EN_PIN, GPIO_OUT_ZERO);//AVCC_R & AVCC_L, 3.3v
    mt_set_gpio_mode(GPIO_HIFI_DVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_DVCC_EN_PIN, GPIO_OUT_ZERO);//DVCC, 1.8v
    
    cust_extHPAmp_gpio_off();
    return -1;
}
#if 0
static ssize_t ecodec_resetRegister(void)
{
    // set registers to default value
    ECODEC_PRINTK("ecodec_resetRegister");
    I2CWrite(CS4398_MODE, 0x00);
    I2CWrite(CS4398_MIXING, 0x09);
    I2CWrite(CS4398_MUTE, 0xC0);
    I2CWrite(CS4398_VOLA, 0x18); //default -12dB
    I2CWrite(CS4398_VOLB, 0x18); //default -12dB
    I2CWrite(CS4398_RAMP, 0xb0);
    I2CWrite(CS4398_MISC1, 0x80);
    I2CWrite(CS4398_MISC2, 0x08);
    return 0;
}
#endif
 
/*****************************************************************************
*                  F U N C T I O N        D E F I N I T I O N
******************************************************************************
*/
 
void ExtCodec_Init()
{
    ECODEC_PRINTK("ExtCodec_Init ");
}
 
void ExtCodec_PowerOn(void)
{
    ecodec_set_control_port(1);
    ecodec_set_hw_parameters(DIF_I2S); //Need not power enable(PDN) when R/W register, onlt need turn on CPEN
    ecodec_set_volume(0, 14); //default -7dB
    ecodec_set_volume(1, 14);
    ecodec_set_power(1);
    //ExtCodec_DumpReg();
}
 
void ExtCodec_PowerOff(void)
{
    ecodec_set_power(0);
}
 
bool ExtCodec_Register(void)
{
    int ret = -1;
    ECODEC_PRINTK("ExtCodec_Register ");
    ret = ecodec_register();
    return ret == 0 ? true : false;
}
 
void ExtCodec_Mute(void)
{
    ecodec_mute(1);
}
 
void ExtCodec_SetGain(u8 leftright, u8 gain)
{
    ecodec_set_volume(leftright, gain);
}
 
void ExtCodec_DumpReg(void)
{
    ecodec_dump_register();
}
 
int ExternalCodec(void)
{
    return 1;
}
 
void ExtCodecDevice_Suspend(void)
{
    ecodec_suspend();
}
 
void ExtCodecDevice_Resume(void)
{
    ecodec_resume();
}
 
/*
GPIO12: GPIO_AUD_EXTHP_EN_PIN
GPIO13: GPIO_AUD_EXTHP_GAIN_PIN
GPIO43: GPIO_AUD_EXTDAC_PWREN_PIN
GPIO92: GPIO_AUD_EXTDAC_RST_PIN
GPIO105: GPIO_I2S0_DAT_PIN
GPIO106: GPIO_I2S0_WS_PIN
GPIO107: GPIO_I2S0_CK_PIN
GPIO167: GPIO_AUD_EXTPLL_S0_PIN
GPIO168: GPIO_AUD_EXTPLL_S1_PIN
*/
void cust_extcodec_gpio_on()
{
    ECODEC_PRINTK("+Set GPIO for ES9018 ON [ChipID]:[0x%x]",g_i2c_probe_chipid);

    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied high

    mt_set_gpio_mode(GPIO_HIFI_VCCA_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_VCCA_EN_PIN, GPIO_OUT_ONE);//VCCA, 3,3v
    mt_set_gpio_mode(GPIO_HIFI_AVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_AVCC_EN_PIN, GPIO_OUT_ONE);//AVCC_R & AVCC_L, 3.3v
    mt_set_gpio_mode(GPIO_HIFI_DVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_DVCC_EN_PIN, GPIO_OUT_ONE);//DVCC, 1.8v

    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ONE); // RST tied high
    ECODEC_PRINTK("-Set GPIO for ES9018 ON");
}

 
void cust_extcodec_gpio_off()
{
    ECODEC_PRINTK("+Set GPIO for ES9018 OFF "); 
    mt_set_gpio_mode(GPIO_HIFI_VCCA_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_VCCA_EN_PIN, GPIO_OUT_ZERO);//VCCA, 3,3v
    mt_set_gpio_mode(GPIO_HIFI_AVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_AVCC_EN_PIN, GPIO_OUT_ZERO);//AVCC_R & AVCC_L, 3.3v
    mt_set_gpio_mode(GPIO_HIFI_DVCC_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_HIFI_DVCC_EN_PIN, GPIO_OUT_ZERO);//DVCC, 1.8v
    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied low
    ECODEC_PRINTK("-Set GPIO for ES9018 OFF "); 
}

#if 0//lipeng debug
static unsigned int switch_delay = 1;//unit: ms
void jrd_set_switch_delay(unsigned int ms)
{
    ECODEC_PRINTK("pre-switch_delay: %u, cur-switch_delay: %u", switch_delay, ms);
    switch_delay = ms;
}
#endif
 
void cust_extHPAmp_gpio_on()
{
    //external HPAmp, gain set to 0dB, AMP set to enable
#ifdef TPA6141_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for TPA6141 ON  ");
    mt_set_gpio_mode(GPIO_AUD_EXTHP_GAIN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_GAIN_PIN, GPIO_OUT_ZERO); // fixed at 0dB
    mt_set_gpio_mode(GPIO_AUD_EXTHP_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_EN_PIN, GPIO_OUT_ONE); // enable HP amp
#else
    ECODEC_PRINTK("+Set GPIO for MAX97220 ON ");
#ifdef MTK_MODIFY_AUDIODRV
    volatile u8 reg;
    reg = I2CRead(ES9018_GPIO_STATUS);
    ECODEC_PRINTK("B4 On GPIO2 Status [%x]",(reg>>ES9018_GPIO_STATUS_GPIO2_BIT)&0x01);
    
    reg = I2CRead(ES9018_GPIO_CON);
    reg &= ~0xf0;
    reg |= ES9018_GPIO2_OUTPUT1;
    I2CWrite(ES9018_GPIO_CON, reg);
    
    reg = I2CRead(ES9018_GPIO_STATUS);
    ECODEC_PRINTK("After On GPIO2 Status [%x]",(reg>>ES9018_GPIO_STATUS_GPIO2_BIT)&0x01);
#else
    mt_set_gpio_mode(GPIO_AUD_EXTHP_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_EN_PIN, GPIO_OUT_ONE); // enable HP amp
#endif
    
#if 0//lipeng debug
    int times = 0;
    while((I2CRead(64) & (1 << 0)) == 0) {
        times++;
        msleep(20);
        if(times>10)
            break;
    }
    ECODEC_PRINTK("The Jitter Eliminator is locked. Reg64: 0x%x. times: %d", I2CRead(64), times);
#endif
    msleep(1);//lipeng debug

    /*GPIO_SWITCH1_1V8_PIN: HIGH <-> PMIC; LOW <-> HIFI*/
    mt_set_gpio_mode(GPIO_SWITCH1_1V8_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_SWITCH1_1V8_PIN, GPIO_OUT_ZERO);
    ECODEC_PRINTK("-Set GPIO for MAX97220 ON ");
#endif
    
}
 
void cust_extHPAmp_gpio_off()
{
    /*GPIO_SWITCH1_1V8_PIN: HIGH <-> PMIC; LOW <-> HIFI*/
//    mt_set_gpio_mode(GPIO_SWITCH1_1V8_PIN, GPIO_MODE_00);
//    mt_set_gpio_out(GPIO_SWITCH1_1V8_PIN, GPIO_OUT_ONE);

    //external HPAmp, AMP set to disable, gain set to 0dB
#ifdef TPA6141_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for TPA6141 OFF ");
    mt_set_gpio_mode(GPIO_AUD_EXTHP_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_EN_PIN, GPIO_OUT_ZERO); // disable HP amp
#else
    ECODEC_PRINTK("+Set GPIO for MAX97220 OFF ");
#ifdef MTK_MODIFY_AUDIODRV
    volatile u8 reg;
    reg = I2CRead(ES9018_GPIO_STATUS);
    ECODEC_PRINTK("B4 Off GPIO2 Status [%x]",(reg>>ES9018_GPIO_STATUS_GPIO2_BIT)&0x01);
    
    reg = I2CRead(ES9018_GPIO_CON);
    reg &= ~0xf0;
    reg |= ES9018_GPIO2_OUTPUT0;
    I2CWrite(ES9018_GPIO_CON, reg); 

    reg = I2CRead(ES9018_GPIO_STATUS);
    ECODEC_PRINTK("After Off GPIO2 Status [%x]",(reg>>ES9018_GPIO_STATUS_GPIO2_BIT)&0x01);
#else
    mt_set_gpio_mode(GPIO_AUD_EXTHP_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_EN_PIN, GPIO_OUT_ZERO); // disable HP amp
#endif
    ECODEC_PRINTK("-Set GPIO for MAX97220 OFF ");
#endif
}
 
void cust_extPLL_gpio_config()
{
    //external PLL, set S0/S1
#ifdef NB3N501_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for NB3N501 ");
    mt_set_gpio_mode(GPIO_AUD_EXTPLL_S0_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTPLL_S0_PIN, GPIO_OUT_ONE); // S0 = 1
    mt_set_gpio_mode(GPIO_AUD_EXTPLL_S1_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTPLL_S1_PIN, GPIO_OUT_ONE);   // S1 = 1
#endif
 
}

