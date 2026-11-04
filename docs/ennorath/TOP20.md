# TOP20: поля Эннората, которые порт не использует

Рабочий список по разделу 7а [PROCESS.md](PROCESS.md): брать сверху вниз, по одной, в своей рабочей станции (раздел 4). Источник — аудит полей (задача 1, [audit/](audit/)), исходный файл `workspace/ennorath-audit/TOP20-problems.md`.

Статус: `открыто` → `исправлено, ждёт: <узкий тест | владельца | аудит>` → `проверено` (узкий тест прошёл, владелец сказал «работает» или в игре не видно, аудит `used`) → окончательно после ночного прогона ([NIGHT_WATCH.md](NIGHT_WATCH.md), `ночь: прошло`). Можно и `стоп S-<номер>` или `RotWK тоже не использует` с адресом.

Задача 1, сессия 2026-11-04 (пятая), ветка `agent/claude/field-audit`, коммит инструмента `90ec2e88`.
Источник: `field-audit-v2/field-audit.tsv` с ручными вердиктами из `manual-check.tsv`. Порядок: по числу определений мода, использующих поле.
Для решения позже: по каждой строке нужно выбрать одно — портировать, записать стоп или признать, что RotWK тоже не использует.

## Важно: ответ разработчика мода (2026-11-04)

- `game.dat` у Эннората от патча **2.02**. По словам владельца, 2.01 и 2.02 отличаются только фиксом задержки офхоста, поэтому таблицы полей (`engine/data/rotwk-201`) и адреса RW 2.01 для Эннората верны.
- `asset.dat` общий для всех модов.
- `lotrbfmeep1.exe` когда-то пропатчен программой на 4 ГБ памяти (на логику не влияет).
- Остальные файлы игры не менялись.
- Кольцо: носители кольца, скорее всего, засчитываются как за крепость (в ванили кольцо относили в крепость, чтобы построить носителя).

## clang (решение владельца 2026-11-04)

Сейчас не делаем; сделаем, если понадобится. clang уточнил бы только, член какой структуры читается (метки AMBIGUOUS и неверный `where`), а не статус: из 466 проверенных AMBIGUOUS ошибок статуса 16 (3,4%), все исправлены вручную, неверных `where` 234. Ручные вердикты закреплены в `manual-check.tsv`, повторный прогон с `--manual` показывает конфликты. Понадобится, если после крупных правок движка аудит придётся снова перепроверять вручную. Цена: поставить libclang (`pip install libclang`), `compile_commands.json` уже есть в `engine/build` главной копии, разбор 776 файлов — оценка 15–40 минут на прогон (не замерено).

## Список

| # | поле | опр. | статус в порту | что известно | решить | статус |
|---|---|---|---|---|---|---|
| 1 | `CommandButton/ButtonBorderType` | 5858 | parsed-unused | `m_commandButtonBorder` только объявлен (`ControlBarCommands.h:164`) | рамка кнопки: видно в интерфейсе | стоп S-7162: RotWK читает (setControlCommand RW 0x71CF3E -> RW 0x71C4F4 -> RW 0x729EA7) и рисует рамку только у не-радиальных окон команд (RW 0x4A579C); окна кольца радиальные или убраны за экран (RW 0x9459D1), видна только у кнопки продажи таймера OCL (RW 0x944D9D), которой в порту нет; нужны цвета ControlBarScheme (RW 0x7206D5) и это окно |
| 2 | `Object/EvaEnemyObjectSightedEvent` | 1707 | parsed-unused | в `engine/src` ссылок нет | оповещение EVA «замечен враг» | стоп S-709 (уточнён): RotWK читает в RW 0x678C69 при выходе drawable из тумана (RW 0x678F54); перенос требует в клиенте запросов логики (видимость RW 0x694CCC, обход объектов владельца RW 0x6ABABD, ближайший свой объект RW 0x672DFB); порт строит клиент по снимкам логики |
| 3 | `Object/SoundOnDamaged`, `SoundOnReallyDamaged`, `SoundImpact` | 1292 / 1295 / 981 | parsed-unused | стоп S-707: звуки объекта не портированы | звук урона и попадания | проверено: узкий тест `SOUND-707 retail*`, владелец: работает (`7c2a8e22`), аудит `used` (`ActiveBody.cpp:422`, `PhysicsBehavior.cpp:774`); ждёт ночи |
| 4 | `Terrain/Class` | 1277 | not-read | `TerrainTypes.cpp` читает только `Texture` | класс текстуры земли (ZH: для редактора; проверить RW) | RotWK тоже не использует: разбор RW 0x73B7CB (таблица RW 0xC22A20) кладёт список в + 0x10, деструктор RW 0x71992D его только освобождает; типы земли берут только через findTerrain RW 0x71962F, а его вызовы читают одну Texture (+ 8: RW 0x4AF348 через RW 0x44F0DF / 0x4AEC01; getTerrainTile RW 0x491F1A — слот 0x1C TheTerrainVisual, вызовов нет); у vtable RW 0xC22CE4 только деструктор |
| 5 | `Object/ShadowTexture`, `ShadowSizeX`, `ShadowSizeY` | 1138 | parsed-unused | стопы S-3304, S-5032: тени объекта разбираются и отбрасываются | тени | стоп S-3304 (уточнён): объектные Shadow* (шаблон + 0x90, + 0x4F8 / + 0x4FC) читает RW 0x4780DC, 0x47A0AD, 0x4B931E; это декальные тени, нужен W3DProjectedShadow |
| 6 | `AIUpdateInterface/AILuaEventsList` | 985 | raw | данные модуля не типизированы; стопы S-127, S-147 | Lua-события ИИ | стопы S-127, S-147: порт уже читает поле (MapCreationHooks.cpp:335 по сырым строкам модуля, LiveScripting.cpp:198 привязывает EventList); аудит видит `raw`, потому что данные модуля ИИ не типизированы |
| 7 | `Object/ThingClass` | 985 | parsed-unused | RotWK читает: RW `0x912BDA` пишет имя класса (UNSPECIFIED, HORDE_UNIT, CHARACTER_UNIT ...) в запись рядом с именем шаблона; потребитель записи не прослежен | найти, куда идёт запись (RW `0x912BC8`) | открыто |
| 8 | `W3DScriptedModelDraw/UseStandardModelNames` | 985 | parsed-unused | только объявлено (`W3DModelDraw.h:289`) | имена моделей | открыто |
| 9 | `UnitSpecificSounds/VoiceGarrison` | 918 | parsed-unused | ни один читатель порта не просит это имя; RotWK просит (RW `0x8DF698`) | голос при входе в здание | исправлено, ждёт: владельца (первая проверка 2026-11-04: тишина, причина — цель голоса брала первый аргумент MSG_ENTER, исправлено `e0171761`) — вид входа `ActionManager::getEnterMessage` (RW 0x82CEFB: ENTER / CONTEST при чужом занятом контейнере и CAN_ENTER_ANYTHING / DELIVER_RING в COMMANDCENTER с HOLDING_THE_RING / SLAUGHTER по слоту контейнера 0x30); голос MSG_ENTER после VoiceEnterUnit<цель> (RW 0x8DF50D .. 0x8DF716): ENTER — VoiceGetHealed (HEAL_PAD) / VoiceGarrison (STRUCTURE) / VoiceEnter, VoiceEnterHostile, VoiceSendToSlaughterhouse / VoiceDeliverRing; узкий тест `TOP20-9 enter voice*`; аудит `used` (`UnitVoiceResponse.cpp:960`, ручной вердикт обновлён); S-704 уточнён |
| 10 | `Object/VoiceMoveToCamp`, `VoiceRetreatToCastle` (+ `VoiceEnterStateMoveToCamp`, `VoiceEnterStateRetreatToCastle`) | 857 / 774 (354 / 331) | parsed-unused | стоп S-704: правила голосов лагеря и крепости не портированы | голоса | открыто |
| 11 | `UnitSpecificSounds/VoiceInitiateCaptureBuilding` | 813 | parsed-unused | его имя называет `SpecialPower/UnitSpecificSoundToUseAsInitiateIntendToDoVoice`, а это поле тоже не читается (`SpecialPower.cpp:73`) | голос захвата здания | открыто |
| 12 | `SpecialAbilityUpdate/DoCaptureFX` | 781 | parsed-unused | разобрано (`SpecialAbilityModules.cpp:200`), чтения нет | эффект захвата | открыто |
| 13 | `Object/Browser` | 704 | parsed-unused | по RW не определено: смещение `0x3B8` встречается в коде 75 раз | проверить, читает ли RotWK (в ZH поле для редактора, INFERENCE) | открыто |
| 14 | `ExperienceLevel/LevelUpTint*` (4 поля) | 652 | parsed-unused | `LevelUpTintColor`, `LevelUpTintPostColorTime` только объявлены; два других разобраны, не читаются | подсветка при повышении уровня | открыто |
| 15 | `Upgrade/DisplayName` | 643 | parsed-unused | `m_displayName` не читается; стопы S-072, S-145, S-178 | имя апгрейда в интерфейсе | открыто |
| 16 | `Object/DescriptionStrategic`, `DisplayNameStrategic` | 616 / 363 | parsed-unused | по RW не определено: смещения `0x3C` и `0x54` слишком частые | тексты Войны Кольца | открыто |
| 17 | `HordeContain/ShowPips` | 598 | parsed-unused | только объявлено и конструктор (`OpenContain.h:71`) | значки отряда | открыто |
| 18 | `SlowDeathBehavior/FadeTime` | 596 | parsed-unused | разобрано (`CombatModules.cpp:142`), чтения нет | исчезновение трупа | проверено: узкий тест `TOP20-18 slow death*`, владелец: работает (Gondor Worker), аудит `used` (`CombatModules.cpp:628`); ждёт ночи — RW 0x860D35 .. 0x860D5E: при наступлении FadeDelay тело гаснет (fadeOut RW 0x670A50) за FadeTime * 6 кадров клиента |
| 19 | `Object/EmotionRange` | 547 | parsed-unused | чтения нет | эмоции юнитов | открыто |
| 20 | `ActiveBody/CheerRadius` (+ `RespawnBody/CheerRadius` 246) | 505 | parsed-unused | разобрано (`ActiveBody.cpp:85`), чтения нет | радиус ликования | проверено: владелец: работает (2026-11-04, 4 проверки), узкий тест, аудит `used`; ждёт ночи — RW 0x8C3B89 (слот 0x58 тела): враги в CheerRadius (ENEMIES, живые) получают запрос EMOTION_CHEER на 10 кадров (RW 0x68F37F); вызывают смерть от источника вне полного контейнера (RW 0x8C4679 .. 0x8C46A6) и удаление живого (onDelete RW 0x8C1E18), кроме PROJECTILE / INERT; узкий тест `TOP20-20 cheer*`, аудит `used` (`ActiveBody.cpp:632`); эталоны хэша `perf1` пересняты (сдвиг с кадров 103 / 723) |

Просмотрено 2026-11-04 (станция claude/station-2), не взято как большое — по правилу 2а раздела 7а [PROCESS.md](PROCESS.md) так больше не делать, эти пункты брать по порядку; адреса для того, кто возьмётся:

- 7 `ThingClass`: потребитель записи RW 0x912BBC вызывается из RW 0x912F49, дальше не прослежен.
- 8 `UseStandardModelNames`: читается в поиске LOD-модели для орд (RW 0x4C27D4, рядом сообщения о `StaticModelLODMode`), это отрисовка.
- 12 `DoCaptureFX`: RW 0x852B14 .. 0x852C88 — вспышка drawable цели цветом игрока (RW 0x678FA7), поиск соседнего объекта (RW 0xA39090) и сообщение интерфейса; поле + 0x70 — состояние логики.
- 14 `LevelUpTint*`: в порту нет оттенков drawable (TintEnvelope).
- 17 `ShowPips`: порт не рисует значки контейнера.
- 19 `EmotionRange`: объект копирует его в + 0x1BC (RW 0x699DFD), читатель не найден; порт берёт радиус обзора (EmotionModules.cpp:667).
- `ActiveBody/BurningDeathFX`: часть механики сгорания в attemptDamage (RW 0x8C417D ..), которой в порту нет.

Сразу за двадцаткой: `CampnessValue` 446 (S-560), `HordeAIUpdate` (raw, 404: `AutoAcquireEnemiesWhenIdle`, `MinCowerTime`, `MaxCowerTime`, `MoodAttackCheckRate`, `AttackPriority`), `OCL CreateObject/FadeIn` 462 и `FadeTime` 412 (исправлено вместе с `FadeOut` и `FadeSound`, проверено: узкий тест `TOP20-OCL fade*`, владелец: работает (Армия мёртвых, Око Саурона), аудит `used` (ручной вердикт обновлён); ждёт ночи — RW 0x5F1678 .. 0x5F17A6), `ActiveBody/BurningDeathFX` 448, `MeshNameMatches` (stub-block, 380), `Weapon/HitPassengerPercentage` 320 (S-321).

## Убрано из главных

- `Object/EditorSorting` (5599): RotWK читает его только в одном фильтре шаблонов (RW `0x62C85C`: `EditorSorting != EMITTERS`, рядом проверки имён `CINE`, `GLAInfantryAngryMobNexus`, `FlockOfBirds`). INFERENCE: служебный код, на игру не влияет.
- `Mouse/TooltipFontName`, `TooltipFontSize`, `TooltipFontIsBold`: RotWK их тоже не применяет — шрифт берётся из `Language.ini` (`lang/English.big`: `"Albertus MT" 12 No`), RW `0x5EE8C8`.
- 11 полей `fxparticlesystemcustom.ini`: RotWK этот файл не загружает (`InitFile` закомментирован в `default/subsystemlegend.ini:70`).

## Найдено попутно (не в двадцатке, но решить)

- Блок `Fire` (`terrain-fire`, `8d487f95`): `EnableScorches`, `ScorchFrequency`, `ScorchSize`, `ScorchIntensity`, `BurntTerrainColor`, `FuelIndicatorColor` разбираются в `FireSetting.h`, но не применяются (обугленная земля, подпалины), а стоп S-191 закрыт целиком без нового стопа. Нужен стоп или перенос по RW.

- `FireLogicNugget`: `DelayTime` (20), `DamageScalar` (1) — RotWK тоже не использует (проверено 2026-11-04): таблица нагета (RW 0xC7BF18, 4 своих поля) продолжает таблицу DamageNugget, поэтому поля разбираются в + 0x174 / + 0x188; читает их только fillDamageInfo (RW 0x90E28C: + 0x188 в RW 0x90E3F1 .. 0x90E433, + 0x174 в RW 0x90E4F8), а её зовут слоты DamageNugget 1, 5, 6, 14 (RW 0x90E855, 0x90DAFD, 0x90DEF0 через слот 14 RW 0x90E683) и MetaImpact (RW 0x9107C8); у FireLogicNugget (vtable RW 0xC7BF68) слоты 1, 5, 6 свои (RW 0x8F8014, 0x912087, 0x9120E6) и читают только Damage / Radius / DamageArc (+ 0x148 / + 0x150 / + 0x158) и свои поля; RW 0x912241 — конструктор нагета, не доставка.
- Блок `Fire`, подпалины: RotWK применяет — клиентская система RW 0x49829C (копия записи RW 0x70C5C3) при EnableScorches и детализации > 1 раз в ScorchFrequency кадров обходит горящие клетки TheFireLogic (+ 0x84) и с вероятностью ScorchIntensity кладёт подпалину ScorchSize (тип 7 или 8) на землю; FuelIndicatorColor отдаёт геттер RW 0x497F1D. В порту подпалины не рисуются совсем (S-117, S-033): перенос начинается с декалей подпалин террейна.
- Микрофон Living World (`LivingWorldMicrophone*`, `LivingWorldZoomMaxDistance`): вид Living World в звуке никто не включает (`set_current_view` без вызова в `godot/`).
- 270 полей GameData / AIData / MultiplayerSettings уходят в строку-ловушку (S-152): загружаются, не применяются.
- Ошибка мода: `cinematicobjects.ini:23597` — `End  StateName = Sword` (потерян перенос строки), `StateName` теряется и в RotWK.
