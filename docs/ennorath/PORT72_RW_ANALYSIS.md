# Port 72: FactionVictoryData — анализ RotWK 2.01

Рабочая запись по issue #186. Адреса прочитаны через `tools/mod/rw.py dis|xref` из RotWK 2.01 (`game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`, оговорка S-001) и сверены с декомпилом BFME2 1.06 (`Open-BFME-2`, `INIVictorySystem.cpp`, `VictorySystem.h/.cpp`). Выдержки INI мода и его файлы здесь не приводятся.

**Вердикт (остановка, issue #186):** RotWK 2.01 читает факторы `FactionVictoryData` только внутри `TheVictorySystem` — подсистемы «большой битвы» LOTR RTS (клеточная сетка победы, боевой опыт-бонус участникам). В порте этой подсистемы нет в принципе (см. §6), а существующий путь победы порта — `VictoryConditions` (элиминация) — ничего общего с `TheVictorySystem` не имеет и факторов не читает. Значит переносить нечего: «блок + читатель на существующий путь победы» означал бы писать новую подсистему, а не портировать читателя. Остановка по правилу issue («RotWK 2.01 reads it only in a mode the port does not run») с адресами; S-7197 в `docs/STOPS.md`; держат синтетические тесты `PORT-72*` (`engine/tests/test_port_72.cpp`, 3 теста) + этот документ.

## 1. Блок, его таблица разбора и глобал

- Запись блока RW `0xD9EC68`: `{link = 0x00000000, name = 0x00BFBB3C "FactionVictoryData", func = 0x00615A68, a = 0x00BD1584, b = 0x00BD157C, c = 0x00BD156C, zero = 0}`.
- Парсер блока RW `0x00615A68`: читает токен имени (RW `0x42DC9F`), строит `AsciiString` (RW `0x4050E6`), сравнивает с `neutral` (RW `0x4372C0`, строка RW `0x00BFBB20`) и **очищает имя при совпадении** (RW `0x435D50`, RW `0x615A94` .. RW `0x615AA8`) — служебный способ сказать «neutral записи не создаёт». Затем проверяет глобал `TheVictorySystem` RW `0x00DE897C`: `0` → заглушка-«dev guard»: печатает `TheVictorySystem has not been initialized!.\n` (строка RW `0x00BFBA94`) через `[0xDC62C0]+0x60` (RW `0x615AAD` .. RW `0x615AE8`); иначе find-or-create записи `FactionVictoryParameters` (продолжение от RW `0x615AEF`; в find-or-insert участвует RW `0x8072B5` — вектор +0x120 / счётчик +0x124, рост RW `0x8071EC`; сообщение `Could not find/create FactionVictoryParameters` — строка RW `0x00BFBAC8`).
- Таблица разбора блока RW `0x00C4EBF8` (пять строк, все парсер RW `0x42ED00` — `parseReal`: `scanReal` → `fstp dword [store + offset]`, без проверок диапазона):

| строка RW | имя (строка RW) | парсер | смещение записи |
|---|---|---|---|
| `0xC4EBF8` | `AllyDeathScaleFactor` (`0xC4ED38`) | `0x42ED00` | `+0x04` |
| `0xC4EC08` | `EnemyKillScaleFactor` (`0xC4ED20`) | `0x42ED00` | `+0x08` |
| `0xC4EC18` | `VictoryThreshold` (`0xC4ED0C`) | `0x42ED00` | `+0x10` |
| `0xC4EC28` | `MapToCellVictoryRatio` (`0xC4ECF4`) | `0x42ED00` | `+0x0C` |
| `0xC4EC38` | `MajorUnitValue` (`0xC4ECE4`) | `0x42ED00` | `+0x14` |

- Запись — 24 байта: `{AsciiString text; f0 +0x04; f1 +0x08; f2 +0x0C; f3 +0x10; f4 +0x14}`, т.е. `f0 = AllyDeathScaleFactor`, `f1 = EnemyKillScaleFactor`, `f2 = MapToCellVictoryRatio`, `f3 = VictoryThreshold`, `f4 = MajorUnitValue`. Хранятся вектором по `TheVictorySystem+0x120` (база `+0x120`, счётчик `+0x124`).
- `TheVictorySystem` — глобал RW `0x00DE897C`; регистрация при инициализации подсистемы: RW `0x63C301`..RW `0x63C306` (`push 0xDE897C; call 0x638418`) в блоке инициализации подсистем (RW `0x63C180` ..), рядом строка `TheVictorySystem` (RW `0x00BFEBF8`, `docs`: `SubsystemInitOrder`).

## 2. Раскладка TheVictorySystem (смещения, подтверждены байтами)

- `+0x00` vtable; `+0x28` `field28[20]` — индекс записи фактора на слот игрока, бит 31 = нейтрал/пропуск; `+0x78` `kills[20]` и `deaths[20]` (kills по `+0x78`, deaths по `+0x78+0x50`); `+0x120` вектор записей (§1); `+0x12C`, `+0x130` — два `CellGrid*` («арены»); `+0x134` байт «инициализирован» (конец `newMap`); `+0x138` — активная сетка (0/1); `+0x13C` — текущий игрок.
- Размер элемента записей — 24 байта (`imul ..., 0x18` в геттере RW `0x8068D8` .. RW `0x8068DC`).

## 3. Все обращения к глобалу TheVictorySystem (полный перебор .text)

`xref 0xDE897C` даёт ровно 10 сайтов в 8 функциях; все декодированы и принадлежат подсистеме:

| сайт RW | функция | что это |
|---|---|---|
| `0x6159DC`, `0x615AAD` | RW `0x61584E` (парсеры блоков) | null-check + dev guard (§1), обе ветки (`VictorySystemData`/`FactionVictoryData`) |
| `0x63C301` | RW `0x63C180`-область | регистрация глобала подсистемы при инициализации |
| `0x62E6D3` | функция от RW `0x62E630` (…walks players, `[0xDE412C]` = TheGameLogic, `[0xDE4148]`) | вызов слота vtable `+0x28` на TheVictorySystem (`call [eax+0x28]`) |
| `0x631395` | RW `0x631186` (вызывается из RW `0x6312B9`) | вызов слота vtable `+0x38` на TheVictorySystem (`call [eax+0x38]`) |
| `0x6DFA8E` | RW `0x6DF73E` | писатель снимка подсистем (`add eax, 0xC` / `neg`-`sbb`-`and` = `vector::begin()` над списком): `CHUNK_VictorySystem` — тот самый адрес, который упоминает порт в `GameLogicSnapshot4d.cpp` |
| `0x698FC4` | RW `0x698EC3` | путь начисления боевого опыта (`[ESI+0x25C]`, ~130 вызывающих): `test ecx,ecx; je skip; push [ebp+8]; push esi; call 0x806D18` — просит у TheVictorySystem размер/выдачу бонуса |
| `0x806935`, `0x80695F` | RW `0x806909` | сборщик 20-битной маски победы (§4) |
| `0x973CCB` | RW `0x973C81` | сканирование ячеек (§4) |

Ни один сайт не стоит в `VictoryConditions` (элиминация, RW `0x808D3C`), в скриптах или в Wotr-авторазрешении.

## 4. Читатели факторов (внутри подсистемы)

- Геттер записи RW `0x8068CF`: `idx = field28[i] & 0x7FFFFFFF`; границы `(end-begin)/0x18`; возврат `vecBegin + idx*0x18` (RW `0x8068D8` .. RW `0x8068E8`). Вызыватели: RW `0x80693E` / `0x806966` (сборщик маски), RW `0x806CEE`, RW `0x973CD2` (сканирование ячеек).
- Сборщик маски RW `0x806909` (тело от RW `0x806926`): для слотов 0..19: слот с бит 31 в `field28` пропускается (нейтрал); иначе `rec = &vec[idx]`; ветка `flag == 0`: `xmm0 = kills[i] = [edi+i*4]`, `xmm1 = deaths[i] = [edi+i*4+0x50]`; `xmm0 *= rec[+0x08]` (EnemyKillScaleFactor), `xmm1 *= rec[+0x04]` (AllyDeathScaleFactor); `xmm0 -= xmm1`; `xmm0 > rec[+0x10]` (VictoryThreshold) → бит `i`; ветка `flag != 0`: предикат RW `0x806539(rec)`. Возврат 20-битной маски (RW `0x806935` / RW `0x80695F` окружение).
- `newMap` RW `0x806A80` .. RW `0x806CB4`: строит/клампит две «арены» (`CellGrid`, `+0x12C`/`+0x130`; размеры связаны с константами RW `0x00BD1908` = 1.0f, `sqrt` через RW `0xA3CF96`); цикл по `[this+0x14]` игрокам: нейтральное правило (пустое имя / Observer / Civilian → индекс `0x80000000`-бит), иначе запись фактора ищется через RW `0x8069A1` (промах → индекс 0, записи Default); конец — `mov byte [this+0x134], 1` (инициализация).
- Статистика kill/death в сетку RW `0x806CC5`: активная сетка выбирается байтом `[this+0x138]` из `+0x12C`/`+0x130`; игрок — через геттер (RW `0x8068CF`, `[this+0x13C]`); событие подаётся в RW `0x973EFE(grid, player)`. Единственный вызывающий — обёртка RW `0x806F6B` (единственный ссылающийся RW `0x806F5F`; гейт `[this+0x134] == 0 → return`).
- Маска → выдача бонуса RW `0x806EA2`: `0x806926(this+0x78, 0)`; ноль → возврат; argmax по двум аренам (метрика арены; ничья/нет → `0x7FFFFFFF`); по ненулевым u32-записям арены → RW `0x973C04(&out, edi)`; по каждому установленному биту → RW `0x806E06(this, i, &out)` (гейт `flags[i] & 0x80`, выдача через RW `0x6A844E` / RW `0xA39340`).
- Размер бонусной выдачи RW `0x806D19` (вход RW `0x806D18`): аргументы `(объект /*ebx*/, grant /*eax*/)`; пропуск при `[объект+0x458] & 0x08`; владелец через метод объекта RW `0x68B678` (индекс игрока в `+0x54`); `field28[idx]` (бит 31 → выход); если флаги игрока `[..+0x109] & 4` или `[..+0x113] & 4` → MajorUnitValue подменяется константой RW `0x00BD1908` = **1.0f** (RW `0x806DA8`), иначе `xmm0 = rec[+0x14]` (MajorUnitValue, RW `0x806DB2`); далее `fld` и вызов RW `0x80639D(this+0x78, &value)` (постановка бонуса в очередь) и циклы по аренам `+0x12C`/`+0x130` с RW `0x973FE7(arena, …)` на каждую. Вызывающий — RW `0x698FC4` (§3, путь боевого опыта).
- Сканирование ячеек RW `0x973C81`: TheVictorySystem → геттер записи (RW `0x973CD2` → RW `0x973CCD`/`0x973CCC`) → предикат RW `0x806539(rec)`; далее ячейковая ветка формулы (`VictoryThreshold * MapToCellVictoryRatio`, операнды `[this+0x120]+…`, `esi+0x14`/`esi+0x18`). Внутри `0x973C59`-диапазона; полностью не дочитана — для вердикта достаточно класса.

Итого: **каждый** читатель `AllyDeathScaleFactor` / `EnemyKillScaleFactor` / `VictoryThreshold` / `MapToCellVictoryRatio` / `MajorUnitValue` лежит внутри `TheVictorySystem` (таблица разбора → карта `field28` → маска/ячейки/бонус). Формулы описаны самим модом в `default/victorysystem.ini`: `kills * EnemyKillScaleFactor - deaths * AllyDeathScaleFactor > VictoryThreshold` по карте, затем `> VictoryThreshold * MapToCellVictoryRatio` по ячейке, затем бонус единицам в ячейке.

## 5. Что говорит декомпил BFME2 1.06 (совпадение формы, не доказательство)

`Open-BFME-2`: `VictorySystem.cpp` — класс с `m_28[20]` (playerParameterIndex), `m_78` (kills/deaths), `m_120` (вектор записей 24-байтных), `m_12C[2]` (`CellGrid*`), `m_134` initialized, `m_138` activeGrid, `m_13C` currentPlayer; `newMap` (нейтральное правило empty/Observer/Civilian → `0x80000000`), default-запись `{f4=MajorUnitValue=10, f0..f2≈1.0f (`g_Va00BBB8D8`, значение в RW не проверено), f3=VictoryThreshold=500}`; `update()` → `rva00405094` (grid victory), `rva00405258` (cell scan), `rva00404A29` (bonus grant). Это клетка/битва, **не** элиминация. Любые значения только-BFME2 (например, `g_Va00BBB8D8`) — INFERENCE, переносить нельзя.

## 6. В порте этой подсистемы нет

- `SubsystemInitOrder.cpp:128`: `TheVictorySystem` — строка в легенде подсистем (`{"TheVictorySystem", …}`), тело `{}`; `SubsystemLegend` только перечисляет.
- Ни `CellGrid`, ни `field28`, ни вектора записей, ни kill/death фида (RW `0x806CC5`), ни бонуса (RW `0x806EA2` / `0x806D19`), ни `CHUNK_VictorySystem` в сохранении (`GameLogicSnapshot4d.cpp` — только комментарий «RW 0x6DF73E»).
- Путь победы порта — `VictoryConditions` (элиминация, `engine/src/GameLogic/VictoryConditions.cpp`); War of the Ring «battle runner» и VictorySystem в порте не портированы (`docs/MAINLOOP.md`: строка 271 — VictorySystem среди подсистем обновления; строка 895 — «New subsystems … VictorySystem … War of the Ring hooks» в списке несделанного).
- `FactionVictoryData` в `INIBlockStubs.cpp` (рядом с `VictorySystemData`): записывающий stub, содержимое не разбирается.
- Проверка `git log` по `FactionVictoryData` / полям на `main` (`origin/main` 547e37b): порт этого не делает (поиск пуст). Второй путь не добавлен и не нужен.

Вывод: RotWK 2.01 читает блок только в подсистеме, которой порт не запускает ни в одном режиме. Правило issue («reads it only in a mode the port does not run → stop with verdict + addresses») выполнено буквально. Переносить «блок + читатель на существующий путь победы» нельзя без второй проверки победы (запрещено задачей) или без написания новой подсистемы (отдельный проект: CellGrid, фид статистики, бонусный трекер, xfer/CHUNK).

## 7. Что остаётся (owner question / Дальше)

1. Если владелец хочет саму подсистему — это отдельная задача: CellGrid + TheVictorySystem (новый модуль), включая сохранение `CHUNK_VictorySystem`, боевой опыт через `ExperienceTracker`/`0x698EC3`-аналог и проверки из `default/victorysystem.ini`.
2. Если не нужна — закрыть issue как «не используется доступными порту путями»; S-7197 тогда остаётся как запись вердикта; тесты `PORT-72*` продолжают держать то, что stub не снимают без реального парсера.
3. Все «не дочитанные» внутри подсистемы места (RW `0x806539`, RW `0x973C59`-диапазон, роль флагов игрока `+0x109`/`+0x113`) сознательно оставлены за рамками — на вердикт они не влияют, для их переноса всё равно нужен п.1.
