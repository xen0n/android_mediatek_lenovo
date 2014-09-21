#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_i2c.h>   
#include <platform/mt_pmic.h>
#include <platform/tps6128x.h>
#include <printf.h>

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define tps6128x_SLAVE_ADDR_WRITE   0xEA
#define tps6128x_SLAVE_ADDR_READ    0xEB

#ifdef I2C_EXT_VBAT_BOOST_CHANNEL
#define TPS6128X_I2C_ID I2C_EXT_VBAT_BOOST_CHANNEL
#else
#define TPS6128X_I2C_ID I2C1
#endif

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
int g_tps6128x_log_en=0;
int g_tps6128x_hw_exist=0;

kal_uint8 tps6128x_reg[tps6128x_REG_NUM] = {0};

/**********************************************************
  *
  *   [I2C Function For Read/Write tps6128x] 
  *
  *********************************************************/
#if 0
int tps6128x_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&tps6128x_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;

        mutex_unlock(&tps6128x_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&tps6128x_i2c_access);    
    return 1;
}

int tps6128x_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;
    
    mutex_lock(&tps6128x_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;
    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&tps6128x_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&tps6128x_i2c_access);
    return 1;
#else
static struct mt_i2c_t tps6128x_i2c;
kal_uint32 tps6128x_write_byte(kal_uint8 addr, kal_uint8 value)
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint8 write_data[2];
    kal_uint16 len;

    write_data[0]= addr;
    write_data[1] = value;

    tps6128x_i2c.id = TPS6128X_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set TPS6128X I2C address to >>1 */
    tps6128x_i2c.addr = (tps6128x_SLAVE_ADDR_WRITE >> 1);
    tps6128x_i2c.mode = ST_MODE;
    tps6128x_i2c.speed = 100;
    len = 2;

    ret_code = i2c_write(&tps6128x_i2c, write_data, len);
    //dprintf(INFO, "%s: i2c_write: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

kal_uint32 tps6128x_read_byte(kal_uint8 addr, kal_uint8 *dataBuffer) 
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint16 len;
    *dataBuffer = addr;

    tps6128x_i2c.id = TPS6128X_I2C_ID;
    /* Since i2c will left shift 1 bit, we need to set TPS6128X I2C address to >>1 */
    tps6128x_i2c.addr = (tps6128x_SLAVE_ADDR_READ >> 1);
    tps6128x_i2c.mode = ST_MODE;
    tps6128x_i2c.speed = 100;
    len = 1;

    ret_code = i2c_write_read(&tps6128x_i2c, dataBuffer, len, len);
    //dprintf(INFO, "%s: i2c_read: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}
#endif
/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 tps6128x_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 tps6128x_reg = 0;
    int ret = 0;

    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = tps6128x_read_byte(RegNum, &tps6128x_reg);

    dprintf(INFO, "[tps6128x_read_interface] Reg[%x]=0x%x\n", RegNum, tps6128x_reg);
	
    tps6128x_reg &= (MASK << SHIFT);
    *val = (tps6128x_reg >> SHIFT);
	
    if(g_tps6128x_log_en>1)
	    dprintf(INFO, "%d\n", ret);
	
    return ret;
}

kal_uint32 tps6128x_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 tps6128x_reg = 0;
    int ret = 0;

    dprintf(INFO, "--------------------------------------------------LK\n");

    ret = tps6128x_read_byte(RegNum, &tps6128x_reg);
    dprintf(INFO, "[tps6128x_config_interface] Reg[%x]=0x%x\n", RegNum, tps6128x_reg);
    
    tps6128x_reg &= ~(MASK << SHIFT);
    tps6128x_reg |= (val << SHIFT);

    ret = tps6128x_write_byte(RegNum, tps6128x_reg);
    dprintf(INFO, "[tps6128x_config_interface] write Reg[%x]=0x%x\n", RegNum, tps6128x_reg);

    // Check
    //tps6128x_read_byte(RegNum, &tps6128x_reg);
    //dprintf("[tps6128x_config_interface] Check Reg[%x]=0x%x\n", RegNum, tps6128x_reg);

    if(g_tps6128x_log_en>1)
	    dprintf(INFO, "%d\n", ret);

    return ret;
}

//write one register directly
kal_uint32 tps6128x_reg_config_interface (kal_uint8 RegNum, kal_uint8 val)
{   
    int ret = 0;
    
    ret = tps6128x_write_byte(RegNum, val);

    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void tps6128x_hw_component_detect(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=tps6128x_read_interface(0x3,&val,0xFF, 0);

    if(val == 0)
        g_tps6128x_hw_exist=0;
    else
        g_tps6128x_hw_exist=1;

    dprintf(INFO, "[tps6128x_hw_component_detect] exist=%d, Reg[0x3]=0x%x\n", 
        g_tps6128x_hw_exist, val);

    if(g_tps6128x_log_en>1)
        dprintf(INFO, "%d\n", ret);

}

void tps6128x_dump_register(void)
{
    int i=0;

    for (i=0;i<tps6128x_REG_NUM;i++)
    {
        tps6128x_read_byte(i, &tps6128x_reg[i]);
        dprintf(CRITICAL, "[tps6128x_dump_register][0x%x]=0x%x ", i, tps6128x_reg[i]);
    }
}

void tps6128x_hw_init(void)
{
    dprintf(INFO, "[tps6128x_hw_init] From Johnson\n");
    tps6128x_hw_component_detect(); 
    if(1 == g_tps6128x_hw_exist)
    {
        tps6128x_dump_register();
        tps6128x_config_interface(0x3, 0xA, 0x1F, 0); // Output voltage threshold 0xA= 3.35V; 0xB = 3.4V
        tps6128x_config_interface(0x4, 0xF, 0xF,  0); // OC_input=max	 
    }

    dprintf(INFO, "[tps6128x_hw_init] After HW init\n");
    tps6128x_dump_register();
}
