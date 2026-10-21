#include "synth_rom_fx03.h"

#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: tessera_fixture_roms OUTPUT_DIRECTORY\n";
        return 2;
    }
    const std::filesystem::path output = argv[1];
    std::error_code error;
    std::filesystem::create_directories(output, error);
    if (error) return 1;

    ayther::synth::Rom canonical("AYTHER SYNTHETIC ROM");
    ayther::synth::program_canonical(canonical, true);
    if (!canonical.save(output / "ayther_synthetic.md")) return 1;

    ayther::synth::Rom graphics("AYTHER FX03 SYNTH ROM");
    ayther::synth::program_canonical(graphics, true, true);
    if (!graphics.save(output / "fx03_rom.md")) return 1;
    return 0;
}
