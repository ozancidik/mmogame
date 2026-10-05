"""Seviye basina oldurme sayisi raporu (statik, editor gerektirmez).

Calistir:
  "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/BalanceReport.py

Her seviye L (1..79) icin: L->L+1 icin gereken XP, mob seviyesi [L-8, L+3] araligindaki en cok XP veren normal mob
(seviye farki kurali AMMOMob::ScaleXPForLevelGap ile) ve gereken oldurme sayisi. Plan hedefi
12-25 oldurme (docs/PROGRESSION_PLAN.md). Gorev XP'si dahil DEGIL (en kotu durum).

Not: mob XP'sine MMOXPTuning carpani uygulanir (ProgressionModel ile ayni egri; Core/MMOXPTuning.cpp'den okunur,
ayni floor(x+0.5) yuvarlama ile). Egri okunamazsa script hata ile cikar.
"""

import math
import re
from pathlib import Path

from ProgressionModel import load_code_curves, mob_xp, parse_curve, round_half_up

ROOT = Path(__file__).resolve().parent.parent
mob_src = (ROOT / "Source/MyProject/Private/Characters/MMOMobArchetype.cpp").read_text(encoding="utf-8", errors="replace")

# MakeMob(Id, Ad, Level, Str, Vit, Dmg, XP, ...) ; elit bayragi sonraki Table.Last() satirlarinda
mobs = []
pat = re.compile(
    r'Table\.Add\(MakeMob\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*(\d+),\s*\d+,\s*\d+,\s*[\d.]+f,\s*(\d+)'
    r'.*?\)\);((?:(?:\s|//[^\n]*)*(?:Table\.Last\(\)\.[^;]+|\w+\(Table\.Last\(\)\));)*)', re.S)
for m in pat.finditer(mob_src):
    mid, lv, xp, tail = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4) or ""
    mobs.append(dict(id=mid, lv=lv, xp=xp, elite="bIsElite = true" in tail))

# ProgressionModel ile ayni egri: ham MakeMob XP'si -> MakeMob icindeki carpanli deger.
mob_mult = parse_curve(load_code_curves()[0])
for m in mobs:
    m["xp"] = mob_xp(m["xp"], m["lv"], mob_mult)


def need(level):
    """L -> L+1 icin gereken XP (MMOStatsComponent::ComputeExperienceForLevel)."""
    if level <= 25:
        return round(100 * 1.25 ** (level - 1))
    return round(100 * 1.25 ** 24 + 4725 * (level - 25) + 591 * (level - 25) ** 2)


def scaled(xp, player, mob):
    gray = player - mob - 5
    leech = mob - player - 10
    mult = 1.0
    if gray > 0:
        mult = max(0.1, 1 - 0.1 * gray)
    elif leech > 0:
        mult = max(0.2, 1 - 0.1 * leech)
    return max(1, round_half_up(xp * mult))


normals = [m for m in mobs if not m["elite"]]
print(f"Normal mob: {len(normals)}  Elit: {len(mobs) - len(normals)}")
print(f"{'Lv':>3} {'gerekenXP':>10} {'en iyi normal':<20} {'XP/kill':>8} {'kill':>5}  not")
flag = 0
for lv in range(1, 80):
    n = need(lv)
    best = None
    for m in normals:
        if not (lv - 8 <= m["lv"] <= lv + 3):
            continue
        x = scaled(m["xp"], lv, m["lv"])
        if best is None or x > best[0]:
            best = (x, m)
    if not best:
        print(f"{lv:>3} {n:>10} {'-':<20} {'-':>8} {'-':>5}  UYARI: uygun normal mob yok")
        flag += 1
        continue
    x, m = best
    kills = math.ceil(n / x)
    note = ""
    if kills > 25:
        note = "UYARI: >25 (kas)"
        flag += 1
    elif kills < 8:
        note = "hizli (<8)"
    print(f"{lv:>3} {n:>10} {m['id']+' L'+str(m['lv']):<20} {x:>8} {kills:>5}  {note}")
print(f"\nUyari sayisi: {flag}")
