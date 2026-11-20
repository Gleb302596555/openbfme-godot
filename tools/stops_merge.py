"""stops_merge.py: the three-way merge of docs/STOPS.md by stop id (owner 2026-11-19: a line union of the shared table doubled 67 stop rows in one
landing, and two sides' edits of one row stayed side by side).

    stops_merge.py <base> <ours> <theirs>      git merge driver: writes the result to <ours>, exit 0 clean / 1 conflict

Registered for docs/STOPS.md in .gitattributes (`merge=stops`); a clone runs it once it has
    git config merge.stops.driver "python3 tools/stops_merge.py %O %A %B"
(tools/colab/batch_bot.py and pr_checker.py set it in their trees; without it git falls back to its line merge).

The file's skeleton (every line, a stop row replaced by a placeholder of its id) is merged by git merge-file, so prose, sections, added and
removed rows merge as lines do. Then every placeholder gets its row, three-way: a row only one side changed takes that side's; a row both changed
is merged word by word (git merge-file on one word per line) and is a conflict when the edits overlap. A conflict leaves git's markers in the
file and exit 1, never a guess. A row id the merged skeleton holds twice keeps its first place.
"""
from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROW = re.compile(r"^\| (S-\d+) \|")
MARK = "@@stops_merge row {}@@"


def split(text: str) -> tuple[list[str], dict[str, str]]:
    """(skeleton lines with placeholders, id -> row) of a STOPS.md text (the first row of an id wins)."""
    skel, rows = [], {}
    for line in text.split("\n"):
        m = ROW.match(line)
        if m:
            rows.setdefault(m.group(1), line)
            skel.append(MARK.format(m.group(1)))
        else:
            skel.append(line)
    return skel, rows


def merge_file(base: str, ours: str, theirs: str, labels=("ours", "base", "theirs")) -> tuple[str, bool]:
    """git merge-file -p over three texts: (result, clean)."""
    with tempfile.TemporaryDirectory() as d:
        paths = []
        for name, text in zip(("o", "b", "t"), (ours, base, theirs)):
            p = Path(d) / name
            p.write_text(text, encoding="utf-8", newline="\n")
            paths.append(str(p))
        done = subprocess.run(["git", "merge-file", "-p", "-L", labels[0], "-L", labels[1], "-L", labels[2], *paths],
                              capture_output=True, text=True, encoding="utf-8", errors="replace")
        if done.returncode < 0 or done.returncode > 127:
            raise SystemExit(f"stops_merge: git merge-file failed: {done.stderr.strip()}")
        return done.stdout, done.returncode == 0


def merge_row(base: str | None, ours: str | None, theirs: str | None) -> tuple[str | None, bool]:
    """One stop row, three-way: (row or None = removed, clean)."""
    if ours == theirs:
        return ours, True
    if ours == base:
        return theirs, True
    if theirs == base:
        return ours, True
    if ours is None or theirs is None:  # one side removed the row, the other changed it
        return ours if ours is not None else theirs, False
    words = lambda s: "\n".join(re.split(r"(?<= )", s)) + "\n"
    merged, clean = merge_file(words(base or ""), words(ours), words(theirs))
    return ("".join(merged.split("\n")) if clean else ours), clean


def merge(base: str, ours: str, theirs: str) -> tuple[str, list[str]]:
    """(merged text, conflicts: the ids or 'skeleton')."""
    sb, rb = split(base)
    so, ro = split(ours)
    st, rt = split(theirs)
    skel, clean = merge_file("\n".join(sb), "\n".join(so), "\n".join(st))
    conflicts = [] if clean else ["skeleton"]
    out, placed = [], set()
    for line in skel.split("\n"):
        m = re.fullmatch(r"@@stops_merge row (S-\d+)@@", line)
        if not m:
            out.append(line)
            continue
        sid = m.group(1)
        if sid in placed:
            continue  # the merged skeleton holds the id twice (both sides added it in different places): its first place
        placed.add(sid)
        row, ok = merge_row(rb.get(sid), ro.get(sid), rt.get(sid))
        if not ok:
            conflicts.append(sid)
            out += ["<<<<<<< ours", ro.get(sid) or "", "=======", rt.get(sid) or "", f">>>>>>> theirs ({sid})"]
            continue
        if row is not None:
            out.append(row)
    return "\n".join(out), conflicts


def main(argv=None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 3:
        print(__doc__)
        return 2
    base, ours, theirs = (Path(p).read_text(encoding="utf-8", errors="replace") for p in argv)
    text, conflicts = merge(base, ours, theirs)
    Path(argv[1]).write_text(text, encoding="utf-8", newline="\n")
    if conflicts:
        print(f"stops_merge: conflicts in docs/STOPS.md: {', '.join(conflicts)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
