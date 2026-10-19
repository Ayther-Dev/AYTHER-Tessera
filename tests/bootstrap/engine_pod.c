#include "engine_pod.h"
_Static_assert(sizeof(Phase0SdkInfo) == 8, "The phase 0 POD must contain two adjacent u32 fields.");
uint64_t __cdecl phase0_sdk_encode(const Phase0SdkInfo info) {
    return ((uint64_t)info.core_abi << 32) | info.sdk_version;
}
