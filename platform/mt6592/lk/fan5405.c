#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_i2c.h>
#include <platform/fan5405.h>
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <target/cust_battery.h>
#include <printf.h>

int g_fan5405_log_en=0;

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define fan5405_SLAVE_ADDR_WRITE   0xD4
#define fan5405_SLAVE_ADDR_Read    0xD5

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
#define fan5405_REG_NUM 7  
kal_uint8 fan5405_reg[fan5405_REG_NUM] = {0};

#define FAN5405_I2C_ID	I2C1
static struct mt_i2c_t fan5405_i2c;

/**********************************************************
  *
  *   [I2C Function For Read/Write fan5405] 
  *
  *********************************************************/
kal_uint32 fan5405_write_byte(kal_uint8 addr, kal_uint8 value)
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint8 write_data[2];
    kal_uint16 len;

    write_data[0]= addr;
    write_data[1] = value;

    fan5405_i2c.id = FAN5405_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set FAN5405 I2C address to >>1 */
    fan5405_i2c.addr = (fan5405_SLAVE_ADDR_WRITE >> 1);
    fan5405_i2c.mode = ST_MODE;
    fan5405_i2c.speed = 100;
    len = 2;

    ret_code = i2c_write(&fan5405_i2c, write_data, len);
    //dprintf(INFO, "%s: i2c_write: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

kal_uint32 fan5405_read_byte (kal_uint8 addr, kal_uint8 *dataBuffer) 
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint16 len;
    *dataBuffer = addr;

    fan5405_i2c.id = FAN5405_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set FAN5405 I2C address to >>1 */
    fan5405_i2c.addr = (fan5405_SLAVE_ADDR_WRITE >> 1);
    fan5405_i2c.mode = ST_MODE;
    fan5405_i2c.speed = 100;
    len = 1;

    ret_code = i2c_write_read(&fan5405_i2c, dataBuffer, len, len);
    //dprintf(INFO, "%s: i2c_read: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 fan5405_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 fan5405_reg = 0;
    int ret = 0;
    
    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = fan5405_read_byte(RegNum, &fan5405_reg);
    dprintf(INFO, "[fan5405_read_interface] Reg[%x]=0x%x\n", RegNum, fan5405_reg);
    
    fan5405_reg &= (MASK << SHIFT);
    *val = (fan5405_reg >> SHIFT);    
    dprintf(INFO, "[fan5405_read_interface] val=0x%x\n", *val);

    return ret;
}

kal_uint32 fan5405_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 fan5405_reg = 0;
    int ret = 0;

    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = fan5405_read_byte(RegNum, &fan5405_reg);
    dprintf(INFO, "[fan5405_config_interface] Reg[%x]=0x%x\n", RegNum, fan5405_reg);
    
    fan5405_reg &= ~(MASK << SHIFT);
    fan5405_reg |= (val << SHIFT);

    ret = fan5405_write_byte(RegNum, fan5405_reg);
    dprintf(INFO, "[fan5405_config_interface] write Reg[%x]=0x%x\n", RegNum, fan5405_reg);

    // Check
    //fan5405_read_byte(RegNum, &fan5405_reg);
    //dprintf(INFO, "[fan5405_config_interface] Check Reg[%x]=0x%x\n", RegNum, fan5405_reg);

    return ret;
}

/**********************************************************
  *
  *   [fan5405 Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------

void fan5405_set_tmr_rst(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_TMR_RST_MASK),
                                    (kal_uint8)(CON0_TMR_RST_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

kal_uint32 fan5405_get_otg_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_OTG_MASK),
                                    (kal_uint8)(CON0_OTG_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

void fan5405_set_en_stat(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_EN_STAT_MASK),
                                    (kal_uint8)(CON0_EN_STAT_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
            dprintf(INFO, "%d\n", ret);    
}

kal_uint32 fan5405_get_chip_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_STAT_MASK),
                                    (kal_uint8)(CON0_STAT_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5405_get_boost_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_BOOST_MASK),
                                    (kal_uint8)(CON0_BOOST_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5405_get_fault_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_FAULT_MASK),
                                    (kal_uint8)(CON0_FAULT_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

//CON1----------------------------------------------------

void fan5405_set_input_charging_current(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LIN_LIMIT_MASK),
                                    (kal_uint8)(CON1_LIN_LIMIT_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);    
}

void fan5405_set_v_low(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LOW_V_MASK),
                                    (kal_uint8)(CON1_LOW_V_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_te(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_TE_MASK),
                                    (kal_uint8)(CON1_TE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_ce(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_CE_MASK),
                                    (kal_uint8)(CON1_CE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_hz_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_HZ_MODE_MASK),
                                    (kal_uint8)(CON1_HZ_MODE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_opa_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_OPA_MODE_MASK),
                                    (kal_uint8)(CON1_OPA_MODE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON2----------------------------------------------------

void fan5405_set_oreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OREG_MASK),
                                    (kal_uint8)(CON2_OREG_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_otg_pl(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OTG_PL_MASK),
                                    (kal_uint8)(CON2_OTG_PL_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_otg_en(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_OTG_EN_MASK),
                                    (kal_uint8)(CON2_OTG_EN_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON3----------------------------------------------------

kal_uint32 fan5405_get_vender_code(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_VENDER_CODE_MASK),
                                    (kal_uint8)(CON3_VENDER_CODE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5405_get_pn(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_PIN_MASK),
                                    (kal_uint8)(CON3_PIN_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5405_get_revision(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON3), 
                                    (&val),
                                    (kal_uint8)(CON3_REVISION_MASK),
                                    (kal_uint8)(CON3_REVISION_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

//CON4----------------------------------------------------

void fan5405_set_reset(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_RESET_MASK),
                                    (kal_uint8)(CON4_RESET_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);    
}

void fan5405_set_iocharge(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_I_CHR_MASK),
                                    (kal_uint8)(CON4_I_CHR_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_iterm(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_I_TERM_MASK),
                                    (kal_uint8)(CON4_I_TERM_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON5----------------------------------------------------

void fan5405_set_dis_vreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_DIS_VREG_MASK),
                                    (kal_uint8)(CON5_DIS_VREG_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_io_level(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_IO_LEVEL_MASK),
                                    (kal_uint8)(CON5_IO_LEVEL_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

kal_uint32 fan5405_get_sp_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON5), 
                                    (&val),
                                    (kal_uint8)(CON5_SP_STATUS_MASK),
                                    (kal_uint8)(CON5_SP_STATUS_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

kal_uint32 fan5405_get_en_level(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=fan5405_read_interface(     (kal_uint8)(fan5405_CON5), 
                                    (&val),
                                    (kal_uint8)(CON5_EN_LEVEL_MASK),
                                    (kal_uint8)(CON5_EN_LEVEL_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
    
    return val;
}

void fan5405_set_vsp(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_VSP_MASK),
                                    (kal_uint8)(CON5_VSP_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

//CON6----------------------------------------------------

void fan5405_set_i_safe(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_ISAFE_MASK),
                                    (kal_uint8)(CON6_ISAFE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

void fan5405_set_v_safe(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=fan5405_config_interface(   (kal_uint8)(fan5405_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_VSAFE_MASK),
                                    (kal_uint8)(CON6_VSAFE_SHIFT)
                                    );
    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/

unsigned int fan5405_reg_config_interface (unsigned char RegNum, unsigned char val)
{
    int ret = 0;
    
    ret = fan5405_write_byte(RegNum, val);

    if(g_fan5405_log_en>1)        
        dprintf(INFO, "%d\n", ret);

    return ret;
}

void fan5405_dump_register(void)
{
    int i=0;

    for (i=0;i<fan5405_REG_NUM;i++)
    {
        fan5405_read_byte(i, &fan5405_reg[i]);
        dprintf(INFO, "[0x%x]=0x%x\n", i, fan5405_reg[i]);        
    }
}

void fan5405_read_register(int i)
{
    fan5405_read_byte(i, &fan5405_reg[i]);
    dprintf(INFO, "[0x%x]=0x%x\n", i, fan5405_reg[i]); 
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

void fan5405_turn_on_charging(void)
{
    mt_set_gpio_mode(gpio_number,gpio_on_mode);  
    mt_set_gpio_dir(gpio_number,gpio_on_dir);
    mt_set_gpio_out(gpio_number,gpio_on_out);

    fan5405_reg_config_interface(0x00,0xC0);
    fan5405_reg_config_interface(0x01,0x78);
    //fan5405_reg_config_interface(0x02,0x8e);
    fan5405_reg_config_interface(0x05,0x04);
    fan5405_reg_config_interface(0x04,0x1A); //146mA
}

void fan5405_hw_init(void)
{
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
    dprintf(INFO, "[fan5405_hw_init] (0x06,0x77)\n");
    fan5405_reg_config_interface(0x06,0x77); // set ISAFE and HW CV point (4.34)
    fan5405_reg_config_interface(0x02,0xaa); // 4.34    
#else
    dprintf(INFO, "[fan5405_hw_init] (0x06,0x70)\n");
    fan5405_reg_config_interface(0x06,0x70); // set ISAFE
    fan5405_reg_config_interface(0x02,0x8e); // 4.2
#endif    
}

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
      
	//RG_BC11_IPD_EN[1:0] = 01
	upmu_set_rg_bc11_ipd_en(0x1);
	//RG_BC11_VREF_VTH[1:0]=00
	upmu_set_rg_bc11_vref_vth(0x0);
	// RG_BC11_CMP_EN[1:0] = 01
	upmu_set_rg_bc11_cmp_en(0x1);

    //msleep(80);
    mdelay(80);

    wChargerAvail = upmu_get_rgs_bc11_cmp_out();

    if(Enable_Detection_Log)
    {
    	dprintf(INFO, "hw_bc11_stepA1() \r\n");
    	hw_bc11_dump_register();
    }

    //RG_BC11_IPU_EN[1.0] = 00
	upmu_set_rg_bc11_ipd_en(0x0);
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
			 *(CHARGER_TYPE*)(data) = APPLE_2_1A_CHARGER;
			 dprintf(INFO, "step A1 : Apple 2.1A CHARGER!\r\n");
		 }
		 else
		 {
			 *(CHARGER_TYPE*)(data) = NONSTANDARD_CHARGER;
			 dprintf(INFO, "step A1 : Non STANDARD CHARGER!\r\n");
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
