# PORT-234 — от события к звуку (RotWK 2.01)

Источник — линейный `tools/mod/rw.py dis` на retail `daed3668…`. Это **не** 39 новых звуков при клике. Есть поля без читателя, уже работающие читатели и читатели в механизмах, которых в порте нет. Проверка данных — совмещённые INI Ennorath 1.9.8.3, не список значений из issue.

## Как найти читателя

`RW 0x45C675` устанавливает vtable MilesAudioManager `0xBDB550`; `45CAC8/45CAE7/45CAF6` создают `0xF4` байт через `RW 0x5ECE45` и кладут в manager +14. Слоты `+138` и `+13C` указывают **на один** `RW 0x9F9F37` (`return this[+14]`). `RW 0x5ECE27`: TheAudio `0xDE42FC`, слот `+13C`, `initFromINI` с таблицей `0xBF5450`; поле через `RW 0x73B217`, NoSound — пустая ссылка. Поиск вызовов по адресу глобала **и** инструкций vcall `+138` по всей `.text` (в частности, в `715A84` регистр с TheAudio переживает цикл). От возвращённого указателя прочитаны инструкции до использования, включая короткие `add eax,imm8` и переходы. Динамический индекс `91FBD7` относится только к пяти VolumeSample. Вызов `RW 0x6DA95E` не означает «позиционный»: `RW 0x6DA67E` задаёт позицию, `RW 0x6DA6A7` ID объекта, `RW 0x66ECE1` игрока; `RW 0x6DB5B3` сразу конструирует событие на объекте. Проигрывает слот `+64`.

| Поле MiscAudio (смещение hex) | Кто и когда спрашивает в RW; соответствие порту |
|---|---|
| RadarNotifyInfiltrationSound 08 | `6996DC:699750–699784` до смены команды: запись радара есть, обе стороны playable; `6D9A71` только у прежнего локального владельца создаёт событие 4, строку и звук для игрока. Не любая смена владельца. В порте нет самой записи `Object+268` (Radar::captureObject). |
| RadarNotifyOnlineSound 0C / RadarNotifyOfflineSound 10 | `6AB311/6AB3B4` add/remove, `6AB457/6AB4EB` disable/enable: переход `hasRadar` и локальный игрок. Не при каждом построенном здании. Общие счётчики/модули радара отсутствуют, S-2453. |
| DefectorTimerTickSound 20 / DefectorTimerDingSound 24 | `852A3E:852C49` тик захвата уже есть. `6996DC:6997D0–699847` после смены команды тик на объекте для новой стороны, только с drawable. `8E38DD:8E3927–8E39B3` окончание таймера: очистить флаг, с drawable и FX — белая вспышка и ding; до срока смерть с AI или status 13 прерывают без ding. `8E39E4–8E3A97`: только переход нечётной целой фазы во вторую (чётную), не каждый кадр. |
| AllCheerSound 28 | `81F8D8:820128–820186`: только LAN/internet (`441B7C`), звук и **одно** `MSG_DO_CHEER`. Не скриптовая анимация отдельного юнита. |
| StealthDiscoveredSound 30 / StealthNeutralizedSound 34 | `7767A9:776968/776A36/776ACE` и `81ACD7:81AD94/81AE58/81AEF1`: reveal из StealthUpdate и InvisibilityManager. Локальный владелец, feedback 2 — neutralized на раскрытом; не союзник, видимый drawable и detector — discovered **на детекторе**; союзник, feedback 2 — discovered на раскрытом. Игрок звука — локальный. Нужен клиентский reveal с состоянием drawable, не второй опрос в LiveGameAudio. |
| MoneyDepositSound 38 / MoneyWithdrawSound 3C | `7B17EF:7B1809–7B1863` ограничивает расход остатком, `7B18B8:7B18CD–7B1921`: нулевая операция молчит, флаг sound, событие без координат с `Money+8` как player index **перед** изменением баланса и score. DONOR BFME2 0x003B0CB3 / 0x003B0D7C (`MoneyRva003B0D7C.cpp`). |
| BuildingDisabled 40 / BuildingReenabled 44; VehicleDisabled 48 / VehicleReenabled 4C; SplatterVehiclePilotsBrain 50 | `6907F1` сначала проверяет будущий срок. `690843`: UNMANNED не-DRONE звучит даже при повторном таком же сроке (`690954` позже). `69087C–690939`: EMP/UNDERPOWERED звучат только если не было обоих; STRUCTURE раньше MACHINE (биты 7 и 11: `690896/6908ED`, таблица `DA0E68`; Vehicle — имя звукового слота, не KindOf). `692443:69246E–692555`: снимаемый бит должен стоять, второй не должен; приоритет видов тот же. Все пять — **снимок позиции**, без игрока. DONOR BFME2 0x00290114 (`ObjectScriptStatus.cpp`). |
| CrateHeal 54 / CrateShroud 58 / CrateFreeUnit 5C / CrateMoney 60 | Четыре различных override `CrateCollide` слота `+30`, вызываемые `8C06B6`. `8C0A02` после лечения — позиция **сборщика**; `8C0C6F` после шрауда — его ID; `8C0E06` только при найденном шаблоне юнита — его (сборщика) ID; `8C0B32` после возможного `7B18B8` — его ID. Этих четырёх экономических модулей нет в активных объектах мода. Сундуки используют **другой** override `8C158B`, щиты — `8C10DB`, ни один не вызывает этих слотов. Не ставить CrateMoney на возврат денег SalvageCrateCollide. |
| UnitPromoted 64 | `6957B9:69581A–695833/6959AE` — старый обработчик переходов четырёх VeterancyLevel, рост, видимость, не IGNORED_IN_GUI, включены иконки. Нет rel32-вызовов или указателя `6957B9` в образе. Действующий путь XP `79D141 → 79DA9D → 821319` использует LevelUpFx; это не данный читатель. |
| RepairSparks 68 | `8A58FA:8A5A88–8A5ACD` — в подмашине ремонта SlavedUpdate после размещения искр на позиции. Не обычный ремонт портером; подмашина отсутствует (S-1426). |
| EnterCloseCombat 6C / ExitCloseCombat 70 / BuddyMessageNotification 7C | Нет обращений к этим смещениям результата audio getter. `5ECE45` обнуляет поля, `5ECF79` освобождает, таблица только заполняет; `75DF26` выбирает 8C/90, не 6C/70. Проверены также DONOR BFME2 0x001ED6EE / 0x001ED70C / 0x001ED840 (`INIMiscAudio.cpp`, `MiscAudioClass.h`, поиск имён и m_6C/m_70/m_7C у потребителей и всех audio vcall). Нет основания играть звук ни при ударе, ни при изменении списка друзей. |
| IncomingChatNotification 74 / PrivateMessageNotification 78 | `8D354A` входящая **игровая** строка (уже `game.gd`); `5EF0F0:5EF1E8` **GameSpy**, не своё сообщение, не public, после фильтра текста, view 2. Это не второй звук игрового чата. |
| DisabledButtonClicked 88 | `715A84:715AF6–715BA2`: видимый hit-child, enabled/ignoreEnableCheck/бит `20000000` → рекурсия с обоими флагами. Иначе только при playDisabledSound и TheAudio звук view 2, остаётся обход братьев. Не Palantir (свои клипы). |
| ShellMapLoadMusic 98 / SaveFileLoadMusic A0 | `81C79C` режим загрузки shell **карты**: если музыка не играет, view 2, fade off. Не фоновая картинка (S-1912). `626A87:626B5F/626C4C/626D22` передаёт A0 picture screen для **film still**, в т.ч. replay и отсутствие картинки, не только save. Явная картинка миссии берёт её собственные треки; Players screen — другая ветка. |
| GameSpyCommunicatorOpen C4 | `94F607:94F622/94F6D6–94F70F` событие 2 оверлея GameSpy, при определённом результате его slot 10, не каждый показ AptMessenger. |
| RIFThingTemplateReloadedSound C8 / RIFObjectsRefreshedSound CC | `6D2880:6D2932`, `6316EF:6319D6` — developer rapid iteration, перезагрузка шаблона / refresh объектов. Не загрузка INI при старте; reload type 5 отсутствует (S-632). |
| FastForwardModeOn D0 / FastForwardModeOff D4 | `81F8D8:820542–820624`: только replay (mode 3), переключается флаг GlobalData+BBD, сообщение на экран и звук согласно **новому** флагу. Не скорость CLI при начале повтора. |
| UnableToSetRallyPoint DC | `779544:779664–77974C` отказ именно QuickDoesPathExist, не произвольный неправильный приказ (S-202). |
| PlanningModeOrderGiven E0 | `8DDF3B/8DE838/8DEDBB` — move, attack, enter уже идут через planningOrderVoice. Отдельно `8DFAE9–8DFB5F`: только для кнопки TOGGLE_STANCE (0x39; SET_STANCE = 0x3A), а не для любой кнопки с UnitSpecificSound. |
| BuildingPlacementSound E4 / WallPlacementSound EC | `779A3D:77AA00/77B12A/77C5B0` — **после вызова** BuildAssistant для plot, dozer, span, без проверки возвращаемого значения. `77C5DA/77B174` объект-строитель/хаб, не созданное здание, плюс индекс его владельца. В wall-ветке `77C536` требует order mode 0. Не при перерисовке призрака и не по одному разу на сегмент. |
| TargetObjectWithSpecialPowerSound F0 | `81E147:81E243`: вспышка цели и звук уже в `CommandXlat.cpp`; второго запроса не нужно. |

## Объекты, голоса и границы

- `8DEDBB:8DF484–8DF4FF` строит `VoiceEnterUnit` + **точное имя шаблона** цели; `8DF7FE/8DFCE1` используют имя из SpecialPower. Обе динамические карты уже читаются в `UnitVoiceResponse`; нет общей строки «любой транспорт» или «любое проклятие». В 1.9.8.3 нет объекта ровно `TransportShip`, ссылки специальной силы на `VoiceSpecialAbilityCurseEnemy` закомментированы. `8DF74F` читает Primary/Secondary/Tertiary **только** для 43A, не 457 (см. исследование PORT-85). AutoAbility 7E4/7E5 — команды кнопки, не кольцо; звук выбран в `ControlBarCommandProcessing::rightPressButton`. 46A требует общей очереди приказов.
- `8DE838:8DEABD–8DEBC6` узнаёт строки camp/retreat, включая enter-state, через `676B30 → 676A8A`; оба селектора уже участвуют (`objectVoice`/`fillRow`). В отличие от них удар (`68FFBD`, 35) и петля движения (`7486A8`, 23) в логике используют только шаблон. Нужен один общий клиентский lookup с тем же состоянием RandomSoundSelector, что и голос; он не должен бросать симуляционный генератор или разыменовывать drawable с logic worker (S-701/707/712). Так же для `SoundCreated/StealthOn/Off/Enter/Exit/FallingFromPlane/Promoted*/AmbientBattle`: читатели строк `2C/2A/2B/2F/30/34/31–33/29` ни в коем случае не поля MiscAudio.
- `825DC2` требует существующий CrowdResponse во время разбора объекта (S-700; отдельные HUD/world stores). `8DD7DC` проверяет ENGAGED и `701443(me,68E43B(),6E,null,null,1)`; attack-move `8DFBE5` также ищет вокруг точки приказа с половиной дальности (`8DFC35`) и передаёт найденный target attack helper. В `AIClosestEnemy::adjustVictim` последний аргумент **игнорируется** и horde pick использует logic RNG; вызывать эту функцию из клиента небезопасно (S-702). `8DEDBB:8DF079–8DF106` даёт герою INT_MIN или валидный ранг его **текущей записи уровня** (не сырой XP), сравнение `8DD663`; порядок указателей (S-705) этим не меняется.
- `698F06:698F63/6990DD–699140`: self-kill и DEATH_FADED молчат; остальные — owner, иначе local→team ALLIES, иначе enemy (в т.ч. NEUTRAL); источник позиции — жертва. Watch живых объектов в `LiveGameAudio` не знает причину смерти. `67B4B7/5DCC88` — другой, damage, с drawable far-frame, contain и таймерами EVA (S-7090).
- `64DF64 → 89C04B`, gate interfaces: `89C261` новый OPENING/CLOSING добавляет configured loop на gate ID и сохраняет handle; `89C1A4` сначала remove handle, затем конечный звук прежнего направления один раз. `89CC29` dead снимает loop. Аудио-сторона умеет хранить handle (S-1942). `45CB3B:45CB71/45CB92` — позиционный запрос, не освобождённый от проверки `452726`, обязан иметь позицию. `461A83:461AAC–461B25 → 4A7C71`: предзагрузка **семпла** при Delay меньше окна, не ещё одно addAudioEvent (S-1241).
- S-1240: fire/dozer/move уже удерживаются на audio owner. Вложенные move states одного объекта не разделены. S-712: `8E3411` ставит конец fire loop на tracker; нельзя решать время пробуждения логики по возвращённому хэндлу с другого потока. S-1242: AnimationSoundClientBehavior и dock 2→3 уже есть; остаток зависит от общего selector lookup / CrushDie. S-7226 устарел: `7549A2/7549A8`, `AIAttackCharge.cpp` уже **постит** голос. S-7341: `66C0B9` отдаёт attack-move машине 21; `AIIdleState::update → resumeAttackMove` — другой путь S-328. Не добавлять голос на внутренний re-issue.

## Что перенесено и что осталось (итог 2026-11-18)

Читатель, который уже был в `main`, не переносится второй раз: для 24 имён из 39 поле помечено `used` с адресом кода (`docs/ennorath/port234-manual-check.tsv`, строка на каждое из 51 полей issue). Перенесено в этой ветке:

- **Деньги** `MoneyDepositSound` / `MoneyWithdrawSound`: `Money::deposit` / `withdraw` играют слот **до** изменения баланса и счёта, нулевая операция молчит (`Common/Player.cpp`).
- **Статусы питания** `BuildingDisabled` / `BuildingReenabled` / `VehicleDisabled` / `VehicleReenabled` / `SplatterVehiclePilotsBrain`: `Object::setDisabled` / `clearDisabled`, STRUCTURE раньше MACHINE, EMP/UNDERPOWERED только на первом бите, UNMANNED не ребро и не DRONE.
- **Крышки-награды** `CrateHeal` / `CrateShroud` / `CrateFreeUnit` / `CrateMoney`: четыре разных override (лечение — позиция сборщика, остальные — его id), `GameLogic/Module/RewardCrates.cpp`.
- **Дефектор** `DefectorTimerTickSound` / `DefectorTimerDingSound`: тик на новой стороне и звонок на чётном переходе фазы, не каждый кадр (`ObjectHelper`, `SpecialAbilityModules`).
- **Стелс** `StealthDiscoveredSound` / `StealthNeutralizedSound`: звук, пинг радара, текст и EVA одного раскрытия (`DrawableManager::applyStealthReveal`), а не опрос обнаруженных объектов.
- **GUI / HUD**: `DisabledButtonClicked`, `AllCheerSound` (LAN/internet, одно `MSG_DO_CHEER`), `TargetObjectWithSpecialPowerSound`, `SaveFileLoadMusic` (+ голос до музыки, film still).
- **Размещение**: `BuildingPlacementSound` / `WallPlacementSound` — после вызова BuildAssistant, без проверки результата; цена по RW 0x6AAA2E со знаком и с зачётом стоимости призрака у dozer'а.
- **Смерть**: Eva-объявления из `Object::friend_onDie`, без клиентского сторожа трупов (S-709 закрыт).
- **Ворота**: петля движения на объекте, снятие до конечного звука и при смерти (S-1942 закрыт).
- **Очередь звука**: отказ события без позиции, предзагрузка семпла внутри окна опережения (S-1241 закрыт).

Голос `VoiceStartCharging` (0x7EA) уже постится состоянием заряда — RW 0x7549A2 в `AIChargeTargetState::onEnter`, тот же порядок, что в доноре; S-7226 сужен до очереди поста (S-814), второго поста нет.

Осталось без читателя или без механизма (вердикты, новых стопов четыре):

| Поле | Почему нет звука | Стоп |
|---|---|---|
| `EnterCloseCombat`, `ExitCloseCombat` | ни одна инструкция RotWK не читает manager +6C / +70 (и донор тоже) | вердикт |
| `BuddyMessageNotification` | ни одна инструкция не читает manager +7C | вердикт |
| `UnitPromoted` | единственный читатель RW 0x6957B9 не вызывается и не адресуется в образе; живой путь XP идёт через LevelUpFx | вердикт |
| `RadarNotifyOnline` / `Offline` / `InfiltrationSound` | ребро `hasRadar` и запись радара `Object + 0x268` в порте отсутствуют | S-7499 |
| `FastForwardModeOn` / `Off` | только переключатель перемотки реплея (GlobalData + 0xBBD) | S-7500 |
| `GameSpyCommunicatorOpen`, `PrivateMessageNotification` | самих онлайн-экранов (оверлей GameSpy, приём личного сообщения) нет | S-7501 |
| `RepairSparks` | подмашина ремонта SlavedUpdate не перенесена | S-7502 (S-1426) |
| `UnableToSetRallyPoint` | звучит только при отказе `QuickDoesPathExist`, проверки пути у ралли в порте нет | S-202 |
| `ShellMapLoadMusic` | загрузочного экрана карты оболочки нет | S-1912 |
| `RIFThingTemplateReloadedSound`, `RIFObjectsRefreshedSound` | тип перезагрузки 5 (developer reload) не перенесён | S-632 |
| `VoiceEnterUnitTransportShip` | имя строится динамически из имени цели; объекта `TransportShip` в моде нет | вердикт |
| `VoicePrimaryWeaponMode` / `VoiceSecondaryWeaponMode` | читатель только в ветке 0x43A, производителя нет | S-7285 / S-703 |
| `VoiceSpecialAbilityCurseEnemy` | все ссылки в 1.9.8.3 закомментированы | вердикт |
| `SoundImpact`, `SoundMoveLoop` в селекторах | играются из строки шаблона, не через селектор draw-модуля | S-701 / S-707 |
