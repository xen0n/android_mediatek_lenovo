################################################################################
#
################################################################################

LOCAL_PATH := $(call my-dir)

################################################################################
#
################################################################################
include $(CLEAR_VARS)

#-----------------------------------------------------------
LOCAL_SRC_FILES += AcdkCCAPTest.cpp

LOCAL_C_INCLUDES += \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/include \
    $(TOP)/$(MTK_PATH_PLATFORM)/hardware/include \
    $(MTK_PATH_PLATFORM)/hardware/include/mtkcam/acdk \
    $(TOP)/$(MTK_PATH_PLATFORM)/hardware/mtkcam/acdk/inc/acdk \
    $(TOP)/$(MTK_PATH_PLATFORM)/hardware/mtkcam/acdk/inc/cct \
    $(MTK_PATH_SOURCE)/external/mhal/src/custom/inc \
    $(MTK_PATH_SOURCE)/external/mhal/inc \
    $(MTK_PATH_SOURCE)/hardware/mtkcam/inc/acdk \
    $(MTK_PATH_CUSTOM)/kernel/imgsensor/inc \
  
#-----------------------------------------------------------
LOCAL_WHOLE_STATIC_LIBRARIES += 
#
LOCAL_STATIC_LIBRARIES += libacdk_entry_cctif
LOCAL_STATIC_LIBRARIES += libacdk_entry_mdk

#-----------------------------------------------------------
LOCAL_SHARED_LIBRARIES += liblog libcutils
LOCAL_SHARED_LIBRARIES += libdl

#-----------------------------------------------------------
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := ccaptest

#-----------------------------------------------------------
ifneq (yes,$(strip $(MTK_EMULATOR_SUPPORT)))
include $(BUILD_EXECUTABLE)
endif
