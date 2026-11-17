# PORT-247 — Ability parameters: FlingPassengerVelocity, Teleport MaxDistance, AutoAbilityBehavior WorkingRadius

Разбор по issue #571 (задача 207). Образ: `workspace/RotWK/game.dat`, RotWK 2.01, md5 `daed3668…`.
Все адреса прочитаны `python tools/mod/rw.py workspace/RotWK/game.dat dis <addr>`;
донор BFME2 1.06 — клон `Open-BFME-2` (`DONOR`), истина — RotWK.

Итог по пунктам:

| поле | что в RotWK | вывод |
|---|---|---|
| `TeleportSpecialAbilityUpdate/MaxDistance` (+0xDC) | читает переопределение слота 8 интерфейса специальной способности, RW 0x896608 | реальный пробел: проверка «дальность телепорта» не перенесена |
| `FlingPassengerSpecialAbilityUpdate/FlingPassengerVelocity` (+0xD0) | читателя нет (23 слота vtable и все функции класса прочитаны; донор тоже не читает) | вердикт «RotWK не использует» |
| `AutoAbilityBehavior/WorkingRadius` (+0x10) | читателя нет (все функции класса прочитаны; донор тоже не читает) | вердикт «RotWK не использует» |

## 1. TeleportSpecialAbilityUpdate MaxDistance — читатель найден

Таблица полей класса RW 0xC054B0 (4 строки, сентинел 0xC054F0):

| запись | поле | парсер | смещение |
|---|---|---|---|
| RW 0xC054B0 | `BusyForDuration` | RW 0x73A429 | +0xD0 |
| RW 0xC054C0 | `DestinationWeaponName` | RW 0x42EE5E | +0xD4 |
| RW 0xC054D0 | `SourceWeaponName` | RW 0x42EE5E | +0xD8 |
| RW 0xC054E0 | `MaxDistance` | RW 0x42ED00 | +0xDC |

Читатель — **RW 0x896608**, слот 8 (смещение 0x20) интерфейсной таблицы TeleportSpecialAbilityUpdate
**RW 0xC6490C**; тот же слот базовой таблицы RW 0xC64658 — RW 0x851B6E (базовая
`allowsSpecialPowerUse`, которую порт уже держит). Привязка класса: таблица RW 0xC6490C лежит в одном
блоке с vtable RW 0xC64930 (слот 0x00 → RW 0x896484), её слоты 0/2/6/7 — базовые RW 0x852E8F / 0x8BD372 /
0x851C92 / 0x851E83, слот 8 — единственный переопределённый, RW 0x896608.

RW 0x896608 целиком (аргумент — `const Coord3D *`, может быть нулём):

```
00896608  mov edx,[esp+4]          ; loc
0089660C  test edx,edx / je 0x896682                        ; нет места -> базовое поведение
00896611  mov esi,[ecx-0x1c]       ; moduleData (интерфейс this = модуль + 0x20)
00896614  test esi,esi / je 0x896682
00896618  movss xmm0,[esi+0xdc]    ; MaxDistance
00896620  comiss xmm0,[0xc1b594]   ; 0.0f: MaxDistance <= 0 -> базовое поведение
00896629  mov eax,[ecx-0x18]       ; объект; ноль -> базовое поведение
00896630..0089664D  xmm2/xmm1/xmm0 = loc - позиция объекта (+0x38/+0x3C/+0x40)
00896655..00896675  xmm4 = |loc - pos|^2 (три mulss + два addss), xmm0 = MaxDistance^2
00896679  comiss xmm4,xmm0 / jbe 0x896682                    ; в пределах -> базовое поведение
0089667E  xor al,al                 ; дальше MaxDistance -> ЛОЖЬ
00896682  push edx / call 0x851b6e  ; иначе базовая allowsSpecialPowerUse(loc) (RW 0x851B6E)
```

Базовая RW 0x851B6E: `conditionsAllow` (RW 0x851551), затем для способностей гарнизонных типов
0x27/0x28 — место в contain (уже перенесено, `SpecialAbilityUpdate::allowsSpecialPowerUse`).

Вызов слота 8 с местом: **RW 0x897E82** внутри `SpecialPowerModuleInterface` RW 0x897E01 (та же
функция, которую порт держит как `moduleAllows(...)` в `ActionManager.cpp`): `00897E14 mov ebp,[esp+0x10]`
— первый аргумент функции (указатель на место, может быть нулём: `00897E18 test ebp,ebp`), `00897E4C`
цикл по модулям объекта (`[object+0x24C]`, слот 0x1C с шаблоном силы из данных +8), `00897E7F push ebp;
00897E82 call [eax+0x20]`. То есть в RotWK у метода есть параметр места, и TeleportSpecialAbilityUpdate
режет им дальность.

Других читателей +0xDC нет: второй кандидат аудита RW 0x8C6E82 — не этот класс (читает `[data+0xD0]`
как int, делит на счётчик, кладёт `[data+0xDC]` в структуру и обходит contain слотом 0x110; стоит в
блоке `LevelGrantSpecialPower`, имя класса — строка, которую пушит RW 0x8C6DD2).

## 2. FlingPassengerSpecialAbilityUpdate FlingPassengerVelocity — вердикт: RotWK не читает

Таблица полей RW 0xC0556C (2 строки, сентинел 0xC0558C): `FlingPassengerVelocity` (строка RW 0xC05554,
парсер RW 0x42F247) → +0xD0, `FlingPassengerLandingWarhead` (строка RW 0xC05534, парсер RW 0x73AE79) → +0xDC.
`buildFieldParse` класса — RW 0x64DB10 (`push 0xC0556C` на 0x64DB06).

Класс привязан цепочкой: строка имени RW 0xC0B37C (`FlingPassengerSpecialAbilityUpdate`) ← геттер
RW 0x8986A3 (и его статическая ветка RW 0x8986C9), ctor RW 0x898672 (vtables +0 → RW 0xC652D8,
+0xC → RW 0xC56310, +0x10 → RW 0xC769EC, +0x20 → RW 0xC64658), ctor данных RW 0x89870E (обнуляет
три float по +0xD0/+0xD4/+0xD8 и слово +0xDC), деструктор RW 0x89873E, триггер RW 0x89875A.

Прочитаны все 23 слота vtable RW 0xC652D8:
0 RW 0x89873E (деструктор), 1 RW 0x8E38D8, 2 RW 0x8986A3 (геттер имени), 3 RW 0x8C6E67 (общая
«получить данные модуля»), 4 RW 0x8986C9, 5-8 RW 0x63F3BF (общий no-op), 9 RW 0x8851E4, 10/11 RW 0x851E97,
12 RW 0x64DFDA, 13 RW 0x8530BA, 14 RW 0x85393B, 15 RW 0x853BEC, 16 RW 0x852A3E (базовые
finishAbility / approachTarget / startPreparation / continuePreparation), 17 RW 0x89875A (триггер),
18 RW 0x851E9A, 19 RW 0x8549E7, 20 RW 0x851E9F, 21 RW 0x853468, 22 RW 0x8536EA (базовые
onEffectExpired / finishAfterPacking / handlePackingProcessing / startPacking / startUnpacking).
Дальше — сентинел 0xC7C34FF3.

Триггер RW 0x89875A прочитан целиком: базовый триггер (RW 0x853EDF), contain объекта (+0x258),
слот счёта 0x114, слот пассажира 0x13C, `[data+0xDC]` (LandingWarhead), временное оружие RW 0x6CF590,
далее либо две модельные кондиции мёртвого пассажира (RW 0x68B53C через биты +0x110 / +0x118),
либо `PhysicsBehavior::standUp` физики пассажира (+0x264 → RW 0x792AFF). **`[data+0xD0]` не читается
ни одной инструкцией**: ни в триггере, ни в конструкторах, ни в одном из 23 слотов, ни в общей
«получить данные модуля» RW 0x8C6E67.

Донор BFME2 1.06 (DONOR): `Code/GameEngine/Source/GameLogic/Object/Update/FlingPassengerSpecialAbilityUpdate.cpp`
— триггер (retail 0x494FCC) читает только данные +0xD4 (оружие), конструктор данных (0x494F80)
обнуляет три float +0xC8/+0xCC/+0xD0, `buildFieldParse` (0x24DE1C) кладёт `FlingPassengerVelocity`
в +0xC8. Других файлов класса в доноре нет (`git grep FlingPassenger -- Code`), читателя нет и там.

Значит поле разбирается и не используется ни в одной редакции: код не добавляю, строка `manual-check`
для аудита — в PR.

## 3. AutoAbilityBehavior WorkingRadius — вердикт: RotWK не читает

Таблица полей RW 0xC57EC0 (её ставит `buildFieldParse` RW 0x85D3DC): `MaxScanRange` +0x08,
`MinScanRange` +0x0C, `WorkingRadius` +0x10 (парсер RW 0x42ED00), `IdleTimeSeconds` +0x14;
конструктор данных RW 0x85D355 обнуляет ровно +8 / +0xC / +0x10 / +0x14 (vtable данных RW 0xC57D98).

Класс: ctor RW 0x85D3ED (vtables +0 → RW 0xC57F8C, +0xC → RW 0xC67300, +0x10 → RW 0xC57F80),
`readFrom` RW 0x85D2FF, геттер строки типа RW 0x85D436, геттер имени RW 0x85D43C, деструктор RW 0x85D5CE,
`set` RW 0x85D48F, выбор цели RW 0x85D4B5, update RW 0x85D9D1, «может действовать» RW 0x85D5EA.

Прочитаны все слоты vtables: поведение RW 0xC57F8C (0 RW 0x85D5CE, 1 RW 0x8B88B0, 2 RW 0x85D436,
3 RW 0x85D2FF, 4 RW 0x85D43C, 5-8 RW 0x63F3BF, 9 RW 0x8851E4, 10/11 RW 0x851E97, 12 RW 0x64DFDA),
update RW 0xC57F80 (0 RW 0x85D9D1, 1 RW 0x855830, 2 RW 0x850C4E, 3 RW 0x85D5CE, 4 RW 0x8B88B0,
5 RW 0x85D436, 6 RW 0x85D2FF, 7 RW 0x85D43C, 8-11 RW 0x63F3BF, 12 RW 0x8851E4, 13/14 RW 0x851E97,
15 RW 0x64DFDA), третий интерфейс RW 0xC67300 (все слоты RW 0x851E97, кроме 9 → RW 0x863BF3 —
общий «получить данные модуля»). Ни один из них не читает данные +0x10.

Update RW 0x85D9D1 берёт данные как `[this-0xc]` и читает `[data+0x14]` (IdleTimeSeconds, RW 0x85DA8E →
RW 0x85DA99/DACA). Регион класса RW 0x85D2FF .. 0x85E100 прочитан целиком: чтений `[data+0x10]` нет.
У полей +0x08 / +0x0C (MaxScanRange / MinScanRange) читатель тоже не найден — в порте они уже
участвуют в скане; разбор этих двух полей — не эта задача.

Донор BFME2 1.06 (DONOR): `WorkingRadius` встречается только в конструкторе данных
(`AutoAbilityBehaviorModuleDataCtor.cpp:6` — комментарий к полям) и в таблице разбора
(`ModuleDataBuildFieldParse.cpp:537`, смещение 0x10). В коде update/поведения чтения нет
(`git grep WorkingRadius -- Code`).

Значит поле разбирается и не используется ни в одной редакции: код не добавляю, строка `manual-check`
для аудита — в PR.

## Что делаем в порте

1. `SpecialPowerUpdateInterface::allowsSpecialPowerUse` получает параметр места (`const Coord3D *`,
   по умолчанию ноль — так базовая RW 0x851B6E и вызывается с нулём из объектной формы RW 0x82DAAB).
2. `TeleportSpecialAbilityUpdate` переопределяет его по RW 0x896608 (порядок проверок и порог 0:
   сначала ноль-место, потом данные, потом объект, потом MaxDistance > 0, потом |loc - pos|² > MaxDistance²).
3. `ActionManager` передаёт место (RW 0x897E82).
4. Тесты `PORT-247 …`: телепорт за MaxDistance отвергается, в пределах — разрешается, без поля
   (MaxDistance 0) — разрешается всегда; соседнее поведение (базовый `allowsSpecialPowerUse`,
   условия силы) держится отдельным тестом.

## Почему правку пока не видно в игре (S-1874)

Все телепорты Эннората (`SpecialAbilityWildShelobTunnel`, `SpecialAbilityKarshBlink`, `SpecialAbilitySauronBlink`,
`SpecialAbilityHeroBlink`, `SpecialAbilityColdDrakeReposition`, `SpecialAbilitySummonedDragonReposition`,
`SpecialAbilityWyrmReposition`, `SpecialAbilityHaradSpyShadowstep`, `SpecialAbilityMagiebndlung`) объявлены как
`Enum = SPECIAL_BALROG_WINGS`. Это тип **0x41**: список имён RW 0xDA5C90, строка RW 0xC0… (имя на 65-м месте).
`ActionManager::canDoSpecialPowerAtLocation` доходит до таблицы концовок `finalTestOf` только после собственного
теста типа: `case 0x41` (RW 0x82E252) в порте отвечает через `unportedAnswer`, то есть **нет**, — это открытый стоп
S-1874. Поэтому порт сегодня отказывает любому телепорту в точку и до, и после этой правки, по чужой причине;
строки `finalTestOf` относят 0x41 к FINAL_CLEAR (RW 0x82E91A), но до неё дело не доходит.

Тесты PORT-247 держат именно мою правку: путь `ActionManager::canDoSpecialPowerAtLocation` → `moduleAllows`
(RW 0x897E01) → слот 0x20 идёт с типом 0x34 (`SPECIAL_ROUSING_SPEECH`, строка FINAL_YES RW 0x82ED8E), который порт
отвечает «да», поэтому решает модуль; и отдельно — прямой вызов метода интерфейса с тем же
`TeleportSpecialAbilityUpdate`, что у мода. После переноса S-1874 проверка владельца (Шелоб / Карш) заработает без
правок в этой ветке.
