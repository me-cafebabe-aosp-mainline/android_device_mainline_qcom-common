#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(TARGET_USES_TIMEKEEP),true)

PRODUCT_PACKAGES += \
    com.sony.timekeep

endif # TARGET_USES_TIMEKEEP
