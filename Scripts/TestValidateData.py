"""ValidateData.py oynanabilirlik denetimlerinin negatif testi (CI; Unreal gerektirmez).

Source/ ve ValidateData.py gecici klasore kopyalanir; her senaryoda TEK bir kasitli bozulma yapilir ve beklenen
HATA/UYARI satiri aranir. Bozulmamis kopya temiz (cikis 0) olmali.
Calistir: python Scripts/TestValidateData.py     Cikis 0 = tum senaryolar yakalandi.
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
P = "Source/MyProject/Private/"
GM = P + "Core/MMOGameMode.cpp"
ITEMS = P + "Items/MMOItemDatabase.cpp"
TUNING = P + "Core/MMOXPTuning.cpp"

# (ad, dosya, desen, yerine, beklenen cikti parcasi, HATA mi)
CASES = [
    ("Gorev hedefi uretilmiyor (elit spawner'i baska elite cevrildi)", GM,
     r'(EliteSpawner->ForcedArchetypeId = FName\(TEXT\(")SpiderQueen', r"\1StoneGolem",
     "hedef SpiderQueen (elit (yalniz ForcedArchetypeId ile dogar)) hicbir spawner'da uretilmiyor", True),
    ("Bos spawner bandi", GM, r"ThirdZone->MinMobLevel = 8;", "ThirdZone->MinMobLevel = 13;",
     "Lv13-12 bandinda elit olmayan mob yok", True),
    ("Lambda spawner bandi bos", GM, r"SpawnRift\(FVector\(800\.f, -4800\.f, 50\.f\), 58, 60,",
     "SpawnRift(FVector(800.f, -4800.f, 50.f), 99, 99,", "Lv99-99 bandinda elit olmayan mob yok", True),
    ("Zorunlu arketip tabloda yok", GM, r'(BossSpawner->ForcedArchetypeId = FName\(TEXT\(")OrcWarlord', r"\1OrcWarlrod",
     "ForcedArchetypeId OrcWarlrod mob tablosunda yok", True),
    ("Odul esyasi MaxLevel ustu", ITEMS, r'(TEXT\("MithrilBoots"\), TEXT\("[^"]*"\), S::Boots, 0\.f, 16\.f, )10\)',
     r"\g<1>85)", "odul MithrilBoots Lv85 gerektiriyor, MaxLevel 80", True),
    ("Satici yuva boslugu", ITEMS, r'(TEXT\("AdamantineBoots"\), TEXT\("[^"]*"\), S::Boots, 0\.f, 22\.f, )16\)',
     r"\g<1>24)", "Boots yuvasinda Lv10 -> Lv24 arasi yeni esya yok", False),
    ("XP knot sirasi bozuk", TUNING, r"\{10, 0\.25f\}", "{7, 0.25f}", "GMobKnots seviyeler kesin artan degil", True),
    ("XP knot carpani sifir", TUNING, r"\{12, 0\.08f\}, \{16, 0\.10f\}, \{20, 0\.18f\}",
     "{12, 0.08f}, {16, 0.0f}, {20, 0.18f}", "GQuestKnots Lv16 carpani 0.0", True),
    ("XP knot araligi disi", TUNING, r"\{34, 1\.00f\}\n\t\};\n\t// XP_TUNING_QUEST", "{90, 1.00f}\n\t};\n\t// XP_TUNING_QUEST",
     "GMobKnots knot Lv90 1..80 disinda", True),
    ("Parti bonusu araligi disi", TUNING, r"GPartyBonusPerExtraMember = 0\.10f", "GPartyBonusPerExtraMember = 0.90f",
     "parti bonusu 0.90 [0, 0.5] disinda", True),
]

# Beceri boslugu (UYARI) senaryolari: MMOSkillDatabase.cpp'deki her MakeDef'in ReqLevel alani bir eslemeyle yeniden
# yazilir (sahte seviye tablosu). Gercek seviyeler: 1-28 arasi sik, sonra 33, 45, 58, 70, 80.
SKILLS = P + "Skills/MMOSkillDatabase.cpp"
SKILL_LVL = re.compile(r'(MakeDef\(TEXT\("[^"]+"\),\s*TEXT\("[^"]*"\),\s*C::(\w+),\s*T::\w+,\s*\d+,\s*)(\d+)')
# Tum farklar <= 4. 52/64/75: Lv46-80 bosluk dolgusu becerileri (SKILL_LVL esler; once 80 -> 43, sonra 52 -> 46,
# 64 -> 49, 75 -> 52 sirasi korunur).
GAPLESS = {33: 31, 45: 34, 58: 37, 70: 40, 80: 43, 52: 46, 64: 49, 75: 52}
GAP_RE = re.compile(r"UYARI: \w+: beceri boslugu ")


def remap_skills(src, warrior_extra=None):
    def sub(m):
        lv = int(m.group(3))
        lv = GAPLESS.get(lv, lv)
        if warrior_extra and m.group(2) == "Warrior":
            lv = warrior_extra.get(lv, lv)
        return m.group(1) + str(lv)
    return SKILL_LVL.sub(sub, src)


# (ad, donusum, cikti icinde OLMASI gereken satirlar, beklenen toplam bosluk uyarisi sayisi)
SKILL_CASES = [
    ("Bosluksuz sahte tablo -> bosluk uyarisi yok", remap_skills, [], 0),
    ("Lv30 alti bosluk tek uyari (eski Lv1-30 kurali ile cift sayilmaz)", lambda s: remap_skills(s, {24: 22}),
     ["UYARI: Warrior: beceri boslugu Lv22 -> Lv28 (Lv23-27 arasi yeni beceri yok, 6 seviye)"], 1),
    ("Kasitli bosluk (Warrior Lv52 -> Lv60)", lambda s: remap_skills(s, {52: 60}),
     ["UYARI: Warrior: beceri boslugu Lv49 -> Lv60 (Lv50-59 arasi yeni beceri yok, 11 seviye)"], 1),
    ("Esik siniri (fark 4 uyarmaz, fark 5 uyarir)", lambda s: remap_skills(s, {43: 45}),
     ["UYARI: Warrior: beceri boslugu Lv40 -> Lv45 (Lv41-44 arasi yeni beceri yok, 5 seviye)"], 1),
]


def run(tmp):
    r = subprocess.run([sys.executable, str(tmp / "Scripts" / "ValidateData.py")], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = Path(td) / "base"
        shutil.copytree(ROOT / "Source", base / "Source")
        (base / "Scripts").mkdir()
        shutil.copy(ROOT / "Scripts" / "ValidateData.py", base / "Scripts" / "ValidateData.py")
        code, out = run(base)
        if code != 0:
            failures.append("bozulmamis kopya temiz degil:\n" + out)
        for name, rel, pat, rep, expect, hard in CASES:
            tmp = Path(td) / "case"
            if tmp.exists():
                shutil.rmtree(tmp)
            shutil.copytree(base, tmp)
            f = tmp / rel
            src = f.read_text(encoding="utf-8")
            new, n = re.subn(pat, rep, src, count=1)
            if n == 0 or new == src:
                failures.append(f"[{name}] bozulma uygulanamadi (desen bulunamadi: {pat})")
                continue
            f.write_text(new, encoding="utf-8")
            code, out = run(tmp)
            if expect not in out:
                failures.append(f"[{name}] beklenen satir yok: '{expect}'\n{out}")
            elif hard and code != 1:
                failures.append(f"[{name}] HATA bekleniyordu ama cikis kodu {code}")
            elif not hard and code != 0:
                failures.append(f"[{name}] yalniz UYARI bekleniyordu ama cikis kodu {code}\n{out}")
            else:
                print(f"yakalandi: {name}")
        for name, fn, expects, count in SKILL_CASES:
            tmp = Path(td) / "case"
            if tmp.exists():
                shutil.rmtree(tmp)
            shutil.copytree(base, tmp)
            f = tmp / SKILLS
            src = f.read_text(encoding="utf-8")
            if not SKILL_LVL.search(src):
                failures.append(f"[{name}] MakeDef seviye deseni bulunamadi")
                continue
            f.write_text(fn(src), encoding="utf-8")
            code, out = run(tmp)
            gaps = [l for l in out.splitlines() if GAP_RE.match(l)]
            missing = [e for e in expects if e not in out]
            if code != 0:
                failures.append(f"[{name}] yalniz UYARI bekleniyordu ama cikis kodu {code}\n{out}")
            elif missing:
                failures.append(f"[{name}] beklenen satir yok: {missing}\n{out}")
            elif len(gaps) != count or len(set(gaps)) != len(gaps):
                failures.append(f"[{name}] {count} bosluk uyarisi bekleniyordu, {len(gaps)} geldi:\n" + "\n".join(gaps))
            else:
                print(f"yakalandi: {name}")
    for f in failures:
        print("KALDI:", f)
    if failures:
        return 1
    print(f"ValidateData negatif testi gecti ({len(CASES) + len(SKILL_CASES)} senaryo).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
