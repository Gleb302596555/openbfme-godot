# PORT-93 — `SpecialAbilityUpdate/ActiveLoopSound` (issue #241)

Задача: `ArrowStormUpdate/ActiveLoopSound` (14 использований в Эннорате 1.9.8.2, `harad/mouthsummon.ini:1084`, `GandalfLightningLoop`) разбирается, но не звучит.
Ниже — что делает RotWK 2.01 (`game.dat`, md5 `daed3668`), адреса прочитаны `tools/mod/rw.py ... dis`, и что из этого перенесено.

## 1. Поле

| что | адрес |
|---|---|
| имя `ActiveLoopSound` | строка RW `0xC56140` |
| таблица полей `SpecialAbilityUpdate` (60 строк) | RW `0xC55F40` |
| парсер (строка -> `AudioEventRTS`) | RW `0x73B217` (`NoSound` очищает, неизвестное событие — ошибка INI) |
| смещение данных | `+ 0x34` (между `TriggerSound` + 0x14 … и `LoseStealthOnTrigger`) |

Порт разбирает поле тем же порядком (`SpecialAbilityModules.cpp`, `kSpecialAbility`, `SA_OFF(m_activeLoopSound)`).

## 2. Старт лупа — хвост `SpecialAbilityUpdate::triggerAbilityEffect` (RW `0x853EDF`)

`ArrowStormUpdate::triggerAbilityEffect` RW `0x894100` первым делом вызывает базу (RW `0x894104` -> `0x853EDF`), так что луп запускает общий код способности, а не
`ArrowStormUpdate`.

| адрес | что стоит |
|---|---|
| RW `0x853F9C` | световые очки (нет менеджера в скирмише, S-531) |
| RW `0x853FD0 .. 0x853FF6` | сборка `AudioEventRTS` из `TriggerSound` (+ 0x14) для объекта: игрок RW `0x853FE4` (Object + 0x74), id RW `0x853FEA` |
| RW `0x853FF6 .. 0x854005` | `TheAudio` RW `0xDE42FC`, слот `0x64` (`addAudioEvent`) |
| RW `0x854008` | `cmp dword ptr [ebx + 0x34], 0` — поле данных пустое: к лупу не переходим |
| RW `0x85400E .. 0x85401F` | `TheAudio` RW `0xDE42FC`; `push [edi + 0x38]` (хендл модуля); слот `0xD0` (`isCurrentlyPlaying`) |
| RW `0x854021` | ответ «звучит» — луп не добавляется второй раз |
| RW `0x854023 .. 0x854033` | иначе: evento из данных + 0x34 для объекта (id RW `0x854026`, RW `0x6DB5B3`) |
| RW `0x854038 .. 0x854054` | слот `0x64` добавляет событие; результат пишется в модуль `+ 0x38` (RW `0x854054`) |
| RW `0x85405B` | удаление временного события |

Донор BFME2 1.06 (`/tmp/open-bfme-2/Code/GameEngine/Source/GameLogic/Object/Update/SpecialAbilityUpdateTriggerAbilityEffect.cpp:615-618`) содержит тот же код:
`if (moduleData->triggerLoopSound.referent && !TheAudio->isCurrentlyPlaying(loopSound)) { ... loopSound = TheAudio->addAudioEvent(&loop); }` — то есть поле
`m_activeLoopSound` (+ 0x34) донор называет `triggerLoopSound`, хендл лежит в `SpecialAbilityUpdate` (`loopSound`, `unsigned`).

## 3. Стоп лупа — `finishAbility` (RW `0x8530BA`)

| адрес | что стоит |
|---|---|
| RW `0x85311B .. 0x853121` | `push [esi + 0x38]`, слот `0x6C` (`removeAudioEvent`) |
| RW `0x853124 .. 0x853131` | `+ 0x38 = 1` (значение, которое оставляет конструктор RW `0x851D42`) |
| RW `0x853134` | вызов `endPreparation` RW `0x85260A` (там же, RW `0x85261D .. 0x852628`, снимается `PrepSoundLoop` + 0x34) |

Донор: `SpecialAbilityUpdate.cpp:1018-1021` (`onExit`: `removeAudioEvent(m_prepSoundLoop)`, `m_packSoundHandle`) — в BFME2 хендл лупа способности живёт в том же
блоке; сам `finishAbility` RW `0x8530BA` снимает только `+ 0x38`.

`removeAudioEvent` над хендлом, который уже не звучит, безопасен (RW `0x45CEE9`), а в покое `+ 0x38` равен 1.

## 4. Сохранение

RW `0x8528C8` и RW `0x8528DB`: `xfer` `SpecialAbilityUpdate` переносит оба хендла (`+ 0x34` и `+ 0x38`) через `TheAudio` (`xferAudioHandle`), версия блока RW `0x852860`
области: `(1, 3)`. Значит состояние «луп звучит» входит в снапшот, и загруженная игра слышит луп, который слышала сохранённая.

## 5. Рантайм в порту

- `AudioApi::HELD_ABILITY_LOOP` (`Common/Audio/AudioRequests.h`) — четвёртый слот удерживаемых звуков; хендл, как и у `HELD_UPGRADE_LOOP`/`HELD_BUILDING_LOOP`,
  хранит аудио-сторона, ключ — свой на модуль (`SpecialAbilityUpdate::activeLoopKey()`).
- `AudioApi::resumeHeldSound` (`AudioEntryPoints.cpp`) — ровно ветка RW `0x854008 .. 0x85405B`: пустое имя ничего не делает (это уже проверяет вызывающий), удерживаемый
  хендл ещё звучит (`isCurrentlyPlaying`, слот `0xD0` RW `0x854014`) — ничего не добавляем (RW `0x854021`), иначе событие добавляется для объекта и хендл
  запоминается (RW `0x85404B` / `0x854054`). Второй путь не добавлен: это тот же механизм удерживаемых лупов, что у `AudioLoopUpgrade`.
- `SpecialAbilityUpdate::startActiveLoopSound()` — вызывается в `triggerAbilityEffect` сразу после `TriggerSound` (порядок RW `0x853FD0` -> `0x854008`).
- `SpecialAbilityUpdate::stopActiveLoopSound()` — вызывается в `finishAbility` перед `endPreparation` (порядок RW `0x85311B` -> `0x853134`).
- `SpecialAbilityUpdate::xfer` — версия `1 -> 2`, в конце сохраняется признак «хендл удерживается»; при загрузке луп переставляется или снимается
  (`AudioLoopUpgrade::xfer` делает то же со своим + 0x2C). `crc` не тронут: звук — не состояние логики, как и в RotWK.

## 6. Тесты (PORT-93)

`engine/tests/test_port_93.cpp`, лейн `engine/cmake/Port93.cmake`:

| тест | что держит |
|---|---|
| `the data` | поле доходит до данных `ArrowStormUpdate`; у объекта без поля имя пустое |
| `the loop` | срабатывание шторма запускает луп для героя ровно один раз (RW `0x854054`), повторные срабатывания не добавляют второй звук (RW `0x854021`), `finishAbility` его снимает (RW `0x85311B`) |
| `no field, no loop` | та же способность без `ActiveLoopSound` не играет ничего (RW `0x854008`) — соседнее поведение |
| `the save` | сохранение с удержанным лупом возвращает звук после загрузки; сохранение после конца способности — нет (RW `0x8528C8`/`0x8528DB`) |

Без правки падают 2 из 4 (`the loop`, `the save`), `no field, no loop` остаётся зелёным: он держит именно отсутствие второго пути.

## 7. Стопы

S-7220 (`docs/STOPS.md`): `PrepSoundLoop` (+ 0x10, старт RW `0x853EAC .. 0x853EC8`, снятие RW `0x85261D .. 0x852628`), `PackSound` / `UnpackSound` и звук
`TriggerModelCondition` той же способности не переносились — в порте на их месте комментарии.
