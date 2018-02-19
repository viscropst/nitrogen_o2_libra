#
# This policy configuration will be used by all qcom products
# that inherit from Lineage
#

BOARD_SEPOLICY_DIRS += \
    device/xiaomi/libra/sepolicy/oth/qcom/common \
    device/xiaomi/libra/sepolicy/oth/qcom/$(TARGET_BOARD_PLATFORM)
