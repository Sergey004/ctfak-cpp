#!/usr/bin/env python3
# Generates a synthetic Clickteam Fusion 2.5 style .ccn file (per docs/FORMAT_NOTES.md)
# for end-to-end testing of ctfak-cpp.
import struct, zlib, sys

out = bytearray()

def u16(v): out.extend(struct.pack('<H', v))
def i16(v): out.extend(struct.pack('<h', v))
def u32(v): out.extend(struct.pack('<I', v))
def i32(v): out.extend(struct.pack('<i', v))
def raw(b): out.extend(b)

def chunk(cid, flag, payload):
    i16(cid); i16(flag); i32(len(payload))
    raw(payload)

def compressed_payload(data):
    z = zlib.compress(data, 6)
    return struct.pack('<ii', len(data), len(z)) + z

# ---------------- file header ----------------
raw(b'PAMU')
u16(0); u16(0)          # runtime version / subversion
i32(4); i32(294)        # product version / build

# ---------------- app header (8739) ----------------
hdr = bytearray()
def h_u16(v): hdr.extend(struct.pack('<H', v))
def h_i16(v): hdr.extend(struct.pack('<h', v))
def h_i32(v): hdr.extend(struct.pack('<i', v))
def h_u32(v): hdr.extend(struct.pack('<I', v))
h_i32(96)                      # size
h_u16(0); h_u16(0)             # flags, newFlags
h_i16(0); h_u16(0)             # graphicsMode, otherFlags
h_i16(320); h_i16(240)         # window
h_u32(0xFFFFFFFF); h_u32(0xFFFFFFFF)  # score/lives (xor'd)
for _ in range(4):             # player controls
    h_i16(0)
    for _ in range(8): h_i16(0)
hdr.extend(bytes([0, 0, 0, 255]))  # border color
h_i32(1)                       # frames
h_i32(60)                      # frame rate
h_i32(0)                       # windows menu index
chunk(8739, 0, bytes(hdr))

# ---------------- app name (8740) ----------------
chunk(8740, 0, b'TestGame\x00')
# ---------------- author (8741) ----------------
chunk(8741, 0, b'Tester\x00')
# ---------------- editor filename (8750): triggers MakeKey(name, copyright, editor) ----------------
chunk(8750, 0, b'game.exe\x00')

# ---------------- image bank (26214) ----------------
W, H = 2, 2
pixels_bgra = bytes([
    255,0,0,255,   0,255,0,255,
    0,0,255,255,   255,255,255,255,
])
img_body = bytearray()
img_body.extend(struct.pack('<i', 0x12345678))  # checksum
img_body.extend(struct.pack('<i', 2))           # references
img_body.extend(struct.pack('<i', len(pixels_bgra)))  # dataSize
img_body.extend(struct.pack('<hh', W, H))
img_body.append(8)                              # graphicMode: 32-bit
img_body.append(16)                             # flags: Alpha (bit 4)
img_body.extend(struct.pack('<h', 0))           # reserved skip
img_body.extend(struct.pack('<hhhh', 1, 2, 3, 4))  # hot/action points
img_body.extend(bytes([0, 0, 0, 255]))          # transparent color (unused: alpha set)
img_body.extend(pixels_bgra)

img_item = bytearray()
img_item.extend(struct.pack('<i', 1))                       # handle (1-based)
img_item.extend(compressed_payload(bytes(img_body)))
bank = struct.pack('<i', 1) + bytes(img_item)
chunk(26214, 0, bank)

# ---------------- object bank (8745): one object ----------------
obj_stream = bytearray()
def obj_chunk(cid, payload):
    global obj_stream
    obj_stream.extend(struct.pack('<hh', cid, 0))
    obj_stream.extend(struct.pack('<i', len(payload)))
    obj_stream.extend(payload)

# 17476 object header
oh = bytearray()
oh.extend(struct.pack('<hhh', 0, 2, 0))   # handle=0, type=2 (active), flags
oh.extend(struct.pack('<h', 0))           # skip 2
oh.append(0)                              # inkEffect != 1
oh.extend(b'\x00\x00\x00')                # skip 3
oh.extend(bytes([10, 20, 30]))            # rgb (stored swapped in file)
oh.append(255)                            # blend
obj_chunk(17476, bytes(oh))
# 17477 name
obj_chunk(17477, b'Hero\x00')
# terminator
obj_chunk(32639, b'')
bank = struct.pack('<i', 1) + bytes(obj_stream)
chunk(8745, 0, bank)

# ---------------- sound bank (26216) ----------------
snd_name = 'snd'
snd_data = b'RIFFxxxxWAVEfmt data'
body = bytearray()
for ch in snd_name:
    body.extend(struct.pack('<H', ord(ch)))
body.extend(snd_data)
z = zlib.compress(bytes(body), 6)
item = bytearray()
item.extend(struct.pack('<I', 1))               # handle 1-based
item.extend(struct.pack('<i', 0))               # checksum
item.extend(struct.pack('<I', 1))               # references
item.extend(struct.pack('<i', len(body)))       # decompressedSize
item.append(1)                                  # flags
item.extend(b'\x00\x00\x00')
item.extend(struct.pack('<i', 0))               # reserved
item.extend(struct.pack('<i', len(snd_name)))   # nameLen (wide chars)
item.extend(struct.pack('<i', len(z)))          # compressed size
item.extend(z)
chunk(26216, 0, struct.pack('<i', 1) + bytes(item))

# ---------------- frame (13107) ----------------
fr = bytearray()
def f_chunk(cid, payload):
    global fr
    fr.extend(struct.pack('<hh', cid, 0))
    fr.extend(struct.pack('<i', len(payload)))
    fr.extend(payload)

fh = struct.pack('<ii', 640, 480) + bytes([128, 128, 200, 255]) + struct.pack('<I', 0)
f_chunk(13108, fh)
f_chunk(13109, b'Level 1\x00')
# instances
inst = bytearray()
inst.extend(struct.pack('<i', 1))  # count
inst.extend(struct.pack('<HH', 1, 0))    # handle=1, objectInfo=0
inst.extend(struct.pack('<ii', 100, 50)) # x, y
inst.extend(struct.pack('<hh', -1, 0))   # parentType, parentHandle
inst.extend(struct.pack('<hh', 0, 0))    # layer, instance
f_chunk(13112, bytes(inst))
# layers
lay = bytearray()
lay.extend(struct.pack('<I', 1))
lay.extend(struct.pack('<I', 0))
lay.extend(struct.pack('<ff', 1.0, 1.0))
lay.extend(struct.pack('<ii', 0, 0))
lay.extend(struct.pack('<H', ord('L'))); lay.extend(struct.pack('<H', 0))
f_chunk(13121, bytes(lay))
# events: empty program
ev = bytearray()
ev.extend(b'ER>>')
ev.extend(struct.pack('<hhh', 0, 0, 0))
for _ in range(17): ev.extend(struct.pack('<h', 0))
ev.extend(struct.pack('<h', 0))  # qualifiers
ev.extend(b'ERop'); ev.extend(struct.pack('<i', 0))
ev.extend(b'ERev'); ev.extend(struct.pack('<i', 0))
ev.extend(b'<<ER')
f_chunk(13117, bytes(ev))
f_chunk(32639, b'')
chunk(13107, 0, bytes(fr))

# ---------------- terminator ----------------
chunk(32639, 0, b'')

path = sys.argv[1] if len(sys.argv) > 1 else '/home/z/my-project/scripts/test_game.ccn'
open(path, 'wb').write(bytes(out))
print('Wrote', path, len(out), 'bytes')
