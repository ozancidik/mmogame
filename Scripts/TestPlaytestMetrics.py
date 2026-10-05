"""PlaytestMetrics.py icin sahte log (fixture) testi. Unreal gerektirmez; CI'da calisir.

Calistir:
  python Scripts/TestPlaytestMetrics.py [--script <PlaytestMetrics.py yolu>]
--script yalnizca negatif test icindir (bozulmus gecici kopyayi denemek). Cikis 0 = gecti, 1 = kaldi.
Fixture: Scripts/fixtures/playtest_metrics_sample.log (iki oyuncu, olcum disi satir, 'who' alani eksik bozuk satir).
"""
import contextlib
import importlib.util
import io
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(HERE, "fixtures", "playtest_metrics_sample.log")

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def load(path):
    spec = importlib.util.spec_from_file_location("playtest_metrics_under_test", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run_main(mod, argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = mod.main(argv)
    return code, buf.getvalue()


def main(argv):
    script = os.path.join(HERE, "PlaytestMetrics.py")
    if "--script" in argv:
        script = argv[argv.index("--script") + 1]
    pm = load(script)

    events = pm.parse_log(FIXTURE)
    # 13 [MMOMETRIC] satiri; 'who' alani olmayan bozuk satir atlanir -> 12
    check(len(events) == 12, "parse_log: 12 olay bekleniyordu, %d" % len(events))

    players = pm.build_stats(events)
    check(sorted(players) == ["Istemci", "Ozan"], "oyuncular: %s" % sorted(players))
    o = players.get("Ozan")
    if o:
        lv = o["lv"]
        check(lv[1]["kills"] == 2 and lv[1]["xp"] == 60, "Lv1: 2 oldurme / 60 XP: %s" % dict(lv[1]))
        # seviye bilinmeden gelen ilk altin (+10) Lv1'e yazilmali
        check(lv[1]["gold_in"] == 22, "Lv1 altin girisi 22 olmali (bekleyen +10 dahil): %s" % lv[1]["gold_in"])
        check(lv[2]["gold_in"] == 120 and lv[2]["gold_out"] == 40, "Lv2 altin +120/-40: %s" % dict(lv[2]))
        check(lv[2]["deaths"] == 1 and lv[2]["xploss"] == 5, "Lv2 1 olum / 5 XP kaybi: %s" % dict(lv[2]))
        check(lv[3]["kills"] == 1, "Lv3 1 oldurme")
        check(o["dur"].get(1) == 55.0, "Lv1 suresi 55 sn (ilk olay t=5 -> levelup t=60): %s" % o["dur"].get(1))
        check(o["dur"].get(2) == 90.0, "Lv2 suresi 90 sn: %s" % o["dur"].get(2))
        check(o.get("open_level") == 3 and o["dur"].get(3) == 20.0, "Lv3 acik, 20 sn: %s" % o["dur"].get(3))
        check(o["src"]["satin_alma"] == [0, 40], "satin_alma cikisi 40: %s" % o["src"].get("satin_alma"))
        check(o["mobs"]["Goblin"] == 2, "Goblin x2")
    i = players.get("Istemci")
    if i:
        check(i["lv"][1]["kills"] == 1 and i["lv"][1]["xp"] == 20, "Istemci Lv1 1 oldurme / 20 XP")

    code, out = run_main(pm, ["x", FIXTURE])
    check(code == 0, "main cikis kodu 0 olmali: %s" % code)
    check("levelup=2 kill=5 death=1 gold=4" in out, "olay sayisi satiri yanlis:\n" + out)
    check("=== Oyuncu: Ozan ===" in out and "=== Oyuncu: Istemci ===" in out, "oyuncu basliklari eksik")

    code, out = run_main(pm, ["x", FIXTURE, "--who", "Istemci"])
    check(code == 0 and "Ozan ===" not in out and "Istemci ===" in out, "--who filtresi calismiyor")

    code, out = run_main(pm, ["x", os.path.join(HERE, "fixtures", "yok_boyle_bir_log.log")])
    check(code == 0 and "bulunamadi" in out, "eksik log: 0 ile cikip aciklama basmali")

    with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False, encoding="utf-8") as fh:
        fh.write("LogInit: olcum satiri yok\n")
        empty = fh.name
    try:
        code, out = run_main(pm, ["x", empty])
        check(code == 0 and "[MMOMETRIC] satiri yok" in out, "olcumsuz log: 0 ile cikip aciklama basmali")
    finally:
        os.unlink(empty)

    if failures:
        for f in failures:
            print("KALDI:", f)
        return 1
    print("PlaytestMetrics fixture testi gecti (%d olay, 2 oyuncu)." % len(events))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
