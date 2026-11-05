// Explicit instantiation of the YM2612 FM engine (TP-101.11). ym.cpp calls
// `fm_engine_base::output` directly for the per-channel mute; GCC inlines it
// inside ymfm_opn.cpp and emits no out-of-line copy, so the linker finds none.
#include "ymfm_opn.h"
#include "ymfm_fm.ipp"

namespace ymfm {
template class fm_engine_base<opn_registers_base<true>>;
} // namespace ymfm
