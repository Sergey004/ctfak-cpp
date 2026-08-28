// Shader list chunks (8771 / 8794).
#pragma once

#include "../core/BinaryReader.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ctfak {

struct ShaderParameter {
    std::string name;
    uint8_t type = 0; // 0 int, 1 float, 2 int_float4, 3 image
};

struct Shader {
    std::string name;
    std::string data;
    int32_t backgroundTexture = 0;
    std::vector<ShaderParameter> parameters;
};

struct Shaders {
    std::vector<Shader> list;                       // index = shader handle
    std::unordered_map<int32_t, size_t> byHandle;   // handle -> index into list

    void read(BinaryReader& reader);
    const Shader* find(int32_t handle) const;
};

} // namespace ctfak
