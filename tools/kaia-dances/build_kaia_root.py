"""Builds a new root pack with the Kaia dances (LEXIW_KAIA_DANCES_V1).

python -I build_kaia_root.py /pack /kaia /out /rootsrc /newpack
  /pack  the client's pack folder (read only)
  /kaia  "Dances by KaiaProduction" (read only; only its .gr2/.msa are read as data)
  /out   where root.index / root.data / report.txt are written
  /rootsrc  the ORIGINAL root.index / root.data (a backup when the client has a patched one)
  /newpack  the new MT2009 client pack folder (backflip and pushup, read only)
Every original entry is copied byte for byte; the patched scripts and the new
animation, motion and icon files are added. Nothing outside /out is written.
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eterpack  # noqa: E402

FIRST, LAST = 7, 24
FOLDERS = ("pc/warrior", "pc/assassin", "pc/sura", "pc/shaman",
           "pc2/warrior", "pc2/assassin", "pc2/sura", "pc2/shaman")


def L(text):
    """Text in the client's code page, as the latin-1 string the files are handled in."""
    return text.encode("cp1250").decode("latin-1")


# ---------------------------------------------------------------- packing
def xtea_encrypt(data, key):
    out = bytearray()
    delta, mask = 0x9E3779B9, 0xFFFFFFFF
    for off in range(0, len(data), 8):
        v0, v1 = struct.unpack_from("<II", data, off)
        s = 0
        for _ in range(32):
            v0 = (v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (s + key[s & 3]))) & mask
            s = (s + delta) & mask
            v1 = (v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (s + key[(s >> 11) & 3]))) & mask
        out += struct.pack("<II", v0, v1)
    return bytes(out)


def lzo_literal(data):
    """A valid LZO1X stream that holds the data as literals (no matches)."""
    n = len(data)
    out = bytearray()
    if n == 0:
        pass
    elif n <= 238:
        out.append(n + 17)
    else:
        r = n - 3 - 15
        k = (r - 1) // 255
        out.append(0)
        out += b"\0" * k
        out.append(r - 255 * k)
    out += data
    out += b"\x11\x00\x00"
    return bytes(out)


def mcoz(data, key):
    comp = lzo_literal(data)
    if key is None:
        return struct.pack("<4sIII", b"MCOZ", 0, len(comp), len(data)) + b"MCOZ" + comp
    plain = b"MCOZ" + comp
    plain += b"\0" * ((8 - len(plain) % 8) % 8)
    return struct.pack("<4sIII", b"MCOZ", len(plain), len(comp), len(data)) + xtea_encrypt(plain, key)


# ---------------------------------------------------------------- icons
FONT = {"0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111",
        "4": "101101111001001", "5": "111100111001111", "6": "111100111101111", "7": "111001001001001",
        "8": "111101111101111", "9": "111101111001111"}


def tga_decode(raw):
    idlen, cmap, kind = raw[0], raw[1], raw[2]
    w, h, bpp, desc = struct.unpack_from("<HHBB", raw, 12)
    assert cmap == 0 and bpp == 32 and kind in (2, 10), (kind, bpp)
    p, px = 18 + idlen, bytearray()
    if kind == 2:
        px = bytearray(raw[p:p + w * h * 4])
    else:
        while len(px) < w * h * 4:
            c = raw[p]
            p += 1
            if c & 0x80:
                px += raw[p:p + 4] * ((c & 0x7F) + 1)
                p += 4
            else:
                n = (c + 1) * 4
                px += raw[p:p + n]
                p += n
    return w, h, desc, px


def make_icon(base, number):
    w, h, desc, px = tga_decode(base)
    top = bool(desc & 0x20)

    def put(x, y, bgra):
        if 0 <= x < w and 0 <= y < h:
            row = y if top else h - 1 - y
            px[(row * w + x) * 4:(row * w + x) * 4 + 4] = bytes(bgra)

    text = str(number)
    width = len(text) * 6 + (len(text) - 1) * 2
    x0, y0 = w - width - 2, h - 12
    for outline, color in ((True, (0, 0, 0, 255)), (False, (80, 215, 255, 255))):
        for i, ch in enumerate(text):
            bits = FONT[ch]
            for r in range(5):
                for c in range(3):
                    if bits[r * 3 + c] != "1":
                        continue
                    for sy in range(2):
                        for sx in range(2):
                            x, y = x0 + i * 8 + c * 2 + sx, y0 + r * 2 + sy
                            if outline:
                                for dx in (-1, 0, 1):
                                    for dy in (-1, 0, 1):
                                        put(x + dx, y + dy, color)
                            else:
                                put(x, y, color)
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, desc)
    return header + bytes(px)


# ---------------------------------------------------------------- script patches
def replace_once(text, old, new, where):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{where}: anchor found {count} times: {old[:60]!r}")
    return text.replace(old, new)


EMOTION_ADD = L('''

# LEXIW_KAIA_DANCES_V1: 18 tancow z paczki "Dances by KaiaProduction" (taniec 7-24).
# Klient zna na sztywno tylko dance1..dance6, wiec te tance gra sam Python:
# ruchy sa rejestrowane po wczytaniu postaci (RegisterKaiaDanceAnis, z
# introLoading), przycisk w oknie postaci gra taniec swojej postaci, a serwer
# wysyla "kaia_dance <vid> <n>" dla tancerzy z pokazu (game.py).
# 25 = salto, 26 = pompka: ruchy z nowego klienta MT2009 (backflip, pushup).
KAIA_DANCE_FIRST = 7
KAIA_DANCE_LAST = 26
KAIA_EXTRA = {25: ("Salto", "x_backflip"), 26: ("Pompka", "x_pushup")}
KAIA_SLOT_FIRST = 101
KAIA_MOTION_FIRST = 80
KAIA_RACES = (
	(0, "pc/warrior", ("MOTION_MODE_GENERAL", "MOTION_MODE_ONEHAND_SWORD", "MOTION_MODE_TWOHAND_SWORD")),
	(1, "pc/assassin", ("MOTION_MODE_GENERAL", "MOTION_MODE_ONEHAND_SWORD", "MOTION_MODE_DUALHAND_SWORD", "MOTION_MODE_BOW")),
	(2, "pc/sura", ("MOTION_MODE_GENERAL", "MOTION_MODE_ONEHAND_SWORD")),
	(3, "pc/shaman", ("MOTION_MODE_GENERAL", "MOTION_MODE_FAN", "MOTION_MODE_BELL")),
	(4, "pc2/warrior", ("MOTION_MODE_GENERAL", "MOTION_MODE_ONEHAND_SWORD", "MOTION_MODE_TWOHAND_SWORD")),
	(5, "pc2/assassin", ("MOTION_MODE_GENERAL", "MOTION_MODE_ONEHAND_SWORD", "MOTION_MODE_DUALHAND_SWORD", "MOTION_MODE_BOW")),
	(6, "pc2/sura", ("MOTION_MODE_GENERAL", "MOTION_MODE_ONEHAND_SWORD")),
	(7, "pc2/shaman", ("MOTION_MODE_GENERAL", "MOTION_MODE_FAN", "MOTION_MODE_BELL")),
)

def KaiaDanceSlot(n):
	return KAIA_SLOT_FIRST + n - KAIA_DANCE_FIRST

def KaiaDanceMotion(n):
	return KAIA_MOTION_FIRST + n - KAIA_DANCE_FIRST

def KaiaDanceFile(n):
	if n in KAIA_EXTRA:
		return KAIA_EXTRA[n][1]
	return "kaia_dance_%d" % n

for _kaiaDance in xrange(KAIA_DANCE_FIRST, KAIA_DANCE_LAST + 1):
	if _kaiaDance in KAIA_EXTRA:
		_kaiaName, _kaiaIcon = KAIA_EXTRA[_kaiaDance][0], "icon/action/%s.tga" % KAIA_EXTRA[_kaiaDance][1]
	else:
		_kaiaName, _kaiaIcon = "Taniec %d (custom)" % _kaiaDance, "icon/action/kaia_dance%d.tga" % _kaiaDance
	EMOTION_DICT[KaiaDanceSlot(_kaiaDance)] = {"name": _kaiaName, "command": "", "kaia": _kaiaDance}
	ICON_DICT[KaiaDanceSlot(_kaiaDance)] = _kaiaIcon

_kaiaDanceRegistered = [False]

def RegisterKaiaDanceAnis():
	if _kaiaDanceRegistered[0]:
		return
	_kaiaDanceRegistered[0] = True
	for race, folder, modes in KAIA_RACES:
		try:
			chrmgr.SelectRace(race)
			chrmgr.SetPathName("d:/ymir work/%s/action/" % folder)
			for modeName in modes:
				mode = getattr(chr, modeName, None)
				if mode is None:
					continue
				chrmgr.RegisterMotionMode(mode)
				for n in xrange(KAIA_DANCE_FIRST, KAIA_DANCE_LAST + 1):
					chrmgr.RegisterMotionData(mode, KaiaDanceMotion(n), KaiaDanceFile(n) + ".msa")
		except Exception, msg:
			import dbg
			dbg.TraceError("KAIA_DANCES race %d: %s" % (race, msg))

def PlayKaiaDance(vid, n):
	try:
		vid = int(vid)
		n = int(n)
		if n < KAIA_DANCE_FIRST or n > KAIA_DANCE_LAST or not vid:
			return
		chr.SelectInstance(vid)
		chr.PushOnceMotion(KaiaDanceMotion(n))
	except Exception, msg:
		import dbg
		dbg.TraceError("KAIA_DANCES play %s %s: %s" % (vid, n, msg))
''')


def patch_emotion(text):
    return text.rstrip("\n") + "\n" + EMOTION_ADD


def patch_introloading(text):
    return replace_once(text, "\tdef __StartGame(self):\n",
                        "\tdef __StartGame(self):\n"
                        "\t\t# LEXIW_KAIA_DANCES_V1: the custom dances, once the races are loaded.\n"
                        "\t\ttry:\n\t\t\temotion.RegisterKaiaDanceAnis()\n\t\texcept:\n\t\t\tpass\n", "introloading")


def patch_game(text):
    text = replace_once(text, '\t\tserverCommandList["SidekickVid"] = self.__SidekickVid\n',
                        '\t\tserverCommandList["SidekickVid"] = self.__SidekickVid\n'
                        '\t\tserverCommandList["kaia_dance"] = self.__KaiaDance # LEXIW_KAIA_DANCES_V1\n', "game")
    return replace_once(text, "\tdef __SidekickVid(self, ",
                        "\tdef __KaiaDance(self, vid=\"0\", n=\"0\", *rest):\n"
                        "\t\t# LEXIW_KAIA_DANCES_V1: a dancer of the show plays custom dance n.\n"
                        "\t\timport emotion\n"
                        "\t\temotion.PlayKaiaDance(vid, n)\n\n"
                        "\tdef __SidekickVid(self, ", "game")


def patch_uicharacter(text, name):
    text = replace_once(text, '\t\tself.dualEmotionSlot = self.GetChild("DualEmotionSlot")\n',
                        '\t\tself.dualEmotionSlot = self.GetChild("DualEmotionSlot")\n'
                        '\t\t# LEXIW_KAIA_DANCES_V1: the custom dances under the emotions.\n'
                        '\t\ttry:\n\t\t\tself.kaiaEmotionSlot = self.GetChild("KaiaEmotionSlot")\n'
                        '\t\texcept:\n\t\t\tself.kaiaEmotionSlot = None\n', name)
    text = replace_once(text, "\t\tfor slot in (self.soloEmotionSlot, self.dualEmotionSlot):\n",
                        "\t\tfor slot in filter(None, (self.soloEmotionSlot, self.dualEmotionSlot, getattr(self, \"kaiaEmotionSlot\", None))):\n",
                        name)
    text = replace_once(text, "\t\t\tslot = self.soloEmotionSlot\n\t\t\tif slotIdx > 50:\n\t\t\t\tslot = self.dualEmotionSlot\n",
                        "\t\t\tslot = self.soloEmotionSlot\n\t\t\tif slotIdx > 50:\n\t\t\t\tslot = self.dualEmotionSlot\n"
                        "\t\t\tif slotIdx > 100:\n\t\t\t\tslot = getattr(self, \"kaiaEmotionSlot\", None)\n"
                        "\t\t\t\tif not slot:\n\t\t\t\t\tcontinue\n", name)
    return replace_once(text, "\t\tcommand = emotion.EMOTION_DICT[slotIndex][\"command\"]\n",
                        "\t\tif \"kaia\" in emotion.EMOTION_DICT[slotIndex]:\n"
                        "\t\t\temotion.PlayKaiaDance(player.GetMainCharacterIndex(), emotion.EMOTION_DICT[slotIndex][\"kaia\"])\n"
                        "\t\t\treturn\n\n"
                        "\t\tcommand = emotion.EMOTION_DICT[slotIndex][\"command\"]\n", name)


KAIA_GRID_OLD = L('''
						{ "name":"Kaia_Bar", "type":"horizontalbar", "x":12, "y":188, "width":223, },
						{ "name":"Kaia_Bar_Text", "type":"text", "x":15, "y":190, "text":"Tańce (custom)" },
						{
							"name" : "KaiaEmotionSlot",
							"type" : "grid_table",
							"x" : 12,
							"y" : 208,
							"start_index" : 101,
							"x_count" : 7,
							"y_count" : 3,
							"x_step" : 32,
							"y_step" : 32,
							"x_blank" : 0,
							"y_blank" : 0,
							"image" : ICON_SLOT_FILE,
						},
''')


def patch_characterwindow(text, name):
    # The old window: the mutual actions take one row, the custom dances the rest.
    text = replace_once(text, '"y":8+150, "width":223, },', '"y":133, "width":223, },', name)
    text = replace_once(text, '"x":15, "y":10+150, "text":uiScriptLocale.CHARACTER_MUTUAL_ACTION },',
                        '"x":15, "y":135, "text":uiScriptLocale.CHARACTER_MUTUAL_ACTION },', name)
    old = ('\t\t\t\t\t\t\t"name" : "DualEmotionSlot",\n\t\t\t\t\t\t\t"type" : "grid_table",\n\t\t\t\t\t\t\t"x" : 30,\n'
           '\t\t\t\t\t\t\t"y" : 180,\n\t\t\t\t\t\t\t"start_index" : 51,\n\t\t\t\t\t\t\t"x_count" : 6,\n\t\t\t\t\t\t\t"y_count" : 3,\n')
    new = old.replace('"y" : 180,', '"y" : 153,').replace('"y_count" : 3,', '"y_count" : 1,')
    text = replace_once(text, old, new, name)
    anchor = '\t\t\t\t\t\t\t"image" : ICON_SLOT_FILE,\n\t\t\t\t\t\t},\n\t\t\t\t\t),\n\t\t\t\t},\n\t\t\t\t{\n\t\t\t\t\t"name" : "Quest_Page",'
    return replace_once(text, anchor,
                        '\t\t\t\t\t\t\t"image" : ICON_SLOT_FILE,\n\t\t\t\t\t\t},' + KAIA_GRID_OLD.rstrip("\n") +
                        '\n\t\t\t\t\t),\n\t\t\t\t},\n\t\t\t\t{\n\t\t\t\t\t"name" : "Quest_Page",', name)


def patch_characterwindownew(text, name):
    # The new window: its empty "special actions" grid becomes the custom dances.
    text = replace_once(text, '{ "name":"Special_Action_Bar_Img", "type":"image", "x":15, "y":10+190+24, "image":ROOT_PATH+"special_action_bar_img.sub", },',
                        L('{ "name":"Kaia_Bar_Text", "type":"text", "x":15, "y":10+190+25, "text":"Tańce (custom)" },'), name)
    old = ('\t\t\t"name" : "SpecialEmotionSlot",\n\t\t\t"type" : "grid_table",\n\n\t\t\t"x" : 15,\n\t\t\t"y" : 220+25,\n'
           '\t\t\t"start_index" : 0,#app.SPECIAL_ACTION_START_INDEX,\n\t\t\t"x_count" : 6,\n\t\t\t"y_count" : 2,\n'
           '\t\t\t"x_step" : 32,\n\t\t\t"y_step" : 32,\n\t\t\t"x_blank" : 5,\n\t\t\t"y_blank" : 10,\n')
    new = ('\t\t\t"name" : "KaiaEmotionSlot",\n\t\t\t"type" : "grid_table",\n\n\t\t\t"x" : 15,\n\t\t\t"y" : 220+25,\n'
           '\t\t\t"start_index" : 101,\n\t\t\t"x_count" : 7,\n\t\t\t"y_count" : 3,\n'
           '\t\t\t"x_step" : 32,\n\t\t\t"y_step" : 32,\n\t\t\t"x_blank" : 0,\n\t\t\t"y_blank" : 2,\n')
    return replace_once(text, old, new, name)


PATCHES = {
    "emotion.py": patch_emotion,
    "introloading.py": patch_introloading,
    "game.py": patch_game,
    "uicharacter.py": lambda t: patch_uicharacter(t, "uicharacter.py"),
    "uicharacternew.py": lambda t: patch_uicharacter(t, "uicharacternew.py"),
    "uiscript/characterwindow.py": lambda t: patch_characterwindow(t, "characterwindow.py"),
    "uiscript/characterwindownew.py": lambda t: patch_characterwindownew(t, "characterwindownew.py"),
    "uiscript/characterwindownewae.py": lambda t: patch_characterwindownew(t, "characterwindownewae.py"),
}


def main():
    pack_dir, kaia_dir, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    # The root to start from (the original, also when the client already has a patched one)
    # and the new MT2009 client's pack with the backflip and the push-up.
    root_dir = sys.argv[4] if len(sys.argv) > 4 else pack_dir
    if len(sys.argv) < 6:
        raise SystemExit("usage: build_kaia_root.py /pack /kaia /out /root_src /new_client_pack")
    extra_dir = sys.argv[5]
    ikey, dkey = eterpack.load_keys(pack_dir)
    magic, version, size, entries = eterpack.read_index(root_dir, "root")
    raw_index = eterpack.unmcoz(open(os.path.join(root_dir, "root.index"), "rb").read(), ikey)
    assert magic == b"EPKD" and size == 192
    names = {e["name"] for e in entries}
    report = []
    for f in PATCHES:
        if f not in names:
            raise SystemExit(f"root has no {f}")
    if any(e["name"].startswith("icon/action/kaia_dance") for e in entries):
        raise SystemExit("root already has the Kaia dances: build from the original root")

    data_in = open(os.path.join(root_dir, "root.data"), "rb")
    blocks = []   # (record bytes template or None, name, type, stored bytes)
    for i, e in enumerate(entries):
        rec = raw_index[12 + i * 192:12 + (i + 1) * 192]
        data_in.seek(e["pos"])
        stored = data_in.read(e["size"])
        if e["name"] in PATCHES:
            text = eterpack.read_file(root_dir, "root", e).decode("latin-1").replace("\r\n", "\n")
            patched = PATCHES[e["name"]](text).replace("\n", "\r\n").encode("latin-1")
            open(os.path.join(out_dir, "patched_" + e["name"].replace("/", "_")), "wb").write(patched)
            stored = mcoz(patched, dkey) if e["type"] == 2 else (mcoz(patched, None) if e["type"] == 1 else patched)
            report.append(f"patched {e['name']} ({len(patched)} B)")
        blocks.append((rec, e["name"], e["type"], stored))

    def add(name, ctype, payload):
        stored = payload if ctype == 0 else mcoz(payload, dkey if ctype == 2 else None)
        blocks.append((None, name, ctype, stored))

    added = 0
    for folder in FOLDERS:
        src = os.path.join(kaia_dir, "ymir work", folder, "action")
        for n in range(FIRST, LAST + 1):
            gr2 = open(os.path.join(src, f"dance_{n}.gr2"), "rb").read()
            if gr2[:4] != bytes.fromhex("29de6cc0"):
                raise SystemExit(f"{folder} dance_{n}.gr2 is not a Granny file")
            msa = open(os.path.join(src, f"dance_{n}.msa"), "rb").read().decode("latin-1")
            if "ScriptType" not in msa or "MotionData" not in msa:
                raise SystemExit(f"{folder} dance_{n}.msa is not a motion script")
            lines = []
            for line in msa.replace("\r\n", "\n").split("\n"):
                if line.strip().startswith("MotionFileName"):
                    line = f'MotionFileName           "D:/Ymir Work/{folder}/action/kaia_dance_{n}.gr2"'
                lines.append(line)
            base = f"d:/ymir work/{folder}/action/kaia_dance_{n}"
            add(base + ".gr2", 0, gr2)
            add(base + ".msa", 2, "\r\n".join(lines).encode("latin-1"))
            added += 2
    icon_pack = eterpack.read_index(pack_dir, "icon")[3]
    bases = {e["name"]: e for e in icon_pack if e["name"].startswith("icon/action/dance")}
    for n in range(FIRST, LAST + 1):
        base = bases[f"icon/action/dance{(n - 1) % 8 + 1}.tga"]
        icon = make_icon(eterpack.read_file(pack_dir, "icon", base), n)
        open(os.path.join(out_dir, f"kaia_dance{n}.tga"), "wb").write(icon)
        add(f"icon/action/kaia_dance{n}.tga", 0, icon)
        added += 1
    if extra_dir:
        # Backflip (salto, 25) and push-up (pompka, 26) from the new client, for every race.
        packs = {}
        for pk in ("pc_1", "pc_2", "pc2_1", "pc2_2", "icon"):
            packs[pk] = {e["name"]: e for e in eterpack.read_index(extra_dir, pk)[3]}
        def extra(name):
            for pk, found in packs.items():
                if name in found:
                    return eterpack.read_file(extra_dir, pk, found[name])
            raise SystemExit(f"the new client has no {name}")
        for move in ("backflip", "pushup"):
            for folder in FOLDERS:
                gr2 = extra(f"d:/ymir work/{folder}/action/{move}.gr2")
                if gr2[:4] != bytes.fromhex("29de6cc0"):
                    raise SystemExit(f"{folder} {move}.gr2 is not a Granny file")
                msa = extra(f"d:/ymir work/{folder}/action/{move}.msa").decode("latin-1")
                if "ScriptType" not in msa or "MotionData" not in msa:
                    raise SystemExit(f"{folder} {move}.msa is not a motion script")
                lines = [f'MotionFileName           "D:/Ymir Work/{folder}/action/x_{move}.gr2"'
                         if line.strip().startswith("MotionFileName") else line
                         for line in msa.replace("\r\n", "\n").split("\n")]
                base = f"d:/ymir work/{folder}/action/x_{move}"
                add(base + ".gr2", 0, gr2)
                add(base + ".msa", 2, "\r\n".join(lines).encode("latin-1"))
                added += 2
            add(f"icon/action/x_{move}.tga", 0, extra(f"icon/action/{move}.tga"))
            added += 1
    report.append(f"added {added} files")

    # Write: blocks contiguous, every one at a 256 boundary, as the original packer does.
    index = bytearray(struct.pack("<4sII", b"EPKD", version, len(blocks)))
    pos = 0
    with open(os.path.join(out_dir, "root.data"), "wb") as out:
        for i, (rec, name, ctype, stored) in enumerate(blocks):
            slot = (len(stored) + 255) // 256 * 256
            out.write(stored + b"\0" * (slot - len(stored)))
            r = bytearray(rec) if rec else bytearray(192)
            if not rec:
                nb = name.encode("latin-1")
                r[4:165] = nb + b"\0" * (161 - len(nb))
            struct.pack_into("<i", r, 0, i)
            struct.pack_into("<IiiIi", r, 168, zlib.crc32(name.encode("latin-1")) & 0xFFFFFFFF, slot, len(stored),
                             zlib.crc32(stored) & 0xFFFFFFFF, pos)
            r[188] = ctype
            index += r
            pos += slot
    open(os.path.join(out_dir, "root.index"), "wb").write(mcoz(bytes(index), ikey))

    # Read the result back with the reader and compare every file.
    check_dir = out_dir
    import shutil
    shutil.copy(os.path.join(pack_dir, "PackMakerLite.json"), os.path.join(check_dir, "PackMakerLite.json"))
    _, _, _, new_entries = eterpack.read_index(check_dir, "root")
    old_by_name = {e["name"]: e for e in entries}
    bad = 0
    for e in new_entries:
        got = eterpack.read_file(check_dir, "root", e)
        if e["name"] in old_by_name and e["name"] not in PATCHES:
            if got != eterpack.read_file(root_dir, "root", old_by_name[e["name"]]):
                bad += 1
    report.append(f"entries {len(entries)} -> {len(new_entries)}, mismatches {bad}")
    report.append(f"root.data {pos} B")
    open(os.path.join(out_dir, "report.txt"), "w").write("\n".join(report) + "\n")
    print("\n".join(report))
    if bad:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
