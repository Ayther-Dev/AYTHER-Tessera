#include <ayther/ayther_sdk_version.h>
#include <ayther/engine/capabilities.hpp>
#include "engine_pod.h"

#include <iostream>

int main() {
    const auto headers = ayther::sdk_headers_version();
    const auto linked = ayther::sdk_version();
    const auto engine = ayther::engine::version();
    const auto core_abi = ayther::engine::core_abi_revision();
    std::cout << "ENGINE_LINK_CHECK headers=" << headers.str()
              << " linked=" << linked.str()
              << " engine=" << engine.major << '.' << engine.minor << '.' << engine.patch
              << " core_abi=" << core_abi << '\n';
    const auto mismatch = ayther::sdk_version_check(headers, linked);
    if (!mismatch.empty()) {
        std::cerr << "ENGINE_SDK_MISMATCH: " << mismatch << '\n';
        return 1;
    }
    if (engine.major != linked.major || engine.minor != linked.minor || engine.patch != linked.patch || core_abi != AYTHER_CORE_C_ABI_REVISION) {
        std::cerr << "ENGINE_ARTIFACT_MISMATCH: Engine version or core ABI differs from the installed SDK.\n";
        return 1;
    }
    static_assert(sizeof(Phase0SdkInfo) == 8 && alignof(Phase0SdkInfo) == 4);
    const Phase0SdkInfo info{linked.number(), core_abi};
    if (phase0_sdk_encode(info) != ((static_cast<std::uint64_t>(core_abi) << 32) | headers.number())) {
        std::cerr << "ENGINE_C_POD_MISMATCH: C and C++ units disagree on SDK values.\n";
        return 1;
    }
    return 0;
}
