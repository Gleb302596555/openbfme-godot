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
| `factc abilities: every SPECIAL_POWER button of the Goblins' and Angmar's units and hero...` | `test_factc_sweep.cpp:694`, `CHECK( r.outcome.compare(0, 2, "ok") == 0 ) is NOT correct!` |
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
| `LOCAL-20 ennorath: Ennorath scenario retaliation with real GondorFighterHorde (MaxRetal...` | `test_local_20.cpp:301`, `CHECK( AIGuardMachine::getRetaliateRange(*gondorHorde) == 250.0f ) is NOT correct!` |
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

## Нестабильные

| Тест | Как ведёт себя |
|---|---|
| `fbsim1 retail: a right press on a CANCELABLE...` | падает в наборе (`test_hud6.cpp:787`), отдельно проходит |
| `net retail hud5` | падает время от времени (и в прогоне `f7255b2f`) |
| `cheat1 mode: every command's effect and limits in a Cheat Mode game` | упал в полном прогоне `f7255b2f`, отдельно проходит |
| `LOCAL-5 capture flash timing` | упал в полном прогоне `f7255b2f`, отдельно проходит |
