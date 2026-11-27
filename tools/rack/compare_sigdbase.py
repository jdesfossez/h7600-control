"""Compare the unit's 0C sigdbase reply (ASCII records) with sigdbase.sbs (binary) from Eventide's zip."""
import re, sys
syx = open(sys.argv[1], "rb").read(); ref = open(sys.argv[2], "rb").read()
p = syx[5:-1]
print(f"frame {len(syx)} B, payload {len(p)} B, max byte 0x{max(p):02x} (nibblized: {all(b < 16 for b in p)}), sbs {len(ref)} B")
txt = p.decode("latin1")
# unit: split into operators, collect field NAMEs per operator
unit = {}; order_u = []
for blk in re.split(r"NAME:opstart\r?\n", txt)[1:]:
    name = re.search(r"OPERATOR_NAME:(\S+)", blk).group(1)
    body = blk.split("TYPE:OPERATOREND")[0]
    unit[name] = re.findall(r"\tNAME:(\S+)", body); order_u.append(name)
# file: opstart records, length-prefixed name at +5; field names are length-prefixed strings
starts = [m.end() for m in re.finditer(rb"opstart(?!_)", ref)] + [len(ref)]
fil = {}; order_f = []
for a, b in zip(starts, starts[1:]):
    n = ref[a + 4]; name = ref[a + 5:a + 5 + n].decode("latin1"); fil[name] = ref[a:b]; order_f.append(name)
print("operators: unit", len(unit), "file", len(fil), "same order:", order_u == order_f)
print("only on unit:", sorted(set(unit) - set(fil)))
print("only in file:", sorted(set(fil) - set(unit)))
# per-operator: are all unit field names present (in order) in the file's record?
missing = {}
for k in set(unit) & set(fil):
    rec = fil[k]; pos = 0; miss = []
    for f in unit[k]:
        i = rec.find(f.encode(), pos)
        if i < 0: miss.append(f)
        else: pos = i
    if miss: missing[k] = miss
print("operators whose unit field names are not all found in the file record:", len(missing))
for k, v in list(missing.items())[:10]: print("  ", k, v[:8])
