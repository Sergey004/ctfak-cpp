#include "ChunkNames.h"
#include <unordered_map>

namespace ctfak {

const char* chunkName(int32_t id) {
    switch (id) {
        case 4386: return "Preview";
        case 8738: return "Mini-Header";
        case 8739: return "Header";
        case 8740: return "Name";
        case 8741: return "Author";
        case 8742: return "Menu";
        case 8743: return "Extension Path";
        case 8744: return "Extensions";
        case 8745: return "Frame Items";
        case 8746: return "Global Events";
        case 8747: return "Frame Handles";
        case 8748: return "Extension Data";
        case 8749: return "Additional Extension";
        case 8750: return "App Editor-Filename";
        case 8751: return "App Target-Filename";
        case 8752: return "App Docs";
        case 8753: return "Other Extensions";
        case 8754: return "Global Values";
        case 8755: return "Global Strings";
        case 8756: return "Extension List";
        case 8757: return "Icon 16x16";
        case 8758: return "Is Demo";
        case 8759: return "Serial Number";
        case 8760: return "Binary Files";
        case 8761: return "Menu Images";
        case 8762: return "About Text";
        case 8763: return "Copyright";
        case 8764: return "Global Value Names";
        case 8765: return "Global String Names";
        case 8766: return "Movement Extensions";
        case 8767: return "Frame Items 2";
        case 8768: return "Exe Only";
        case 8770: return "Protection";
        case 8771: return "Shaders";
        case 8773: return "Extended Header";
        case 8774: return "Spacer";
        case 8775: return "Frame Offset";
        case 8776: return "Ad Mob ID";
        case 8779: return "Android Menu";
        case 8787: return "2.5+ Object Headers";
        case 8788: return "2.5+ Object Names";
        case 8789: return "2.5+ Object Shaders";
        case 8790: return "2.5+ Object Properties";
        case 8792: return "Font Info";
        case 8793: return "Fonts";
        case 8794: return "Shaders";
        case 13107: return "Frame";
        case 13108: return "Frame Header";
        case 13109: return "Frame Name";
        case 13110: return "Frame Password";
        case 13111: return "Frame Palette";
        case 13112: return "Frame Item Instances";
        case 13113: return "Frame Fade In Frame";
        case 13114: return "Frame Fade Out Frame";
        case 13115: return "Frame Fade In";
        case 13116: return "Frame Fade Out";
        case 13117: return "Frame Events";
        case 13118: return "Frame Play Header";
        case 13119: return "Additional Frame Item";
        case 13120: return "Additional Frame Item Instance";
        case 13121: return "Frame Layers";
        case 13122: return "Frame Virtual Rect";
        case 13123: return "Demo File Path";
        case 13124: return "Random Seed";
        case 13125: return "Frame Layer Effects";
        case 13126: return "Blu-Ray Frame Options";
        case 13127: return "Mvt Timer Base";
        case 13128: return "Mosiac Image Table";
        case 13129: return "Frame Effects";
        case 13130: return "Frame iPhone Options";
        case 17476: return "Object Info Header";
        case 17477: return "Object Info Name";
        case 17478: return "Object Common";
        case 17479: return "Unknown Object Chunk";
        case 17480: return "Object Effects";
        case 17664: return "Object Shapes";
        case 21845: return "Image Handles";
        case 21846: return "Font Handles";
        case 21847: return "Sound Handles";
        case 21848: return "Music Handles";
        case 26213: return "Bank Offsets";
        case 26214: return "Images";
        case 26215: return "Fonts";
        case 26216: return "Sounds";
        case 26217: return "Music";
        case 32494: return "Fusion 3 Seed";
        case 32639: return "Last Chunk";
        default: return nullptr;
    }
}

} // namespace ctfak
