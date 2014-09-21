#include <linux/init.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/localtimer.h>
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include <asm/hardware/gic.h>
#include <mach/mtk_boot_share_page.h>
#endif
#include <asm/fiq_glue.h>
#include <mach/mt_reg_base.h>
#include <mach/smp.h>
#include <mach/sync_write.h>
#include <mach/hotplug.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_idle.h>
#include <mach/wd_api.h>
#if defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
#include <mach/mt_secure_api.h>
#endif
#ifdef CONFIG_MTK_SCHED_TRACERS
#include <trace/events/mtk_events.h>
#include "kernel/trace/trace.h"
#endif

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#define SLAVE1_MAGIC_REG (0xF0002000 + 0x38)
#define SLAVE2_MAGIC_REG (0xF0002000 + 0x38)
#define SLAVE3_MAGIC_REG (0xF0002000 + 0x38)
#define SLAVE4_MAGIC_REG (0xF0002000 + 0x3C)
#define SLAVE5_MAGIC_REG (0xF0002000 + 0x3C)
#define SLAVE6_MAGIC_REG (0xF0002000 + 0x3C)
#define SLAVE7_MAGIC_REG (0xF0002000 + 0x3C)
#else
#define SLAVE1_MAGIC_REG (SRAMROM_BASE+0x38)
#define SLAVE2_MAGIC_REG (SRAMROM_BASE+0x38)
#define SLAVE3_MAGIC_REG (SRAMROM_BASE+0x38)
#define SLAVE4_MAGIC_REG (SRAMROM_BASE+0x3C)
#define SLAVE5_MAGIC_REG (SRAMROM_BASE+0x3C)
#define SLAVE6_MAGIC_REG (SRAMROM_BASE+0x3C)
#define SLAVE7_MAGIC_REG (SRAMROM_BASE+0x3C)
#endif

#define SLAVE1_MAGIC_NUM 0x534C4131
#define SLAVE2_MAGIC_NUM 0x4C415332
#define SLAVE3_MAGIC_NUM 0x41534C33
#define SLAVE4_MAGIC_NUM 0x534C4134
#define SLAVE5_MAGIC_NUM 0x4C415335
#define SLAVE6_MAGIC_NUM 0x41534C36
#define SLAVE7_MAGIC_NUM 0x534C4137

#define SLAVE_JUMP_REG  (SRAMROM_BASE+0x34)


extern void mt_secondary_startup(void);
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);
extern void mt_gic_secondary_init(void);
extern u32 get_devinfo_with_index(u32 index);
extern unsigned int mt_cpufreq_hotplug_notify(unsigned int ncpu);


extern unsigned int irq_total_secondary_cpus;
#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
static unsigned int is_secondary_cpu_first_boot = 0;
#endif
static DEFINE_SPINLOCK(boot_lock);

static const unsigned int secure_magic_reg[] = {SLAVE1_MAGIC_REG, SLAVE2_MAGIC_REG, SLAVE3_MAGIC_REG, SLAVE4_MAGIC_REG, SLAVE5_MAGIC_REG, SLAVE6_MAGIC_REG, SLAVE7_MAGIC_REG};
static const unsigned int secure_magic_num[] = {SLAVE1_MAGIC_NUM, SLAVE2_MAGIC_NUM, SLAVE3_MAGIC_NUM, SLAVE4_MAGIC_NUM, SLAVE5_MAGIC_NUM, SLAVE6_MAGIC_NUM, SLAVE7_MAGIC_NUM};
typedef int (*spm_mtcmos_ctrl_func)(int state, int chkWfiBeforePdn);
static const spm_mtcmos_ctrl_func secure_ctrl_func[] = {spm_mtcmos_ctrl_cpu1, spm_mtcmos_ctrl_cpu2, spm_mtcmos_ctrl_cpu3, spm_mtcmos_ctrl_cpu4, spm_mtcmos_ctrl_cpu5, spm_mtcmos_ctrl_cpu6, spm_mtcmos_ctrl_cpu7};

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen".
 */
volatile int pen_release = -1;


/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
    pen_release = val;
    smp_wmb();
    __cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
    outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

#if (CONFIG_MTK_FAKE_CORE_COUNT == 0)
/* use efuse setting */

int get_core_count(void){

//#if defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
//    printk("return get_core_count for TEE:4\n");
//    return 4;
//#endif

    unsigned int cores = 0;

#if 1
    cores = get_devinfo_with_index(3);
    HOTPLUG_INFO("get_devinfo_with_index(3): 0x%08x\n", cores);
    //evaluate bits 15:12 to verify the core count in cluster 1
    if ((cores & 0x0000F000) == 0x0000F000)
        cores = 4;
    else if ((cores & 0x0000F000) == 0x0000C000)
        cores = 6;
    else
        cores = 8;
#else
    asm volatile(
    "MRC p15, 1, %0, c9, c0, 2\n"
    : "=r" (cores)
    :
    : "cc"
    );

    cores = cores >> 24;
    cores += 1;    
#endif

    printk("return get_core_count:%d\n",cores);
    return cores;  
}
#else
/* use fake core count */
int get_core_count(void){
	return CONFIG_MTK_FAKE_CORE_COUNT;
}
#endif /* end of CONFIG_MTK_FAKE_CORE_COUNT */

void __cpuinit platform_secondary_init(unsigned int cpu)
{
    struct wd_api *wd_api = NULL;

    printk(KERN_INFO "Slave cpu init\n");
    HOTPLUG_INFO("platform_secondary_init, cpu: %d\n", cpu);

    mt_gic_secondary_init();

    /*
     * let the primary processor know we're out of the
     * pen, then head off into the C entry point
     */
    write_pen_release(-1);

    get_wd_api(&wd_api);
    if (wd_api)
        wd_api->wd_cpu_hot_plug_on_notify(cpu);

    fiq_glue_resume();

#ifdef SPM_MCDI_FUNC
    spm_hot_plug_in_before(cpu);
#endif

#ifdef CONFIG_MTK_SCHED_TRACERS
    trace_cpu_hotplug(cpu, 1, per_cpu(last_event_ts, cpu));
    per_cpu(last_event_ts, cpu) = ns2usecs(ftrace_now(cpu));
#endif

    /*
     * Synchronise with the boot thread.
     */
    spin_lock(&boot_lock);
    spin_unlock(&boot_lock);
}

#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
static void __cpuinit mt_wakeup_cpu(int cpu)
{
    if (is_secondary_cpu_first_boot)
    {
    	printk("mt_wakeup_cpu: first boot!(%d)\n", cpu);
        --is_secondary_cpu_first_boot;
        mt65xx_reg_sync_writel(secure_magic_num[cpu-1], secure_magic_reg[cpu-1]);
        HOTPLUG_INFO("SLAVE%d_MAGIC_NUM:%x\n", cpu, secure_magic_num[cpu-1]);
    }
#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
    else
    {
    	printk("mt_wakeup_cpu: not first boot!(%d)\n", cpu);
        mt65xx_reg_sync_writel(virt_to_phys(mt_secondary_startup), BOOT_ADDR);        
        (*secure_ctrl_func[cpu-1])(STA_POWER_ON, 1);
    }
#endif
}
#endif

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
    unsigned long timeout;

    printk(KERN_CRIT "Boot slave CPU\n");

    atomic_inc(&hotplug_cpu_count);

    /*
     * Set synchronisation state between this boot processor
     * and the secondary one
     */
    spin_lock(&boot_lock);

    HOTPLUG_INFO("boot_secondary, cpu: %d\n", cpu);
    /*
     * The secondary processor is waiting to be released from
     * the holding pen - release it, then wait for it to flag
     * that it has been released by resetting pen_release.
     *
     * Note that "pen_release" is the hardware CPU ID, whereas
     * "cpu" is Linux's internal ID.
     */
    /*
     * This is really belt and braces; we hold unintended secondary
     * CPUs in the holding pen until we're ready for them.  However,
     * since we haven't sent them a soft interrupt, they shouldn't
     * be there.
     */
    write_pen_release(cpu);

    switch(cpu)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            if (((cpu == 5) && (cpu_online(4) == 0) && (cpu_online(6) == 0) && (cpu_online(7) == 0)) || 
                ((cpu == 6) && (cpu_online(4) == 0) && (cpu_online(5) == 0) && (cpu_online(7) == 0)) || 
                ((cpu == 7) && (cpu_online(4) == 0) && (cpu_online(5) == 0) && (cpu_online(6) == 0)))
            {
                HOTPLUG_INFO("up CPU%d fail, please up CPU4 first\n", cpu);
                spin_unlock(&boot_lock);
                atomic_dec(&hotplug_cpu_count);
                return -ENOSYS;
            }
#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
            mt_wakeup_cpu(cpu);
#else //#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
            printk("mt_wakeup_cpu: not first boot!(%d)\n", cpu);
            mt_secure_call(MC_FC_SET_RESET_VECTOR, virt_to_phys(mt_secondary_startup), cpu, 0);
			(*secure_ctrl_func[cpu-1])(STA_POWER_ON, 1);
#endif //#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
            break;

        default:
            break;

    }

#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
    smp_cross_call(cpumask_of(cpu));
#endif //#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)

    /*
     * Now the secondary core is starting up let it run its
     * calibrations, then wait for it to finish
     */
    spin_unlock(&boot_lock);

    timeout = jiffies + (1 * HZ);
    while (time_before(jiffies, timeout)) {
        smp_rmb();
        if (pen_release == -1)
            break;

        udelay(10);
    }

    if (pen_release == -1)
    {
        mt_cpufreq_hotplug_notify(num_online_cpus());
        return 0;
    }
    else
    {
        if (cpu < 4)
        {
            mt65xx_reg_sync_writel(cpu + 8, MP0_DBG_CTRL);
            printk(KERN_EMERG "CPU%u, MP0_DBG_CTRL: 0x%08x, MP0_DBG_FLAG: 0x%08x\n", cpu, *(volatile u32 *)(MP0_DBG_CTRL), *(volatile u32 *)(MP0_DBG_FLAG));
        }
        else
        {
            mt65xx_reg_sync_writel(cpu - 4 + 8, MP1_DBG_CTRL);
            printk(KERN_EMERG "CPU%u, MP1_DBG_CTRL: 0x%08x, MP1_DBG_FLAG: 0x%08x\n", cpu, *(volatile u32 *)(MP1_DBG_CTRL), *(volatile u32 *)(MP1_DBG_FLAG));
        }
        on_each_cpu((smp_call_func_t)dump_stack, NULL, 0);
        atomic_dec(&hotplug_cpu_count);
        return -ENOSYS;
    }
}

void __init smp_init_cpus(void)
{
    unsigned int i = 0, ncores;
    
    /* Enable CA7 snoop function */
    REG_WRITE(MP0_AXI_CONFIG, REG_READ(MP0_AXI_CONFIG) & ~ACINACTM);
    
    /* Enables DVM */
    asm volatile(
        "MRC p15, 0, %0, c1, c0, 1\n"
        "BIC %0, %0, #1 << 15\n"        /* DDVM: bit15 */
        "MCR p15, 0, %0, c1, c0, 1\n"
        : "+r"(i)
        :
        : "cc"
    );
    
    /* Enable snoop requests and DVM message requests*/
    REG_WRITE(CCI400_SI4_SNOOP_CONTROL, REG_READ(CCI400_SI4_SNOOP_CONTROL) | (SNOOP_REQ | DVM_MSG_REQ));
    while (REG_READ(CCI400_STATUS) & CHANGE_PENDING);
    
    ncores = get_core_count();
    if (ncores > NR_CPUS) {
        printk(KERN_WARNING
               "L2CTLR core count (%d) > NR_CPUS (%d)\n", ncores, NR_CPUS);
        printk(KERN_WARNING
               "set nr_cores to NR_CPUS (%d)\n", NR_CPUS);
        ncores = NR_CPUS;
    }

    for (i = 0; i < ncores; i++)
        set_cpu_possible(i, true);

#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
    irq_total_secondary_cpus = num_possible_cpus() - 1;
    is_secondary_cpu_first_boot = num_possible_cpus() - 1;
#endif //#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)

    set_smp_cross_call(irq_raise_softirq);

#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
    spm_mtcmos_init_cpusys1_1st_bring_up(ncores);
#endif //#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
    int i;

    for (i = 0; i < max_cpus; i++)
        set_cpu_present(i, true);

#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
    /* write the address of slave startup into the system-wide flags register */
    mt65xx_reg_sync_writel(virt_to_phys(mt_secondary_startup), SLAVE_JUMP_REG);
#endif //#if !defined (CONFIG_TRUSTONIC_TEE_SUPPORT)
}
