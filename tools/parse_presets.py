"""Parse the H7600 v5.22 Presets Manual (pdftotext -layout output) into a JSON catalog.

Usage:
    pdftotext -layout H7600V522PresetsManual.pdf presets.txt
    python3 tools/parse_presets.py presets.txt resources/presets.json
"""
import json, re, sys
FX = {"P": "Pitch", "R": "Reverb", "D": "Delay", "E": "EQ/Filter", "M": "Modulation", "Y": "Dynamics", "S": "Sampler", "C": "Complex"}
SRC = {"V": "Vocals", "G": "Guitar", "D": "Drums", "S": "Surround", "K": "Keyboard", "X": "Special FX"}
ENTRY = re.compile(r"^\s{0,6}(\d{2,4})\s{2,}(\S.*?)\s{2,}(48|96)\s+(\d)\s*,\s*(\d)\s*$")
BANK = re.compile(r"^\s{15,}(\d{1,2})\s+(\S.*?)\s*$")
FLAGS = re.compile(r"^\s{0,4}((?:\{[A-Z]*\}|\[[A-Z]*\]|\([A-Za-z]+\))+)\s*(.*)$")
TWEAK = re.compile(r"^\s*(?:⇒|Ö|Þ)\s*(.*)$")
NOISE = re.compile(r"^(Release 1\.1|\s*The H7600 Preset Collection|\s*Banks and Presets)")
def parse(lines):
    banks, presets = {}, []
    bank = None; pending = []; desc_open = None; bank_desc_open = False
    for raw in lines:
        line = raw.rstrip("\n")
        if not line.strip() or NOISE.match(line) or "Page " in line and "njr" in line:
            if not line.strip():
                bank_desc_open = False if bank_desc_open and banks[bank]["description"] else bank_desc_open
            continue
        m = ENTRY.match(line)
        if m:
            if desc_open is not None:
                pending, desc_open = [], None
            num, name, rate, ins, outs = m.groups()
            p = {"number": int(num), "name": name.strip(), "bank": bank, "max_rate_khz": int(rate), "inputs": int(ins), "outputs": int(outs), "effects": [], "sources": [], "tempo": [], "tweak": "", "description": ""}
            if pending and pending[-1]["name"].split()[0][:4].lower() != p["name"].split()[0][:4].lower():
                pending = []
            presets.append(p); pending.append(p); bank_desc_open = False
            continue
        b = BANK.match(line)
        if b and not pending and desc_open is None or (b and int(b.group(1)) <= 99 and len(line) - len(line.lstrip()) > 30):
            bank = int(b.group(1))
            banks[bank] = {"number": bank, "name": b.group(2).replace("–", "-"), "description": ""}
            pending, desc_open, bank_desc_open = [], None, True
            continue
        t = TWEAK.match(line)
        if t and pending:
            pending[-1]["tweak"] = (pending[-1]["tweak"] + " " + t.group(1)).strip(); continue
        f = FLAGS.match(line)
        if f and pending:
            flags, text = f.groups()
            for grp in re.findall(r"\{([A-Z]*)\}", flags):
                fx = [FX[c] for c in grp if c in FX]
                for p in pending: p["effects"] = fx
            for grp in re.findall(r"\[([A-Z]*)\]", flags):
                src = [SRC[c] for c in grp if c in SRC]
                for p in pending: p["sources"] = src
            for grp in re.findall(r"\(([A-Za-z]+)\)", flags):
                for p in pending: p["tempo"].append(grp)
            desc_open = pending
            for p in pending: p["description"] = text.strip()
            continue
        if desc_open is not None or pending:
            target = desc_open if desc_open is not None else pending
            for p in target: p["description"] = (p["description"] + " " + line.strip()).strip()
            desc_open = target; continue
        if bank is not None and bank_desc_open:
            banks[bank]["description"] = (banks[bank]["description"] + " " + line.strip()).strip()
    return banks, presets
if __name__ == "__main__":
    src, out = sys.argv[1], sys.argv[2]
    lines = open(src, encoding="utf-8").read().splitlines()
    start = next(i for i, l in enumerate(lines) if re.match(r"^\s{20,}1 Simple\s*$", l))
    end = next(i for i, l in enumerate(lines) if re.match(r"^\s{15,}Custom Scales Pitch Shifters\s*$", l) and i > start)
    banks, presets = parse(lines[start:end])
    idx_start = next(i for i, l in enumerate(lines) if "H7600 Presets by Number" in l)
    have = {p["number"] for p in presets}
    for l in lines[idx_start:start]:
        for m in re.finditer(r"(?:^|\s{2,})(\d{2,4})\s{2,}(\S(?:.*?\S)?)(?=\s{2,}\d{2,4}\s{2,}|\s*$)", l):
            n = int(m.group(1))
            if n not in have:
                have.add(n); b = n // 100 if n >= 100 else n // 10
                presets.append({"number": n, "name": m.group(2).strip(), "bank": b if b in banks else None, "max_rate_khz": None, "inputs": None, "outputs": None, "effects": [], "sources": [], "tempo": [], "tweak": "", "description": "", "index_only": True})
    presets.sort(key=lambda p: p["number"])
    json.dump({"source": "H7600 Presets Manual v5.22 Release 1.1", "banks": list(banks.values()), "presets": presets}, open(out, "w"), indent=1, ensure_ascii=False)
    print(len(banks), "banks,", len(presets), "presets")
