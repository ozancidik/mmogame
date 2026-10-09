"""PIE olcum gunlugu ozeti (docs/VERTICAL_SLICE_LV1_30.md madde 1).

Sunucu logundaki "[MMOMETRIC] t=.. ev=.. anahtar=deger" satirlarini okuyup oyuncu basina
seviye basina sure / oldurme / olum / altin tablosu basar.

Kullanim:
  python Scripts/PlaytestMetrics.py [log_yolu] [--who OyuncuAdi]
log_yolu verilmezse <proje>/Saved/Logs/MyProject.log kullanilir. Log yoksa ya da icinde
[MMOMETRIC] satiri yoksa aciklayici mesaj basar ve 0 ile cikar (hata vermez).

Olaylar (Source/MyProject, UE_LOG LogTemp Display):
  levelup  who from to
  kill     who lv mob mlv xp party elite     (lv = XP verilmeden onceki oyuncu seviyesi)
  death    who lv x y z by xploss
  gold     who src amount balance            (amount: +kazanc / -harcama)
  quest_accept who lv quest
  quest_claim  who lv to quest xp_reward capped (lv = odulden once; xp_reward = nominal odul)
  first_quest_accept / first_quest_claim who lv quest scope=controller sid start elapsed
  first_loot who lv kind=gold|item|equipment item amount scope=controller sid start elapsed
  first_kill who lv mob mlv xp credit=fatal|contribution|party|fallback elite scope=controller sid start elapsed
  first_equip who lv item instance slot=0..3 scope=controller sid start elapsed (successful player equip request)
  first_skill who lv skill effect=0..6 mana scope=controller sid start elapsed
    (first authoritative accepted cast after ConsumeMana, before ExecuteSkill; lv is pre-effect)
    (full-HP self heal / empty AoESelf still count; no damage/heal benefit required)
  first_basic_attack who lv target=mob|player targetid tlv scope=controller sid start elapsed
    (targetid is a nonempty mob identifier or the literal Player; tlv is an integer >=1)
  first_death who lv by xploss scope=controller sid start elapsed
    (after real server death and actual XP penalty; by is a mob archetype or lowercase none)
    (xploss is the actual integer loss 0..2147483647; marker adds no gameplay counters)
  first_levelup who lv from to scope=controller sid start elapsed
    (first authoritative GainExperience -> HandleLevelUp transition before refill/level delegates)
    (integer from=1..79, to=from+1=2..80, lv=to; first adjacent step of a multi-level award)
    (excludes save load, direct Level edits, dead/capped/no threshold/NPC; no gameplay counters)
  src: loot, gorev, satis, satin_alma, yukseltme, iyilestirme, diger (ticaret dahil)
t = surec baslangicindan beri saniye.
"""
import os
import math
import re
import sys
from collections import defaultdict

LINE_RE = re.compile(r"\[MMOMETRIC\]\s+(.*)$")
FIRST_ACTIONS = ("first_quest_accept", "first_quest_claim", "first_loot", "first_kill", "first_equip", "first_skill", "first_basic_attack", "first_death", "first_levelup")


def project_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def to_num(text, default=0.0):
    try:
        value = float(text)
        return value if math.isfinite(value) else default
    except (TypeError, ValueError):
        return default


def bounded_int_text(text, minimum, maximum):
    """Validate ASCII integer text exactly, converting only bounded significant digits."""
    if not isinstance(text, str) or not re.fullmatch(r"[+-]?[0-9]+", text):
        return None
    # Strip leading zeroes before conversion: huge tokens cannot hit Python's
    # integer-string limit, and decimal/exponent text never passes via rounding.
    digits = text.lstrip("+-").lstrip("0") or "0"
    if len(digits) > len(str(maximum)):
        return None
    value = int(digits) * (-1 if text.startswith("-") else 1)
    return value if minimum <= value <= maximum else None


def parse_log(path):
    events = []
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            m = LINE_RE.search(raw)
            if not m:
                continue
            fields = {}
            for tok in m.group(1).split():
                if "=" in tok:
                    k, _, v = tok.partition("=")
                    fields[k] = v
            if "ev" in fields and "who" in fields:
                events.append(fields)
    return events


def add_gold(p, lv, amount):
    if amount >= 0:
        p["lv"][lv]["gold_in"] += amount
    else:
        p["lv"][lv]["gold_out"] += -amount


def flush_pending(p):
    """Seviye ilk kez bilinince, oncesinde gelen altin olaylarini o seviyeye yaz."""
    if p["level"] is not None and p["pending"]:
        base = p.get("start_level", p["level"])
        for amount in p["pending"]:
            add_gold(p, base, amount)
        p["pending"] = []


def build_stats(events):
    """Oyuncu -> seviye -> toplamlar. Seviye girisi levelup ile belirlenir; ilk seviye ilk olaydan baslar."""
    def record_time(player, timestamp):
        if not player["timing_started"]:
            player["first_t"] = player["last_t"] = timestamp
            player["timing_started"] = True
        else:
            player["last_t"] = max(player["last_t"], timestamp)

    players = {}
    for e in events:
        who = e["who"]
        t = to_num(e.get("t"))
        p = players.setdefault(who, {
            "level": None, "enter": {}, "dur": {}, "lv": defaultdict(lambda: defaultdict(float)),
            "first_t": 0, "last_t": 0, "timing_started": False,
            "src": defaultdict(lambda: [0, 0]), "mobs": defaultdict(int),
            "pending": [],  # seviye henuz bilinmezken gelen altin olaylari (ilk bilinen seviyeye yazilir)
            "quests": defaultdict(lambda: {"accepts": 0, "claims": 0, "xp_reward": 0,
                                            "seconds": [], "unmatched": 0, "capped": 0}),
            "quest_start": {},
            "first_actions": {}, "first_invalid": 0, "first_duplicates": 0,
        })
        ev = e["ev"]
        if ev not in FIRST_ACTIONS:
            record_time(p, t)
        if ev in FIRST_ACTIONS:
            # Explicit controller lifetime, never guessed from first log time or player name.
            sid = e.get("sid", "")
            start = to_num(e.get("start"), float("nan"))
            elapsed = to_num(e.get("elapsed"), float("nan"))
            event_time = to_num(e.get("t"), float("nan"))
            amount = to_num(e.get("amount"), float("nan"))
            if ev == "first_loot":
                payload_valid = (e.get("kind") in ("gold", "item", "equipment") and bool(e.get("item"))
                                 and math.isfinite(amount) and amount > 0 and amount.is_integer()
                                 and (e.get("kind") != "equipment" or amount == 1)
                                 and (e.get("kind") != "gold" or e.get("item") == "Gold"))
            elif ev == "first_kill":
                mob_level = to_num(e.get("mlv"), float("nan"))
                xp = to_num(e.get("xp"), float("nan"))
                elite = to_num(e.get("elite"), float("nan"))
                payload_valid = (bool(e.get("mob")) and e["mob"] != "None"
                                 and math.isfinite(mob_level) and mob_level >= 1 and mob_level.is_integer()
                                 and math.isfinite(xp) and xp >= 0 and xp.is_integer() and elite in (0, 1)
                                 and e.get("credit") in ("fatal", "contribution", "party", "fallback"))
            elif ev == "first_equip":
                instance = to_num(e.get("instance"), float("nan"))
                slot = to_num(e.get("slot"), float("nan"))
                payload_valid = (bool(e.get("item")) and e["item"] != "None"
                                 and math.isfinite(instance) and 0 < instance <= 2147483647 and instance.is_integer()
                                 and math.isfinite(slot) and 0 <= slot <= 3 and slot.is_integer())
            elif ev == "first_skill":
                effect = to_num(e.get("effect"), float("nan"))
                mana = to_num(e.get("mana"), float("nan"))
                # Runtime DB validity belongs to the emitter; this parser validates the payload.
                payload_valid = (bool(e.get("skill")) and e["skill"] != "None"
                                 and math.isfinite(effect) and 0 <= effect <= 6 and effect.is_integer()
                                 and math.isfinite(mana) and mana >= 0)
            elif ev == "first_basic_attack":
                target_level = to_num(e.get("tlv"), float("nan"))
                # Mob identifiers belong to the runtime DB; player identity is never emitted.
                payload_valid = (e.get("target") in ("mob", "player")
                                 and bool(e.get("targetid")) and e["targetid"] != "None"
                                 and (e["target"] != "player" or e["targetid"] == "Player")
                                 and math.isfinite(target_level) and target_level >= 1
                                 and target_level.is_integer())
            elif ev == "first_death":
                xp_loss = to_num(e.get("xploss"), float("nan"))
                # Runtime identifies the killer and applies the penalty before emitting.
                # Require integer text too: float rounding must not admit fractional XP.
                payload_valid = (bool(e.get("by")) and e["by"] != "None"
                                 and bool(re.fullmatch(r"[+-]?[0-9]+", e.get("xploss", "")))
                                 and math.isfinite(xp_loss) and 0 <= xp_loss <= 2147483647)
            elif ev == "first_levelup":
                from_level = bounded_int_text(e.get("from"), 1, 79)
                to_level = bounded_int_text(e.get("to"), 2, 80)
                post_level = bounded_int_text(e.get("lv"), 2, 80)
                payload_valid = (from_level is not None and to_level is not None
                                 and post_level is not None and to_level == from_level + 1
                                 and post_level == to_level)
                level = float(post_level) if post_level is not None else float("nan")
            else:
                payload_valid = bool(e.get("quest"))
            if ev != "first_levelup":
                level = to_num(e.get("lv"), float("nan"))
            if (e.get("scope") != "controller" or not re.fullmatch(r"[0-9A-Fa-f]{32}", sid)
                    or not all(math.isfinite(n) and n >= 0 for n in (start, elapsed, event_time))
                    or not math.isfinite(level) or level < 1 or not level.is_integer() or not payload_valid
                    or event_time < start or abs(event_time - start - elapsed) > 0.01):
                p["first_invalid"] += 1
                continue
            session = p["first_actions"].setdefault(sid.lower(), {})
            if ev in session:
                p["first_duplicates"] += 1
                continue
            # A skipped first marker is not a new observation for timing either.
            record_time(p, event_time)
            sample = {"seconds": elapsed, "lv": int(level)}
            if ev == "first_loot":
                sample.update(kind=e["kind"], item=e["item"], amount=int(amount))
            elif ev == "first_kill":
                sample.update(mob=e["mob"], mlv=int(mob_level), xp=int(xp), credit=e["credit"], elite=int(elite))
            elif ev == "first_equip":
                sample.update(item=e["item"], instance=int(instance), slot=int(slot))
            elif ev == "first_skill":
                sample.update(skill=e["skill"], effect=int(effect), mana=mana)
            elif ev == "first_basic_attack":
                sample.update(target=e["target"], targetid=e["targetid"], tlv=int(target_level))
            elif ev == "first_death":
                sample.update(by=e["by"], xploss=int(xp_loss))
            elif ev == "first_levelup":
                sample.update({"from": from_level, "to": to_level})
            else:
                sample["quest"] = e["quest"]
            session[ev] = sample
        elif ev == "levelup":
            frm = int(to_num(e.get("from")))
            to = int(to_num(e.get("to")))
            enter = p["enter"].get(frm, p["first_t"] if p["level"] is None else t)
            p["enter"].setdefault(frm, enter)
            p["dur"][frm] = p["dur"].get(frm, 0.0) + max(0.0, t - enter)
            p["enter"][to] = t
            if p["level"] is None:
                p["start_level"] = frm
                p["level"] = frm
                flush_pending(p)
            p["level"] = to
            p["lv"][to]  # seviye satiri olussun
            p["lv"][frm]
        elif ev in ("kill", "death", "quest_accept", "quest_claim"):
            lv = int(to_num(e.get("lv")))
            if p["level"] is None:
                p["level"] = lv
                p["enter"][lv] = p["first_t"]
                flush_pending(p)
            if ev == "kill":
                p["lv"][lv]["kills"] += 1
                p["lv"][lv]["xp"] += to_num(e.get("xp"))
                p["mobs"][e.get("mob", "?")] += 1
            elif ev == "death":
                p["lv"][lv]["deaths"] += 1
                p["lv"][lv]["xploss"] += to_num(e.get("xploss"))
            else:
                quest = e.get("quest", "?")
                q = p["quests"][quest]
                if ev == "quest_accept":
                    q["accepts"] += 1
                    p["quest_start"].setdefault(quest, t)
                else:
                    # levelup satirlari ClaimReward icinde once gelir: lv eski seviyedir.
                    # Mevcut seviyeyi geriye sarma, altini tekrar sayma.
                    xp = max(0, int(to_num(e.get("xp_reward"))))
                    p["lv"][lv]["quest_xp_reward"] += xp
                    q["claims"] += 1
                    q["xp_reward"] += xp
                    q["capped"] += int(to_num(e.get("capped"))) != 0
                    start = p["quest_start"].pop(quest, None)
                    if start is not None and t >= start:
                        q["seconds"].append(t - start)
                    else:
                        q["unmatched"] += 1
        elif ev == "gold":
            amount = int(to_num(e.get("amount")))
            src = e.get("src", "diger")
            if amount >= 0:
                p["src"][src][0] += amount
            else:
                p["src"][src][1] += -amount
            if p["level"] is None:
                p["pending"].append(amount)
            else:
                add_gold(p, p["level"], amount)
    # Devam eden (son) seviyenin suresi
    for p in players.values():
        for amount in p["pending"]:  # seviye hic ogrenilemediyse "?" satirina
            add_gold(p, 0, amount)
        if p["level"] is not None and p["level"] in p["enter"] and p["level"] not in p["dur"]:
            p["dur"][p["level"]] = max(0.0, p["last_t"] - p["enter"][p["level"]])
            p["open_level"] = p["level"]
    return players


def fmt_time(sec):
    sec = int(round(sec))
    return "%d:%02d" % (sec // 60, sec % 60)


def print_player(who, p):
    print("")
    print("=== Oyuncu: %s ===" % who)
    levels = sorted(set(p["lv"].keys()) | set(p["dur"].keys()))
    header = "%-7s %-9s %-9s %-9s %-10s %-7s %-10s %-10s %-10s" % (
        "Seviye", "Sure", "Oldurme", "XP(kill)", "XP(quest*)", "Olum", "Altin+", "Altin-", "Net")
    print(header)
    print("-" * len(header))
    tot_kills = tot_deaths = tot_in = tot_out = tot_quest_xp = 0
    for lv in levels:
        d = p["lv"][lv]
        dur = p["dur"].get(lv)
        dur_txt = fmt_time(dur) if dur is not None else "-"
        if lv == p.get("open_level"):
            dur_txt += "*"
        gi, go = int(d["gold_in"]), int(d["gold_out"])
        print("%-7s %-9s %-9d %-9d %-10d %-7d %-10d %-10d %-10d" % (
            lv if lv else "?", dur_txt, d["kills"], d["xp"], d["quest_xp_reward"], d["deaths"], gi, go, gi - go))
        tot_kills += d["kills"]
        tot_deaths += d["deaths"]
        tot_in += gi
        tot_out += go
        tot_quest_xp += d["quest_xp_reward"]
    print("-" * len(header))
    print("%-7s %-9s %-9d %-9s %-10d %-7d %-10d %-10d %-10d" % (
        "Toplam", fmt_time(p["last_t"] - p["first_t"]), tot_kills, "", tot_quest_xp, tot_deaths, tot_in, tot_out, tot_in - tot_out))
    if any(str(l) for l in levels) and p.get("open_level") is not None:
        print("(*) devam eden seviye: sure son olay zamanina kadar")
    if tot_kills:
        rate = (100.0 * tot_deaths / tot_kills)
        print("Olum orani: %.1f olum / 100 oldurme (hedef: 1 olum / 30-80 oldurme = 1.3-3.3)" % rate)
        print("Giris/cikis orani (altin): %s" % ("%.2f" % (tot_in / tot_out) if tot_out else "cikis yok"))
    if p["src"]:
        print("Altin akisi (kaynaga gore):")
        for src, (gin, gout) in sorted(p["src"].items()):
            print("  %-12s giris=%-8d cikis=%-8d" % (src, gin, gout))
    if p["mobs"]:
        top = sorted(p["mobs"].items(), key=lambda kv: -kv[1])[:8]
        print("En cok oldurulen: " + ", ".join("%s x%d" % kv for kv in top))
    if p["quests"]:
        print("Gorevler (kabul -> teslim suresi; ayni surec/oyuncu adi):")
        for quest, q in sorted(p["quests"].items()):
            seconds = q["seconds"]
            avg = fmt_time(sum(seconds) / len(seconds)) if seconds else "?"
            print("  %s kabul=%d teslim=%d XP(odul)=%d ort_sure=%s eslesmeyen=%d tavana_ulasan=%d" % (
                quest, q["accepts"], q["claims"], q["xp_reward"], avg, q["unmatched"], q["capped"]))
        print("(*) XP(quest) nominal oduldur, teslim oncesi seviyeye yazilir; seviye tavanindaki kayip dusulmez.")
        print("(?) Kabul bu logda yoksa (onceki oturum/kayit/ad degisimi) gorev suresi bilinmez.")
    else:
        print("Gorev olayi yok: XP(quest)=0, eski loglarda gercek sifir kazanc kaniti degildir.")
    if p["first_actions"]:
        print("Ilk eylemler (sunucu kontrolcu omru; kalici karakter ilerlemesi degil):")
        for sid, actions in sorted(p["first_actions"].items()):
            for action in FIRST_ACTIONS:
                sample = actions.get(action)
                if sample:
                    if action == "first_loot":
                        detail = "kind=%s item=%s amount=%d" % (sample["kind"], sample["item"], sample["amount"])
                    elif action == "first_kill":
                        detail = "mob=%s mlv=%d xp=%d credit=%s elite=%d" % (sample["mob"], sample["mlv"], sample["xp"], sample["credit"], sample["elite"])
                    elif action == "first_equip":
                        detail = "item=%s instance=%d slot=%d" % (sample["item"], sample["instance"], sample["slot"])
                    elif action == "first_skill":
                        detail = "skill=%s effect=%d mana=%.3f" % (sample["skill"], sample["effect"], sample["mana"])
                    elif action == "first_basic_attack":
                        detail = "target=%s targetid=%s tlv=%d" % (sample["target"], sample["targetid"], sample["tlv"])
                    elif action == "first_death":
                        detail = "by=%s xploss=%d" % (sample["by"], sample["xploss"])
                    elif action == "first_levelup":
                        detail = "from=%d to=%d" % (sample["from"], sample["to"])
                    else:
                        detail = "quest=%s" % sample["quest"]
                    print("  sid=%s %s sure=%s lv=%d %s" % (
                        sid, action, fmt_time(sample["seconds"]), sample["lv"], detail))
                else:
                    print("  sid=%s %s sure=? (bu logda olay yok)" % (sid, action))
    else:
        print("Ilk eylem olayi yok: first_* suresi bilinmiyor; sifir sure veya basarisizlik kaniti degil.")
        print("  first_skill sure=? (bu logda olay yok)")
        print("  first_basic_attack sure=? (bu logda olay yok)")
        print("  first_death sure=? (bu logda olay yok)")
        print("  first_levelup sure=? (bu logda olay yok)")
    if p["first_invalid"] or p["first_duplicates"]:
        print("Ilk olay atlanan: gecersiz=%d tekrar=%d" % (p["first_invalid"], p["first_duplicates"]))


def main(argv):
    path = None
    only = None
    args = list(argv[1:])
    while args:
        a = args.pop(0)
        if a == "--who" and args:
            only = args.pop(0).replace(" ", "_")
        elif not a.startswith("--") and path is None:
            path = a
    if path is None:
        path = os.path.join(project_root(), "Saved", "Logs", "MyProject.log")

    if not os.path.isfile(path):
        print("Log dosyasi bulunamadi: %s" % path)
        print("Once oyunu (PIE) oynayin; sunucu logu Saved/Logs/MyProject.log icine yazilir. "
              "Baska bir log icin: python Scripts/PlaytestMetrics.py <log_yolu>")
        return 0

    events = parse_log(path)
    if not events:
        print("Log icinde [MMOMETRIC] satiri yok: %s" % path)
        print("Olcum satirlari yalniz sunucuda (listen server host / dedicated) basilir ve "
              "seviye atlama, oldurme, olum, altin degisimi ve gorev kabul/tesliminde olusur.")
        return 0

    players = build_stats(events)
    print("Log: %s" % path)
    print("Olay sayisi: %d (levelup=%d kill=%d death=%d gold=%d)" % (
        len(events),
        sum(1 for e in events if e["ev"] == "levelup"),
        sum(1 for e in events if e["ev"] == "kill"),
        sum(1 for e in events if e["ev"] == "death"),
        sum(1 for e in events if e["ev"] == "gold")))
    print("Gorev olaylari: quest_accept=%d quest_claim=%d" % (
        sum(1 for e in events if e["ev"] == "quest_accept"),
        sum(1 for e in events if e["ev"] == "quest_claim")))
    shown = 0
    for who in sorted(players):
        if only and who != only:
            continue
        print_player(who, players[who])
        shown += 1
    if only and shown == 0:
        print("'%s' adli oyuncu icin olay yok. Mevcut: %s" % (only, ", ".join(sorted(players))))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except Exception as exc:  # betik hicbir kosulda hata ile dusmesin
        print("PlaytestMetrics beklenmeyen hata: %s" % exc)
        sys.exit(0)
