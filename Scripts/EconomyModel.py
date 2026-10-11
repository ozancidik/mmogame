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

Yeni kalemler (2026-10-11; ciktinin sonunda ayri bolum, ustteki eski tablo DEGISMEZ):
  sifaci bedeli (sink, MMOHealerNPC.h::ComputeHealCost), basarim altini (kaynak, tek seferlik, MMOAchievementDatabase.cpp),
  gunluk gorevler (kaynak, gunluk, MMODailyComponent.cpp::BuildBounties), posta ucreti (sink, MMOMailComponent.h::FeeFor).
  Katsayilar koddan okunur. Zaman: seviye basina oldurme x KILL_SEC + gorev x QUEST_SEC (TimelineModel varsayilanlari;
  oldurme sayisi gelirdeki ile ayni, grind=0 tabani dahil). Olum/saat ve oyun saati/gun [TAHMIN] (asagidaki sabitler).
"""

import argparse
import importlib.util
import re
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
    args = ap.parse_args()

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
    m = _must(r"ComputeHealCost.*?\(\s*L\s*\*\s*L\s*\*\s*(\d+)\s*\)\s*/\s*(\d+)", healer, "ComputeHealCost katsayisi")
    c["heal_num"], c["heal_den"] = int(m.group(1)), int(m.group(2))

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
        return max(0, c["heal_base"]) + (l1 * l1 * c["heal_num"]) // c["heal_den"]

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
    print(f"Koddan [Kesin]: sifaci {c['heal_base']} + {c['heal_num']}/{c['heal_den']} x (Lv-1)^2 (Lv1 {heal_cost(1)}, Lv30 {heal_cost(30)}, "
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


if __name__ == "__main__":
    main()
