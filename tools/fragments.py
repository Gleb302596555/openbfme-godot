#!/usr/bin/env python3
"""New stop rows and NIGHT_WATCH rows as one file each, so parallel branches never edit the same table (owner, 2026-11-14).

A branch adds, it does not edit the shared tables:
  docs/stops.d/S-<id>.md                  line 1: the stop row `| S-<id> | kind | ... |`; optionally a blank line and
                                          a `## ...` section (it goes to the end of docs/STOPS.md)
  docs/ennorath/night_watch.d/<name>.md   one journal row `| <date> | ... |` of docs/ennorath/NIGHT_WATCH.md

Closing or narrowing an existing stop is still an edit of its own row in docs/STOPS.md (one row, no conflict).
Stop ids come with the issue (`reserve`), so two branches never take the same one.

    fragments.py check [--issue N]        validate the fragments (format, ids unused in STOPS.md and among fragments) and, with the
                                          branch's issue (--issue, else $OPENBFME_ISSUE, else `git config openbfme.issue`), that no
                                          new stop takes an id reserved for another issue (Port 212 took #578's S-7423, 2026-11-17);
                                          the commit gate runs the same check
    fragments.py collect                  landing only: fold the fragments into the tables, delete them, sync README's count
    fragments.py reserve <issue> [count]  next free stop ids for an issue, recorded in docs/stops.d/RESERVED.tsv
    fragments.py next                     the next free stop id (STOPS.md, fragments and reservations)
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
STOPS = REPO / "docs" / "STOPS.md"
STOPS_D = REPO / "docs" / "stops.d"
RESERVED = STOPS_D / "RESERVED.tsv"
NIGHT = REPO / "docs" / "ennorath" / "NIGHT_WATCH.md"
NIGHT_D = REPO / "docs" / "ennorath" / "night_watch.d"

ROW = re.compile(r"^\| (S-(\d+)) \| (code|project|closed) \|.*\|\s*$")
NIGHT_ROW = re.compile(r"^\| (\d{4}-\d{2}-\d{2}) \|.*\|\s*$")
NIGHT_HEAD = "|---|---|---|---|---|---|---|---|---|---|---|"


def read(p: Path) -> str:
    # a Windows checkout has CRLF: a fragment's blank line before its `## ...` section must not read as "\r" (S-7434, 2026-11-17)
    return p.read_bytes().decode("utf-8").replace("\r\n", "\n")


def stop_ids(text: str) -> set[int]:
    return {int(m.group(1)) for m in re.finditer(r"^\| S-(\d+) \|", text, re.M)}


def stop_fragments() -> list[Path]:
    return sorted(STOPS_D.glob("S-*.md")) if STOPS_D.is_dir() else []


def night_fragments() -> list[Path]:
    return sorted(NIGHT_D.glob("*.md")) if NIGHT_D.is_dir() else []


def parse_stop(p: Path) -> tuple[int, str, str]:
    """(id, row, section) of a stop fragment; a malformed one is an error."""
    lines = read(p).rstrip("\n").split("\n")
    m = ROW.match(lines[0])
    if not m:
        raise ValueError(f"{p.relative_to(REPO)}: line 1 must be a stop row `| S-<id> | code|project|closed | ... |`")
    if p.stem != m.group(1):
        raise ValueError(f"{p.relative_to(REPO)}: the file name must be the row's id {m.group(1)}.md")
    rest = "\n".join(lines[1:]).strip("\n")
    if rest and not rest.startswith("## "):
        raise ValueError(f"{p.relative_to(REPO)}: after the row only a blank line and a `## ...` section may follow")
    return int(m.group(2)), lines[0].rstrip(), rest


def parse_night(p: Path) -> str:
    lines = [l for l in read(p).split("\n") if l.strip()]
    if len(lines) != 1 or not NIGHT_ROW.match(lines[0]):
        raise ValueError(f"{p.relative_to(REPO)}: one journal row `| YYYY-MM-DD | ... |` expected")
    if lines[0].count("|") != NIGHT_HEAD.count("|"):
        raise ValueError(f"{p.relative_to(REPO)}: {lines[0].count('|') - 1} columns, the journal has {NIGHT_HEAD.count('|') - 1}")
    return lines[0].rstrip()


def check(issue: str = "") -> list[str]:
    problems = []
    taken = stop_ids(read(STOPS))
    seen: set[int] = set()
    issue = (issue or my_issue()).lstrip("#")
    res = reservations() if issue else []
    own = [(a, b) for i, a, b in res if i == issue]
    for p in stop_fragments():
        try:
            sid, _, _ = parse_stop(p)
        except ValueError as e:
            problems.append(str(e))
            continue
        if sid in taken:
            problems.append(f"{p.relative_to(REPO)}: S-{sid} is already a row of docs/STOPS.md (take the issue's reserved id)")
        others = sorted({i for i, a, b in res if a <= sid <= b and i != issue})
        if others and not any(a <= sid <= b for a, b in own):
            mine = ", ".join(f"S-{a}" if a == b else f"S-{a}..S-{b}" for a, b in own) or "none: take `python tools/fragments.py next`"
            problems.append(f"{p.relative_to(REPO)}: S-{sid} is reserved for issue #{', #'.join(others)} (docs/stops.d/RESERVED.tsv); "
                            f"issue #{issue}'s ids: {mine}")
        if sid in seen:
            problems.append(f"{p.relative_to(REPO)}: S-{sid} twice")
        seen.add(sid)
    for p in night_fragments():
        try:
            parse_night(p)
        except ValueError as e:
            problems.append(str(e))
    return problems


STOP_ROW = re.compile(r"^\| (S-\d+) ")


def sort_rows(lines: list[str]) -> list[str]:
    """The table's stop rows in id order as tools/sim/test_sim_audit.py checks it (the "S-<n>" strings sorted; 2026-11-18: the gate failed on
    S-7330 .. above S-001). The rows are sorted among their own line positions; every other line stays where it is."""
    at = [i for i, line in enumerate(lines) if STOP_ROW.match(line)]
    rows = sorted((lines[i] for i in at), key=lambda line: STOP_ROW.match(line).group(1))
    out = list(lines)
    for i, row in zip(at, rows):
        out[i] = row
    return out


def collect() -> int:
    problems = check()
    if problems:
        print("\n".join(problems))
        return 1
    stops = read(STOPS)
    lines = stops.split("\n")
    last_row = max(i for i, l in enumerate(lines) if l.startswith("| S-"))
    rows, sections = [], []
    for p in stop_fragments():
        sid, row, section = parse_stop(p)
        rows.append((sid, row))
        if section:
            sections.append(section)
    rows.sort()
    lines[last_row + 1:last_row + 1] = [r for _, r in rows]
    out = "\n".join(sort_rows(lines))
    for s in sections:
        out = out.rstrip("\n") + "\n\n" + s + "\n"
    STOPS.write_bytes(out.encode("utf-8"))
    night = read(NIGHT)
    nrows = sorted((parse_night(p) for p in night_fragments()), reverse=True)
    if nrows:
        head = night.index(NIGHT_HEAD) + len(NIGHT_HEAD)
        night = night[:head] + "\n" + "\n".join(nrows) + night[head:]
        NIGHT.write_bytes(night.encode("utf-8"))
    for p in stop_fragments() + night_fragments():
        p.unlink()
    sys.path.insert(0, str(REPO / "tools"))
    import stops_count
    stops_count.main(["--write"])
    print(f"collected {len(rows)} stop rows ({len(sections)} sections), {len(nrows)} night-watch rows")
    return 0


def my_issue() -> str:
    """The branch's issue for the reservation check: $OPENBFME_ISSUE, else `git config openbfme.issue` (the prompt sets it), else ""."""
    if os.environ.get("OPENBFME_ISSUE"):
        return os.environ["OPENBFME_ISSUE"].strip()
    try:
        return subprocess.run(["git", "-C", str(REPO), "config", "--get", "openbfme.issue"], capture_output=True, text=True).stdout.strip()
    except OSError:
        return ""


def reservations() -> list[tuple[str, int, int]]:
    """The tree's reservations and those of the last fetched origin/main: an issue reserved after the branch started counts too."""
    texts = [read(RESERVED)] if RESERVED.exists() else []
    try:
        fresh = subprocess.run(["git", "-C", str(REPO), "show", "origin/main:docs/stops.d/RESERVED.tsv"], capture_output=True)
        if fresh.returncode == 0:
            texts.append(fresh.stdout.decode("utf-8").replace("\r\n", "\n"))
    except OSError:
        pass
    out = []
    for text in texts:
        for l in text.splitlines()[1:]:
            if l.strip():
                issue, first, last = l.split("\t")[:3]
                if (issue, int(first), int(last)) not in out:
                    out.append((issue, int(first), int(last)))
    return out


def next_free() -> int:
    used = stop_ids(read(STOPS))
    for p in stop_fragments():
        used.add(int(p.stem[2:]))
    top = max(used)
    for _, _, last in reservations():
        top = max(top, last)
    return top + 1


def reserve(issue: str, count: int) -> int:
    first = next_free()
    last = first + count - 1
    if not RESERVED.exists():
        STOPS_D.mkdir(parents=True, exist_ok=True)
        RESERVED.write_bytes(b"issue\tfirst\tlast\n")
    with RESERVED.open("ab") as f:
        f.write(f"{issue}\t{first}\t{last}\n".encode("utf-8"))
    print(f"S-{first}" if count == 1 else f"S-{first} .. S-{last}")
    return 0


def main(argv: list[str]) -> int:
    if not argv or argv[0] not in {"check", "collect", "reserve", "next"}:
        print(__doc__)
        return 2
    if argv[0] == "check":
        problems = check(argv[argv.index("--issue") + 1] if "--issue" in argv[:-1] else "")
        print("\n".join(problems) or f"fragments ok ({len(stop_fragments())} stops, {len(night_fragments())} night-watch)")
        return 1 if problems else 0
    if argv[0] == "collect":
        return collect()
    if argv[0] == "next":
        print(f"S-{next_free()}")
        return 0
    return reserve(argv[1], int(argv[2]) if len(argv) > 2 else 3)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
