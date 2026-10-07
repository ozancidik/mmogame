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
QUEST_FIXTURE = os.path.join(HERE, "fixtures", "quest_metrics_sample.log")
FIRST_FIXTURE = os.path.join(HERE, "fixtures", "first_quest_metrics_sample.log")
LOOT_FIXTURE = os.path.join(HERE, "fixtures", "first_loot_metrics_sample.log")
KILL_FIXTURE = os.path.join(HERE, "fixtures", "first_kill_metrics_sample.log")
EQUIP_FIXTURE = os.path.join(HERE, "fixtures", "first_equip_metrics_sample.log")
SKILL_FIXTURE = os.path.join(HERE, "fixtures", "first_skill_metrics_sample.log")

failures = []
assertions = 0


def check(cond, msg):
    global assertions
    assertions += 1
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
    check("eski loglarda gercek sifir kazanc kaniti degildir" in out, "eski logda eksik gorev telemetrisi belirtilmeli")

    code, out = run_main(pm, ["x", FIXTURE, "--who", "Istemci"])
    check(code == 0 and "Ozan ===" not in out and "Istemci ===" in out, "--who filtresi calismiyor")

    quest_events = pm.parse_log(QUEST_FIXTURE)
    check(len(quest_events) == 14, "gorev fixture: 14 olay")
    qs = pm.build_stats(quest_events)
    host = qs["Host"]
    check(host["level"] == 3 and host["open_level"] == 3, "odul sonrasi levelup geriye sarilmamali")
    check(host["lv"][1]["quest_xp_reward"] == 200 and host["lv"][3]["quest_xp_reward"] == 400,
          "nominal gorev XP'si teslim oncesi seviyede sayilmali")
    check(host["lv"][1]["xp"] == 20, "kill ve gorev XP ayri sayilmali")
    check(host["lv"][1]["gold_in"] == 10 and host["lv"][3]["gold_in"] == 120
          and host["lv"][3]["gold_out"] == 5, "gorev altini cift sayilmamali / sonraki altin Lv3'te kalmali")
    check(host["dur"].get(1) == 45 and host["dur"].get(2) == 0 and host["dur"].get(3) == 50,
          "tek odulde coklu levelup ve acik seviye sureleri")
    goblins = host["quests"]["Q_Goblins"]
    check(goblins["accepts"] == 1 and goblins["claims"] == 1 and goblins["seconds"] == [40.0],
          "gorev kabul/teslim ve 40 sn sure")
    check(qs["Client"]["quests"]["Q_Goblins"]["seconds"] == [58.0], "oyuncu sureleri karismamali")
    saved = host["quests"]["Q_Saved"]
    check(saved["unmatched"] == 1 and saved["seconds"] == [], "logda kabul yoksa sure uydurulmamali")
    check(host["quests"]["Q_Unfinished"]["claims"] == 0 and "Q_Unfinished" in host["quest_start"],
          "bekleyen gorev teslim edilmis sayilmamali")
    check(qs["Cap"]["quests"]["Q_Cap"]["capped"] == 1, "seviye tavani nominal odul uyarisi")
    code, out = run_main(pm, ["x", QUEST_FIXTURE, "--who", "Host"])
    check(code == 0 and "quest_accept=4 quest_claim=4" in out, "gorev olay sayilari")
    check("XP(quest*)" in out and "Q_Goblins kabul=1 teslim=1 XP(odul)=200 ort_sure=0:40" in out,
          "gorev tablosu ve sure raporu")
    check("Q_Saved kabul=0 teslim=1 XP(odul)=400 ort_sure=? eslesmeyen=1" in out,
          "bilinmeyen sure raporu")
    check("Oyuncu: Client" not in out and "Oyuncu: Cap" not in out, "gorev raporu --who filtresi")
    reversed_pair = pm.build_stats([
        {"who": "Reversed", "ev": "quest_accept", "quest": "Q", "t": "20", "lv": "1"},
        {"who": "Reversed", "ev": "quest_claim", "quest": "Q", "t": "10", "lv": "1", "xp_reward": "1"},
    ])["Reversed"]["quests"]["Q"]
    check(reversed_pair["seconds"] == [] and reversed_pair["unmatched"] == 1,
          "ters saat farki sifir sure diye raporlanmamali")

    first_events = pm.parse_log(FIRST_FIXTURE)
    check(len(first_events) == 16, "first fixture: 16 olay")
    fs = pm.build_stats(first_events)
    first = fs["Host"]["first_actions"]
    check(first["a" * 32]["first_quest_accept"]["seconds"] == 8
          and first["a" * 32]["first_quest_claim"]["seconds"] == 38, "ilk gorev sureleri ve tekrar korumasi")
    check(first["b" * 32]["first_quest_accept"]["seconds"] == 5
          and "first_quest_claim" not in first["b" * 32], "yeni kontrolcu/eksik teslim karismamali")
    check(fs["Client"]["first_actions"]["c" * 32]["first_quest_accept"]["seconds"] == 9,
          "oyuncularin ilk olaylari ayrilmali")
    check(fs["Host"]["first_duplicates"] == 1 and fs["Bad"]["first_invalid"] == 9
          and fs["Bad"]["first_actions"] == {}, "gecersiz/tekrar ilk olaylar")
    check(fs["Host"]["quests"]["Q_Goblins"]["accepts"] == 1
          and fs["Host"]["quests"]["Q_Goblins"]["claims"] == 1
          and fs["Host"]["lv"][1]["quest_xp_reward"] == 200, "first olaylari gorev/XP'yi cift saymamali")
    check(o["first_actions"] == {} and host["first_actions"] == {}, "eski logdan ilk eylem uydurulmamali")
    code, out = run_main(pm, ["x", FIRST_FIXTURE, "--who", "Host"])
    check(code == 0 and "first_quest_accept sure=0:08" in out
          and "first_quest_claim sure=0:38" in out and "first_quest_claim sure=?" in out,
          "ilk gorev tablosu/missing teslim raporu")
    check("Oyuncu: Client" not in out and "Oyuncu: Bad" not in out, "first --who filtresi")
    code, bad_out = run_main(pm, ["x", FIRST_FIXTURE, "--who", "Bad"])
    check(code == 0 and "gecersiz=9 tekrar=0" in bad_out and "first_* suresi bilinmiyor" in bad_out,
          "bozuk sayilar/eksik alan raporu hata vermeden reddedilmeli")
    code, old_out = run_main(pm, ["x", FIXTURE])
    check("first_* suresi bilinmiyor" in old_out, "eski log ilk eylem olcumu yok uyarisini vermeli")

    loot_events = pm.parse_log(LOOT_FIXTURE)
    check(len(loot_events) == 13, "first loot fixture: 13 olay")
    ls = pm.build_stats(loot_events)
    check(ls["Host"]["first_actions"]["a" * 32]["first_loot"] == {
        "seconds": 6, "lv": 1, "kind": "gold", "item": "Gold", "amount": 2}, "ilk gercek altin ve tekrar korumasi")
    check(ls["Host"]["first_actions"]["b" * 32]["first_loot"]["amount"] == 1
          and ls["Client"]["first_actions"]["c" * 32]["first_loot"]["kind"] == "equipment", "sarf/ekipman ve oturum ayrimi")
    check(ls["Host"]["first_duplicates"] == 1 and ls["Bad"]["first_invalid"] == 8
          and ls["Bad"]["first_actions"] == {}, "gecersiz loot payload reddi")
    check(ls["Host"]["src"]["loot"] == [2, 0] and ls["Host"]["lv"][0]["gold_in"] == 2
          and ls["Host"]["quests"] == {} and sum(x["xp"] for x in ls["Host"]["lv"].values()) == 0,
          "ilk loot olayi altin/XP/gorev sayisini artirmamali")
    code, out = run_main(pm, ["x", LOOT_FIXTURE, "--who", "Host"])
    check(code == 0 and "first_loot sure=0:06 lv=1 kind=gold item=Gold amount=2" in out
          and "first_loot sure=0:01 lv=1 kind=item item=HealthPotion amount=1" in out
          and "Oyuncu: Client" not in out, "loot sure/payload raporu ve --who")
    code, out = run_main(pm, ["x", FIRST_FIXTURE, "--who", "Host"])
    check("first_loot sure=?" in out, "eski gorev logundan loot suresi uydurulmamali")

    kill_events = pm.parse_log(KILL_FIXTURE)
    check(len(kill_events) == 17, "first kill fixture: 17 olay")
    ks = pm.build_stats(kill_events)
    check(ks["Host"]["first_actions"]["a" * 32]["first_kill"] == {
        "seconds": 8, "lv": 1, "mob": "Goblin", "mlv": 1, "xp": 25, "credit": "fatal", "elite": 0}, "ilk son vurus ve tekrar korumasi")
    check(ks["Host"]["first_actions"]["b" * 32]["first_kill"]["credit"] == "party"
          and ks["Client"]["first_actions"]["c" * 32]["first_kill"]["elite"] == 1
          and ks["Fallback"]["first_actions"]["d" * 32]["first_kill"]["xp"] == 0, "parti/katki/elit/fallback ve sifir XP kredisi")
    check(ks["Host"]["first_duplicates"] == 1 and ks["Bad"]["first_invalid"] == 10
          and ks["Bad"]["first_actions"] == {}, "gecersiz kill payload reddi")
    check(ks["Host"]["lv"][1]["kills"] == 1 and ks["Host"]["lv"][1]["xp"] == 25
          and ks["Client"]["lv"][1]["kills"] == 1 and ks["Client"]["lv"][1]["xp"] == 12
          and ks["Host"]["quests"] == {} and ks["Host"]["src"] == {}, "first kill sayi/XP/altin/gorev cift saymamali")
    code, out = run_main(pm, ["x", KILL_FIXTURE, "--who", "Host"])
    check(code == 0 and "first_kill sure=0:08 lv=1 mob=Goblin mlv=1 xp=25 credit=fatal elite=0" in out
          and "first_kill sure=0:05 lv=2 mob=Wolf mlv=2 xp=0 credit=party elite=0" in out
          and "Oyuncu: Client" not in out, "kill raporu ve --who filtresi")
    code, out = run_main(pm, ["x", FIRST_FIXTURE, "--who", "Host"])
    check("first_kill sure=?" in out, "eski logdan ilk kill suresi uydurulmamali")

    equip_events = pm.parse_log(EQUIP_FIXTURE)
    check(len(equip_events) == 17, "first equip fixture: 17 olay")
    es = pm.build_stats(equip_events)
    check(es["Host"]["first_actions"]["a" * 32]["first_equip"] == {
        "seconds": 8, "lv": 1, "item": "ShortSword", "instance": 4, "slot": 0}, "ilk kusanma ve tekrar korumasi")
    check(es["Host"]["first_actions"]["b" * 32]["first_equip"]["slot"] == 1
          and es["Client"]["first_actions"]["c" * 32]["first_equip"]["slot"] == 2
          and es["Peer"]["first_actions"]["d" * 32]["first_equip"]["slot"] == 3, "dort slot ve oturum/oyuncu ayrimi")
    check(es["Host"]["first_duplicates"] == 1 and es["Bad"]["first_invalid"] == 11
          and es["Bad"]["first_actions"] == {}, "gecersiz item/instance/slot payload reddi")
    check(es["Host"]["src"]["loot"] == [2, 0] and es["Host"]["lv"][0]["gold_in"] == 2
          and es["Host"]["quests"] == {} and es["Host"]["mobs"] == {}
          and sum(x["xp"] for x in es["Host"]["lv"].values()) == 0, "first equip altin/XP/gorev/kill saymamali")
    code, out = run_main(pm, ["x", EQUIP_FIXTURE, "--who", "Host"])
    check(code == 0 and "first_equip sure=0:08 lv=1 item=ShortSword instance=4 slot=0" in out
          and "first_equip sure=0:05 lv=1 item=LeatherArmor instance=1 slot=1" in out
          and "Oyuncu: Client" not in out, "equip raporu ve --who filtresi")
    code, out = run_main(pm, ["x", FIRST_FIXTURE, "--who", "Host"])
    check("first_equip sure=?" in out, "eski logdan ilk kusanma suresi uydurulmamali")

    skill_events = pm.parse_log(SKILL_FIXTURE)
    check(len(skill_events) == 26, "first skill fixture: 26 olay")
    check("first_skill" in pm.FIRST_ACTIONS, "first_skill ilk eylemler listesinde olmali")
    ss = pm.build_stats(skill_events)
    check(sorted(ss) == ["Archer", "Bad", "Mage", "Priest", "Warrior"], "skill fixture dort sinif ve bozuk payload oyuncusu")
    warrior = ss["Warrior"]
    skill = warrior["first_actions"]["a" * 32]["first_skill"]
    check(skill == {"seconds": 8, "lv": 1, "skill": "War_PowerStrike", "effect": 0, "mana": 8.0},
          "ilk kabul edilen cast/pre-effect seviye, tekrar ilk payload'u degistirmemeli")
    check(warrior["first_duplicates"] == 1 and warrior["first_invalid"] == 0,
          "buyuk/kucuk harf sid ayni oyuncu+oturum+action icin tek ilk cast")
    check(warrior["first_actions"]["b" * 32]["first_skill"] == {
        "seconds": 5, "lv": 4, "skill": "War_BloodDrain", "effect": 4, "mana": 0.0},
          "yeni kontrolcude sifir mana/full-HP self heal, fayda alani gerektirmeden kabul edilmeli")
    check(warrior["first_actions"]["c" * 32]["first_skill"]["effect"] == 5,
          "buff cast ilk skill sayilmali")
    check(warrior["first_actions"]["a" * 32]["first_equip"]["item"] == "ShortSword",
          "ayni oyuncu/sid farkli action birbirini engellememeli")
    check(ss["Archer"]["first_actions"]["a" * 32]["first_skill"]["effect"] == 3
          and ss["Archer"]["first_actions"]["b" * 32]["first_skill"]["effect"] == 1,
          "ayni sid farkli oyuncu ve yeni oturum birbirinden bagimsiz olmali")
    check(ss["Mage"]["first_actions"]["a" * 32]["first_skill"] == {
        "seconds": 15, "lv": 3, "skill": "Mag_FrostNova", "effect": 2, "mana": 0.125},
          "kesirli mana/empty AoESelf, hasar faydasi gerektirmeden kabul edilmeli")
    check(ss["Priest"]["first_actions"]["a" * 32]["first_skill"]["effect"] == 6,
          "HealTarget effect ust siniri 6 kabul edilmeli")
    samples = [actions["first_skill"] for p in ss.values() for actions in p["first_actions"].values()
               if "first_skill" in actions]
    check(len(samples) == 7 and {s["effect"] for s in samples} == set(range(7)),
          "7 ilk cast ve tum 0..6 effect degerleri")
    check(ss["Bad"]["first_invalid"] == 13 and ss["Bad"]["first_duplicates"] == 0
          and ss["Bad"]["first_actions"] == {}, "13 bozuk skill/mana/effect payload reddedilmeli")
    for index, bad_event in enumerate(e for e in skill_events if e["who"] == "Bad"):
        rejected = pm.build_stats([bad_event])["Bad"]
        check(rejected["first_invalid"] == 1 and rejected["first_actions"] == {},
              "skill bozuk payload #%d ayri ayri reddedilmeli: %s" % (index + 1, bad_event))

    base_events = [e for e in skill_events if e["ev"] not in pm.FIRST_ACTIONS]
    # Invalid first markers must not move a player's observation clock, even when
    # they appear before the first legitimate event or after the last one.
    clock_base = [{"who": "Clock", "ev": "kill", "t": "10", "lv": "1", "xp": "25", "mob": "Goblin"}]
    clock_bad = dict(next(e for e in skill_events if e["ev"] == "first_skill"), who="Clock", mana="NaN", t="1000")
    clock_reference = pm.build_stats(clock_base)["Clock"]
    for sequence in ([clock_bad] + clock_base, clock_base + [clock_bad]):
        clock_result = pm.build_stats(sequence)["Clock"]
        check(clock_result["first_invalid"] == 1, "invalid marker retained in diagnostics")
        check(clock_result["first_t"] == clock_reference["first_t"]
              and clock_result["last_t"] == clock_reference["last_t"]
              and clock_result["dur"] == clock_reference["dur"], "invalid marker must not extend level duration")
    for action in pm.FIRST_ACTIONS:
        invalid = dict(clock_bad, ev=action, scope="invalid")
        clock_result = pm.build_stats([invalid] + clock_base + [invalid])["Clock"]
        check(clock_result["first_invalid"] == 2 and clock_result["first_t"] == 10 and clock_result["last_t"] == 10
              and clock_result["dur"] == clock_reference["dur"], "invalid first marker clock isolation: " + action)
    baseline = pm.build_stats(base_events)["Warrior"]
    for key in ("level", "lv", "src", "mobs", "quests", "quest_start", "pending"):
        check(warrior[key] == baseline[key], "first skill ekonomi/ilerleme toplamini degistirmemeli: " + key)
    check(warrior["lv"][1]["kills"] == 1 and warrior["lv"][1]["xp"] == 25
          and warrior["lv"][1]["gold_in"] == 3 and warrior["lv"][1]["gold_out"] == 0
          and warrior["lv"][1]["quest_xp_reward"] == 200
          and warrior["quests"]["Q_Goblins"]["accepts"] == 1
          and warrior["quests"]["Q_Goblins"]["claims"] == 1, "skill log toplamlarinda XP/altin/kill/gorev artmamali")
    for who in ("Archer", "Mage", "Priest", "Bad"):
        check(ss[who]["lv"] == {} and ss[who]["quests"] == {} and ss[who]["src"] == {}
              and ss[who]["mobs"] == {} and ss[who]["level"] is None,
              "yalniz skill marker ekonomi/seviye/kill/gorev olusturmamali: " + who)

    valid_cast = next(e for e in skill_events if e["ev"] == "first_skill")
    malformed_common = [
        ("scope", "character"), ("scope", None), ("sid", "a" * 31), ("sid", "g" * 32), ("sid", None),
        ("t", "NaN"), ("t", "Inf"), ("t", "-1"), ("t", None),
        ("start", "NaN"), ("start", "Inf"), ("start", "-1"), ("start", None), ("start", "9"),
        ("elapsed", "NaN"), ("elapsed", "Inf"), ("elapsed", "-1"), ("elapsed", None), ("elapsed", "7"),
        ("lv", "0"), ("lv", "-1"), ("lv", "1.5"), ("lv", "NaN"), ("lv", "Inf"), ("lv", None),
    ]
    for field, value in malformed_common:
        bad_event = dict(valid_cast)
        if value is None:
            bad_event.pop(field)
        else:
            bad_event[field] = value
        rejected = pm.build_stats([bad_event])["Warrior"]
        check(rejected["first_invalid"] == 1 and rejected["first_actions"] == {},
              "skill ortak sid/scope/saat/seviye reddi: %s=%s" % (field, value))
    accepted_after_bad = pm.build_stats([bad_event, valid_cast])["Warrior"]
    check(accepted_after_bad["first_invalid"] == 1 and accepted_after_bad["first_duplicates"] == 0
          and accepted_after_bad["first_actions"]["a" * 32]["first_skill"] == skill,
          "gecersiz marker sonra gelen ilk gecerli cast'i engellememeli")
    unknown_id = dict(valid_cast, skill="Future_RuntimeSkill")
    check(pm.build_stats([unknown_id])["Warrior"]["first_actions"]["a" * 32]["first_skill"]["skill"]
          == "Future_RuntimeSkill", "parser runtime DB skill listesi varsaymamali")

    code, out = run_main(pm, ["x", SKILL_FIXTURE, "--who", "Warrior"])
    check(code == 0 and "Olay sayisi: 26 (levelup=0 kill=1 death=0 gold=1)" in out
          and "quest_accept=1 quest_claim=1" in out, "skill log CLI olay sayilari artmamali")
    check("first_skill sure=0:08 lv=1 skill=War_PowerStrike effect=0 mana=8.000" in out
          and "first_skill sure=0:05 lv=4 skill=War_BloodDrain effect=4 mana=0.000" in out
          and "gecersiz=0 tekrar=1" in out, "skill detay/sure/pre-effect seviye/3dp mana raporu")
    check(all("Oyuncu: " + who not in out for who in ("Archer", "Mage", "Priest", "Bad")), "skill --who filtresi")
    code, out = run_main(pm, ["x", SKILL_FIXTURE, "--who", "Mage"])
    check(code == 0 and "first_skill sure=0:15 lv=3 skill=Mag_FrostNova effect=2 mana=0.125" in out,
          "kesirli mana raporda korunmali")
    code, out = run_main(pm, ["x", SKILL_FIXTURE, "--who", "Bad"])
    check(code == 0 and "gecersiz=13 tekrar=0" in out and "first_skill sure=?" in out,
          "bozuk skill logu sure uydurmadan raporlanmali")
    code, out = run_main(pm, ["x", FIRST_FIXTURE, "--who", "Host"])
    check(code == 0 and out.count("first_skill sure=?") == 2,
          "eski first-action logunun iki sid'sinde eksik skill suresi bilinmez")
    code, out = run_main(pm, ["x", FIXTURE, "--who", "Istemci"])
    check(code == 0 and "first_skill sure=?" in out and "first_* suresi bilinmiyor" in out
          and "Oyuncu: Ozan" not in out, "eski marker olmayan logda --who ve eksik first_skill suresi")

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
    print("PlaytestMetrics fixture testi gecti (eski: %d olay; gorev: %d olay; first: %d olay; loot: %d olay; kill: %d olay; equip: %d olay; skill: %d olay; asserts: %d/%d)." % (len(events), len(quest_events), len(first_events), len(loot_events), len(kill_events), len(equip_events), len(skill_events), assertions - len(failures), assertions))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
