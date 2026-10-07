"""Require the shared parser to accept every native fixture first_skill marker."""
import argparse
import re
from pathlib import Path

import PlaytestMetrics as metrics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log")
    parser.add_argument("--events", type=int, required=True)
    args = parser.parse_args()
    raw = Path(args.log).read_text(encoding="utf-8", errors="replace")
    raw_count = len(re.findall(r"\[MMOMETRIC\].*\bev=first_skill\s", raw))
    events = [e for e in metrics.parse_log(args.log) if e["ev"] == "first_skill"]
    players = metrics.build_stats(events)
    accepted = sum("first_skill" in actions for p in players.values() for actions in p["first_actions"].values())
    invalid = sum(p["first_invalid"] for p in players.values())
    duplicates = sum(p["first_duplicates"] for p in players.values())
    if (raw_count != args.events or len(events) != args.events or accepted != args.events
            or invalid or duplicates or any(not e["who"] or e["who"] == "None" for e in events)):
        print("[MMO_FIRST_SKILL_PARSER_TEST] FAIL raw=%d parsed=%d accepted=%d invalid=%d duplicates=%d" %
              (raw_count, len(events), accepted, invalid, duplicates))
        return 1
    print("[MMO_FIRST_SKILL_PARSER_TEST] PASS events=%d who/clock/payload accepted" % accepted)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
