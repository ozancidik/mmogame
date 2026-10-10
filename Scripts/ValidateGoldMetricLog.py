"""Check actual production gold rows against independent native-fixture expectations."""
import argparse
import re
from pathlib import Path

MAX_GOLD = 999999999
CASES = {
    "plain": [("diger", 100, 100)],
    "explicit": [("loot", 25, 25)],
    "nested_from": [("satin_alma", -100, 200), ("yukseltme", -50, 150)],
    "nested_plain": [("gorev", 100, 200), ("diger", 20, 220)],
    "nested_then_plain": [("loot", 80, 100), ("gorev", 30, 130), ("diger", 40, 170)],
    "deep": [("loot", 100, 300), ("yukseltme", -60, 240), ("gorev", 5, 245), ("diger", -10, 235)],
    "cap": [("loot", 5, MAX_GOLD), ("satin_alma", -20, MAX_GOLD - 20)],
    "floor": [("yukseltme", -5, 0), ("gorev", 7, 7)],
    "zero": [],
    "cap_unchanged": [("loot", -1, MAX_GOLD - 1)],
    "floor_unchanged": [("loot", 1, 1)],
    "detach": [("loot", 50, 60)],
    "npc": [],
    "null_source": [("diger", 11, 11)],
    "returns_seed": [("loot", 10, 110), ("diger", -10, 100)],
}
BEGIN = re.compile(r"\[MMO_GOLD_METRIC_BEGIN\] case=(\S+) who=(\S+) expected=(\d+)$")
EXPECT = re.compile(r"\[MMO_GOLD_METRIC_EXPECT\] case=(\S+) src=(\S+) amount=(-?\d+) balance=(\d+)$")
GOLD = re.compile(r"\[MMOMETRIC\] t=\d+(?:\.\d+)? ev=gold who=(\S+) src=(\S+) amount=(-?\d+) balance=(\d+)$")
END = re.compile(r"\[MMO_GOLD_METRIC_END\] case=(\S+)$")


def validate(raw):
    errors, seen, active, total = [], set(), None, 0
    # Pre-scan identities so rows before the first BEGIN are checked too.
    fixture_who = {m.group(2) for line in raw.splitlines() if (m := BEGIN.search(line))}
    for line in raw.splitlines():
        if "[MMO_GOLD_METRIC_BEGIN]" in line:
            m = BEGIN.search(line)
            if not m or active:
                errors.append("invalid/nested begin")
                continue
            name, who, count = m.groups()
            if name not in CASES or name in seen or int(count) != len(CASES.get(name, [])) or who in ("?", "None"):
                errors.append("invalid begin " + name)
            active = {"name": name, "who": who, "expected": [], "actual": []}
        elif "[MMO_GOLD_METRIC_EXPECT]" in line:
            m = EXPECT.search(line)
            if not active or not m or m.group(1) != active["name"]:
                errors.append("invalid expectation")
            else:
                active["expected"].append((m.group(2), int(m.group(3)), int(m.group(4))))
        elif "[MMOMETRIC]" in line and re.search(r"\bev=gold\b", line):
            if not active:
                who = re.search(r"\bwho=(\S+)", line)
                if who and who.group(1) in fixture_who:
                    errors.append("fixture gold outside case")
                continue
            m = GOLD.search(line)
            if not m or m.group(1) != active["who"]:
                errors.append("invalid identity/schema " + active["name"])
            else:
                active["actual"].append((m.group(2), int(m.group(3)), int(m.group(4))))
                total += 1
        elif "[MMO_GOLD_METRIC_END]" in line:
            m = END.search(line)
            if not active or not m or m.group(1) != active["name"]:
                errors.append("invalid end")
            else:
                name = active["name"]
                if active["expected"] != CASES.get(name) or active["actual"] != CASES.get(name):
                    errors.append("payload/order " + name + " actual=" + repr(active["actual"]))
                seen.add(name)
                active = None
    if active or seen != set(CASES) or total != 23:
        errors.append("incomplete cases/events seen=%d events=%d" % (len(seen), total))
    return errors


def self_test():
    lines = []
    for name, events in CASES.items():
        lines.append("[MMO_GOLD_METRIC_BEGIN] case=%s who=Fixture expected=%d" % (name, len(events)))
        for src, amount, balance in events:
            lines.append("[MMO_GOLD_METRIC_EXPECT] case=%s src=%s amount=%d balance=%d" % (name, src, amount, balance))
        for src, amount, balance in events:
            lines.append("[MMOMETRIC] t=1.0 ev=gold who=Fixture src=%s amount=%d balance=%d" % (src, amount, balance))
        lines.append("[MMO_GOLD_METRIC_END] case=" + name)
    good = "\n".join(lines)
    assert not validate(good)
    row = "[MMOMETRIC] t=1.0 ev=gold who=Fixture src=diger amount=100 balance=100"
    mutations = ["", good.replace(row, ""), good.replace(row, row + "\n" + row),
                 good.replace(row, row.replace("src=diger", "src=loot")),
                 good.replace(row, row.replace("amount=100", "amount=101")),
                 good.replace(row, row.replace("balance=100", "balance=101")),
                 good.replace(row, row.replace("who=Fixture", "who=Other")),
                 good.replace("[MMO_GOLD_METRIC_END] case=plain", ""),
                 good.replace("ev=gold who=Fixture src=loot amount=50 balance=60", "ev=gold who=Fixture src=loot amount=57 balance=67"),
                 row + "\n" + good, good + "\n" + row,
                 good.replace("[MMO_GOLD_METRIC_END] case=plain", "[MMO_GOLD_METRIC_END] case=plain\n" + row)]
    for bad in mutations:
        assert validate(bad), "invalid fixture accepted"
    assert not validate(row.replace("who=Fixture", "who=UnrelatedMapPlayer") + "\n" + good)
    print("[MMO_GOLD_METRIC_VALIDATOR_SELF_TEST] PASS checks=14")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.log:
        parser.error("log required")
    errors = validate(Path(args.log).read_text(encoding="utf-8-sig", errors="replace"))
    for error in errors:
        print("[MMO_GOLD_METRIC_LOG_TEST] FAIL " + error)
    if errors:
        return 1
    print("[MMO_GOLD_METRIC_LOG_TEST] PASS cases=15 metrics=23 identity/source/delta/balance/order")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
