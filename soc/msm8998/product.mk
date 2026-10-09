#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Firmware
## Note: linux-firmware repo does not have a540_gpmu.fw2
PRODUCT_PACKAGES += \
    linux_firmware_qcom-a530

# Modem
PRODUCT_PACKAGES += \
    rmtfs \
    rmtfs.rc \
    tqftpserv \
    tqftpserv.rc

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.qcom.soc.enable_modem_services=1

# QRTR
PRODUCT_PACKAGES += \
    qrtr-cfg
