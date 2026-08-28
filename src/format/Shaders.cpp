#include "Shaders.h"
#include "../core/Logger.h"

namespace ctfak {

void Shaders::read(BinaryReader& reader) {
    int64_t base = reader.tell();
    int32_t count = reader.readI32();
    std::vector<int32_t> offsets(static_cast<size_t>(count), 0);
    try {
        for (int32_t i = 0; i < count; ++i) offsets[i] = reader.readI32();
    } catch (const ReadError&) {
        return;
    }

    list.clear();
    for (int32_t i = 0; i < count; ++i) {
        if (offsets[i] == 0) continue;
        try {
            reader.seek(base + offsets[i]);
            int32_t nameOffset = reader.readI32();
            int32_t dataOffset = reader.readI32();
            int32_t parameterOffset = reader.readI32();
            int32_t backgroundTexture = reader.readI32();

            Shader shader;
            shader.backgroundTexture = backgroundTexture;
            int64_t shaderStart = base + offsets[i];
            reader.seek(shaderStart + nameOffset);
            shader.name = reader.readAscii();
            reader.seek(shaderStart + dataOffset);
            shader.data = reader.readAscii();

            if (parameterOffset != 0) {
                int64_t p = shaderStart + parameterOffset;
                reader.seek(p);
                int32_t paramCount = reader.readI32();
                int32_t typeOffset = reader.readI32();
                int32_t namesOffset = reader.readI32();
                shader.parameters.resize(paramCount);
                reader.seek(p + typeOffset);
                for (int32_t k = 0; k < paramCount; ++k)
                    shader.parameters[k].type = reader.readU8();
                reader.seek(p + namesOffset);
                for (int32_t k = 0; k < paramCount; ++k)
                    shader.parameters[k].name = reader.readAscii();
            }
            byHandle[i] = list.size();
            list.push_back(std::move(shader));
        } catch (const ReadError&) {
            Logger::warning("Invalid Shader.");
        }
    }
}

const Shader* Shaders::find(int32_t handle) const {
    auto it = byHandle.find(handle);
    if (it == byHandle.end()) return nullptr;
    return &list[it->second];
}

} // namespace ctfak
