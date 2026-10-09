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
"""

import argparse
import importlib.util
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source" / "MyProject" / "Private"

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
    floored = 0
    for lv, _n, _xpk, knq, kills, _qx, _gx, grind in out:
        if kills is None:
            continue
        if grind == 0 and kills < args.min_kill_frac * knq:
            kills = args.min_kill_frac * knq
            floored += 1
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


if __name__ == "__main__":
    main()
