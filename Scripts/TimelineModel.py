"""Zaman cizelgesi modeli (statik; ProgressionModel'in oldurme sayilarini dakikaya cevirir).

Calistir:
  python Scripts/TimelineModel.py                         # varsayilan: oldurme 25 sn, gorev 90 sn
  python Scripts/TimelineModel.py --kill-sec 15 --quest-sec 60
  python Scripts/TimelineModel.py --sweep                 # 15/25/40 sn duyarlilik tablosu (kilometre taslari)

Varsayimlar (hepsi [TAHMIN], oynanis olcumu DEGIL):
- Oldurme sayisi = ProgressionModel 'gorevli' sutunu (kapisi acilan tum gorevler yapilir; Q_Gold* haric).
- Bir oldurme ortalama --kill-sec saniye (hedef secme + savas + kisa yurume); olum, parti, mob bekleme yok.
- Her gorev (Q_Gold* haric) kapisi acildigi seviyede --quest-sec saniye ek is (NPC'ye gidis-donus + konusma).
- Bolgeler arasi yolculuk, dinlenme, alisveris, envanter yonetimi sayilmaz: gercek sure bundan UZUN olur.
Karar 6 (DECISIONS_NEEDED: 'Lv30 kac saat', hedef 2-3 saat) icin kaba bir referanstir; [MMOMETRIC] levelup sureleri gelince
bu modelin yerine gercek veri gecer.
"""

import argparse
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ProgressionModel as pm  # noqa: E402

MILESTONES = (10, 20, 30, 40, 50, 60, 70, 80)


def timeline(kill_sec, quest_sec, max_level=80):
    out = pm.simulate(max_level - 1)
    quests_at = Counter(q["req"] for q in pm.quests if not q["id"].startswith("Q_Gold"))
    rows, cum = [], 0.0
    for lv, _need, _xpk, _knq, kills, _qx, _gx, _grind in out:
        k = kills or 0.0
        q = quests_at.get(lv, 0)
        minutes = (k * kill_sec + q * quest_sec) / 60.0
        cum += minutes
        rows.append((lv, k, q, minutes, cum))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kill-sec", type=float, default=25.0)
    ap.add_argument("--quest-sec", type=float, default=90.0)
    ap.add_argument("--max", type=int, default=80)
    ap.add_argument("--sweep", action="store_true")
    a = ap.parse_args()

    if a.sweep:
        print("Kilometre taslari (kumulatif saat) - oldurme suresi duyarliligi, gorev 90 sn:")
        print(f"{'Lv':>4} " + " ".join(f"{s:>8}" for s in ("15 sn", "25 sn", "40 sn")))
        tables = {s: {r[0] + 1: r[4] for r in timeline(s, a.quest_sec, a.max)} for s in (15, 25, 40)}
        for m in MILESTONES:
            if m <= a.max:
                print(f"{m:>4} " + " ".join(f"{tables[s].get(m, 0) / 60:>8.1f}" for s in (15, 25, 40)))
        return

    rows = timeline(a.kill_sec, a.quest_sec, a.max)
    print(f"Varsayim: oldurme {a.kill_sec:g} sn, gorev {a.quest_sec:g} sn (MODEL, olcum degil; yolculuk/olum/parti yok)")
    print(f"{'Lv':>4} {'oldurme':>8} {'gorev':>6} {'dk':>7} {'kum.saat':>9}")
    for lv, k, q, minutes, cum in rows:
        print(f"{lv:>4} {k:>8.1f} {q:>6} {minutes:>7.1f} {cum / 60:>9.2f}")
    print("\nKilometre taslari (o seviyeye ulasma, kumulatif saat):")
    by_target = {r[0] + 1: r[4] for r in rows}
    print("  " + "  ".join(f"Lv{m}: {by_target[m] / 60:.1f} sa" for m in MILESTONES if m in by_target))


if __name__ == "__main__":
    main()
