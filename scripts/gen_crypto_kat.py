#!/usr/bin/env python3
# Independent transcription of the documented CF2.5 key derivation + chunk transform
# (from docs/FORMAT_NOTES.md), used to generate known-answer vectors for the C++ tests.
MAGIC = 54

def rotl8(v):
    v &= 0xFF
    return ((v << 7) | (v >> 1)) & 0xFF

def key_string(s):
    # UTF-8 -> UTF-16 code units -> low byte if nonzero, high byte if nonzero
    out = bytearray()
    for ch in s.encode('utf-8').decode('utf-8'):
        code = ord(ch)
        if code < 0x10000:
            units = [code]
        else:
            code -= 0x10000
            units = [0xD800 + (code >> 10), 0xDC00 + (code & 0x3FF)]
        for u in units:
            if (u & 0xFF) != 0:
                out.append(u & 0xFF)
            if ((u >> 8) & 0xFF) != 0:
                out.append((u >> 8) & 0xFF)
    return bytes(out)

def make_key_combined(data):
    data = bytearray(data[:])
    data_len = len(data)
    data.extend(b'\x00' * (256 - data_len))
    v34 = MAGIC
    last = MAGIC
    for i in range(data_len + 1):
        v34 = rotl8(v34)
        data[i] ^= v34
        last = (last + data[i] * ((v34 & 1) + 2)) & 0xFF
    data[data_len + 1] = last
    return bytes(data)

def init_table(key, magic_char=MAGIC):
    table = list(range(256))
    accum = magic_char
    hash_ = magic_char
    never_reset = True
    i2 = 0
    key_pos = 0
    for i in range(256):
        hash_ = rotl8(hash_)
        if never_reset:
            accum = (accum + (2 if (hash_ & 1) == 0 else 3)) & 0xFF
            accum = (accum * key[key_pos]) & 0xFF
        if hash_ == key[key_pos]:
            hash_ = rotl8(magic_char)
            key_pos = 0
            never_reset = False
        i2 = (i2 + (hash_ ^ key[key_pos]) + table[i]) & 0xFF
        table[i], table[i2] = table[i2], table[i]
        key_pos += 1
    return table

def transform(table, chunk):
    buf = table[:]
    i = 0
    i2 = 0
    out = bytearray()
    for b in chunk:
        i = (i + 1) & 0xFF
        i2 = (i2 + buf[i]) & 0xFF
        buf[i], buf[i2] = buf[i2], buf[i]
        x = buf[(buf[i] + buf[i2]) & 0xFF]
        out.append(b ^ x)
    return bytes(out)

def make_key(s1, s2, s3):
    return make_key_combined(key_string(s1) + key_string(s2) + key_string(s3))

# --- vectors ---
name, copyright_, editor = "Game", "", "game.exe"
key = make_key(name, copyright_, editor)  # build > 284 order
table = init_table(key)
payload = bytes(range(64))
transformed = transform(table, payload)

print("KEY hex (first 32):", key[:32].hex())
print("KEY[255]:", key[255])
print("TABLE first 16:", ' '.join(str(table[i]) for i in range(16)))
print("PAYLOAD:", payload.hex())
print("TRANSFORMED:", transformed.hex())
print("ROUNDTRIP OK:", transform(table, transformed) == payload)
