// Sound file extension sniffing by magic bytes (order mirrors the documented behaviour).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

std::string sniffSoundExtension(const std::vector<uint8_t>& data);

} // namespace ctfak
