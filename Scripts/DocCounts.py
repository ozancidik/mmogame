#!/usr/bin/env python3
"""Dokuman toplam sayilari kodla uyumlu mu (statik; oyunu baslatmaz).

Kaynak: docs/CONTENT_MAP.md ilk satirlari ("Toplam: N mob (M elit), N gorev, N ekipman, N beceri.").
Karsilastirilanlar yalniz CANLI baslik satirlaridir (tarihsel notlar bilerek eski sayi tasir, taranmaz):
  - docs/PLAYTEST_PRIORITY.md ilk 8 satir: "(N mob, N gorev, N ekipman"
  - docs/ASSET_PROMPTS.md: "Koddaki **N beceri**"
CONTENT_MAP.md'nin kendisi Scripts/ContentMap.py --write ile uretilir; eski kalmasi ayri adimda yakalanir.

Kullanim: python Scripts/DocCounts.py [kok_dizin]    Cikis: 0 temiz, 1 sapma.
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def main():
    cm = read("docs/CONTENT_MAP.md")
    m = re.search(r"Toplam:\s*(\d+)\s+mob\s*\((\d+)\s+elit\),\s*(\d+)\s+g[oö]rev,\s*(\d+)\s+ekipman,\s*(\d+)\s+beceri", cm)
    if not m:
        print("HATA: CONTENT_MAP.md 'Toplam:' satiri okunamadi")
        return 1
    mobs, _elit, quests, gear, skills = (int(x) for x in m.groups())
    errors = []

    head = "\n".join(read("docs/PLAYTEST_PRIORITY.md").splitlines()[:8])
    h = re.search(r"\((\d+)\s+mob,\s*(\d+)\s+g[oö]rev(?:\s*\([^)]*\))?,\s*(\d+)\s+ekipman", head)
    if not h:
        errors.append("PLAYTEST_PRIORITY.md ilk 8 satirda '(N mob, N gorev, N ekipman' kalibi bulunamadi")
    else:
        for name, got, want in (("mob", int(h.group(1)), mobs), ("gorev", int(h.group(2)), quests), ("ekipman", int(h.group(3)), gear)):
            if got != want:
                errors.append(f"PLAYTEST_PRIORITY.md baslik: {name} {got}, kod/CONTENT_MAP {want}")
    q = re.search(r"(\d+)\s+g[oö]rev tan[ıi]m[ıi] var", head)
    if q and int(q.group(1)) != quests:
        errors.append(f"PLAYTEST_PRIORITY.md baslik: '{q.group(1)} gorev tanimi var', kod {quests}")

    ap = read("docs/ASSET_PROMPTS.md")
    a = re.search(r"Koddaki \*\*(\d+) beceri\*\*", ap)
    if not a:
        errors.append("ASSET_PROMPTS.md 'Koddaki **N beceri**' kalibi bulunamadi")
    elif int(a.group(1)) != skills:
        errors.append(f"ASSET_PROMPTS.md: beceri {a.group(1)}, kod {skills}")

    for e in errors:
        print("HATA:", e)
    print(f"Kod: {mobs} mob, {quests} gorev, {gear} ekipman, {skills} beceri  | sapma: {len(errors)}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
