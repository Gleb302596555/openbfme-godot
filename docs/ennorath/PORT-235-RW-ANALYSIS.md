# PORT-235 / issue #558 — итог аудита GameData (28 полей)

Closes #558

## Источники и правило атрибуции

- RotWK 2.01 `google_drive/game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`.
- Ennorath `google_drive/ennorath-ini-1.9.8.3.zip`, MD5 `c53b6dea885cab044f80e783565f41f3`.
- Проверены все 28 имён в `origin/main` (`b8d60d3c39c6d990daf84a0cda73ae8377fe075a`) и в текущем исходном дереве. Совпадение смещения само по себе не считалось чтением GameData; неподтверждённые основы оставлены unresolved.
- Архивы и `game.dat` не добавлены в репозиторий.

## Изменение PORT-235

`DefaultStructureRepairBuffFxList` — RW row `0xBFF8D0`, parser `0x42EE5E`, `GlobalData +0x9E0`. RW `StructureBody` data ctor `0x656D03` разрешает строку в module-data `+0x48`; после роста здоровья `ActiveBody::attemptHealing` (`0x8C2FC1`) читает `+0x48` в `0x8C30FA` и вызывает FX helper `0x4B1B5A`, но сохраняет два retail gate: не `KindOf STRUCTURE` (`0x8C30EE`) и не `AIRBORNE_TARGET` (`0x8C3101`). Следовательно, это условный generic-healing consumer, а не доказательство FX при обычном Dozer-ремонте здания.

Порт добавляет парсинг в `GameLogicSettings.cpp` и выбирает structure default только для `StructureBody` в уже существующем FX-событии `ActiveBody::attemptHealing`. Явный `HealingBuffFx` и `HealingBuffFx = None` остаются overrides. Нового simulation state, `crc` или `xfer` нет. Ennorath задаёт `DefaultStructureRepairBuffFxList = DefaultStructureRepairBuffFx`; список содержит `BuffNugget` типа Healing.

`engine/tests/test_port_235.cpp` использует синтетические GameData/Object INI: default FX на healed non-STRUCTURE `StructureBody`, явный FX override, явный `None`, отсутствие FX для `STRUCTURE` и `AIRBORNE_TARGET`. В тесте один case / 37 assertions.

Проверка Ennorath object definitions была перепроверена с учётом форматирования: 579 object-scoped `Body = StructureBody`; 13 строк `KindOf` были пропущены старым сканом из-за нестандартных отступов, но все они содержат `STRUCTURE`. `WILD_HOLE_KINDOF` раскрывается в `PRELOAD STRUCTURE ...`; единственные три оставшиеся текстовые non-STRUCTURE шаблона — `StartObjectGeneric` и два CINE skybox placeholder, ни один не показывает достижимый healing-путь. Поэтому Ennorath in-game witness не найден; в игре этот эффект не запускался, и синтетический тест удерживает условие. В песочнице нет установленного Ennorath executable.

## Все 28 полей

| № | Поле (значение Ennorath) | Итог / доказательство |
|---:|---|---|
| 1 | `DefaultStructureRepairBuffFxList` (`DefaultStructureRepairBuffFx`) | **ported**: условный FX event для non-STRUCTURE `StructureBody` после actual health rise; обычный structure repair и airborne targets исключены как в RW. Нет подходящего Ennorath healing object, поэтому проверка — синтетический `PORT-235*`. |
| 2 | `GetHealedAnimationTime` (`4.0`) | **RotWK читает (клиент, условно)**: `AutoHealBehavior::update` RW `0x8558C0`, только при `SingleBurst`, GameLogic `+0x9A`, непустом `GetHealedAnimationName` (`+0x128`) и найденном Anim2D. В Ennorath имя закомментировано как removed; не портировано. См. batch-17 #25. |
| 3 | `GetHealedAnimationZRise` (`15.0`) | **RotWK читает (клиент, условно)**: тот же путь, RW `0x855CF2` читает `+0x130` и передаёт ZRise в `0x6A1875`. В Ennorath имя закомментировано; не портировано. См. batch-17 #26. |
| 4 | `GroundStiffness` (`0.8`) | **left: S-7636**: `+0xBC` имеет сырые displacement-кандидаты, но их GameData base/caller не доказаны; не объявлен no-reader. См. batch-17 #29. |
| 5 | `GroupSelectMinSelectSize` (`5`) | **left: S-7636**: RW `+0xB44` кандидаты `0xAB7DD4`, `0xAB8D53`, `0xAB8FFB`; GameData owner не установлен. См. batch-17 #30. |
| 6 | `GroupSelectVolumeBase` (`0.5`) | **RotWK не использует**: row `0xC005A0`, `+0xB48`; 19 same-offset sites принадлежат другим base/типам, ни один не прослеживается к `[0xDE4364]`. `manual-check.tsv`, batch-18 #1. |
| 7 | `GroupSelectVolumeIncrement` (`0.02`) | **RotWK не использует**: row `0xC005B0`, `+0xB4C`; восемь same-offset sites — чужие объекты, GameData dataflow отсутствует. `manual-check.tsv`, batch-18 #2. |
| 8 | `HideGarrisonFlags` (`No`) | **RotWK не использует**: `+0x13D` шесть sites — bumpmap flags, чужой module data/UI predicate и другой class; ни один не читает GameData. `manual-check.tsv`, batch-18 #23. |
| 9 | `InitialMaxRingLevel` (`60`) | **RW читает только Living World ветку**: GameData `+0xE84`, reader `0x6B3517`; caller `0x7827C8` сначала проверяет `[0xDE4950]` и выходит при нуле. S-525 говорит, что обычная skirmish не достигает ветки; не переносить отдельный геттер как skirmish rank cap. |
| 10 | `LevelGainAnimationName` (`LevelGainedAnimation`) | **left: S-7635**: static Anim2D consumer RW `0x6957B9` найден, но у функции нет доказанного incoming call/entry; результат lookup идёт в UI helper `0x6A1875`. Не утверждать runtime playback без caller. См. batch-18 #28. |
| 11 | `LevelGainAnimationTime` (`4.0`) | **left: S-7635**: RW `0x695988` читает `+0x120` внутри того же Anim2D consumer `0x6957B9`; reachability не установлена. См. batch-18 #29. |
| 12 | `LevelGainAnimationZRise` (`15.0`) | **left: S-7635**: RW `0x69597E` читает `+0x124` внутри того же consumer; reachability не установлена. См. batch-18 #30. |
| 13 | `MaxCellsAdjustTargetDestination` (`400`) | **left: S-7634**: live RW read `0x6F5025` передаёт budget в spiral search `0x6F4BD0`; его общий AI target-destination caller chain `0x6CBFCD` .. `0x66E39A` отсутствует в порте. PORT-36/issue #121 owner decision — не переносить budget отдельно от механизма. |
| 14 | `MaxCellsAdjustToNearestGroundCell` (`400`) | **RotWK runtime не использует**: единственный static read `0x6EF0F5` находится в `0x6EF0C1`, у которой нет входящих вызовов или абсолютных указателей. `manual-check.tsv`; batch-19 #5 и `port36-cellbudgets-rw.md` §2.2. |
| 15 | `MaxCellsAdjustToNearestValidCell` (`400`) | **left: S-530 / S-931**: единственный consumer — `OCLSpecialPower::CREATE_AT_EDGE_*` / Dragon Strike (`0x8C7657`, `0x8C7688`); порт не моделирует edge spawn. См. `port36-cellbudgets-rw.md` §2.3. |
| 16 | `MaxCellsFindAttackPathSideways` (`2500`) | **left: S-784**: RW `0x6FDE38`, attack-search vtable `0xC1B8D8 +0xC`, ship sideways search; портирует только ground variant. См. `port36-cellbudgets-rw.md` §2.4. |
| 17 | `MaxFieldParticleCount` (`30`) | **RotWK не использует**: полный GameData-base scan не нашёл `+0xACC` read; apparent `0xAB7E01` читает FX-manager argument object, не GlobalData. `manual-check.tsv`, batch-19 #10. |
| 18 | `MaxParticleCount` (`2500`) | **RotWK читает (клиент)**: particle manager `0x5FC43D` / `0x5FC459`. Upstream уже имеет `ParticleSettings::maxParticleCount` и StaticGameLOD override (`ParticleSys.cpp`, `GameLODManager.cpp`), но GameData row `+0xAC8` не загружается; значение Ennorath совпадает с default. Client-only — отдельный GameData parser не добавлялся. См. batch-19 #11. |
| 19 | `MaxShellScreens` (`8`) | **left: S-7636**: bounded scan нашёл no direct `[0xDE4364]+0x38` reader; shell's `[0xDE412C]+0x38` — другой global, но argument-owned path не исключён. Не объявлен no-reader. См. batch-19 #16. |
| 20 | `MaxUnitSelectSounds` (`8`) | **left: S-7636**: reads `0xABEB58` / `0xABA025` на `+0xB50` реальны, но base не доказан; `0xAB39C0` читаeт от того же pointer `+0x17C4/+0x1814`, вне GlobalData size. См. batch-19 #18. |
| 21 | `ParticleScale` (`1.0`) | **RotWK читает (клиент)**: particle sizing RW `0x5F4BB0`, `0x5F4C23`, `0x7B0703` и др. Upstream уже имеет `ParticleSettings::particleScale` default `1.0` и client consumer `ParticleSys.cpp`, но GameData `+0x9EC` не загружается; render-only, оставлен без новой ветки. См. batch-20 #10. |
| 22 | `StructureStiffness` (`0.3`) | **RotWK не использует**: ctor write `0x642B47`; 592 raw `+0xC0` hits owner-checked, ни один не читает GameData. `manual-check.tsv`, batch-20 #30. |
| 23 | `TerrainHeightAtEdgeOfMap` (`100.0`) | **RotWK не использует**: ctor write `0x642B2F`; прочих GameData-owned reads нет. `manual-check.tsv`, batch-21 #2. |
| 24 | `TintUnitIfPathingForMoreThan` (`0`) | **RW читает в path queue**: threshold reads `0x6F2548` / `0x6F2581` / `0x6F25AF` in `0x6F2364`; timeout triggers Drawable tint flag / red diagnostic effect via `0x6ED163`. Client-visible debug diagnostic, not a gameplay budget; left out under S-164's path-queue scope. `gamedata-rows.md` corrected from the stale client/no-reader summary. |
| 25 | `ScoreKeeper_NormalVictoryRequiredObjectivesPercentage` (`50`) | **Уже parsed/hashed в main** (`GameLogicSettings.cpp:389`), не добавлять второй path. RW rating body `0x79E175` reads `+0x11B0`, but wrapper `0x79E218` has no reachable caller; owner decision remains S-7186. |
| 26 | `ScoreKeeper_NormalVictoryRequiredScore` (`10000`) | **Уже parsed/hashed в main** (`GameLogicSettings.cpp:388`); RW static rating read `0x79E1B7`, wrapper unreachable; S-7186. |
| 27 | `ScoreKeeper_TotalVictoryRequiredScore` (`0`) | **Уже parsed/hashed в main** (`GameLogicSettings.cpp:387`); RW static rating read `0x79E199`, wrapper unreachable; S-7186. |
| 28 | `StealthFriendlyOpacity` (`50%`) | **left: S-7636**: upstream parses/stores it in `VisionSettings.cpp:119,218`, but no port consumer uses it and the RW `+0xC8` displacement candidates are not attributed to GameData. Not a RotWK no-reader verdict. |

## Manual checks and stops

- `docs/ennorath/manual-check.tsv` contains hand-checked `not-read` / `parsed-unused` port verdicts with the RW addresses. Its ScoreKeeper and Stealth rows explicitly do not assert that RW has no static reader.
- New project-stop fragments: S-7634 (shared AI target-destination search), S-7635 (LevelGain Anim2D reachability), S-7636 (unowned GameData displacement candidates). Existing follow-ups remain S-525, S-7186, S-530 / S-931, S-784 and S-164.
- `docs/ennorath/gamedata-rows.md` corrects two stale summaries: `MaxShellScreens` is unresolved (not a proven client read), and `TintUnitIfPathingForMoreThan` is read in logic and triggers a client debug tint.

## Checks

- `/tmp/cmake-venv/bin/cmake --build build/port235 --target openbfme_tests -j4` — passed on the rebased `origin/main` baseline.
- `build/port235/openbfme_tests -tc='PORT-235*'` — 1/1 test case, 37/37 assertions passed; 7115 skipped on the rebased `origin/main` baseline.
- `build/port235/openbfme_tests -tc='ownerfix1*'` — 16 cases returned, but all retail-dependent cases were skipped because `ROTWK_INSTALL` and `BFME2_INSTALL` are absent (0 assertions).
- Full suite and Ennorath in-game check were not run. The sandbox has the verified data archives but no installed Ennorath executable; no in-game result is claimed.
- Do not merge this branch.
