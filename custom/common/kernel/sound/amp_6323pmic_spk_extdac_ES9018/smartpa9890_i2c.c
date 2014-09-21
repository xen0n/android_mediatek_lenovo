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

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <linux/dma-mapping.h>

/*ioctl cmd(tmply defined urgly)*/
#define I2C_SLAVE 0x703//_IOW(SMARTPA_MAGIC, 0x10, unsigned int)//0x0703
#define SMARTPA_RESET 0x704//_IOW(SMARTPA_MAGIC, 0x10, unsigned int)//0x0703
#define SMARTPA_SET_POWER 0x705
#define TFA_I2CSLAVEBASE (0x34)

#define SMARTPA_I2C_BUSNUM 2
#define SMARTPA_DEV_NAME "SMARTPA"

#define I2C_RETRY_CNT (5)
/*When the size to be transfered is bigger than 8 bytes. I2C DMA way should be used.*/
#define MTK_I2C_DMA_THRES (8)

#define G_BUF_SIZE (1024)

#define GSE_TAG                  "[smartpa] "
#define GSE_FUN(f)               printk(KERN_ERR GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt"\n", __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_ERR GSE_TAG fmt"\n", ##args)

struct i2c_client* smartpa_i2c_client = NULL;
static u8 * DMAbuffer_va = NULL;
static dma_addr_t DMAbuffer_pa = NULL;
static char *g_buf = NULL;

struct smartpa_hw {
    MT65XX_POWER pwid;
    MT65XX_POWER_VOLTAGE vol;
};
static struct smartpa_hw hw = {MT6323_POWER_LDO_VIO18, VOL_1800};

static const struct i2c_device_id smartpa_i2c_id[] = {{SMARTPA_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_smartpa={ I2C_BOARD_INFO("SMARTPA", TFA_I2CSLAVEBASE)};

static int smartpa_open (struct inode *node, struct file *file)
{
    file->private_data = smartpa_i2c_client;
    return 0;
}

static int smartpa_release (struct inode *node, struct file *file)
{
    return 0;
}

/*There is only read operation. So before calling this API, a subaddress writing should be done.*/
int smartpa_i2c_dma_read(struct i2c_client *client, U8 *rxbuf, U16 len)
{
    int retry,i;
    int msg_cnt = 0;

    memset(DMAbuffer_va, 0, G_BUF_SIZE);

    struct i2c_msg msg[] = {
        {
            .addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG,
            .flags = I2C_M_RD,
            .len = len,
            .buf = DMAbuffer_pa,
        }
    };
    msg_cnt = sizeof(msg) / sizeof(msg[0]);
    for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
        if (i2c_transfer(client->adapter, msg, msg_cnt) == msg_cnt)
            break;
        mdelay(5);
//        GSE_LOG("Line %s, i2c_transfer error, retry = %d\n", __LINE__, retry);
    }
    if (retry == I2C_RETRY_CNT) {
        printk(KERN_ERR "i2c_read_block retry over %d\n",I2C_RETRY_CNT);
        return -EIO;
    }

    for(i=0;i<len;i++)
        rxbuf[i]=DMAbuffer_va[i];

    return 0;
}

static ssize_t smartpa_read (struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    char *tmp;
    int ret;
    struct i2c_client *client = (struct i2c_client *)file->private_data;

    if (count > 8192) {
//        GSE_ERR("Read size is bigger than 8192. Cut off to 8192.");
        count = 8192;
    }

#if 1
    if(g_buf == NULL) return -ENOMEM;
    tmp = g_buf;
    memset(tmp, 0, G_BUF_SIZE);
#else
    tmp = kmalloc(count,GFP_KERNEL);
    if (tmp==NULL)
        return -ENOMEM;
#endif

    //GSE_LOG("reading %zu bytes.\n", count);

    if(count < MTK_I2C_DMA_THRES) {
        ret = i2c_master_recv(client,tmp,count);
    } else {
//        GSE_LOG("reading %zu bytes in dma way.", count);
        ret = smartpa_i2c_dma_read(client, tmp, count);
    }
    if(ret < 0) {
//        GSE_ERR("i2c read failed!(%d)", ret);
        goto free_;
    }
    ret = copy_to_user(buf,tmp,count)?-EFAULT:ret;
free_:
    //kfree(tmp);
    return ret;
}

int smartpa_i2c_dma_write(struct i2c_client *client, U8 *txbuf, U16 len)
{
    int retry,i;
    int msg_cnt = 0;


    memset(DMAbuffer_va, 0, G_BUF_SIZE);
    for(i=0;i<len;i++)
       DMAbuffer_va[i] = txbuf[i];

    struct i2c_msg msg[] = {
        {
            .addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG,
            .flags = 0,
            .len = len,
            .buf = DMAbuffer_pa,
        }
    };
    msg_cnt = sizeof(msg) / sizeof(msg[0]);
    for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
        if (i2c_transfer(client->adapter, msg, msg_cnt) == msg_cnt)
            break;
        mdelay(5);
//        GSE_LOG("Line %s, i2c_transfer error, retry = %d\n", __LINE__, retry);
    }
    if (retry == I2C_RETRY_CNT) {
        printk(KERN_ERR "i2c_read_block retry over %d\n",I2C_RETRY_CNT);
        return -EIO;
    }

    return 0;
}

static ssize_t smartpa_write (struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int ret;
    char *tmp;
    struct i2c_client *client = (struct i2c_client *)file->private_data;
    //return 0;//debug

    if (count > 8192) {
//        GSE_ERR("Write size is bigger than 8192. Cut off to 8192.");
        count = 8192;
    }

#if 1
    if(g_buf == NULL) return -ENOMEM;
    tmp = g_buf;
    memset(tmp, 0, G_BUF_SIZE);
#else
    tmp = kmalloc(count,GFP_KERNEL);
    if (tmp==NULL)
        return -ENOMEM;
#endif

    if (copy_from_user(tmp,buf,count)) {
        //kfree(tmp);
        return -EFAULT;
    }

    //GSE_LOG("writing %zu bytes.", count);
#if 0
    if(1) {
#else
    if(count < MTK_I2C_DMA_THRES) {
#endif
        ret = i2c_master_send(client,tmp,count);
    } else {
//        GSE_LOG("writing %zu bytes in dma way.", count);
        ret = smartpa_i2c_dma_write(client, tmp, count);
    }
    if(ret < 0) {
//        GSE_ERR("i2c write failed!(%d)", ret);
    }
    //kfree(tmp);
    return ret;
}

static void smartpa_vddd_power(unsigned int on) 
{
    static unsigned int power_on = 0;

    if(hw.pwid != MT65XX_POWER_NONE) {// have externel LDO
//        GSE_LOG("power %s", on ? "on" : "off");
        if(power_on == on) {// power status not change
//            GSE_LOG("ignore power control: %d\n", on);
        }
        else if(on) {// power on
            if(!hwPowerOn(hw.pwid, hw.vol, "SMARTPA")) {
                GSE_ERR("power on failed!");
            }
        } else {// power off
            if (!hwPowerDown(hw.pwid, "SMARTPA")) {
                GSE_ERR("power off failed!");
            }
        }
    }
    power_on = on;
}

static long smartpa_unlocked_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
    struct i2c_client *client = (struct i2c_client*)file->private_data;

    switch(cmd) {
        case I2C_SLAVE:
            /*Just for giving a cmd for TFA9890 hal codes. Do nothing.*/
            //GSE_ERR( "I2C_SLAVE");
            break;
#if 1
        case SMARTPA_RESET:
            if(arg <= 0 || arg > 50000) {
                arg = 500;
            }
            GSE_ERR( "TFA9890_RESET %dms", arg);

            mt_set_gpio_mode(GPIO_NXPSPA_I2S_DATAIN_PIN, GPIO_MODE_01);
            mt_set_gpio_dir(GPIO_NXPSPA_I2S_DATAIN_PIN,GPIO_DIR_IN);
            mt_set_gpio_mode(GPIO_NXPSPA_I2S_LRCK_PIN, GPIO_MODE_03);
            mt_set_gpio_dir(GPIO_NXPSPA_I2S_LRCK_PIN,GPIO_DIR_OUT);
            mt_set_gpio_mode(GPIO_NXPSPA_I2S_BCK_PIN, GPIO_MODE_03);
            mt_set_gpio_dir(GPIO_NXPSPA_I2S_BCK_PIN,GPIO_DIR_OUT);
            mt_set_gpio_mode(GPIO_NXPSPA_I2S_DATAOUT_PIN, GPIO_MODE_03);
            mt_set_gpio_dir(GPIO_NXPSPA_I2S_DATAOUT_PIN,GPIO_DIR_OUT);

            //High then low to reset the chip.
            mt_set_gpio_mode(GPIO_SMARTPA_RST_PIN,GPIO_MODE_00);
            mt_set_gpio_dir(GPIO_SMARTPA_RST_PIN,GPIO_DIR_OUT); 
            mt_set_gpio_out(GPIO_SMARTPA_RST_PIN,GPIO_OUT_ONE); 
            msleep(arg);
            mt_set_gpio_mode(GPIO_SMARTPA_RST_PIN,GPIO_MODE_00);  // gpio mode
            mt_set_gpio_dir(GPIO_SMARTPA_RST_PIN,GPIO_DIR_OUT); // output
            mt_set_gpio_out(GPIO_SMARTPA_RST_PIN,GPIO_OUT_ZERO); // low


            break;
        case SMARTPA_SET_POWER:
            arg = (arg == 0) ? 0 : 1;
            smartpa_vddd_power(arg);
            break;
#endif
        default:
            GSE_ERR("[%s]wrong cmd(%d)", cmd);
            return -1;
    }
    return 0;
}

static struct file_operations smartpa_fops = {
	//.owner = THIS_MODULE,
	.open = smartpa_open,
	.release = smartpa_release,
	.read = smartpa_read,
	.write = smartpa_write,
	.unlocked_ioctl = smartpa_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice smartpa_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smartpa_i2c",
	.fops = &smartpa_fops,
};

static int smartpa_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct i2c_client *new_client;
    int err = 0;
    GSE_FUN();

    smartpa_i2c_client = client;

    //smart pa DMA support
    if(DMAbuffer_va == NULL) {
        DMAbuffer_va = (u8 *)dma_alloc_coherent(NULL, 4096,&DMAbuffer_pa, GFP_KERNEL);
    }
//    GSE_LOG("dma_alloc_coherent va = 0x%8x, pa = 0x%8x \n",DMAbuffer_va,DMAbuffer_pa);
    if(!DMAbuffer_va)
    {
        GSE_ERR("Allocate DMA I2C Buffer failed!\n");
        return -1;
    }

    
    g_buf = kmalloc(G_BUF_SIZE,GFP_KERNEL);
    if(g_buf == NULL) {
        GSE_ERR("kmalloc for g_buf failed!");
        return -ENOMEM;
    }

    /*Read the rev number of smart pa*/

    if(err = misc_register(&smartpa_device))
    {
        GSE_ERR("smartpa_device register failed\n");
        return err;
    }

    return err;
}

static int smartpa_i2c_remove(struct i2c_client *client)
{
    int err = 0;

    if(DMAbuffer_va) {
        dma_free_coherent(NULL, 4096, DMAbuffer_va, DMAbuffer_pa);
        DMAbuffer_va = NULL;
        DMAbuffer_pa = 0;
    }
    
    if(g_buf != NULL) {
        kfree(g_buf);
    }

    if(err = misc_deregister(&smartpa_device))
    {
        GSE_ERR("misc_deregister fail: %d\n", err);
    }

    i2c_unregister_device(client);

    return 0;
}

static struct i2c_driver smartpa_i2c_driver = {
    .driver = {
        .owner          = THIS_MODULE,
        .name           = SMARTPA_DEV_NAME,
    },
    .probe     = smartpa_i2c_probe,
    .remove    = smartpa_i2c_remove,
    .id_table = smartpa_i2c_id,
};

/*----------------------------------------------------------------------------*/
static int __init smartpa_i2c_init(void)
{
	GSE_FUN();
	i2c_register_board_info(SMARTPA_I2C_BUSNUM, &i2c_smartpa, 1);
	if(i2c_add_driver(&smartpa_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit smartpa_i2c_exit(void)
{
    GSE_FUN();
    i2c_del_driver(&smartpa_i2c_driver);
}
/*----------------------------------------------------------------------------*/
module_init(smartpa_i2c_init);
module_exit(smartpa_i2c_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SMART PA I2C driver");
MODULE_AUTHOR("somebody");




