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

Ayar denemesi (kodu DEGISTIRMEZ; yalniz modele uygular):
  Varsayilan: koddaki carpan egrisi (Source/.../Core/MMOXPTuning.cpp); --raw ile carpansiz.
  --mob-mult   "1:1.0,10:0.6,30:0.35"   mob XP carpani, mob seviyesine gore parcali dogrusal
  --quest-mult "1:1.0,16:0.5,30:0.4"    gorev odul XP carpani, gorev kapi seviyesine gore
"""

import argparse
import math
import re
import sys
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
    return max(1, round_half_up(xp * mult))


def avg_xp_per_kill(level):
    pool = [scaled(m["xp"], level, m["lv"]) for m in normals if level - 8 <= m["lv"] <= level + 3]
    return sum(pool) / len(pool) if pool else None


def load_code_curves():
    """Source/.../Core/MMOXPTuning.cpp'deki XP_TUNING_MOB / XP_TUNING_QUEST egri noktalarini okur (kodla ayni degerler).

    Dosya yoksa ya da egri ayristirilamazsa sessizce carpansiz hesaplamaz: hata ile cikar (CI yakalasin).
    """
    path = SRC / "Core" / "MMOXPTuning.cpp"
    if not path.exists():
        sys.exit(f"HATA: {path} bulunamadi (XP carpan egrisi okunamadi; carpansiz hesap icin --raw).")
    txt = path.read_text(encoding="utf-8", errors="replace")
    res = []
    for key in ("XP_TUNING_MOB", "XP_TUNING_QUEST"):
        m = re.search(key + r":[^\n]*\n[^{]*\{(.*?)\};", txt, re.S)
        pts = re.findall(r"\{\s*(\d+)\s*,\s*([\d.]+)f\s*\}", m.group(1)) if m else []
        if not pts:
            sys.exit(f"HATA: {path.name} icinde {key} egrisi ayristirilamadi (format degisti mi?).")
        res.append(",".join(f"{a}:{b}" for a, b in pts))
    return res[0], res[1]


def round_half_up(x):
    """FMath::RoundToInt kopyasi: floor(x + 0.5) (Python round() banker's rounding kullanir)."""
    return math.floor(x + 0.5)


def mob_xp(xp, mob_level, mob_mult):
    """MakeMob: max(1, RoundToInt(xp * carpan))."""
    return max(1, round_half_up(xp * mob_mult(mob_level)))


def quest_xp(xp, gate_level, quest_mult):
    """MakeQuest: max(0, RoundToInt(xp * carpan))."""
    return max(0, round_half_up(xp * quest_mult(gate_level)))


def parse_curve(text):
    """"lv:deger,lv:deger" -> parcali dogrusal f(lv) (uclarda sabit)."""
    if not text:
        return lambda lv: 1.0
    pts = sorted((int(a), float(b)) for a, b in (p.split(":") for p in text.split(",")))

    def f(lv):
        if lv <= pts[0][0]:
            return pts[0][1]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if lv <= x1:
                return y0 + (y1 - y0) * (lv - x0) / max(1, x1 - x0)
        return pts[-1][1]
    return f


def simulate(max_level, mob_mult=None, quest_mult=None):
    """Seviye basina (lv, gerekenXP, ortXP/kill, kill gorevsiz, kill gorevli, kum.gorevXP, kum.Q_Gold, grind) satirlari."""
    if mob_mult is None or quest_mult is None:
        mtxt, qtxt = load_code_curves()  # varsayilan: koddaki (MMOXPTuning) carpanlar
        mob_mult = mob_mult or parse_curve(mtxt)
        quest_mult = quest_mult or parse_curve(qtxt)
    is_gold = lambda q: q["id"].startswith("Q_Gold")
    is_kill = lambda q: q["type"].startswith("Kill") and q["count"] > 0

    # ReachLevel gorevlerinin odulu yalniz hedef seviyeye ulasilinca teslim edilir (count = hedef seviye);
    # digerleri kapi seviyesinde acilir.
    avail = lambda q: q["count"] if q["type"] == "ReachLevel" else q["req"]

    def avg_xp(level):
        pool = [scaled(mob_xp(m["xp"], m["lv"], mob_mult), level, m["lv"]) for m in normals if level - 8 <= m["lv"] <= level + 3]
        return sum(pool) / len(pool) if pool else None

    cum_need = 0
    prev_need_q = 0.0
    out = []
    for lv in range(1, max_level + 1):
        n = need(lv)
        cum_need += n
        xpk = avg_xp(lv)
        done = [q for q in quests if avail(q) <= lv and not is_gold(q)]
        q_xp = sum(quest_xp(q["xp"], q["req"], quest_mult) for q in done)
        g_xp = sum(quest_xp(q["xp"], q["req"], quest_mult) for q in quests if avail(q) <= lv and is_gold(q))
        if xpk is None:
            out.append((lv, n, None, None, None, q_xp, g_xp, None))
            continue
        qk_xp = 0.0
        qk_here = 0.0
        for q in done:
            if is_kill(q):
                qxpk = avg_xp(q["req"]) or xpk
                qk_xp += q["count"] * qxpk
                if q["req"] == lv:
                    qk_here += q["count"]
        need_q = max(0.0, cum_need - q_xp - qk_xp)
        grind = max(0.0, need_q - prev_need_q) / xpk
        prev_need_q = max(prev_need_q, need_q)
        out.append((lv, n, xpk, n / xpk, grind + qk_here, q_xp, g_xp, grind))
    return out


def band_avgs(out):
    """(a, b, lo, hi, ort. gorevsiz, ort. gorevli, grind=0 olan seviye sayisi, bant seviye sayisi)."""
    res = []
    for a, b, lo, hi in TARGETS:
        sel = [o for o in out if a <= o[0] <= b and o[2] is not None]
        if sel:
            free = sum(1 for o in sel if o[7] == 0)
            res.append((a, b, lo, hi, sum(o[3] for o in sel) / len(sel), sum(o[4] for o in sel) / len(sel), free, len(sel)))
    return res


def band_flag(lo, hi, s_q, free, total):
    """Bant bayragi. Bantin TUM seviyelerinde grind=0 ise (gorev XP'si ihtiyaci asiyor) 'gorevli' sayi yalniz
    gorevlerin ZORUNLU oldurme hedefleridir; XP hizi hakkinda bir sey soylemez, bu yuzden hiz bayragi yerine ayri
    etiket basilir. Kismen grind=0 olan bantta hiz bayragi korunur ama not dusulur."""
    if free == total:
        return "  gorev XP'si yeterli (grind gerekmiyor; sayi = yalniz gorev oldurme hedefleri)"
    flag = "" if lo <= s_q <= hi else ("  < hedef (cok hizli)" if s_q < lo else "  > hedef (cok yavas)")
    if free:
        flag += f"  [{free}/{total} seviyede grind=0]"
    return flag


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", type=int, default=79, help="son seviye (L -> L+1)")
    ap.add_argument("--mob-mult", default=None, help="varsayilan: koddaki MMOXPTuning egrisi")
    ap.add_argument("--quest-mult", default=None)
    ap.add_argument("--raw", action="store_true", help="carpansiz (ham tablo) XP ile hesapla")
    args = ap.parse_args()

    ident = parse_curve("")
    mob_f = ident if args.raw else (parse_curve(args.mob_mult) if args.mob_mult is not None else None)
    quest_f = ident if args.raw else (parse_curve(args.quest_mult) if args.quest_mult is not None else None)
    out = simulate(args.max, mob_f, quest_f)

    print(f"Normal mob: {len(normals)}  Gorev: {len(quests)} (Q_Gold*: {sum(1 for q in quests if q['id'].startswith('Q_Gold'))})")
    print("Not: gorevli sutun, kapisi acilmis TUM gorevlerin (Q_Gold* haric) yapildigini varsayar (iyimser).")
    print("kum.gorev% = kapisi acilmis gorevlerin (Q_Gold* haric) kumulatif odul XP'si / o seviyeye kadar kumulatif gereken XP.")
    print("             %100 ustu: gorev odulleri tek basina oyuncuyu o seviyeye tasir (grind gerekmez).")
    print("G = grind 0: gorev XP'si bu seviyenin ihtiyacini zaten karsiliyor; 'gorevli' sayi yalniz gorevlerin zorunlu oldurme")
    print("    hedefleridir (0 olabilir) ve XP hizi olcusu DEGILDIR.")
    print(f"{'Lv':>3} {'gerekenXP':>10} {'ort XP/kill':>11} {'kill (gorevsiz)':>16} {'kill (gorevli)':>15} {'kum.gorevXP':>12} {'kum.gorev%':>11} {'kum.Q_Gold':>11}")
    cum = 0
    for lv, n, xpk, knq, kq, q_xp, g_xp, gr in out:
        cum += n
        pct = 100.0 * q_xp / cum if cum else 0.0
        if xpk is None:
            print(f"{lv:>3} {n:>10} {'-':>11} {'-':>16} {'-':>15} {q_xp:>12.0f} {pct:>10.0f}% {g_xp:>11.0f}  UYARI: uygun normal mob yok")
        else:
            mark = "  G" if gr == 0 else ""
            print(f"{lv:>3} {n:>10} {xpk:>11.0f} {knq:>16.1f} {kq:>15.1f} {q_xp:>12.0f} {pct:>10.0f}% {g_xp:>11.0f}{mark}")

    print("\nBant ozeti (ortalama oldurme/seviye) ve hedef [PLACEHOLDER]:")
    print(f"{'bant':<8} {'gorevsiz':>9} {'gorevli':>8}  hedef")
    for a, b, lo, hi, s_nq, s_q, free, total in band_avgs(out):
        print(f"{a}-{b:<5} {s_nq:>9.1f} {s_q:>8.1f}  {lo}-{hi}{band_flag(lo, hi, s_q, free, total)}")
    tot_nq = sum(o[3] for o in out if o[3] is not None and o[0] <= 30)
    tot_q = sum(o[4] for o in out if o[4] is not None and o[0] <= 30)
    print(f"\nLv1->30 toplam oldurme: gorevsiz ~{tot_nq:.0f}, gorevli ~{tot_q:.0f}")


if __name__ == "__main__":
    main()
