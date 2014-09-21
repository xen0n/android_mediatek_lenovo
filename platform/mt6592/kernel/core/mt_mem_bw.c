#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include "mach/mt_emi_bm.h"
#include "mach/mt_dcm.h"
#include <asm/div64.h>  

#define EMI_CLK_RATE 266
#if 0
static struct mtk_monitor *mtk_mon;
static struct mt_mon_log *mtk_mon_log;
static struct pmu_cfg  p_cfg;
static struct l2c_cfg  l_cfg;

#define	BM_BOTH_READ_WRITE 0
#define	BM_READ_ONLY 1
#define	BM_WRITE_ONLY 2

/*********************************************************/
/* Show L1 D-Cache and I-Cache Hit Rate                  */
/*       and                                             */
/* Show APMCU, MM, MD, and Peripheral memory throughput  */
/*********************************************************/
void display_hitrate_throughput(struct mt_mon_log *mon_log)
{
	unsigned int hit_rate;
	unsigned int throughput;
	unsigned int parameter  = (EMI_CLK_RATE / (1024 * 1024)) * 8;
	int i = 0;

	/*L1 D-Cache Hit Rate*/
	for(i=0; i<8; i++)
	{
	  if(mon_log->cpu_cnt3[i] == 0)	  
	    continue;
  	hit_rate = 100 - ((mon_log->cpu_cnt2[i] * 100) / mon_log->cpu_cnt3[i] );
  	printk("CPU[%d] L1 D-Cache Hit Rate = %u%\n", i, hit_rate);
  	
  	/*L1 I-Cache Hit Rate*/
  	if(mon_log->cpu_cnt1[i] == 0)	  
	    continue;
  	hit_rate = 100 - ((mon_log->cpu_cnt0[i] * 100) / mon_log->cpu_cnt1[i]);
  	printk("CPU[%d] L1 I-Cache Hit Rate = %u%\n", i, hit_rate);
  }

#ifdef CONFIG_SMP
	/*L1 D-Cache Hit Rate*/
	//hit_rate = 100 - ((mon_log->cpu_cnt2[1] * 100) / mon_log->cpu_cnt3[1]);
	//printk("L1 D-Cache Hit Rate = %u% [Core1]\n", hit_rate);
	
	/*L1 I-Cache Hit Rate*/
	//hit_rate = 100 - ((mon_log->cpu_cnt0[1] * 100) / mon_log->cpu_cnt1[1]);
	//printk("L1 I-Cache Hit Rate = %u% [Core1]\n", hit_rate);
#endif	
	

 
    unsigned int parameter  = (EMI_CLK_RATE / (1024 * 1024)) * 8;
  	/*APMCU MEMORY THROUGHPUT(MB/s)*/
  	throughput = ((mon_log->BM_WSCT2 * 8 * EMI_CLK_RATE) / mon_log->BM_BCNT) / (1024 * 1024);
//	throughput =  mon_log->BM_WSCT2 * parameter / mon_log->BM_BCNT;
        printk("APMCU MEMORY THROUGHPUT = %u(MB/s)\n", throughput);
  
  	/*MD SUBSYS MEMORY THROUGHPUT(MB/s)*/
 	throughput = ((mon_log->BM_WSCT3 * 8 * EMI_CLK_RATE) / mon_log->BM_BCNT) / (1024 * 1024);
  //	throughput =  mon_log->BM_WSCT3 * parameter / mon_log->BM_BCNT;
	printk("MD MEMORY THROUGHPUT = %u(MB/s)\n", throughput);
  
  	/*MM SUBSYS MEMORY THROUGHPUT(MB/s)*/
  	throughput = ((mon_log->BM_WSCT * 8 * EMI_CLK_RATE) / mon_log->BM_BCNT) / (1024 * 1024);
  //	throughput =  mon_log->BM_WSCT * parameter / mon_log->BM_BCNT;
	printk("MM MEMORY THROUGHPUT = %u(MB/s)\n", throughput);
    
	/*PERIPHERAL SUBSYS MEMORY THROUGHPUT(MB/s)*/
  	throughput = ((mon_log->BM_WSCT4 * 8 * EMI_CLK_RATE) / mon_log->BM_BCNT) / (1024 * 1024);
//	throughput =  mon_log->BM_WSCT4 * parameter / mon_log->BM_BCNT;
  	printk("PERIPHERAL MEMORY THROUGHPUT = %u(MB/s)\n", throughput);
 }
#endif
extern void disable_infra_dcm(void);
static int __init mon_kernel_init(void)
{
    /* init EMI bus monitor */
    BM_SetReadWriteType(BM_BOTH_READ_WRITE);
    BM_SetMonitorCounter(1, BM_MASTER_MM, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(2, BM_MASTER_AP_MCU, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(3, BM_MASTER_MD_MCU | BM_MASTER_2G_3G_MDDMA, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);
    BM_SetMonitorCounter(4, BM_MASTER_PERI, BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);

    BM_SetLatencyCounter();

    BM_Init();	

//dcm_disable(ALL_DCM);
disable_infra_dcm();
BM_SetEmiDcm(0xff); //disable EMI dcm
 // stopping EMI monitors will reset all counters
BM_Enable(0);
 // start EMI monitor counting
BM_Enable(1);

#if 0
    int ret;
    int index;
    ret = register_monitor(&mtk_mon, MODE_MANUAL_KERNEL);
    if(ret < 0) {
    
        printk("Register MTK monitor Error\n");
        return -1;
    }

    mtk_mon->init();
    
    p_cfg.event_cfg[0] = 0x1; //ARMV7_IFETCH_MISS
    p_cfg.event_cfg[1] = 0x14; //ARMV7_L1_ICACHE_ACCESS
    p_cfg.event_cfg[2] = 0x3; //ARMV7_DCACHE_REFILL
    p_cfg.event_cfg[3] = 0x4;// ARMV7_DCACHE_ACCESS

    mtk_mon->set_pmu(&p_cfg);

    //l_cfg.l2c_evt[0] = 4; //L2C_EVT_DWHIT
    //l_cfg.l2c_evt[1] = 5; //L2C_EVT_DWREQ

    //mtk_mon->set_l2c(&l_cfg);
	
    mtk_mon->set_bm_rw(BM_BOTH_READ_WRITE);
    
    mtk_mon->enable();
    /****************** Run the application *****************/ 
    printk("[Monitor test]Run the application!!!\n");
    printk("[Monitor test]Run the application!!!\n");
    printk("[Monitor test]Run the application!!!\n");
    printk("[Monitor test]Run the application!!!\n");
    printk("[Monitor test]Run the application!!!\n");

    mtk_mon->disable();

    index = mtk_mon->mon_log(NULL);
    mtk_mon_log = &(mtk_mon->log_buff[index]);

    display_hitrate_throughput(mtk_mon_log);
   
    mtk_mon->deinit();
    unregister_monitor(&mtk_mon);
#endif
    return 0;
}

static void __exit mon_kernel_exit(void)
{
    return;
}

extern unsigned int mt_get_emi_freq(void);
unsigned long long get_mem_bw(void)
{
    unsigned long long throughput;
    unsigned long long WordAllCount;
    unsigned int BusCycCount;
    unsigned int emi_clk_rate;

BM_Pause();
//printk("[EMI] WACT:%d, BCNT:%d, 1:%d,2:%d,3:%d,4:%d\n",BM_GetWordAllCount(),BM_GetBusCycCount(),BM_GetWordCount(1),BM_GetWordCount(2),BM_GetWordCount(3),BM_GetWordCount(4));
WordAllCount = BM_GetWordAllCount();
BusCycCount = BM_GetBusCycCount();
emi_clk_rate = mt_get_emi_freq();
throughput = (WordAllCount * 8 * emi_clk_rate);
do_div(throughput,BusCycCount);
throughput /= 1024;
//printk("Total MEMORY THROUGHPUT =%llu(MB/s),%d\n",throughput,emi_clk_rate);
disable_infra_dcm();
BM_SetEmiDcm(0xff); //disable EMI dcm
// stopping EMI monitors will reset all counters
BM_Enable(0);
// start EMI monitor counting
BM_Enable(1);

#if 0
    int index;
    mtk_mon->disable();
    index = mtk_mon->mon_log(NULL);
    mtk_mon_log = &(mtk_mon->log_buff[index]);
    printk("[EMI] WACT:%d, BCNT:%d\n");
    display_hitrate_throughput(mtk_mon_log);
    mtk_mon->enable();
#endif
    return throughput;
}
module_init(mon_kernel_init);
module_exit(mon_kernel_exit);

