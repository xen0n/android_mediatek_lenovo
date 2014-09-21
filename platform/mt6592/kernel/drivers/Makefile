include $(MTK_PATH_BUILD)/common.mk
#Fix-me: modified for early porting
#$(call all-subdir-src-or-makefile)
obj-y   += uart/
obj-y   += gpio/
obj-y   += power/
obj-y   += power_gs/
obj-y   += usb20/
obj-y   += m4u/
obj-y   += smi/
obj-y   += keypad/
obj-y   += ../../../common/kernel/drivers/video/
obj-y   += video/
obj-y   += hdmitx/
obj-y   += dispsys/
obj-y   += i2c/
obj-y   += mmc-host/
obj-y   += pmic_wrap/
obj-y   += pmt/
obj-y   += accdet/
obj-y   += vibrator/
obj-y   += leds/
obj-y   += sound/
obj-y   += spi/
obj-$(CONFIG_MTK_SYSTRACKER)   += systracker/
obj-$(CONFIG_MTK_BTIF)  += btif/
ifneq ($(CONFIG_MTK_COMBO),)
ifneq ($(filter CONSYS_6592,$(MTK_COMBO_CHIP)),)
obj-y   += wmt/
endif
endif
obj-y   += mrdump/

ifneq ($(CONFIG_EARLY_LINUX_PORTING),y)
obj-y   += pwm/
obj-y   += dual_ccci/
obj-y   += btcvsd/
obj-y   += mmc-host/
obj-y   += jpeg/

obj-y   += rtc/
obj-y   += wdt/
obj-y   += devinfo/
obj-y   += devapc/
obj-y   += auxadc/
obj-y   += ldvt/
obj-y   += videocodec/
obj-y   += mjc/
obj-y   += masp/
obj-y   += thermal/
include $(MTK_PATH_BUILD)/common.mk
obj-y   += $(call all-subdirs-with-makefile,cpu_stress)
#obj-y   += nand/
#obj-m   += mdrvb/
#obj-y   += touchpanel/
#obj-y   += ccci/
#
#obj-y   += rtc/
#
#obj-y   += jpeg/
#obj-y   += gdma/
#
endif
ifneq   ($(MTK_GPU_SUPPORT),no)
obj-y   += gpu/
endif

ifeq ($(TRUSTONIC_TEE_SUPPORT),yes)
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += gud/
endif

obj-y	+= $(call all-subdirs-with-makefile,met)

ifeq ($(MTK_NFC_SUPPORT),yes)
obj-y	+= nfc/
endif
