 
#include "tpd.h"
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <mach/mt_boot.h>
#include <mach/mt_pm_ldo.h>
#include <cust_eint.h>

#include "tpd_custom_bt5xx.h"
#include "cust_gpio_usage.h"

#include "zinitix_touch.h"

#if TOUCH_ONESHOT_UPGRADE
#include "zinitix_touch_zxt_firmware.h"

#define TSP_TYPE_COUNT	1
const u8 *m_pFirmware [TSP_TYPE_COUNT] = {(u8*)m_firmware_data,};
u8 m_FirmwareIdx=0;
#endif

extern struct tpd_device *tpd;

static struct i2c_client *i2c_client = NULL;
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static void tpd_eint_interrupt_handler(void);

static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);

static bool ts_init_touch(void);
static void tpd_clear_report_data(void);

static int tpd_flag = 0;
static int tpd_halt = 0;

#define TPD_OK 0

#ifdef TPD_ESD_CHECK	
static struct delayed_work ctp_read_id_work;
static struct workqueue_struct *ctp_read_id_workqueue = NULL;
#endif

//=============================================================================

//register define
#define TPD_RESET_ISSUE_WORKAROUND
#define TPD_MAX_RESET_COUNT 3

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
u8 button[TPD_KEY_COUNT] = {ICON_BUTTON_UNCHANGE,};
#endif

#define	TOUCH_MODE	0

#define DELAY_FOR_TRANSCATION 50
#define DELAY_FOR_POST_TRANSCATION 10

#define	MAX_SUPPORTED_FINGER_NUM    5
    
struct _ts_zinitix_coord {
    u16    x;
    u16    y;
    u8    width;
    u8    sub_status;
};

struct _ts_zinitix_point_info {
    u16    status;
    u8    finger_cnt;
    u8    time_stamp;
    struct _ts_zinitix_coord    coord[MAX_SUPPORTED_FINGER_NUM];
};

struct _ts_zinitix_point_info reported_touch_info;
struct _ts_zinitix_point_info touch_info;

struct touch_info {
    int y[3];
    int x[3];
    int p[3];
    int count;
};

#define	TPD_DEVICE_BTXXX	"BT532"
static const struct i2c_device_id tpd_id[] = {{TPD_DEVICE_BTXXX,0}, {}};
static struct i2c_board_info __initdata zinitix_i2c_tpd={ I2C_BOARD_INFO(TPD_DEVICE_BTXXX, (0x40>>1))};

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.name = TPD_DEVICE_BTXXX,//TPD_DEVICE,
	},
	.probe = tpd_probe,
	.remove = __devexit_p(tpd_remove),
	.id_table = tpd_id,
	.detect = tpd_detect,
};

#ifdef TPD_HIGH_SPEED_DMA
#define	CTP_I2C_DMA_BUF_SIZE	(4096)
static u8 *CTPI2CDMABuf_va = NULL;
static u32 CTPI2CDMABuf_pa = NULL;
#endif

//define sub functions
//=====================================================================================
#ifdef TPD_HIGH_SPEED_DMA
static int ts_i2c_write_dma(struct i2c_client *client, u8 *buf, u32 len)
{
	int i = 0, err = 0;

	TPD_DMESG("%s, %d, len=%d\n", __FUNCTION__, __LINE__, len);
	for(i = 0 ; i < len; i++)
	{
		CTPI2CDMABuf_va[i] = buf[i];
	}
	
	if(len <= 8)
	{
		client->addr = client->addr & I2C_MASK_FLAG;
		return i2c_master_send(client, buf, len);
	}
	else
	{
		//TPD_DMESG("CTPI2CDMABuf_va=0x%x, CTPI2CDMABuf_pa=0x%x\n", CTPI2CDMABuf_va, CTPI2CDMABuf_pa);
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
		err = i2c_master_send(client, (char*)CTPI2CDMABuf_pa, len);
		client->addr = client->addr & I2C_MASK_FLAG;
		return err;
	}    
}

static int ts_i2c_read_dma(struct i2c_client *client, u8 *buf, u32 len)
{
	int i = 0, err = 0;
	char *ptr;

	//TPD_DMESG("%s, %d, len=%d\n", __FUNCTION__, __LINE__, len);
	if(len < 8)
	{
		client->addr = client->addr & I2C_MASK_FLAG;
		return i2c_master_recv(client, buf, len);
	}
	else
	{
		//TPD_DMESG("CTPI2CDMABuf_va=0x%x, CTPI2CDMABuf_pa=0x%x\n", CTPI2CDMABuf_va, CTPI2CDMABuf_pa);
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
		err = i2c_master_recv(client, (char*)CTPI2CDMABuf_pa, len);
		client->addr = client->addr & I2C_MASK_FLAG;
		if(err < 0)
		{
			return err;
		}
		
		for(i = 0; i < len; i++)
		{
			buf[i] = CTPI2CDMABuf_va[i];
		}
		return err;
	}
}
#endif

s32 ts_read_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;

	TPD_DMESG("%s, %d, reg=0x%x, length=%d\n", __FUNCTION__, __LINE__, reg, length);
	/* select register*/
	ret = i2c_master_send(client , (u8*)&reg , 2);
	if (ret < 0)
		return ret;
	/* for setup tx transaction.*/
	udelay(DELAY_FOR_TRANSCATION);

#ifndef TPD_HIGH_SPEED_DMA
	ret = i2c_master_recv(client , values , length);
#else
	ret = ts_i2c_read_dma(client , values , length);
#endif
	TPD_DMESG("%s, %d, ret=%d\n", __FUNCTION__, __LINE__, ret);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

s32 ts_write_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;
	u8 pkt[4];

	TPD_DMESG("%s, %d, reg=0x%x, length=%d\n", __FUNCTION__, __LINE__, reg, length);
	pkt[0] = (reg)&0xff;
	pkt[1] = (reg >>8)&0xff;
	pkt[2] = values[0];
	pkt[3] = values[1];
#ifndef TPD_HIGH_SPEED_DMA
	ret = i2c_master_send(client, pkt, length+2);
#else
	ret = ts_i2c_write_dma(client, pkt, length+2);
#endif
	TPD_DMESG("%s, %d, ret=%d\n", __FUNCTION__, __LINE__, ret);
	udelay(DELAY_FOR_POST_TRANSCATION);
	return ret;
}

s32 ts_write_cmd(struct i2c_client *client, u16 reg)
{
	//TPD_DMESG("%s, %d, reg=0x%x\n", __FUNCTION__, __LINE__, reg);
	if(i2c_master_send(client, (u8 *)&reg, 2) < 0)
		return -1;
	return 0;
}

s32 ts_write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	//TPD_DMESG("%s, %d, reg=0x%x, value=%d\n", __FUNCTION__, __LINE__, reg, value);
	if(ts_write_data(client, reg, (u8 *)&value, 2) < 0)
		return -1;
	return 0;
}

s32 ts_read_raw_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;

	//TPD_DMESG("%s, %d, reg=0x%x, length=%d\n", __FUNCTION__, __LINE__, reg, length); 
	if(i2c_master_send(client, (u8 *)&reg, 2) < 0)
		return -1;
	mdelay(5); 	   // for setup tx transaction
#ifndef TPD_HIGH_SPEED_DMA
	ret = i2c_master_recv(client , values , length);
#else
	ret = ts_i2c_read_dma(client , values , length);
#endif
	//if((ret = i2c_master_recv(client , values , length)) < 0)
	if(ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);	
	return length;
}

static bool ts_power_sequence(struct i2c_client *client)
{
	int retry = 0;
	u16 chip_code;

	TPD_DMESG("enter %s, %d\n", __FUNCTION__, __LINE__); 
retry_power_sequence:	
	if (ts_write_reg(i2c_client, 0xc000, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (vendor cmd enable)\n");
		goto fail_power_sequence;
	}
	udelay(DELAY_FOR_POST_TRANSCATION);
	if (ts_read_data(i2c_client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		TPD_DMESG("fail to read chip code\n");
		goto fail_power_sequence;
	}

	TPD_DMESG("chip code = 0x%x\n", chip_code); 
    	TPD_DMESG("chip code = 0x%x\n", chip_code);

	udelay(DELAY_FOR_POST_TRANSCATION);	
	if (ts_write_cmd(i2c_client, 0xc004) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (intn clear)\n");
		goto fail_power_sequence;
	}
	udelay(DELAY_FOR_POST_TRANSCATION);
	if (ts_write_reg(i2c_client, 0xc002, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (nvm init)\n");
		goto fail_power_sequence;
	}
	mdelay(2);
	if (ts_write_reg(i2c_client, 0xc001, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (program start)\n");
		goto fail_power_sequence;
	}
	msleep(FIRMWARE_ON_DELAY);	//wait for checksum cal
	TPD_DMESG("%s, %d, return true\n", __FUNCTION__, __LINE__); 
	return true;
	
fail_power_sequence:
	if(retry++ < 3) {
		msleep(CHIP_ON_DELAY);
		TPD_DMESG("retry = %d\n", retry);
		goto retry_power_sequence;
	}
	return false;
}

#if TOUCH_ONESHOT_UPGRADE
static bool ts_check_need_upgrade(u16 curVersion, u16 curMinorVersion, u16 curRegVersion)
{
	u16	newVersion;
	u16	newMinorVersion;	
	u16	newRegVersion;
	u16	newChipCode;
	u16	newHWID;
	u8 *firmware_data;
	
	firmware_data = (u8*)m_pFirmware[m_FirmwareIdx];

	curVersion = curVersion&0xff;
	newVersion = (u16) (firmware_data[52] | (firmware_data[53]<<8));
	newVersion = newVersion&0xff;
	newMinorVersion = (u16) (firmware_data[56] | (firmware_data[57]<<8));
	newRegVersion = (u16) (firmware_data[60] | (firmware_data[61]<<8));
	newChipCode = (u16) (firmware_data[64] | (firmware_data[65]<<8));

	TPD_DMESG("%s, %d, cur version = 0x%x, new version = 0x%x, cur minor version = 0x%x, new minor version = 0x%x, cur reg data version = 0x%x, new reg data version = 0x%x\r\n", 
		__FUNCTION__, __LINE__, \
		curVersion, newVersion, \
		curMinorVersion, newMinorVersion, \
		curRegVersion, newRegVersion);
	
	TPD_DMESG("cur version = 0x%x, new version = 0x%x\n",
		curVersion, newVersion);
	if (curVersion < newVersion)
		return true;
	else if (curVersion > newVersion)
		return false;
	
	TPD_DMESG("cur minor version = 0x%x, new minor version = 0x%x\n",
			curMinorVersion, newMinorVersion);
	if (curMinorVersion < newMinorVersion)
		return true;
	else if (curMinorVersion > newMinorVersion)
		return false;

	TPD_DMESG("cur reg data version = 0x%x, new reg data version = 0x%x\n",
			curRegVersion, newRegVersion);
	if (curRegVersion < newRegVersion)
		return true;
	
	return false;
}

static bool ts_hw_calibration(void)
{
	u16	chip_eeprom_info;
	int time_out = 0;

	if (ts_write_reg(i2c_client,
		ZINITIX_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;
	mdelay(10);
	ts_write_cmd(i2c_client,	ZINITIX_CLEAR_INT_STATUS_CMD);
	mdelay(10);
	ts_write_cmd(i2c_client,	ZINITIX_CLEAR_INT_STATUS_CMD);	
	mdelay(50);
	ts_write_cmd(i2c_client,	ZINITIX_CLEAR_INT_STATUS_CMD);	
	mdelay(10);
	if (ts_write_cmd(i2c_client,
		ZINITIX_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;
	if (ts_write_cmd(i2c_client,
		ZINITIX_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;
	mdelay(10);
	ts_write_cmd(i2c_client,	ZINITIX_CLEAR_INT_STATUS_CMD);
		
	/* wait for h/w calibration*/
	do {
		mdelay(500);
		ts_write_cmd(i2c_client,
				ZINITIX_CLEAR_INT_STATUS_CMD);			
		if (ts_read_data(i2c_client,
			ZINITIX_EEPROM_INFO_REG,
			(u8 *)&chip_eeprom_info, 2) < 0)
			return false;
		TPD_DMESG("touch eeprom info = 0x%04X\r\n",
			chip_eeprom_info);
		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;
		if(time_out++ == 4){
			ts_write_cmd(i2c_client,	ZINITIX_CALIBRATE_CMD);
			mdelay(10);
			ts_write_cmd(i2c_client,
				ZINITIX_CLEAR_INT_STATUS_CMD);						
			TPD_DMESG("h/w calibration retry timeout.\n");
		}
		if(time_out++ > 10){
			TPD_DMESG("[error] h/w calibration timeout.\n");
			break;						
		}
	} while (1);
	
	if (ts_write_reg(i2c_client,
		ZINITIX_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		return false;
#if 0
	if (touch_dev->cap_info.ic_int_mask != 0)
		if (ts_write_reg(i2c_client,
			ZINITIX_INT_ENABLE_FLAG,
			touch_dev->cap_info.ic_int_mask)
			!= I2C_SUCCESS)
			return false;
#endif
	ts_write_reg(i2c_client, 0xc003, 0x0001);
	ts_write_reg(i2c_client, 0xc104, 0x0001);
	udelay(100);
	if (ts_write_cmd(i2c_client,
		ZINITIX_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;
	mdelay(1000);	
	ts_write_reg(i2c_client, 0xc003, 0x0000);
	ts_write_reg(i2c_client, 0xc104, 0x0000);
	return true;				

}

#define TC_SECTOR_SZ		8

static s32 ts_read_firmware_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	return ts_read_data(client, reg, values, length);
}

static u8 ts_upgrade_firmware(const u8 *firmware_data, u32 size)
{
	u16 flash_addr;
	u8 *verify_data;
	int retry_cnt = 0;
	int i;
	int page_sz = 64;
	u16 chip_code;

	verify_data = kzalloc(size, GFP_KERNEL);
	if (verify_data == NULL) {
		TPD_DMESG(KERN_ERR "cannot alloc verify buffer\n");
		return false;
	}

retry_upgrade:
	//ts_power_control(touch_dev, POWER_OFF);
    hwPowerDown(MT6323_POWER_LDO_VGP2,"TP"); 
	hwPowerDown(MT6323_POWER_LDO_VGP1,"CTP");
	//ts_power_control(touch_dev, POWER_ON);
	hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_3300,"CTP");
	hwPowerOn(MT6323_POWER_LDO_VGP2,VOL_2800,"TP"); 
	mdelay(10);

	if (ts_write_reg(i2c_client, 0xc000, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (vendor cmd enable)\n");
		goto fail_upgrade;
	}
	udelay(10);
	if (ts_read_data(i2c_client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		TPD_DMESG("fail to read chip code\n");
		goto fail_upgrade;
	}
	TPD_DMESG("chip code = 0x%x\n", chip_code);
	if(chip_code == 0xf400) {
//		touch_dev->cap_info.is_zmt200 = 0;
		page_sz = 128;
	} else {
//		touch_dev->cap_info.is_zmt200 = 1;
		page_sz = 64;
	}
		
	udelay(10);	
	if (ts_write_cmd(i2c_client, 0xc004) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (intn clear)\n");
		goto fail_upgrade;
	}
	udelay(10);
	if (ts_write_reg(i2c_client, 0xc002, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("power sequence error (nvm init)\n");
		goto fail_upgrade;
	}

	TPD_DMESG(KERN_INFO "init flash\n");
	if (ts_write_reg(i2c_client, 0xc003, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("fail to write nvm vpp on\n");
		goto fail_upgrade;
	}

	if (ts_write_reg(i2c_client, 0xc104, 0x0001) != I2C_SUCCESS){
		TPD_DMESG("fail to write nvm wp disable\n");
		goto fail_upgrade;
	}
	
	if (ts_write_cmd(i2c_client, ZINITIX_INIT_FLASH) != I2C_SUCCESS) {
		TPD_DMESG(KERN_INFO "failed to init flash\n");
		goto fail_upgrade;
	}
	
	TPD_DMESG(KERN_INFO "writing firmware data\n");
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz/TC_SECTOR_SZ; i++) {
			TPD_DMESG("write :addr=%04x, len=%d\n",	flash_addr, TC_SECTOR_SZ);
			if (ts_write_data(i2c_client,
				ZINITIX_WRITE_FLASH,
				(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ) < 0) {
				TPD_DMESG(KERN_INFO"error : write zinitix tc firmare\n");
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
			udelay(100);
		}
//		if(touch_dev->cap_info.is_zmt200 == 0)
			mdelay(30);	//for fuzing delay
//		else
//			mdelay(15);
	}

	if (ts_write_reg(i2c_client, 0xc003, 0x0000) != I2C_SUCCESS){
		TPD_DMESG("nvm write vpp off\n");
		goto fail_upgrade;
	}

	if (ts_write_reg(i2c_client, 0xc104, 0x0000) != I2C_SUCCESS){
		TPD_DMESG("nvm wp enable\n");
		goto fail_upgrade;
	}

	TPD_DMESG(KERN_INFO "init flash\n");
	if (ts_write_cmd(i2c_client, ZINITIX_INIT_FLASH) != I2C_SUCCESS) {
		TPD_DMESG(KERN_INFO "failed to init flash\n");
		goto fail_upgrade;
	}
	
	TPD_DMESG(KERN_INFO "read firmware data\n");
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz/TC_SECTOR_SZ; i++) {
			//TPD_DMESG("read :addr=%04x, len=%d\n", flash_addr, TC_SECTOR_SZ);
			if (ts_read_firmware_data(i2c_client,
				ZINITIX_READ_FLASH,
				(u8*)&verify_data[flash_addr], TC_SECTOR_SZ) < 0) {
				TPD_DMESG(KERN_INFO "error : read zinitix tc firmare\n");
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
		}
	}
	/* verify */
	TPD_DMESG(KERN_INFO "verify firmware data\n");
	if (memcmp((u8 *)&firmware_data[0], (u8 *)&verify_data[0], size) == 0) {
		TPD_DMESG(KERN_INFO "upgrade finished\n");
		kfree(verify_data);
		//ts_power_control(touch_dev, POWER_OFF);
		hwPowerDown(MT6323_POWER_LDO_VGP2,"TP"); 
    	hwPowerDown(MT6323_POWER_LDO_VGP1,"CTP");
        //ts_power_control(touch_dev, POWER_ON_SEQUENCE);
		hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_3300,"CTP");
    	hwPowerOn(MT6323_POWER_LDO_VGP2,VOL_2800,"TP"); 
		
		return true;
	}
	
	
fail_upgrade:
    //ts_power_control(touch_dev, POWER_OFF);
    hwPowerDown(MT6323_POWER_LDO_VGP2,"TP"); 
	hwPowerDown(MT6323_POWER_LDO_VGP1,"CTP");
	if (retry_cnt++ < ZINITIX_INIT_RETRY_CNT) {
		TPD_DMESG(KERN_INFO "upgrade fail : so retry... (%d)\n", retry_cnt);
		goto retry_upgrade;		
	}

	if (verify_data != NULL)
		kfree(verify_data);

	TPD_DMESG(KERN_INFO "upgrade fail..\n");
	return false;


}
#endif

#ifdef TPD_ESD_CHECK	
static void ctp_read_id_workqueue_esd(struct work_struct *work)
{
	char data;
	int retval = TPD_OK;
	u16 chip_code;
	u16 ic_revision;
	u16 firmware_version;
	u16 reg_data_version;

	TPD_DMESG("%s, %d\n", __FUNCTION__, __LINE__); 
	if(tpd_halt) {
		return;
	}

	//read CTP inform..
	TPD_DMESG("read bt532 inform...\r\n");
	if (ts_read_data(i2c_client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		goto esd_fail_reset;
	}
	TPD_DMESG("bt532 touch chip code = %d\r\n", chip_code);
	
	if (ts_read_data(i2c_client, ZINITIX_FIRMWARE_VERSION, (u8 *)&firmware_version, 2) < 0) {
		goto esd_fail_reset;
	}
	TPD_DMESG("bt532 touch firmware version = %d\r\n", firmware_version);

	if (ts_read_data(i2c_client, ZINITIX_DATA_VERSION_REG, (u8 *)&reg_data_version, 2) < 0) {
		goto esd_fail_reset;
	}
	TPD_DMESG("bt532 touch reg data version = %d\r\n", reg_data_version);

	if (ts_read_data(i2c_client, ZINITIX_CHIP_REVISION, (u8 *)&ic_revision, 2) < 0) {
		goto esd_fail_reset;
	}
	TPD_DMESG("bt532 touch chip version = %d\r\n", ic_revision);

	return;

esd_fail_reset:
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	//CTP power off
	hwPowerDown(MT6323_POWER_LDO_VGP2,"TP"); 
	hwPowerDown(MT6323_POWER_LDO_VGP1,"CTP");
    	msleep(CHIP_ON_DELAY);
	//CTP power on
	hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_3300,"CTP");
	hwPowerOn(MT6323_POWER_LDO_VGP2,VOL_2800,"TP");
	msleep(CHIP_ON_DELAY);
	
    	ts_power_sequence(i2c_client);
    
    	retval = ts_init_touch();
	TPD_DEBUG("esd ts_init_touch() retval=%d\n", retval);

    	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    
    	//mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE); //level
   	 //mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_LOW, tpd_eint_interrupt_handler, 0);
    	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	tpd_clear_report_data();
	
	msleep(CHIP_ON_DELAY);
	queue_delayed_work(ctp_read_id_workqueue, &ctp_read_id_work, 400); //schedule a work for the first detection
}
#endif


static void tpd_clear_report_data(void)
{
	int i;
	u8 reported = false;

#if	0
#ifdef TPD_HAVE_BUTTON
	for (i = 0; i < TPD_KEY_COUNT; i++) {
		if (button[i] == ICON_BUTTON_DOWN) {
			button[i] = ICON_BUTTON_UP;
			input_report_key(tpd->dev, tpd_keys_local[i], 0);
			reported = true;
			TPD_DMESG(TPD_DEVICE "button up = %d \r\n", i);
		}
	}
#endif

	 for (i = 0; i < MAX_SUPPORTED_FINGER_NUM; i++) {		 
		 if (zinitix_bit_test(reported_touch_info.coord[i].sub_status, SUB_BIT_EXIST)) {
			 input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i); 		   
			 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, (u32)0);
			 input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, (u32)0);
			 input_report_abs(tpd->dev, ABS_MT_POSITION_X, reported_touch_info.coord[i].x);
			 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, reported_touch_info.coord[i].y);  
			 input_mt_sync(tpd->dev);
			 reported = true;
		 }
		 reported_touch_info.coord[i].sub_status = 0;
	 }
	 
	 if (reported == true) {
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 input_sync(tpd->dev);
	 }
#else
	memset((void*)&reported_touch_info, 0x00, sizeof(reported_touch_info));
	memset((void*)&touch_info, 0x00, sizeof(touch_info));
	input_sync(tpd->dev);
#endif
}

static void tpd_down(s32 x, s32 y, s32 p)
{
	//input_report_abs(tpd->dev, ABS_PRESSURE, p);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p);
	input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, p);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p);
	input_mt_sync(tpd->dev);
	TPD_DMESG("down: x=%d, y=%d\n", x, y);

#ifdef TPD_HAVE_BUTTON
	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
	{
		tpd_button(x, y, 1);  
	}
#endif
	if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	{
		//msleep(50);
		TPD_DMESG("D virtual key \n");
	}
	TPD_EM_PRINT(x, y, x, y, p-1, 1);
 }
 
static void tpd_up(s32 x, s32 y, s32 *count)
{
	//input_report_abs(tpd->dev, ABS_PRESSURE, 0);
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	//input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
	//input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	//input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(tpd->dev);
	TPD_DMESG("down: x=%d, y=%d\n", x, y);

#ifdef TPD_HAVE_BUTTON
	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
	{
		tpd_button(x, y, 0);
	}
#endif
	TPD_EM_PRINT(x, y, x, y, 0, 0);
}

static int tpd_touchinfo(void)
{
	int i = 0;
	u8 reported = false;
	u32 x, y;
	u32 w;

#ifdef TPD_HAVE_BUTTON
	u16 icon_event_reg;
#endif

	ts_write_cmd(i2c_client, ZINITIX_CLEAR_INT_STATUS_CMD);
	//TPD_DMESG("bt532 clear int status\n");

	if(tpd_halt) {
		TPD_DMESG("bt532 suspend halt status...\n");
		return false;
	}

	if (ts_read_data(i2c_client, ZINITIX_POINT_STATUS_REG, (u8 *)&(touch_info), sizeof(struct _ts_zinitix_point_info)) < 0) {
		TPD_DMESG(KERN_INFO "tpd_touchinfo : I2C Error\n");
		return false;
	}

	//TPD_DMESG("tpd_touchinfo : I2C OK\n");
cont_tpd_touchinfo:

#ifdef TPD_HAVE_BUTTON
	if (zinitix_bit_test(touch_info.status, BIT_ICON_EVENT)) {
		udelay(DELAY_FOR_POST_TRANSCATION);
		if (ts_read_data(i2c_client, ZINITIX_ICON_STATUS_REG, (u8 *)(&icon_event_reg), 2) < 0) {
			TPD_DMESG(KERN_INFO "error read icon info using i2c.\n");
			return false;
		}
		//TPD_DMESG("tpd_icon_event, icon_event_reg=%d\n", icon_event_reg);
		//return true;
	}
#endif

	//move it to the begin of this function to clear the interrupt status
	//ts_write_cmd(i2c_client, ZINITIX_CLEAR_INT_STATUS_CMD);

	/* invalid : maybe periodical repeated int. */
	if (touch_info.status == 0x0) {
		TPD_DMESG("touch_info.status == NULL\n");
		return false;
	}

#ifdef TPD_HAVE_BUTTON
	if (zinitix_bit_test(touch_info.status, BIT_ICON_EVENT)) {
		for (i = 0; i < TPD_KEY_COUNT; i++) {
			if (zinitix_bit_test(icon_event_reg, (BIT_O_ICON0_DOWN+i))) {
				button[i] = ICON_BUTTON_DOWN;
			#if	0
				input_report_key(tpd->dev, tpd_keys_local[i], 1);
			#else
				x = tpd_keys_dim_local[i][0];
				y = tpd_keys_dim_local[i][1];
				//input_report_key(tpd->dev, BTN_TOUCH, 1);
				//input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i);
				//input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, (u32)i);
				//input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, (u32)i);
				//input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
				//input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
				tpd_down(x, y, i);
			#endif
				reported = true;
				//TPD_DMESG(TPD_DEVICE "button down = %d \r\n", i);
			}
		}
		for (i = 0; i < TPD_KEY_COUNT; i++) {
			if (zinitix_bit_test(icon_event_reg, (BIT_O_ICON0_UP+i))) {
				button[i] = ICON_BUTTON_UP;
			#if	0
				input_report_key(tpd->dev, tpd_keys_local[i], 0);
			#else
				x = tpd_keys_dim_local[i][0];
				y = tpd_keys_dim_local[i][1];
				//input_report_key(tpd->dev, BTN_TOUCH, 0);
				tpd_up(x, y, NULL);
			#endif
				reported = true;
				//TPD_DMESG(TPD_DEVICE "button up = %d \r\n", i);
			}
		}
		//input_mt_sync(tpd->dev);
		input_sync(tpd->dev);
		//TPD_DMESG(TPD_DEVICE "button event\r\n");
		return true;
	}
#endif

	/* if button press or up event occured... */
	if (reported == true || !zinitix_bit_test(touch_info.status, BIT_PT_EXIST)) {
		for (i = 0; i < MAX_SUPPORTED_FINGER_NUM; i++) {
			if (zinitix_bit_test(reported_touch_info.coord[i].sub_status, SUB_BIT_EXIST)) {
				//TPD_DMESG(TPD_DEVICE "finger [%02d] up end \r\n", i);
			#if	0
				input_report_key(tpd->dev, BTN_TOUCH, 0);
				input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i);
				input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, (u32)0);
				input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, (u32)0);
				input_report_abs(tpd->dev, ABS_MT_POSITION_X, reported_touch_info.coord[i].x);
				input_report_abs(tpd->dev, ABS_MT_POSITION_Y, reported_touch_info.coord[i].y);
				input_mt_sync(tpd->dev);
			#else
				x = reported_touch_info.coord[i].x;
				y = reported_touch_info.coord[i].y;
				tpd_up(x, y, NULL);
			#endif
			}
		}
		//input_report_key(tpd->dev, BTN_TOUCH, 0);
		memset(&reported_touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));
		input_sync(tpd->dev);
		if(reported == true)
			udelay(100);
		return true;
	}

	//if down event occure....
	for (i = 0; i < MAX_SUPPORTED_FINGER_NUM; i++) {
		if (zinitix_bit_test(touch_info.coord[i].sub_status, SUB_BIT_DOWN)
			|| zinitix_bit_test(touch_info.coord[i].sub_status, SUB_BIT_MOVE)
			|| zinitix_bit_test(touch_info.coord[i].sub_status, SUB_BIT_EXIST)) {

			x = touch_info.coord[i].x;
			y = touch_info.coord[i].y;
			w = touch_info.coord[i].width;

			//TPD_DMESG(TPD_DEVICE "finger [%02d] down x = %d, y = %d \r\n", i, x, y);
			if (w == 0) {
				w = 1;
			}
		#if	0
			input_report_key(tpd->dev, BTN_TOUCH, 1);
			input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i);
			input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, (u32)w);
			input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, (u32)w);
			input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
			input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
			input_mt_sync(tpd->dev);
		#else
			tpd_down(x, y, i);
		#endif
		} else if (zinitix_bit_test(touch_info.coord[i].sub_status, SUB_BIT_UP)) {
			//TPD_DMESG(TPD_DEVICE "finger [%02d] up \r\n", i);
			memset(&touch_info.coord[i], 0x0, sizeof(struct _ts_zinitix_coord));
		#if	0
			input_report_key(tpd->dev, BTN_TOUCH, 0);
			input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i);
			input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, (u32)0);
			input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, (u32)0);
			input_report_abs(tpd->dev, ABS_MT_POSITION_X, reported_touch_info.coord[i].x);
			input_report_abs(tpd->dev, ABS_MT_POSITION_Y, reported_touch_info.coord[i].y);
			input_mt_sync(tpd->dev);
		#else
			x = reported_touch_info.coord[i].x;
			y = reported_touch_info.coord[i].y;
			tpd_up(x, y, NULL);
		#endif
		} else {
			memset(&touch_info.coord[i], 0x0, sizeof(struct _ts_zinitix_coord));
		}
	}
	memcpy((char *)&reported_touch_info, (char *)&touch_info, sizeof(struct _ts_zinitix_point_info));
//	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_sync(tpd->dev);
	//TPD_DMESG("tpd_touchinfo end \r\n");
	return true;
}

static int touch_event_handler(void *unused)
{
	//struct sched_param param = {.sched_priority = RTPM_PRIO_TPD};
	//sched_setscheduler(current, SCHED_RR, &param);

	do
	{
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag!=0);
		tpd_flag = 0;
		set_current_state (TASK_RUNNING);
		
		//TPD_DMESG("touch_event_handler\n");
		if (tpd_touchinfo()) {
			TPD_DEBUG_SET_TIME;
		}
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	}while(!kthread_should_stop());
	
	return 0;
 }


static int tpd_detect (struct i2c_client *client, int kind, struct i2c_board_info *info) 
{
	TPD_DEBUG("mtk_tpd: enter bt532 tpd_detect\n");
	strcpy(info->type, TPD_DEVICE);	
	return 0;
}
 
static void tpd_eint_interrupt_handler(void)
{
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	//TPD_DMESG("TPD interrup has been triggered\n");    
	TPD_DEBUG_PRINT_INT;
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
}
 
static bool ts_init_touch(void)
{
	u16 reg_mask = 0x880f;	 
	int i; 
	u16 ic_revision;
	u16 firmware_version;
	u16 minor_firmware_version;	
	u16 reg_data_version;
	u16 chip_eeprom_info;
#if USE_CHECKSUM	
	u16 chip_check_sum;
	u8 checksum_err;
    u32 ic_fw_size;
#endif	
//	int retry_cnt = 0;
	u16 res_x, res_y;
 
	 memset(&reported_touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));
	 
	 //TPD_DMESG("send reset command\r\n");	
	 for (i = 0; i < 10; i++) {
		 if (ts_write_cmd(i2c_client ,ZINITIX_SWRESET_CMD) == 0)
			 break;
		 mdelay(10);
	 }
 
	 if (i == 10) {
		 TPD_DMESG(KERN_INFO "fail to send reset command\r\n");
		 goto fail_init;
	 }

    // read inform..
    //TPD_DMESG("read inform...\r\n");
    if (ts_read_data(i2c_client, ZINITIX_FIRMWARE_VERSION,
        (u8 *)&firmware_version, 2) < 0)
        goto fail_init;
	TPD_DMESG("bt532 touch firmware version = %d\r\n", firmware_version);

    if (ts_read_data(i2c_client, ZINITIX_DATA_VERSION_REG,
        (u8 *)&reg_data_version, 2) < 0)
        goto fail_init;
	TPD_DMESG("bt532 touch reg data version = %d\r\n", reg_data_version);

    if (ts_read_data(i2c_client, ZINITIX_CHIP_REVISION,
        (u8 *)&ic_revision, 2) < 0) 
        goto fail_init;
	TPD_DMESG("bt532 touch chip version = %d\r\n", ic_revision);
	
	 //set touch_mode
	 if (ts_write_reg(i2c_client, ZINITIX_TOUCH_MODE, TOUCH_MODE) != 0)
		 goto fail_init;
 
	 if (ts_write_reg(i2c_client ,ZINITIX_SUPPORTED_FINGER_NUM,(u16)MAX_SUPPORTED_FINGER_NUM) != 0)
		 goto fail_init;
	 
#if USE_CHECKSUM	
     ic_fw_size = 32*1024;
#endif

#if TOUCH_ONESHOT_UPGRADE

	if (ts_read_data(i2c_client, ZINITIX_DATA_VERSION_REG, (u8 *)&reg_data_version, 2) < 0)
		goto fail_init;
	TPD_DMESG(KERN_INFO "touch reg data version = %d\r\n", reg_data_version);

	if (ts_read_data(i2c_client, ZINITIX_MINOR_FW_VERSION, (u8 *)&minor_firmware_version, 2) < 0)
        goto fail_init;
	TPD_DMESG(KERN_INFO "bt532 touch minor firmware version = %d\r\n", minor_firmware_version);

	if (ts_check_need_upgrade(firmware_version, minor_firmware_version, reg_data_version) == true) {
		TPD_DMESG(KERN_INFO "zinitix start upgrade firmware\n");
		
		if(ts_upgrade_firmware(m_pFirmware[m_FirmwareIdx], ic_fw_size) == false)
			goto fail_init;

		if(ts_hw_calibration() == false)
			goto fail_init;

		/* disable chip interrupt */
		if (ts_write_reg(i2c_client, ZINITIX_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;

		/* get chip firmware version */
		if (ts_read_data(i2c_client, ZINITIX_FIRMWARE_VERSION, (u8 *)&firmware_version, 2) < 0)
			goto fail_init;
		TPD_DMESG(KERN_INFO "touch chip firmware version = %x\r\n", firmware_version);

		if (ts_read_data(i2c_client, ZINITIX_MINOR_FW_VERSION, (u8 *)&minor_firmware_version, 2) < 0)
			goto fail_init;
		TPD_DMESG(KERN_INFO "touch chip firmware version = %x\r\n", minor_firmware_version);
	}
	else {
		TPD_DMESG("zinitix bt532 no need to upgrade firmware\n");
	}
#endif

	if (ts_write_reg(i2c_client, ZINITIX_X_RESOLUTION, (u16)TPD_RES_MAX_X) < 0)
		goto fail_init;
	if (ts_read_data(i2c_client, ZINITIX_X_RESOLUTION, (u8 *)&res_x, 2) < 0)
		goto fail_init;
	if(res_x != TPD_RES_MAX_X)
		goto fail_init;
	TPD_DMESG("bt532 touch x resolutin = %d\r\n", TPD_RES_MAX_X);

	if (ts_write_reg(i2c_client, ZINITIX_Y_RESOLUTION, (u16)TPD_RES_MAX_Y) < 0)
		goto fail_init;
	if (ts_read_data(i2c_client, ZINITIX_Y_RESOLUTION, (u8 *)&res_y, 2) < 0)
		goto fail_init;
	if(res_y != TPD_RES_MAX_Y)
		goto fail_init;
	TPD_DMESG("bt532 touch y resolutin = %d\r\n", TPD_RES_MAX_Y);
	
	if (ts_read_data(i2c_client,
		ZINITIX_DATA_VERSION_REG,
		(u8 *)&reg_data_version, 2) < 0)
		goto fail_init;
	TPD_DMESG("bt532 touch reg data version = %d\r\n", reg_data_version);

	if (ts_read_data(i2c_client,
		ZINITIX_EEPROM_INFO_REG,
		(u8 *)&chip_eeprom_info, 2) < 0)
		goto fail_init;
  	TPD_DMESG("bt532 touch eeprom info = 0x%04X\r\n", chip_eeprom_info);
 
     if (ts_write_reg(i2c_client, ZINITIX_INT_ENABLE_FLAG, reg_mask) !=0)
		 goto fail_init;
     TPD_DMESG("bt532 touch ZINITIX_INT_ENABLE_FLAG = 0x%04X\r\n", reg_mask);
	 /* read garbage data */
	 for (i = 0; i < 10; i++) {
		 ts_write_cmd(i2c_client , ZINITIX_CLEAR_INT_STATUS_CMD);
		 udelay(10);
	 }
	 TPD_DMESG("successfully initialized\r\n");
	 TPD_DMESG("%s, %d, return true\n", __FUNCTION__, __LINE__); 
	 return true;
 
 fail_init:
	TPD_DMESG("%s, %d, return false\n", __FUNCTION__, __LINE__); 
	 return false;
 }

 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
	int retval = TPD_OK;
	
    	int reset_count = 0;
#ifdef TPD_ESD_CHECK	
	int ret;
#endif
	char data;
	i2c_client = client;
    	TPD_DEBUG("mtk_tpd: enter bt532 tpd_i2c_probe\n");
	TPD_DMESG("bt532  tpd_probe\n");

#ifdef TPD_HIGH_SPEED_DMA
	CTPI2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, CTP_I2C_DMA_BUF_SIZE, &CTPI2CDMABuf_pa, GFP_KERNEL);
	if(!CTPI2CDMABuf_va)
	{
		TPD_DMESG(TPD_DEVICE "dma_alloc_coherent error\n");
	}
	TPD_DMESG("CTPI2CDMABuf_va=0x%x, CTPI2CDMABuf_pa=0x%x\n", CTPI2CDMABuf_va, CTPI2CDMABuf_pa);
#endif

reset_proc:    

#ifdef GPIO_CTP_EN_PIN
    mt_set_gpio_mode(GPIO_ETP_EN_PIN, GPIO_ETP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_ETP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_ETP_EN_PIN, GPIO_OUT_ONE); 
#else
//	hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_3300, "TP");
	hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_3300,"CTP");
	hwPowerOn(MT6323_POWER_LDO_VGP2,VOL_2800,"TP");
#endif
    msleep(1);

#if	0
    TPD_DMESG("set tpd reset pin to high\n");
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
#endif

	msleep(CHIP_ON_DELAY);
	ts_power_sequence(i2c_client);
	retval = ts_init_touch();
	TPD_DMESG("ts_init_touch() retval=%d\n", retval);

#ifdef TPD_RESET_ISSUE_WORKAROUND
	if(reset_count < TPD_MAX_RESET_COUNT && retval == false) {
		reset_count++;
	   	goto reset_proc;
	   }
#endif    
    
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	
    	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_LOW, tpd_eint_interrupt_handler, 0);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
        TPD_DMESG("error : not cmpatible i2c function \n");
        return -1;
    }
	
    tpd_load_status = 1;
    
    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) {
        retval = false;
        TPD_DMESG(TPD_DEVICE "failed to create touch event thread: %d\n", retval);
		return -1;
    }

#ifdef TPD_ESD_CHECK	
	ctp_read_id_workqueue = create_workqueue("ctp_read_id");
	INIT_DELAYED_WORK(&ctp_read_id_work, ctp_read_id_workqueue_esd);
	ret = queue_delayed_work(ctp_read_id_workqueue, &ctp_read_id_work, 400); //schedule a work for the first detection					
	TPD_DMESG("[CTP ESD] ret =%d\n", ret);
#endif

	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    //---------------------------------------------------------------
    TPD_DMESG("bt532 tpd_probe end\n ");
    TPD_DMESG(TPD_DEVICE "Zinitix bt532 Touch Panel Device Probe %s\n", (retval == false) ? "FAIL" : "PASS");
    return 0;
   
}

static int __devexit tpd_remove(struct i2c_client *client)
{
   
   TPD_DEBUG("mtk_tpd: enter bt532 tpd_remove\n");
#ifdef TPD_HIGH_SPEED_DMA
	if(CTPI2CDMABuf_va)
	{
		dma_free_coherent(NULL, CTP_I2C_DMA_BUF_SIZE, CTPI2CDMABuf_va, CTPI2CDMABuf_pa);
		CTPI2CDMABuf_va = NULL;
		CTPI2CDMABuf_pa = NULL;
	}
#endif
#ifdef TPD_ESD_CHECK
	destroy_workqueue(ctp_read_id_workqueue);
#endif
   return 0;
}

static int tpd_local_init(void)
{
	TPD_DMESG("mtk_tpd: enter bt532 tpd_local_init\n");
	TPD_DMESG("Focaltech bt532 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);

	if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
		TPD_DMESG(" bt532 unable to add i2c driver.\n");
		return -1;
	}
#ifdef TPD_HAVE_BUTTON     
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

	TPD_DMESG("end bt532 %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;
	return 0;
}

static void tpd_resume(struct early_suspend *h)
{
	int i;

	TPD_DMESG(TPD_DEVICE "TPD wake up\n");
	
#ifdef TDP_CLOSE_POWER_IN_SLEEP
	hwPowerOn(TPD_POWER_SOURCE, VOL_3300, "TP");
	msleep(CHIP_ON_DELAY);  
	if(ts_init_touch() == false) {
		TPD_DMESG("tpd_resume : tsp init error\n");
		return;
	}
#else
	ts_write_cmd(i2c_client, ZINITIX_WAKEUP_CMD);
	msleep(1);
	ts_write_cmd(i2c_client ,ZINITIX_SWRESET_CMD);
	msleep(20);
	for(i=0; i<10; i++)
	{
		ts_write_cmd(i2c_client, ZINITIX_CLEAR_INT_STATUS_CMD);
		mdelay(10);
	}
#endif

	tpd_clear_report_data();
	tpd_halt = 0;
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	TPD_DMESG(TPD_DEVICE "TPD wake up done\n");
	return;
}

static void tpd_suspend(struct early_suspend *h)
{
	TPD_DMESG(TPD_DEVICE "TPD enter sleep\n");
	tpd_halt = 1;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

#ifdef TPD_CLODE_POWER_IN_SLEEP
	hwPowerDown(TPD_POWER_SOURCE, "TP");
#else
	ts_write_cmd(i2c_client, ZINITIX_CLEAR_INT_STATUS_CMD);
	ts_write_cmd(i2c_client, ZINITIX_SLEEP_CMD);
#endif

	TPD_DMESG(TPD_DEVICE "TPD enter sleep done\n");
	return;
} 

static struct tpd_driver_t tpd_device_driver =
{
	.tpd_device_name = "bt532",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
	TPD_DMESG("MediaTek bt532 touch panel driver init\n");

	i2c_register_board_info(TPD_I2C_NUMBER, &zinitix_i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add bt532 driver failed\n");
	return 0;
}
 
/* should never be called */
static void __exit tpd_driver_exit(void)
{
	TPD_DMESG("MediaTek bt532 touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}
 
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);


