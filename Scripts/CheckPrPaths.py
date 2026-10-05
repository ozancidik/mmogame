"""PR diff'inde korunan yollar denetimi (CI; Unreal gerektirmez).

Calistir:
  python Scripts/CheckPrPaths.py <base_ref>            # git diff --name-only <base_ref>...HEAD
  python Scripts/CheckPrPaths.py --files a.cpp b.ini   # negatif test / elle deneme

HATA (cikis 1): Content/ (ikili .uasset/.umap), Config/ (proje ayarlari), MMOSaveGame.h (kayit bicimi).
  Bulut oturumlari bunlara dokunmaz (docs/CLOUD_TASKS.md); yerelde bilerek degistirildiyse PR'a
  'protected-paths-ok' etiketi eklenir (workflow ALLOW_PROTECTED_PATHS=1 verir, hatalar uyariya duser).
UYARI (cikis 0): MMOPartyComponent.* / MMOTradeComponent.* (yerelde Codex'in elinde; cakisma riski).
GitHub Actions'ta satirlar ::error / ::warning bicimiyle basilir (PR'da dosya uzerinde gorunur).
"""
import os
import subprocess
import sys

ERROR_RULES = [
    ("Content/", "Content/ ikili icerik: bulutta/PR'da elle degistirilmez"),
    ("Config/", "Config/ proje ayari: bulut kapsami disi"),
]
ERROR_NAMES = {"MMOSaveGame.h": "kayit bicimi (MMOSaveGame.h): eski .sav uyumlulugu"}
WARN_STEMS = {"MMOPartyComponent": "parti bileseni yerelde Codex'te", "MMOTradeComponent": "ticaret bileseni yerelde Codex'te"}


def classify(path):
    """('error'|'warn'|None, neden)"""
    p = path.replace("\\", "/")
    for prefix, why in ERROR_RULES:
        if p.startswith(prefix):
            return "error", why
    name = p.rsplit("/", 1)[-1]
    if name in ERROR_NAMES:
        return "error", ERROR_NAMES[name]
    stem = name.split(".", 1)[0]
    if stem in WARN_STEMS:
        return "warn", WARN_STEMS[stem]
    return None, ""


def changed_files(base):
    out = subprocess.run(["git", "diff", "--name-only", "%s...HEAD" % base],
                         capture_output=True, text=True, check=True).stdout
    return [l for l in out.splitlines() if l.strip()]


def main(argv):
    if not argv:
        print("Kullanim: CheckPrPaths.py <base_ref> | --files <yol...>")
        return 2
    files = argv[1:] if argv[0] == "--files" else changed_files(argv[0])
    allow = os.environ.get("ALLOW_PROTECTED_PATHS") == "1"
    gha = os.environ.get("GITHUB_ACTIONS") == "true"
    errors = warns = 0
    for f in files:
        kind, why = classify(f)
        if kind is None:
            continue
        if kind == "error" and not allow:
            errors += 1
            print(("::error file=%s::%s" if gha else "HATA: %s: %s") % (f, why))
        else:
            warns += 1
            print(("::warning file=%s::%s" if gha else "UYARI: %s: %s") % (f, why))
    print("PR yol denetimi: %d dosya, %d hata, %d uyari%s" % (
        len(files), errors, warns, " (protected-paths-ok: hatalar uyariya dustu)" if allow else ""))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
