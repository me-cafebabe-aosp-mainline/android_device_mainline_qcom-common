#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(TARGET_GRAPHICS),mesa)

ifneq ($(TARGET_QCOM_SOC_FAMILY_IS_LEGACY),true)
PRODUCT_VENDOR_PROPERTIES += \
    ro.opengles.version?=196610

TARGET_MESA_SUPPORTS_VULKAN ?= true
endif

ifeq ($(TARGET_MESA_SUPPORTS_VULKAN),true)
PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.vulkan=freedreno
endif

TARGET_USES_VULKAN ?= $(TARGET_MESA_SUPPORTS_VULKAN)

endif # TARGET_GRAPHICS
