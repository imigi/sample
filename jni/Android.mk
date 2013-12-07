LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := movieplayer 
LOCAL_SRC_FILES := movieplayer_gl2.c movieplayer_omax.c
LOCAL_LDLIBS    := -llog -lGLESv2 -lOpenMAXAL -landroid

include $(BUILD_SHARED_LIBRARY)

