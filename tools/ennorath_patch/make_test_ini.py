#!/usr/bin/env python3
"""The INI for the in-game check of the upgrade-limit guard (fork issue 514, Local 69): Ennorath's own Upgrade.ini with N dummy
upgrades after it.

    python make_test_ini.py 200 <Ennorath's data/ini/upgrade.ini, extracted> > <mod folder>/data/ini/upgrade.ini

The dummies are plain PLAYER upgrades (`Upgrade Test_0001` .. `Test_<N>`), written from scratch here. RotWK reads upgrades only from
Data\\INI\\Default\\Upgrade.ini and Data\\INI\\Upgrade.ini (TheUpgradeCenter's InitFile lines in Default\\SubsystemLegend.ini), so the
dummies go at the end of Ennorath's Upgrade.ini (its #include lines keep pulling the rest from the archive). Ennorath 1.9.8.3 holds 1098
upgrades at start-up (3 veterancy + 1095 INI blocks), so with N >= 55 the dummy Test_0055 is number 1153: the patched game.dat stops with
the guard's message naming it; the unpatched one keeps loading and writes past its masks.
"""
from __future__ import annotations

import sys
from pathlib import Path


def dummy_upgrades(count: int, prefix: str = "Test_") -> str:
    if count < 1:
        raise ValueError("count must be at least 1")
    blocks = [f"Upgrade {prefix}{i:04d}\n  Type = PLAYER\n  BuildTime = 1.0\n  BuildCost = 0\nEnd\n" for i in range(1, count + 1)]
    return "\n; upgrade-limit guard check (fork issue 514): remove the mod folder after the check\n\n" + "\n".join(blocks)


def main(argv=None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 2:
        raise SystemExit(__doc__)
    base = Path(argv[1]).read_bytes().decode("latin1")
    sys.stdout.write(base.rstrip("\r\n") + "\n" + dummy_upgrades(int(argv[0])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
