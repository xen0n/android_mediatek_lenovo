#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_i2c.h>
#include <platform/fan5402.h>
#include <platform/mt_gpio.h>
#include <target/cust_battery.h>
#include <printf.h>

int g_fan5402_log_en=0;

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define fan5402_SLAVE_ADDR_WRITE   0xD6
#define fan5402_SLAVE_ADDR_Read    0xD7

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
#define fan5402_REG_NUM 5  
kal_uint8 fan5402_reg[fan5402_REG_NUM] = {0};

#define fan5402_I2C_ID	I2C2
static struct mt_i2c_t fan5402_i2c;

/**********************************************************
  *
  *   [I2C Function For Read/Write fan5402] 
  *
  *********************************************************/
kal_uint32 fan5402_write_byte(kal_uint8 addr, kal_uint8 value)
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint8 write_data[2];
    kal_uint16 len;

    write_data[0]= addr;
    write_data[1] = value;

    fan5402_i2c.id = fan5402_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set fan5402 I2C address to >>1 */
    fan5402_i2c.addr = (fan5402_SLAVE_ADDR_WRITE >> 1);
    fan5402_i2c.mode = ST_MODE;
    fan5402_i2c.speed = 100;
    len = 2;

    ret_code = i2c_write(&fan5402_i2c, write_data, len);
    //dprintf(INFO, "%s: i2c_write: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

kal_uint32 fan5402_read_byte (kal_uint8 addr, kal_uint8 *dataBuffer) 
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint16 len;
    *dataBuffer = addr;

    fan5402_i2c.id = fan5402_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set fan5402 I2C address to >>1 */
    fan5402_i2c.addr = (fan5402_SLAVE_ADDR_WRITE >> 1);
    fan5402_i2c.mode = ST_MODE;
    fan5402_i2c.speed = 100;
    len = 1;

    ret_code = i2c_write_read(&fan5402_i2c, dataBuffer, len, len);
    //dprintf(INFO, "%s: i2c_read: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 fan5402_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 fan5402_reg = 0;
    int ret = 0;
    
    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = fan5402_read_byte(RegNum, &fan5402_reg);
    dprintf(INFO, "[fan5402_read_interface] Reg[%x]=0x%x\n", RegNum, fan5402_reg);
    
    fan5402_reg &= (MASK << SHIFT);
    *val = (fan5402_reg >> SHIFT);    
    dprintf(INFO, "[fan5402_read_interface] val=0x%x\n", *val);

    return ret;
}

kal_uint32 fan5402_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 fan5402_reg = 0;
    int ret = 0;

    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = fan5402_read_byte(RegNum, &fan5402_reg);
    dprintf(INFO, "[fan5402_config_interface] Reg[%x]=0x%x\n", RegNum, fan5402_reg);
    
    fan5402_reg &= ~(MASK << SHIFT);
    fan5402_reg |= (val << SHIFT);

    ret = fan5402_write_byte(RegNum, fan5402_reg);
    dprintf(INFO, "[fan5402_config_interface] write Reg[%x]=0x%x\n", RegNum, fan5402_reg);

    // Check
    //fan5402_read_byte(RegNum, &fan5402_reg);
    //dprintf(INFO, "[fan5402_config_interface] Check Reg[%x]=0x%x\n", RegNum, fan5402_reg);

    return ret;
}

/**********************************************************
  *
  *   [fan5402 Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------

void fan5402_set_tmr_rst(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_TMR_RST_MASK),
                                    (kal_uint8)(CON0_TMR_RST_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

kal_uint32 fan5402_get_otg_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_OTG_MASK),
                                    (kal_uint8)(CON0_OTG_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

void fan5402_set_en_stat(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_EN_STAT_MASK),
                                    (kal_uint8)(CON0_EN_STAT_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
            dprintf(INFO, "%d\n", ret);    
}

kal_uint32 fan5402_get_chip_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_STAT_MASK),
                                    (kal_uint8)(CON0_STAT_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5402_get_boost_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_BOOST_MASK),
                                    (kal_uint8)(CON0_BOOST_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5402_get_fault_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_FAULT_MASK),
                                    (kal_uint8)(CON0_FAULT_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

//CON1----------------------------------------------------

void fan5402_set_input_charging_current(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LIN_LIMIT_MASK),
                                    (kal_uint8)(CON1_LIN_LIMIT_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);    
}

void fan5402_set_v_low(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LOW_V_MASK),
                                    (kal_uint8)(CON1_LOW_V_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_te(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_TE_MASK),
                                    (kal_uint8)(CON1_TE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_ce(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_CE_MASK),
                                    (kal_uint8)(CON1_CE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_hz_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_HZ_MODE_MASK),
                                    (kal_uint8)(CON1_HZ_MODE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_opa_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_OPA_MODE_MASK),
                                    (kal_uint8)(CON1_OPA_MODE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON2----------------------------------------------------

void fan5402_set_oreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OREG_MASK),
                                    (kal_uint8)(CON2_OREG_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_otg_pl(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OTG_PL_MASK),
                                    (kal_uint8)(CON2_OTG_PL_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_otg_en(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OTG_EN_MASK),
                                    (kal_uint8)(CON2_OTG_EN_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON3----------------------------------------------------

kal_uint32 fan5402_get_vender_code(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_VENDER_CODE_MASK),
                                    (kal_uint8)(CON3_VENDER_CODE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5402_get_pn(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_PIN_MASK),
                                    (kal_uint8)(CON3_PIN_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5402_get_revision(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_REVISION_MASK),
                                    (kal_uint8)(CON3_REVISION_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

//CON4----------------------------------------------------

void fan5402_set_reset(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_RESET_MASK),
                                    (kal_uint8)(CON4_RESET_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);    
}

void fan5402_set_iocharge(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_I_CHR_MASK),
                                    (kal_uint8)(CON4_I_CHR_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_iterm(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_I_TERM_MASK),
                                    (kal_uint8)(CON4_I_TERM_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}
#if 0
//CON5----------------------------------------------------

void fan5402_set_dis_vreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_DIS_VREG_MASK),
                                    (kal_uint8)(CON5_DIS_VREG_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_io_level(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_IO_LEVEL_MASK),
                                    (kal_uint8)(CON5_IO_LEVEL_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

kal_uint32 fan5402_get_sp_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON5), 
                                    (&val),
                                    (kal_uint8)(CON5_SP_STATUS_MASK),
                                    (kal_uint8)(CON5_SP_STATUS_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5402_get_en_level(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5402_read_interface(     (kal_uint8)(fan5402_CON5), 
                                    (&val),
                                    (kal_uint8)(CON5_EN_LEVEL_MASK),
                                    (kal_uint8)(CON5_EN_LEVEL_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

void fan5402_set_vsp(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_VSP_MASK),
                                    (kal_uint8)(CON5_VSP_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON6----------------------------------------------------

void fan5402_set_i_safe(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_ISAFE_MASK),
                                    (kal_uint8)(CON6_ISAFE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5402_set_v_safe(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5402_config_interface(   (kal_uint8)(fan5402_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_VSAFE_MASK),
                                    (kal_uint8)(CON6_VSAFE_SHIFT)
                                    );
    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}
#endif
/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/

unsigned int fan5402_reg_config_interface (unsigned char RegNum, unsigned char val)
{
    int ret = 0;
    
    ret = fan5402_write_byte(RegNum, val);

    if(g_fan5402_log_en>1)        
        dprintf(INFO, "%d\n", ret);

    return ret;
}

void fan5402_dump_register(void)
{
    int i=0;

    for (i=0;i<fan5402_REG_NUM;i++)
    {
        fan5402_read_byte(i, &fan5402_reg[i]);
        dprintf(INFO, "[0x%x]=0x%x\n", i, fan5402_reg[i]);        
    }
}

void fan5402_read_register(int i)
{
    fan5402_read_byte(i, &fan5402_reg[i]);
    dprintf(INFO, "[0x%x]=0x%x\n", i, fan5402_reg[i]); 
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

void fan5402_turn_on_charging(void)
{
    mt_set_gpio_mode(gpio_number,gpio_on_mode);  
    mt_set_gpio_dir(gpio_number,gpio_on_dir);
    mt_set_gpio_out(gpio_number,gpio_on_out);

    fan5402_reg_config_interface(0x00,0xC0);
    fan5402_reg_config_interface(0x01,0x78);
    //fan5402_reg_config_interface(0x02,0x8e);
    fan5402_reg_config_interface(0x04,0x1A); //146mA
}

void fan5402_hw_init(void)
{
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
    dprintf(INFO, "[fan5402_hw_init] (0x02,0xaa)\n");
    fan5402_reg_config_interface(0x02,0xaa); // 4.34    
#else
    fan5402_reg_config_interface(0x02,0x8e); // 4.2
#endif    
}

