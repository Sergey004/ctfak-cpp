#include "ChunkTrace.h"
#include "../format/Chunk.h"
#include "../format/ChunkNames.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"

namespace ctfak {

void ChunkTraceTool::execute(GameData& game) {
    if (game.sourcePath.empty()) {
        Logger::error("Chunk Trace needs a source file (load a game first).");
        return;
    }
    std::vector<uint8_t> file;
    if (!readFileBytes(game.sourcePath, file)) {
        Logger::error("Cannot open source file: " + game.sourcePath);
        return;
    }

    std::string dir = pathJoin(pathJoin(g_dumpsDir, "CHUNK_TRACE"), fileStem(game.sourcePath));
    createDirectories(dir);

    BinaryReader reader(file.data(), file.size());
    reader.seek(game.dataOffset);

    std::string magic = reader.readAscii(4);
    reader.skip(12); // runtimeVersion/SubVersion/productVersion/build

    int index = 0;
    Logger::log("Tracing chunks from: " + game.sourcePath);
    while (!reader.eof()) {
        Chunk chunk;
        std::vector<uint8_t> data;
        try {
            data = chunk.read(reader);
        } catch (const ReadError&) {
            break;
        }
        const char* cname = chunkName(chunk.id);
        std::string base = std::string(cname ? cname : ("UNKNOWN-" + std::to_string(chunk.id))) + "-" +
                           std::to_string(++index) + ".bin";
        writeFileBytes(pathJoin(dir, base), data);
        Logger::log("Encountered chunk: " + std::string(cname ? cname : "UNKNOWN") + ", flag: " +
                    std::to_string(chunk.flag) + ", exe size: " + std::to_string(chunk.size) +
                    ", decompressed size: " + std::to_string(data.size()));
        if (chunk.id == 32639) break;
    }
    Logger::log("Chunk trace written to " + dir);
}

} // namespace ctfak
