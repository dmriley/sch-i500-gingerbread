# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	make_ext4fs.c \
        ext4_utils.c \
        allocate.c \
        backed_block.c \
        output_file.c \
        contents.c \
        extent.c \
        indirect.c \
        uuid.c \
        sha1.c \
	sparse_crc32.c

LOCAL_MODULE := libext4_utils
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += external/zlib
LOCAL_STATIC_LIBRARIES := libz
LOCAL_PRELINK_MODULE := false

include $(BUILD_STATIC_LIBRARY)
