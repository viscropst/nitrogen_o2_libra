ifneq ($(filter msm8992 msm8994,$(TARGET_BOARD_PLATFORM)),)
include $(call all-named-subdir-makefiles,libbt-vendor)
endif # is-vendor-board-platform
