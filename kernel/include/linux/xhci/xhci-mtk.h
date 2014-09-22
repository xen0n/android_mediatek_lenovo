#ifndef _XHCI_MTK_H
#define _XHCI_MTK_H

#include <linux/usb.h>


#define SSUSB_U3_BASE		0xf1270000
#define SSUSB_U3_XHCI_BASE	SSUSB_U3_BASE
#define SSUSB_U3_MAC_BASE	(SSUSB_U3_BASE + 0x2400)
#define SSUSB_U3_SYS_BASE	(SSUSB_U3_BASE + 0x2600)
#define SSUSB_U2_SYS_BASE	(SSUSB_U3_BASE + 0x3400)
#define SIFSLV_IPPC 		0xf1280700

#define U3_PIPE_LATCH_SEL_ADD 		SSUSB_U3_MAC_BASE + 0x130
#define U3_PIPE_LATCH_TX	0
#define U3_PIPE_LATCH_RX	0

#define U3_UX_EXIT_LFPS_TIMING_PAR	0xa0
#define U3_REF_CK_PAR	0xb0
#define U3_RX_UX_EXIT_LFPS_REF_OFFSET	8
#define U3_RX_UX_EXIT_LFPS_REF	3
#define	U3_REF_CK_VAL	10

#define U3_TIMING_PULSE_CTRL	0xb4
#define CNT_1US_VALUE			63	//62.5MHz:63, 70MHz:70, 80MHz:80, 100MHz:100, 125MHz:125

#define USB20_TIMING_PARAMETER	0x40
#define TIME_VALUE_1US			63	//62.5MHz:63, 80MHz:80, 100MHz:100, 125MHz:125

#define LINK_PM_TIMER	0x8
#define PM_LC_TIMEOUT_VALUE	3


#define SSUSB_IP_PW_CTRL	(SIFSLV_IPPC+0x0)
#define SSUSB_IP_SW_RST		(1<<0)
#define SSUSB_IP_PW_CTRL_1	(SIFSLV_IPPC+0x4)
#define SSUSB_IP_PDN		(1<<0)
#define SSUSB_U3_CTRL(p)	(SIFSLV_IPPC+0x30+(p*0x08))
#define SSUSB_U3_PORT_DIS	(1<<0)
#define SSUSB_U3_PORT_PDN	(1<<1)
#define SSUSB_U3_PORT_HOST_SEL	(1<<2)
#define SSUSB_U3_PORT_CKBG_EN	(1<<3)
#define SSUSB_U3_PORT_MAC_RST	(1<<4)
#define SSUSB_U3_PORT_PHYD_RST	(1<<5)
#define SSUSB_U2_CTRL(p)	(SIFSLV_IPPC+(0x50)+(p*0x08))
#define SSUSB_U2_PORT_DIS	(1<<0)
#define SSUSB_U2_PORT_PDN	(1<<1)
#define SSUSB_U2_PORT_HOST_SEL	(1<<2)
#define SSUSB_U2_PORT_CKBG_EN	(1<<3)
#define SSUSB_U2_PORT_MAC_RST	(1<<4)
#define SSUSB_U2_PORT_PHYD_RST	(1<<5)
#define SSUSB_IP_CAP			(SIFSLV_IPPC+0x024)

#define SSUSB_U3_PORT_NUM(p)	(p & 0xff)
#define SSUSB_U2_PORT_NUM(p)	((p>>8) & 0xff)

extern void reinitIP(void);
extern void setInitialReg(void);
extern int dbg_u3init(int argc, char**argv);

/*
  mediatek probe out
*/
/************************************************************************************/

#define SW_PRB_OUT_ADDR	(SIFSLV_IPPC+0xc0)		//0xf00447c0
#define PRB_MODULE_SEL_ADDR	(SIFSLV_IPPC+0xbc)	//0xf00447bc

static inline void mtk_probe_init(const u32 byte){
	__u32 __iomem *ptr = (__u32 __iomem *) PRB_MODULE_SEL_ADDR;
	writel(byte, ptr);
}

static inline void mtk_probe_out(const u32 value){
	__u32 __iomem *ptr = (__u32 __iomem *) SW_PRB_OUT_ADDR;
	writel(value, ptr);
}

static inline u32 mtk_probe_value(void){
	__u32 __iomem *ptr = (__u32 __iomem *) SW_PRB_OUT_ADDR;

	return readl(ptr);
}


#endif
