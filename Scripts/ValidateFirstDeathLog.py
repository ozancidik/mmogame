"""Require accepted first_death markers with globally unique native controller sessions."""
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
    # Count raw event tokens before parse_log can discard missing who or overwrite ev.
    raw_count = sum(len(re.findall(r"(?:^|\s)ev=first_death(?=\s|$)", match.group(1)))
                    for line in raw.splitlines()
                    for match in [metrics.LINE_RE.search(line)] if match)
    events = [e for e in metrics.parse_log(args.log) if e["ev"] == "first_death"]
    players = metrics.build_stats(events)
    accepted = sum("first_death" in actions for p in players.values()
                   for actions in p["first_actions"].values())
    invalid = sum(p["first_invalid"] for p in players.values())
    duplicates = sum(p["first_duplicates"] for p in players.values())
    # The native fixture creates independent controllers in one process. Its SIDs
    # must be unique across who too; the shared parser intentionally groups by who/sid.
    session_count = len({e.get("sid", "").lower() for e in events})
    if (raw_count != args.events or len(events) != args.events or accepted != args.events
            or session_count != len(events) or invalid or duplicates
            or any(not e["who"] or e["who"] == "None" for e in events)):
        print("[MMO_FIRST_DEATH_PARSER_TEST] FAIL raw=%d parsed=%d accepted=%d invalid=%d duplicates=%d sessions=%d" %
              (raw_count, len(events), accepted, invalid, duplicates, session_count))
        return 1
    print("[MMO_FIRST_DEATH_PARSER_TEST] PASS events=%d who/clock/payload accepted" % accepted)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
