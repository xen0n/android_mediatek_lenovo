/******************************************************************************
 * mtk_tpd.c - MTK Android Linux Touch Panel Device Driver               *
 *                                                                            *
 * Copyright 2008-2009 MediaTek Co.,Ltd.                                      *
 *                                                                            *
 * DESCRIPTION:                                                               *
 *     this file provide basic touch panel event to input sub system          *
 *                                                                            *
 * AUTHOR:                                                                    *
 *     Kirby.Wu (mtk02247)                                                    *
 *                                                                            *
 * NOTE:                                                                      *
 * 1. Sensitivity for touch screen should be set to edge-sensitive.           *
 *    But in this driver it is assumed to be done by interrupt core,          *
 *    though not done yet. Interrupt core may provide interface to            *
 *    let drivers set the sensitivity in the future. In this case,            *
 *    this driver should set the sensitivity of the corresponding IRQ         *
 *    line itself.                                                            *
 ******************************************************************************/

#include "tpd.h"

//#ifdef VELOCITY_CUSTOM
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

/*lenovo-sw chenglong1 add for leSnapshot*/
#include <linux/notifier.h>
/*lenovo-sw add end*/

// for magnify velocity********************************************
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)

extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;
extern UINT32 DISP_GetScreenHeight(void);
extern UINT32 DISP_GetScreenWidth(void);

static int tpd_misc_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int tpd_misc_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
                               unsigned long arg)
{
    //char strbuf[256];
    void __user *data;

    long err = 0;

    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
        printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
        case TPD_GET_VELOCITY_CUSTOM_X:
            data = (void __user *) arg;

            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }

            if (copy_to_user(data, &tpd_v_magnify_x, sizeof(tpd_v_magnify_x)))
            {
                err = -EFAULT;
                break;
            }

            break;

        case TPD_GET_VELOCITY_CUSTOM_Y:
            data = (void __user *) arg;

            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }

            if (copy_to_user(data, &tpd_v_magnify_y, sizeof(tpd_v_magnify_y)))
            {
                err = -EFAULT;
                break;
            }

            break;

        default:
            printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
            err = -ENOIOCTLCMD;
            break;

    }

    return err;
}


static struct file_operations tpd_fops =
{
//	.owner = THIS_MODULE,
    .open = tpd_misc_open,
    .release = tpd_misc_release,
    .unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "touch",
    .fops = &tpd_fops,
};

//**********************************************
//#endif


/* function definitions */
static int __init  tpd_device_init(void);
static void __exit tpd_device_exit(void);
static int         tpd_probe(struct platform_device *pdev);
static int tpd_remove(struct platform_device *pdev);

extern void        tpd_suspend(struct early_suspend *h);
extern void        tpd_resume(struct early_suspend *h);
extern void tpd_button_init(void);

//int tpd_load_status = 0; //0: failed, 1: sucess
int tpd_register_flag=0;
/* global variable definitions */
struct tpd_device  *tpd = 0;
static struct tpd_driver_t tpd_driver_list[TP_DRV_MAX_COUNT] ;//= {0};

static struct platform_driver tpd_driver = {
    .remove     = tpd_remove,
    .shutdown   = NULL,
    .probe      = tpd_probe,
    #ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend    = NULL,
    .resume     = NULL,
    #endif
    .driver     = {
        .name = TPD_DEVICE,
    },
};

/*20091105, Kelvin, re-locate touch screen driver to earlysuspend*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend MTK_TS_early_suspend_handler = 
{
    .level = EARLY_SUSPEND_LEVEL_STOP_DRAWING-1,    
    .suspend = NULL,
    .resume  = NULL,
};
#endif

static struct tpd_driver_t *g_tpd_drv = NULL;

/*Begin lenovo-sw wengjun1 add for tp info struct. 2014-1-15*/
struct tpd_version_info *tpd_info_t = NULL;
unsigned int have_correct_setting = 0;
/*End lenovo-sw wengjun1 add for tp info struct. 2014-1-15*/


/* Add driver: if find TPD_TYPE_CAPACITIVE driver sucessfully, loading it */
int tpd_driver_add(struct tpd_driver_t *tpd_drv)
{
	int i;
	
	if(g_tpd_drv != NULL)
	{
		TPD_DMESG("touch driver exist \n");
		return -1;
	}
	/* check parameter */
	if(tpd_drv == NULL)
	{
		return -1;
	}
	/* R-touch */
	if(strcmp(tpd_drv->tpd_device_name, "generic") == 0)
	{
			tpd_driver_list[0].tpd_device_name = tpd_drv->tpd_device_name;
			tpd_driver_list[0].tpd_local_init = tpd_drv->tpd_local_init;
			tpd_driver_list[0].suspend = tpd_drv->suspend;
			tpd_driver_list[0].resume = tpd_drv->resume;
			tpd_driver_list[0].tpd_have_button = tpd_drv->tpd_have_button;
			return 0;
	}
	for(i = 1; i < TP_DRV_MAX_COUNT; i++)
	{
		/* add tpd driver into list */
		if(tpd_driver_list[i].tpd_device_name == NULL)
		{
			tpd_driver_list[i].tpd_device_name = tpd_drv->tpd_device_name;
			tpd_driver_list[i].tpd_local_init = tpd_drv->tpd_local_init;
			tpd_driver_list[i].suspend = tpd_drv->suspend;
			tpd_driver_list[i].resume = tpd_drv->resume;
			tpd_driver_list[i].tpd_have_button = tpd_drv->tpd_have_button;
			tpd_driver_list[i].attrs = tpd_drv->attrs;
			#if 0
			if(tpd_drv->tpd_local_init()==0)
			{
				TPD_DMESG("load %s sucessfully\n", tpd_driver_list[i].tpd_device_name);
				g_tpd_drv = &tpd_driver_list[i];
			}
			#endif
			break;
		}
		if(strcmp(tpd_driver_list[i].tpd_device_name, tpd_drv->tpd_device_name) == 0)
		{
			return 1; // driver exist
		}
	}
	
	return 0;
}

int tpd_driver_remove(struct tpd_driver_t *tpd_drv)
{
	int i = 0;
	/* check parameter */
	if(tpd_drv == NULL)
	{
		return -1;
	}
	for(i = 0; i < TP_DRV_MAX_COUNT; i++)
	{
		/* find it */
		if (strcmp(tpd_driver_list[i].tpd_device_name, tpd_drv->tpd_device_name) == 0)
		{
			memset(&tpd_driver_list[i], 0, sizeof(struct tpd_driver_t));
			break;
		}
	}
	return 0;
}

static void tpd_create_attributes(struct device *dev, struct tpd_attrs *attrs)
{
	int num = attrs->num;

	for (; num>0;) {
		device_create_file(dev, attrs->attr[--num]);
		printk("mtk_tpd attr name: %s", attrs->attr[num]->attr.name);
	}
}

/*lenovo-sw chenglong1 add for touch boost*/
#define TPD_NOTIFY_CHAIN
extern int cpu_up(unsigned int cpu);
extern void hp_based_cpu_num(int num);
//extern void hp_disable_cpu_hp(int disable);
static int tpd_hold_cores=4;
static int tpd_dyn_boost_cores = 4;
static int tpd_hold_cores_ms = 2000;

#ifdef TPD_NOTIFY_CHAIN
/*============Touch Event RAW Notifier Implementation============*/
static RAW_NOTIFIER_HEAD(tpd_evt_notifier_list);

 /**
 *	tpd_evt_notify_register - Add notifier to the touch event raw notifier chain
 *	@nb: New entry in notifier chain
 *
 *	Adds a notifier to the touch event raw notifier chain.
 *	All locking must be provided by the caller.
 *
 *	Currently always returns zero.
 */
int tpd_evt_notify_register(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&tpd_evt_notifier_list, nb);
}
EXPORT_SYMBOL(tpd_evt_notify_register);

/**
 *	tpd_evt_notify_unregister - Remove notifier from the touch event raw notifier chain
 *	@nb: Entry to remove from notifier chain
 *
 *	Removes a notifier from the touch event raw notifier chain.
 *	All locking must be provided by the caller.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int tpd_evt_notify_unregister(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&tpd_evt_notifier_list, nb);
}
EXPORT_SYMBOL(tpd_evt_notify_unregister);

int tpd_evt_do_notify(unsigned long evt, void *data)
{
	raw_notifier_call_chain(&tpd_evt_notifier_list, evt, data);
}
EXPORT_SYMBOL(tpd_evt_do_notify);
#endif

/*============Dynamic Booster Param Setting============*/
static ssize_t t_dyn_boost_cores_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "cpu dynamic boost to %d cores!!!\n", tpd_dyn_boost_cores);
}

static ssize_t t_dyn_boost_cores_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int cores = 1;
	
	sscanf(buf, "%d", &cores);
	tpd_dyn_boost_cores = cores;
	
	return count;
}
static DEVICE_ATTR(t_dyn_boost_cores, 0664, t_dyn_boost_cores_show, t_dyn_boost_cores_store);

static ssize_t t_hold_cores_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "cpu hold in %d cores!!!\n", tpd_hold_cores);
}

static ssize_t t_hold_cores_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int cores = 1;
	int disable_hp = 0;
	
	sscanf(buf, "%d %d", &cores, &disable_hp);
	tpd_hold_cores = cores;
	//hp_disable_cpu_hp(disable_hp ? 1 : 0);
	hp_based_cpu_num(cores);
	
	return count;
}
static DEVICE_ATTR(t_hold_cores, 0664, t_hold_cores_show, t_hold_cores_store);

static ssize_t t_hold_cores_ms_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "cpu hold time is %d ms!!!\n", tpd_hold_cores_ms);
}

static ssize_t t_hold_cores_ms_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int time = 2000;
	
	sscanf(buf, "%d", &time);
	tpd_hold_cores_ms = time;
	
	return count;
}
static DEVICE_ATTR(t_hold_cores_ms, 0664, t_hold_cores_ms_show, t_hold_cores_ms_store);

static struct device_attribute *g_tpd_boost_attrs[] =
{
	&dev_attr_t_dyn_boost_cores,
	&dev_attr_t_hold_cores,
	&dev_attr_t_hold_cores_ms
};

static struct tpd_attrs g_tpd_booster_attrs = {
	.attr = g_tpd_boost_attrs,
	.num = sizeof(g_tpd_boost_attrs) /sizeof(g_tpd_boost_attrs[0])
};
/*============Dynamic Booster Implementation============*/
#define MIN_CPU_BOOST_INTERVAL (300*1000000)
#define CPU_MAX_CORES (8)
static struct work_struct boost_cpu_work;
static struct delayed_work cpu_holder_handler_work;
static struct workqueue_struct *mtk_tpd_booster_wq;
static unsigned long long last_boost_time = 0;
static int g_is_dynamic_boosted = 0;
static void boost_cpu_work_func(struct work_struct *work)
{
	int j;
	unsigned long long cur = sched_clock();

	if (cur - last_boost_time < MIN_CPU_BOOST_INTERVAL) {
		printk("abandon boost request, too frequent...\n");
		return ;
	}
	
	if (tpd_dyn_boost_cores > CPU_MAX_CORES) tpd_dyn_boost_cores = CPU_MAX_CORES;
	for (j=1; j<tpd_dyn_boost_cores; j++) {
		cpu_up(j);
	}
	last_boost_time = cur;
	if (tpd_hold_cores_ms) { //set tpd_hold_cores_ms to disable 4 cores holding
		hp_based_cpu_num(tpd_dyn_boost_cores);
		schedule_delayed_work(&cpu_holder_handler_work, msecs_to_jiffies(tpd_hold_cores_ms));
	}
}

static void cpu_holder_timeout_handler(struct work_struct *work)
{
	hp_based_cpu_num(1);
}

int tpd_touch_down_hook()
{
	if (!g_is_dynamic_boosted) {
		queue_work(mtk_tpd_booster_wq, &boost_cpu_work);
		g_is_dynamic_boosted = 1;
	}
}
EXPORT_SYMBOL(tpd_touch_down_hook);

int tpd_touch_up_hook()
{
	g_is_dynamic_boosted = 0;
	return 0;
}
EXPORT_SYMBOL(tpd_touch_up_hook);

int tpd_touch_evt_hook(unsigned int code, int value)
{
	return 0;
}
EXPORT_SYMBOL(tpd_touch_evt_hook);

/*============Notify Chain Test Implementation============*/
#ifdef TPD_NOTIFY_TEST
static int tpd_test_chain_nb(struct notifier_block *this, unsigned long event, void *ptr)
{
	return 0;
}

static struct notifier_block panic_test = {
	.notifier_call  = tpd_test_chain_nb,
};

static ssize_t tpd_notify_test_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "cpu hold time is %d ms!!!", tpd_hold_cores_ms);
}

static ssize_t tpd_notify_test_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int en = 0;
	
	sscanf(buf, "%d", &en);
	if (en) {
		tpd_evt_notify_register(&panic_test);
	} else {
		tpd_evt_notify_unregister(&panic_test);
	}
	
	return count;
}
static DEVICE_ATTR(tpd_notify_test, 0664, tpd_notify_test_show, tpd_notify_test_store);

static struct device_attribute *g_tpd_notify_test_attrs[] =
{
	&dev_attr_tpd_notify_test
};

static struct tpd_attrs g_tpd_notify_test_attr = {
	.attr = g_tpd_notify_test_attrs,
	.num = sizeof(g_tpd_notify_test_attrs) /sizeof(g_tpd_notify_test_attrs[0])
};
#endif
/*lenovo-sw add end*/

/* touch panel probe */
static int tpd_probe(struct platform_device *pdev) {
		int  touch_type = 1; // 0:R-touch, 1: Cap-touch
		int i=0;
	  TPD_DMESG("enter %s, %d\n", __FUNCTION__, __LINE__); 
	  /* Select R-Touch */
	 // if(g_tpd_drv == NULL||tpd_load_status == 0) 
#if 0
	 if(g_tpd_drv == NULL) 
	  {
	  	g_tpd_drv = &tpd_driver_list[0];
	  	/* touch_type:0: r-touch, 1: C-touch */
	  	touch_type = 0;
	  	TPD_DMESG("Generic touch panel driver\n");
	  }
	  	
    #ifdef CONFIG_HAS_EARLYSUSPEND
    MTK_TS_early_suspend_handler.suspend = g_tpd_drv->suspend;
    MTK_TS_early_suspend_handler.resume = g_tpd_drv->resume;
    register_early_suspend(&MTK_TS_early_suspend_handler);
    #endif
    #endif

/*Begin lenovo-sw wengjun1 add for tp info struct. 2014-1-15*/
    tpd_info_t = (struct tpd_version_info*)kmalloc(sizeof(struct tpd_version_info), GFP_KERNEL);
/*End lenovo-sw wengjun1 add for tp info struct. 2014-1-15*/
	
    if (misc_register(&tpd_misc_device))
    {
	printk("mtk_tpd: tpd_misc_device register failed\n");
    }
	
    if((tpd=(struct tpd_device*)kmalloc(sizeof(struct tpd_device), GFP_KERNEL))==NULL) return -ENOMEM;
    memset(tpd, 0, sizeof(struct tpd_device));

    /* allocate input device */
    if((tpd->dev=input_allocate_device())==NULL) { kfree(tpd); return -ENOMEM; }
  
  //TPD_RES_X = simple_strtoul(LCM_WIDTH, NULL, 0);
  //TPD_RES_Y = simple_strtoul(LCM_HEIGHT, NULL, 0);    
  TPD_RES_X = DISP_GetScreenWidth();
    TPD_RES_Y = DISP_GetScreenHeight();

  
    printk("mtk_tpd: TPD_RES_X = %d, TPD_RES_Y = %d\n", TPD_RES_X, TPD_RES_Y);
  
    tpd_mode = TPD_MODE_NORMAL;
    tpd_mode_axis = 0;
    tpd_mode_min = TPD_RES_Y/2;
    tpd_mode_max = TPD_RES_Y;
    tpd_mode_keypad_tolerance = TPD_RES_X*TPD_RES_X/1600;
    /* struct input_dev dev initialization and registration */
    tpd->dev->name = TPD_DEVICE;
    set_bit(EV_ABS, tpd->dev->evbit);
    set_bit(EV_KEY, tpd->dev->evbit);
    set_bit(ABS_X, tpd->dev->absbit);
    set_bit(ABS_Y, tpd->dev->absbit);
    set_bit(ABS_PRESSURE, tpd->dev->absbit);
    set_bit(BTN_TOUCH, tpd->dev->keybit);
    set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);
#if 1    
  for(i = 1; i < TP_DRV_MAX_COUNT; i++)
	{
    		/* add tpd driver into list */
		if(tpd_driver_list[i].tpd_device_name != NULL)
		{
			tpd_driver_list[i].tpd_local_init();
			//msleep(1);
			if(tpd_load_status ==1) {
				TPD_DMESG("[mtk-tpd]tpd_probe, tpd_driver_name=%s\n", tpd_driver_list[i].tpd_device_name);
				g_tpd_drv = &tpd_driver_list[i];
				break;
			}
		}    
  }
	 if(g_tpd_drv == NULL) {
	 	if(tpd_driver_list[0].tpd_device_name != NULL) {
	  	g_tpd_drv = &tpd_driver_list[0];
	  	/* touch_type:0: r-touch, 1: C-touch */
	  	touch_type = 0;
	  	g_tpd_drv->tpd_local_init();
	  	TPD_DMESG("[mtk-tpd]Generic touch panel driver\n");
	  } else {
	  	TPD_DMESG("[mtk-tpd]cap touch and Generic touch both are not loaded!!\n");
	  	return 0;
	  	} 
	  }	
    #ifdef CONFIG_HAS_EARLYSUSPEND
    MTK_TS_early_suspend_handler.suspend = g_tpd_drv->suspend;
    MTK_TS_early_suspend_handler.resume = g_tpd_drv->resume;
    register_early_suspend(&MTK_TS_early_suspend_handler);
    #endif		  
#endif	  
//#ifdef TPD_TYPE_CAPACITIVE
		/* TPD_TYPE_CAPACITIVE handle */
		if(touch_type == 1){
			
		set_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
    	set_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
    	set_bit(ABS_MT_TOUCH_MINOR, tpd->dev->absbit);
    	set_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
    	set_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
        #if 0 // linux kernel update from 2.6.35 --> 3.0
    	tpd->dev->absmax[ABS_MT_POSITION_X] = TPD_RES_X;
    	tpd->dev->absmin[ABS_MT_POSITION_X] = 0;
    	tpd->dev->absmax[ABS_MT_POSITION_Y] = TPD_RES_Y;
    	tpd->dev->absmin[ABS_MT_POSITION_Y] = 0;
    	tpd->dev->absmax[ABS_MT_TOUCH_MAJOR] = 100;
    	tpd->dev->absmin[ABS_MT_TOUCH_MINOR] = 0;
#else
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MINOR, 0, 100, 0, 0); 	
#endif
    	TPD_DMESG("Cap touch panel driver\n");
  	}
//#endif
    #if 0 //linux kernel update from 2.6.35 --> 3.0
    tpd->dev->absmax[ABS_X] = TPD_RES_X;
    tpd->dev->absmin[ABS_X] = 0;
    tpd->dev->absmax[ABS_Y] = TPD_RES_Y;
    tpd->dev->absmin[ABS_Y] = 0;
	
    tpd->dev->absmax[ABS_PRESSURE] = 255;
    tpd->dev->absmin[ABS_PRESSURE] = 0;
    #else
		input_set_abs_params(tpd->dev, ABS_X, 0, TPD_RES_X, 0, 0);
		input_set_abs_params(tpd->dev, ABS_Y, 0, TPD_RES_Y, 0, 0);
		input_abs_set_res(tpd->dev, ABS_X, TPD_RES_X);
		input_abs_set_res(tpd->dev, ABS_Y, TPD_RES_Y);
		input_set_abs_params(tpd->dev, ABS_PRESSURE, 0, 255, 0, 0);

    #endif
    if(input_register_device(tpd->dev))
        TPD_DMESG("input_register_device failed.(tpd)\n");
    else
			tpd_register_flag = 1;
    /* init R-Touch */
    #if 0
	  if(touch_type == 0) 
	  {
	  	g_tpd_drv->tpd_local_init();
	  }    
		#endif
    if(g_tpd_drv->tpd_have_button)
    {
    	tpd_button_init();
    }

	/*lenovo-sw chenglong1 add for touch boost*/
	printk("mtk_tpd dev_attributes num: %d\n",  g_tpd_booster_attrs.num);
	tpd_create_attributes(&pdev->dev, &g_tpd_booster_attrs);
	INIT_WORK(&boost_cpu_work, boost_cpu_work_func);
	INIT_DELAYED_WORK_DEFERRABLE(&cpu_holder_handler_work, cpu_holder_timeout_handler);
	mtk_tpd_booster_wq = create_singlethread_workqueue("tp_booster_wq");
	/*lenovo-sw add end*/

	if (g_tpd_drv->attrs.num)
		tpd_create_attributes(&pdev->dev, &g_tpd_drv->attrs);

    return 0;
}

static int tpd_remove(struct platform_device *pdev)
{
	   input_unregister_device(tpd->dev);
    #ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&MTK_TS_early_suspend_handler);
    #endif
    return 0;
}

/* called when loaded into kernel */
static int __init tpd_device_init(void) {
    printk("MediaTek touch panel driver init\n");
    if(platform_driver_register(&tpd_driver)!=0) {
        TPD_DMESG("unable to register touch panel driver.\n");
        return -1;
    }   
    return 0;
}

/* should never be called */
static void __exit tpd_device_exit(void) {
    TPD_DMESG("MediaTek touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    platform_driver_unregister(&tpd_driver);
    #ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&MTK_TS_early_suspend_handler);
    #endif
}

module_init(tpd_device_init);
module_exit(tpd_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_AUTHOR("Kirby Wu<kirby.wu@mediatek.com>");

