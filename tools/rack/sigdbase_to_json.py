"""Convert the unit's 0C sigdbase reply (.syx) to text and JSON.

Structure of the payload (plain ASCII, CR LF): a sequence of module groups '{ ... }', each holding
field records '{ KEY:VALUE ... }' (depth 2). A module opens with a TYPE:OPERATOR record (NAME:opstart,
OPERATOR_NAME, OPERATOR_CLASS) and closes with TYPE:OPERATOREND. Repeating fields are bracketed by
MULTIPLE / MULTIPLEEND records (field names like obj~n), alternatives by SWITCH / CASE / SWITCHEND.
Values are kept as strings exactly as sent (e.g. '.1', or formulas like '=10/524288/...'). ATTRIBUTE
can repeat in one record, so it is always a list. All records, markers included, are kept in order."""
import json, sys
syx, txt_out, json_out = sys.argv[1:4]
d = open(syx, "rb").read()
assert d[:5] == b"\xF0\x1C\x70\x01\x0C" and d[-1] == 0xF7
p = d[5:-1].replace(b"\x00", b"")
open(txt_out, "wb").write(p)
modules, depth, group, rec = [], 0, None, None
for raw in p.decode("latin1").split("\r\n"):
    s = raw.strip()
    if s == "{":
        depth += 1
        if depth == 1: group = []
        else: rec = {}
    elif s == "}":
        if depth == 2: group.append(rec); rec = None
        else:
            head = group[0]; assert head["TYPE"] == "OPERATOR" and group[-1]["TYPE"] == "OPERATOREND"
            modules.append({"name": head["OPERATOR_NAME"], "class": head["OPERATOR_CLASS"], "fields": group})
        depth -= 1
    elif s:
        k, v = s.split(":", 1)
        if k == "ATTRIBUTE": rec.setdefault(k, []).append(v)
        else: assert k not in rec, (k, rec); rec[k] = v
assert depth == 0
json.dump({"source": "Eventide H7600 firmware 5.22, SysEx 0C reply to 0E (SIGDBASE_WANT)",
           "modules": modules}, open(json_out, "w"), indent=1)
print(len(modules), "modules")
