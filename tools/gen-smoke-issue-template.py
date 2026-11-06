#!/usr/bin/env python3
"""Generate the Vanilla+ Smoke Test GitHub issue template from SMOKE-CHECKLIST.md.

Why: GitHub renders task-list checkboxes in repo FILES read-only - they are only
clickable inside issues/PRs. The owner runs this checklist manually against every
build, so the checks ship as a classic issue template: one "Smoke Test" issue per
build run, every check pre-filled as a live `- [ ]` box.

Single source of truth is SMOKE-CHECKLIST.md. When a round changes a check there,
re-run this script and commit both files. The transforms are content-keyed (not
line-numbered) so ordinary edits do not break them:

  - the intro block (title -> first `---`) is replaced with a round-agnostic one;
    the **Known-fail paragraph is carried over verbatim
  - `- [x]` items reset to `- [ ]` (a fresh issue is an unrun pass; the old
    CONFIRMED verdicts stay in the text as history)
  - item 0 and the section-I boot gate lose their per-round self-ID pin (the
    template asks for the round under test instead)
  - the giant section-H field-history paragraph collapses into a <details> block
  - a Verdict section is appended for the pass's outcome

Usage: python3 tools/gen-smoke-issue-template.py   (run from the repo root)
"""
import sys
from pathlib import Path

SRC = Path("SMOKE-CHECKLIST.md")
DST = Path(".github/ISSUE_TEMPLATE/smoke-test.md")

REPO = "https://github.com/VTSTech/D2VP-QoL"

FRONT_MATTER = """\
---
name: Vanilla+ Smoke Test
about: Run the manual smoke checklist against one build — the checkboxes are live once the issue is open
title: "Smoke Test — Vanilla+0.0.8-RXX"
---

"""

INTRO = """\
**Build under test:** `Vanilla+0.0.8-RXX` — fill in the round (title too); line 1 of `d2boot.log` must say it (item 0). Tick the boxes as you run — they are live in this issue; paste logs and screenshots as comments. A crash gets its own comments with the artifacts from the table at the bottom.

Core path takes ~15 min. Run top to bottom — the fast checks come first. The repo copy ([SMOKE-CHECKLIST.md]({repo}/blob/main/SMOKE-CHECKLIST.md)) is the canonical reference: the per-round build stamp and field history live there, and any round that adds or changes a check re-runs `tools/gen-smoke-issue-template.py` and commits both.

**Verdict:** fill in the Verdict section at the bottom when the pass is done.

"""


def transform(line: str) -> list[str]:
    """Content-keyed per-line transforms; returns one or more output lines."""
    if line.startswith("- [x] "):
        return ["- [ ] " + line[len("- [x] "):]]
    if line.startswith("- [ ] **0.** Confirm the right exe:"):
        return [
            "- [ ] **0.** Confirm the right exe: line 1 of `d2boot.log` must say the round"
            " under test — `Vanilla+0.0.8-RXX` (fill it in above and in the title). Anything"
            " older, or a `0.0.6` prefix, is the wrong exe — don't test further.\n"
        ]
    if line.startswith("Field status on the R66 build"):
        return [
            "<details><summary>Field history — R63 → R82 (kept from SMOKE-CHECKLIST.md §H)</summary>\n",
            "\n",
            line,
            "\n",
            "</details>\n",
        ]
    if line.startswith("Boot gate: the first"):
        return [
            "Boot gate: the first `d2boot.log` line must print the round under test's"
            " self-ID (item 0 — R73 never booted, its overlay was compile-broken; R74 was"
            " the first post-1792 build).\n",
        ]
    return [line]


def main() -> int:
    if not SRC.exists():
        print(f"error: {SRC} not found (run from the repo root)", file=sys.stderr)
        return 1
    lines = SRC.read_text(encoding="utf-8").splitlines(keepends=True)

    # Split at the first standalone `---`: everything before it is the intro block.
    cut = next(i for i, l in enumerate(lines) if l.rstrip("\n") == "---")
    intro_block, body = lines[:cut], lines[cut:]

    known_fail = "".join(
        l for l in intro_block if l.startswith("**Known-fail")
    )
    if not known_fail:
        print("error: Known-fail paragraph not found in the intro block", file=sys.stderr)
        return 1

    out = [FRONT_MATTER, INTRO.replace("{repo}", REPO)]
    out.append(known_fail)
    # body already begins with the original `---` separator (the intro cut point).

    for line in body:
        out.extend(transform(line))

    out.append(
        "\n## Verdict\n\n"
        "Fill in when the pass is done — what worked / what regressed / artifacts"
        " attached (`d2boot.log`, `d2crash-*.txt`, `glide3x.log` per the table above),"
        " and the one-line owner verdict.\n"
    )

    DST.parent.mkdir(parents=True, exist_ok=True)
    text = "".join(out)
    DST.write_text(text, encoding="utf-8")

    boxes = text.count("\n- [ ] ")
    size = len(text.encode("utf-8"))
    print(f"wrote {DST}: {boxes} open boxes, {size} bytes (issue-body limit 65536)")
    if size > 60000:
        print("warning: close to the GitHub issue-body limit", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
