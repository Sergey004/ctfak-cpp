#include "ImageBank.h"
#include "ImageDecoder.h"
#include "../core/LZ4.h"
#include "../core/Settings.h"
#include "../core/Zlib.h"
#include "../core/Logger.h"

namespace ctfak {

void ImageItem::decode() {
    if (!rgba.empty()) return;
    if (pixelData.empty() || width <= 0 || height <= 0) {
        rgba.assign(static_cast<size_t>(width > 0 ? width : 1) * (height > 0 ? height : 1) * 4, 0);
        return;
    }
    switch (graphicMode) {
        case 0: ImageDecoder::androidMode0ToRGBA(pixelData, width, height, rgba); break;
        case 1: ImageDecoder::androidMode1ToRGBA(pixelData, width, height, rgba); break;
        case 2: ImageDecoder::androidMode2ToRGBA(pixelData, width, height, rgba); break;
        case 3: ImageDecoder::androidMode3ToRGBA(pixelData, width, height, rgba); break;
        case 4:
            if (Settings::android())
                ImageDecoder::androidMode4ToRGBA(pixelData, width, height, rgba);
            else
                rgba = ImageDecoder::mode24MaskedToRGBA(pixelData, width, height, flags["Alpha"],
                                                        transparent,
                                                        Settings::f3() && Settings::fusion3Seed);
            break;
        case 5:
            // JPEG payload: not decoded in this implementation; output opaque black.
            rgba.assign(static_cast<size_t>(width) * height * 4, 0);
            break;
        case 6: ImageDecoder::mode15ToRGBA(pixelData, width, height, flags["Alpha"], transparent, rgba); break;
        case 7: ImageDecoder::mode16ToRGBA(pixelData, width, height, flags["Alpha"], transparent, rgba); break;
        case 8:
            rgba = ImageDecoder::mode32ToRGBA(pixelData, width, height, flags["Alpha"], transparent,
                                              flags["RGBA"], Settings::fusion3Seed);
            break;
        default:
            Logger::warning("Unknown graphic mode " + std::to_string(graphicMode) + " for image " +
                            std::to_string(handle));
            rgba.assign(static_cast<size_t>(width) * height * 4, 0);
            break;
    }
}

void ImageBank::read(BinaryReader& reader) {
    if (Settings::hasParameter("noimg")) return;

    int32_t count = 0;
    if (Settings::android()) {
        reader.readI16(); // max handle
        count = reader.readI16();
    } else {
        count = reader.readI32();
    }

    items.clear();
    for (int32_t i = 0; i < count; ++i) {
        auto image = std::make_shared<ImageItem>();
        try {
            if (Settings::android()) {
                // Android image layout.
                image->handle = reader.readI16();
                if (Settings::buildType == 284) image->handle--;
                image->graphicMode = static_cast<uint8_t>(reader.readI32());
                image->width = reader.readI16();
                image->height = reader.readI16();
                image->hotX = reader.readI16();
                image->hotY = reader.readI16();
                image->actionX = reader.readI16();
                image->actionY = reader.readI16();
                int32_t dataSize = reader.readI32();
                image->flags = BitDict({"RLE", "RLEW", "RLET", "LZX", "Alpha", "ACE", "Mac", "RGBA"}, 16);
                image->transparent = {0, 0, 0, 255};
                if (reader.peekU8() == 255)
                    image->pixelData = reader.readBytes(dataSize);
                else
                    image->pixelData = zlibInflate(reader.readBytes(dataSize));
            } else {
                image->handle = reader.readI32();
                if (Settings::twoFivePlus() || Settings::f3()) {
                    image->handle -= 1;
                } else if (Settings::build >= 284) {
                    image->handle -= 1;
                }

                int32_t decompSize = reader.readI32();
                int32_t compSize = reader.readI32();
                std::vector<uint8_t> body = zlibInflate(reader.readBytes(compSize), decompSize);
                BinaryReader r(body.data(), body.size());

                image->checksum = r.readI32();
                image->references = r.readI32();
                if (Settings::twoFivePlus() || Settings::f3()) r.skip(4);
                int32_t dataSize = r.readI32();
                image->width = r.readI16();
                image->height = r.readI16();
                image->graphicMode = r.readU8();
                image->flags.set(r.readU8());
                if (!Settings::old()) r.skip(2);
                image->hotX = r.readI16();
                image->hotY = r.readI16();
                image->actionX = r.readI16();
                image->actionY = r.readI16();
                if (!Settings::old()) image->transparent = r.readColor();
                else image->transparent = {0, 0, 0, 255};

                if (Settings::twoFivePlus() || Settings::f3()) {
                    int32_t lz4Size = r.readI32();
                    auto raw = r.readBytes(dataSize - 4);
                    image->pixelData = lz4DecodeBlock(raw, static_cast<size_t>(lz4Size));
                } else if (image->flags["LZX"]) {
                    r.readI32(); // decompSize prefix
                    image->pixelData = zlibInflate(r.readBytes(-1));
                } else {
                    image->pixelData = r.readBytes(dataSize);
                }
            }
        } catch (const ReadError& e) {
            Logger::warning(std::string("Image item read failed: ") + e.what());
            break;
        } catch (const std::runtime_error& e) {
            Logger::warning(std::string("Image decode storage failed: ") + e.what());
            continue;
        }
        items[image->handle] = image;
        if (reader.eof()) break;
    }
}

std::shared_ptr<ImageItem> ImageBank::find(int32_t handle) const {
    auto it = items.find(handle);
    if (it == items.end()) return nullptr;
    return it->second;
}

} // namespace ctfak
