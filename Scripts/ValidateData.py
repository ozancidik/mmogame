"""Statik veri tablosu denetimi (derleme/editor gerektirmez).

Calistir:
  "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/ValidateData.py

Kontroller: mob/item/gorev kimlikleri tekil mi, KillSpecificMob hedefleri mob tablosunda mi,
on kosul gorevleri var mi, odul ekipmanlari item tablosunda mi, gorev kapisi ile hedef mob
seviyesi farki (XP seviye farki kurali: oyuncu > mob+5 ise XP duser), gorev zinciri dongusu.
Oynanabilirlik: MMOGameMode.cpp spawner bantlari bos mu, ForcedArchetypeId tabloda mi, KillSpecificMob hedefi
bir spawner'da uretiliyor mu, odul esyasi MaxLevel icinde mi, saticidaki (tum tablo) yuva kapsamasi,
MMOXPTuning knot sirasi/araligi ve parti bonusu. Negatif test: Scripts/TestValidateData.py.
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

# Beceri boslugu (UYARI, CI kirmaz): sinif basina Lv1'den tablodaki en yuksek beceri seviyesine kadar, ardisik iki
# yeni-beceri seviyesi arasindaki fark (sonraki - onceki) SKILL_GAP_LEVELS veya fazlaysa. Lv1 baslangic noktasi sayilir.
# Fark 5 = arada 4 bos seviye (orn. Lv28 -> Lv33: Lv29-32).
SKILL_GAP_LEVELS = 5
by_class = {}
for k in skills.values():
    by_class.setdefault(k["cls"], set()).add(k["lvl"])
for cls, lvls in sorted(by_class.items()):
    pts = sorted(lvls | {1})
    for a, b in zip(pts, pts[1:]):
        if b - a >= SKILL_GAP_LEVELS:
            warnings.append(f"{cls}: beceri boslugu Lv{a} -> Lv{b} (Lv{a + 1}-{b - 1} arasi yeni beceri yok, {b - a} seviye)")

# On kosul dongusu
for qid in quests:
    seen, cur = set(), qid
    while cur and cur in quests:
        if cur in seen:
            errors.append(f"On kosul dongusu: {qid}")
            break
        seen.add(cur)
        cur = quests[cur]["prereq"]


# ---------------------------------------------------------------------------
# Oynanabilirlik denetimleri (B3): spawner bantlari, gorev hedefi uretilebilir mi, odul kusanilabilir mi,
# saticidan alinabilen ekipman kapsamasi, MMOXPTuning knot araligi.
# ---------------------------------------------------------------------------
MAX_LEVEL = 80
gm_src = (SRC / "Core" / "MMOGameMode.cpp").read_text(encoding="utf-8", errors="replace")
tuning_src = (SRC / "Core" / "MMOXPTuning.cpp").read_text(encoding="utf-8", errors="replace")
mob_src_nc = re.sub(r"//[^\n]*", "", mob_src)
gm_nc = re.sub(r"//[^\n]*", "", gm_src)

elites = set()
for m in re.finditer(r'MakeMob\(TEXT\("([^"]+)"\)[^;]*;((?:\s*(?:Table\.Last\(\)\.[^;]+|Set\w+\(Table\.Last\(\)\));)*)', mob_src_nc):
    if re.search(r"bIsElite\s*=\s*true", m.group(2)):
        elites.add(m.group(1))


def split_args(text):
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def call_args(src, start):
    """src[start] == '(' -> (argumanlar, kapanis indeksi)"""
    depth = 0
    for j in range(start, len(src)):
        if src[j] == "(":
            depth += 1
        elif src[j] == ")":
            depth -= 1
            if depth == 0:
                return split_args(src[start + 1:j]), j
    return [], len(src)


def lit(expr):
    """'58' -> 58, 'FName(TEXT("X"))' -> 'X', 'NAME_None' -> '' ; degilse None"""
    expr = expr.strip()
    if re.fullmatch(r"-?\d+", expr):
        return int(expr)
    m = re.fullmatch(r'FName\(\s*TEXT\("([^"]*)"\)\s*\)', expr)
    if m:
        return m.group(1)
    if expr == "NAME_None":
        return ""
    return None


# spawner: dict(src, min, max, forced)
spawners = []
lambda_spans = []
for lm in re.finditer(r"auto\s+(\w+)\s*=\s*\[&\]\s*\(", gm_nc):
    params, pclose = call_args(gm_nc, lm.end() - 1)
    pnames = [re.findall(r"(\w+)\s*$", p)[0] for p in params]
    body_start = gm_nc.find("{", pclose)
    depth, j = 0, body_start
    while j < len(gm_nc):
        depth += (gm_nc[j] == "{") - (gm_nc[j] == "}")
        if depth == 0:
            break
        j += 1
    body = gm_nc[body_start:j]
    lambda_spans.append((body_start, j))
    if "AMMOMobSpawner" not in body:
        continue
    fields = {}
    for fld in ("MinMobLevel", "MaxMobLevel", "ForcedArchetypeId"):
        fm = re.search(r"->\s*" + fld + r"\s*=\s*([^;]+);", body)
        fields[fld] = fm.group(1).strip() if fm else None
    for cm in re.finditer(r"\b" + lm.group(1) + r"\s*\(", gm_nc[j:]):
        args, _ = call_args(gm_nc, j + cm.end() - 1)
        bind = dict(zip(pnames, args))

        def val(fld):
            e = fields[fld]
            return None if e is None else lit(bind.get(e, e))
        forced = val("ForcedArchetypeId") or ""
        line = gm_nc.count("\n", 0, j + cm.start()) + 1
        if forced:
            spawners.append(dict(src=f"MMOGameMode.cpp:{line} {lm.group(1)}", min=0, max=0, forced=forced))
        else:
            spawners.append(dict(src=f"MMOGameMode.cpp:{line} {lm.group(1)}", min=val("MinMobLevel") or 0,
                                 max=val("MaxMobLevel") or 0, forced=""))

direct_sites = 0
for sm in re.finditer(r"AMMOMobSpawner\s*\*\s*(\w+)\s*=\s*World->SpawnActorDeferred<AMMOMobSpawner>", gm_nc):
    if any(a <= sm.start() <= b for a, b in lambda_spans):
        continue
    direct_sites += 1
    var = sm.group(1)
    end = gm_nc.find("FinishSpawningActor(" + var, sm.end())
    block = gm_nc[sm.end():end if end > 0 else sm.end() + 2000]
    line = gm_nc.count("\n", 0, sm.start()) + 1
    get = lambda fld: re.search(r"\b" + var + r"->" + fld + r"\s*=\s*([^;]+);", block)
    f, mn, mx = get("ForcedArchetypeId"), get("MinMobLevel"), get("MaxMobLevel")
    forced = lit(f.group(1)) if f else ""
    lo = lit(mn.group(1)) if mn else 0
    hi = lit(mx.group(1)) if mx else 0
    if forced is None or lo is None or hi is None:
        errors.append(f"MMOGameMode.cpp:{line}: spawner {var} sabit olmayan deger kullaniyor; ValidateData okuyamadi")
        continue
    spawners.append(dict(src=f"MMOGameMode.cpp:{line} {var}", min=lo, max=hi, forced=forced))

if direct_sites + len(lambda_spans) and not spawners:
    errors.append("MMOGameMode.cpp: hic spawner ayristirilamadi (ValidateData kalibi bozuk)")

producible = set()
for sp in spawners:
    if sp["forced"]:
        if sp["forced"] not in mobs:
            errors.append(f"{sp['src']}: ForcedArchetypeId {sp['forced']} mob tablosunda yok")
        producible.add(sp["forced"])
        continue
    lo, hi = sp["min"], sp["max"]
    if lo == 0 and hi == 0:
        warnings.append(f"{sp['src']}: bant da zorunlu arketip de yok (mob rastgele secer)")
        producible |= {m for m in mobs if m not in elites}
        continue
    band = {m for m, lv in mobs.items() if m not in elites and (lo == 0 or lv >= lo) and (hi == 0 or lv <= hi)}
    if (lo and hi and lo > hi) or not band:
        errors.append(f"{sp['src']}: Lv{lo}-{hi} bandinda elit olmayan mob yok (spawner hic mob uretmez)")
    producible |= band

for qid, q in quests.items():
    if q["type"] == "KillSpecificMob" and q["target"] in mobs and q["target"] not in producible:
        kind = "elit (yalniz ForcedArchetypeId ile dogar)" if q["target"] in elites else f"Lv{mobs[q['target']]}"
        errors.append(f"{qid}: hedef {q['target']} ({kind}) hicbir spawner'da uretilmiyor (gorev tamamlanamaz)")
for m in sorted(set(mobs) - producible):
    warnings.append(f"Mob {m} (Lv{mobs[m]}{', elit' if m in elites else ''}) hicbir spawner'da uretilmiyor (olu icerik)")

# Odul ekipmani: kusanma seviyesi oyuncu tavanini asmamali; kapidan cok dusukse odul anlamsiz.
for qid, q in quests.items():
    lv = item_levels.get(q["reward"]) if q["reward"] else None
    if lv is None:
        continue
    if lv > MAX_LEVEL:
        errors.append(f"{qid}: odul {q['reward']} Lv{lv} gerektiriyor, MaxLevel {MAX_LEVEL} (hic kusanilamaz)")
    elif q["req"] - lv > 15:
        warnings.append(f"{qid}: odul {q['reward']} Lv{lv}, gorev kapisi Lv{q['req']} (odul {q['req'] - lv} seviye eski)")

# Satici: tum ekipman tablosu satilir (AMMOHUD shop = FMMOItemDatabase::GetAll). Yuva basina kapsama.
slot_levels = {}
for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*S::(\w+),\s*[\d.]+f,\s*[\d.]+f,\s*(\d+)', item_src):
    slot_levels.setdefault(m.group(2), []).append(int(m.group(3)))
for slot, lvls in sorted(slot_levels.items()):
    lvls = sorted(lvls)
    if lvls[0] > 10:
        warnings.append(f"Satici: {slot} yuvasinda ilk esya Lv{lvls[0]} (Lv1-10 arasi bu yuva bos)")
    for a, b in zip(lvls, lvls[1:]):
        if b - a > 12:
            warnings.append(f"Satici: {slot} yuvasinda Lv{a} -> Lv{b} arasi yeni esya yok ({b - a} seviye)")
    if lvls[-1] < MAX_LEVEL - 20:
        warnings.append(f"Satici: {slot} yuvasinda en yuksek esya Lv{lvls[-1]} (Lv{lvls[-1]}-{MAX_LEVEL} arasi yenilenmez)")
if not slot_levels:
    errors.append("Satici: MakeItem ayristirilamadi (ekipman tablosu bos goruluyor)")

# MMOXPTuning knotlari: seviye kesin artan, 1..MaxLevel, carpan (0, 2].
for label in ("GMobKnots", "GQuestKnots"):
    km = re.search(label + r"\[\]\s*=\s*\{(.*?)\};", tuning_src, re.S)
    if not km:
        errors.append(f"MMOXPTuning.cpp: {label} bulunamadi")
        continue
    knots = [(int(a), float(b)) for a, b in re.findall(r"\{\s*(\d+)\s*,\s*([\d.]+)f?\s*\}", km.group(1))]
    if len(knots) < 2:
        errors.append(f"MMOXPTuning.cpp: {label} en az 2 knot olmali ({len(knots)})")
        continue
    if knots[0][0] != 1:
        errors.append(f"MMOXPTuning.cpp: {label} ilk knot Lv1 olmali (Lv{knots[0][0]})")
    for (la, _), (lb, _) in zip(knots, knots[1:]):
        if lb <= la:
            errors.append(f"MMOXPTuning.cpp: {label} seviyeler kesin artan degil (Lv{la} -> Lv{lb})")
    for lv, mult in knots:
        if not 1 <= lv <= MAX_LEVEL:
            errors.append(f"MMOXPTuning.cpp: {label} knot Lv{lv} 1..{MAX_LEVEL} disinda")
        if not 0.0 < mult <= 2.0:
            errors.append(f"MMOXPTuning.cpp: {label} Lv{lv} carpani {mult} (0, 2] disinda")
pb = re.search(r"GPartyBonusPerExtraMember\s*=\s*([\d.]+)f?", tuning_src)
if not pb:
    errors.append("MMOXPTuning.cpp: GPartyBonusPerExtraMember bulunamadi")
elif not 0.0 <= float(pb.group(1)) <= 0.5:
    errors.append(f"MMOXPTuning.cpp: parti bonusu {pb.group(1)} [0, 0.5] disinda")

print(f"Mob: {len(mobs)} (elit {len(elites)})  Item: {len(items)} (seviyesi okunan: {len(item_levels)})  Gorev: {len(quests)}  Beceri: {len(skills)}  Efekt: {len(effects)}  Spawner: {len(spawners)}")
for w in warnings:
    print("UYARI:", w)
for e in errors:
    print("HATA:", e)
if errors:
    sys.exit(1)
print("Veri denetimi temiz.")
