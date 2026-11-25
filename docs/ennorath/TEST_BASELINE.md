# Базовый список падений

Правило в [PROCESS.md](PROCESS.md), раздел 7: падение перед `land` допускается, только если тест записан здесь с тем же сообщением. Записи добавляются только по полному прогону на чистом `main`. Если тест исправлен, запись убирается в той же ветке.

Основание: полный прогон `1ac41052` (2026-11-02), перепроверка на `2af911bb` и на `0b58d290`. На `b9581453` полный прогон ещё не сделан: при первом таком прогоне список нужно обновить.

## Окружение этой машины

| Тест | Причина |
|---|---|
| `audio decode: MP3` (5 тестов) | нет `lame` и `mpg123` |
| `binary: AptMainMenu`, `binary: AptSkirmish` | нет `game.dat` в `ROTWK_INSTALL` |
| `win1 msvcr71 real` | нет `msvcr71.dll` в `ROTWK_INSTALL` |

## Падают на чистом main, не исправлены

| Тест | Место или сообщение |
|---|---|
| `backlog8 retail: the FLOATING_Z horde members...` (P-31) | `test_hud_horde2_retail.cpp:991`, `memberIds(archers).size() == 6` |
| `MODS-1` (3 теста: `overridden Apt movie`, `parseMod`, `precedence`) | `test_mods1_mount.cpp:270`, `:110`, `:171` |
| `script4 conditions: BUILT_BY_PLAYER...` | `test_script4.cpp:446`, `PLAYER_HAS_SCIENCEPURCHASEPOINTS` |
| `hero abilities retail: FactionIsengard` | `test_hero_retail.cpp:1461`, `unexplained == 0` |
| `online16-fix` (3 теста), `online3 spectators`, `onlineux1 r2` | `remove_all`: файл занят другим процессом |
| `online2: a classic room's relay log...` | `test_online_relay.cpp:334`, digest |
| `relay4: valid one-human room logs...` | `test_online_relay.cpp:750`, digest |

## Полный прогон `f7255b2f` (2026-11-13, чистый `main`, Windows MSVC, все три установки)

6372 теста: 6319 прошли, 53 упали, 29 пропущены (`doctest::skip`), `CRASHED` нет. Все падения ниже падают и на `e29c0a39` (до слияния upstream `1d505ac3`), и на сборке вчерашнего `main` `8b12b926`, то есть внесены не посадками 2026-11-13. Место — первая ошибка теста.

| Тест | Место или сообщение |
|---|---|
| `AIPOW-1: end to end: a Hard computer's wounded Aragorn uses Athelas on his own (SELFAOE...` | `test_aipow1.cpp:686`, `CHECK( frame == 23 ) is NOT correct!` |
| `RAI-POW-2: DEBUFFECONOMYBUILDING (RW 0x9E74B7) takes the enemy record's first visible, ...` | `test_aipow1.cpp:1621`, `CHECK( empty.data()[0] == 5 ) is NOT correct!` |
| `audio decode: corrupted MP3 fixtures decode or throw AudioDecodeError, nothing else` | `test_audio_decode.cpp:831`, `REQUIRE( problem.empty() ) is NOT correct!` |
| `bfme1 fields: BFME1's parsers accept every field in BFME1's tables, apart from the list...` | `test_bfme1_fields.cpp:128`, `CHECK( gaps.count(r) == 1 ) is NOT correct!` |
| `bfme2camp1 retail: MAP BEGINNER TUTORIAL (mode 6) plays its first lessons by its own sc...` | `test_bfme2camp1.cpp:1995`, `CHECK( objectiveMarked(m, k) ) is NOT correct!` |
| `stableqa1: Evil Erebor's porters marching on a team path in formation keep their group ...` | `test_bfme2camp1.cpp:2281`, `REQUIRE( marching > 0 ) is NOT correct!` |
| `bridgeclick1 retail: a right click reaches the river bridge or Celduin boardwalk deck` | `test_bridge1_bridges.cpp:167`, `CHECK( input.ui().cursor() == MouseCursorName::Move ) is NOT correct!` |
| `camp1 bonus: MAP ANG Bonus (ANGMAR_BONUS_CAMPAIGN) plays from its intro to the victory ...` | `test_camp1_missions.cpp:118`, `CHECK( st.genericScriptsFired == generic ) is NOT correct!` |
| `cbhunt: ActionManager::canDoSpecialPowerAtObject (RW 0x82D925): the generic tests, the ...` | `test_cbhunt.cpp:329`, `CHECK( unported ) is NOT correct!` |
| `exit1 retail: a Udun 2v2 Hard computer game: produced members leave their exit path and...` | `test_exit1_motion.cpp:332`, `CHECK( probe.seen.size() >= 400 ) is NOT correct!` |
| `fx3 retail: an orc warrior's MOVING InfantryDustTrails plays nothing (RW 0x73AECB: NULL...` | `test_fx3_particles.cpp:135`, `CHECK( pitBoneMiss ) is NOT correct!` |
| `hero abilities retail: FactionIsengard` | `test_hero_retail.cpp:1461`, `CHECK( unexplained == 0u ) is NOT correct!` |
| `capture flag presentation: authored capture events select one faction and snapshot the ...` | `test_hero_retail.cpp:2399`, `CHECK( drawables.findByObject(flag->getID())->getHouseColor() == g.players.getNeutralPlayer()->getPlayerColor(` |
| `heroring1: the grid carries an alpha decal after the merge decals: its own list in each...` | `test_heroring1_decals.cpp:181`, `CHECK( SelectionDecalShaderMode(g) == 0 ) is NOT correct!` |
| `factb inventory: Isengard and Mordor reach every structure, unit, hero, upgrade and pow...` | `test_hud_factb_inventory.cpp:71`, `CHECK( classes == (std::string(f) == "FactionIsengard" ? isengard : mordor) ) is NOT correct!` |
| `factb structures: every Mordor unit and structure upgrade from the structures' buttons` | `test_hud_factb_production.cpp:87`, `CHECK( false ) is NOT correct!` |
| `factc found: every Goblin castle expansion from its pad's buttons` | `test_hud_factb_production.cpp:87`, `CHECK( false ) is NOT correct!` |
| `factc found: every Angmar castle expansion from its pad's buttons` | `test_hud_factb_production.cpp:87`, `CHECK( false ) is NOT correct!` |
| `factc upgrade effects: every Goblin and Angmar weapon and armour upgrade changes the hi...` | `test_hud_factb_units.cpp:129`, `CHECK( false ) is NOT correct!` |
| `factc spell book: every Goblin and Angmar spell bought in the store and cast through th...` | `test_hud_factb_units.cpp:129`, `CHECK( false ) is NOT correct!` |
| `ring delivery retail: a pending carrier exit survives a whole-game save loaded into a f...` | `test_hud_garrison_retail.cpp:783`, `REQUIRE( b.logic.loadSnapshot(snapshot, &error) ) is NOT correct!` |
| `backlog8 retail: the FLOATING_Z horde members of the retail data: the Mumakil's Haradri...` | `test_hud_horde2_retail.cpp:991`, `REQUIRE( a.memberIds(archers).size() == 6 ) is NOT correct!` |
| `move SPEED: a formation move caps every member's locomotor at the slowest member's max ...` | `test_move_speed.cpp:425`, `CHECK( w.aiOf(fast)->curLocomotor()->temporarySpeedCap() == 6.0f ) is NOT correct!` |
| `onlineux1 r2 (Sol 1) retail: a rejoin that fails before it is live leaves nothing behin...` | `test_online3_rejoin.cpp:2398`, `test case THREW exception: remove_all: The process cannot access the file because it is being used by another ` |
| `pickup1: a computer player's horde walks to a treasure chest in a temporary AI machine,...` | `test_pickup_modules.cpp:297`, `CHECK( p.ai->currentStateId() == (unsigned)AI_MOVE_TO ) is NOT correct!` |
| `play4 heroes retail: every faction's heroes, recruited from the fortress, fight an enem...` | `test_play4_heroes_retail.cpp:468`, `CHECK( own.lost == 525.0f ) is NOT correct!` |
| `attack1 heroes retail: the mounted Witch-king and Mouth of Sauron, ordered onto a horde...` | `test_play4_heroes_retail.cpp:664`, `CHECK( ord.weaponShots == c.weaponShots ) is NOT correct!` |
| `play4 stances retail: a fleeing enemy is chased out of the guarded spot in Battle; in H...` | `test_play4_stances_retail.cpp:396`, `CHECK( hold.rangeAtAttack == hold.guardRange ) is NOT correct!` |
| `save4b retail T4: a real 4-player skirmish's snapshot is written (SAVE-4c and 4d merged...` | `test_save4b_retail.cpp:449`, `CHECK( classes == std::set<std::string>{ "AISpecialPowerUpdate", "ActiveBody", "ArmorUpgrade", "ArrowStormUpda` |
| `ship1 retail: an Elven battle ship sails into range, its five archers sink an enemy tra...` | `test_ship1_ships.cpp:435`, `CHECK( pitHits == 486 ) is NOT correct!` |
| `shroud objects: a geometry row that does not parse is an error naming the template, nev...` | `test_shroud.cpp:781`, `test case THREW exception: Bogus: Geometry 'BOGUS' is not SPHERE, CYLINDER or BOX` |
| `siege1 attack retail: Gondor's trebuchet hits a Helm's Deep wall from its range (PRIMAR...` | `test_siege1_attack.cpp:102`, `CHECK( r.healthAfter == 5578.68506f ) is NOT correct!` |
| `siege1 attack retail: Mordor's catapult made inside its MinimumAttackRange (150) of Hel...` | `test_siege1_attack.cpp:154`, `CHECK( r.healthAfter == 2935.83984f ) is NOT correct!` |
| `siege1 attack retail: a mumak and the Dwarven demolisher break Helm's Deep's main gate;...` | `test_siege1_attack.cpp:175`, `CHECK( m.firstHit == 134 ) is NOT correct!` |
| `siege1 gate: the stone and bolt throwers in front of Helm's Deep's gate break it; the a...` | `test_siege1_gate.cpp:240`, `CHECK( c.shots == 9 ) is NOT correct!` |
| `siege1 gate: Mordor's catapult and Gondor's trebuchet in front of Minas Tirith's gate b...` | `test_siege1_gate.cpp:277`, `CHECK( c.contactsWhileStanding == 1 ) is NOT correct!` |
| `topple1 retail: a Fornost_FallingTower of map wor ang fornost killed from the east fall...` | `test_topple1_retail.cpp:104`, `CHECK( onCrush == 29 ) is NOT correct!` |
| `shader text: the camera-normal mapper's world path reads the same world-space normal` | `test_w3d_shadergen.cpp:53`, `CHECK( has(code, "vec4(vec3(v_wnrm.x, -v_wnrm.z, v_wnrm.y), 1.0)") ) is NOT correct!` |
| `shader budget: the Fx, occlusion, blend and alpha-override cross product fits Intel's 29` | `test_w3d_shadergen.cpp:294`, `CHECK( varyingNames(result) == expectedW3DVaryings(fx, shadow, false) ) is NOT correct!` |
| `shader budget: no other W3D key field changes the varying set` | `test_w3d_shadergen.cpp:396`, `CHECK( varyingNames(result) == expected ) is NOT correct!` |
| `warg1 retail: Isengard warg riders trained at a level 2 warg pit and sent with a player...` | `test_warg1_retail.cpp:224`, `CHECK( t.victimsEnd == 0 ) is NOT correct!` |
| `warg1 retail: Rohirrim trained at a level 2 stable and sent with a player's attack orde...` | `test_warg1_retail.cpp:239`, `CHECK( t.crushes == 17 ) is NOT correct!` |

## Полный прогон `43a14cb4` (2026-11-24, песочница 4, Linux GCC, `night_full.sh`)

Неполный: файл `test_mods1_retail.cpp` оборвался. Остальное: 7302 прошли, 81 упали (82 случая). Около 25 падений — Port 273 (нет `ShroudManager` в тестовых мирах, Local 159 #926); версии xfer — исправлены 2026-11-24. Остальные 23 прошли бисекцию на ПК (Windows MSVC; на `f7255b2f` все 23 проходят). Записи — до исправления, каждая со своей P.

| Тест | Место | Причина (отрезок) |
|---|---|---|
| `BACKLOG2 S-950` | `test_backlog2_retail.cpp:363` | 2026-11-22 до слияния upstream (P-178; `S-653` исправлен `c4e7fa9b0`) |

## Приёмка 2026-11-24 (`accept-1124`: партия 2026-11-23g + Locals 159-170, Windows MSVC, перенос в `main`)

Запуски на сборке слияния с `5868a72d4`, с установками игр; полного прогона не было (выбор `test_select` = FULL), полный прогон — ночью.

- **Local 159 (#940) убрал исключение `no shroud manager`** во всех тестах группы Port 273: следующие теперь проходят: `SPELL-1 retail*` (все), `SPELL-2 retail*` (все), `SPELL-3 retail*` (все), `herocast1 retail*`, `hero execution*`, `SPELLFIX-1 retail` (2 из 3), `SPELLAUDIT-1` (без исключения).
- `blocks: the lazily built Flammability table…` проходит (P-179), запись удалена из таблицы выше.
- `factb spell book: every Isengard / Mordor spell book power*` проходят после `197dae512` (P-191, пины S-1997 сняты): 2 из 2.
- Тесты `modgap2 ai: ...` (5) упали на слиянии: Local 169 (#948) перенёс таблицу полей `RainOfFireUpdate` (RW 0xC6B618, `DarknessLevel` вместо `Darkness`); фикстура теста исправлена в этом же переносе, записи не нужны.
- Пины `perf1*`, `uber1*`, `skirmish ai vs ai*`, `save4b*` не сдвинулись (43 из 43).

Остаются красными после переноса (не туман; причины — по описанию Local 159 и совпадают с ночным прогоном, не перепроверены):

| Тест | Место | Запись |
|---|---|---|
| `ENNORATH Arnor: powers are purchased and cast through the message path` | `test_mods1_retail.cpp:737`, `:738`, `:740` (`CHECK( countOwned(objectName) == n + 1 )`: 0 вместо 1 на `SCIENCE_NumenorMastery`) | падает после посадки 2026-11-24, причина не искалась (P-193); Local 159 его чинил, в слитой сборке красный |
| `hero abilities retail: FactionMen / Elves / Dwarves / Isengard / Mordor / Wild / Angmar` | `test_hero_retail.cpp:1457`, `:1480` | P-194 |
| `SPELLAUDIT-1: every faction's spell book power...`, `SPELLFIX-1 retail: a spell book structure clears the trees...` | `test_spellaudit1.cpp:359`, `:563`, `:991` | P-194 (`SpellBookSpawnLoneTower*`, `SpellBookBarricade`: ветка RW 0x82E2DE не портирована) |
| `hero facing: ...` (2 теста) | `test_hero_facing_execution.cpp:174`, `:313` | P-194 |
| `b2spell1 MAP GOOD Rivendell...`, `b2spell1 campaign powers played` (2 теста) | `test_b2spell1.cpp:45`, `test_ownerfix1_input.cpp:1172` | P-194 |
| `hero activation targeting: ...` (2 теста) | `test_hero_activation_targeting.cpp:167`, `:211` | P-192 |

## Приёмка 2026-11-24/25 (`accept-local-1124b`: Locals 171-176, 178-182, PR #964-#974, Windows MSVC, перенос в `main`)

Запуски на сборке слияния, с установками игр; полного прогона не было. Local 177 (PR #975) возвращён: с ним падают `modgap2 audio loop retail`, `ENNORATH scenario ModelConditionAudioLoopClientBehavior`, `skirmish ai vs ai: Evendim...`, `save4b retail T4` (`test_save4b_retail.cpp:454`), `script2 angmar: ... undefended Uber citadel loss` (`test_script2_angmar.cpp:150`); без вызова `handleEngagedFlag` все проходят.

- Пин `perf1` (skirmish, с кадра 845) сдвинул Local 181 (#962, PR #970: `SlaughterHordeContain::update` / `onDie`, RW 0x8840D4 / 0x883A83); сборка с этими кусками, возвращёнными назад, совпала со старыми векторами; переснят один раз. Остальные пины (`perf1` battle и blob, `uber1`, `skirmish ai vs ai*`, `save4b*`) не двигались.
- Красные и после слияния, не от этой партии: `BACKLOG2 S-950` (`test_backlog2_retail.cpp:363`, строка выше), `AIPOW-1: ... Athelas` (`test_aipow1.cpp:687`), `RAI-POW-2: DEBUFFECONOMYBUILDING` (`test_aipow1.cpp:1624`), `attack1 heroes retail` (`test_play4_heroes_retail.cpp:672`), `ring delivery retail` (`test_hud_garrison_retail.cpp:783`): все в списках выше (номера строк сдвинулись).
- Новая запись, не от этой партии (код погоды в правках не менялся; сборка без причины не проверялась): `MapWeatherScan / WaterSetScan retail` падает: `test_map_render.cpp:889`, `data\ini\weather.ini: WeatherSetting defined twice`.

## Нестабильные

| Тест | Как ведёт себя |
|---|---|
| `fbsim1 retail: a right press on a CANCELABLE...` | падает в наборе (`test_hud6.cpp:787`), отдельно проходит |
| `net retail hud5` | падает время от времени (и в прогоне `f7255b2f`) |
| `LOCAL-5 capture flash timing` | упал в полном прогоне `f7255b2f`, отдельно проходит |
