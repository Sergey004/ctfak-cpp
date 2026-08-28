# Clickteam Fusion file format notes (implementation reference)

This document records the byte-level facts of the Clickteam Fusion 2.5 / MMF 1.5 / Fusion 3
runtime file formats as implemented by this project. It was compiled by studying publicly
available reverse-engineering references and independent format documentation. No source code
was copied: this file is the interface between the "study" phase and the "implementation" phase
of the clean-room process.

All integers are little-endian unless stated otherwise. `u8/i8/u16/i16/u32/i32/f32/f64`.

## 1. Containers

### 1.1 Windows EXE (.exe)
1. PE parsing: `MZ` at 0; `u16 e_lfanew` at offset 60; at `e_lfanew` read 2 (`"PE"`), skip 4
   (`\0\0` + Machine), read `u16 numberOfSections`, skip 16, skip 28+68 (optional header),
   skip 16*8 (data dirs). NOTE: the section count lives at `e_lfanew + 6`; skipping only the
   4-byte PE signature reads `Machine` (0x014C) instead and derails the whole scan.
2. For each 40-byte section header: if name == ".extra" -> game data starts at
   `u32 PointerToRawData` (at header offset +20). If last section -> game data starts at
   `PointerToRawData + SizeOfRawData` (header offsets +16, +20).
3. At game data start: `u16` == 0x7777 -> normal CF2.5; anything else -> MMF 1.5 (Settings.old).
4. Pack header (32 bytes) at game data start:
   `u16[4] ignored (0x7777 ...)`, `u32 headerSize(==32)`, `u32 dataSize`, `u32 formatVersion`,
   `i32 check`, `i32 check2`, `u32 fileCount`.
   Magic check: 4 ASCII chars at `start + dataSize - 32`: "PAMU" -> unicode CF2.5,
   "PAME" -> MMF2 (non-unicode). 
5. Pack file records at start+32, one per file:
   `u16 nameLen`, name (`nameLen` UTF-16 chars if unicode else `nameLen` ASCII bytes),
   `i32 unused`, `i32 size`, then payload: if first 2 bytes are `78 DA` -> zlib stream,
   else raw `size` bytes.
6. After the records the cursor is at `start + dataSize`; the CCN chunk stream follows.
7. MMF 1.5 exes: no pack header; chunk stream begins right after the PE (first `u16` != 0x7777),
   `Settings.unicode = false`.

### 1.2 CCN (.ccn/.dat/.bin)
- If first `u32` == 0x77777777 -> skip 32 bytes, then chunk stream.
- Otherwise chunk stream starts at 0.

### 1.3 APK (.apk)
ZIP container: `application.ccn` (or similar) is the game; `s*.ogg/mp3/wav` in the root are
sounds (handle = number after leading 's'); optional launcher icon. Requires a minimal ZIP reader.

## 2. Chunk stream

Header: `i16 id`, `i16 flag`, `i32 size`, then `size` bytes of payload.
Flags: 0 = raw, 1 = compressed, 2 = encrypted, 3 = compressed + encrypted.

- flag 1 (modern): `i32 decompSize`, `i32 compSize`, zlib stream of `compSize` bytes.
- flag 1 (old/MMF 1.5): if next `i32` != -1 it is `decompSize` (consume), else no size prefix;
  zlib stream follows, consume whatever inflate used.
- flag 2: payload is decrypted in place with the state transform (3.2).
- flag 3: `u32 decompSize`; then if `(chunkId & 1) == 1 && build > 284`:
  `data[0] ^= (u8)((chunkId & 0xFF) ^ (chunkId >> 8))`; state transform; then `u32 compSize`,
  zlib stream.

File magic at the very beginning of the stream: "PAMU" (unicode), "PAME" (ANSI), "CRUF"
(Fusion 3). Then `u16 runtimeVersion`, `u16 runtimeSubversion`, `i32 productVersion`,
`i32 productBuild`. Chunk 0x7F7F (32639) terminates the top-level stream.

## 3. State encryption (CF 2.5)

### 3.1 Key derivation
`magicChar = 54`. Inputs: app `name`, `copyright`, `editorFilename` strings.
String -> bytes: for each UTF-16 char emit low byte if != 0, then high byte if != 0.
`MakeKey(name, copyright, editorFilename)` if build > 284, else
`MakeKey(editorFilename, name, copyright)` (called when chunk 0x222E "Editor Filename" is read).

`MakeKeyCombined(data)`: resize buffer to 256 (zero fill); `v = magicChar`;
`for i in 0..len(data)` inclusive: `v = rotl8(v, 1)` wait — exact: `v = (v << 7) + (v >> 1)`
(rotl by 1 in 8 bits); `data[i] ^= v`; `lastKey += data[i] * ((v & 1) + 2)` (all u8 arithmetic);
finally `data[len + 1] = lastKey`.

### 3.2 KSA (table init)
`table[i] = i` for 256 entries; `hash = accum = magicChar`; `neverReset = true`; `key = 0`;
`for i in 0..255`:
```
hash = rotl8(hash, 1)
if (neverReset) { accum += (hash & 1) == 0 ? 2 : 3; accum *= keyData[key]; }   // u8
if (hash == keyData[key]) { hash = rotl8(magicChar, 1); key = 0; neverReset = false; }
i2 += (hash ^ keyData[key]) + table[i]                                          // u8
swap(table[i], table[i2])
key++   // for-increment
```

### 3.3 PRGA (transform)
Copy of `table`; `i = i2 = 0`; per byte: `++i; i2 += buf[i]; swap(buf[i], buf[i2]);
data[j] ^= buf[(u8)(buf[i] + buf[i2])]`.

## 4. Top-level chunk ids (decimal, with names)

4386 Preview, 8738 Mini-Header, 8739 Header, 8740 Name, 8741 Author, 8742 Menu,
8743 Extension Path, 8744 Extensions, 8745 Frame Items, 8746 Global Events, 8747 Frame Handles,
8748 Extension Data, 8749 Additional Extension, 8750 App Editor-Filename, 8751 App Target-Filename,
8752 App Docs, 8753 Other Extensions, 8754 Global Values, 8755 Global Strings, 8756 Extension List,
8757 Icon 16x16, 8758 Is Demo, 8759 Serial Number, 8760 Binary Files, 8761 Menu Images,
8762 About Text, 8763 Copyright, 8764 Global Value Names, 8765 Global String Names,
8766 Movement Extensions, 8767 Frame Items 2, 8768 Exe Only, 8770 Protection, 8771 Shaders,
8773 Extended Header, 8774 Spacer, 8775 Frame Offset, 8776 Ad Mob ID, 8779 Android Menu,
8787 2.5+ Object Headers, 8788 2.5+ Object Names, 8789 2.5+ Object Shaders,
8790 2.5+ Object Properties, 8792 Font Info, 8793 Fonts, 8794 Shaders,
13107 Frame, 13108 Frame Header, 13109 Frame Name, 13110 Frame Password, 13111 Frame Palette,
13112 Frame Item Instances, 13113 Frame Fade In Frame, 13114 Frame Fade Out Frame,
13115 Frame Fade In, 13116 Frame Fade Out, 13117 Frame Events, 13118 Frame Play Header,
13119 Additional Frame Item, 13120 Additional Frame Item Instance, 13121 Frame Layers,
13122 Frame Virtual Rect, 13123 Demo File Path, 13124 Random Seed, 13125 Frame Layer Effects,
13126 Blu-Ray Frame Options, 13127 Mvt Timer Base, 13128 Mosiac Image Table, 13129 Frame Effects,
13130 Frame iPhone Options, 17476 Object Info Header, 17477 Object Info Name,
17478 Object Common, 17479 Unknown Object Chunk, 17480 Object Effects, 17664 Object Shapes,
21845 Image Handles, 21846 Font Handles, 21847 Sound Handles, 21848 Music Handles,
26213 Bank Offsets, 26214 Images, 26215 Fonts, 26216 Sounds, 26217 Music,
32494 Fusion 3 Seed, 32639 Last Chunk.

## 5. Key structures

### 5.1 App header (8739)
modern: `i32 size`, `i16 flags`, `i16 newFlags`, `i16 graphicsMode`, `i16 otherFlags`,
`i16 windowWidth`, `i16 windowHeight`, `u32 initialScore ^ 0xFFFFFFFF`, `u32 initialLives ^ 0xFFFFFFFF`,
4 player controls (each: `i16 controlType` + 8 x `i16` keys), color border (r,g,b,a),
`i32 numberOfFrames`, `i32 frameRate`, `i32 windowsMenuIndex`.
old (1.5): no size field; skip 56 instead of controls; stop after numberOfFrames.
OtherFlags bits 14/15 = Direct3D9or11 / Direct3D8or11 (used by blend fixups).

### 5.2 Extended header (8773), 20 bytes
`i32 flags`, `u8 buildType`, skip 3, `i32 compressionFlags`, `i16 screenRatio`,
`i16 screenAngle`, `i16 viewFlags`, `i16 newFlags`.
buildType 12/34 -> android, 13/14/15 -> iOS, 74/75/78 -> Fusion 3.

### 5.3 Frame (13107) — nested chunk stream
13108 header: modern `i32 width, i32 height, color, u32 flags`; old: `i16, i16, color, u16`.
13109 name (Yuniversal). 13111 palette: 257 colors. 13112 instances:
`i32 count` + per instance: `u16 handle, u16 objectInfo`, then old: `i16 y, i16 x` else
`i32 x, i32 y`, `i16 parentType, i16 parentHandle`, and if !old && !F3: `i16 layer, i16 instance`.
13115/13116 transitions. 13117 events. 13121 layers: `u32 count` + per layer:
`u32 flags, f32 xCoeff, f32 yCoeff, i32 numberOfBackgrounds, i32 backgroundIndex`,
(+6 skip if Fusion3Seed), Yuniversal name.
13122 virtual rect: 4 x i32. 13124 random seed i16. 13125 layer effects; 13127 movement timer.
13129 frame effects: `i16 effect, i16 effectParam, color rgbCoeff, i32 inkEffect, i32 numParams`
(+ params per shader definition), etc.

### 5.4 Object bank (8745 / 8767)
`i32 count` + count entries. Each entry is a nested chunk stream:
17476 header: `i16 handle, i16 objectType, i16 flags, skip2, u8 inkEffect`;
if inkEffect != 1: `skip3, u8 r, u8 g, u8 b, u8 blend` (rgbCoeff stored swapped:
R=b, G=g, B=r) else `u8 flag, skip2, u8 inkEffectValue, skip3`.
Old/MMF2: rgbCoeff forced white, blend 255.
17477 name (Yuniversal). 17478 properties (see 5.5). 17480 object effects (shader params).
32639 terminates the entry.
2.5+ chunk 8787 stores plain headers (same layout as 17476 without nesting);
8788 names (Yuniversal per object index); 8789 shaders; 8790 properties: `i32 ignored`,
then per object: `i32 size`, zlib block, decompressed -> ObjectCommon by object type,
advance by size+8 from position before size.

### 5.5 ObjectCommon (17478 payload)
Header variants by game type (offsets are i16, relative to payload start; sub-loaders are
seeked to `base + offset`, base = payload start except android build>=290 where base = 0):
- modern CF2.5 (NORMAL, build >= 284): probe `i32 size` at +0, skip 2, `i32 check` at +6,
  seek back to +4. If build == 284 && check == 0:
  `i16 counter@+4, i32 version@+6, i16 movements@+10, i16 extension@+12, i16 animations@+14`;
  else `i16 animations@+4, i16 movements@+6, i32 version@+8, i16 extension@+12, i16 counter@+14`.
- NORMAL (build < 284) / MMF2: `i32 size, i16 movements, i16 animations, i16 version,
  i16 counter, i16 systemObj, skip2`.
- 2.5+ / F3: `i32 size, i16 animations, i16 movements, u16 version, skip2, i16 extension, i16 counter`
  (+ bail-out if fewer than 20 bytes remain after qualifiers).
Common tail (unless noted): `u16 flags, i16 unknown (== 6 -> DoNotCreateAtStart), 8 x i16 qualifiers,
i16 systemObjOffset, i16 valuesOffset, i16 stringsOffset, u16 newFlags, u16 preferences,
ASCII identifier (4 bytes; 2 bytes in MMF2 path), color backColor, u32 fadeIn, u32 fadeOut`.
Sub-loader order: animations, alterable values, alterable strings, movements, system object
(by identifier: "XT\xFF\xFF"/"TE"/"TEXT" -> text, "TR\xFF\xFF"/"CNTR"/"LIVE"/"CN" -> counter,
"CCA " -> sub-application), extension (`i32 totalSize; skip 4; i32 version, i32 id, i32 private;
data = totalSize - 20`), counter (`i16 size, i32 initial, i32 min, i32 max`).

Movements: `u32 count` + 16-byte stride table; each entry: `i32 nameOffset, i32 movementId,
i32 newOffset, i32 dataSize`; data at `tableStart + newOffset`:
`u16 player, u16 type, u8 movingAtStart, skip1(u8 moving)+2, i32 directionAtStart`;
types: 1 mouse (10 bytes), 2 race (14), 3 eight-directions (12, last is i32), 4 ball (10),
5 path (i16 count, i16 minSpeed, i16 maxSpeed, u8 loop, u8 repositionAtEnd, u8 reverseAtEnd,
skip 1, then 16-byte steps: u8 flags, u8 size, u8 speed, u8 direction, i16 x, i16 y, i16 cos,
i16 sin, i16 length, i16 pause, tail size-16), 9 platform (12), 14 extension data.
Old path: single inline movement, no table.

Animations: `i16 size(discard), i16 count, count x i16 offsets` (relative to animations start);
each animation: 32 x `i16` direction offsets (relative to animation start); each direction:
`i8 minSpeed, i8 maxSpeed, i16 repeat, i16 backTo, u16 frameCount, frameCount x i16 handles`.

Alterable values: `i16 count, count x i32, i32 flags(best-effort)`.
Alterable strings: `i16 count, count x UTF-16 NUL-terminated`.

Quick backdrop (type 0): `i32 size, i16 obstacle, i16 collision, i32 width, i32 height` + shape.
Backdrop (type 1): `i32 size, i16 obstacle, i16 collision`, if !old `i32 width, i32 height`, `i16 image`.
Shape: `i16 borderSize, color borderColor, i16 shapeType, i16 fillType`, conditional fields
(shapeType==1 -> i16 lineFlags; fillType==1 -> color; fillType==2 -> 2 colors + i16 gradFlags),
then always `i16 image`.

Text (system): `i32 size, i32 width, i32 height, i32 count, count x i32 offsets`;
paragraph: `u16 font, u16 flags, color, Yuniversal value`. Old: i16 sizes/offsets and
`u16 size, u16 font, color, u16 flags, string`.

### 5.6 Image bank (26214)
`i32 count` (android: `i16 maxHandle, i16 count`), then items sequentially.
Item (modern non-android): `i32 handle (-1 if build>=284), i32 decompSize, i32 compSize, zlib`.
2.5+ item: handle read, then -1 unconditionally. Decompressed item body:
`i32 checksum (i16 if old), i32 references, [skip 4 if 2.5+], i32 dataSize, i16 width, i16 height,
u8 graphicMode, u8 flags, [skip i16 if !old], i16 hotX, i16 hotY, i16 actionX, i16 actionY,
color transparent (not for old)`.
Pixel payload: 2.5+ -> `i32 lz4Size` + LZ4 block (`dataSize - 4` bytes) -> lz4Size bytes;
else if flags bit 3 (LZX) -> `i32 decompSize` + zlib to end of buffer; else raw `dataSize` bytes.
Old (1.5): item = `i32 handle, i32 decompSize, raw rest`, zlib(tinf) whole remainder; body:
`i16 checksum, i32 references, i32 dataSize, i16 w, i16 h, u8 mode, u8 flags, i16 hotX, i16 hotY,
i16 actX, i16 actY` (no transparent color).

Android item variant: `i16 handle, i32 graphicMode, i16 w, i16 h, i16 hotX, i16 hotY, ...`.
QUIRK: the reference implementation contains a dead check here — it compares the
extended-header *build type byte* (a small enum: 0 = EXE, 12 = Android, 74/75/78 = console)
against 284, which is actually a *product build number*. Since a byte can never equal 284,
the guarded handle adjustment never executes. This implementation preserves that observed
runtime behavior exactly (`Settings::buildType == 284` is kept but can never fire); the
extended-header build type byte must NOT be conflated with the product build number from
the application header.

Flags bits: 0 RLE, 1 RLEW, 2 RLET, 3 LZX, 4 Alpha, 5 ACE, 6 Mac, 7 RGBA.

Graphic modes: 0 android RGBA8888, 1 android RGBA4444, 2 android 5551-ish, 3 android RGB888,
4 normal 24-bit (+ optional 1-byte alpha plane), 5 android JPEG, 6 15bpp, 7 16bpp (565),
8 2.5+ 32-bit BGRA (+ optional alpha plane or color-key).

Row padding: `pad(width, pointSize, bytes=2) = bytes - (width*pointSize) % bytes`;
if == bytes -> 0 else `ceil(pad / pointSize)` (in pixels). Modular variant (android mode 3):
`(bytes - width*pointSize % bytes) % bytes` raw bytes.
- mode 4: 3 bytes/px + `pad(width,3)` skipped pixels per row * 3 bytes; alpha plane afterwards with
  `aPad = (4 - width % 4) % 4` bytes per row; color-key when no alpha: compare first output byte
  against transparent.R, second against transparent.G, third against transparent.B.
- mode 8: 4 bytes/px (B,G,R,A order in file), skip `pad(width,4)*4` bytes per row (pad==1 always);
  alpha taken from 4th byte when flags alpha or RGBA; else color-key using bytes 2,1,0 vs
  transparent.R,G,B (file order quirk); trailing 1-byte alpha plane when alpha && !RGBA and
  bytes remain.
- mode 6: 15bpp: r = (v & 31744) >> 10 << 3, g = (v & 992) >> 5 << 3, b = (v & 31) << 3.
- mode 7: 16bpp 565: r = (v & 63488) >> 11 << 3, g = (v & 2016) >> 5 << 2, b = (v & 31) << 3.
Output buffer byte order in the reference is B,G,R,A in memory; PNG writers map accordingly.

### 5.7 Sound bank (26216)
`i32 count`, items (skipped entirely for old/F3/android non-MFA builds). Item:
`u32 handle (file value minus 1), i32 checksum, u32 references, i32 decompSize, u8 flags, skip3,
i32 reserved, i32 nameLen`; if bank compressed (default true) && flags != 33:
`i32 size` + zlib block, else raw `decompSize` bytes. Inside: `nameLen` UTF-16 chars = name
(trimmed), rest = sound data (from offset 0 when flags == 33).
Extension sniffing: RIFF -> .wav, OggS -> .ogg, FORM -> .aiff, ID3 -> .mp3, IMPM -> .it,
"Extended Module: " -> .xm, offset 0x438 in {"M.K.","M!K!",xCHN,xCH,FLT4,OCTA...} -> .mod,
0x2C "SCRM" -> .s3m, default .wav; <4 or <0x2C bytes -> .bin.

### 5.8 Fonts (26215) / TTF (8793) / Music (26217)
Font bank: `i32 count`, per item `u32 handle` then zlib block (`i32 decompSize, i32 compSize,
zlib`); inside: `i32 checksum, i32 references, i32 size`, LOGFONTW
(`i32 height,width,escapement,orientation,weight`, 8 bytes italic/underline/strikeout/charSet/
outPrecision/clipPrecision/quality/pitchAndFamily, 32 UTF-16 faceName chars).
TTF chunk: sequence of zlib blocks, each a .ttf file.
Music: `i32 count`, per item `i32 handle` + zlib block; inside `i32 checksum, i32 references,
u32 size, u32 flags, i32 reserved, i32 nameLen, nameLen wide chars, data`.

### 5.9 Other chunks
- Extensions (8744): `u16 count, u16 preload`, entries: `i16 size(abs), i16 handle, i32 magic,
  i32 versionLs, i32 versionMs, Yuniversal name (NUL-term)`, if name contains '.':
  Yuniversal subType; seek to entry start + size.
- Global values (8754): `i16 count, count x i32 raw, count x u8 type` (2 = float, 0 = int).
- Global strings (8755): `i32 count, count x UTF-16 NUL-terminated`.
- Shaders (8771/8794): `i32 count, count x i32 offsets`; shader: `i32 nameOffset, i32 dataOffset,
  i32 paramOffset, i32 backgroundTexture`; ASCII NUL-term name/data; params block:
  `i32 paramCount, i32 typeOffset, i32 namesOffset` (relative to param block), u8 types,
  ASCII NUL-term names.
- Binary files (8760): `i32 count`; per file `i16 nameLen, Yuniversal name (fixed len),
  i32 dataLen, bytes`.
- FrameHandles (8747): array of i16.
- ChunkOffsets (26213): 4 x i32.
- ExeOnly (8768): u8. AppIcon (8757): 1316 bytes (BGRA palette etc.).
- Menu (8742): `u32 headerSize(20), i32 menuOffset, i32 menuSize, i32 accelOffset, i32 accelSize`,
  menu items recursive (`i16 flags`, if no popup bit `i16 id`, UTF-16 name; popup bit 0x10,
  end bit 0x80); accel entries 8 bytes.
- Transition (13115/13116): `ASCII4 module, ASCII4 name, i32 duration, i32 flags, color,
  i32 nameOffset, i32 paramOffset, i32 paramSize` + pointed blocks.
- Image shapes (17664): `i32 count`, per shape `i32 image, i32 pointCount, points (2 x i32)`.
- ExtData (8748): ASCII NUL-term filename + rest.

## 6. Events (13117)

Sections identified by 4 ASCII tags:
- "ER>>": `i16 maxObjects, i16 maxObjectInfo, i16 numberOfPlayers, 17 x i16 conditionsPerObject,
  i16 qualifierCount, qualifierCount x (u16 objectInfo, i16 type)`.
- "ERes": (android: skip 4) `i32 size` (ignored).
- "ERev": `i32 size` (android: size += 4, read i32); EventGroups until `start + size`.
- "ERop": `i32 optionFlags`.
- "<<ER": end.

### 6.1 EventGroup
`i16 size` (stored NEGATIVE; after conditions/actions seek back to `start - size`),
`u8 numConditions, u8 numActions, u16 flags`; build >= 284: `i16 line, i32 isRestricted,
i32 restrictCpt`; else `i16 isRestricted, i16 restrictCpt, i16 identifier, i16 undo`.

### 6.2 Condition (size-prefixed: `u16 size` covers everything)
`i16 objectType, i16 num, u16 objectInfo, i16 objectInfoList, i8 flags, i8 otherFlags,
u8 numParameters, u8 defType, i16 identifier` + numParameters x Parameter.

### 6.3 Action
Same as condition but no identifier field.

Fixups: condition num -42/-43 -> -27 (when objectType != -1); system (-1) nums -28..-33 -> -8.
Action nums 27..30 -> 3, 35/32/33/34 -> 4, 31/36/37/38 -> 5 (when objectType == -1).
Action num == 2 with exactly 2 parameters splits into X (2) / Y (3) actions.
Condition num -25 or -41 with a MultipleVariables parameter expands into flag/value conditions
(see reference behaviour; identifiers offset by a running counter).

### 6.4 Parameter
`i16 size` (from size field start), `i16 code`, loader by code:
1 ParamObject(`i16 list, u16 info, i16 type`), 2/42 Time(`i32 timer, i32 loops, i16 comp`),
3/4/10/11/12/17/26/31/43/57/58/60/61 Short(`i16`), 5/25/29/34/48/56/67/70 IntParam(`i32`),
6/7/35/36 Sample(`i16 handle, u16 flags, Yuniversal name`), 9/21 Create(Position + `u16 instances,
u16 info`), 13 Every(`i32 delay, i32 counter`), 14/44 Key(`u16`), 15/22/23/27/28/45/46/52/53/54/59/62
ExpressionParameter(`i16 comparsion`, expressions, `i32 0` terminator), 16 Position(22 bytes:
`u16 parentInfo, i16 flags, i16 x, i16 y, i16 slope, i16 angle, i32 direction, i16 typeParent,
i16 objectInfoList, i16 layer`), 18 Shoot(Position + `u16 instance, u16 info, skip4, i16 speed`),
19 Zone(`4 x i16`), 24 Colour(4 bytes R,G,B,pad), 32 Click(`u8 button, u8 isDouble`), 33 Program
(`i16 flags, ASCII[260] filename, ASCII NUL-term command`), 38 Group(`u16 flags, u16 id,
UTF-16 NUL-term name, padding to 190`), 39 GroupPointer(`i32 pointer, i16 id`), 40 Filename /
41/64 StringParam(ASCII NUL-term), 49 GlobalValue(`i16`), 50 AlterableValue(`i16`), 47/51
TwoShorts(2 x i16), 55 Extension(`i16 size, i16 type, i16 code, size bytes`), 68 MultipleVariables
(`i32 flags, i32 flagMasks, i32 flagValues` + up to 4 x 16-byte values: `i32 index, i32 op,
i32 value (+ skip 4) or f64`), 69 ChildEvent(`i32 count, count x 2 x i16, skip 4`).
Unknown code: log warning, leave stream after the code field.

### 6.5 Expression item
`i16 objectType, i16 num`; if both 0 -> terminator (4 bytes total, no size field).
Otherwise `i16 size` (whole item incl. 6-byte header). Payload:
- objectType == -1 (system): num 0 -> i32; num 3 -> Yuniversal; num 23 -> f64 + f32 (12 bytes);
  num 24/50 -> skip i32 + i32; others -> payload skipped via size.
- objectType >= 2 or == -7: `u16 objectInfo, i16 objectInfoList`; if num 16/19 -> extra i16.
Trailing bytes skipped via size.

## 7. Misc constants
Object types: -7 player, -6 keyboard, -5 create, -4 timer, -3 game, -2 speaker, -1 system,
0 quick backdrop, 1 backdrop, 2 active, 3 text, 4 question, 5 score, 6 lives, 7 counter, 8 rtf,
9 sub-application, 32+ extension.
Image/sound/font handles in files are 1-based; normalized to 0-based when build >= 284
(2.5+ images always; sounds always; fonts when build > 284).

## 8. Real-game validation (v1.2.0)

Validated end-to-end against the original Five Nights at Freddy's (CF2.5, build R284,
PAMU unicode, 231 MB standalone EXE, 17 frames / 605 images / 52 sounds / 8 packed files).
All seven tools complete with zero errors and zero warnings.

Two defects surfaced only on real data (synthetic tests never exercised them):

1. **EXE entry point off by 2 bytes** — `calculateEntryPoint` skipped only the 4-byte PE
   signature before reading `numberOfSections`, so it read `Machine` (0x014C = 332 "sections")
   and computed a garbage game-data offset (read out of bounds at EOF). Fixed to skip
   6 bytes total (signature + Machine), matching the reference `ReadAscii(2) + Skip(4)`.

2. **Condition/Action size field not consumed** — both event items are `u16`-size-prefixed;
   the reader must consume the size even though it then parses fields sequentially.
   Omitting it shifted every field by 2 bytes (objectType read the size, num read the
   objectType, ...), corrupting all events: parameters desynced, condition/action stream
   ran off the buffer. FNaF frame data uses flag-3 (encrypted+compressed) nested chunks,
   so once the AES-free RC4-style key is derived from (editorFilename, appName, copyright)
   — with build 284 ordering (editorFilename FIRST) — events decode cleanly.
