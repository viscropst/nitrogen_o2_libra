# TODO:  Find a better way to separate build configs for ADP vs non-ADP devices
ifneq ($(BOARD_IS_AUTOMOTIVE),true)
  ifneq ($(filter caf-msm8916 caf-msm8952 caf-msm8992 caf-msm8996 caf-msm8998,$(TARGET_QCOM_BLUETOOTH_VARIANT)),)
    ifeq (caf-msm8994,$(TARGET_QCOM_BLUETOOTH_VARIANT))
        TARGET_QCOM_BLUETOOTH_VARIANT := caf-msm8992
    endif
    include $(call all-named-subdir-makefiles,$(TARGET_QCOM_BLUETOOTH_VARIANT))
  else
   ifneq ($(filter msm8x27 msm8226 msm8974 8960,$(TARGET_BOARD_PLATFORM)),)
     include $(call all-named-subdir-makefiles,msm8960)
   else ifneq ($(filter msm8994,$(TARGET_BOARD_PLATFORM)),)
     include $(call all-named-subdir-makefiles,msm8992)
   else ifneq ($(wildcard $(LOCAL_PATH)/$(TARGET_BOARD_PLATFORM)),)
     include $(call all-named-subdir-makefiles,$(TARGET_BOARD_PLATFORM))
   endif
  endif
endif
