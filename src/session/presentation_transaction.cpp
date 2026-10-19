#include "api/call.hpp"
#include "session/session.hpp"
namespace tessera {
using namespace wire;
void Session::cancel(tessera_context_v1 &ctx, std::uint32_t reason) {
  if (pending) {
    ctx.frame_id = {handle, pending->epoch, pending->frame};
    ctx.transaction = pending->id;
    ctx.flags = TESSERA_CONTEXT_CONTROL_CANCELLED;
    ctx.cancel_reason = reason;
    pending.reset();
  }
}
} // namespace tessera
