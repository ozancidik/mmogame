"""Altin ekonomisi modeli (statik, editor gerektirmez; python3 yeterli).

Calistir:  python Scripts/EconomyModel.py [--upgrade-to N]

ProgressionModel.py'nin seviye basina oldurme sayisini kullanir ve sunlari hesaplar:
  gelir  = oldurme x ortalama mob altini (seviye penceresindeki normal moblar) + gorev altin odulleri
  gider  = her ekipman kademesinin 4 parcasinin alis fiyati + parca basina +N yukseltme maliyeti
Kademe basina (kademe seviyeleri arasi) gelir/gider oranini basar. MODEL: iksir/sifaci/ticaret/ganimet satisi,
olum ve parti yok; yukseltme basarisizliginda kaybolan altin ve yanma yok (iyimser gider); loot satisi yok
(kotumser gelir). Sonuclar [PLACEHOLDER] degerlendirme icindir, oynanis olcumu degildir.

Grind=0 tabani: ProgressionModel'de gorev XP'si ihtiyaci astigi seviyelerde (grind=0) 'gorevli' oldurme sayisi yalniz
gorevlerin zorunlu hedefleridir ve 0 olabilir; oyuncu gercekte gorev hedefine giderken/ganimet icin yine mob keser.
Bu seviyelerde gelir icin oldurme = max(gorev hedefleri, --min-kill-frac x gorevsiz oldurme sayisi) varsayilir
(varsayilan 0.5 = "gorevsiz grind'in yarisi kadar yan oldurme"; [PLACEHOLDER], olcumle ([MMOMETRIC] kill) degismeli).

Birlestirme (nadirlik fuzyonu, 2026-10-17, docs/DESIGN_FUSION.md D0; ciktinin en sonunda ayri bolum):
  Fiyat/nadirlik/dusme sabitleri MMOItemDatabase.cpp, MMOMob.h, MMOFusion.cpp/.h dosyalarindan PARSE edilir (elle sayi yok).
  --check-fusion : yalniz tablo denetimi (CI/yerel); bedava terfi, sifir ucret, satis arbitraji, Efsanevi uretimi => cikis 1.
  --fusion-full  : 57 ekipmanin tamami icin adim tablosu. Normal kosu da denetimi yapar ve hata varsa cikis 1 verir.

Altin sink senaryolari (2026-10-17, DECISIONS_NEEDED karar 3; ayri bayrak, normal cikti DEGISMEZ):
  --sink-scenarios : bant bazinda mevcut sink toplami (yukseltme beklenen maliyeti basarisizlik/yanma dahil, sifaci, iksir, Q_Gold bagisi,
                     birlestirme), acik, 4 aday (A sifaci, B bagis, C yukseltme egrisi, D opsiyonel) ve paketler. Formuller kaynaktan
                     parse edilir (GetUpgradeCost/Chances/BurnChances/ComputeHealCost/PotionPrice/GoldDonationMinTarget); varsayimlar
                     dosyadaki SINK_* sabitleridir [TAHMIN].

Yeni kalemler (2026-10-11; ciktinin sonunda ayri bolum, ustteki eski tablo DEGISMEZ):
  sifaci bedeli (sink, MMOHealerNPC.h::ComputeHealCost), basarim altini (kaynak, tek seferlik, MMOAchievementDatabase.cpp),
  gunluk gorevler (kaynak, gunluk, MMODailyComponent.cpp::BuildBounties), posta ucreti (sink, MMOMailComponent.h::FeeFor).
  Katsayilar koddan okunur. Zaman: seviye basina oldurme x KILL_SEC + gorev x QUEST_SEC (TimelineModel varsayilanlari;
  oldurme sayisi gelirdeki ile ayni, grind=0 tabani dahil). Olum/saat ve oyun saati/gun [TAHMIN] (asagidaki sabitler).
"""

import argparse
import importlib.util
import math
import random
import re
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source" / "MyProject" / "Private"
PUB = ROOT / "Source" / "MyProject" / "Public"

# Yeni kalem varsayimlari [TAHMIN], oynanis olcumu DEGIL.
KILL_SEC = 25.0            # TimelineModel varsayilani
QUEST_SEC = 90.0           # TimelineModel varsayilani
DEATHS_PER_HOUR = (2, 4, 6)   # sifaci ziyareti = olum basina bir tam sifa varsayimi (dusuk, orta, yuksek)
PLAY_HOURS_PER_DAY = (1, 2, 3)  # gunluk gorev seti gunde bir kez (cok, orta, az gunluk payi)
HEAL_TARGET_PCT = (5.0, 10.0)   # DECISIONS_NEEDED: sifaci gelirin ~%5-10'u (hedef [TAHMIN])
DAILY_GOLD_PCT_1H = (10.0, 100.0)  # gunluk set altini / 1 saatlik gelir: <10 etkisiz, >100 asiri [TAHMIN]
DAILY_XP_BAND_PCT = (2.0, 15.0)    # gunluk XP / bant esik toplami @2 sa/gun: <2 etkisiz, >15 asiri [TAHMIN]

spec = importlib.util.spec_from_file_location("pm", ROOT / "Scripts" / "ProgressionModel.py")
pm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pm)

mob_src = (SRC / "Characters" / "MMOMobArchetype.cpp").read_text(encoding="utf-8", errors="replace")
quest_src = (SRC / "Quests" / "MMOQuestTypes.cpp").read_text(encoding="utf-8", errors="replace")
item_src = (SRC / "Items" / "MMOItemDatabase.cpp").read_text(encoding="utf-8", errors="replace")

# MakeMob(Id, Ad, Lv, Str, Vit, Dmg, XP, MinGold, MaxGold, ...)
mob_gold = {}
for m in re.finditer(r'MakeMob\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*\d+,\s*\d+,\s*\d+,\s*[\d.]+f,\s*\d+,\s*(\d+),\s*(\d+)', mob_src):
    mob_gold[m.group(1)] = (int(m.group(2)) + int(m.group(3))) / 2.0

# Gorev altin odulleri: MakeQuest(..., target, count, xp, gold, req)
quest_gold = {}
for m in re.finditer(
        r'Table\.Add\(MakeQuest\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("(?:[^"\\]|\\.)*"\),\s*'
        r'(?:T|EMMOQuestType)::(\w+),\s*TEXT\("[^"]*"\),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)', quest_src):
    qid, qtype, count, _xp, gold, req = m.groups()
    quest_gold[qid] = (qtype, int(count), int(gold), int(req))

GOLD_DONATION_MIN = 5000  # MMOQuestComponent.h GoldDonationMinTarget ile ayni tut\n\n# Ekipman: MakeItem(Id, Ad, Slot, Atk, Def, ReqLv)
items = []
for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*S::(\w+),\s*([\d.]+)f,\s*([\d.]+)f,\s*(\d+)', item_src):
    # bVendorSold = false (boss/odul seti): saticidan alinmaz, altin giderine sayilmaz.
    nxt = item_src.find("Table.Add(", m.end())
    if re.search(r"bVendorSold\s*=\s*false", item_src[m.end(): nxt if nxt > 0 else len(item_src)]):
        continue
    items.append(dict(id=m.group(1), slot=m.group(2), atk=float(m.group(3)), df=float(m.group(4)), lv=int(m.group(5))))


def buy_price(it):
    return round((it["atk"] + it["df"]) * 12.0) + it["lv"] * 25 + 50


def upgrade_cost(to_plus, req_level=0):
    # FMMOItemDatabase::GetUpgradeCost ile ayni: 50 x (+N) x (100 + Lv^2) / 100 (tamsayi, 2026-10-09 altin sink'i).
    return sum(50 * (p + 1) * (100 + req_level * req_level) // 100 for p in range(to_plus))


def avg_gold(level):
    pool = [mob_gold[m["id"]] for m in pm.normals if level - 8 <= m["lv"] <= level + 3 and m["id"] in mob_gold]
    return sum(pool) / len(pool) if pool else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upgrade-to", type=int, default=3, help="her parcayi bu +N seviyeye yukselt (varsayilan 3)")
    ap.add_argument("--min-kill-frac", type=float, default=0.5,
                    help="grind=0 seviyelerinde gelir icin oldurme tabani = bu oran x gorevsiz oldurme (varsayilan 0.5)")
    ap.add_argument("--check-fusion", action="store_true",
                    help="yalniz birlestirme tablo denetimi (arbitraj/bedava terfi/sifir ucret => cikis 1)")
    ap.add_argument("--fusion-full", action="store_true", help="birlestirme: 57 ekipmanin tamami icin adim tablosu")
    ap.add_argument("--sink-scenarios", action="store_true",
                    help="yalniz altin sink senaryolari (karar 3): mevcut sink toplami, acik, 4 aday ve paketler (2026-10-17)")
    args = ap.parse_args()

    if args.check_fusion:
        errs, warns, _rows, _rules = check_fusion()
        for w in warns:
            print("UYARI:", w)
        for e in errs:
            print("HATA:", e)
        print("Birlestirme denetimi " + ("BASARISIZ." if errs else "temiz."))
        sys.exit(1 if errs else 0)

    out = pm.simulate(80 - 1)
    income_by_level = {}
    kills_by_level = {}
    floored = 0
    for lv, _n, _xpk, knq, kills, _qx, _gx, grind in out:
        if kills is None:
            continue
        if grind == 0 and kills < args.min_kill_frac * knq:
            kills = args.min_kill_frac * knq
            floored += 1
        kills_by_level[lv] = kills
        gold_kills = kills * avg_gold(lv)
        quest_here = sum(g for (t, c, g, req) in quest_gold.values()
                         if (c if t == "ReachLevel" else req) == lv)
        # Altin bagisi (MMOQuestComponent::GoldDonationMinTarget): buyuk HaveGold gorevleri teslimde altini dusurur (sink).
        donated_here = sum(c for (t, c, g, req) in quest_gold.values() if t == "HaveGold" and c >= GOLD_DONATION_MIN and req == lv)
        income_by_level[lv] = gold_kills + quest_here - donated_here

    if args.sink_scenarios:
        # Yalniz sink senaryolari (normal cikti degismez). Kademe/bant kurulumu asagidakiyle ayni.
        s_tiers = {}
        for it in items:
            s_tiers.setdefault(it["lv"], []).append(it)
        s_lvls = sorted(s_tiers)
        s_bands = [(tl, (s_lvls[i + 1] if i + 1 < len(s_lvls) else 80) - 1,
                    sum(buy_price(p) + upgrade_cost(args.upgrade_to, p["lv"]) for p in s_tiers[tl]))
                   for i, tl in enumerate(s_lvls)]
        sys.exit(1 if sink_report(income_by_level, kills_by_level, s_bands, s_tiers, args.upgrade_to) else 0)

    tiers = {}
    for it in items:
        tiers.setdefault(it["lv"], []).append(it)
    tier_lvls = sorted(tiers)
    print(f"Kademe: 4 parcanin alisi + her parca +{args.upgrade_to} yukseltme (esya seviyesiyle olceklenir: Lv1 {upgrade_cost(args.upgrade_to, 1)}, Lv30 {upgrade_cost(args.upgrade_to, 30)}, Lv78 {upgrade_cost(args.upgrade_to, 78)} altin/parca).")
    print("Gelir: oldurme x ort. mob altini + gorev altin odulleri - Q_Gold5000+ bagislari (Q_Gold* altin odulu 0; bagis teslimde dusulur).")
    print(f"Grind=0 tabani: {floored} seviyede oldurme = {args.min_kill_frac:g} x gorevsiz oldurme varsayildi (--min-kill-frac).")
    print(f"{'kademe Lv':>9} {'aralik':>9} {'gelir':>10} {'gider':>10} {'gelir/gider':>11}  parca sayisi")
    for i, tl in enumerate(tier_lvls):
        nxt = tier_lvls[i + 1] if i + 1 < len(tier_lvls) else 80
        # Bu kademeden bir sonrakine kadar kazanilan gelir, bu kademenin giderini karsilar mi?
        income = sum(v for lv, v in income_by_level.items() if tl <= lv < nxt)
        parts = tiers[tl]
        cost = sum(buy_price(p) + upgrade_cost(args.upgrade_to, p["lv"]) for p in parts)
        ratio = income / cost if cost else float("inf")
        print(f"{tl:>9} {tl:>4}-{nxt - 1:<4} {income:>10.0f} {cost:>10.0f} {ratio:>11.1f}  {len(parts)}")
    tot_income = sum(v for lv, v in income_by_level.items() if lv <= 30)
    tot_cost30 = sum(buy_price(p) + upgrade_cost(args.upgrade_to, p["lv"]) for tl in tier_lvls if tl <= 30 for p in tiers[tl])
    print(f"\nLv1->30 toplam gelir ~{tot_income:.0f}, Lv<=30 kademelerinin toplam gideri ~{tot_cost30:.0f} (oran {tot_income / tot_cost30:.1f}x)")
    print("Yorum: oran >>1 ise altin birikir (sink yetersiz); <1 ise kit. Q_Gold5000+ gorevleri altini bagistir (sink); Q_Gold500/1000 birikimdir.")

    bands = []
    for i, tl in enumerate(tier_lvls):
        nxt = tier_lvls[i + 1] if i + 1 < len(tier_lvls) else 80
        cost = sum(buy_price(p) + upgrade_cost(args.upgrade_to, p["lv"]) for p in tiers[tl])
        bands.append((tl, nxt - 1, cost))
    new_items_report(income_by_level, kills_by_level, bands)
    if fusion_report(income_by_level, kills_by_level, bands, tiers, args.fusion_full):
        sys.exit(1)


# ---------------------------------------------------------------------------
# Yeni kalemler (2026-10-11): sifaci, basarim, gunluk gorev, posta
# ---------------------------------------------------------------------------

def _must(pattern, text, what):
    m = re.search(pattern, text, re.S)
    if not m:
        sys.exit(f"HATA: {what} ayristirilamadi (kaynak formati degisti mi? EconomyModel.py yeni kalemler).")
    return m


def load_new_items():
    healer = (PUB / "World" / "MMOHealerNPC.h").read_text(encoding="utf-8", errors="replace")
    daily = (SRC / "Components" / "MMODailyComponent.cpp").read_text(encoding="utf-8", errors="replace")
    mail_h = (PUB / "Components" / "MMOMailComponent.h").read_text(encoding="utf-8", errors="replace")
    store_h = (PUB / "Mail" / "MMOMailStore.h").read_text(encoding="utf-8", errors="replace")
    ach = (SRC / "Achievements" / "MMOAchievementDatabase.cpp").read_text(encoding="utf-8", errors="replace")

    c = {}
    c["heal_base"] = int(_must(r"int32\s+HealCost\s*=\s*(\d+)\s*;", healer, "HealCost tabani").group(1))
    # (L * L * N) / D (kare) veya (L * L * L * N) / D (kup, karar 3 P1 A1): "L *" sayisi = us.
    m = _must(r"ComputeHealCost.*?\(\s*((?:L\s*\*\s*)+)(\d+)\s*\)\s*/\s*(\d+)", healer, "ComputeHealCost katsayisi")
    c["heal_pow"], c["heal_num"], c["heal_den"] = m.group(1).count("L"), int(m.group(2)), int(m.group(3))

    body = _must(r"void UMMODailyComponent::BuildBounties\((.*?)\n\}", daily, "BuildBounties govdesi").group(1)
    c["daily_gold_k"] = int(_must(r"RewardGold\s*=\s*(\d+)\s*\*\s*L\s*\*\s*Mult", body, "gunluk altin katsayisi").group(1))
    c["daily_xp_pct"] = int(_must(r"ExpToNextLevel\)\)\s*\*\s*(\d+)\s*/\s*100", body, "gunluk XP yuzdesi").group(1))
    c["daily_rows"] = re.findall(r"Make\(EMMODailyKind::(\w+),\s*([^,]+?),\s*(\d+)\);", body)
    if not c["daily_rows"]:
        sys.exit("HATA: BuildBounties Make(...) satirlari ayristirilamadi.")
    c["daily_max"] = int(_must(r"DailyMaxReward\s*=\s*(\d+)", daily, "DailyMaxReward").group(1))

    c["mail_flat"] = int(_must(r"FlatFee\s*=\s*(\d+)", mail_h, "posta FlatFee").group(1))
    c["mail_pct"] = int(_must(r"Gold\s*\*\s*(\d+)\s*\+\s*99\)\s*/\s*100", mail_h, "posta yuzdesi").group(1))
    c["mail_max"] = int(_must(r"MaxGoldPerMail\s*=\s*(\d+)", store_h, "MaxGoldPerMail").group(1))

    c["ach"] = [(i, k, t, int(n), int(g)) for i, k, t, n, g in re.findall(
        r'MakeAch\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("[^"]*"\),\s*K::(\w+),\s*TEXT\("([^"]*)"\),\s*(\d+),\s*(\d+)', ach)]
    if not c["ach"]:
        sys.exit("HATA: MMOAchievementDatabase.cpp MakeAch satirlari ayristirilamadi.")
    return c


def new_items_report(income_by_level, kills_by_level, bands):
    c = load_new_items()

    def heal_cost(lv):  # AMMOHealerNPC::ComputeHealCost(HealCost, Level)
        l1 = max(0, lv - 1)
        return max(0, c["heal_base"]) + (l1 ** c["heal_pow"] * c["heal_num"]) // c["heal_den"]

    def daily_target(expr, lv):
        expr = expr.strip()
        m = re.fullmatch(r"(\d+)\s*\+\s*L\s*/\s*(\d+)", expr)
        if m:
            return int(m.group(1)) + max(1, lv) // int(m.group(2))
        return int(expr) if expr.isdigit() else None

    def daily_gold(lv):  # 3 gorevin hepsi tamamlanirsa
        return sum(c["daily_gold_k"] * max(1, lv) * int(mult) for _k, _t, mult in c["daily_rows"])

    def daily_xp(lv):
        base = max(1, pm.need(lv) * c["daily_xp_pct"] // 100)
        return sum(min(base * int(mult), c["daily_max"]) for _k, _t, mult in c["daily_rows"])

    def mail_fee(g):  # UMMOMailComponent::FeeFor
        return c["mail_flat"] + ((g * c["mail_pct"] + 99) // 100 if g > 0 else 0)

    quests_at = Counter(q["req"] for q in pm.quests if not q["id"].startswith("Q_Gold"))
    hours = {lv: (k * KILL_SEC + quests_at.get(lv, 0) * QUEST_SEC) / 3600.0 for lv, k in kills_by_level.items()}
    levels = sorted(income_by_level)

    # Basarim acilma seviyesi (model): seviye = o seviye; boss = arketip seviyesi; oldurme/gorev = kumulatif sayac.
    mob_lv = {m["id"]: m["lv"] for m in pm.mobs}
    avail = lambda q: q["count"] if q["type"] == "ReachLevel" else q["req"]
    cum_k, cum_q, kill_at, quest_at = 0.0, 0, {}, {}
    for lv in levels:
        cum_k += kills_by_level[lv]
        cum_q += sum(1 for q in pm.quests if avail(q) == lv)
        kill_at[lv], quest_at[lv] = cum_k, cum_q
    ach_at, ach_never = Counter(), []
    for aid, kind, tid, n, g in c["ach"]:
        if g <= 0:
            continue
        lv = None
        if kind == "ReachLevel":
            lv = n - 1  # L -> L+1 satiri seviye L+1'e ulasmayi verir; 80'e ulasma = satir 79
        elif kind == "KillMob":
            lv = mob_lv.get(tid)
        elif kind == "KillTotal":
            lv = next((x for x in levels if kill_at[x] >= n), None)
        elif kind == "QuestsDone":
            lv = next((x for x in levels if quest_at[x] >= n), None)
        if lv is None or lv > levels[-1]:
            ach_never.append((aid, g))
        else:
            ach_at[max(lv, levels[0])] += g
    ach_total = sum(g for *_x, g in c["ach"])

    def band_sum(f, a, b):
        return sum(f(lv) for lv in levels if a <= lv <= b)

    print("\n=== Yeni kalemler (2026-10-11): sifaci / basarim / gunluk gorev / posta ===")
    print(f"Koddan [Kesin]: sifaci {c['heal_base']} + {c['heal_num']}/{c['heal_den']} x (Lv-1)^{c['heal_pow']} (Lv1 {heal_cost(1)}, Lv30 {heal_cost(30)}, "
          f"Lv60 {heal_cost(60)}, Lv80 {heal_cost(80)}); gunluk {len(c['daily_rows'])} gorev, altin {c['daily_gold_k']} x L x carpan "
          f"({'/'.join(m for *_x, m in c['daily_rows'])}), XP esigin %{c['daily_xp_pct']}'si x carpan; posta {c['mail_flat']} + eke "
          f"%{c['mail_pct']} (tavan {c['mail_max']}); basarim {len(c['ach'])} satir, altin toplami {ach_total}.")
    print(f"Varsayim [Tahmin]: oldurme {KILL_SEC:g} sn, gorev {QUEST_SEC:g} sn; sifaci = olum basina 1 tam sifa, "
          f"{DEATHS_PER_HOUR[0]}-{DEATHS_PER_HOUR[-1]} olum/sa (dogusta can zaten dolar: sifaci ZORUNLU degil, bu bir ust sinirdir); "
          f"oyun {PLAY_HOURS_PER_DAY[0]}-{PLAY_HOURS_PER_DAY[-1]} sa/gun, 3 gunluk gorevin hepsi biter.")
    tot_h = sum(hours.values())
    print(f"Model suresi Lv1->80 ~{tot_h:.1f} sa (grind=0 tabani dahil; TimelineModel'den biraz uzun) -> "
          + ", ".join(f"{h} sa/gun: {tot_h / h:.1f} gun" for h in PLAY_HOURS_PER_DAY) + ".")

    h_lo, h_mid, h_hi = PLAY_HOURS_PER_DAY
    print("\n(a) Gunluk gorev: set altini (L x 60) ve XP'si. %1sa = set altini / bandin 1 saatlik geliri;"
          f" bant payi = gunluk altin / bant geliri ({h_hi}-{h_lo} sa/gun); XP payi = gunluk XP / bant esik toplami ({h_hi}-{h_lo} sa/gun).")
    print(f"{'aralik':>9} {'sure sa':>8} {'gelir/sa':>10} {'set altin':>11} {'%1sa':>6} {'altin payi':>12} {'set XP %esik':>12} {'XP payi':>12}  bayrak")
    for a, b, _cost in bands:
        inc = band_sum(lambda lv: income_by_level[lv], a, b)
        hrs = band_sum(lambda lv: hours[lv], a, b)
        inc_h = inc / hrs if hrs else 0.0
        need_b = band_sum(pm.need, a, b)
        g_rng = [band_sum(lambda lv: hours[lv] / h * daily_gold(lv), a, b) / inc * 100 if inc else 0.0 for h in (h_hi, h_lo)]
        x_rng = [band_sum(lambda lv: hours[lv] / h * daily_xp(lv), a, b) / need_b * 100 for h in (h_hi, h_lo)]
        x_mid = band_sum(lambda lv: hours[lv] / h_mid * daily_xp(lv), a, b) / need_b * 100
        set_g = f"{daily_gold(a)}-{daily_gold(b)}" if a != b else f"{daily_gold(a)}"
        pct1h = daily_gold((a + b) // 2) / inc_h * 100 if inc_h else 0.0
        set_xp = daily_xp(a) / pm.need(a) * 100
        flag = []
        if pct1h > DAILY_GOLD_PCT_1H[1]:
            flag.append("altin ASIRI")
        elif pct1h < DAILY_GOLD_PCT_1H[0]:
            flag.append("altin etkisiz")
        if x_mid > DAILY_XP_BAND_PCT[1]:
            flag.append("XP ASIRI")
        elif x_mid < DAILY_XP_BAND_PCT[0]:
            flag.append("XP etkisiz")
        print(f"{a:>4}-{b:<4} {hrs:>8.2f} {inc_h:>10.0f} {set_g:>11} {pct1h:>5.0f}% {g_rng[0]:>5.1f}-{g_rng[1]:<5.1f}% "
              f"{set_xp:>11.1f}% {x_rng[0]:>5.1f}-{x_rng[1]:<5.1f}%  {', '.join(flag)}")
    print(f"Esikler [Tahmin]: altin %1sa <{DAILY_GOLD_PCT_1H[0]:g} etkisiz / >{DAILY_GOLD_PCT_1H[1]:g} asiri; "
          f"XP payi @{h_mid} sa/gun <{DAILY_XP_BAND_PCT[0]:g}% etkisiz / >{DAILY_XP_BAND_PCT[1]:g}% asiri. "
          f"Hedefler: {', '.join(f'{k} {daily_target(t, 1)}' + ('+L/' + t.split('/')[-1].strip() if '/' in t else '') for k, t, _m in c['daily_rows'])}.")

    d_lo, d_mid, d_hi = DEATHS_PER_HOUR
    print(f"\n(b) Sifaci: bedel, kac oldurmenin altini (ort. mob altini), {d_lo}/{d_hi} olum/sa'te saatlik gelirin yuzdesi; hedef %{HEAL_TARGET_PCT[0]:g}-{HEAL_TARGET_PCT[1]:g} [Tahmin].")
    print(f"{'aralik':>9} {'bedel':>11} {'=oldurme':>9} {f'%gelir@{d_lo}':>10} {f'%gelir@{d_hi}':>10}  bayrak")
    for a, b, _cost in bands:
        inc = band_sum(lambda lv: income_by_level[lv], a, b)
        hrs = band_sum(lambda lv: hours[lv], a, b)
        heal_h = band_sum(lambda lv: hours[lv] * heal_cost(lv), a, b)  # 1 olum/sa icin bant toplami
        p_lo, p_hi = (heal_h * d / inc * 100 if inc else 0.0 for d in (d_lo, d_hi))
        mid = (a + b) // 2
        kills_eq = heal_cost(mid) / avg_gold(mid) if avg_gold(mid) else 0.0
        flag = "ASIRI" if p_lo > HEAL_TARGET_PCT[1] else ("YETERSIZ" if p_hi < HEAL_TARGET_PCT[0] else
                                                            ("aralikta" if p_lo >= HEAL_TARGET_PCT[0] and p_hi <= HEAL_TARGET_PCT[1] else "kismen"))
        cost_s = f"{heal_cost(a)}-{heal_cost(b)}" if a != b else f"{heal_cost(a)}"
        print(f"{a:>4}-{b:<4} {cost_s:>11} {kills_eq:>9.1f} {p_lo:>9.1f}% {p_hi:>9.1f}%  {flag}")

    print(f"\n(c) Posta ucreti (oyuncular arasi transfer net 0; ucret sink): ucret = {c['mail_flat']} + ceil(%{c['mail_pct']} x ek).")
    print("  " + "  ".join(f"{g}: {mail_fee(g)} (%{mail_fee(g) / g * 100:.1f})" for g in (100, 1000, 10000, 100000, c["mail_max"])))
    one_h = []
    for a, b, _cost in bands:
        inc = band_sum(lambda lv: income_by_level[lv], a, b)
        hrs = band_sum(lambda lv: hours[lv], a, b)
        g = min(c["mail_max"], int(inc / hrs)) if hrs else 0
        if g > 0:
            one_h.append(f"{a}-{b}: {g} -> %{mail_fee(g) / g * 100:.1f}")
    print("  1 saatlik gelir postalanirsa: " + "; ".join(one_h))
    print("  Sink hacmi = posta sayisi x ucret; posta sikligi modelde yok ([MMOMETRIC] ev=gold src=posta gerekir).")

    print(f"\n(d) Basarim altini (tek seferlik, toplam {ach_total}): acildigi bant ve bant gelirine orani (model kumulatif oldurme/gorev).")
    print(f"{'aralik':>9} {'basarim':>8} {'%gelir':>7}")
    for a, b, _cost in bands:
        inc = band_sum(lambda lv: income_by_level[lv], a, b)
        g = band_sum(lambda lv: ach_at.get(lv, 0), a, b)
        print(f"{a:>4}-{b:<4} {g:>8} {g / inc * 100 if inc else 0.0:>6.1f}%")
    tot_inc = sum(income_by_level.values())
    print(f"  Lv1->80 model geliri ~{tot_inc:.0f}; basarim toplami bunun %{ach_total / tot_inc * 100:.2f}'i. "
          f"Leveling'de acilmayan (model): {', '.join(f'{i} ({g})' for i, g in ach_never) or 'yok'} "
          f"(Lv80'e kadar kumulatif oldurme ~{cum_k:.0f}).")

    print(f"\n(e) Net etki: eski gelir/gider ile yeni (gelir + gunluk + basarim - sifaci) / gider. "
          f"orta = {h_mid} sa/gun + {d_mid} olum/sa; aralik = [{h_hi} sa/gun + {d_hi} olum/sa, {h_lo} sa/gun + {d_lo} olum/sa].")
    print(f"{'aralik':>9} {'eski':>6} {'orta':>6} {'aralik':>13}")

    def net(a, b, h, d):
        return (band_sum(lambda lv: income_by_level[lv] + hours[lv] / h * daily_gold(lv) + ach_at.get(lv, 0)
                         - hours[lv] * d * heal_cost(lv), a, b))
    for a, b, cost in bands:
        old = band_sum(lambda lv: income_by_level[lv], a, b) / cost
        mid_r = net(a, b, h_mid, d_mid) / cost
        lo_r, hi_r = net(a, b, h_hi, d_hi) / cost, net(a, b, h_lo, d_lo) / cost
        print(f"{a:>4}-{b:<4} {old:>6.1f} {mid_r:>6.1f} {lo_r:>6.1f}-{hi_r:<6.1f}")
    print("Not: gunluk XP ilerlemeyi biraz kisaltir (sure ve dolayisiyla gelir dusmez sayildi); posta ve iksir dahil degil.")


# ---------------------------------------------------------------------------
# Birlestirme (nadirlik fuzyonu) ekonomisi (2026-10-17, docs/DESIGN_FUSION.md D0)
# 3 ayni esya (ayni nadirlik, +0) -> bir ust nadirlik; ucret = GetBuyPrice x FusionFeeMult[girdi nadirligi].
# Tum sabitler kaynaktan PARSE edilir; asagidaki sabitler yalniz model varsayimidir [TAHMIN].
# ---------------------------------------------------------------------------

ABSORB_MEANINGFUL_PCT = 10.0       # [TAHMIN] sink "anlamli": bandin gelir-gider fazlasinin >= %10'u
FUSION_ALT_MULTS = ((0.25, 0.5, 1.0), (1.0, 2.0, 4.0), (2.0, 4.0, 8.0))  # onerilen alternatifler (kodda olan ayrica okunur)
DROP_SAMPLE_LEVELS = (10, 16, 25, 33, 42, 51, 58, 66, 72, 79)
MC_RUNS = 3000                     # ganimet Monte Carlo tekrar sayisi (tohum sabit: tekrarlanabilir)
MC_SEED = 20261017
FUSION_FILES_NOTE = "MMOFusion.cpp/.h"


def _f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def _rint(x):  # FMath::RoundToInt
    return int(math.floor(x + 0.5))


def load_fusion_rules():
    """Fiyat/nadirlik/dusme/birlestirme sabitlerini kaynak dosyalardan okur (elle girilmis sayi yok)."""
    r = {}
    types_h = (PUB / "Items" / "MMOItemTypes.h").read_text(encoding="utf-8", errors="replace")
    enum = _must(r"enum class EMMOItemRarity[^{]*\{(.*?)\}", types_h, "EMMOItemRarity enum").group(1)
    r["rar"] = [n for n in re.findall(r"(\w+)\s+UMETA", enum) if n != "MAX"]

    m = _must(r"GetBuyPrice\(const FMMOItemDef& Def\)\s*\{.*?RoundToInt\(\(Def\.BaseAttack \+ Def\.BaseDefense\) \* ([\d.]+)f\)"
              r"\s*\+\s*Def\.RequiredLevel \* (\d+)\s*\+\s*(\d+);", item_src, "GetBuyPrice formulu")
    r["buy_k"], r["buy_lv"], r["buy_c"] = float(m.group(1)), int(m.group(2)), int(m.group(3))
    m = _must(r"GetSellPrice\(const FMMOItemDef& Def, int32 Plus\)\s*\{.*?GetBuyPrice\(Def\) \* ([\d.]+)f \* \(1\.f \+ ([\d.]+)f \* Plus\)",
              item_src, "GetSellPrice formulu")
    r["sell_k"] = float(m.group(1))

    body = _must(r"GetRarityMultiplier\(EMMOItemRarity Rarity\)\s*\{(.*?)\n\}", item_src, "GetRarityMultiplier").group(1)
    explicit = {n: float(v) for n, v in re.findall(r"case EMMOItemRarity::(\w+):\s*return ([\d.]+)f;", body)}
    dflt = float(_must(r"default:\s*return ([\d.]+)f;", body, "GetRarityMultiplier default").group(1))
    r["rar_mult"] = [explicit.get(n, dflt) for n in r["rar"]]

    body = _must(r"RollRarity\(\)\s*\{(.*?)\n\}", item_src, "RollRarity").group(1)
    cuts = [float(c) for c in re.findall(r"R < ([\d.]+)f\)", body)]
    names = re.findall(r"return EMMOItemRarity::(\w+);", body)
    if len(names) != len(cuts) + 1:
        sys.exit("HATA: RollRarity ayristirilamadi (kaynak formati degisti mi?).")
    probs, prev = {}, 0.0
    for n, c in zip(names, cuts + [1.0]):
        probs[n] = c - prev
        prev = c
    r["roll"] = [probs.get(n, 0.0) for n in r["rar"]]

    mob_h = (PUB / "Characters" / "MMOMob.h").read_text(encoding="utf-8", errors="replace")
    mob_cpp = (SRC / "Characters" / "MMOMob.cpp").read_text(encoding="utf-8", errors="replace")
    r["equip_chance"] = float(_must(r"float EquipDropChance = ([\d.]+)f;", mob_h, "EquipDropChance").group(1))
    r["plus_chance"] = float(_must(r"FRand\(\) < ([\d.]+)f\) \? 1 : 0", mob_cpp, "normal mob Plus sansi").group(1))
    r["pool_slack"] = int(_must(r"It\.RequiredLevel <= MobLevel \+ (\d+)", mob_cpp, "normal mob esya havuzu").group(1))

    # Birlestirme sabitleri: D1 dosyalari commit'siz/eksik olabilir; o zaman ucret denetimi atlanir (UYARI).
    fcpp = SRC / "Items" / "MMOFusion.cpp"
    fh = PUB / "Items" / "MMOFusion.h"
    r["fee_mult"], r["cap"], r["inputs"] = None, None, None
    if fcpp.exists() and fh.exists():
        ft = fcpp.read_text(encoding="utf-8", errors="replace")
        ht = fh.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"FusionFeeMult\[\]\s*=\s*\{([^}]*)\}", ft)
        if m:
            r["fee_mult"] = tuple(float(x) for x in re.findall(r"[\d.]+", m.group(1)))
        m = re.search(r"MaxOutputRarity\s*=\s*EMMOItemRarity::(\w+)", ht)
        if m and m.group(1) in r["rar"]:
            r["cap"] = r["rar"].index(m.group(1))
        m = re.search(r"InputCount\s*=\s*(\d+)", ht)
        if m:
            r["inputs"] = int(m.group(1))
    return r


def load_all_items():
    """Tum ekipman (bVendorSold=false dahil)."""
    res = []
    for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*S::(\w+),\s*([\d.]+)f,\s*([\d.]+)f,\s*(\d+)', item_src):
        nxt = item_src.find("Table.Add(", m.end())
        vendor = not re.search(r"bVendorSold\s*=\s*false", item_src[m.end(): nxt if nxt > 0 else len(item_src)])
        res.append(dict(id=m.group(1), slot=m.group(2), atk=float(m.group(3)), df=float(m.group(4)),
                        lv=int(m.group(5)), vendor=vendor))
    calls = len(re.findall(r'MakeItem\(TEXT\(', item_src))
    return res, calls


def f_buy(rules, it):  # FMMOItemDatabase::GetBuyPrice
    return _rint(_f32(_f32(it["atk"] + it["df"]) * _f32(rules["buy_k"]))) + it["lv"] * rules["buy_lv"] + rules["buy_c"]


def f_sell(rules, it, rar_idx):  # GetSellPrice(Plus 0) sonra GetRarityMultiplier (MMOInventoryComponent.cpp: SellEquipment)
    s0 = _rint(_f32(f_buy(rules, it) * _f32(rules["sell_k"])))
    return _rint(_f32(s0 * _f32(rules["rar_mult"][rar_idx])))


def fusion_steps(rules, it, mults, cap):
    """Bir ekipman icin her birlestirme adimi: girdi nadirligi 0..cap-1 -> +1."""
    n_in = rules["inputs"] or 3
    b = f_buy(rules, it)
    steps = []
    for s in range(cap):
        s_in, s_out = f_sell(rules, it, s), f_sell(rules, it, s + 1)
        fee = _rint(b * mults[s]) if s < len(mults) else 0
        steps.append(dict(frm=s, in3=n_in * s_in, out=s_out, fee=fee, sink=n_in * s_in + fee - s_out))
    return b, steps


def chain_fee(rules, b, mults, cap):
    """Girdi tamamen Siradan, tavan nadirlige kadar toplam ucret (27 -> 1 icin 9/3/1 birlestirme)."""
    n_in = rules["inputs"] or 3
    return sum(n_in ** (cap - 1 - s) * _rint(b * mults[s]) for s in range(cap))


def check_fusion():
    """(hatalar, uyarilar, satirlar, kurallar). Hata: bedava terfi / sifir ucret / satis arbitraji / Efsanevi uretimi."""
    rules = load_fusion_rules()
    its, calls = load_all_items()
    errs, warns, rows = [], [], []
    rar = rules["rar"]
    n_in = rules["inputs"] or 3

    if not its or calls != len(its):
        errs.append(f"MakeItem ayristirma uyumsuz: {calls} cagri, {len(its)} ayristirildi (EconomyModel kalibi bozuk)")
    ids = [i["id"] for i in its]
    if len(set(ids)) != len(ids):
        errs.append("Esya kimlikleri yinelenmis")
    for it in its:
        if it["vendor"] and f_buy(rules, it) != buy_price(it):
            errs.append(f"{it['id']}: f_buy {f_buy(rules, it)} != buy_price {buy_price(it)} (formul kaynaktan ayrildi)")
    mu = rules["rar_mult"]
    for a in range(len(mu) - 1):
        if not mu[a + 1] > mu[a]:
            errs.append(f"Nadirlik carpani artmiyor: {rar[a]} {mu[a]} -> {rar[a + 1]} {mu[a + 1]}")
    if rules["fee_mult"] is None or rules["cap"] is None:
        warns.append(f"{FUSION_FILES_NOTE} yok/okunamadi: ucret ve tavan denetimi ATLANDI (D1 commit'ten sonra tam calisir)")
        return errs, warns, rows, rules

    cap, mults = rules["cap"], rules["fee_mult"]
    if n_in < 2:
        errs.append(f"InputCount {n_in} < 2")
    if cap >= rar.index("Legendary"):
        errs.append(f"MaxOutputRarity {rar[cap]}: Efsanevi birlestirmeyle uretilmemeli (karar A1)")
    if cap < rar.index("Epic"):
        warns.append(f"MaxOutputRarity {rar[cap]} < Epic (tasarim varsayilani Destansi)")
    if len(mults) < cap:
        errs.append(f"FusionFeeMult {len(mults)} eleman, {cap} adim gerekir (FeeFor 0 doner => InternalError)")
        return errs, warns, rows, rules
    for s, mv in enumerate(mults[:cap]):
        if not mv > 0:
            errs.append(f"FusionFeeMult[{s}] = {mv} <= 0 (bedava terfi)")

    for it in its:
        b, steps = fusion_steps(rules, it, mults, cap)
        rows.append(dict(it=it, b=b, steps=steps))
        for st in steps:
            tag = f"{it['id']} {rar[st['frm']]}->{rar[st['frm'] + 1]}"
            if st["fee"] <= 0:
                errs.append(f"{tag}: ucret {st['fee']} <= 0 (sifir ucret/bedava terfi)")
            if st["sink"] <= 0:
                errs.append(f"{tag}: satis arbitraji/bedava terfi: {n_in} girdi satisi {st['in3']} + ucret {st['fee']} <= cikti satisi "
                            f"{st['out']} (kar {-st['sink']})")
        # Zincir: r0 nadirliginde baslayip tavana kadar birlestir ve sat (ganimetten gelen girdi, alis yok).
        for r0 in range(cap):
            k = cap - r0
            fees = sum(n_in ** (k - 1 - j) * steps[r0 + j]["fee"] for j in range(k))
            profit = steps[cap - 1]["out"] - n_in ** k * f_sell(rules, it, r0) - fees
            if profit > 0:
                errs.append(f"{it['id']}: zincir {rar[r0]}->{rar[cap]} satis kari {profit} > 0")
        # Saticidan alip birlestir ve sat (yalniz saticida sunulanlar).
        if it["vendor"]:
            fees = chain_fee(rules, b, mults, cap)
            profit = steps[cap - 1]["out"] - n_in ** cap * b - fees
            if profit > 0:
                errs.append(f"{it['id']}: saticidan al + {n_in ** cap}->1 birlestir + sat kari {profit} > 0")
    return errs, warns, rows, rules


def exp_draws_to_triple(n_types, need=3):
    """n_types esit olasilikli tur arasinda herhangi birinden `need` adet gelene kadar beklenen cekim sayisi (tam, Fraction)."""
    from fractions import Fraction
    base = [Fraction(1, math.factorial(j)) for j in range(need)]
    poly = [Fraction(1)]
    for _ in range(n_types):
        nxt = [Fraction(0)] * (len(poly) + need - 1)
        for i, c in enumerate(poly):
            for j, d in enumerate(base):
                nxt[i + j] += c * d
        poly = nxt
    exp = Fraction(0)
    for k in range(0, (need - 1) * n_types + 1):
        exp += math.factorial(k) * poly[k] / Fraction(n_types ** k)
    return float(exp)


def sim_target_epic(rules, n_types, cap, need=3, runs=MC_RUNS, seed=MC_SEED):
    """Tek bir hedef esya icin: ilk Destansi (dusme ya da 3'lu birlestirme) icin ortalama oldurme (Monte Carlo)."""
    rng = random.Random(seed + n_types)
    p_x = rules["equip_chance"] / n_types
    cum, acc = [], 0.0
    for p in rules["roll"]:
        acc += p
        cum.append(acc)
    lp = math.log(1.0 - p_x)
    total = 0
    for _ in range(runs):
        c = [0] * len(rules["rar"])
        kills = 0
        while True:
            kills += 1 + int(math.log(1.0 - rng.random()) / lp)
            u = rng.random()
            ridx = next((i for i, v in enumerate(cum) if u < v), len(cum) - 1)
            plus0 = rng.random() >= rules["plus_chance"]
            if ridx >= cap:
                break
            if not plus0:
                continue
            c[ridx] += 1
            done = False
            while c[ridx] >= need:
                c[ridx] = 0
                ridx += 1
                if ridx >= cap:
                    done = True
                    break
                c[ridx] += 1
            if done:
                break
        total += kills
    return total / runs


def fusion_report(income_by_level, kills_by_level, bands, tiers, full=False):
    errs, warns, rows, rules = check_fusion()
    rar = rules["rar"]
    print("\n=== Birlestirme (nadirlik fuzyonu) ekonomisi (2026-10-17, DESIGN_FUSION.md D0) [MODEL] ===")
    for w in warns:
        print("UYARI:", w)
    print(f"Koddan [Kesin]: alis = round((Atk+Def) x {rules['buy_k']:g}) + Lv x {rules['buy_lv']} + {rules['buy_c']}; "
          f"satis = round(round(alis x {rules['sell_k']:g}) x nadirlik carpani); nadirlik carpani "
          + "/".join(f"{n} {m:g}" for n, m in zip(rar, rules["rar_mult"])) + "; ganimet zari "
          + "/".join(f"{n} %{p * 100:g}" for n, p in zip(rar, rules["roll"])) + ".")
    if rules["fee_mult"] is None or rules["cap"] is None:
        print("Birlestirme sabitleri (ucret carpani/tavan) okunamadi; bolum burada biter.")
        return bool(errs)
    cap, mults = rules["cap"], rules["fee_mult"]
    n_in = rules["inputs"] or 3
    print(f"Birlestirme [Kesin, PLACEHOLDER]: {n_in} ayni esya (+0, ayni nadirlik) -> bir ust; tavan {rar[cap]}; "
          f"ucret = alis x {{" + ", ".join(f"{rar[i]} {m:g}" for i, m in enumerate(mults[:cap])) + "} (girdi nadirligine gore). "
          f"Ekipman sayisi {len(rows)} (saticida {sum(1 for r in rows if r['it']['vendor'])}).")

    # (a) Adim tablosu: 57 ekipmanin hepsi, her adimda
    print(f"\n(a) Adim ozeti ({len(rows)} ekipman; deger/B = o esyanin alis fiyatina oran; sink = {n_in} girdi satisi + ucret - cikti satisi)")
    print(f"{'adim':>20} {n_in}xgirdi-satis/B {'cikti-satis/B':>15} {'ucret/B':>9} {'net sink/B':>11} {'net sink altin (min..maks)':>28}")
    for s in range(cap):
        col = lambda f: [f(r["steps"][s], r["b"]) for r in rows]
        a, o, f_, k = col(lambda st, b: st["in3"] / b), col(lambda st, b: st["out"] / b), col(lambda st, b: st["fee"] / b), \
            col(lambda st, b: st["sink"] / b)
        g = [r["steps"][s]["sink"] for r in rows]
        print(f"{rar[s] + '->' + rar[s + 1]:>20} {min(a):>6.2f}-{max(a):<6.2f} {min(o):>7.2f}-{max(o):<6.2f} "
              f"{min(f_):>4.2f}-{max(f_):<4.2f} {min(k):>5.2f}-{max(k):<5.2f} {min(g):>12}..{max(g):<12}")
    best = max(((-st["sink"], r["it"]["id"], rar[st["frm"]]) for r in rows for st in r["steps"]), key=lambda t: t[0])
    print(f"Arbitraj (adim bazinda en iyi satis kari): {best[0]} altin ({best[1]} {best[2]}->): "
          + ("KAR VAR (HATA)" if best[0] > 0 else "kar yok, her adim net altin yok eder"))

    if full:
        hdr = f"{'esya':<22}{'yuva':<9}{'Lv':>3}{'sat':>4}{'alis':>7} |"
        for s in range(cap):
            hdr += f" {rar[s][:3]}->{rar[s + 1][:3]}: girdi/cikti/ucret/sink |"
        print("\n(a2) Tum ekipman (--fusion-full)")
        print(hdr)
        for r in sorted(rows, key=lambda r: (r["it"]["lv"], r["it"]["slot"], r["it"]["id"])):
            line = f"{r['it']['id']:<22}{r['it']['slot']:<9}{r['it']['lv']:>3}{'E' if r['it']['vendor'] else 'H':>4}{r['b']:>7} |"
            for st in r["steps"]:
                line += f" {st['in3']:>6}/{st['out']:>6}/{st['fee']:>6}/{st['sink']:>6} |"
            print(line)
        print("(sat: E = saticida, H = saticida degil)")

    # (b) 27 -> 1
    k_in = n_in ** cap
    print(f"\n(b) {k_in} Siradan -> 1 {rar[cap]}: toplam ucret, kayip deger, satici yolu")
    print(f"{'esya':<20}{'Lv':>3}{'alis B':>8}{'toplam ucret':>13}{'ucret/B':>8}{'kayip deger':>12}{'satici yolu':>12}{'satici/B':>9}{'cikti satis':>12}{'+5 yuk.':>9}")
    show = {}
    for r in sorted(rows, key=lambda r: r["it"]["lv"]):
        if r["it"]["vendor"] and r["it"]["slot"] == "Weapon":
            show.setdefault(r["it"]["lv"], r)
    picks = [show[k] for k in sorted(show)][:1] + [show[k] for k in sorted(show) if k in (25, 51, 72, 78)]
    for r in picks:
        fee_t = chain_fee(rules, r["b"], mults, cap)
        lost = k_in * f_sell(rules, r["it"], 0) + fee_t - r["steps"][cap - 1]["out"]
        vend = k_in * r["b"] + fee_t
        print(f"{r['it']['id']:<20}{r['it']['lv']:>3}{r['b']:>8}{fee_t:>13}{fee_t / r['b']:>8.2f}{lost:>12}{vend:>12}{vend / r['b']:>9.2f}"
              f"{r['steps'][cap - 1]['out']:>12}{upgrade_cost(5, r['it']['lv']):>9}")
    fts = [chain_fee(rules, r["b"], mults, cap) / r["b"] for r in rows]
    print(f"  {len(rows)} ekipmanda toplam ucret/B: {min(fts):.2f}..{max(fts):.2f}; tum zincir kontrolu: "
          + ("temiz" if not errs else f"{len(errs)} HATA"))

    # (c) Ucret carpani alternatifleri: ayni tablo, farkli carpan
    code_label = "{" + ",".join(f"{m:g}" for m in mults[:cap]) + "}"
    variants = [(code_label + " (kod)", tuple(mults[:cap]))]
    for alt in FUSION_ALT_MULTS:
        if alt != tuple(mults[:cap]):
            variants.append(("{" + ",".join(f"{m:g}" for m in alt) + "}", alt))
    print(f"\n(c) Ucret carpani alternatifleri ({len(rows)} ekipman): toplam ucret = " + " + ".join(f"{n_in ** (cap - 1 - s)}x{{c{s + 1}}}" for s in range(cap))
          + " (alis B ile carpilir)")
    print(f"{'carpan':<18}{'toplam/B':>9}{'satici yolu/B':>14}{'min adim sink/B':>16}{'bedava terfi':>14}")
    for label, mv in variants:
        tot = sum(n_in ** (cap - 1 - s) * mv[s] for s in range(cap))
        min_sink = min(st["sink"] / r["b"] for r in rows for st in fusion_steps(rules, r["it"], mv, cap)[1])
        free = any(st["fee"] <= 0 or st["sink"] <= 0 for r in rows for st in fusion_steps(rules, r["it"], mv, cap)[1])
        print(f"{label:<18}{tot:>9.2f}{k_in + tot:>14.2f}{min_sink:>16.2f}{'VAR' if free else 'yok':>14}")

    # (d) Gelir fazlasini absorbe etme: bant basina 4 parcalik setin Destansi'ya cikarilmasi
    quests_at = Counter(q["req"] for q in pm.quests if not q["id"].startswith("Q_Gold"))
    hrs = lambda a, b: sum((kills_by_level[lv] * KILL_SEC + quests_at.get(lv, 0) * QUEST_SEC) / 3600.0
                           for lv in kills_by_level if a <= lv <= b)
    print(f"\n(d) Bant basina tek 4 parcalik set Siradan'dan {rar[cap]}'ya: ek gider / (bant geliri - bant gideri) = '%fazla'. "
          f"D = ganimetten gelen girdi (yalniz ucret), S = girdiyi saticidan al (({k_in}-1) x B + ucret, 1. parca zaten kademe giderinde)."
          f" Anlamli esik >= %{ABSORB_MEANINGFUL_PCT:g} [TAHMIN]; 'kit' = gelir <= gider (fazla yok).")
    head = f"{'aralik':>9} {'gelir':>10} {'gider':>9} {'fazla':>10} {'gelir/sa':>9}"
    for label, _mv in variants:
        head += f" | {label[:14]:>14} D%   S%"
    print(head)
    meaningful = {label: [0, 0] for label, _ in variants}
    tot_ds = {label: [0.0, 0.0, 0.0] for label, _ in variants}
    flat = []
    for a, b, cost in bands:
        inc = sum(income_by_level[lv] for lv in income_by_level if a <= lv <= b)
        surplus = inc - cost
        h = hrs(a, b)
        tl_parts = tiers[a]
        line = f"{a:>4}-{b:<4} {inc:>10.0f} {cost:>9.0f} {surplus:>10.0f} {inc / h if h else 0:>9.0f}"
        for label, mv in variants:
            d = sum(chain_fee(rules, f_buy(rules, p), mv, cap) for p in tl_parts)
            s = sum((k_in - 1) * f_buy(rules, p) + chain_fee(rules, f_buy(rules, p), mv, cap) for p in tl_parts)
            if surplus > 0:
                pd, ps = d / surplus * 100, s / surplus * 100
                meaningful[label][0] += pd >= ABSORB_MEANINGFUL_PCT
                meaningful[label][1] += ps >= ABSORB_MEANINGFUL_PCT
                tot_ds[label][0] += d
                tot_ds[label][1] += s
                tot_ds[label][2] += surplus
                fmt = lambda v: ">999" if v > 999 else f"{v:.1f}"
                line += f" | {'':>14} {fmt(pd):>4} {fmt(ps):>4}"
            else:
                line += f" | {'':>14} {'kit':>4} {'kit':>4}"
            if label == variants[0][0]:
                flat.append((a, b, d, s, inc / h if h else 0.0))
        print(line)
    print("Anlamli bant sayisi (D / S, esik %" + f"{ABSORB_MEANINGFUL_PCT:g}): "
          + "; ".join(f"{lbl} {v[0]}/{v[1]}" for lbl, v in meaningful.items()) + f" (toplam {len(bands)} bant).")
    print("Ust sinir (her bantta bir set Destansi'ya cikarilsa; fazlasi olan bantlar): toplam ek gider / toplam fazla = D | S: "
          + "; ".join(f"{lbl} %{v[0] / v[2] * 100:.1f} | %{v[1] / v[2] * 100:.1f}" for lbl, v in tot_ds.items() if v[2]))
    print("Kod carpani, saat esdegeri (ek gider / bandin 1 saatlik geliri): "
          + "; ".join(f"{a}-{b}: D {d / ih:.2f} sa, S {s / ih:.2f} sa" for a, b, d, s, ih in flat if ih))

    # (e) Ganimetten 3 ayni esya toplama beklentisi (yalniz normal mob zari; elit havuzu dahil DEGIL)
    all_items, _calls = load_all_items()
    chance, p_c = rules["equip_chance"], rules["roll"][0]
    p_up = sum(rules["roll"][cap:])
    rate = chance * p_c * (1.0 - rules["plus_chance"])
    print(f"\n(e) Ganimetten toplama (normal mob, kaynak: MMOMob.cpp zarlari): ekipman dusme sansi {chance:g}, esya havuzu = Lv <= mob Lv+{rules['pool_slack']} "
          f"(tum {len(all_items)} ekipman, esit olasilik), Siradan+0 sansi {p_c:g} x {1 - rules['plus_chance']:g} => oldurme basina "
          f"{rate:.4f} kullanilabilir Siradan; oldurme {KILL_SEC:g} sn [TAHMIN]; elit ganimeti ve havuz-disi kaynaklar yok ('veri yok').")
    print(f"{'Lv':>3} {'havuz N':>8} {'herhangi 3 ayni':>20} {'belirli esyadan 3':>20} {'belirli ' + rar[cap] + ' dusme':>20} "
          f"{'belirli ' + rar[cap] + ' (dusme+birl.)':>26}")
    print(f"{'':>3} {'':>8} {'oldurme (saat)':>20} {'oldurme (saat)':>20} {'oldurme (saat)':>20} {'oldurme (saat)':>26}")
    hf = KILL_SEC / 3600.0
    for lv in DROP_SAMPLE_LEVELS:
        n = sum(1 for i in all_items if i["lv"] <= lv + rules["pool_slack"])
        k_any = exp_draws_to_triple(n, n_in) / rate
        k_tgt = n_in * n / rate
        k_dir = n / (chance * p_up)
        k_mc = sim_target_epic(rules, n, cap, n_in)
        print(f"{lv:>3} {n:>8} {k_any:>11.0f} ({k_any * hf:>5.1f}) {k_tgt:>11.0f} ({k_tgt * hf:>5.1f}) {k_dir:>11.0f} ({k_dir * hf:>5.1f}) "
              f"{k_mc:>16.0f} ({k_mc * hf:>5.1f})")
    print("  Not: 'herhangi 3 ayni' havuzdaki HERHANGI bir esyadan (cogu dusuk kademe, oyuncu icin degersiz); 'belirli' = oyuncunun hedef esyasi. "
          "Dusme+birlestirme, belirli Destansi bekleyisini dusme-yalniz yola gore yaklasik yarilar ama yine tek parca icin onlarca-yuzlerce "
          "oldurme ve saatler surer (bant suresi: 'sure sa' sutunu, EconomyModel (a) tablosu). "
          f"MC: {MC_RUNS} tekrar, tohum {MC_SEED}.")
    for e in errs:
        print("HATA:", e)
    return bool(errs)


# ---------------------------------------------------------------------------
# Altin sink senaryolari (karar 3, 2026-10-17): python Scripts/EconomyModel.py --sink-scenarios
# Kodda olan sink'lerin bant bazinda toplami + 4 aday sink'in bant basina emme orani.
# Formul/sabitler kaynaktan PARSE edilir; asagidaki sabitler yalniz model varsayimidir [TAHMIN], oynanis olcumu DEGIL.
# ---------------------------------------------------------------------------

SINK_DEATHS = 4.0            # [TAHMIN] olum/sa (mevcut yeni-kalemler bolumunun orta degeri)
SINK_HEAL_BYPASS = 0.5       # [TAHMIN] sifa gerektiren olumlerin yarisi bedava dogus/iksirle atlatilir (sifaci zorunlu degil)
SINK_POTIONS_PER_HOUR = 30   # [TAHMIN] Buyuk Can Iksiri/sa
SINK_FIRST_LV = 16           # bu seviyeden once gelir ~ gider: aday sink'ler yalniz a >= 16 bantlarinda hesaplanir
SINK_TARGET_RATIO = 1.5      # [TAHMIN] hedef: gelir / tum sink <= 1.5 (gelirin >= %67'si harcanir)
SINK_KNEE = 30               # aday C: dik egri bu seviyeden sonra baslar
SINK_UPG_GAMMAS = (1.0, 2.0)  # aday C: maliyet x (1 + (Lv-30)/30)^gamma, Lv > 30
SINK_HEAL_CUBE = 0.09        # aday A1: taban + 0,09 x (Lv-1)^3 (DECISIONS_NEEDED 2026-10-09 onerisi)
SINK_HEAL_KILLS = 10         # aday A2: sifa bedeli = 10 x seviyedeki ort. mob altini
SINK_DONATE = ((0.5, 0.5), (1.0, 0.5))   # aday B: (bantin saatlik gelirinin kati, katilim orani) [TAHMIN]
SINK_OPTIONAL_GATES = (20, 40, 60)       # aday D: opsiyonel satin alimlarin acildigi seviyeler (tasarim varsayimi)
SINK_OPTIONAL_HOURS = 0.25               # aday D: her biri bandin 1 saatlik gelirinin bu kati
SINK_OPTIONAL_PART = 0.5                 # aday D: katilim orani [TAHMIN]


def load_sink_rules():
    r = {}
    db_h = (PUB / "Items" / "MMOItemDatabase.h").read_text(encoding="utf-8", errors="replace")
    r["max_plus"] = int(_must(r"MaxPlus\s*=\s*(\d+)", db_h, "MaxPlus").group(1))
    r["potion"] = int(_must(r"PotionPrice\s*=\s*(\d+)", db_h, "PotionPrice").group(1))
    cons = (SRC / "Items" / "MMOConsumableDatabase.cpp").read_text(encoding="utf-8", errors="replace")
    r["gpotion"] = int(_must(r'MakeConsumable\(TEXT\("GreaterHealthPotion"\),\s*TEXT\("[^"]*"\),\s*(\d+)', cons,
                             "GreaterHealthPotion fiyati").group(1))
    body = _must(r"GetUpgradeCost\(int32 CurrentPlus, int32 RequiredLevel\)\s*\{(.*?)\n\}", item_src, "GetUpgradeCost").group(1)
    if not re.search(r"50ll\s*\*\s*\(CurrentPlus \+ 1\)\s*\*\s*\(100ll \+ Lv \* Lv\)\s*/\s*100ll", body):
        sys.exit("HATA: GetUpgradeCost formulu degisti; EconomyModel.upgrade_cost ve sink_report guncellenmeli.")
    ch = _must(r"Chances\[MaxPlus\]\s*=\s*\{([^}]*)\}", item_src, "Chances").group(1)
    bu = _must(r"BurnChances\[MaxPlus\]\s*=\s*\{([^}]*)\}", item_src, "BurnChances").group(1)
    r["chance"] = [int(x) for x in re.findall(r"\d+", ch)]
    r["burn"] = [int(x) for x in re.findall(r"\d+", bu)]
    if len(r["chance"]) != r["max_plus"] or len(r["burn"]) != r["max_plus"]:
        sys.exit("HATA: yukseltme sans/yanma tablosu MaxPlus ile uyusmuyor.")
    qc = (PUB / "Components" / "MMOQuestComponent.h").read_text(encoding="utf-8", errors="replace")
    r["don_min"] = int(_must(r"GoldDonationMinTarget\s*=\s*(\d+)", qc, "GoldDonationMinTarget").group(1))
    return r


def upgrade_ev(r, target, buy, lv, gamma=0.0):
    """+0'dan hedef +N'ye beklenen altin (parca basina): her deneme (basari/basarisizlik) ucret oder, yanarsa parca
    saticidan yeniden alinir (buy) ve +0'dan baslanir. gamma > 0: aday C dik egri. V(k) = a_k + m_k x V(0) dogrusal cozulur."""
    def step(k):
        base = 50 * (k + 1) * (100 + lv * lv) // 100
        return base * (1.0 + max(0, lv - SINK_KNEE) / SINK_KNEE) ** gamma if gamma else base
    a, m = 0.0, 0.0
    for k in range(target - 1, -1, -1):
        p, b = r["chance"][k] / 100.0, r["burn"][k] / 100.0
        den = 1.0 - (1.0 - p) * (1.0 - b)
        a, m = ((step(k) + p * a + (1.0 - p) * b * buy) / den, (p * m + (1.0 - p) * b) / den)
    return a / (1.0 - m)


def sink_report(income_by_level, kills_by_level, bands, tiers, upg_to):
    r, c, fr = load_sink_rules(), load_new_items(), load_fusion_rules()
    quests_at = Counter(q["req"] for q in pm.quests if not q["id"].startswith("Q_Gold"))
    hours = {lv: (k * KILL_SEC + quests_at.get(lv, 0) * QUEST_SEC) / 3600.0 for lv, k in kills_by_level.items()}
    levels = sorted(income_by_level)
    don_at = Counter()
    for t, cnt, _g, req in quest_gold.values():
        if t == "HaveGold" and cnt >= r["don_min"]:
            don_at[req] += cnt
    bsum = lambda f, a, b: sum(f(lv) for lv in levels if a <= lv <= b)
    hl = lambda lv: max(0, lv - 1)
    heal_cur = lambda lv: max(0, c["heal_base"]) + (hl(lv) ** c["heal_pow"] * c["heal_num"]) // c["heal_den"]
    heal_cube = lambda lv: max(0, c["heal_base"]) + int(SINK_HEAL_CUBE * hl(lv) ** 3)
    heal_kill = lambda lv: round(SINK_HEAL_KILLS * avg_gold(lv))
    fus_ok = fr["fee_mult"] is not None and fr["cap"] is not None
    k_in = fr["inputs"] or 3
    heal_w = SINK_DEATHS * (1.0 - SINK_HEAL_BYPASS)
    PH = SINK_POTIONS_PER_HOUR * r["gpotion"]

    rows = []
    for a, b, cost in bands:
        inc = bsum(lambda lv: income_by_level[lv], a, b)
        hrs = bsum(lambda lv: hours[lv], a, b)
        don = bsum(lambda lv: don_at.get(lv, 0), a, b)
        parts = tiers[a]
        buys = sum(buy_price(p) for p in parts)
        row = dict(a=a, b=b, inc=inc, hrs=hrs, inc_h=inc / hrs if hrs else 0.0, don=don, inc_g=inc + don, c3=cost,
                   S0=inc - cost, buys=buys, heal=heal_w * bsum(lambda lv: hours[lv] * heal_cur(lv), a, b),
                   pot=hrs * PH, parts=parts, hours_fn=(lambda lv: hours[lv]))
        row["ev"] = {n: buys + sum(upgrade_ev(r, n, buy_price(p), p["lv"]) for p in parts) for n in (3, 5, 7)}
        row["fd"] = row["fs"] = 0.0
        if fus_ok:
            row["fd"] = sum(chain_fee(fr, f_buy(fr, p), fr["fee_mult"], fr["cap"]) for p in parts)
            row["fs"] = sum((k_in ** fr["cap"] - 1) * f_buy(fr, p) + chain_fee(fr, f_buy(fr, p), fr["fee_mult"], fr["cap"]) for p in parts)
        rows.append(row)

    print("=== Altin sink senaryolari (karar 3, 2026-10-17) [MODEL]; oynanis olcumu YOK ===")
    print(f"Koddan [Kesin]: yukseltme maliyeti 50 x (+N) x (100 + Lv^2) / 100 HER denemede odenir; basari %{'/'.join(map(str, r['chance']))}, "
          f"basarisizlikta yanma %{'/'.join(map(str, r['burn']))} (MaxPlus {r['max_plus']}); yanan parca saticidan yeniden alinir. "
          f"Sifaci {c['heal_base']} + {c['heal_num']}/{c['heal_den']} x (Lv-1)^{c['heal_pow']}; iksir {r['potion']} / Buyuk Can Iksiri {r['gpotion']}; "
          f"Q_Gold bagisi >= {r['don_min']} altin teslimde dusulur; birlestirme {'okundu' if fus_ok else 'YOK (D/S = 0)'}.")
    print(f"Varsayim [Tahmin]: {SINK_DEATHS:g} olum/sa x (1 - {SINK_HEAL_BYPASS:g} bypass) sifa; {SINK_POTIONS_PER_HOUR} Buyuk Can Iksiri/sa; "
          f"oldurme {KILL_SEC:g} sn, gorev {QUEST_SEC:g} sn; her bantta 4 parcalik tek kademe. Gelir = ana tablodaki (Q_Gold bagisi dusulmus); "
          f"'gelir brut' = gelir + bagis. Ganimet satisi gelirde YOK (asagida duyarlilik).")

    # --- 1. Mevcut sink'ler ------------------------------------------------
    print("\n(1) Mevcut sink'ler, bant basina. gider+3 = ana tablodaki (kesin +3, basarisizlik yok); EV+N = 4 parcanin alisi + beklenen "
          "yukseltme ucreti (basarisizlik ve yanma/yeniden alis dahil) hedef +N icin. Birl D/S = bir 4 parcalik setin Destansi'ya cikarilmasi "
          "(D ganimet girdisi, S saticidan).")
    print(f"{'aralik':>9} {'gelir':>10} {'gelir/sa':>9} {'gider+3':>8} {'EV+3':>8} {'EV+5':>9} {'EV+7':>10} {'sifaci':>9} {'iksir':>8} {'bagis':>7} {'birlD':>8} {'birlS':>9}")
    for x in rows:
        print(f"{x['a']:>4}-{x['b']:<4} {x['inc']:>10.0f} {x['inc_h']:>9.0f} {x['c3']:>8.0f} {x['ev'][3]:>8.0f} {x['ev'][5]:>9.0f} {x['ev'][7]:>10.0f} "
              f"{x['heal']:>9.0f} {x['pot']:>8.0f} {x['don']:>7.0f} {x['fd']:>8.0f} {x['fs']:>9.0f}")

    def tot(x, n, with_fus):
        return x["ev"][n] + x["heal"] + x["pot"] + x["don"] + (x["fd"] if with_fus == "D" else x["fs"] if with_fus == "S" else 0.0)
    scen = (("ALT", 3, None), ("ORTA", 5, "D"), ("UST", 7, "S"))
    print("\n(1b) Senaryo toplami ve acik: ALT = EV+3 + sifaci + iksir + bagis; ORTA = EV+5 + ... + birl.D; UST = EV+7 + ... + birl.S. "
          "gelir_brut / sink: >1 altin birikir, <1 kit; hedef <= " + f"{SINK_TARGET_RATIO:g}" + ". kalan = gelir_brut - sink.")
    print(f"{'aralik':>9} {'ALT g/s':>8} {'ORTA g/s':>9} {'UST g/s':>8} {'kalan ALT':>11} {'kalan ORTA':>11} {'kalan UST':>11}")
    for x in rows:
        v = [tot(x, n, f) for _l, n, f in scen]
        print(f"{x['a']:>4}-{x['b']:<4} {x['inc_g'] / v[0]:>8.1f} {x['inc_g'] / v[1]:>9.1f} {x['inc_g'] / v[2]:>8.1f} "
              f"{x['inc_g'] - v[0]:>11.0f} {x['inc_g'] - v[1]:>11.0f} {x['inc_g'] - v[2]:>11.0f}")
    hi = [x for x in rows if x["a"] >= SINK_FIRST_LV]
    s_inc = sum(x["inc_g"] for x in hi)
    print(f"Lv{SINK_FIRST_LV}-79 toplam: gelir_brut {s_inc:.0f}; sink ALT {sum(tot(x, 3, None) for x in hi):.0f} "
          f"(%{sum(tot(x, 3, None) for x in hi) / s_inc * 100:.1f}), ORTA {sum(tot(x, 5, 'D') for x in hi):.0f} "
          f"(%{sum(tot(x, 5, 'D') for x in hi) / s_inc * 100:.1f}), UST {sum(tot(x, 7, 'S') for x in hi):.0f} "
          f"(%{sum(tot(x, 7, 'S') for x in hi) / s_inc * 100:.1f}). Hangi senaryonun gercek oldugu oyuncunun +N hedefine bagli: "
          "[MMOMETRIC] ev=gold src=yukseltme ve item_upgrade (to_plus) olmadan bilinmez. "
          "Gelir < sink olan bantlarda (g/s < 1; ozellikle Lv<16) oyuncu o +N'e ulasamaz: UST bir ust sinirdir, gercek sink = gelir.")

    # Gelir tarafi duyarlilik: ganimet satisi (modelde yok = kotumser gelir)
    all_items, _calls = load_all_items()

    def loot_sale(lv):
        pool = [i for i in all_items if i["lv"] <= lv + fr["pool_slack"]]
        e = sum(p * f_sell(fr, i, ri) for i in pool for ri, p in enumerate(fr["roll"])) / len(pool)
        return fr["equip_chance"] * e
    print("Gelir duyarliligi [Hesap]: ganimet satisi (dusme sansi x ort. satis degeri, havuz Lv <= mob+3, elit ve +N dahil degil) "
          "ortalama mob altinina eklenir: " + "; ".join(
              f"{x['a']}-{x['b']}: +%{loot_sale((x['a'] + x['b']) // 2) / avg_gold((x['a'] + x['b']) // 2) * 100:.0f}"
              for x in hi if avg_gold((x['a'] + x['b']) // 2)) + ". (Gelir daha yuksek ise acik buyur.)")

    # --- 2. Adaylar --------------------------------------------------------
    cand = {}   # etiket -> {bant a: delta}

    def add(label, fn):
        cand[label] = {x["a"]: fn(x) for x in hi}
    add("A1", lambda x: heal_w * bsum(lambda lv: hours[lv] * (heal_cube(lv) - heal_cur(lv)), x["a"], x["b"]))
    add("A2", lambda x: heal_w * bsum(lambda lv: hours[lv] * (heal_kill(lv) - heal_cur(lv)), x["a"], x["b"]))
    for i, (xh, q) in enumerate(SINK_DONATE, 1):
        add(f"B{i}", lambda x, xh=xh, q=q: max(0.0, q * xh * x["inc_h"] - x["don"]))

    def up_delta(n, g):
        return lambda x: sum(upgrade_ev(r, n, buy_price(p), p["lv"], g) - upgrade_ev(r, n, buy_price(p), p["lv"]) for p in x["parts"])
    for i, g in enumerate(SINK_UPG_GAMMAS, 1):
        add(f"C{i}", up_delta(5, g))   # N=5 (ORTA); N=3/7 asagida
    gate_band = {}
    for gl in SINK_OPTIONAL_GATES:
        for x in hi:
            if x["a"] <= gl <= x["b"]:
                gate_band[x["a"]] = gate_band.get(x["a"], 0) + 1
    add("D1", lambda x: SINK_OPTIONAL_PART * SINK_OPTIONAL_HOURS * x["inc_h"] * gate_band.get(x["a"], 0))

    names = {
        "A1": f"A1 sifaci kup {SINK_HEAL_CUBE:g}",
        "A2": f"A2 sifaci {SINK_HEAL_KILLS}xmob altini",
        "B1": f"B1 bagis {SINK_DONATE[0][0]:g}sa x%{SINK_DONATE[0][1] * 100:.0f}",
        "B2": f"B2 bagis {SINK_DONATE[1][0]:g}sa x%{SINK_DONATE[1][1] * 100:.0f}",
        "C1": f"C1 yuk. dik g={SINK_UPG_GAMMAS[0]:g} (+5)",
        "C2": f"C2 yuk. dik g={SINK_UPG_GAMMAS[1]:g} (+5)",
        "D1": f"D1 opsiyonel {len(SINK_OPTIONAL_GATES)} alim",
    }
    print("\n(2) Aday sink'ler: EK altin / bant fazlasi (gelir - gider+3, ana tablodaki). Karsilastirma: birlestirme D %6.9, S %25.7 (kod carpani, ayni payda).")
    for k, v in names.items():
        print(f"    {v}")
    print(f"{'aralik':>9} {'fazla':>10}" + "".join(f" {k:>6}" for k in names))
    for x in hi:
        print(f"{x['a']:>4}-{x['b']:<4} {x['S0']:>10.0f}" + "".join(f" {cand[k][x['a']] / x['S0'] * 100:>5.1f}%" for k in names))
    s0 = sum(x["S0"] for x in hi)
    print(f"{'toplam':>9} {s0:>10.0f}" + "".join(f" {sum(cand[k].values()) / s0 * 100:>5.1f}%" for k in names)
          + f"   (Lv{SINK_FIRST_LV}-79, yalniz EK sink)")
    cn = []
    for n in (3, 5, 7):
        for i, g in enumerate(SINK_UPG_GAMMAS, 1):
            d = sum(up_delta(n, g)(x) for x in hi)
            cn.append(f"C{i} hedef +{n}: %{d / s0 * 100:.1f}")
    print("  Aday C hedef +N'ye gore (toplam fazlanin yuzdesi): " + "; ".join(cn) + ".")
    xh0, q0 = SINK_DONATE[0]
    print(f"  B1 icin bant basina tek bagis hedefi (= {xh0:g} sa gelir, 2 anlamli basamak; mevcut Q_Gold bagislari bant icinde ise o bant 0 EK): "
          + ", ".join("Lv%d: %d" % (x["a"], int(float("%.2g" % (xh0 * x["inc_h"])))) for x in hi) + ".")
    print(f"  Duyarlilik: A'da sifa bypass 0 ise etki x{1 / (1 - SINK_HEAL_BYPASS):g}; olum {DEATHS_PER_HOUR[0]}/{DEATHS_PER_HOUR[-1]} /sa ise x{DEATHS_PER_HOUR[0] / SINK_DEATHS:g}"
          f" / x{DEATHS_PER_HOUR[-1] / SINK_DEATHS:g}. B ve D katilim oranina dogrusal. C'de oyuncunun +N'den vazgecmesi (talep esnekligi) modellenmedi: etkiyi azaltir.")

    # --- 3. Paketler -------------------------------------------------------
    packs = (("P1 = A1+B1", ("A1", "B1")), ("P2 = P1+C1", ("A1", "B1", "C1")), ("P3 = A2+B2+C2+D1", ("A2", "B2", "C2", "D1")))
    print(f"\n(3) Paketler: kalan gelir_brut / (sink + EK). ALT (hedef +3) ve ORTA (hedef +5) tabanlari; hedef <= {SINK_TARGET_RATIO:g}. "
          "C'nin EKi ilgili +N icin yeniden hesaplanir.")
    print(f"{'aralik':>9} {'taban ALT':>9} {'taban ORTA':>10}" + "".join(f" {p[0][:2] + ' ALT':>7} {p[0][:2] + ' ORTA':>8}" for p in packs))
    sums = {}
    for x in hi:
        base = {"ALT": tot(x, 3, None), "ORTA": tot(x, 5, "D")}
        line = f"{x['a']:>4}-{x['b']:<4} {x['inc_g'] / base['ALT']:>9.1f} {x['inc_g'] / base['ORTA']:>10.1f}"
        for pn, keys in packs:
            for sc, n in (("ALT", 3), ("ORTA", 5)):
                d = 0.0
                for k in keys:
                    d += up_delta(n, SINK_UPG_GAMMAS[int(k[1]) - 1])(x) if k[0] == "C" else cand[k][x["a"]]
                sums.setdefault((pn, sc), []).append((x["inc_g"], base[sc] + d, x["S0"], d))
                line += f" {x['inc_g'] / (base[sc] + d):>7.1f}" if sc == "ALT" else f" {x['inc_g'] / (base[sc] + d):>8.1f}"
        print(line)
    print("Paket ozeti (Lv16-79): EK sink / toplam fazla; hedefi tutan bant sayisi (gelir/sink <= " + f"{SINK_TARGET_RATIO:g}" + f", {len(hi)} bant):")
    for pn, _k in packs:
        out = []
        for sc in ("ALT", "ORTA"):
            v = sums[(pn, sc)]
            out.append(f"{sc} EK %{sum(t[3] for t in v) / s0 * 100:.1f}, hedef {sum(1 for t in v if t[0] / t[1] <= SINK_TARGET_RATIO)}/{len(hi)}")
        print(f"  {pn}: " + "; ".join(out))
    base_hit = {"ALT": sum(1 for x in hi if x['inc_g'] / tot(x, 3, None) <= SINK_TARGET_RATIO),
                "ORTA": sum(1 for x in hi if x['inc_g'] / tot(x, 5, 'D') <= SINK_TARGET_RATIO)}
    print(f"  taban (paketsiz): ALT hedef {base_hit['ALT']}/{len(hi)}, ORTA hedef {base_hit['ORTA']}/{len(hi)}")

    print("\n(4) Kodda olmayan / sadece not:")
    print("  Onarim/dayaniklilik: YOK (Durability/Repair grep bos). Isim degistirme ucreti: YOK (GameMode::ChangeName yalniz ad kurali, altin yok). "
          f"Depo/canta genisletme: YOK (MaxWarehouseSize 24, MaxBagSize 12 sabit). Beceri/stat sifirlama, binek, kozmetik: YOK. "
          "Bolge gecidi ucreti: alan var ama 0 (MMOZoneLayout.h GoldCost, Codex dosyasi, commit'siz). Pazar/ilan vergisi: pazar yok (S12 atamasi), model yok; "
          "vergi ancak pazar olunca anlamlidir (islem hacmi olmadan hesaplanamaz).")
    return False


if __name__ == "__main__":
    main()
