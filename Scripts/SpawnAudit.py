"""Kullanim: python Scripts/SpawnAudit.py <soak log> [<soak log> ...] (Scripts/SoakTest.ps1 ciktisi).
Bir soak logundaki 'is stuck' satirlarini en yakin spawner'a esler; spawner basina ilk doldurma ve takilma sayisi."""
import math
import re
import sys
from collections import defaultdict

spawners, stuck_by = [], defaultdict(int)
mob_lines = defaultdict(set)
cur = None
for path in sys.argv[1:]:
    for line in open(path, encoding="utf-8", errors="ignore"):
        m = re.search(r"Spawner BeginPlay @ V\(([^)]*)\)", line)
        if m:
            coords = dict(re.findall(r"([XYZ])=(-?[\d.]+)", m.group(1)))
            c = (float(coords.get("X", 0)), float(coords.get("Y", 0)))
            if c not in [s["c"] for s in spawners]:
                spawners.append({"c": c, "live": [], "tries": []})
            cur = next(s for s in spawners if s["c"] == c)
            cur["_t"] = 0
            continue
        if cur is not None and "SpawnOne: requested" in line:
            cur["_t"] += 1
            continue
        m = re.search(r"after TopUp: live=(\d+)", line)
        if cur is not None and m:
            cur["live"].append(int(m.group(1)))
            cur["tries"].append(cur["_t"])
            cur = None
            continue
        m = re.search(r"LogCharacterMovement: (\w+) is stuck.*Location: X=(-?[\d.]+) Y=(-?[\d.]+)", line)
        if m and spawners:
            x, y = float(m.group(2)), float(m.group(3))
            s = min(spawners, key=lambda s: math.hypot(s["c"][0] - x, s["c"][1] - y))
            d = math.hypot(s["c"][0] - x, s["c"][1] - y)
            key = s["c"] if d < 1500 else ("far", round(x, -2), round(y, -2))
            stuck_by[key] += 1
            mob_lines[key].add(m.group(1) + "@" + path[-12:])

print(f"{'spawner (X,Y)':>22} {'ilk dolum':>14} {'takilma':>8} {'tak. mob':>9}")
for s in sorted(spawners, key=lambda s: -stuck_by.get(s["c"], 0)):
    fill = ",".join(f"{l}/{t}" for l, t in zip(s["live"], s["tries"]))
    print(f"{str((int(s['c'][0]), int(s['c'][1]))):>22} {fill:>14} {stuck_by.get(s['c'], 0):>8} {len(mob_lines.get(s['c'], ())):>9}")
for k, v in stuck_by.items():
    if isinstance(k, tuple) and k and k[0] == "far":
        print("  spawner'dan uzak takilma:", k[1:], v)
