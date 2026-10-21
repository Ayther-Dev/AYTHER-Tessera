#include <stddef.h>
#include <tessera/tessera.h>

_Static_assert(sizeof(void *) == 8, "The installed ABI requires x64.");
_Static_assert(sizeof(tessera_api_info_v1) == 32, "ABI info layout.");
_Static_assert(sizeof(tessera_context_v1) == 64, "Context layout.");
_Static_assert(sizeof(tessera_limits_v1) == 96, "Limits layout.");
_Static_assert(offsetof(tessera_context_v1, frame_id) == 24,
               "Context frame identity offset.");

tessera_status TESSERA_CALL installed_c_query(tessera_api_info_v1 *out,
                                             tessera_context_v1 *context) {
  return tessera_api_query_v1(TESSERA_ABI_MAJOR, TESSERA_ABI_MINOR, out, context);
}
