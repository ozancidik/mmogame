"""StaticLint.py UE kalip denetimlerinin negatif testi (CI; Unreal gerektirmez).

Source/ ve StaticLint.py gecici bir klasore kopyalanir, her senaryoda TEK bir kasitli bozulma yapilir ve
StaticLint'in beklenen HATA/UYARI satirini basip basmadigi denetlenir. Ayrica bozulmamis kopya temiz olmali.
Calistir: python Scripts/TestStaticLint.py     Cikis 0 = tum senaryolar yakalandi.
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
P = "Source/MyProject/"
INV_H = P + "Public/Components/MMOInventoryComponent.h"
INV_CPP = P + "Private/Components/MMOInventoryComponent.cpp"
GM_H = P + "Public/Core/MMOGameMode.h"
SPAWNER_CPP = P + "Private/Core/MMOMobSpawner.cpp"

# (ad, dosya, desen, yerine, beklenen cikti parcasi, cikis kodu 1 mi)
CASES = [
    ("RPC Reliable eksik", INV_H, r"UFUNCTION\(Server, Reliable\)(\s*void ServerDepositEquipment)",
     r"UFUNCTION(Server)\1", "ServerDepositEquipment RPC'sinde Reliable/Unreliable yok", True),
    ("RPC _Implementation eksik", INV_CPP, r"::ServerDepositEquipment_Implementation\(",
     "::ServerDepositEquipmentX_Implementation(", "ServerDepositEquipment_Implementation hicbir cpp'de tanimli degil", True),
    ("WithValidation ama _Validate yok", INV_H, r"UFUNCTION\(Server, Reliable\)(\s*void ServerDepositEquipment)",
     r"UFUNCTION(Server, Reliable, WithValidation)\1", "WithValidation ama ServerDepositEquipment_Validate tanimi yok", True),
    ("_Validate var ama WithValidation yok", INV_CPP, r"\Z",
     "\nbool UMMOInventoryComponent::ServerDepositEquipment_Validate(int32 InstanceId) { return true; }\n",
     "ServerDepositEquipment_Validate tanimli ama UFUNCTION'da WithValidation yok", True),
    ("RPC dogrudan tanimli", INV_CPP, r"::ServerDepositEquipment_Implementation\(",
     "::ServerDepositEquipment(", "ServerDepositEquipment dogrudan tanimlanmis", True),
    ("RPC const olmayan referans", INV_H, r"void ServerDepositEquipment\(int32 InstanceId\)",
     "void ServerDepositEquipment(int32& InstanceId)", "RPC parametresi const olmayan referans", True),
    ("DOREPLIFETIME eksik", INV_CPP, r"\s*DOREPLIFETIME_CONDITION\(UMMOInventoryComponent, Gold, COND_OwnerOnly\);",
     "", "Gold Replicated ama GetLifetimeReplicatedProps icinde DOREPLIFETIME yok", True),
    ("DOREPLIFETIME replike olmayan ozellik", INV_CPP, r"DOREPLIFETIME_CONDITION\(UMMOInventoryComponent, Gold,",
     "DOREPLIFETIME_CONDITION(UMMOInventoryComponent, Gold, COND_OwnerOnly);\n\tDOREPLIFETIME(UMMOInventoryComponent, YokBoyle);\n\tDOREPLIFETIME_CONDITION(UMMOInventoryComponent, Gold,",
     "YokBoyle kayitli ama UPROPERTY(Replicated...) degil", True),
    ("ReplicatedUsing bildirilmemis OnRep", INV_H, r"ReplicatedUsing = OnRep_Gold", "ReplicatedUsing = OnRep_GoldTypo",
     "ReplicatedUsing=OnRep_GoldTypo ama OnRep_GoldTypo UFUNCTION() olarak bildirilmemis", True),
    ("PreLogin imzasi", GM_H, r"FString& ErrorMessage\) override", "FString ErrorMessage) override",
     "PreLogin override imzasi UE 5.7 ile uyusmuyor", True),
    ("ChangeName imzasi", GM_H, r"bool bNameChange\) override", "int32 bNameChange) override",
     "ChangeName override imzasi UE 5.7 ile uyusmuyor", True),
    ("GetLifetimeReplicatedProps const eksik", INV_H, r"OutLifetimeProps\) const override", "OutLifetimeProps) override",
     "GetLifetimeReplicatedProps override imzasi UE 5.7 ile uyusmuyor", True),
    ("Beyaz liste disi override", INV_H, r"(GENERATED_BODY\(\))", r"\1\n\tvirtual void OnRegister() override;",
     "OnRegister override beyaz listede yok", False),
    ("Net/UnrealNetwork.h eksik", INV_CPP, r'#include "Net/UnrealNetwork.h"\n', "",
     "Net/UnrealNetwork.h include edilmemis", True),
    ("TimerManager.h eksik", SPAWNER_CPP, r'#include "TimerManager.h"\n', "",
     "TimerManager.h include edilmemis", True),
]


def run_lint(tmp):
    r = subprocess.run([sys.executable, str(tmp / "Scripts" / "StaticLint.py")], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = Path(td) / "base"
        shutil.copytree(ROOT / "Source", base / "Source")
        (base / "Scripts").mkdir()
        shutil.copy(ROOT / "Scripts" / "StaticLint.py", base / "Scripts" / "StaticLint.py")
        code, out = run_lint(base)
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
            code, out = run_lint(tmp)
            if expect not in out:
                failures.append(f"[{name}] beklenen satir yok: '{expect}'\n{out}")
            elif hard and code != 1:
                failures.append(f"[{name}] HATA bekleniyordu ama cikis kodu {code}")
            else:
                print(f"yakalandi: {name}")
    for f in failures:
        print("KALDI:", f)
    if failures:
        return 1
    print(f"StaticLint negatif testi gecti ({len(CASES)} senaryo).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
