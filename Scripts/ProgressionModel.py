"""Ilerleme hizi modeli v2 (statik, editor gerektirmez; python3 yeterli).

Calistir:
  python Scripts/ProgressionModel.py            # Lv1-80 tablo + bant ozeti
  python Scripts/ProgressionModel.py --max 30   # yalniz Lv1-30

BalanceReport.py'nin iki sinirini giderir (bkz. docs/VERTICAL_SLICE_LV1_30.md 1.5):
  (a) en cok XP veren moba degil, seviye penceresindeki normal moblarin ORTALAMASINA bakar
      (spawner banttan esit olasilikla secer);
  (b) GOREV XP'sini sayar: seviye L'ye kadar kapisi acilmis tum gorevlerin odul XP'si
      toplami (kumulatif) o seviyeye kadar gereken XP'den dusulur ("hepsi yapildi" varsayimi,
      iyimser sinir). Gorevlerin kendi OLDURME hedefleri de hem oldurme sayisina hem XP'ye eklenir
      (gorev kapisi seviyesindeki ortalama XP/kill ile). Q_Gold* gorevleri ayri gosterilir
      (harcama degil birikim: bedava XP).

Bu bir MODEL'dir, oynanis olcumu degil: yurume/geri donus, olum, parti ve gercek oldurme suresi yok.
Cikti hedefleri [PLACEHOLDER]: docs/VERTICAL_SLICE_LV1_30.md 3. is (XP hiz yeniden ayari).
"""

import argparse
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source" / "MyProject" / "Private"
mob_src = (SRC / "Characters" / "MMOMobArchetype.cpp").read_text(encoding="utf-8", errors="replace")
quest_src = (SRC / "Quests" / "MMOQuestTypes.cpp").read_text(encoding="utf-8", errors="replace")

# Bant hedefleri (oldurme/seviye) — VERTICAL_SLICE 3. is, [PLACEHOLDER]
TARGETS = [(1, 5, 3, 5), (6, 10, 6, 9), (11, 15, 9, 13), (16, 20, 11, 15), (21, 25, 13, 18), (26, 30, 14, 20)]

mobs = []
mob_pat = re.compile(
    r'Table\.Add\(MakeMob\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*(\d+),\s*\d+,\s*\d+,\s*[\d.]+f,\s*(\d+)'
    r'.*?\)\);((?:(?:\s|//[^\n]*)*(?:Table\.Last\(\)\.[^;]+|\w+\(Table\.Last\(\)\));)*)', re.S)
for m in mob_pat.finditer(mob_src):
    mobs.append(dict(id=m.group(1), lv=int(m.group(2)), xp=int(m.group(3)),
                     elite="bIsElite = true" in (m.group(4) or "")))
normals = [m for m in mobs if not m["elite"]]

quests = []
q_pat = re.compile(
    r'Table\.Add\(MakeQuest\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("(?:[^"\\]|\\.)*"\),\s*'
    r'(?:T|EMMOQuestType)::(\w+),\s*TEXT\("([^"]*)"\),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)')
for m in q_pat.finditer(quest_src):
    qid, qtype, _t, count, xp, _g, req = m.groups()
    quests.append(dict(id=qid, type=qtype, xp=int(xp), req=int(req), count=int(count)))


def need(level):
    """L -> L+1 icin gereken XP (MMOStatsComponent::ComputeExperienceForLevel)."""
    if level <= 25:
        return round(100 * 1.25 ** (level - 1))
    return round(100 * 1.25 ** 24 + 4725 * (level - 25) + 591 * (level - 25) ** 2)


def scaled(xp, player, mob):
    """AMMOMob::ScaleXPForLevelGap kopyasi (BalanceReport.py ile ayni)."""
    gray = player - mob - 5
    leech = mob - player - 10
    mult = 1.0
    if gray > 0:
        mult = max(0.1, 1 - 0.1 * gray)
    elif leech > 0:
        mult = max(0.2, 1 - 0.1 * leech)
    return max(1, round(xp * mult))


def avg_xp_per_kill(level):
    pool = [scaled(m["xp"], level, m["lv"]) for m in normals if level - 8 <= m["lv"] <= level + 3]
    return sum(pool) / len(pool) if pool else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", type=int, default=79, help="son seviye (L -> L+1)")
    args = ap.parse_args()

    is_gold = lambda q: q["id"].startswith("Q_Gold")
    is_kill = lambda q: q["type"].startswith("Kill") and q["count"] > 0
    cum_need = 0
    prev_need_q = 0.0
    out = []  # (lv, gerekenXP, ortXP/kill, kill gorevsiz, kill gorevli(toplam), kum.gorevXP, kum.Q_Gold, grind)
    for lv in range(1, args.max + 1):
        n = need(lv)
        cum_need += n
        xpk = avg_xp_per_kill(lv)
        done = [q for q in quests if q["req"] <= lv and not is_gold(q)]
        q_xp = sum(q["xp"] for q in done)
        g_xp = sum(q["xp"] for q in quests if q["req"] <= lv and is_gold(q))
        if xpk is None:
            out.append((lv, n, None, None, None, q_xp, g_xp, None))
            continue
        # Gorev oldurmeleri: her gorevin hedef adedi, kapi seviyesindeki ortalama XP/kill ile XP uretir.
        qk_xp = 0.0
        qk_here = 0.0
        for q in done:
            if is_kill(q):
                qxpk = avg_xp_per_kill(q["req"]) or xpk
                qk_xp += q["count"] * qxpk
                if q["req"] == lv:
                    qk_here += q["count"]
        # Ek (gorev disi) oldurme: kumulatif gerekli XP - gorev odul XP'si - gorev oldurmelerinin XP'si
        need_q = max(0.0, cum_need - q_xp - qk_xp)
        grind = max(0.0, need_q - prev_need_q) / xpk
        prev_need_q = max(prev_need_q, need_q)
        out.append((lv, n, xpk, n / xpk, grind + qk_here, q_xp, g_xp, grind))

    print(f"Normal mob: {len(normals)}  Gorev: {len(quests)} (Q_Gold*: {sum(1 for q in quests if is_gold(q))})")
    print("Not: gorevli sutun, kapisi acilmis TUM gorevlerin (Q_Gold* haric) yapildigini varsayar (iyimser).")
    print(f"{'Lv':>3} {'gerekenXP':>10} {'ort XP/kill':>11} {'kill (gorevsiz)':>16} {'kill (gorevli)':>15} {'kum.gorevXP':>12} {'kum.Q_Gold':>11}")
    for lv, n, xpk, knq, kq, q_xp, g_xp, _gr in out:
        if xpk is None:
            print(f"{lv:>3} {n:>10} {'-':>11} {'-':>16} {'-':>15} {q_xp:>12} {g_xp:>11}  UYARI: uygun normal mob yok")
        else:
            print(f"{lv:>3} {n:>10} {xpk:>11.0f} {knq:>16.1f} {kq:>15.1f} {q_xp:>12} {g_xp:>11}")

    print("\nBant ozeti (ortalama oldurme/seviye) ve hedef [PLACEHOLDER]:")
    print(f"{'bant':<8} {'gorevsiz':>9} {'gorevli':>8}  hedef")
    for a, b, lo, hi in TARGETS:
        sel = [o for o in out if a <= o[0] <= b and o[2] is not None]
        if not sel:
            continue
        s_nq = sum(o[3] for o in sel) / len(sel)
        s_q = sum(o[4] for o in sel) / len(sel)
        flag = "" if lo <= s_q <= hi else ("  < hedef (cok hizli)" if s_q < lo else "  > hedef (cok yavas)")
        print(f"{a}-{b:<5} {s_nq:>9.1f} {s_q:>8.1f}  {lo}-{hi}{flag}")
    tot_nq = sum(o[3] for o in out if o[3] is not None and o[0] <= 30)
    tot_q = sum(o[4] for o in out if o[4] is not None and o[0] <= 30)
    print(f"\nLv1->30 toplam oldurme: gorevsiz ~{tot_nq:.0f}, gorevli ~{tot_q:.0f}")


if __name__ == "__main__":
    main()
