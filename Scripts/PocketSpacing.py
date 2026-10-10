#!/usr/bin/env python3
"""Spawner cepleri arasi mesafe denetimi (statik; oyunu baslatmaz, derleme degildir).

MMOGameMode.cpp icindeki SpawnFrostbound(FVector(x,y,z), ...) ve SpawnRift(FVector(x,y,z), ...) cagrilarini okur
ve ayni haritadaki cep ciftlerinin yatay (XY) uzakligini olcer. Cok yakin iki cep birbirinin mobunu cekebilir
(aggro 600 + gezinme 450); 2026-10-10'da P48 ve Ucurum Gezgini taşinirken elle olculen kural otomatiklesti.

Esikler: HATA < MIN_ERR (1500 u), UYARI < MIN_WARN (1800 u). Yalniz literal FVector okunur; lambda disi
(SpawnActorDeferred + FTransform) cepler kapsam disidir.

Kullanim: python Scripts/PocketSpacing.py [MMOGameMode.cpp yolu]
Cikis: 0 temiz (uyari olabilir), 1 hata.
"""
import itertools
import math
import os
import re
import sys

MIN_ERR = 1500.0
MIN_WARN = 1800.0

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = os.path.join(ROOT, "Source", "MyProject", "Private", "Core", "MMOGameMode.cpp")

CALL = re.compile(
    r"\b(SpawnFrostbound|SpawnRift)\s*\(\s*FVector\(\s*(-?\d+(?:\.\d*)?)f?\s*,\s*(-?\d+(?:\.\d*)?)f?\s*,\s*(-?\d+(?:\.\d*)?)f?\s*\)"
)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT
    with open(path, encoding="utf-8", errors="replace") as f:
        raw = f.read()
    code = strip_comments(raw)
    pockets = []
    for m in CALL.finditer(code):
        line = code.count("\n", 0, m.start()) + 1
        # Winter (SpawnFrostbound) ve Summer (SpawnRift) farkli haritalardir; ciftler yalniz kendi icinde.
        pockets.append((m.group(1), float(m.group(2)), float(m.group(3)), line))
    if not pockets:
        print("HATA: hic cep ayristirilamadi (PocketSpacing kalibi bozuk)")
        return 1

    errors, warnings = [], []
    for kind in sorted({p[0] for p in pockets}):
        group = [p for p in pockets if p[0] == kind]
        for a, b in itertools.combinations(group, 2):
            d = math.hypot(a[1] - b[1], a[2] - b[2])
            msg = f"MMOGameMode.cpp:{a[3]} ({a[1]:.0f},{a[2]:.0f}) <-> :{b[3]} ({b[1]:.0f},{b[2]:.0f}) = {d:.0f}u"
            if d < MIN_ERR:
                errors.append(msg + f" < {MIN_ERR:.0f}")
            elif d < MIN_WARN:
                warnings.append(msg + f" < {MIN_WARN:.0f}")
    for w in warnings:
        print("UYARI:", w)
    for e in errors:
        print("HATA:", e)
    print(f"Cep: {len(pockets)}  hata: {len(errors)}  uyari: {len(warnings)}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
