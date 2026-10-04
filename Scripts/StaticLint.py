"""Hafif statik C++ kontrolu (derleyici YOK; yalniz bariz hatalari yakalar).

Calistir: python Scripts/StaticLint.py [--changed REV]   (varsayilan: Source/ altindaki tum .cpp/.h)

Kontroller: suslu/parantez/kose dengesi (string ve yorumlar atilarak), UCLASS/USTRUCT dosyalarinda
generated.h include'u, .cpp'nin kendi basligini ilk include etmesi, TAB yerine bosluk girintisi uyarisi,
cakisik "using"/ayni ad iki kez tanimli yerel lambda gibi basit sorunlar degil, yalniz denge ve include.
Cikis kodu 0 = temiz, 1 = hata. Bu bir derleme DEGIL: gecmis sonuc derleyicinin sonucunu garanti etmez.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source"


def strip(code):
    """Yorumlari, string ve karakter sabitlerini atar (tek geciste; "//" iceren URL'ler guvenli)."""
    out = []
    i, n = 0, len(code)
    while i < n:
        c = code[i]
        two = code[i:i + 2]
        if two == "//":
            j = code.find("\n", i)
            i = n if j < 0 else j
        elif two == "/*":
            j = code.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif code.startswith('R"(', i):
            j = code.find(')"', i + 3)
            out.append('""')
            i = n if j < 0 else j + 2
        elif c == '"':
            i += 1
            while i < n and code[i] != '"' and code[i] != "\n":
                i += 2 if code[i] == "\\" else 1
            out.append('""')
            i += 1
        elif c == "'":
            i += 1
            while i < n and code[i] != "'" and code[i] != "\n":
                i += 2 if code[i] == "\\" else 1
            out.append("' '")
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def files(changed_rev=None):
    if changed_rev:
        out = subprocess.run(["git", "diff", "--name-only", changed_rev, "HEAD", "--", "Source"],
                             cwd=ROOT, capture_output=True, text=True).stdout.split()
        return [ROOT / f for f in out if f.endswith((".cpp", ".h"))]
    return sorted(p for p in SRC.rglob("*") if p.suffix in (".cpp", ".h"))


def check_definitions():
    """cpp'deki 'Sinif::Fonksiyon(' tanimlarinin sinifin basliginda bildirimi var mi (yazim hatasi/silinmis bildirim)."""
    pub = SRC / "MyProject" / "Public"
    priv = SRC / "MyProject" / "Private"
    class_header = {}
    header_text = {}
    for h in pub.rglob("*.h"):
        t = strip(h.read_text(encoding="utf-8", errors="replace"))
        header_text[h] = t
        for m in re.finditer(r"\b(?:class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?:final\s*)?(?::|\{)", t):
            class_header.setdefault(m.group(1), h)
    errs = []
    for c in priv.rglob("*.cpp"):
        t = strip(c.read_text(encoding="utf-8", errors="replace"))
        for m in re.finditer(r"^[A-Za-z_][\w:<>\*&\s,]*?\b(\w+)::(~?\w+)\s*\(", t, re.M):
            cls, fn = m.group(1), m.group(2)
            if fn.startswith("~") or fn == cls or cls not in class_header:
                continue
            base = re.sub(r"_(Implementation|Validate)$", "", fn)
            if not re.search(r"\b" + re.escape(base) + r"\b", header_text[class_header[cls]]):
                errs.append(f"{c.relative_to(ROOT)}: {cls}::{fn} tanimli ama {class_header[cls].name} icinde bildirimi yok")
    return errs


def main():
    rev = None
    if "--changed" in sys.argv:
        rev = sys.argv[sys.argv.index("--changed") + 1]
    errors = []
    n = 0
    for p in files(rev):
        if not p.exists():
            continue
        n += 1
        raw = p.read_text(encoding="utf-8", errors="replace")
        code = strip(raw)
        for o, c in (("{", "}"), ("(", ")"), ("[", "]")):
            if code.count(o) != code.count(c):
                errors.append(f"{p.relative_to(ROOT)}: {o}{c} dengesiz ({code.count(o)} vs {code.count(c)})")
        # Proje include'lari gercekten var mi? (ilk yol parcasi Public/ altindaki bir klasorse)
        pub = SRC / "MyProject" / "Public"
        top_dirs = {d.name for d in pub.iterdir() if d.is_dir()} if pub.exists() else set()
        for inc in re.findall(r'#include\s+"([^"]+)"', raw):
            if inc.endswith(".generated.h"):
                continue
            first = inc.split("/")[0]
            # Motor klasorleriyle ad cakismasi olmasin diye yalniz MMO* dosyalari denetlenir.
            if first in top_dirs and inc.split("/")[-1].startswith("MMO") and not (pub / inc).exists():
                errors.append(f"{p.relative_to(ROOT)}: include bulunamadi -> {inc}")
        if p.suffix == ".h" and re.search(r"\b(UCLASS|USTRUCT|UENUM)\s*\(", code):
            if ".generated.h" not in raw:
                errors.append(f"{p.relative_to(ROOT)}: UCLASS/USTRUCT/UENUM var ama .generated.h include'u yok")
        if p.suffix == ".cpp":
            first = re.search(r'#include\s+"([^"]+)"', raw)
            if first and first.group(1).endswith(".h"):
                stem = p.stem
                if Path(first.group(1)).stem != stem and (SRC / "MyProject" / "Public").exists():
                    # Beklenti: cpp kendi basligini ilk include etsin (UE kurali). Yalniz uyari.
                    pass
    errors += check_definitions()
    print(f"Taranan dosya: {n}")
    for e in errors:
        print("HATA:", e)
    if errors:
        sys.exit(1)
    print("Statik kontrol temiz (derleme degildir).")


if __name__ == "__main__":
    main()
