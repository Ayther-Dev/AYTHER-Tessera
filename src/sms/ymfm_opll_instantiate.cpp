// Explicit instantiation of the YM2413 FM engine (TP-601.3), for the same
// reason as src/md/ymfm_instantiate.cpp. sms/ym2413.cpp calls
// `fm_engine_base<opll_registers>::clock` and `::output` directly, to mute
// channels one by one. ymfm_opl.cpp only instantiates them implicitly, and once
// GCC inlines every use inside that unit it emits no out-of-line copy, so the
// link fails on Linux. ymfm_fm.ipp holds only templates and inline functions,
// so including it here duplicates nothing. The third-party source stays
// untouched.
#include "ymfm_opl.h"

#include "ymfm_fm.ipp"

namespace ymfm {
template class fm_engine_base<opll_registers>;
} // namespace ymfm
