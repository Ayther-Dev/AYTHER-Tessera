#ifndef TESSERA_PHASE0_ENGINE_POD_H
#define TESSERA_PHASE0_ENGINE_POD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Phase0SdkInfo {
    uint32_t sdk_version;
    uint32_t core_abi;
} Phase0SdkInfo;
uint64_t __cdecl phase0_sdk_encode(Phase0SdkInfo info);
#ifdef __cplusplus
}
#endif
#endif
