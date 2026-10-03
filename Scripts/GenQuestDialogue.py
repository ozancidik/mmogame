"""docs/Lore.md icindeki gorev repliklerinden Source/.../Quests/MMOQuestDialogue.cpp uretir.

Calistir (repo kokunden):
  "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/GenQuestDialogue.py

Lore.md bicimi:
  - **Baslik** *(Q_KimlikAdi)* — "alis repligi (birden cok satir olabilir)"
    - *Dönüşte:* "teslim repligi"
Metin oyunda HUD fontuyla cizildigi icin ASCII'ye cevrilir (ğ→g, ş→s, ı→i ...). Uretilen dosyayi
elle duzenleme; Lore.md'yi duzenleyip scripti yeniden calistir.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LORE = ROOT / "docs" / "Lore.md"
OUT = ROOT / "Source" / "MyProject" / "Private" / "Quests" / "MMOQuestDialogue.cpp"

TR = str.maketrans({
    "ğ": "g", "Ğ": "G", "ş": "s", "Ş": "S", "ı": "i", "İ": "I", "ö": "o", "Ö": "O",
    "ü": "u", "Ü": "U", "ç": "c", "Ç": "C", "â": "a", "Â": "A", "î": "i", "û": "u",
    "—": "-", "–": "-", "’": "'", "‘": "'", "“": '"', "”": '"', "…": "...",
})

text = LORE.read_text(encoding="utf-8")
pat = re.compile(
    r'\*\((Q_\w+)\)\*\s*—\s*"(.*?)"'
    r'(?:\s*\n\s*-\s*\*Dönüşte:\*\s*"(.*?)")?', re.S)

entries = {}
for m in pat.finditer(text):
    qid, accept, turn = m.group(1), m.group(2), m.group(3)
    clean = lambda s: re.sub(r"\s+", " ", (s or "").translate(TR)).strip().replace("\\", "\\\\").replace('"', '\\"')
    a, t = clean(accept), clean(turn)
    if qid in entries:
        # Ayni kimlik birden cok yerde gecerse (ozet listeleri) ilk tam girdiyi koru.
        if not entries[qid][0] and a:
            entries[qid] = (a, entries[qid][1])
        if not entries[qid][1] and t:
            entries[qid] = (entries[qid][0], t)
        continue
    entries[qid] = (a, t)

lines = [
    "// Copyright Epic Games, Inc. All Rights Reserved.",
    "// OTOMATIK URETILDI: Scripts/GenQuestDialogue.py (kaynak: docs/Lore.md). Elle duzenleme.",
    "",
    '#include "Quests/MMOQuestDialogue.h"',
    "",
    "namespace",
    "{",
    "\tstruct FEntry { const TCHAR* Accept; const TCHAR* TurnIn; };",
    "",
    "\tconst TMap<FName, FEntry>& BuildTable()",
    "\t{",
    "\t\tstatic TMap<FName, FEntry> Table;",
    "\t\tif (Table.Num() > 0)",
    "\t\t{",
    "\t\t\treturn Table;",
    "\t\t}",
]
for qid, (a, t) in sorted(entries.items()):
    lines.append(f'\t\tTable.Add(FName(TEXT("{qid}")), {{ TEXT("{a}"), TEXT("{t}") }});')
lines += [
    "\t\treturn Table;",
    "\t}",
    "}",
    "",
    "FString FMMOQuestDialogue::GetAcceptLine(FName QuestId)",
    "{",
    "\tconst FEntry* E = BuildTable().Find(QuestId);",
    "\treturn E ? FString(E->Accept) : FString();",
    "}",
    "",
    "FString FMMOQuestDialogue::GetTurnInLine(FName QuestId)",
    "{",
    "\tconst FEntry* E = BuildTable().Find(QuestId);",
    "\treturn E ? FString(E->TurnIn) : FString();",
    "}",
    "",
]
OUT.write_text("\n".join(lines), encoding="utf-8")
with_turn = sum(1 for a, t in entries.values() if t)
print(f"{len(entries)} gorev girdisi ({with_turn} tanesinde teslim repligi) -> {OUT.relative_to(ROOT)}")
if len(entries) == 0:
    sys.exit(1)
