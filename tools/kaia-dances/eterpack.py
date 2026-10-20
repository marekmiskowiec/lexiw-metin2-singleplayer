"""Read-only EterPack (EPKD index, MCOZ/LZO + XTEA) reader for inspecting the client's packs.

usage: python -I eterpack.py <pack_dir> <pack_name> list
       python -I eterpack.py <pack_dir> <pack_name> extract <out_dir> [substring]
Keys come from the client's own pack/PackMakerLite.json.
"""
import json
import os
import struct
import sys


def xtea_decrypt(data, key):
    out = bytearray()
    delta, mask = 0x9E3779B9, 0xFFFFFFFF
    for off in range(0, len(data) - len(data) % 8, 8):
        v0, v1 = struct.unpack_from("<II", data, off)
        s = (delta * 32) & mask
        for _ in range(32):
            v1 = (v1 - ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (s + key[(s >> 11) & 3]))) & mask
            s = (s - delta) & mask
            v0 = (v0 - ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (s + key[s & 3]))) & mask
        out += struct.pack("<II", v0, v1)
    return bytes(out)


def lzo1x_decompress(src, out_len):
    dst = bytearray()
    ip = 0

    def copy(dist, n):
        s = len(dst) - dist
        if s < 0:
            raise ValueError("lzo: bad distance")
        for k in range(n):
            dst.append(dst[s + k])

    def lits(n):
        nonlocal ip
        dst.extend(src[ip:ip + n])
        ip += n

    state = "loop"
    if src[0] > 17:
        t = src[0] - 17
        ip = 1
        if t < 4:
            state = "match_next"
        else:
            lits(t)
            state = "first_literal_run"
    while True:
        if state == "loop":
            t = src[ip]; ip += 1
            if t >= 16:
                state = "match"
            else:
                if t == 0:
                    while src[ip] == 0:
                        t += 255; ip += 1
                    t += 15 + src[ip]; ip += 1
                lits(t + 3)
                state = "first_literal_run"
            continue
        if state == "first_literal_run":
            t = src[ip]; ip += 1
            if t >= 16:
                state = "match"
                continue
            dist = 1 + 0x0800 + (t >> 2) + (src[ip] << 2); ip += 1
            copy(dist, 3)
            state = "match_done"
            continue
        if state == "match":
            if t >= 64:
                dist = 1 + ((t >> 2) & 7) + (src[ip] << 3); ip += 1
                copy(dist, (t >> 5) - 1 + 2)
            elif t >= 32:
                t &= 31
                if t == 0:
                    while src[ip] == 0:
                        t += 255; ip += 1
                    t += 31 + src[ip]; ip += 1
                dist = 1 + ((src[ip] | (src[ip + 1] << 8)) >> 2); ip += 2
                copy(dist, t + 2)
            elif t >= 16:
                dist = (t & 8) << 11
                t &= 7
                if t == 0:
                    while src[ip] == 0:
                        t += 255; ip += 1
                    t += 7 + src[ip]; ip += 1
                dist += (src[ip] | (src[ip + 1] << 8)) >> 2; ip += 2
                if dist == 0:
                    break
                copy(dist + 0x4000, t + 2)
            else:
                dist = 1 + (t >> 2) + (src[ip] << 2); ip += 1
                copy(dist, 2)
            state = "match_done"
            continue
        if state == "match_done":
            t = src[ip - 2] & 3
            if t == 0:
                state = "loop"
                continue
            state = "match_next"
            continue
        if state == "match_next":
            lits(t)
            t = src[ip]; ip += 1
            state = "match"
            continue
    return bytes(dst[:out_len])
def unmcoz(buf, key):
    fourcc, enc_size, comp_size, real_size = struct.unpack_from("<4sIII", buf, 0)
    if fourcc != b"MCOZ":
        raise ValueError("not MCOZ")
    body = buf[16:]
    if enc_size:
        body = xtea_decrypt(body[:enc_size], key)
    if body[:4] == b"MCOZ":
        body = body[4:]
    return lzo1x_decompress(body[:comp_size], real_size)


def load_keys(pack_dir):
    cfg = json.load(open(os.environ.get("EPK_KEYS") or os.path.join(pack_dir, "PackMakerLite.json"), encoding="utf-8"))
    return cfg["eter_pack_index_key"], cfg["eter_pack_data_key"]


def read_index(pack_dir, name):
    ikey, _ = load_keys(pack_dir)
    raw = open(os.path.join(pack_dir, name + ".index"), "rb").read()
    if raw[:4] == b"MCOZ":
        raw = unmcoz(raw, ikey)
    magic, version, count = struct.unpack_from("<4sII", raw, 0)
    entries = []
    size = (len(raw) - 12) // max(1, count)
    for i in range(count):
        off = 12 + i * size
        rec = raw[off:off + size]
        fid = struct.unpack_from("<i", rec, 0)[0]
        fname = rec[4:165].split(b"\0", 1)[0].decode("cp949", "replace")
        crc, real, dsize, dcrc, pos = struct.unpack_from("<IiiIi", rec, 168)
        ctype = rec[188]
        entries.append({"id": fid, "name": fname, "real": real, "size": dsize, "pos": pos, "type": ctype})
    return magic, version, size, entries


def read_file(pack_dir, name, e):
    _, dkey = load_keys(pack_dir)
    with open(os.path.join(pack_dir, name + ".data"), "rb") as f:
        f.seek(e["pos"])
        buf = f.read(e["size"])
    if e["type"] == 0:
        return buf
    return unmcoz(buf, dkey if e["type"] == 2 else [0, 0, 0, 0])


if __name__ == "__main__":
    pack_dir, name, cmd = sys.argv[1], sys.argv[2], sys.argv[3]
    magic, version, size, entries = read_index(pack_dir, name)
    if cmd == "list":
        print(magic, version, "entry", size, "count", len(entries))
        for e in entries:
            print(e["type"], e["real"], e["name"])
    elif cmd == "extract":
        out, sub = sys.argv[4], (sys.argv[5] if len(sys.argv) > 5 else "")
        for e in entries:
            if sub in e["name"]:
                data = read_file(pack_dir, name, e)
                path = os.path.join(out, name, e["name"].replace(":", "").lstrip("/"))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                open(path, "wb").write(data)
                print("ok", e["name"], len(data))
