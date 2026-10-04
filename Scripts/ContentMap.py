"""Seviye seviye icerik haritasi (statik, editor gerektirmez; python3 yeterli).

Calistir:
  python Scripts/ContentMap.py            # Markdown'i ekrana basar
  python Scripts/ContentMap.py --write    # docs/CONTENT_MAP.md dosyasina yazar

Moblar, gorevler (kapi seviyesi), ekipman (gereken seviye) ve beceriler (gereken seviye) icin hangi
seviyede ne acildigini tek tabloda gosterir ve "hic yeni icerik gelmeyen" seviye araliklarini isaretler.
Kaynak: Source/MyProject/Private/{Characters,Quests,Items,Skills}/*.cpp (regex ile okunur; ValidateData.py
ile ayni kaliplar). Cikti bir tasarim raporudur, oynanis olcumu degildir.
"""

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source" / "MyProject" / "Private"


def read(rel):
    return (SRC / rel).read_text(encoding="utf-8", errors="replace")


mob_src = read("Characters/MMOMobArchetype.cpp")
quest_src = read("Quests/MMOQuestTypes.cpp")
item_src = read("Items/MMOItemDatabase.cpp")
skill_src = read("Skills/MMOSkillDatabase.cpp")

mobs = []
mob_pat = re.compile(
    r'Table\.Add\(MakeMob\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*(\d+),\s*\d+,\s*\d+,\s*[\d.]+f,\s*(\d+)'
    r'.*?\)\);((?:(?:\s|//[^\n]*)*(?:Table\.Last\(\)\.[^;]+|\w+\(Table\.Last\(\)\));)*)', re.S)
for m in mob_pat.finditer(mob_src):
    mobs.append(dict(id=m.group(1), lv=int(m.group(2)), xp=int(m.group(3)), elite="bIsElite = true" in (m.group(4) or "")))

quests = []
q_pat = re.compile(
    r'Table\.Add\(MakeQuest\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("(?:[^"\\]|\\.)*"\),\s*'
    r'(?:T|EMMOQuestType)::(\w+),\s*TEXT\("([^"]*)"\),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)')
for m in q_pat.finditer(quest_src):
    qid, qtype, _t, _c, _xp, _g, req = m.groups()
    quests.append(dict(id=qid, type=qtype, req=int(req)))

items = []
for m in re.finditer(r'MakeItem\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*S::(\w+),\s*[\d.]+f,\s*[\d.]+f,\s*(\d+)', item_src):
    items.append(dict(id=m.group(1), slot=m.group(2), lv=int(m.group(3))))

skills = []
for m in re.finditer(r'Table\.Add\(MakeDef\(TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*C::(\w+),\s*T::(\w+),\s*(\d+),\s*(\d+)', skill_src):
    skills.append(dict(id=m.group(1), cls=m.group(2), lv=int(m.group(5))))


def short(ids, limit=4):
    ids = list(ids)
    if len(ids) <= limit:
        return ", ".join(ids)
    return ", ".join(ids[:limit]) + f" (+{len(ids) - limit})"


def build(max_level=80):
    out = []
    out.append("# Seviye seviye icerik haritasi\n")
    out.append("`Scripts/ContentMap.py --write` ile uretilir (elle duzenleme). Kaynak: kod tablolari; oynanis olcumu degildir.\n")
    out.append(f"Toplam: {len(mobs)} mob ({sum(1 for m in mobs if m['elite'])} elit), {len(quests)} gorev, {len(items)} ekipman, {len(skills)} beceri.\n")
    out.append("| Lv | Mob (normal) | Elit | Gorev kapisi | Ekipman | Beceri |")
    out.append("|---:|---|---|---|---|---|")
    empty_run = []
    gaps = []
    for lv in range(1, max_level + 1):
        n = [m["id"] for m in mobs if m["lv"] == lv and not m["elite"]]
        e = [m["id"] for m in mobs if m["lv"] == lv and m["elite"]]
        q = [x["id"] for x in quests if x["req"] == lv]
        i = [x["id"] for x in items if x["lv"] == lv]
        s = [x["id"] for x in skills if x["lv"] == lv]
        if not (n or e or q or i or s):
            empty_run.append(lv)
            continue
        if empty_run:
            gaps.append((empty_run[0], empty_run[-1]))
            out.append(f"| {empty_run[0]}-{empty_run[-1]} | _yeni icerik yok_ | | | | |" if len(empty_run) > 1 else f"| {empty_run[0]} | _yeni icerik yok_ | | | | |")
            empty_run = []
        out.append(f"| {lv} | {short(n)} | {short(e)} | {short(q)} | {short(i)} | {short(s)} |")
    if empty_run:
        gaps.append((empty_run[0], empty_run[-1]))
        out.append(f"| {empty_run[0]}-{empty_run[-1]} | _yeni icerik yok_ | | | | |")
    out.append("")
    out.append("## Hic yeni icerik gelmeyen seviye araliklari\n")
    long_gaps = [(a, b) for a, b in gaps if b - a + 1 >= 3]
    if long_gaps:
        for a, b in long_gaps:
            out.append(f"- Lv{a}-{b} ({b - a + 1} seviye)")
    else:
        out.append("- (3+ seviyelik bos aralik yok)")
    out.append("")
    # Mob seviyesi kapsama: her seviye icin [L-8, L+3] penceresinde normal mob var mi
    uncovered = [lv for lv in range(1, max_level)
                 if not any(lv - 8 <= m["lv"] <= lv + 3 and not m["elite"] for m in mobs)]
    out.append("## Uygun normal mob olmayan seviyeler ([L-8, L+3] penceresi)\n")
    out.append("- " + (", ".join(f"Lv{x}" for x in uncovered) if uncovered else "yok"))
    out.append("")
    return "\n".join(out)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    text = build()
    if args.write:
        (ROOT / "docs" / "CONTENT_MAP.md").write_text(text, encoding="utf-8")
        print("docs/CONTENT_MAP.md yazildi")
    else:
        print(text)
