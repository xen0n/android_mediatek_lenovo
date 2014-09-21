#ifndef _MT_CPUFREQ_H
#define _MT_CPUFREQ_H

#include <linux/module.h>

/*********************
* SOC Efuse Register
**********************/
#define M_HW_RES4    (0xF0206174)

/*********************
* Clock Mux Register
**********************/
#define TOP_CKMUXSEL    (0xF0001000)
#define TOP_CKDIV1_CPU  (0xF0001008)

/****************************
* PMIC Wrapper DVFS Register
*****************************/
#define PWRAP_BASE              (0xF000D000)
#define PMIC_WRAP_DVFS_ADR0     (PWRAP_BASE + 0xE4)
#define PMIC_WRAP_DVFS_WDATA0   (PWRAP_BASE + 0xE8)
#define PMIC_WRAP_DVFS_ADR1     (PWRAP_BASE + 0xEC)
#define PMIC_WRAP_DVFS_WDATA1   (PWRAP_BASE + 0xF0)
#define PMIC_WRAP_DVFS_ADR2     (PWRAP_BASE + 0xF4)
#define PMIC_WRAP_DVFS_WDATA2   (PWRAP_BASE + 0xF8)
#define PMIC_WRAP_DVFS_ADR3     (PWRAP_BASE + 0xFC)
#define PMIC_WRAP_DVFS_WDATA3   (PWRAP_BASE + 0x100)
#define PMIC_WRAP_DVFS_ADR4     (PWRAP_BASE + 0x104)
#define PMIC_WRAP_DVFS_WDATA4   (PWRAP_BASE + 0x108)
#define PMIC_WRAP_DVFS_ADR5     (PWRAP_BASE + 0x10C)
#define PMIC_WRAP_DVFS_WDATA5   (PWRAP_BASE + 0x110)
#define PMIC_WRAP_DVFS_ADR6     (PWRAP_BASE + 0x114)
#define PMIC_WRAP_DVFS_WDATA6   (PWRAP_BASE + 0x118)
#define PMIC_WRAP_DVFS_ADR7     (PWRAP_BASE + 0x11C)
#define PMIC_WRAP_DVFS_WDATA7   (PWRAP_BASE + 0x120)

/****************************
* SOC DVFS request 
*****************************/
typedef enum
{
    SOC_DVFS_TYPE_VENC = 0,
    SOC_DVFS_TYPE_ZSD,
    SOC_DVFS_TYPE_GPU_HP,
    SOC_DVFS_TYPE_WIFI_DISPLAY,
    SOC_DVFS_TYPE_DISPLAY,
	#ifdef CONFIG_MTK_FLIPER
    SOC_DVFS_TYPE_FLIPER,
	#endif
    SOC_DVFS_TYPE_PAUSE,
    SOC_DVFS_TYPE_FIXED,
    SOC_DVFS_TYPE_NUM
}SOC_DVFS_TYPE_ENUM;

/****************************
* mTH parameter 
*****************************/
/* 1.7GHz */
#define MTH_DELTA_COUNTER_LIMIT_1P7GHz         (10)     //300ms
#define MTH_FREQ_STEP_IDX_1_1P7GHz             (1)      // downgrade frequency 1 level, 45C < x < 65C
#define MTH_FREQ_STEP_IDX_2_1P7GHz             (1)      // downgrade frequency 1 level, 65C < x
#define MTH_TEMP_LIMIT_1_1P7GHz                (45000)  //45000; //45C
#define MTH_TEMP_LIMIT_2_1P7GHz                (65000)  //65000; //65C
#define MTH_TEMP_DELTA_1_1P7GHz                (5000)   // tempature delta 5C
#define MTH_TEMP_DELTA_1_RATIO_1_1P7GHz        (0)      // 0:10 , 45C < x < 65C, delta < 5C
#define MTH_TEMP_DELTA_2_RATIO_1_1P7GHz        (5)      // 5:5, 45C < x < 65C, delta > 5C
#define MTH_TEMP_DELTA_1_RATIO_2_1P7GHz        (0)      // 0:10, 65C < x, delta < 5C
#define MTH_TEMP_DELTA_2_RATIO_2_1P7GHz        (8)      // 8:2, 65C < x, delta > 5C

/* 2GHz */
#define MTH_DELTA_COUNTER_LIMIT_2GHz         (20)     //600ms
#define MTH_FREQ_STEP_IDX_1_2GHz             (1)      // downgrade frequency 1 level, 45C < x < 65C
#define MTH_FREQ_STEP_IDX_2_2GHz             (2)      // downgrade frequency 2 level, 65C < x
#define MTH_TEMP_LIMIT_1_2GHz                (45000)  //45000; //45C
#define MTH_TEMP_LIMIT_2_2GHz                (70000)  //70000; //70C
#define MTH_TEMP_DELTA_1_2GHz                (5000)   // tempature delta 5C
#define MTH_TEMP_DELTA_1_RATIO_1_2GHz        (12)      // 12:8 , 45C < x < 65C, delta < 5C
#define MTH_TEMP_DELTA_2_RATIO_1_2GHz        (18)      // 18:2, 45C < x < 65C, delta > 5C
#define MTH_TEMP_DELTA_1_RATIO_2_2GHz        (10)      // 10:10, 65C < x, delta < 5C
#define MTH_TEMP_DELTA_2_RATIO_2_2GHz        (18)      // 18:2, 65C < x, delta > 5C

/*****************
* extern function 
******************/
extern int mt_cpufreq_state_set(int enabled);
extern void mt_cpufreq_thermal_protect(unsigned int limited_power);
void mt_cpufreq_enable_by_ptpod(void);
unsigned int mt_cpufreq_disable_by_ptpod(void);
extern unsigned int mt_cpufreq_max_frequency_by_DVS(unsigned int num);
void mt_cpufreq_return_default_DVS_by_ptpod(void);
extern bool mt_cpufreq_earlysuspend_status_get(void);
extern unsigned int mt_soc_dvfs(SOC_DVFS_TYPE_ENUM type, unsigned int sochp_enable);
extern void DFS_Phase_delay(void);
extern void DFS_phase_mt6333_config_interface(char dest_vol);
#endif