#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_i2c.h>
#include <platform/bq24158.h>
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <printf.h>

int g_bq24158_log_en=0;

/* High battery support */
//#define HIGH_BATTERY_VOLTAGE_SUPPORT

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define bq24158_SLAVE_ADDR_WRITE   0xD4
#define bq24158_SLAVE_ADDR_READ    0xD5

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
#define bq24158_REG_NUM 7  
kal_uint8 bq24158_reg[bq24158_REG_NUM] = {0};

#define BQ24158_I2C_ID	I2C1
static struct mt_i2c_t bq24158_i2c;

/**********************************************************
  *
  *   [I2C Function For Read/Write bq24158] 
  *
  *********************************************************/
kal_uint32 bq24158_write_byte(kal_uint8 addr, kal_uint8 value)
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint8 write_data[2];
    kal_uint16 len;

    write_data[0]= addr;
    write_data[1] = value;

    bq24158_i2c.id = BQ24158_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set FAN5405 I2C address to >>1 */
    bq24158_i2c.addr = (bq24158_SLAVE_ADDR_WRITE >> 1);
    bq24158_i2c.mode = ST_MODE;
    bq24158_i2c.speed = 100;
    len = 2;

    ret_code = i2c_write(&bq24158_i2c, write_data, len);
    //dprintf(INFO, "%s: i2c_write: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

kal_uint32 bq24158_read_byte (kal_uint8 addr, kal_uint8 *dataBuffer) 
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint16 len;
    *dataBuffer = addr;

    bq24158_i2c.id = BQ24158_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set BQ24158 I2C address to >>1 */
    bq24158_i2c.addr = (bq24158_SLAVE_ADDR_READ >> 1);
    bq24158_i2c.mode = ST_MODE;
    bq24158_i2c.speed = 100;
    len = 1;

    ret_code = i2c_write_read(&bq24158_i2c, dataBuffer, len, len);
    //dprintf(INFO, "%s: i2c_read: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 bq24158_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq24158_reg = 0;
    int ret = 0;

    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = bq24158_read_byte(RegNum, &bq24158_reg);
    dprintf(INFO, "[bq24158_read_interface] Reg[%x]=0x%x\n", RegNum, bq24158_reg);
    
    bq24158_reg &= (MASK << SHIFT);
    *val = (bq24158_reg >> SHIFT);    

    dprintf(INFO, "[bq24158_read_interface] val=0x%x\n", *val);

    if(g_bq24158_log_en>1)		  
	    dprintf(INFO, "%d\n", ret);

    return ret;
}

kal_uint32 bq24158_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq24158_reg = 0;
    kal_uint32 ret = 0;

    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = bq24158_read_byte(RegNum, &bq24158_reg);
    printf("[bq24158_config_interface] Reg[%x]=0x%x\n", RegNum, bq24158_reg);
    
    bq24158_reg &= ~(MASK << SHIFT);
    bq24158_reg |= (val << SHIFT);

    ret = bq24158_write_byte(RegNum, bq24158_reg);
    dprintf(INFO, "[bq24158_config_interface] write Reg[%x]=0x%x\n", RegNum, bq24158_reg);

    // Check
    //bq24158_read_byte(RegNum, &bq24158_reg);
    //dprintf(INFO, "[bq24158_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq24158_reg);

    if(g_bq24158_log_en>1)        
        dprintf(INFO, "%d\n", ret);

    return ret;
}

/**********************************************************
  *
  *   [bq24158 Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------

void bq24158_set_tmr_rst(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_TMR_RST_MASK),
                                    (kal_uint8)(CON0_TMR_RST_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	
}

kal_uint32 bq24158_get_otg_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_OTG_MASK),
                                    (kal_uint8)(CON0_OTG_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

void bq24158_set_en_stat(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_EN_STAT_MASK),
                                    (kal_uint8)(CON0_EN_STAT_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

kal_uint32 bq24158_get_chip_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_STAT_MASK),
                                    (kal_uint8)(CON0_STAT_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

kal_uint32 bq24158_get_boost_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_BOOST_MASK),
                                    (kal_uint8)(CON0_BOOST_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

kal_uint32 bq24158_get_fault_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_FAULT_MASK),
                                    (kal_uint8)(CON0_FAULT_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

//CON1----------------------------------------------------

void bq24158_set_input_charging_current(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LIN_LIMIT_MASK),
                                    (kal_uint8)(CON1_LIN_LIMIT_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_v_low(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LOW_V_MASK),
                                    (kal_uint8)(CON1_LOW_V_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_te(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_TE_MASK),
                                    (kal_uint8)(CON1_TE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_ce(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_CE_MASK),
                                    (kal_uint8)(CON1_CE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_hz_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_HZ_MODE_MASK),
                                    (kal_uint8)(CON1_HZ_MODE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_opa_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_OPA_MODE_MASK),
                                    (kal_uint8)(CON1_OPA_MODE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

//CON2----------------------------------------------------

void bq24158_set_oreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OREG_MASK),
                                    (kal_uint8)(CON2_OREG_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_otg_pl(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OTG_PL_MASK),
                                    (kal_uint8)(CON2_OTG_PL_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_otg_en(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OTG_EN_MASK),
                                    (kal_uint8)(CON2_OTG_EN_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

//CON3----------------------------------------------------

kal_uint32 bq24158_get_vender_code(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_VENDER_CODE_MASK),
                                    (kal_uint8)(CON3_VENDER_CODE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

kal_uint32 bq24158_get_pn(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_PIN_MASK),
                                    (kal_uint8)(CON3_PIN_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

kal_uint32 bq24158_get_revision(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_REVISION_MASK),
                                    (kal_uint8)(CON3_REVISION_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
    
    return val;
}

//CON4----------------------------------------------------

void bq24158_set_reset(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_RESET_MASK),
                                    (kal_uint8)(CON4_RESET_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_iocharge(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_I_CHR_MASK),
                                    (kal_uint8)(CON4_I_CHR_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_iterm(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_I_TERM_MASK),
                                    (kal_uint8)(CON4_I_TERM_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

//CON5----------------------------------------------------

void bq24158_set_dis_vreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_DIS_VREG_MASK),
                                    (kal_uint8)(CON5_DIS_VREG_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

void bq24158_set_io_level(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_IO_LEVEL_MASK),
                                    (kal_uint8)(CON5_IO_LEVEL_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);		
}

kal_uint32 bq24158_get_sp_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON5), 
                                    (&val),
                                    (kal_uint8)(CON5_SP_STATUS_MASK),
                                    (kal_uint8)(CON5_SP_STATUS_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	
    
    return val;
}

kal_uint32 bq24158_get_en_level(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24158_read_interface(     (kal_uint8)(bq24158_CON5), 
                                    (&val),
                                    (kal_uint8)(CON5_EN_LEVEL_MASK),
                                    (kal_uint8)(CON5_EN_LEVEL_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	
    
    return val;
}

void bq24158_set_vsp(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_VSP_MASK),
                                    (kal_uint8)(CON5_VSP_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	
}

//CON6----------------------------------------------------

void bq24158_set_i_safe(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_ISAFE_MASK),
                                    (kal_uint8)(CON6_ISAFE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	
}

void bq24158_set_v_safe(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24158_config_interface(   (kal_uint8)(bq24158_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_VSAFE_MASK),
                                    (kal_uint8)(CON6_VSAFE_SHIFT)
                                    );
    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
//debug liao
unsigned int bq24158_config_interface_liao (unsigned char RegNum, unsigned char val)
{
    int ret = 0;
    
    ret = bq24158_write_byte(RegNum, val);

    if(g_bq24158_log_en>1)        
        dprintf(CRITICAL, "%d\n", ret);	

    return ret;
}

void bq24158_dump_register(void)
{
    int i=0;

    for (i=0;i<bq24158_REG_NUM;i++)
    {
        bq24158_read_byte(i, &bq24158_reg[i]);
        dprintf(INFO, "[0x%x]=0x%x\n", i, bq24158_reg[i]);        
    }
}

void bq24158_read_register(int i)
{
    bq24158_read_byte(i, &bq24158_reg[i]);
    dprintf(INFO, "[0x%x]=0x%x\n", i, bq24158_reg[i]); 
}

#if 1
#include <cust_gpio_usage.h>
int gpio_number   = GPIO_SWCHARGER_EN_PIN; 
int gpio_off_mode = GPIO_SWCHARGER_EN_PIN_M_GPIO;
int gpio_on_mode  = GPIO_SWCHARGER_EN_PIN_M_GPIO;
#else
int gpio_number   = (19 | 0x80000000); 
int gpio_off_mode = 0;
int gpio_on_mode  = 0;
#endif
int gpio_off_dir  = GPIO_DIR_OUT;
int gpio_off_out  = GPIO_OUT_ONE;
int gpio_on_dir   = GPIO_DIR_OUT;
int gpio_on_out   = GPIO_OUT_ZERO;

void bq24158_turn_on_charging(void)
{
    mt_set_gpio_mode(gpio_number,gpio_on_mode);  
    mt_set_gpio_dir(gpio_number,gpio_on_dir);
    mt_set_gpio_out(gpio_number,gpio_on_out);

    bq24158_config_interface_liao(0x00,0xC0);
    bq24158_config_interface_liao(0x01,0x78);
    //bq24158_config_interface_liao(0x02,0x8e);
    bq24158_config_interface_liao(0x05,0x04);
    bq24158_config_interface_liao(0x04,0x1A); //146mA
}

void bq24158_hw_init(void)
{
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
    dprintf(INFO, "[bq24158_hw_init] (0x06,0x77)\n");
    bq24158_config_interface_liao(0x06,0x77); // set ISAFE and HW CV point (4.34)
    bq24158_config_interface_liao(0x02,0xaa); // 4.34    
#else
    dprintf(INFO, "[bq24158_hw_init] (0x06,0x70)\n");
    bq24158_config_interface_liao(0x06,0x70); // set ISAFE
    bq24158_config_interface_liao(0x02,0x8e); // 4.2
#endif    
}

#if 1
kal_uint32 charging_get_charger_type(void *data);
static CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;
static int temp_CC_value = 0;
#endif

void bq24158_charging_enable(kal_uint32 bEnable)
{
    //TBD: set input current limit depends on connected charger type.
    #if 1
    charging_get_charger_type(&CHR_Type_num);
    if ( CHR_Type_num == STANDARD_HOST )
    {
        temp_CC_value = 500;
        bq24158_set_iinlim(0x2); //IN current limit at 500mA
        //bq24196_set_ac_current();
        bq24196_set_ichg(0);  //Fast Charging Current Limit at 500mA
    }
    else if (CHR_Type_num == NONSTANDARD_CHARGER)
    {
        temp_CC_value = 500;
        bq24196_set_iinlim(0x2); //IN current limit at 500mA
        //bq24196_set_ac_current();
        bq24196_set_ichg(0);  //Fast Charging Current Limit at 500mA
    }
    else if (CHR_Type_num == STANDARD_CHARGER)
    {
        temp_CC_value = 2000;
        bq24196_set_iinlim(0x6); //IN current limit at 2A
        //bq24196_set_ac_current();
        //(2000 - 500)/64 = 0x17;
        bq24196_set_ichg(0x17);  //Fast Charging Current Limit at 2A
    }
    else if (CHR_Type_num == CHARGING_HOST)
    {
        temp_CC_value = 500;
        bq24196_set_iinlim(0x2); //IN current limit at 500mA
        //bq24196_set_ac_current();
        bq24196_set_ichg(0);  //Fast Charging Current Limit at 500mA
    }
    else
    {
        dprintf(INFO, "[BATTERY:bq24196] Unknown charger type\n");
        temp_CC_value = 500;
        bq24196_set_iinlim(0x2); //IN current limit at 500mA
        //bq24196_set_ac_current();
        bq24196_set_ichg(0);  //Fast Charging Current Limit at 500mA
        //bq24196_set_low_chg_current();
    }
    #endif

    bq24196_set_en_hiz(0x0);	        	

    if(KAL_TRUE == bEnable)
        bq24196_set_chg_config(0x1); // charger enable
    else
        bq24196_set_chg_config(0x0); // charger disable

    dprintf(INFO, "[BATTERY:bq24196] bq24196_set_ac_current(), CC value(%dmA) \r\n", temp_CC_value);
    dprintf(INFO, "[BATTERY:bq24196] charger enable !\r\n");
}

#if defined(CONFIG_POWER_EXT)
#else

kal_uint32 Enable_Detection_Log = 0;
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern void mdelay (unsigned long msec);

static void hw_bc11_dump_register(void)
{
	kal_uint32 reg_val = 0;
	kal_uint32 reg_num = CHR_CON18;
	kal_uint32 i = 0;

	for(i=reg_num ; i<=CHR_CON19 ; i+=2)
	{
		reg_val = upmu_get_reg_value(i);
		dprintf(INFO, "Chr Reg[0x%x]=0x%x \r\n", i, reg_val);
	}	
}

static void hw_bc11_init(void)
{
    mdelay(300);
    Charger_Detect_Init();

    //RG_BC11_BIAS_EN=1	
    upmu_set_rg_bc11_bias_en(0x1);
    //RG_BC11_VSRC_EN[1:0]=00
    upmu_set_rg_bc11_vsrc_en(0x0);
    //RG_BC11_VREF_VTH = [1:0]=00
    upmu_set_rg_bc11_vref_vth(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);
    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_IPD_EN[1.0] = 00
    upmu_set_rg_bc11_ipd_en(0x0);
    //BC11_RST=1
    upmu_set_rg_bc11_rst(0x1);
    //BC11_BB_CTRL=1
    upmu_set_rg_bc11_bb_ctrl(0x1);

    //msleep(10);
    mdelay(50);

    if(Enable_Detection_Log)
    {
        dprintf(INFO, "hw_bc11_init() \r\n");
        hw_bc11_dump_register();
    }	
}
 
static U32 hw_bc11_DCD(void)
{
    U32 wChargerAvail = 0;

    //RG_BC11_IPU_EN[1.0] = 10
    upmu_set_rg_bc11_ipu_en(0x2);
    //RG_BC11_IPD_EN[1.0] = 01
    upmu_set_rg_bc11_ipd_en(0x1);
    //RG_BC11_VREF_VTH = [1:0]=01
    upmu_set_rg_bc11_vref_vth(0x1);
    //RG_BC11_CMP_EN[1.0] = 10
    upmu_set_rg_bc11_cmp_en(0x2);

    //msleep(20);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
        dprintf(INFO, "hw_bc11_DCD() \r\n");
        hw_bc11_dump_register();
    }

    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_IPD_EN[1.0] = 00
    upmu_set_rg_bc11_ipd_en(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);
    //RG_BC11_VREF_VTH = [1:0]=00
    upmu_set_rg_bc11_vref_vth(0x0);

    return wChargerAvail;
}

static U32 hw_bc11_stepA1(void)
{
    U32 wChargerAvail = 0;
      
    //RG_BC11_IPU_EN[1.0] = 10
    upmu_set_rg_bc11_ipu_en(0x2);
    //RG_BC11_VREF_VTH = [1:0]=10
    upmu_set_rg_bc11_vref_vth(0x2);
    //RG_BC11_CMP_EN[1.0] = 10
    upmu_set_rg_bc11_cmp_en(0x2);

    //msleep(80);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_stepA1() \r\n");
    	hw_bc11_dump_register();
    }

    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);

    return  wChargerAvail;
}


static U32 hw_bc11_stepB1(void)
{
    U32 wChargerAvail = 0;
      
    //RG_BC11_IPU_EN[1.0] = 01
    //upmu_set_rg_bc11_ipu_en(0x1);
    upmu_set_rg_bc11_ipd_en(0x1);
    //RG_BC11_VREF_VTH = [1:0]=10
    //upmu_set_rg_bc11_vref_vth(0x2);
    upmu_set_rg_bc11_vref_vth(0x0);
    //RG_BC11_CMP_EN[1.0] = 01
    upmu_set_rg_bc11_cmp_en(0x1);

    //msleep(80);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_stepB1() \r\n");
    	hw_bc11_dump_register();
    }

    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);
    //RG_BC11_VREF_VTH = [1:0]=00
    upmu_set_rg_bc11_vref_vth(0x0);

    return  wChargerAvail;
}


static U32 hw_bc11_stepC1(void)
{
    U32 wChargerAvail = 0;
      
    //RG_BC11_IPU_EN[1.0] = 01
    upmu_set_rg_bc11_ipu_en(0x1);
    //RG_BC11_VREF_VTH = [1:0]=10
    upmu_set_rg_bc11_vref_vth(0x2);
    //RG_BC11_CMP_EN[1.0] = 01
    upmu_set_rg_bc11_cmp_en(0x1);

    //msleep(80);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_stepC1() \r\n");
    	hw_bc11_dump_register();
    }

    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);
    //RG_BC11_VREF_VTH = [1:0]=00
    upmu_set_rg_bc11_vref_vth(0x0);

    return  wChargerAvail;
}


static U32 hw_bc11_stepA2(void)
{
    U32 wChargerAvail = 0;
      
    //RG_BC11_VSRC_EN[1.0] = 10 
    upmu_set_rg_bc11_vsrc_en(0x2);
    //RG_BC11_IPD_EN[1:0] = 01
    upmu_set_rg_bc11_ipd_en(0x1);
    //RG_BC11_VREF_VTH = [1:0]=00
    upmu_set_rg_bc11_vref_vth(0x0);
    //RG_BC11_CMP_EN[1.0] = 01
    upmu_set_rg_bc11_cmp_en(0x1);

    //msleep(80);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_stepA2() \r\n");
    	hw_bc11_dump_register();
    }

    //RG_BC11_VSRC_EN[1:0]=00
    upmu_set_rg_bc11_vsrc_en(0x0);
    //RG_BC11_IPD_EN[1.0] = 00
    upmu_set_rg_bc11_ipd_en(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);

    return  wChargerAvail;
}


static U32 hw_bc11_stepB2(void)
{
    U32 wChargerAvail = 0;

    //RG_BC11_IPU_EN[1:0]=10
    upmu_set_rg_bc11_ipu_en(0x2);
    //RG_BC11_VREF_VTH = [1:0]=10
    upmu_set_rg_bc11_vref_vth(0x1);
    //RG_BC11_CMP_EN[1.0] = 01
    upmu_set_rg_bc11_cmp_en(0x1);

    //msleep(80);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_stepB2() \r\n");
    	hw_bc11_dump_register();
    }

    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);
    //RG_BC11_VREF_VTH = [1:0]=00
    upmu_set_rg_bc11_vref_vth(0x0);

    return  wChargerAvail;
}


static void hw_bc11_done(void)
{
    //RG_BC11_VSRC_EN[1:0]=00
    upmu_set_rg_bc11_vsrc_en(0x0);
    //RG_BC11_VREF_VTH = [1:0]=0
    upmu_set_rg_bc11_vref_vth(0x0);
    //RG_BC11_CMP_EN[1.0] = 00
    upmu_set_rg_bc11_cmp_en(0x0);
    //RG_BC11_IPU_EN[1.0] = 00
    upmu_set_rg_bc11_ipu_en(0x0);
    //RG_BC11_IPD_EN[1.0] = 00
    upmu_set_rg_bc11_ipd_en(0x0);
    //RG_BC11_BIAS_EN=0
    upmu_set_rg_bc11_bias_en(0x0); 

    Charger_Detect_Release();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_done() \r\n");
    	hw_bc11_dump_register();
    }

}
#endif

kal_uint32 charging_get_charger_type(void *data)
 {
	 kal_uint32 status = 0;
#if defined(CONFIG_POWER_EXT)
	 *(CHARGER_TYPE*)(data) = STANDARD_HOST;
#else
    
#if 0
    CHARGER_TYPE charger_type = CHARGER_UNKNOWN;
    charger_type = hw_charger_type_detection();
    battery_xlog_printk(BAT_LOG_CRTI, "charging_get_charger_type = %d\r\n", charger_type);
        
    *(CHARGER_TYPE*)(data) = charger_type;
#endif

#if 1
	/********* Step initial  ***************/		 
	hw_bc11_init();
 
	/********* Step DCD ***************/  
	if(1 == hw_bc11_DCD())
	{
		 /********* Step A1 ***************/
		 if(1 == hw_bc11_stepA1())
		 {
			 /********* Step B1 ***************/
			 if(1 == hw_bc11_stepB1())
			 {
				 //*(CHARGER_TYPE*)(data) = NONSTANDARD_CHARGER;
				 //battery_xlog_printk(BAT_LOG_CRTI, "step B1 : Non STANDARD CHARGER!\r\n");				
				 *(CHARGER_TYPE*)(data) = APPLE_2_1A_CHARGER;
				 dprintf(INFO, "step B1 : Apple 2.1A CHARGER!\r\n");
			 }	 
			 else
			 {
				 //*(CHARGER_TYPE*)(data) = APPLE_2_1A_CHARGER;
				 //battery_xlog_printk(BAT_LOG_CRTI, "step B1 : Apple 2.1A CHARGER!\r\n");
				 *(CHARGER_TYPE*)(data) = NONSTANDARD_CHARGER;
				 dprintf(INFO, "step B1 : Non STANDARD CHARGER!\r\n");
			 }	 
		 }
		 else
		 {
			 /********* Step C1 ***************/
			 if(1 == hw_bc11_stepC1())
			 {
				 *(CHARGER_TYPE*)(data) = APPLE_1_0A_CHARGER;
				 dprintf(INFO, "step C1 : Apple 1A CHARGER!\r\n");
			 }	 
			 else
			 {
				 *(CHARGER_TYPE*)(data) = APPLE_0_5A_CHARGER;
				 dprintf(INFO, "step C1 : Apple 0.5A CHARGER!\r\n");			 
			 }	 
		 }
 
	}
	else
	{
		 /********* Step A2 ***************/
		 if(1 == hw_bc11_stepA2())
		 {
			 /********* Step B2 ***************/
			 if(1 == hw_bc11_stepB2())
			 {
				 *(CHARGER_TYPE*)(data) = STANDARD_CHARGER;
				 dprintf(INFO, "step B2 : STANDARD CHARGER!\r\n");
			 }
			 else
			 {
				 *(CHARGER_TYPE*)(data) = CHARGING_HOST;
				 dprintf(INFO, "step B2 :  Charging Host!\r\n");
			 }
		 }
		 else
		 {
         *(CHARGER_TYPE*)(data) = STANDARD_HOST;
			 dprintf(INFO, "step A2 : Standard USB Host!\r\n");
		 }
 
	}
 
	 /********* Finally setting *******************************/
	 hw_bc11_done();
#endif
#endif
	 return status;
}
