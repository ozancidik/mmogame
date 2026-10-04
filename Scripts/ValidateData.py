"""Statik veri tablosu denetimi (derleme/editor gerektirmez).

Calistir:
  "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/ValidateData.py

Kontroller: mob/item/gorev kimlikleri tekil mi, KillSpecificMob hedefleri mob tablosunda mi,
on kosul gorevleri var mi, odul ekipmanlari item tablosunda mi, gorev kapisi ile hedef mob
seviyesi farki (XP seviye farki kurali: oyuncu > mob+5 ise XP duser), gorev zinciri dongusu.
Cikis kodu 0 = temiz, 1 = hata var. UYARI satirlari bilgi amaclidir.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source" / "MyProject" / "Private"

mob_src = (SRC / "Characters" / "MMOMobArchetype.cpp").read_text(encoding="utf-8", errors="replace")
item_src = (SRC / "Items" / "MMOItemDatabase.cpp").read_text(encoding="utf-8", errors="replace")
quest_src = (SRC / "Quests" / "MMOQuestTypes.cpp").read_text(encoding="utf-8", errors="replace")
skill_src = (SRC / "Skills" / "MMOSkillDatabase.cpp").read_text(encoding="utf-8", errors="replace")
effect_src = (SRC / "Effects" / "MMOEffectDatabase.cpp").read_text(encoding="utf-8", errors="replace")

errors = []
warnings = []


def dupes(ids, label):
    seen = set()
    for i in ids:
        if i in seen:
            errors.append(f"{label} kimligi yinelenmis: {i}")
        seen.add(i)
    return seen


# --- Moblar: MakeMob(TEXT("Id"), TEXT("Ad"), Level, ...)
mobs = {}
for m in re.finditer(r'MakeMob\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*(\d+)', mob_src):
    mobs[m.group(1)] = int(m.group(2))
dupes([m.group(1) for m in re.finditer(r'MakeMob\(TEXT\("([^"]+)"\)', mob_src)], "Mob")

# --- Itemlar
item_ids = [m.group(1) for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\)', item_src)]
items = dupes(item_ids, "Item")

# --- Gorevler: Table.Add(MakeQuest(...)); ardindan Table.Last().Reward...
quests = {}
order = []
pat = re.compile(
    r'Table\.Add\(MakeQuest\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("(?:[^"\\]|\\.)*"\),\s*'
    r'(?:T|EMMOQuestType)::(\w+),\s*TEXT\("([^"]*)"\),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)'
    r'(?:,\s*TEXT\("([^"]*)"\))?\)\);'
    r'((?:\s*Table\.Last\(\)\.[^;]+;)*)'
)
for m in pat.finditer(quest_src):
    qid, qtype, target, count, xp, gold, req, prereq, tail = m.groups()
    reward = re.search(r'RewardEquipmentId = FName\(TEXT\("([^"]+)"\)\)', tail or "")
    quests[qid] = dict(type=qtype, target=target, count=int(count), req=int(req),
                       prereq=prereq or "", reward=reward.group(1) if reward else "", xp=int(xp))
    order.append(qid)
dupes(order, "Gorev")
total_calls = len(re.findall(r'Table\.Add\(MakeQuest\(', quest_src))
if total_calls != len(quests):
    errors.append(f"Gorev ayristirma uyumsuz: {total_calls} MakeQuest cagrisi, {len(quests)} ayristirildi")

for qid, q in quests.items():
    if q["type"] == "KillSpecificMob":
        if q["target"] not in mobs:
            errors.append(f"{qid}: hedef mob yok -> {q['target']}")
        else:
            gap = q["req"] - mobs[q["target"]]
            if gap > 5:
                warnings.append(f"{qid}: kapi Lv{q['req']} ama hedef {q['target']} Lv{mobs[q['target']]} (fark {gap}, XP azalir)")
    if q["prereq"] and q["prereq"] not in quests:
        errors.append(f"{qid}: on kosul gorev yok -> {q['prereq']}")
    if q["reward"] and q["reward"] not in items:
        errors.append(f"{qid}: odul ekipman yok -> {q['reward']}")

# Zincir tutarliligi: gorev kapisi, on kosul gorevin kapisindan dusuk olmamali
for qid, q in quests.items():
    pre = quests.get(q["prereq"]) if q["prereq"] else None
    if pre and q["req"] < pre["req"]:
        warnings.append(f"{qid}: kapi Lv{q['req']} ama on kosul {q['prereq']} kapisi Lv{pre['req']} (zincir geri gidiyor)")

# Odul ekipmaninin kusanma seviyesi, gorev tamamlandiginda ulasilabilir mi (kapi + 2)
item_levels = {}
for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*S::\w+,\s*[\d.]+f,\s*[\d.]+f,\s*(\d+)', item_src):
    item_levels[m.group(1)] = int(m.group(2))
for qid, q in quests.items():
    lv = item_levels.get(q["reward"]) if q["reward"] else None
    if lv is not None and lv > q["req"] + 2:
        warnings.append(f"{qid}: odul {q['reward']} Lv{lv} gerektiriyor, gorev kapisi Lv{q['req']} (hemen kusanilamaz)")


# --- Beceriler: MakeDef(TEXT("Id"), TEXT("Ad"), C::Sinif, T::Agac, Rutbe, ReqLevel, SP, TEXT("Onkosul"), E::Etki, ...[, TEXT("EfektId")])
effects = dupes([m.group(1) for m in re.finditer(r'MakeEffect\(TEXT\("([^"]+)"\)', effect_src)], "Efekt")
skill_pat = re.compile(
    r'Table\.Add\(MakeDef\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*C::(\w+),\s*T::(\w+),\s*(\d+),\s*(\d+),\s*\d+,\s*'
    r'TEXT\("([^"]*)"\),\s*E::(\w+),([^;]*?)\)\);', re.S)
skills = {}
skill_order = []
for m in skill_pat.finditer(skill_src):
    sid, cls, tree, rank, lvl, prereq, eff, rest = m.groups()
    eff_id = re.search(r'TEXT\("([^"]*)"\)\s*$', rest.strip())
    nums = re.findall(r'([\d.]+)f', rest)  # mana, bekleme, guc, menzil, yaricap
    skills[sid] = dict(cls=cls, tree=tree, rank=int(rank), lvl=int(lvl), prereq=prereq, effect=eff_id.group(1) if eff_id else "",
                       etype=eff, mana=float(nums[0]) if len(nums) > 0 else 0.0, power=float(nums[2]) if len(nums) > 2 else 0.0)
    skill_order.append(sid)
dupes(skill_order, "Beceri")
skill_calls = len(re.findall(r'Table\.Add\(MakeDef\(', skill_src))
if skill_calls != len(skills):
    errors.append(f"Beceri ayristirma uyumsuz: {skill_calls} MakeDef cagrisi, {len(skills)} ayristirildi")

# Zincir mantigi: ayni etki turundeki on kosulun devami, on kosuldan ZAYIF olmamali (QA Bulgu #4 deseni)
for sid, k in skills.items():
    pre = skills.get(k["prereq"]) if k["prereq"] else None
    if pre and pre["etype"] == k["etype"] and k["power"] > 0 and k["power"] < pre["power"]:
        warnings.append(f"{sid}: gucu {k['power']:g}, on kosul {k['prereq']} gucu {pre['power']:g} (devami daha zayif)")

tree_ranks = {}
for sid, k in skills.items():
    if k["prereq"] and k["prereq"] not in skills:
        errors.append(f"{sid}: on kosul beceri yok -> {k['prereq']}")
    if k["effect"] and k["effect"] not in effects:
        errors.append(f"{sid}: efekt yok -> {k['effect']}")
    if k["rank"] > k["lvl"]:
        errors.append(f"{sid}: rutbe {k['rank']} > gereken seviye {k['lvl']} (acilamaz)")
    tree_ranks.setdefault((k["cls"], k["tree"]), set()).add(k["rank"])
for (cls, tree), ranks in sorted(tree_ranks.items()):
    if ranks != set(range(1, max(ranks) + 1)):
        errors.append(f"{cls}/{tree}: rutbeler bosluklu -> {sorted(ranks)}")

# Bilgi: sinif basina Lv1-30 arasinda yeni beceri gelmeyen 5+ seviyelik pencereler
by_class = {}
for k in skills.values():
    by_class.setdefault(k["cls"], set()).add(k["lvl"])
for cls, lvls in sorted(by_class.items()):
    pts = sorted(l for l in lvls if l <= 30)
    after = sorted(l for l in lvls if l > 30)
    nxt = after[0] if after else 31  # Lv30 sonrasi ilk beceri (yoksa 31)
    for a, b in zip(pts, pts[1:] + [nxt]):
        if b - a >= 5:
            warnings.append(f"{cls}: Lv{a} sonrasi Lv{b} oncesine kadar yeni beceri yok ({b - a} seviye)")

# On kosul dongusu
for qid in quests:
    seen, cur = set(), qid
    while cur and cur in quests:
        if cur in seen:
            errors.append(f"On kosul dongusu: {qid}")
            break
        seen.add(cur)
        cur = quests[cur]["prereq"]

print(f"Mob: {len(mobs)}  Item: {len(items)} (seviyesi okunan: {len(item_levels)})  Gorev: {len(quests)}  Beceri: {len(skills)}  Efekt: {len(effects)}")
for w in warnings:
    print("UYARI:", w)
for e in errors:
    print("HATA:", e)
if errors:
    sys.exit(1)
print("Veri denetimi temiz.")
