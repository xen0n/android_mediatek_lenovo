ifneq ($(strip $(MTK_EMULATOR_SUPPORT)),yes)


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	PictureQualityGammaJni.cpp
	 
LOCAL_C_INCLUDES := $(JNI_H_INCLUDE)

LOCAL_C_INCLUDES += \
        $(KERNEL_HEADERS) \
        $(TOP)/frameworks/base/include \
        $(TOP)/mediatek/platform/mt6592/kernel/drivers/dispsys \
        $(MTK_PATH_PLATFORM)/../../hardware/dpframework/inc
	
LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE :=  libPQGAMMAjni

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)




endif
