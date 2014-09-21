#ifndef __DRAMC_H__
#define __DRAMC_H__


#define Vcore_HV_1066  (0x40)   //1.1
#define Vcore_NV_1066  (0x30)   //1.0
#define Vcore_LV_1066  (0x20)  //0.9

#define Vmem_HV_1066 (0x60) //1.3
#define Vmem_NV_1066 (0x54) //1.22
#define Vmem_LV_1066 (0x4A) //1.16
//#define Vmem_LV_1066 (0x4A-0x2) //1.16

#define Vcore_HV_1466  (0x54)   //1.225
#define Vcore_NV_1466  (0x44)   //1.125
#define Vcore_LV_1466  (0x37)   //1.04375
//#define Vcore_LV_1466  (0x37-0x3)       //1.04375 for kenneth HQA

#define Vmem_HV_1466 (0x60) //1.3
#define Vmem_NV_1466 (0x54) //1.22
#define Vmem_LV_1466 (0x4A) //1.16
//#define Vmem_LV_1466 (0x4A-0x2) //1.16

#define Vcore_HV_1200  (0x40)   //1.1
#define Vcore_NV_1200  (0x38)   //1.05
#define Vcore_LV_1200  (0x29)  //0.95625
//#define Vcore_LV_1200  (0x29-0x2)  //0.95625

#define Vmem_HV_1200 (0x60) //1.3
#define Vmem_NV_1200 (0x54) //1.22
#define Vmem_LV_1200 (0x4A) //1.16
//#define Vmem_LV_1200 (0x4A-0x2) //1.16

#define Vcore_HV_1333  (0x54)   //1.225
#define Vcore_NV_1333  (0x44)   //1.125
#define Vcore_LV_1333  (0x34)   //1.025

#define Vmem_HV_1333 (0x60) //1.3
#define Vmem_NV_1333 (0x54) //1.22
#define Vmem_LV_1333 (0x4A) //1.16


#define DDR_1466 367
#define DDR_1200 300
#define DDR_1066 266
#define DDR_1333 333
#define MEM_TEST_SIZE 0x2000
#define DRAMC_READ_REG(offset)         ( \
                                        (*(volatile unsigned int *)(DRAMC0_BASE + (offset))) |\
                                        (*(volatile unsigned int *)(DDRPHY_BASE + (offset))) |\
                                        (*(volatile unsigned int *)(DRAMC_NAO_BASE + (offset))) \
                                        )


#define DRAMC_WRITE_REG(val,offset)     do{ \
                                      (*(volatile unsigned int *)(DRAMC0_BASE + (offset))) = (unsigned int)(val); \
                                      (*(volatile unsigned int *)(DDRPHY_BASE + (offset))) = (unsigned int)(val); \
                                      (*(volatile unsigned int *)(DRAMC_NAO_BASE + (offset))) = (unsigned int)(val); \
                                      }while(0)



#define DMA_BASE_CH(n)     IOMEM((AP_DMA_BASE + 0x0080 * (n + 1)))
#define DMA_SRC(base)           IOMEM((base + 0x001C))
#define DMA_DST(base)           IOMEM((base + 0x0020))
#define DMA_LEN1(base)          IOMEM((base + 0x0024))
#define DMA_GLOBAL_GSEC_EN IOMEM((AP_DMA_BASE + 0x0014))
#define DMA_INT_EN(base)        IOMEM((base + 0x0004))
#define DMA_CON(base)           IOMEM((base + 0x0018))
#define DMA_START(base)         IOMEM((base + 0x0008))
#define DMA_INT_FLAG(base) IOMEM((base + 0x0000))

#define DMA_GDMA_LEN_MAX_MASK   (0x000FFFFF)
#define DMA_GSEC_EN_BIT         (0x00000001)
#define DMA_INT_EN_BIT          (0x00000001)
#define DMA_INT_FLAG_CLR_BIT (0x00000000)


#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
/*Config*/
//#define APDMA_TEST
//#define APDMAREG_DUMP
//#define TEMP_SENSOR
//#define VOLTAGE_DMA_DEBUG
#define PHASE_NUMBER 3
#define Voltage_Debug
#define Coding_Version 3
#define DRAM_BASE             (0x80000000)
#define BUFF_LEN   0x100
#define IOREMAP_ALIGMENT 0x1000
#define Delay_magic_num 0x295;   //We use GPT to measurement how many clk pass in 100us

#define DRAMC_CONF1     (0x004)
#define DRAMC_LPDDR2    (0x1e0)
#define DRAMC_PADCTL4   (0x0e4)
#define DRAMC_ACTIM1    (0x1e8)
#define DRAMC_DQSCAL0   (0x1c0)


enum
{
  LPDDR2 = 0,
  DDR3_16,
  DDR3_32,
  LPDDR3,
  mDDR,
};

enum
{
  LOW_FREQUENCY = 0,
  HIGH_FREQUENCY,
  EMULATED_GPU_HIGH_FREQ,
  EMULATED_GPU_LOW_FREQ,
};

enum
{
 speed_367=0,
 speed_333,
 speed_266,
};

enum
{
  Voltage_1_125=0,
  Voltage_1_05,
  Voltage_1_0,
  Voltage_1_075,
};


typedef struct
{
 int dram_dfs_enable;
 int dram_speed;
 int dram_type;
 int dram_vcore;
}DRAM_INFO;

enum
{
  LV = 0,
  NV,
  HV,
  HVc_LVm,
  LVc_HVm,
  HVc_NVm,
};

struct dfs_configuration
{
  unsigned int vcore_HF_HV;
  unsigned int vcore_HF_NV;
  unsigned int vcore_HF_LV;

  unsigned int vm_HF_HV;
  unsigned int vm_HF_NV;
  unsigned int vm_HF_LV;


  unsigned int vcore_LF_HV;
  unsigned int vcore_LF_NV;
  unsigned int vcore_LF_LV;

  unsigned int vm_LF_HV;
  unsigned int vm_LF_NV;
  unsigned int vm_LF_LV;

  unsigned int ddr_HF;
  unsigned int ddr_LF;
  unsigned int Freq_Status; //1:High Frequency, Low Frequency
  unsigned int V_Status;

};
extern unsigned int DMA_TIMES_RECORDER;
extern unsigned int get_max_DRAM_size(void);
int DFS_Detection(void);
int DFS_APDMA_Enable(void);
int DFS_APDMA_Init(void);
int DFS_APDMA_early_init(void);
void DFS_Voltage_Configuration(void);
int DFS_get_dram_data(int dfs_enable, int dram_dpeed,int dram_type,int dramc_vcore);
int get_DRAM_default_freq(void);
int get_DRAM_default_voltage(void);
//void DFS_phase_mt6333_config_interface(char dest_vol);
int get_ddr_type(void);
void DFS_Phase_delay(void);
extern volatile unsigned int Voltage_DMA_counter;
int get_rank_num(void);
#endif   /*__WDT_HW_H__*/
