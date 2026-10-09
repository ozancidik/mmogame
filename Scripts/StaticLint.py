"""Hafif statik C++ kontrolu (derleyici YOK; yalniz bariz hatalari yakalar).

Calistir: python Scripts/StaticLint.py [--changed REV]   (varsayilan: Source/ altindaki tum .cpp/.h)

Kontroller: suslu/parantez/kose dengesi (string ve yorumlar atilarak), UCLASS/USTRUCT dosyalarinda
generated.h include'u, cpp tanimi <-> baslik bildirimi, ve UE kaliplari:
  - RPC (Server/Client/NetMulticast) ve BlueprintNativeEvent: Reliable/Unreliable, void donus, const olmayan
    referans parametre, _Implementation tanimi, WithValidation <-> _Validate eslesmesi, dogrudan tanim yasagi;
  - ReplicatedUsing=OnRep_X <-> UFUNCTION() bildirimi + cpp tanimi; her Replicated ozellik
    GetLifetimeReplicatedProps icinde DOREPLIFETIME ile kayitli; kayitli ama replike olmayan ozellik; olu OnRep;
  - motor override imzalari OVERRIDE_WHITELIST'e gore (PreLogin, ChangeName, TakeDamage...); liste disi override UYARI;
  - kullanilan tur/cagri icin eksik include (INCLUDE_RULES; DOREPLIFETIME, TimerManager, TActorIterator... HATA,
    dolayli gelebilenler UYARI). Negatif test: Scripts/TestStaticLint.py.
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


def check_declarations():
    """Basliktaki sinif uyesi bildirimlerinin (govdesiz) hicbir cpp'de tanimi yoksa UYARI (olasi link hatasi)."""
    pub = SRC / "MyProject" / "Public"
    priv = SRC / "MyProject" / "Private"
    defs = set()
    for c in priv.rglob("*.cpp"):
        t = strip(c.read_text(encoding="utf-8", errors="replace"))
        for m in re.finditer(r"\b(\w+)::(~?\w+)\s*\(", t):
            defs.add((m.group(1), m.group(2)))
    warns = []
    skip = {"UPROPERTY", "UFUNCTION", "GENERATED_BODY", "check"}
    for h in sorted(pub.rglob("*.h")):
        t = strip(h.read_text(encoding="utf-8", errors="replace"))
        for cm in re.finditer(r"\bclass\s+(?:\w+_API\s+)?(\w+)\s*(?::[^{;]*)?\{", t):
            cls = cm.group(1)
            i, depth = cm.end(), 1
            while depth and i < len(t):
                depth += (t[i] == "{") - (t[i] == "}")
                i += 1
            body = t[cm.end():i - 1]
            flat, d = "", 0
            for ch in body:
                if ch == "{":
                    if d == 0:
                        flat += "{}"
                    d += 1
                elif ch == "}":
                    d -= 1
                elif d == 0:
                    flat += ch
            for m in re.finditer(r"\b(\w+)\s*\(([^;{}]*?)\)\s*(?:const)?\s*(?:override|final)?\s*;", flat):
                name = m.group(1)
                if name in skip or (cls, name) in defs or (cls, name + "_Implementation") in defs:
                    continue
                if re.match(r"[FTUAE][A-Z]", name):  # tur adi / kurucu cagrisi ("FBox2D Rect = FBox2D(...)")
                    continue
                msg = f"{h.name}: {cls}::{name} bildirilmis ama hicbir cpp'de tanimi yok (olasi link hatasi)"
                if msg not in warns:
                    warns.append(msg)
    return warns


# ---------------------------------------------------------------------------
# UE kalip denetimleri (derleyici/UHT olmadan, kaynak metninden). Yalniz bu projenin kaliplarini
# hedefler; yanlis alarm cikarsa kural gevsetilir (bkz. negatif testler, Scripts/TestStaticLint.py).
# ---------------------------------------------------------------------------

def balanced(t, i, o="(", c=")"):
    """t[i] == o ise eslesen kapanisin indeksini dondurur (yoksa -1)."""
    depth = 0
    for j in range(i, len(t)):
        if t[j] == o:
            depth += 1
        elif t[j] == c:
            depth -= 1
            if depth == 0:
                return j
    return -1


def class_bodies(t):
    """Basliktaki (class|struct) Ad ... { govde } -> [(ad, ust_sinif_listesi, govde)]."""
    out = []
    for cm in re.finditer(r"\b(?:class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?:final\s*)?(:[^{;]*)?\{", t):
        start = cm.end() - 1
        end = balanced(t, start, "{", "}")
        if end < 0:
            continue
        bases = re.findall(r"\b(?:public|protected|private)\s+(\w+)", cm.group(2) or "")
        out.append((cm.group(1), bases, t[start + 1:end]))
    return out


def project_headers():
    return sorted((SRC / "MyProject").rglob("*.h"))


def cpp_definitions():
    """Tum cpp'lerde 'Sinif::Fonksiyon(...) [const] {' govdeli tanimlar -> {(sinif, fn): [(dosya, govde)]}."""
    defs = {}
    for c in sorted((SRC / "MyProject").rglob("*.cpp")):
        t = strip(c.read_text(encoding="utf-8", errors="replace"))
        for m in re.finditer(r"\b(\w+)::(~?\w+)\s*\(", t):
            close = balanced(t, m.end() - 1)
            if close < 0:
                continue
            k = close + 1
            tail = re.match(r"\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?", t[k:])
            k += tail.end()
            if k < len(t) and t[k] == ":" and t[k:k + 2] != "::":  # kurucu baslatici listesi
                brace = t.find("{", k)
                k = brace if brace >= 0 else k
            if k < len(t) and t[k] == "{":
                end = balanced(t, k, "{", "}")
                defs.setdefault((m.group(1), m.group(2)), []).append((c, t[k:end + 1] if end > 0 else "", t[m.end():close]))
    return defs


def ufunctions(body):
    """Sinif govdesindeki UFUNCTION(...) bildirimleri -> [(belirtecler, donus, ad, parametreler)]."""
    out = []
    for m in re.finditer(r"\bUFUNCTION\s*\(", body):
        close = balanced(body, m.end() - 1)
        spec = body[m.end():close]
        d = re.match(r"\s*(?:virtual\s+|static\s+|FORCEINLINE\s+)*([\w:<>,\s\*&]+?)\s*\b(\w+)\s*\(", body[close + 1:])
        if not d:
            continue
        pstart = close + 1 + d.end() - 1
        pend = balanced(body, pstart)
        out.append((spec, d.group(1).strip(), d.group(2), body[pstart + 1:pend]))
    return out


def norm_params(params):
    """'const FString& Options, class AController* C = nullptr' -> ['const FString&', 'AController*']."""
    out = []
    depth, cur, parts = 0, "", []
    for ch in params:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur)
    for p in parts:
        p = p.split("=")[0]
        p = re.sub(r"\b(class|struct)\s+", "", p)
        p = re.sub(r"\s+", " ", p).strip()
        p = re.sub(r"\s*([\*&])\s*", r"\1 ", p).strip()
        toks = p.split(" ")
        if len(toks) > 1 and re.match(r"^\w+$", toks[-1]) and not toks[-1] in ("const",):
            toks = toks[:-1]  # parametre adi
        p = " ".join(toks).replace(" *", "*").replace(" &", "&").strip()
        m = re.match(r"^(\w[\w:<>, ]*) const([\*&]+)$", p)  # 'FDamageEvent const&' -> 'const FDamageEvent&'
        if m:
            p = "const %s%s" % (m.group(1), m.group(2))
        if p and p != "void":
            out.append(p)
    return out


# UE 5.7 motor sanallari: ad -> (parametre turleri, const mu). Yalniz projede override edilenler.
# Yeni bir motor override'i eklenince imzasi UE 5.7 kaynagindan dogrulanip buraya yazilir.
OVERRIDE_WHITELIST = {
    "BeginPlay": ([], False),
    "EndPlay": (["const EEndPlayReason::Type"], False),
    "Tick": (["float"], False),
    "TickComponent": (["float", "ELevelTick", "FActorComponentTickFunction*"], False),
    "GetLifetimeReplicatedProps": (["TArray<FLifetimeProperty>&"], True),
    "StartPlay": ([], False),
    "PreLogin": (["const FString&", "const FString&", "const FUniqueNetIdRepl&", "FString&"], False),
    "PostLogin": (["APlayerController*"], False),
    "Logout": (["AController*"], False),
    "ChangeName": (["AController*", "const FString&", "bool"], False),
    "InitGame": (["const FString&", "const FString&", "FString&"], False),
    "InitGameState": ([], False),  # AGameModeBase::InitGameState() UE 5.7 GameModeBase.h:69 (2026-10-09 dogrulandi)
    "SetupPlayerInputComponent": (["UInputComponent*"], False),
    "SetupInputComponent": ([], False),
    "PlayerTick": (["float"], False),
    "PossessedBy": (["AController*"], False),
    "UnPossessed": ([], False),
    "OnPossess": (["APawn*"], False),
    "OnUnPossess": ([], False),
    "PawnClientRestart": ([], False),
    "SeamlessTravelTo": (["APlayerController*"], False),
    "DrawHUD": ([], False),
    "PostInitializeComponents": ([], False),
    "TakeDamage": (["float", "const FDamageEvent&", "AController*", "AActor*"], False),
    "Initialize": (["FSubsystemCollectionBase&"], False),
    "Deinitialize": ([], False),
    "NotifyActorBeginOverlap": (["AActor*"], False),
    "OnConstruction": (["const FTransform&"], False),
}


def check_ue_patterns():
    """RPC eslesmesi, ReplicatedUsing/OnRep/GetLifetimeReplicatedProps, override imza beyaz listesi.
    -> (hatalar, uyarilar)"""
    errs, warns = [], []
    defs = cpp_definitions()
    classes = {}  # ad -> (baslik, ust, govde)
    for h in project_headers():
        t = strip(h.read_text(encoding="utf-8", errors="replace"))
        for name, bases, body in class_bodies(t):
            classes.setdefault(name, (h, bases, body))

    project_virtuals = set()
    for name, (h, bases, body) in classes.items():
        for m in re.finditer(r"\bvirtual\s+[\w:<>,\s\*&]+?\b(\w+)\s*\([^;{]*\)[^;{]*?(?:;|\{)", body):
            if not re.search(r"\boverride\b", body[m.start():m.end()]):
                project_virtuals.add(m.group(1))

    def rel(p):
        return p.relative_to(ROOT)

    def ancestors(cls):
        seen, todo = [], [cls]
        while todo:
            c = todo.pop()
            if c in classes and c not in seen:
                seen.append(c)
                todo += classes[c][1]
        return seen

    for cls, (h, bases, body) in sorted(classes.items()):
        # --- RPC ve BlueprintNativeEvent
        for spec, ret, fn, params in ufunctions(body):
            sp = set(re.findall(r"\b(\w+)\b", spec))
            is_rpc = bool(sp & {"Server", "Client", "NetMulticast"})
            if is_rpc and not (sp & {"Reliable", "Unreliable"}):
                errs.append(f"{rel(h)}: {cls}::{fn} RPC'sinde Reliable/Unreliable yok (UHT hatasi)")
            if is_rpc and ret != "void":
                errs.append(f"{rel(h)}: {cls}::{fn} RPC'si void dondurmeli (UHT hatasi), donus '{ret}'")
            if is_rpc:
                for pt in norm_params(params):
                    if pt.endswith("&") and not pt.startswith("const "):
                        errs.append(f"{rel(h)}: {cls}::{fn} RPC parametresi const olmayan referans '{pt}' (UHT hatasi)")
            if is_rpc or "BlueprintNativeEvent" in sp:
                if (cls, fn + "_Implementation") not in defs:
                    errs.append(f"{rel(h)}: {cls}::{fn}_Implementation hicbir cpp'de tanimli degil (link hatasi)")
                if (cls, fn) in defs:
                    f0 = defs[(cls, fn)][0][0]
                    errs.append(f"{rel(f0)}: {cls}::{fn} dogrudan tanimlanmis; RPC/NativeEvent govdesi {fn}_Implementation olmali")
            if "WithValidation" in sp and (cls, fn + "_Validate") not in defs:
                errs.append(f"{rel(h)}: {cls}::{fn} WithValidation ama {fn}_Validate tanimi yok (link hatasi)")
            if "WithValidation" not in sp and (cls, fn + "_Validate") in defs:
                f0 = defs[(cls, fn + "_Validate")][0][0]
                errs.append(f"{rel(f0)}: {cls}::{fn}_Validate tanimli ama UFUNCTION'da WithValidation yok (bildirimsiz tanim)")

        # --- Replikasyon
        ufn_names = {fn for _, _, fn, _ in ufunctions(body)}
        rep_props = {}
        for m in re.finditer(r"\bUPROPERTY\s*\(", body):
            close = balanced(body, m.end() - 1)
            spec = body[m.end():close]
            using = re.search(r"\bReplicatedUsing\s*=\s*(\w+)", spec)
            if not (using or re.search(r"\bReplicated\b", spec)):
                continue
            d = re.match(r"\s*([^;]*?)\b(\w+)\s*(?:=[^;]*|\{[^;]*\}|\[[^;]*\])?\s*;", body[close + 1:])
            if d:
                rep_props[d.group(2)] = using.group(1) if using else ""
        for prop, onrep in sorted(rep_props.items()):
            if onrep:
                if onrep not in ufn_names and not any(onrep in {f for _, _, f, _ in ufunctions(classes[a][2])} for a in ancestors(cls)[1:]):
                    errs.append(f"{rel(h)}: {cls}::{prop} ReplicatedUsing={onrep} ama {onrep} UFUNCTION() olarak bildirilmemis (UHT hatasi)")
                if not any((a, onrep) in defs for a in ancestors(cls)):
                    errs.append(f"{rel(h)}: {cls}::{onrep} (OnRep) hicbir cpp'de tanimli degil (link hatasi)")
        registered = set()
        glrp = defs.get((cls, "GetLifetimeReplicatedProps"), [])
        for _, gbody, _ in glrp:
            for m in re.finditer(r"\bDOREPLIFETIME\w*\s*\(\s*(\w+)\s*,\s*(\w+)", gbody):
                rc, rp = m.group(1), m.group(2)
                if rc != cls:
                    errs.append(f"{cls}::GetLifetimeReplicatedProps: DOREPLIFETIME({rc}, {rp}) sinif adi yanlis")
                elif rp not in rep_props:
                    errs.append(f"{cls}::GetLifetimeReplicatedProps: {rp} kayitli ama UPROPERTY(Replicated...) degil (calisma aninda assert)")
                registered.add(rp)
            if "Super::GetLifetimeReplicatedProps" not in gbody:
                warns.append(f"{cls}::GetLifetimeReplicatedProps Super:: cagirmiyor (ust sinif ozellikleri replike olmaz)")
        if rep_props and not glrp:
            errs.append(f"{rel(h)}: {cls} replike ozellik tasiyor ama GetLifetimeReplicatedProps tanimi yok")
        for prop in sorted(set(rep_props) - registered):
            if glrp:
                errs.append(f"{rel(h)}: {cls}::{prop} Replicated ama GetLifetimeReplicatedProps icinde DOREPLIFETIME yok (calisma aninda assert)")
        used_onreps = {v for v in rep_props.values() if v}
        for _, _, fn, _ in ufunctions(body):
            if fn.startswith("OnRep_") and fn not in used_onreps:
                # alt siniflar da kullanabilir; yalniz hic kimse kullanmiyorsa
                if not any(fn == v for c2 in classes.values() for v in re.findall(r"ReplicatedUsing\s*=\s*(\w+)", c2[2])):
                    warns.append(f"{rel(h)}: {cls}::{fn} hicbir ReplicatedUsing tarafindan kullanilmiyor (olu OnRep)")

        # --- override imza beyaz listesi
        flat, depth = "", 0
        for ch in body:
            if ch == "{":
                if depth == 0:
                    flat += ";"
                depth += 1
            elif ch == "}":
                depth -= 1
            elif depth == 0:
                flat += ch
        for m in re.finditer(r"\b(\w+)\s*\(", flat):
            name = m.group(1)
            close = balanced(flat, m.end() - 1)
            if close < 0:
                continue
            tail = re.match(r"\s*(const\s*)?(?:override|final)\b", flat[close + 1:])
            if not tail:
                continue
            if name in OVERRIDE_WHITELIST:
                want, want_const = OVERRIDE_WHITELIST[name]
                got = norm_params(flat[m.end():close])
                if got != want or bool(tail.group(1)) != want_const:
                    errs.append(f"{rel(h)}: {cls}::{name} override imzasi UE 5.7 ile uyusmuyor: ({', '.join(got)}){' const' if tail.group(1) else ''} != ({', '.join(want)}){' const' if want_const else ''}")
                for f, _, dparams in defs.get((cls, name), []):
                    if norm_params(dparams) != want:
                        errs.append(f"{rel(f)}: {cls}::{name} tanim imzasi bildirimle uyusmuyor: ({', '.join(norm_params(dparams))})")
            elif name not in project_virtuals and not name.endswith("_Implementation"):
                warns.append(f"{rel(h)}: {cls}::{name} override beyaz listede yok; imzayi UE 5.7'de dogrulayip OVERRIDE_WHITELIST'e ekle")
    return errs, warns


# Kullanilan tur/cagri -> gereken baslik. Dosya + kendi basligi + projeden (dolayli) include ettikleri taranir.
# (desen, kabul edilen basliklar, hata mi). "Engine.h"/"EngineMinimal.h" her seyi karsilar.
INCLUDE_RULES = [
    (r"\bDOREPLIFETIME", {"Net/UnrealNetwork.h"}, True),
    (r"\bTActor(?:Iterator|Range)\s*<", {"EngineUtils.h"}, True),
    (r"\bUGameplayStatics::", {"Kismet/GameplayStatics.h"}, True),
    (r"\bGet(?:World)?TimerManager\(\)\s*\.\s*\w", {"TimerManager.h"}, True),
    (r"\bGEngine\s*->", {"Engine/Engine.h"}, True),
    (r"\bDrawDebug(?:Line|Sphere|Box|String|Point|Circle|Capsule)\s*\(", {"DrawDebugHelpers.h"}, True),
    (r"\bUNavigationSystemV1\b", {"NavigationSystem.h"}, True),
    (r"\bGetCharacterMovement\(\)\s*->", {"GameFramework/CharacterMovementComponent.h"}, True),
    (r"\bGetCapsuleComponent\(\)\s*->", {"Components/CapsuleComponent.h"}, True),
    (r"\bConstructorHelpers::", {"UObject/ConstructorHelpers.h"}, True),
    (r"\bUMaterialInstanceDynamic::Create\b", {"Materials/MaterialInstanceDynamic.h"}, True),
    (r"\bFJsonSerializer::", {"Serialization/JsonSerializer.h"}, True),
    (r"\bFHttpModule::", {"HttpModule.h"}, True),
    (r"\bGetWorld\(\)\s*->", {"Engine/World.h"}, False),
    (r"\bFTimerHandle\b", {"Engine/TimerHandle.h", "TimerManager.h", "Engine/EngineTypes.h"}, False),
    (r"\bTSet\s*<", {"CoreMinimal.h", "Containers/Set.h"}, False),
    (r"\bTPair\s*<", {"CoreMinimal.h", "Templates/Tuple.h"}, False),
    (r"\bF(?:Point|Radial)?DamageEvent\s+\w+\s*[;({=]", {"Engine/DamageEvents.h"}, False),
    (r"\bCanvas\s*->", {"Engine/Canvas.h"}, False),
]
# Motor basliklarinin bilinen dolayli include'lari (derlenmis koddan kanit; yalnizca gozlenmis olanlar).
ENGINE_TRANSITIVE = {
    "GameFramework/Actor.h": {"Engine/EngineTypes.h"},
    "Components/ActorComponent.h": {"Engine/EngineTypes.h"},
    "GameFramework/Character.h": {"Engine/EngineTypes.h"},
    "GameFramework/Pawn.h": {"Engine/EngineTypes.h"},
    "GameFramework/PlayerController.h": {"Engine/EngineTypes.h"},
    "GameFramework/HUD.h": {"Engine/EngineTypes.h"},
    "GameFramework/GameModeBase.h": {"Engine/EngineTypes.h"},
}


def check_includes():
    errs, warns = [], []
    proj = SRC / "MyProject"
    pub = proj / "Public"
    cache = {}

    def direct(path):
        if path not in cache:
            raw = path.read_text(encoding="utf-8", errors="replace")
            cache[path] = re.findall(r'^\s*#include\s+[<"]([^">]+)[">]', raw, re.M)
        return cache[path]

    def resolve(inc, frm):
        for base in (pub, frm.parent, proj, proj / "Private"):
            q = base / inc
            if q.exists():
                return q
        return None

    def closure(path):
        seen, incs, todo = set(), set(), [path]
        if path.suffix == ".cpp":
            for h in pub.rglob(path.stem + ".h"):
                todo.append(h)
        while todo:
            p = todo.pop()
            if p in seen:
                continue
            seen.add(p)
            for inc in direct(p):
                incs.add(inc)
                incs |= ENGINE_TRANSITIVE.get(inc, set())
                q = resolve(inc, p)
                if q is not None:
                    todo.append(q)
        return incs

    for f in sorted(list(proj.rglob("*.cpp")) + list(proj.rglob("*.h"))):
        t = strip(f.read_text(encoding="utf-8", errors="replace"))
        incs = None
        for pat, need, hard in INCLUDE_RULES:
            m = re.search(pat, t)
            if not m:
                continue
            if incs is None:
                incs = closure(f)
            if incs & (need | {"Engine.h", "EngineMinimal.h"}):
                continue
            line = t.count("\n", 0, m.start()) + 1
            msg = f"{f.relative_to(ROOT)}:{line}: '{m.group(0).strip()}' kullaniliyor ama {' / '.join(sorted(need))} include edilmemis"
            (errs if hard else warns).append(msg + ("" if hard else " (dolayli gelebilir)"))
    return errs, warns


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
            if first in top_dirs and inc.split("/")[-1].startswith("MMO") and not (pub / inc).exists() and not (SRC / "MyProject" / "Private" / inc).exists():
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
    ue_errs, ue_warns = check_ue_patterns()
    inc_errs, inc_warns = check_includes()
    errors += ue_errs + inc_errs
    for w in check_declarations() + ue_warns + inc_warns:
        print("UYARI:", w)
    print(f"Taranan dosya: {n}")
    for e in errors:
        print("HATA:", e)
    if errors:
        sys.exit(1)
    print("Statik kontrol temiz (derleme degildir).")


if __name__ == "__main__":
    main()
