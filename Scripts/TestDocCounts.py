#!/usr/bin/env python3
"""DocCounts.py negatif testi: gecici kopyada baslik sayilarini boz, betik her birini yakalamali; temiz kopya gecmeli."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, "Scripts", "DocCounts.py")
FILES = ["docs/CONTENT_MAP.md", "docs/PLAYTEST_PRIORITY.md", "docs/ASSET_PROMPTS.md"]


def run(root):
    p = subprocess.run([sys.executable, "-I", TOOL, root], capture_output=True, text=True)
    return p.returncode, p.stdout


def make_copy(d, mutate=None):
    for rel in FILES:
        dst = os.path.join(d, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(ROOT, rel), dst)
    if mutate:
        rel, pattern, repl = mutate
        path = os.path.join(d, rel)
        with open(path, encoding="utf-8") as f:
            text = f.read()
        new, n = re.subn(pattern, repl, text, count=1)
        if n != 1:
            return False
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
    return True


def main():
    failures = []
    with tempfile.TemporaryDirectory() as d:
        make_copy(d)
        code, out = run(d)
        if code != 0:
            failures.append(f"temiz kopya hata verdi: {out.strip()}")

    cases = [
        ("PLAYTEST_PRIORITY gorev sayisi", ("docs/PLAYTEST_PRIORITY.md", r"\(59 mob, 97 g", "(59 mob, 90 g")),
        ("PLAYTEST_PRIORITY mob sayisi", ("docs/PLAYTEST_PRIORITY.md", r"\(59 mob,", "(48 mob,")),
        ("ASSET_PROMPTS beceri sayisi", ("docs/ASSET_PROMPTS.md", r"Koddaki \*\*93 beceri\*\*", "Koddaki **81 beceri**")),
        ("PLAYTEST_PRIORITY 'gorev tanimi var'", ("docs/PLAYTEST_PRIORITY.md", r"97 g[oö]rev tan", "90 görev tan")),
    ]
    for name, mutate in cases:
        with tempfile.TemporaryDirectory() as d:
            if not make_copy(d, mutate):
                failures.append(f"{name}: mutasyon hedefi bulunamadi (dokuman degismis olabilir; testi guncelle)")
                continue
            code, out = run(d)
            if code == 0 or "HATA:" not in out:
                failures.append(f"{name}: sapma yakalanmadi (cikis={code}): {out.strip()}")

    if failures:
        for x in failures:
            print("KALDI:", x)
        return 1
    print("DocCounts negatif testi GECTI (temiz=0, 4 sapma yakalandi)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
