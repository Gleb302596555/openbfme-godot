# PORT-238 regression status

## Full executable regression run (branch e4c68fc91, main 36c1777a0)

The full `openbfme_tests` target built successfully, then was rebuilt after the
upstream `factc` change. `PORT-238*`: 35 cases, 2184 assertions, no failures.
A combined filter `PORT-286*,PORT-232*,PORT-33*,PORT-222*,LOCAL-81*,PORT-252*,PORT-258*,PORT-124*,hero2: SpecialPowerTimerRefreshSpecialPower*,HeroModeSpecialAbilityUpdate*` reported 49 cases and 2167 assertions, no failures; LOCAL-81's real Ennorath case explicitly skipped.

Night-watch filters, run separately (cases may overlap): `SPELL-2*` 12/8,
`*stealth*` 53/823, `*control bar*` 11/80, `*order*` 427/54998,
`*garrison*` 92/1213, `save4*` 89/2574, `perf1*` 11/1414, `uber1*`
7/66 (cases/assertions, all reported passing). With `RW_GAME_DAT` set to
the local vanilla RW 2.01 executable, combined `*stealth*,*control bar*,*garrison*`
reported 152 cases and 2204 assertions passing. These counts include doctest
cases which print `SKIP` and return normally: missing retail installations
mean real-data checks did **not** run. New PROCESS acceptance filters `factb*`
and `factc*` reported 22 and 26 cases respectively but **zero assertions**:
all require the missing retail files, so neither acceptance sweep was verified.
The separately extracted Ennorath INIs are not a complete game installation.
No game, visual, complete hero audit, or full unfiltered suite is claimed.
