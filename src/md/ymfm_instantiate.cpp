// Explicit instantiation of the YM2612 FM engine (TP-101.11, TP-208.1). ym.cpp
// calls `fm_engine_base::output` and the OPN register set directly; when the
// optimizer inlines every use inside ymfm_opn.cpp (GCC, or clang-cl with
// /Ob2) it emits no out-of-line copy and the linker finds none. ymfm_opn.cpp
// is compiled here, so the definitions are in this unit, and the two class
// templates are instantiated whole. The third-party source stays untouched.
#include "ymfm_opn.cpp"

namespace ymfm {
template class opn_registers_base<true>;
template class fm_engine_base<opn_registers_base<true>>;
} // namespace ymfm
