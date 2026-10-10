#!/usr/bin/env python3
"""PocketSpacing.py negatif testi: gecici kopyada bir cebi komşusunun yanina tasi, betik hata vermeli; temiz dosya hata vermemeli."""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Source", "MyProject", "Private", "Core", "MMOGameMode.cpp")
TOOL = os.path.join(ROOT, "Scripts", "PocketSpacing.py")


def run(path):
    p = subprocess.run([sys.executable, "-I", TOOL, path], capture_output=True, text=True)
    return p.returncode, p.stdout


def main():
    with open(SRC, encoding="utf-8", errors="replace") as f:
        text = f.read()
    failures = []

    code, out = run(SRC)
    if code != 0:
        failures.append(f"temiz dosya hata verdi: {out.strip()}")

    # P48 cebini (4500,-700) Gozcu cebinin (3250,500) 300u yanina tasi -> HATA beklenir.
    mutated, n = re.subn(r"FVector\(4500\.f, -700\.f, 120\.f\)", "FVector(3450.f, 500.f, 120.f)", text)
    if n != 1:
        failures.append("mutasyon hedefi bulunamadi (P48 cebi tasinmis olabilir; testi guncelle)")
    else:
        with tempfile.TemporaryDirectory() as d:
            tmp = os.path.join(d, "MMOGameMode.cpp")
            with open(tmp, "w", encoding="utf-8") as f:
                f.write(mutated)
            code, out = run(tmp)
            if code == 0 or "HATA:" not in out:
                failures.append(f"yakin cep yakalanmadi (cikis={code}): {out.strip()}")

    # Yorumdaki sahte cagri sayilmamali: yorum satirina yakin bir cep ekle, temiz kalmali.
    with tempfile.TemporaryDirectory() as d:
        tmp = os.path.join(d, "MMOGameMode.cpp")
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(text + "\n// SpawnFrostbound(FVector(3300.f, 500.f, 0.f), 1, 1, 225.f, 6, NAME_None, 0.f);\n")
        code, out = run(tmp)
        if code != 0:
            failures.append(f"yorumdaki cagri hata saydi: {out.strip()}")

    if failures:
        for x in failures:
            print("KALDI:", x)
        return 1
    print("PocketSpacing negatif testi GECTI (temiz=0, yakin cep=1, yorum yok sayildi)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
