ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := animationchooser.c
LOCAL_MODULE := animationchooser
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc

include $(BUILD_EXECUTABLE)

endif   # TARGET_ARCH == arm
endif    # !TARGET_SIMULATOR

