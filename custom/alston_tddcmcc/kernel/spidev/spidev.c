  /*
  * Simple synchronous userspace interface to SPI devices
  *
  * Copyright (C) 2006 SWAPP
  *  Andrea Paterniani <a.paterniani@swapp-eng.it>
  * Copyright (C) 2007 David Brownell (simplification, cleanup)
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  */
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
 
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <mach/mt_spi.h>
#include <mach/mt_gpio.h>
#include <mach/mt_clkmgr.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/mca.h>

#define ICE40_FIRMWARE_DL          1
#define HERO2_SPIDEV

 
#ifdef ICE40_FIRMWARE_DL
#include "ice40.h"
#include <mach/mt_pm_ldo.h>
#endif
 /*
  * This supports access to SPI devices using normal userspace I/O calls.
  * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
  * and often mask message boundaries, full SPI support requires full duplex
  * transfers.  There are several kinds of internal message boundaries to
  * handle chipselect management and other protocol options.
  *
  * SPI has a character major number assigned.  We allocate minor numbers
  * dynamically using a bitmask.  You must use hotplug tools, such as udev
  * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
  * nodes, since there is no fixed association of minor numbers with any
  * particular SPI bus or device.
  */
#define SPIDEV_MAJOR            176 /* assigned */
#define N_SPI_MINORS            32  /* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);

//Add GPIO PIN defination
#define GPIO_IR_C_DONE         (GPIO41 | 0x80000000)
#define GPIO_IR_C_DONE_M_GPIO   GPIO_MODE_00
#define GPIO_IR_C_DONE_M_KROW   GPIO_MODE_06
#define GPIO_IR_C_DONE_M_PWM   GPIO_MODE_05

#define GPIO_IR_C_RESET         (GPIO42 | 0x80000000)
#define GPIO_IR_C_RESET_M_GPIO   GPIO_MODE_00
#define GPIO_IR_C_RESET_M_KROW   GPIO_MODE_06
#define GPIO_IR_C_RESET_M_PWM   GPIO_MODE_05

#define GPIO_IR_CLKM2_26M         (GPIO72 | 0x80000000)
#define GPIO_IR_CLKM2_26M_M_GPIO   GPIO_MODE_00
#define GPIO_IR_CLKM2_26M_M_CLK   GPIO_MODE_02
#define GPIO_IR_CLKM2_26M_M_VM   GPIO_MODE_01
#define GPIO_IR_CLKM2_26M_M_DBG_MON_A   GPIO_MODE_07
#define GPIO_IR_CLKM2_26M_CLK     CLK_OUT2
#define GPIO_IR_CLKM2_26M_FREQ    GPIO_CLKSRC_NONE

#define GPIO_IR_C_SPI_CS         (GPIO80 | 0x80000000)
#define GPIO_IR_C_SPI_CS_M_GPIO   GPIO_MODE_00
#define GPIO_IR_C_SPI_CS_M_SPI_CS   GPIO_MODE_01
#define GPIO_IR_C_SPI_CS_M_I2SIN1_DATA_IN   GPIO_MODE_03
#define GPIO_IR_C_SPI_CS_M_DBG_MON_B   GPIO_MODE_07

#define GPIO_IR_C_SPI_SCK         (GPIO81 | 0x80000000)
#define GPIO_IR_C_SPI_SCK_M_GPIO   GPIO_MODE_00
#define GPIO_IR_C_SPI_SCK_M_SPI_CK   GPIO_MODE_01
#define GPIO_IR_C_SPI_SCK_M_I2SIN1_LRCK   GPIO_MODE_03
#define GPIO_IR_C_SPI_SCK_M_DBG_MON_B   GPIO_MODE_07

#define GPIO_IR_C_SPI_MI         (GPIO82 | 0x80000000)
#define GPIO_IR_C_SPI_MI_M_GPIO   GPIO_MODE_00
#define GPIO_IR_C_SPI_MI_M_SPI_MI   GPIO_MODE_01
#define GPIO_IR_C_SPI_MI_M_SPI_MO   GPIO_MODE_02
#define GPIO_IR_C_SPI_MI_M_I2SIN1_BCK   GPIO_MODE_03
#define GPIO_IR_C_SPI_MI_M_DBG_MON_B   GPIO_MODE_07

#define GPIO_IR_C_SPI_MO         (GPIO83 | 0x80000000)
#define GPIO_IR_C_SPI_MO_M_GPIO   GPIO_MODE_00
#define GPIO_IR_C_SPI_MO_M_SPI_MO   GPIO_MODE_01
#define GPIO_IR_C_SPI_MO_M_SPI_MI   GPIO_MODE_02



#define SPI_CS (GPIO_IR_C_SPI_CS) //GPIO80
#define SPI_SCK (GPIO_IR_C_SPI_SCK) //GPIO81
#define SPI_MO (GPIO_IR_C_SPI_MO) //GPIO83
#define SPI_MI (GPIO_IR_C_SPI_MI) //GPIO82
#define C_RESET (GPIO_IR_C_RESET) //hero2 = GPIO42;hero8 = 12
#define C_DONE (GPIO_IR_C_DONE) //hero2 = GPIO41;hero8 = 13

#if defined(HERO2_SPIDEV)
//Add GPIO PIN defination
#define GPIO_IR_LDO1V2         (GPIO30 | 0x80000000)
#define GPIO_IR_LDO1V2_M_GPIO   GPIO_MODE_00
#define GPIO_IR_LDO1V2_M_PWM   GPIO_MODE_03


#define SPI_GPIO30_IRDALDO1V2_EN (GPIO_IR_LDO1V2) //GPIO30
#endif

//#define SPI_GPIO_TEST 0
#ifdef SPI_GPIO_TEST
#define SET_LSCE_LOW                 mt_set_gpio_out(GPIO80, 0)
#define SET_LSCE_HIGH                mt_set_gpio_out(GPIO80, 1)
#define SET_LSCK_LOW             mt_set_gpio_out(GPIO81, 0)
#define SET_LSCK_HIGH            mt_set_gpio_out(GPIO81, 1)
#define SET_LSDA_LOW                 mt_set_gpio_out(GPIO83, 0)
#define SET_LSDA_HIGH                mt_set_gpio_out(GPIO83, 1)
#endif
 
 /* Bit masks for spi_device.mode management.  Note that incorrect
  * settings for some settings can cause *lots* of trouble for other
  * devices on a shared bus:
  *
  *  - CS_HIGH ... this device will be active when it shouldn't be
  *  - 3WIRE ... when active, it won't behave as it should
  *  - NO_CS ... there will be no explicit message boundaries; this
  *  is completely incompatible with the shared bus model
  *  - READY ... transfers may proceed when they shouldn't.
  *
  * REVISIT should changing those flags be privileged?
  */
#define SPI_MODE_MASK       (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                 | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                 | SPI_NO_CS | SPI_READY)
 
extern void vibr_Enable_HW(void);
struct spidev_data {
    dev_t           devt;
    spinlock_t      spi_lock;
    struct spi_device   *spi;
    struct list_head    device_entry;

    /* buffer is NULL unless this device is open (users > 0) */
    struct mutex        buf_lock;
    unsigned        users;
    u8          *buffer;
 
#ifdef ICE40_FIRMWARE_DL
    struct delayed_work ice40_delayed_work;
    atomic_t firmware_state;
#endif
 };
 
#ifdef ICE40_FIRMWARE_DL
#define SIZE_PER_TIME (1024)
static struct spidev_data *ice40_g_spidev = NULL;
static struct workqueue_struct *ice40_wq;
static struct mt_chip_conf ice40_spi_conf;
#endif
 
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");
 
 
#ifdef ICE40_FIRMWARE_DL
static void ice40_firmware_config_gpio(int download)
{
    if (download)
    {
        //SPI_CS
        mt_set_gpio_mode(SPI_CS, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_CS, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(SPI_CS, GPIO_PULL_ENABLE);
        mt_set_gpio_out(SPI_CS, 1);
        //SPI_SCK
        mt_set_gpio_mode(SPI_SCK, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_SCK, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(SPI_SCK, GPIO_PULL_ENABLE);
        mt_set_gpio_out(SPI_SCK, 1);
        //SPI_MO
        mt_set_gpio_mode(SPI_MO, GPIO_MODE_01);
        mt_set_gpio_dir(SPI_MO, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(SPI_MO, GPIO_PULL_ENABLE);
        //SPI_MI
        mt_set_gpio_mode(SPI_MI, GPIO_MODE_01);
        mt_set_gpio_dir(SPI_MI, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(SPI_MI, GPIO_PULL_ENABLE);
        //C_RESET
        mt_set_gpio_mode(C_RESET, GPIO_MODE_GPIO);
        mt_set_gpio_dir(C_RESET, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(C_RESET, GPIO_PULL_ENABLE);
        mt_set_gpio_out(C_RESET, 1);
    }
    else
    {
        //SPI_CS
        mt_set_gpio_mode(SPI_CS, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_CS, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_CS, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_CS, GPIO_PULL_UP);
        //SPI_SCK
        mt_set_gpio_mode(SPI_SCK, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_SCK, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_SCK, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_SCK, GPIO_PULL_UP);
        //SPI_MO
        mt_set_gpio_mode(SPI_MO, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_MO, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_MO, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_MO, GPIO_PULL_UP);
        //SPI_MI
        mt_set_gpio_mode(SPI_MI, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_MI, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_MI, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_MI, GPIO_PULL_UP);
        //C_RESET
        /*mt_set_gpio_mode(C_RESET, GPIO_MODE_GPIO);
        mt_set_gpio_dir(C_RESET, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(C_RESET, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(C_RESET, GPIO_PULL_UP);*///maybe pull down the 
        // reset pin
    }
} 
 
static int ice40_firmware_spi_DMA_transfer(void)
{
    struct spidev_data *spidev = ice40_g_spidev;
    struct spi_message ice40_msg;
    struct spi_transfer ice40_transfer;

    int ret = 0;
    int i = 0;
    int img_size = ARRAY_SIZE(ice40_bin);
    int pkg_count = img_size/SIZE_PER_TIME;
    int remain_size = img_size % SIZE_PER_TIME;
    printk("[wming] ~~ ice40_firmware_spi_DMA_transfer Enter !!!\n");
    printk("[wming] ~~ data size = %d\n",img_size);

    spi_message_init(&ice40_msg);

#if 0
    for (i = 0; i < pkg_count; i++)
    {
        if (i < (pkg_count - 1))
        {
            ice40_transfer.tx_buf = &(ice40_bin[i*SIZE_PER_TIME]);
            ice40_transfer.len = SIZE_PER_TIME;
            spi_message_add_tail(&ice40_transfer, &ice40_msg);
        }
        else
        {
            ice40_transfer.tx_buf = &(ice40_bin[(pkg_count - 1)*SIZE_PER_TIME]);
            ice40_transfer.len = remain_size;
            spi_message_add_tail(&ice40_transfer, &ice40_msg);
        }
    }
#else
    ice40_transfer.tx_buf = &(ice40_bin[0]);
    ice40_transfer.rx_buf = NULL;
    ice40_transfer.len = img_size;
    spi_message_add_tail(&ice40_transfer, &ice40_msg);
#endif
    ice40_msg.spi = spidev->spi;
    ret = spi_sync(spidev->spi, &ice40_msg);
    if (ret < 0)
    {
        printk("[wming] ~~ spi transfer fail\n");
        return -1;
    }

    printk("[wming] ~~ ice40_firmware_spi_DMA_transfer Exit !!!\n");
    return 0;
}
 
static int ice40_download_firmware(void)
{
    int ret = 0;

    //supply IC power
    //hwPowerOn(MT6323_POWER_LDO_VGP3,VOL_2800,"ice40_fw_2V8");
    //config gpio 
    ice40_firmware_config_gpio(1);//enable spi & c_reset
    udelay(1000);
    printk("[wming] ~~ prepare firmware download\n");
    //match IC time sequence
    mt_set_gpio_out(SPI_CS, 0);//cs low
    mt_set_gpio_out(SPI_SCK, 1);//sck high
    mt_set_gpio_out(SPI_MI, 1);//so high
    mt_set_gpio_out(C_RESET, 0);
    udelay(1000);//200 ns at least
    mt_set_gpio_out(C_RESET, 1);
    udelay(1000);//800 us at least
    mt_set_gpio_mode(SPI_SCK, GPIO_MODE_01);   //spi clk config

    ret = ice40_firmware_spi_DMA_transfer();
    if (ret < 0)
    {
        printk("[wming] ~~ firmware download fail\n");
    }
    else
    {
        atomic_set(&ice40_g_spidev->firmware_state, 1);
    }

    mt_set_gpio_out(SPI_CS, 1);//cs high

    ice40_firmware_config_gpio(0);//disable spi & c_reset

	//Disable 26M clk pin
	mt_set_gpio_mode(GPIO72, GPIO_MODE_GPIO);
    mt_set_gpio_dir(GPIO72, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO72, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO72, GPIO_PULL_DOWN);

    //config_c_cdone_gpio();

    //read_c_cdone_gpio();

    //hwPowerDown(MT6323_POWER_LDO_VGP3,"ice40_fw_2V8");

    printk("[wming] ~~ ice40 firmware download %s\n",(ret < 0)?"fail":"success");
    return ret;
}
 
static void ice40_spi_download_func(struct work_struct *work)
{
    int ret;
    struct spidev_data *spidev = ice40_g_spidev;
    printk("[wming] ~~ ice40_spi_download_func Enter !!!\n");    

    mutex_lock(&spidev->buf_lock);
    ret = ice40_download_firmware();
    mutex_unlock(&spidev->buf_lock);

}
#endif
 
static void config_spi_gpio(int download)
{
    if (download)
    {
        //SPI_CS
        mt_set_gpio_mode(SPI_CS, GPIO_MODE_01);
        mt_set_gpio_dir(SPI_CS, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(SPI_CS, GPIO_PULL_ENABLE);
        //mt_set_gpio_out(SPI_CS, GPIO_OUT_ZERO);
        //SPI_SCK
        mt_set_gpio_mode(SPI_SCK, GPIO_MODE_01);
        mt_set_gpio_dir(SPI_SCK, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(SPI_SCK, GPIO_PULL_ENABLE);
        //SPI_MO
        mt_set_gpio_mode(SPI_MO, GPIO_MODE_01);
        mt_set_gpio_dir(SPI_MO, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(SPI_MO, GPIO_PULL_ENABLE);
        //SPI_MI
        mt_set_gpio_mode(SPI_MI, GPIO_MODE_01);
        mt_set_gpio_dir(SPI_MI, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(SPI_MI, GPIO_PULL_ENABLE);
    }
    else
    {
        //SPI_CS
        mt_set_gpio_mode(SPI_CS, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_CS, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_CS, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_CS, GPIO_PULL_UP);
        //SPI_SCK
        mt_set_gpio_mode(SPI_SCK, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_SCK, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_SCK, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_SCK, GPIO_PULL_UP);
        //SPI_MO
        mt_set_gpio_mode(SPI_MO, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_MO, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_MO, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_MO, GPIO_PULL_UP);
        //SPI_MI
        mt_set_gpio_mode(SPI_MI, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_MI, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(SPI_MI, GPIO_PULL_DISABLE);
		//mt_set_gpio_pull_select(SPI_MI, GPIO_PULL_UP);
    }
}
 
static void config_c_reset_gpio(int download) 
{
    if (download)
    {
        //C_RESET
        mt_set_gpio_mode(C_RESET, GPIO_MODE_GPIO);
        mt_set_gpio_dir(C_RESET, GPIO_DIR_OUT);
        mt_set_gpio_pull_enable(C_RESET, GPIO_PULL_ENABLE);
        mt_set_gpio_out(C_RESET, 0);
        mdelay(1);
        //mdelay(15);
        mt_set_gpio_out(C_RESET, 1);
    }
    else
    {
        //C_RESET
        mt_set_gpio_mode(C_RESET, GPIO_MODE_GPIO);
        mt_set_gpio_dir(C_RESET, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(C_RESET, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(C_RESET, GPIO_PULL_UP);
    }
}
 
 /*-------------------------------------------------------------------------*/
 
 /*
  * We can't use the standard synchronous wrappers for file I/O; we
  * need to protect against async removal of the underlying spi_device.
  */
static void spidev_complete(void *arg)
{
    complete(arg);
}
 
static ssize_t
spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int status;

    message->complete = spidev_complete;
    message->context = &done;

    spin_lock_irq(&spidev->spi_lock);
    if (spidev->spi == NULL)
    status = -ESHUTDOWN;
    else
    status = spi_async(spidev->spi, message);
    spin_unlock_irq(&spidev->spi_lock);

    if (status == 0)
    {
        wait_for_completion(&done);
        status = message->status;
        if (status == 0)
        status = message->actual_length;
    }
    return status;
}
 
static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
    struct spi_transfer t = {
        .tx_buf     = spidev->buffer,
        .len        = len,
    };
    struct spi_message  m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}
 
static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
    struct spi_transfer t = {
        .rx_buf     = spidev->buffer,
        .len        = len,
    };
    struct spi_message  m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}
 
 /*-------------------------------------------------------------------------*/
 
/* Read-only message with current device setup */
static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct spidev_data  *spidev;
    ssize_t         status = 0;

    printk("[wming] --- spidev_read Enter!!!\n");

    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    spidev = filp->private_data;

    mutex_lock(&spidev->buf_lock);
    status = spidev_sync_read(spidev, count);
    if (status > 0)
    {
        unsigned long   missing;

        missing = copy_to_user(buf, spidev->buffer, status);
        if (missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }
    mutex_unlock(&spidev->buf_lock);

    return status;
}
 
 /* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
         size_t count, loff_t *f_pos)
{
    struct spidev_data  *spidev;
    ssize_t         status = 0;
    unsigned long       missing;

    printk("[wming] --- spidev_write Enter!!!\n");
    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    spidev = filp->private_data;

    mutex_lock(&spidev->buf_lock);
    missing = copy_from_user(spidev->buffer, buf, count);
    if (missing == 0) 
    {
        status = spidev_sync_write(spidev, count);
    } 
    else
        status = -EFAULT;
    mutex_unlock(&spidev->buf_lock);

    return status;
}
 
 
#ifdef SPI_GPIO_TEST
static DEFINE_SPINLOCK(ir_send_spi);
static void spi_gpio_send_data(unsigned char data)
{
    unsigned int i;
    unsigned long flags;

    spin_lock_irqsave(&ir_send_spi,flags);

    SET_LSCE_HIGH;
    SET_LSCK_LOW;
    SET_LSDA_HIGH;

    SET_LSCE_LOW;

    udelay(10);

    for (i = 0; i < 8; ++i)
    {

        if (data & (1 << 8))
        {
            SET_LSDA_HIGH;
        } 
        else 
        {
            SET_LSDA_LOW;
        }
        data <<= 1;

        udelay(2);
        SET_LSCK_HIGH;
        udelay(2);
        SET_LSCK_LOW;
    }
    printk("\r\nspi_gpio_send_data end\r\n");
    udelay(10);
    SET_LSCE_HIGH;

    printk("ir_unlock\n");

    spin_unlock_irqrestore(&ir_send_spi,flags);
}
#endif
 
 
static int spidev_message(struct spidev_data *spidev,
         struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
    struct spi_message  msg;
    struct spi_transfer *k_xfers;
    struct spi_transfer *k_tmp;
    struct spi_ioc_transfer *u_tmp;
    unsigned        n, total;
    u8          *buf,*buf_tmp;
    int         status = -EFAULT;
    char *sendbuff_tmp;

    int j = 0;

    printk("[wming] --- spidev_message enter !!!");

    spi_message_init(&msg);
    k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if (k_xfers == NULL)
    return -ENOMEM;

    /* Construct spi_message, copying any tx data to bounce buffer.
    * We walk the array of user-provided transfers, using each one
    * to initialize a kernel version of the same transfer.
    */
    buf = spidev->buffer;
    buf_tmp = buf;
    total = 0;
    for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
        n;
        n--, k_tmp++, u_tmp++) 
    {
        k_tmp->len = u_tmp->len;

        total += k_tmp->len;
        if (total > bufsiz) 
        {
            status = -EMSGSIZE;
            goto done;
        }

        if (u_tmp->rx_buf) 
        {
            k_tmp->rx_buf = buf;
            if (!access_ok(VERIFY_WRITE, (u8 __user *)
            (uintptr_t) u_tmp->rx_buf,
            u_tmp->len))
                goto done;
        }
        
        if (u_tmp->tx_buf) 
        {
            k_tmp->tx_buf = buf;
            if (copy_from_user(buf, (const u8 __user *)
            (uintptr_t) u_tmp->tx_buf,
            u_tmp->len))
                goto done;
        }
        //printk("\r\n spidev_message k_tmp->len = %d\r",k_tmp->len);
        //printk("\r\n spidev_message buf = %x %x %x %x %x\r",buf[0],buf[1],buf[2],buf[3],buf[4]);
#if 0 //add by zhlu for send tmp data
        k_tmp->len = 6;
        buf[0]= 0xF0;
        buf[1]= 0x01;
        buf[2]= 0x03;
        buf[3]= 0x07;
        buf[4]= 0x55;
        buf[5]= 0x0F;
#endif

        buf += k_tmp->len;
        k_tmp->cs_change = !!u_tmp->cs_change;
        k_tmp->bits_per_word = u_tmp->bits_per_word;
        k_tmp->delay_usecs = u_tmp->delay_usecs;
        k_tmp->speed_hz = u_tmp->speed_hz;
        /*printk(
        "  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
        u_tmp->len,
        u_tmp->rx_buf ? "rx " : "",
        u_tmp->tx_buf ? "tx " : "",
        u_tmp->cs_change ? "cs " : "",
        u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
        u_tmp->delay_usecs,
        u_tmp->speed_hz ? : spidev->spi->max_speed_hz);*/

#if 0
        sendbuff_tmp = (char*)u_tmp->tx_buf;

        //set gpio mode
        mt_set_gpio_mode(GPIO80,GPIO_MODE_00);
        mt_set_gpio_mode(GPIO81,GPIO_MODE_00);
        mt_set_gpio_mode(GPIO83,GPIO_MODE_00);

        //set gpio dir
        mt_set_gpio_dir(GPIO80,GPIO_DIR_OUT);
        mt_set_gpio_dir(GPIO81,GPIO_DIR_OUT);
        mt_set_gpio_dir(GPIO83,GPIO_DIR_OUT);
        mdelay(2);
        /*spi_gpio_send_data('A');
        udelay(2);
        spi_gpio_send_data('B');
        udelay(2);
        spi_gpio_send_data('C');
        udelay(2);
        spi_gpio_send_data('D');*/
        for (j=0; j < u_tmp->len; j++){
        spi_gpio_send_data(*sendbuff_tmp);
        //printk("sendbuff_tmp [%d]%c",j,*sendbuff_tmp);
        sendbuff_tmp ++;
        }
        printk("\r\nsendbuff_tmp end");
#endif
#ifdef VERBOSE
        dev_dbg(&spidev->spi->dev,
        "  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
        u_tmp->len,
        u_tmp->rx_buf ? "rx " : "",
        u_tmp->tx_buf ? "tx " : "",
        u_tmp->cs_change ? "cs " : "",
        u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
        u_tmp->delay_usecs,
        u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }
    //status = total;//add by zhlu for test

#if 1 //add by zhlu for test
    status = spidev_sync(spidev, &msg);
    if (status < 0)
        goto done;
#endif

    /* copy any rx data out of bounce buffer */
    buf = spidev->buffer;
    for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) 
    {
        if (u_tmp->rx_buf)
        {
            if (__copy_to_user((u8 __user *)
            (uintptr_t) u_tmp->rx_buf, buf,
            u_tmp->len)) 
            {
                status = -EFAULT;
                goto done;
            }
        }
        buf += u_tmp->len;
    }
    status = total;

    done:
    kfree(k_xfers);
    return status;
}
 
static long
spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int         err = 0,i;
    int         retval = 0;
    struct spidev_data  *spidev;
    struct spi_device   *spi;
    u32         tmp;
    unsigned        n_ioc;
    struct spi_ioc_transfer *ioc;
    char * ico_tmp;

    printk("[wming] --- spidev_ioctl Enter!!!\n");
    /* Check type and command number */
    if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
    return -ENOTTY;

    /* Check access direction once here; don't repeat below.
    * IOC_DIR is from the user perspective, while access_ok is
    * from the kernel perspective; so they look reversed.
    */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE,
        (void __user *)arg, _IOC_SIZE(cmd));
    if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ,
        (void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    /* guard against device removal before, or while,
    * we issue this ioctl.
    */
    spidev = filp->private_data;
    spin_lock_irq(&spidev->spi_lock);
    spi = spi_dev_get(spidev->spi);
    spin_unlock_irq(&spidev->spi_lock);

    if (spi == NULL)
       return -ESHUTDOWN;

    /* use the buffer lock here for triple duty:
    *  - prevent I/O (from us) so calling spi_setup() is safe;
    *  - prevent concurrent SPI_IOC_WR_* from morphing
    *    data fields while SPI_IOC_RD_* reads them;
    *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
    */
    mutex_lock(&spidev->buf_lock);

    switch (cmd) 
    {
        /* read requests */
        case SPI_IOC_RD_MODE:
            printk("[wming] --- SPI_IOC_RD_MODE = %d\n", cmd);
            retval = __put_user(spi->mode & SPI_MODE_MASK,
                (__u8 __user *)arg);
            break;
            
        case SPI_IOC_RD_LSB_FIRST:
            printk("[wming] --- SPI_IOC_RD_LSB_FIRST = %d\n", cmd);
            retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
                (__u8 __user *)arg);
            break;
            
        case SPI_IOC_RD_BITS_PER_WORD:
            printk("[wming] --- SPI_IOC_RD_BITS_PER_WORD = %d\n", cmd);
            retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
            break;
            
        case SPI_IOC_RD_MAX_SPEED_HZ:
            printk("[wming] --- SPI_IOC_RD_MAX_SPEED_HZ = %d\n", cmd);
            retval = __put_user(spi->max_speed_hz, (__u32 __user *)arg);
            break;

        /* write requests */
        case SPI_IOC_WR_MODE:
            printk("[wming] --- SPI_IOC_WR_MODE = %d\n", cmd);
            retval = __get_user(tmp, (u8 __user *)arg);
            if (retval == 0) 
            {
                u8  save = spi->mode;
                if (tmp & ~SPI_MODE_MASK) 
                {
                    retval = -EINVAL;
                    break;
                }

                tmp |= spi->mode & ~SPI_MODE_MASK;
                spi->mode = (u8)tmp;
                retval = spi_setup(spi);
                if (retval < 0)
                    spi->mode = save;
                else
                    dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
            }
            break;
            
        case SPI_IOC_WR_LSB_FIRST:
            printk("[wming] --- SPI_IOC_WR_LSB_FIRST = %d\n", cmd);
            retval = __get_user(tmp, (__u8 __user *)arg);
            if (retval == 0) 
            {
                u8  save = spi->mode;
                if (tmp)
                    spi->mode |= SPI_LSB_FIRST;
                else
                    spi->mode &= ~SPI_LSB_FIRST;
                retval = spi_setup(spi);
                if (retval < 0)
                    spi->mode = save;
                else
                    dev_dbg(&spi->dev, "%csb first\n",
                     tmp ? 'l' : 'm');
            }
            break;
            
        case SPI_IOC_WR_BITS_PER_WORD:
            printk("[wming] --- SPI_IOC_WR_BITS_PER_WORD = %d\n", cmd);
            retval = __get_user(tmp, (__u8 __user *)arg);
            if (retval == 0) 
            {
                u8  save = spi->bits_per_word;

                spi->bits_per_word = tmp;
                retval = spi_setup(spi);
                if (retval < 0)
                    spi->bits_per_word = save;
                else
                    dev_dbg(&spi->dev, "%d bits per word\n", tmp);
            }
            break;
            
        case SPI_IOC_WR_MAX_SPEED_HZ:
            printk("[wming] --- SPI_IOC_WR_MAX_SPEED_HZ = %d\n", cmd);
            retval = __get_user(tmp, (__u32 __user *)arg);
            if (retval == 0) {
            u32 save = spi->max_speed_hz;

            spi->max_speed_hz = tmp;
            retval = spi_setup(spi);
            if (retval < 0)
            spi->max_speed_hz = save;
            else
            dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
            }
            break;

        default:
            printk("[wming] --- default = %d\n", cmd);
            /* segmented and/or full-duplex I/O request */
            if (_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
                || _IOC_DIR(cmd) != _IOC_WRITE) 
            {
                retval = -ENOTTY;
                break;
            }

            tmp = _IOC_SIZE(cmd);
            if ((tmp % sizeof(struct spi_ioc_transfer)) != 0) 
            {
                retval = -EINVAL;
                break;
            }
            n_ioc = tmp / sizeof(struct spi_ioc_transfer);
            if (n_ioc == 0)
            break;

            /* copy into scratch area */
            ioc = kmalloc(tmp, GFP_KERNEL);
            if (!ioc) 
            {
                retval = -ENOMEM;
                break;
            }
            if (__copy_from_user(ioc, (void __user *)arg, tmp)) 
            {
                kfree(ioc);
                retval = -EFAULT;
                break;
            }
            printk("ico sizeof =%d\r\n",sizeof(struct spi_ioc_transfer));

            /*ico_tmp= ioc;
            for (i = 0;i<sizeof(struct spi_ioc_transfer);i++)
            {
                printk("[%d]%x\n",i,ico_tmp[i]);
            }*/
            /* translate to spi_message, execute */
            printk("[wming] --- tmp = %d n_ioc %d = \n", tmp,n_ioc);
            retval = spidev_message(spidev, ioc, n_ioc);
            printk("\r\nretval =%d\r\n",retval);
            kfree(ioc);
            break;
    }

    mutex_unlock(&spidev->buf_lock);
    spi_dev_put(spi);
    return retval;
}
 
#ifdef CONFIG_COMPAT
static long
spidev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return spidev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spidev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */
 
static int spidev_open(struct inode *inode, struct file *filp)
{
    struct spidev_data  *spidev;
    struct mt_chip_conf *p_conf; //add by zhlu for
    int         status = -ENXIO;
    int ret;

    config_spi_gpio(1);

    mutex_lock(&device_list_lock);

    list_for_each_entry(spidev, &device_list, device_entry) 
    {
        if (spidev->devt == inode->i_rdev) 
        {
            status = 0;
            printk("[wming] --- spidev_open status:%d\n", status);
            break;
        }
    }
    if (status == 0) 
    {
        if (!spidev->buffer) 
        {
            spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
            printk("[wming] --- spidev_open kmalloc the spidev->buffer\n");

#if 1 //add by zhlu for
            spidev->spi->mode = SPI_MODE_0;
            spidev->spi->bits_per_word = 8;
            p_conf = &ice40_spi_conf;
            printk("com_mod %d,setuptime %d,holdtime %d,high_time %d,low_time %d,tx_mlsb %d,tx_endian %d,cs_idletime %d\n",
            p_conf->com_mod,
            p_conf->setuptime,
            p_conf->holdtime,
            p_conf->high_time,
            p_conf->low_time,
            p_conf->tx_mlsb,
            p_conf->tx_endian,
            p_conf->cs_idletime);
            ret = spi_setup(spidev->spi);
            if (ret < 0)
            {
                printk("[wming] ~~ setup spi fail\n");
            }
#endif
            if (!spidev->buffer) 
            {
                dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
                status = -ENOMEM;
            }
        }
        if (status == 0) 
        {
            spidev->users++;
            filp->private_data = spidev;
            nonseekable_open(inode, filp);
        }
    } 
    else
        pr_debug("spidev: nothing for minor %d\n", iminor(inode));

    mutex_unlock(&device_list_lock);

    printk("[wming] --- spidev_open return:%d\n", status);
    return status;
}
 
static int spidev_release(struct inode *inode, struct file *filp)
{
    struct spidev_data  *spidev;
    int         status = 0;

    mutex_lock(&device_list_lock);
    spidev = filp->private_data;
    filp->private_data = NULL;

    /* last close? */
    spidev->users--;
    if (!spidev->users) 
    {
        int     dofree;

        kfree(spidev->buffer);
        spidev->buffer = NULL;

        /* ... after we unbound from the underlying device? */
        spin_lock_irq(&spidev->spi_lock);
        dofree = (spidev->spi == NULL);
        spin_unlock_irq(&spidev->spi_lock);

        if (dofree)
        kfree(spidev);
    }
    mutex_unlock(&device_list_lock);

    printk("[wming] --- spidev_release return:%d\n", status);

    return status;
}
 
static const struct file_operations spidev_fops = 
{
    .owner =    THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
    * gets more complete API coverage.  It'll simplify things
    * too, except for the locking.
    */
    .write =    spidev_write,
    .read =     spidev_read,
    .unlocked_ioctl = spidev_ioctl,
    .compat_ioctl = spidev_compat_ioctl,
    .open =     spidev_open,
    .release =  spidev_release,
    .llseek =   no_llseek,
};
 
/*-------------------------------------------------------------------------*/
 
/* The main reason to have this class is to make mdev/udev create the
* /dev/spidevB.C character device nodes exposing our userspace API.
* It also simplifies memory management.
*/
 
static struct class *spidev_class;
 
/*-------------------------------------------------------------------------*/
 
static int __devinit spidev_probe(struct spi_device *spi)
{
    struct spidev_data  *spidev;
    int         status;
    unsigned long       minor;

#ifdef ICE40_FIRMWARE_DL
    int ret;
    struct mt_chip_conf *p_conf;
#endif


    /* Allocate driver data */
    spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
    if (!spidev)
    return -ENOMEM;

    /* Initialize the driver data */
    spidev->spi = spi;
    spin_lock_init(&spidev->spi_lock);
    mutex_init(&spidev->buf_lock);

    INIT_LIST_HEAD(&spidev->device_entry);

    /* If we can allocate a minor number, hook up this device.
    * Reusing minors is fine so long as udev or mdev is working.
    */
    mutex_lock(&device_list_lock);

    //add by wming for change spi clk
    //spidev->spi->controller_data = (void*)&spi_conf;
    //p_conf = &spi_conf;
    //p_conf->setuptime = 15;
    //p_conf->holdtime = 6000;
    //p_conf->high_time = 10;       //10--6m   15--4m   20--3m  30--2m  [ 60--1m 120--0.5m  300--0.2m]
    //p_conf->low_time = 10;
    //p_conf->cs_idletime = 0;
    //ret = spi_setup(spidev->spi);
    //if (ret < 0)
    //{
    //  printk("setup spi fail\n");
    //  return -1;
    //}

    minor = find_first_zero_bit(minors, N_SPI_MINORS);
    if (minor < N_SPI_MINORS) 
    {
        struct device *dev;

        spidev->devt = MKDEV(SPIDEV_MAJOR, minor);
        dev = device_create(spidev_class, &spi->dev, spidev->devt,
        spidev, "spidev%d.%d",
        spi->master->bus_num, spi->chip_select);
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    } 
    else 
    {
        dev_dbg(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
    }
    if (status == 0) 
    {
        set_bit(minor, minors);
        list_add(&spidev->device_entry, &device_list);
    }
    mutex_unlock(&device_list_lock);

#ifdef ICE40_FIRMWARE_DL
    atomic_set(&spidev->firmware_state, 0);
    INIT_DELAYED_WORK(&spidev->ice40_delayed_work, ice40_spi_download_func);

    spidev->spi->mode = SPI_MODE_3;
    spidev->spi->bits_per_word = 8;
    spidev->spi->controller_data = (void*)&ice40_spi_conf;

    p_conf = &ice40_spi_conf;
    /*p_conf->com_mod = DMA_TRANSFER;
    p_conf->setuptime = 180;//60;
    p_conf->holdtime = 6000;
    p_conf->high_time = 60;//10;       //10--6m   15--4m   20--3m  30--2m  [ 60--1m 120--0.5m  300--0.2m]
    p_conf->low_time = 60;//10;
    p_conf->tx_mlsb = 1;
    p_conf->tx_endian = 0;
    p_conf->cs_idletime = 0;*/

    p_conf->setuptime = 100;//15;
    p_conf->holdtime = 100;//15;
    p_conf->high_time = 50;//10;  //   与low_time  共同\u0178龆\u0161着SPI CLK 的周期
    p_conf->low_time = 50;//10;
    p_conf->cs_idletime = 100;//20;
    p_conf->rx_mlsb = 1;       
    p_conf->tx_mlsb = 1;     //mlsb=1  表蔦u0178\u017e遙it位先\u017d\u0161常不需要\u017e?    p_conf->tx_endian = 0;     //tx_endian  =1 表蔦u0178\u017d蠖四Ｊ\u0153，对于DMA模蔦u0153，需要\u017e鵟u0178萆璞\u017e的Spec 繺u017d选择，蚛u0161常为\u017d蠖恕?    p_conf->rx_endian = 0;
    p_conf->cpol = 0;      //  这里不需要再设置 ，  ice40_spi_client->mode = SPI_MODE_0 这里设置\u0152\u017d可
    p_conf->cpha = 0;     //   这里不需要再设置 ，  ice40_spi_client->mode = SPI_MODE_0 这里设置\u0152\u017d可
    p_conf->com_mod = DMA_TRANSFER;     // DMA  or FIFO
    //spi_par->com_mod = FIFO_TRANSFER;
    p_conf->pause = 0;     //与 deassert  的意薥u0152相穃u017d。\u0152\u017d是否支持暂停模蔦u0153，如\u017d俗鯯PI_CS 在多\u017d蝨ransfer之\u0152?不会被de-active
    p_conf->finish_intr = 1;  
    //spi_par->deassert = 0;
    p_conf->ulthigh = 0;
    p_conf->tckdly = 0;

    printk("[wming] --- spidev_probe ---spi_setup!!!\n");

    ret = spi_setup(spidev->spi);
    if (ret < 0)
    {
        printk("[wming] ~~ setup spi fail\n");
    }

    ice40_g_spidev = spidev;
    queue_delayed_work(ice40_wq, &spidev->ice40_delayed_work, 2);
#endif

    //add by wming for debug
    printk("[wming] --- spidev_probe Enter!!!\n");
    //config_c_reset_gpio(1);

    if (status == 0)
        spi_set_drvdata(spi, spidev);
    else
        kfree(spidev);

    return status;
}
 
static int __devexit spidev_remove(struct spi_device *spi)
{
    struct spidev_data  *spidev = spi_get_drvdata(spi);

    printk("[wming] --- spidev_remove !!!\n");
    /* make sure ops on existing fds can abort cleanly */
    spin_lock_irq(&spidev->spi_lock);
    spidev->spi = NULL;
    spi_set_drvdata(spi, NULL);
    spin_unlock_irq(&spidev->spi_lock);

    config_spi_gpio(0);
    config_c_reset_gpio(0);

    /* prevent new opens */
    mutex_lock(&device_list_lock);
    list_del(&spidev->device_entry);
    device_destroy(spidev_class, spidev->devt);
    clear_bit(MINOR(spidev->devt), minors);
    if (spidev->users == 0)
        kfree(spidev);
    
    mutex_unlock(&device_list_lock);

    return 0;
}


static int spidev_suspend(struct spi_device *spi, pm_message_t message)
{
    //gpio41-42  pull high
		mt_set_gpio_mode(GPIO_IR_C_DONE, GPIO_MODE_GPIO);
		mt_set_gpio_dir(GPIO_IR_C_DONE, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_IR_C_DONE, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_IR_C_DONE, GPIO_PULL_UP);
		
		mt_set_gpio_mode(GPIO_IR_C_RESET, GPIO_MODE_GPIO);
		mt_set_gpio_dir(GPIO_IR_C_RESET, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_IR_C_RESET, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_IR_C_RESET, GPIO_PULL_UP);

	
    //gpio30 NC
		mt_set_gpio_mode(GPIO_IR_LDO1V2, GPIO_MODE_GPIO);
		mt_set_gpio_dir(GPIO_IR_LDO1V2, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_IR_LDO1V2, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_IR_LDO1V2, GPIO_PULL_DOWN);


	//gpio72 NC
	    mt_set_gpio_mode(GPIO_IR_CLKM2_26M, GPIO_MODE_GPIO);
        mt_set_gpio_dir(GPIO_IR_CLKM2_26M, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(GPIO_IR_CLKM2_26M, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(GPIO_IR_CLKM2_26M, GPIO_PULL_DOWN);
	

	//gpio80-83   NC
        mt_set_gpio_mode(SPI_CS, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_CS, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(SPI_CS, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(SPI_CS, GPIO_PULL_DOWN);
        //SPI_SCK
        mt_set_gpio_mode(SPI_SCK, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_SCK, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(SPI_SCK, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(SPI_SCK, GPIO_PULL_DOWN);
        //SPI_MO
        mt_set_gpio_mode(SPI_MO, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_MO, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(SPI_MO, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(SPI_MO, GPIO_PULL_DOWN);
        //SPI_MI
        mt_set_gpio_mode(SPI_MI, GPIO_MODE_GPIO);
        mt_set_gpio_dir(SPI_MI, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(SPI_MI, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(SPI_MI, GPIO_PULL_DOWN);	

	return 0;
}

static int spidev_resume(struct spi_device *spi)
{
	return 0;
}

 
static struct spi_driver spidev_spi_driver = 
{
    .driver = {
        .name =     "spidev",
        .owner =    THIS_MODULE,
        },
    .probe =    spidev_probe,
    .remove =   __devexit_p(spidev_remove),
	// add for low power issue
	.suspend =  spidev_suspend,
	.resume  =  spidev_resume,

    /* NOTE:  suspend/resume methods are not necessary here.
    * We don't do anything except pass the requests to/from
    * the underlying controller.  The refrigerator handles
    * most issues; the controller driver handles the rest.
    */
};
 
static struct spi_board_info spi_board_devs[] __initdata = 
{
    [0] = {
        .modalias="spidev",
        .bus_num = 0,
        .chip_select=0,
        .mode = SPI_MODE_3,
        .max_speed_hz = 1600000,
    },
};
 
/*-------------------------------------------------------------------------*/
 
static int __init spidev_init(void)
{
    int status;

    mt_set_gpio_mode(C_DONE, GPIO_MODE_GPIO);
    mt_set_gpio_dir(C_DONE, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(C_DONE, GPIO_PULL_DISABLE);

#if defined(HERO2_SPIDEV)
    mt_set_gpio_mode(SPI_GPIO30_IRDALDO1V2_EN, GPIO_MODE_GPIO);
    mt_set_gpio_dir(SPI_GPIO30_IRDALDO1V2_EN, GPIO_DIR_OUT);
    mt_set_gpio_pull_enable(SPI_GPIO30_IRDALDO1V2_EN, GPIO_PULL_ENABLE);
    mt_set_gpio_out(SPI_GPIO30_IRDALDO1V2_EN, 1);
#else
    vibr_Enable_HW();
    hwPowerOn(MT6322_POWER_LDO_VIBR,VOL_1200,"irda_power_1v2");
#endif

    CLK_Monitor(2,1,0);

    spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));

    /* Claim our 256 reserved device numbers.  Then register a class
    * that will key udev/mdev to add/remove /dev nodes.  Last, register
    * the driver which manages those device numbers.
    */
    BUILD_BUG_ON(N_SPI_MINORS > 256);
    status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
    if (status < 0)
        return status;

    spidev_class = class_create(THIS_MODULE, "spidev");
    if (IS_ERR(spidev_class)) 
    {
        unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
        return PTR_ERR(spidev_class);
    }

    status = spi_register_driver(&spidev_spi_driver);
    if (status < 0) 
    {
        class_destroy(spidev_class);
        unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
    }

#ifdef ICE40_FIRMWARE_DL
    ice40_wq = create_singlethread_workqueue("ice40-wq");
#endif

    return status;
}


module_init(spidev_init);
 
static void __exit spidev_exit(void)
{
    spi_unregister_driver(&spidev_spi_driver);
    class_destroy(spidev_class);
    unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);

#ifdef ICE40_FIRMWARE_DL
    destroy_workqueue(ice40_wq);
#endif
}

module_exit(spidev_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev");



