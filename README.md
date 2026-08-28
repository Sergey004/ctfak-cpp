# ctfak-cpp

**C**lick**T**eam **F**usion **A**rmy **K**nife — clean-room C++20 reimplementation.

A tool for decompiling and dumping assets of games built with Clickteam Fusion 2.5,
Multimedia Fusion 1.5 and Fusion 3 runtime files (`.exe`, `.ccn`, `.dat`, `.bin`, `.apk`).

This project is an **independent, from-scratch implementation** of the Clickteam Fusion
runtime file format. It was written in C++ following the clean-room approach: the format
was studied from publicly available reverse-engineering references and recorded in
[`docs/FORMAT_NOTES.md`](docs/FORMAT_NOTES.md); the implementation was then written from
that document. No source code was copied from any existing tool. It is functionally
inspired by CTFAK 2.0 (C#) but shares no code with it.

## Features

- **Containers**: Windows EXE (PE section scan + pack data), raw CCN/DAT/BIN, APK (ZIP)
- **Encryption**: standard CF 2.5 state-chunk encryption (key derivation from app
  metadata, RC4-like stream transform)
- **Compression**: zlib (modern `[size][zlib]` and legacy `[optional size] [zlib]`
  framings), LZ4 block decoder for 2.5+ images
- **Formats**: MMF 1.5, CF 2.0/2.5 (normal + 2.5+ object banks), Fusion 3, Android
- **Banks**: images (all graphic modes incl. 15/16/24/32-bit + alpha planes + colour-key),
  sounds (WAV/OGG/MP3/IT/XM/MOD/S3M sniffing), fonts (LOGFONT + TTF), music
- **Objects**: active, backdrops, text, counters, movements (all built-in types),
  animations, alterable values/strings, extensions, shaders
- **Events**: full condition/action parsing with parameter dispatch, expression tokens,
  qualifiers, the `MultipleVariables` de-obfuscation expansion and action/condition
  number fix-ups
- **Tools** (the "plugin" equivalent):
  - *Dump Everything* — runs all three dumpers
  - *Image Dumper* — every image to PNG (RGBA, transparency preserved)
  - *Sound Dumper* — sounds with magic-byte extension sniffing
  - *Packed Data Dumper* — binary files + pack data sorted by type
  - *Sorted Image Dumper* — images organised by frame/object/animation/direction,
    with `[UNSORTED]` mirrors and a `[~Lost and Found~]` folder
  - *Chunk Trace* — every chunk's processed payload written to `CHUNK_TRACE/`
  - *Export Structure as JSON* — application structure (app header, objects with all
    properties, frames with instances/layers/effects) and the **complete event program**
    (conditions, actions, parameters, expressions) as JSON
  - *Frame Layout Renderer* — rebuilds how every frame looks at start: composites
    all sprites at their stored `(x, y)` positions (hotspots applied) into PNGs,
    plus a wireframe PNG (object boxes, cyan hotspot crosshairs, two-line
    `[name] X|Y` + `img: <handle>.png` labels linking every sprite to the file the
    Image Dumper writes, 100 px coordinate grid with rulers — embedded bitmap font,
    no dependencies) and an exact placement table `.txt` per frame (includes a
    "dump file" column)
  - *Events Listing* — the complete event program (conditions/actions) of every
    frame as a human readable text listing in the Fusion event-grid style:
    `ON`/`DO` lines with resolved object names, condition/action numbers, raw
    flags, and all parameters described (timers in seconds, virtual key names,
    zones, colours, expressions, samples, positions...) — per frame plus a
    combined `ALL_EVENTS.txt`
- Zero GUI dependencies and **no build-time libraries**: zlib is located and
  loaded at runtime (see below), everything else is self-contained.

## Building

Requirements: a C++20 compiler (GCC 11+/Clang 13+/MSVC 2022) and CMake 3.16+.
**No external libraries are required to build** — mirroring the original
tool's distribution model, zlib is loaded at runtime:

- **Windows**: the executable looks for `zlibwapi.dll` (the official prebuilt
  zlib DLL — the same one the original ships in its `ctfakrequirements.zip`),
  then `zlib1.dll`. Just drop the DLL next to `ctfak-cpp.exe`.
- **Linux / macOS**: `libz.so.1` / `libz.dylib` from the system runtime is
  used (present on virtually every distribution).
- Any platform: set the `CTFAK_ZLIB_DLL` environment variable to a full path
  to override the search.

If you prefer classic compile-time linking (e.g. inside a container that ships
zlib dev headers), configure with `-DCTFAKPP_LINK_ZLIB=ON` instead.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Executables: `build/ctfak-cpp` (the tool) and `build/ctfak-tests` (unit tests).

## Usage

Interactive (like the original tool):

```
$ ./ctfak-cpp
Game path: game.exe
Parameters: -noevnt
...
Select tool:
0. Exit ctfak-cpp
1. Dump Everything
2. Image Dumper
...
```

Command line:

```
ctfak-cpp -path <file> -parameters "<params>" [-tool "<tool name>"] [-closeonfinish] [-dumps <dir>]
ctfak-cpp <file>                       # positional path, no parameters
```

Parameters (mirroring the original semantics where applicable):

| Parameter | Effect |
|---|---|
| `-noimg` | skip the image bank |
| `-nosounds` | skip the sound bank |
| `-noevnt` | skip event parsing |
| `-onlyimages` | read only image-related chunks |
| `-excludeframe(N)` | skip frame index N |
| `-f1.5` / `-android` / `-f3` | force the game type |
| `-sorteddumpstrings` | dump text paragraphs in the Sorted Image Dumper |
| `-verbose` | log every chunk while reading |

All dumps go to `Dumps/<game name>/` (override with `-dumps`).

## Tests

```bash
cd build && ctest --output-on-failure
# or directly:
./ctfak-tests
```

The suite covers zlib round-trips, the LZ4 block decoder (incl. overlapping matches and
extended lengths), the CF 2.5 key derivation and stream transform (known-answer vectors
cross-checked against an independent implementation), PNG encoding (structure + pixel
round-trip), JSON output and the binary reader.

An end-to-end check is available: `scripts/gen_test_ccn.py` builds a synthetic CF 2.5
`.ccn` file; loading it and running every tool exercises the whole pipeline.

## Real-game validation

Versions 1.2.0–1.3.0 were validated against the original **Five Nights at Freddy's**
standalone EXE (Clickteam Fusion 2.5, build R284, PAMU/unicode, 231 MB): all eight tools
complete with zero errors and zero warnings, dumping 605 PNG images, 52 sounds and
8 packed files, and parsing all 17 frames with their full event scripts (conditions,
actions, parameters, expressions) into JSON. See `docs/FORMAT_NOTES.md` §8 for the two
real-data defects this uncovered and fixed (EXE entry-point PE offset; event
condition/action size prefixes).

The *Frame Layout Renderer* (new in 1.3.0) rebuilds every FNaF screen from the binary
alone — title menu, office map, paycheck screen — placing all 147 sprites at their
exact stored positions in ~2 seconds. The rendered title screen even reproduces the
original's animated static overlay, because authored instance order is the true
z-order.

## Layout

```
src/core/      binary IO, crypto, zlib/LZ4, settings, logging
src/format/    chunk stream, containers, banks, objects, frames, events
src/io/        PNG / JSON / WAV writers
src/plugins/   FusionTool implementations (the "plugins")
src/util/      APK (ZIP) reader, helpers, sound extension sniffing
tests/         unit tests
docs/          FORMAT_NOTES.md — the clean-room interface document
```

## License

AGPL-3.0 — see [LICENSE](LICENSE).
