"""Altin ekonomisi modeli (statik, editor gerektirmez; python3 yeterli).

Calistir:  python Scripts/EconomyModel.py [--upgrade-to N]

ProgressionModel.py'nin seviye basina oldurme sayisini kullanir ve sunlari hesaplar:
  gelir  = oldurme x ortalama mob altini (seviye penceresindeki normal moblar) + gorev altin odulleri
  gider  = her ekipman kademesinin 4 parcasinin alis fiyati + parca basina +N yukseltme maliyeti
Kademe basina (kademe seviyeleri arasi) gelir/gider oranini basar. MODEL: iksir/sifaci/ticaret/ganimet satisi,
olum ve parti yok; yukseltme basarisizliginda kaybolan altin ve yanma yok (iyimser gider); loot satisi yok
(kotumser gelir). Sonuclar [PLACEHOLDER] degerlendirme icindir, oynanis olcumu degildir.
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

# Ekipman: MakeItem(Id, Ad, Slot, Atk, Def, ReqLv)
items = []
for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*S::(\w+),\s*([\d.]+)f,\s*([\d.]+)f,\s*(\d+)', item_src):
    items.append(dict(id=m.group(1), slot=m.group(2), atk=float(m.group(3)), df=float(m.group(4)), lv=int(m.group(5))))


def buy_price(it):
    return round((it["atk"] + it["df"]) * 12.0) + it["lv"] * 25 + 50


def upgrade_cost(to_plus):
    return sum(50 * (p + 1) for p in range(to_plus))


def avg_gold(level):
    pool = [mob_gold[m["id"]] for m in pm.normals if level - 8 <= m["lv"] <= level + 3 and m["id"] in mob_gold]
    return sum(pool) / len(pool) if pool else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upgrade-to", type=int, default=3, help="her parcayi bu +N seviyeye yukselt (varsayilan 3)")
    args = ap.parse_args()

    out = pm.simulate(80 - 1)
    income_by_level = {}
    for lv, _n, _xpk, _knq, kills, _qx, _gx, _gr in out:
        if kills is None:
            continue
        gold_kills = kills * avg_gold(lv)
        quest_here = sum(g for (t, c, g, req) in quest_gold.values()
                         if (c if t == "ReachLevel" else req) == lv)
        income_by_level[lv] = gold_kills + quest_here

    tiers = {}
    for it in items:
        tiers.setdefault(it["lv"], []).append(it)
    tier_lvls = sorted(tiers)
    print(f"Kademe: 4 parcanin alisi + her parca +{args.upgrade_to} yukseltme ({upgrade_cost(args.upgrade_to)} altin/parca).")
    print("Gelir: oldurme x ort. mob altini + gorev altin odulleri (Q_Gold* odulu 0).")
    print(f"{'kademe Lv':>9} {'aralik':>9} {'gelir':>10} {'gider':>10} {'gelir/gider':>11}  parca sayisi")
    for i, tl in enumerate(tier_lvls):
        nxt = tier_lvls[i + 1] if i + 1 < len(tier_lvls) else 80
        # Bu kademeden bir sonrakine kadar kazanilan gelir, bu kademenin giderini karsilar mi?
        income = sum(v for lv, v in income_by_level.items() if tl <= lv < nxt)
        parts = tiers[tl]
        cost = sum(buy_price(p) + upgrade_cost(args.upgrade_to) for p in parts)
        ratio = income / cost if cost else float("inf")
        print(f"{tl:>9} {tl:>4}-{nxt - 1:<4} {income:>10.0f} {cost:>10.0f} {ratio:>11.1f}  {len(parts)}")
    tot_income = sum(v for lv, v in income_by_level.items() if lv <= 30)
    tot_cost30 = sum(buy_price(p) + upgrade_cost(args.upgrade_to) for tl in tier_lvls if tl <= 30 for p in tiers[tl])
    print(f"\nLv1->30 toplam gelir ~{tot_income:.0f}, Lv<=30 kademelerinin toplam gideri ~{tot_cost30:.0f} (oran {tot_income / tot_cost30:.1f}x)")
    print("Yorum: oran >>1 ise altin birikir (sink yetersiz); <1 ise kit. Q_Gold* gorevleri harcama degil birikim ister.")


if __name__ == "__main__":
    main()
