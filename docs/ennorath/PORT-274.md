# PORT-274: `FIRE_WEAPON` buttons with an object target (issue #641)

Пять кнопок мода (Эннорафт 1.9.8.3) из таблицы `MATRIX.tsv` показывали `NO-SEND` — «нажатие `FIRE_WEAPON` ничего не отправило»:

| фракция | объект | кнопка | данные мода |
|---|---|---|---|
| Evilmen | `HaradMumakil`, `HaradMumakilForOliphant` | `Command_ChargeAttack` | `FIRE_WEAPON`, `PRIMARY`, `NEED_TARGET_ENEMY_OBJECT OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE NEED_UPGRADE`, `NeededUpgrade = Upgrade_MumakilLevel2`, `EnableOnModelCondition = USER_9` (`commandbutton.ini:2921`) |
| Rohan | `RohanErkenbrand`, `RohanErkenbrand_Summoned` | `Command_ErkenbrandChargeAttack` | `FIRE_WEAPON`, `PRIMARY`, `NEED_TARGET_ENEMY_OBJECT`, `EnableOnModelCondition = MOUNTED` (`commandbutton.inc:42962`) |
| Elves | `RohanSam_mod` | `Command_SamBratpfanne` | `FIRE_WEAPON`, `SECONDARY`, `NEED_TARGET_ENEMY_OBJECT`, `AutoAbility = Yes`, `DisableOnModelCondition = WEAPONSET_TOGGLE_1` (`commandbutton.inc:19691`) |

## Что делает RotWK (адреса)

Нажатие и клик — две разные ветки, обе прочитаны в `game.dat` 2.01:

1. **Нажатие** `ControlBar::processCommandUI` (`RW 0x940435`). Для `FIRE_WEAPON` (`+0x14 == 0x17`) и `SPECIAL_POWER` (`0x18`) ветка `RW 0x94070F` спрашивает `CommandButton::isValid`-предикат `RW 0x75CC3E` на объекте-источнике; для `FIRE_WEAPON` он проверяет `NeededUpgrade` (`RW 0x75CC4A` -> `RW 0x691421`) и дедлайн `Object + 0x478` (`RW 0x75CC69`). Затем общий гейт опций `RW 0x940865`: `mov edi,[esi+0x1c]` / `test di,0x227` — при любом бите цели (`NEED_TARGET_ENEMY_OBJECT 0x1`, `NEUTRAL 0x2`, `ALLY 0x4`, `NEED_TARGET_POS 0x20`, `CONTEXTMODE_COMMAND 0x200`) нажатие **не отправляет сообщение**, а ставит режим ожидания цели: `RW 0x94089C` -> `InGameUI::setGUICommand` (нажатие той же кнопки снимает режим, `RW 0x940891`).
2. **Клик** `GUICommandTranslator` (`RW 0x83D7AD`). Ветка `RW 0x83D3A2` для `Options & 7` (объект-цель): выбор цели `RW 0x83D30E` (пик `RW 0x83D41A`, проверка отношения `RW 0x75CBE5` -> `RW 0x75CBB9` по битам 0/1/2 опций), затем сообщение `0x40F` = `MSG_DO_WEAPON_AT_OBJECT` (`RW 0x83D44B`) с аргументами: `WeaponSlot` (`+0x80`, `RW 0x83D453`), `ObjectID` цели (`RW 0x83D462`), `MaxShotsToFire` (`+0xA0`, `RW 0x83D491`). Без подходящей цели и при `Options & 0x10` (`NEED_TARGET_POS`) — `0x40D` = `MSG_DO_WEAPON_AT_LOCATION` (`RW 0x83D478`), иначе ничего. Позиционная и объектная ветки здесь **одни и те же**, отдельного пути для объекта нет.

Логика уже была перенесена (lane FACT-A): `WeaponCommands::atObject` (RW `0x77A394 .. 0x77A3C1`: блокировка слота `LOCKED_TEMPORARILY`, `groupIdle`, `groupAttackObject`) и `atPosition` (RW `0x77A327 .. 0x77A33A`).

## Что оказалось на самом деле

Клиентский путь **исправен**. Синтетические тесты `PORT-274*` проходят на неизменённом движке: нажатие входит в режим цели, левый клик по врагу отправляет `MSG_DO_WEAPON_AT_OBJECT` с нужными слоем/целью/лимитом, слот стреляет по цели, клик по союзнику отказан и режим сохраняется, а соседняя кнопка без опций цели по-прежнему отправляет `MSG_DO_WEAPON`.

`NO-SEND` в свипе объясняется **доступностью**, а не потерей сообщения:

* свип свипа `test_hud_factb_units.cpp` нажимает кнопку и считает `press()` ложным, если `ControlBar::pressButton` вернул `false` — а он возвращает `false` на `ButtonState::Restricted`/`NotReady` (`ControlBarCommandProcessing.cpp`), ещё до ветки команды;
* у всех пяти кнопок есть `EnableOnModelCondition` (`USER_9` у маммака, `MOUNTED` у Эркенбранда) или `DisableOnModelCondition` (`WEAPONSET_TOGGLE_1` у Сэма), и свип ставит условие только когда у него есть кнопка-сеттер;
* `ControlBar::evaluate` применяет `modelConditionsAllow` ко **всем** командам, кроме `SPECIAL_POWER` (см. ниже — это и есть настоящее расхождение с RW).

Проверка в `PORT-274*`: та же кнопка с `EnableOnModelCondition = MOUNTED` при невыполненном условии даёт `ButtonState::Restricted` и нажатие действительно ничего не отправляет; после `setModelConditionState(MOUNTED, true)` та же кнопка — `Enabled`, а нажатие + клик отправляют `MSG_DO_WEAPON_AT_OBJECT`.

### Найденное расхождение с RW (нужна отдельная правка)

`RW 0x942733` (доступность команды, `docs/re/rotwk/RW_0x942733.md`) в общих воротах:
`RW 0x94280D cmp dword ptr [edi+0x14], 0x18` / `RW 0x942811 je 0x942826` — вызов `RW 0x942490` (проверка `Disable`/`EnableOnModelCondition`, вызов на `RW 0x942818`) **пропускается для команды `0x18` (`SPECIAL_POWER`)** и только для неё.

Порт (`engine/src/GameClient/ControlBar.cpp`, `ControlBar::evaluate`) повторяет это исключение для `SPECIAL_POWER`, но применяет `modelConditionsAllow` ко всем остальным командам, включая `FIRE_WEAPON`. В RW это так: гейт `0x18` — единственное исключение, `FIRE_WEAPON` его не имеет. То есть по букве `RW 0x942811` порядок порта совпадает с retail, и `NO-SEND` для маммака и Эркенбранда — **верное поведение** при невыполненном условии: кнопка серая, нажимать нечего.

**INFERENCE:** что именно ставит `USER_9` у маммака (`ModifierList MumakilUpEnabler` из ауры `AttributeModifierAuraUpdate` в `mumakilpennew.ini:365`/`375`, `Range 9000`) и `MOUNTED` у Эркенбранда в сцене свипа — из одних INI не следует; свип не смог дать условия для `USER_9` (у него нет кнопки-сеттера), поэтому строка осталась `NO-SEND`. Это ограничение свипа, а не движка.

## Правка

Движок не менялся: клиентский путь и логика верны, а `NO-SEND` был ложным вердиктом свипа. Правился сам свип
(`engine/tests/test_hud_factb_units.cpp`) в двух местах:

* `modelGate` — новый предикат по тому же правилу, что RW 0x942490: `EnableOnModelCondition` выполнен, если пересечение его маски с битами
  объекта **непусто** (ANY), а `DisableOnModelCondition` — выполнен, если пересечение пусто; при невыполненном гейте свип возвращает
  `DISABLED: needs model <имена битов>` и **не нажимает** кнопку;
* обе ветки вердикта (команда и способность) сначала читают этот предикат, поэтому серая кнопка с модельным гейтом больше не попадает в
  ветку `!sent` -> «the press ... sent nothing».

Для пяти кнопок issue это значит: маммак получает `DISABLED: needs model USER_9` (бит ставит аура `MumakilUpEnabler`, кнопки-сеттера у
свипа нет), Эркенбранд — `DISABLED: needs model MOUNTED` (сеттер `SpecialAbilityToggleMounted` свип пробует), Сэм — гейт снят
(`WEAPONSET_TOGGLE_1` не выставлен), кнопка нажимается по-настоящему.

Сужен `[S-1871]` (`engine/src/GameLogic/ActionManager.cpp:35`, `docs/STOPS.md`): `Object + 0x478` — это
`Object::m_fireWeaponDeadline` (дедлайн + 10 с, `RW 0x68C91B`), FIRE_WEAPON-половина `canDoSpecialPowerAtLocation` перенесена
(`RW 0x75CC5A` + `RW 0x75CC69`); стоп теперь про неперенесённый тест очереди производства апгрейд-кнопок (`RW 0x68C327`).

`[S-1990]` уже закрыт в `docs/STOPS.md:463` — трогать нечего.

## Проверка

```
./engine/build/openbfme_tests -tc="PORT-274*"
[doctest] test cases:   5 |   5 passed | 0 failed | 6864 skipped
[doctest] assertions: 135 | 135 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Синтетика (`engine/tests/test_port_274.cpp`, `engine/cmake/Port274.cmake`), файлов игры не нужно. Полный прогон на ветке: 6842 случая /
18 падений / 1444722 проверки — до правки было 6840 / 18 / 1444664; множества падений совпадают (нет `lame`/`mpg123` для `audio decode`,
нет `ROTWK_INSTALL`/`BFME2_INSTALL` для retail-частей, плюс `cbhunt`, `hero facing`, `heroring1`, `synthetic HUD`, `move SPEED`,
`PORT-169`, `shroud objects`, `wall span`, `bfme1 fields`), каждое воспроизводится поодиночке и ни одно не трогает файлы ветки. Соседи:
`AIPOW-1*` 15/15, `stops*` 58/58.

## Осталось

* **Правки движка нет**, поэтому в игре меняется только отчёт матрицы: строки пяти кнопок уходят из `NO-SEND` в `DISABLED: needs model ...`
  (или в настоящий вердикт, когда условие выполнено).
* Стоп `S-7463..S-7465` не понадобился: ни одно поведение не осталось неперенесённым.
