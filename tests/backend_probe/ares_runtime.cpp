// Gate 0 glue for the pinned ares core; upstream sources remain unchanged.
// The desktop resource bundle is intentionally absent: MD/core nodes do not use it.
#include <ares/ares.hpp>
#include <ares/debug/debug.cpp>
#include <nall/gdb/server.cpp>
#include <ares/node/node.cpp>

namespace ares {

Platform* platform = nullptr;
atomic<bool> _runAhead = false;

const string Name = "ares";
const string Version = "148 (Tessera Gate 0 probe)";
const string Copyright = "ares authors";
const string License = "ISC";
const string LicenseURI = "https://opensource.org/licenses/ISC";
const string Website = "ares-emu.net";
const string WebsiteURI = "https://ares-emu.net/";
const u32 SerializerSignature = 0x31545342;

}

// The upstream MD unity file defines Scheduler after the Thread instances.
// Construct it first here so it remains alive during every Thread destructor.
// These are the same upstream primary sources, included without modifications.
#include <md/system/system.cpp>
#include <md/bus/bus.cpp>
#include <md/cpu/cpu.cpp>
#include <md/apu/apu.cpp>
#include <md/vdp/vdp.cpp>
#include <md/opn2/opn2.cpp>
#include <md/m32x/m32x.cpp>
#include <md/mcd/mcd.cpp>
#include <md/cartridge/cartridge.cpp>
#include <md/controller/controller.cpp>
