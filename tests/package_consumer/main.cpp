#include <tessera/tessera.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

extern "C" tessera_status TESSERA_CALL
installed_c_query(tessera_api_info_v1 *, tessera_context_v1 *);

static_assert(sizeof(tessera_context_v1) == 64);
static_assert(alignof(tessera_context_v1) == 8);
static_assert(sizeof(tessera_original_frame_v1) == 120);
static_assert(offsetof(tessera_original_frame_v1, pixels) == 56);
static_assert(std::is_standard_layout_v<tessera_backend_info_v1>);

template <class T> T initialized() {
  T value{};
  value.header = {sizeof(T), TESSERA_ABI_MAJOR, TESSERA_ABI_MINOR};
  return value;
}

int main() {
  auto context = initialized<tessera_context_v1>();
  auto info = initialized<tessera_api_info_v1>();
  if (installed_c_query(&info, &context) != TESSERA_OK || info.major != 1 ||
      info.minor != 0 || info.pointer_bytes != 8 || info.max_alignment != 8)
    return 1;
  auto limits = initialized<tessera_limits_v1>();
  if (tessera_limits_query_v1(0, &limits, &context) != TESSERA_OK ||
      limits.content_bytes != UINT64_C(67108864) || limits.retained_count != 8)
    return 2;
  std::uint32_t required = 0;
  if (tessera_backends_enumerate_v1(nullptr, 0, &required, &context) !=
          TESSERA_BUFFER_TOO_SMALL ||
      required != 3)
    return 3;
  std::array<tessera_backend_info_v1, 3> backends{};
  for (auto &backend : backends)
    backend = initialized<tessera_backend_info_v1>();
  if (tessera_backends_enumerate_v1(backends.data(), 3, &required, &context) !=
          TESSERA_OK ||
      std::strcmp(backends[0].id, "clownmdemu") != 0 ||
      std::strcmp(backends[1].id, "synthetic.minimal") != 0 ||
      std::strcmp(backends[2].id, "synthetic.complete") != 0 ||
      backends[0].kind != TESSERA_BACKEND_PRODUCT ||
      backends[1].kind != TESSERA_BACKEND_TEST ||
      backends[2].kind != TESSERA_BACKEND_TEST)
    return 4;
  if (tessera_api_query_v1(2, 0, &info, &context) != TESSERA_ABI_INCOMPATIBLE)
    return 5;
  std::array<std::uint8_t, 256> message{};
  std::uint64_t bytes = 0;
  if (tessera_status_format_v1(&context, {message.data(), message.size()},
                               &bytes) != TESSERA_OK ||
      bytes == 0 || bytes >= message.size())
    return 6;
  std::puts("Paquete instalado: ABI C11/C++20, consultas y ownership del caller verificados.");
  return 0;
}
