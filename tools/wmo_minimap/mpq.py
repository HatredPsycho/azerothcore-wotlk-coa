import struct
import zlib


def _crypt_table():
    table = [0] * 0x500
    seed = 0x00100001
    for i in range(0x100):
        index = i
        for _ in range(5):
            seed = (seed * 125 + 3) % 0x2AAAAB
            high = (seed & 0xFFFF) << 16
            seed = (seed * 125 + 3) % 0x2AAAAB
            low = seed & 0xFFFF
            table[index] = high | low
            index += 0x100
    return table


CRYPT = _crypt_table()


def hash_string(name, kind):
    seed1, seed2 = 0x7FED7FED, 0xEEEEEEEE
    for ch in name.upper().replace("/", "\\"):
        value = ord(ch)
        seed1 = CRYPT[(kind << 8) + value] ^ ((seed1 + seed2) & 0xFFFFFFFF)
        seed2 = (value + seed1 + seed2 + (seed2 << 5) + 3) & 0xFFFFFFFF
    return seed1


def encrypt(data, key):
    words = list(struct.unpack("<%dI" % (len(data) // 4), data))
    seed = 0xEEEEEEEE
    for i, word in enumerate(words):
        seed = (seed + CRYPT[0x400 + (key & 0xFF)]) & 0xFFFFFFFF
        words[i] = word ^ ((key + seed) & 0xFFFFFFFF)
        key = ((((~key) << 0x15) + 0x11111111) | (key >> 0x0B)) & 0xFFFFFFFF
        seed = (word + seed + (seed << 5) + 3) & 0xFFFFFFFF
    return struct.pack("<%dI" % len(words), *words)


def write_mpq(path, files):
    files = dict(files)
    files["(listfile)"] = ("\r\n".join(sorted(files)) + "\r\n").encode("latin1")
    names = list(files)
    hash_size = 1
    while hash_size < len(names) * 2:
        hash_size *= 2

    header_size = 32
    sector_size = 512 << 3
    body = bytearray()
    blocks = []
    for name in names:
        data = files[name]
        offset = header_size + len(body)
        sectors = [data[i:i + sector_size] for i in range(0, len(data), sector_size)] or [b""]
        packed = []
        for sector in sectors:
            compressed = b"\x02" + zlib.compress(sector, 9)
            packed.append(compressed if len(compressed) < len(sector) else sector)
        table_size = 4 * (len(packed) + 1)
        positions = [table_size]
        for chunk in packed:
            positions.append(positions[-1] + len(chunk))
        stored = struct.pack("<%dI" % len(positions), *positions) + b"".join(packed)
        body += stored
        blocks.append((offset, len(stored), len(data), 0x80000000 | 0x00000200))

    hash_table = [(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFFFFFF)] * hash_size
    for block_index, name in enumerate(names):
        slot = hash_string(name, 0) % hash_size
        while hash_table[slot][4] != 0xFFFFFFFF:
            slot = (slot + 1) % hash_size
        hash_table[slot] = (hash_string(name, 1), hash_string(name, 2), 0, 0, block_index)

    hash_raw = b"".join(struct.pack("<IIHHI", *entry) for entry in hash_table)
    block_raw = b"".join(struct.pack("<IIII", *entry) for entry in blocks)
    hash_offset = header_size + len(body)
    block_offset = hash_offset + len(hash_raw)
    archive_size = block_offset + len(block_raw)
    header = struct.pack("<4sIIHHIIII", b"MPQ\x1a", header_size, archive_size, 0, 3,
                         hash_offset, block_offset, hash_size, len(blocks))
    with open(path, "wb") as out:
        out.write(header)
        out.write(body)
        out.write(encrypt(hash_raw, hash_string("(hash table)", 3)))
        out.write(encrypt(block_raw, hash_string("(block table)", 3)))
