/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <stdlib.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#define LOG_TAG "msm_drm_quirks"
#include <android-base/logging.h>
#include <android-base/properties.h>

#include <android-base/unique_fd.h>

#include <xf86drm.h>

using namespace android::base;

namespace {

constexpr char kMesaFdMesaDebugProp[] = "vendor.mesa.fd.mesa.debug";
constexpr char kMinigbmAvoidUbwcProp[] = "vendor.minigbm.avoid_ubwc";

const std::string kPropPrefix = "vendor.qcom.soc.msm_drm.";

// From msm_drm.h, which is not usable from C++ (member named "or").
// libdrm_freedreno is not used because it refuses GPUs that report
// MSM_PARAM_GPU_ID as 0, which is every Adreno that is only identified by
// its chip ID.
constexpr unsigned long kDrmMsmGetParam = 0x00;
constexpr uint32_t kMsmPipe3d0 = 0x10;
constexpr uint32_t kMsmParamGpuId = 0x01;
constexpr uint32_t kMsmParamChipId = 0x03;

struct MsmParam {
    uint32_t pipe;
    uint32_t param;
    uint64_t value;
    uint32_t len;
    uint32_t pad;
};

bool GetMsmParam(int fd, uint32_t param, uint64_t* value) {
    MsmParam get_param = {
            .pipe = kMsmPipe3d0,
            .param = param,
    };
    if (drmCommandWriteRead(fd, kDrmMsmGetParam, &get_param, sizeof(get_param))) return false;
    *value = get_param.value;
    return true;
}

// Values of ro.opengles.version
constexpr int kGlesVersion20 = 131072;
constexpr int kGlesVersion30 = 196608;
constexpr int kGlesVersion32 = 196610;

// OpenGL ES version of Mesa freedreno on each Adreno series, as in
// essl_feature_level of freedreno_screen.c
int GetGlesVersion(uint64_t gpu_id) {
    if (gpu_id >= 400) return kGlesVersion32;
    if (gpu_id >= 300) return kGlesVersion30;
    return kGlesVersion20;
}

int Work(int fd) {
    uint64_t chip_id = 0, gpu_id = 0;

    const bool have_chip_id = GetMsmParam(fd, kMsmParamChipId, &chip_id);
    const bool have_gpu_id = GetMsmParam(fd, kMsmParamGpuId, &gpu_id);
    if (!have_chip_id && !have_gpu_id) {
        LOG(ERROR) << "Failed to get the GPU parameters";
        return EXIT_SUCCESS;
    }

    // Same as fd_dev_gpu_id() in Mesa. Chip ID is 0xCCMMNNPP (core, major,
    // minor, patch), and it has the speed bin in the upper 32 bits.
    if (!gpu_id && (chip_id & UINT32_MAX)) {
        gpu_id = ((chip_id >> 24) & 0xff) * 100 + ((chip_id >> 16) & 0xff) * 10 +
                 ((chip_id >> 8) & 0xff);
    }
    LOG(INFO) << "chip_id = " << std::to_string(chip_id) << " gpu_id = " << std::to_string(gpu_id);
    SetProperty(kPropPrefix + "chip_id", std::to_string(chip_id));
    SetProperty(kPropPrefix + "gpu_id", std::to_string(gpu_id));

    // ro.opengles.version is set by init, see init.mainline.qcom.rc
    SetProperty(kPropPrefix + "gles_version", std::to_string(GetGlesVersion(gpu_id)));

    // Adreno 5xx Mesa Freedreno quirks
    if (gpu_id >= 500 && gpu_id <= 599) {
        SetProperty(kMesaFdMesaDebugProp, "sysmem");
    }

    /*
     * Let minigbm avoid UBWC for pre Adreno 6xx
     *
     * Insert this into should_avoid_ubwc() on minigbm/msm.c:
     *
    #ifdef __ANDROID__
    static bool prop_parsed = false, prop_value = false;
    const char *prop_buf;
    if (prop_parsed) {
        return prop_value;
    } else {
        prop_buf = drv_get_os_option("vendor.minigbm.avoid_ubwc");
        prop_value = prop_buf && !strcmp(prop_buf, "true");
        prop_parsed = true;
        return prop_value;
    }
    #endif
     *
     */
    if (gpu_id < 600) {
        SetProperty(kMinigbmAvoidUbwcProp, "true");
    }

    return EXIT_SUCCESS;
}

// The msm DRM device is not necessarily card0, e.g. when another display
// driver probes first, so look for it by driver name.
unique_fd OpenMsmCard() {
    namespace fs = std::filesystem;

    std::error_code ec;
    std::vector<fs::path> cards;
    for (const auto& entry : fs::directory_iterator("/dev/dri", ec)) {
        if (entry.path().filename().string().starts_with("card")) cards.push_back(entry.path());
    }
    if (ec) {
        LOG(ERROR) << "Failed to list /dev/dri: " << ec.message();
        return {};
    }
    std::sort(cards.begin(), cards.end());

    for (const auto& card : cards) {
        unique_fd fd(open(card.c_str(), O_RDWR | O_CLOEXEC));
        if (!fd.ok()) {
            PLOG(WARNING) << "open(" << card << ") failed";
            continue;
        }

        drmVersionPtr version = drmGetVersion(fd.get());
        if (!version) continue;
        const bool is_msm = std::string_view(version->name, version->name_len) == "msm";
        drmFreeVersion(version);
        if (is_msm) {
            LOG(INFO) << "Using " << card;
            return fd;
        }
    }
    return {};
}

}  // namespace

int main(int, char* argv[]) {
    InitLogging(argv, &KernelLogger);

    unique_fd fd = OpenMsmCard();
    if (!fd.ok()) {
        LOG(ERROR) << "No msm DRM device found";
        return EXIT_SUCCESS;
    }

    return Work(fd.get());
}
