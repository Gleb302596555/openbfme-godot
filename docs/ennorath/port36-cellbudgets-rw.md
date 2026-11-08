# PORT-36: GameData pathfinder cell budgets — разбор RotWK 2.01 (issue #121)

Бинарник: `game.dat` md5 daed3668006cd90f01c34e5a7da1901f (оговорка S-001).
Все адреса прочитаны `tools/mod/rw.py dis|xref` по этому файлу; достижимость функций проверена
сканированием всего отображённого образа на rel32-вызовы (`E8`/`E9`) и на абсолютные 4-байтовые
указатели (таблицы vtable хранят абсолютные смещения). Имена из декомпиляции BFME2 / порта
помечены `INFERENCE`; адрес инструкции и смещение поля при этом доказаны RW.
Значений INI мода в этом файле нет.

## 1. Четыре поля GameData (RW 0x42ECB2 = int-парсер GameData)

| поле | строка | GameData row | offset | значение по умолчанию (ctor) | мод (gamedata.ini) |
|---|---|---|---|---|---|
| MaxCellsAdjustTargetDestination | 0xC013D0 | RW 0xC01120 `{name, 0x42ECB2, 0, +0x11F8}` | +0x11F8 | 0x190 = 400 (`mov eax, 0x190` RW 0x6439E6; запись RW 0x6439FB) | 400 |
| MaxCellsAdjustToNearestGroundCell | 0xC01364 | RW 0xC01150 `{… +0x1204}` | +0x1204 | 0x190 = 400 (RW 0x643A0D) | 400 |
| MaxCellsAdjustToNearestValidCell | 0xC01340 | RW 0xC01160 `{… +0x1208}` | +0x1208 | 0x190 = 400 (RW 0x643A13) | 400 |
| MaxCellsFindAttackPathSideways | 0xC012DC | RW 0xC011A0 `{… +0x1218}` | +0x1218 | 0x9C4 = 2500 (`mov eax, 0x9c4` RW 0x643A2D; записи RW 0x643A32 (+0x1214) и RW 0x643A38 (+0x1218)) | 2500 |

Все четыре строки GameData идентичны по форме: `{name-ptr, parser 0x42ECB2, 0, offset}`
(проверено чтением dword по каждому row). `[0xDE4364]` — глобальный указатель на рабочий
GameData-объект (по всему бинарнику тысячи чтений `[0xDE4364]+offset`; синхронизация
«текущий ← синглтон» — RW 0x641DB9, `[0xDE4364]` vs `[0xDE4368]`; сам `[0xDE4368]` пишется
в конструкторе RW 0x643AA3 под guard `cmp dword ptr [0xDE4368], 0`).

Рядом, для справки (ctor RW 0x6439C8..0x643A3E): +0x11EC = edi (не разобрано),
+0x11F0 = 400 (MaxCellsAdjustDestination, RW 0x6439EB),
+0x11F4 = 200 (MaxCellsAdjustHordeMeleeDestination, RW 0x6439F1), +0x11F8/+0x11FC/+0x1200/
+0x1204/+0x1208 = 400, +0x120C = 2000, +0x1210 = 15000, +0x1214/+0x1218 = 2500, +0x121C = 25000.

## 2. Где RW читает каждое поле, и что это за потребитель

### 2.1 MaxCellsAdjustTargetDestination (+0x11F8) — цепочка базового AI, vslot +0x84 базы

```
read  [0xDE4364]+0x11F8 : RW 0x6F5020 / 0x6F5025 (push) в RW 0x6F4FE5
call  0x6F4BD0          : RW 0x6F5031 → 0x6F4BD0(this, &centerCell, RW[0xDE4364]+0x11F8, ctx)
```

`0x6F4FE5` (ret 0x14): `centerCell = 0x6ecffe(&out, [ebp+8], [ebp+0x18])`; `< 0` → false;
затем `ctx = 0x6eeb80(ecx = &out, [this], [ebp+8], [ebp+0xc], [ebp+0x10], [ebp+0x14], [ebp+0x18])`
(ctx-объект предиката), потом единственный вызов `0x6F4BD0`. Не виртуальная.

**0x6F4BD0** — спиральный поиск по квадратным кольцам: сначала тест центральной ячейки
(RW 0x6F4C02..0x6F4C11), затем кольца наружу; бюджет (`arg3 = [ebp+0xC]`) убывает в начале
каждого кольца (RW 0x6F4C33..0x6F4C40), цикл стоп при `[ebp+0xC] <= 0` (RW 0x6F4C2A и
RW 0x6F4D8D: `cmp dword ptr [ebp + 0xc], esi; jle/jg`). Предикат каждой ячейки —
**RW 0x6F4777(ctx, y, x)** → `0x6F43B1(ctx->объект, …)` (reach/footprint-тест RW против
объекта-цели; состояние пишет только в ctx). Возвращает `al` = bool; лучшую ячейку не сообщает.

Единственный вызывающий `0x6F4FE5` — **RW 0x6CBFCD ⊂ 0x6CBBC3** (сам 0x6CBBC3 вызывается
только из `0x666BBC ⊂ 0x665921`; сам 0x665921 — только из `0x66E73A ⊂ 0x66E39A`). Функция `0x66E39A` —
общая для всех AI-классов: vslot **+0x84 базовой vtable AI 0xC10590** (`0xC10590+0x84 = 0xC10614`)
и vslot **0 во вторичных vftable** у 12 AI-классов (0xC5E40C, 0xC620F4, 0xC632B4, 0xC63554,
0xC63BE4, 0xC66DCC, 0xC6710C, 0xC69A84, 0xC6A63C, 0xC6A8CC, 0xC6AD2C, 0xC10614). По телу:
`0x66E39A` вызывает `isMoving` (RW 0x664485), сравнивает `getAIStateType` (RW 0x662D70)
с 6 (`AI_FOLLOW_PATH`) и 0x43 (67), выбирает между `0x66C748` и `0x6695EF`, считает минимум
по квадратным расстояниям и вызывает `0x665921` (0x66E73A). **Какой именно класс/состояние
это обслуживает, этим разбором не установлен** (INFERENCE: общая update-часть базового
AI-интерфейса, сравнивающая тип текущего состояния); важно другое: потребитель поля —
внутри общей механики обновления AI, не «фича» одного класса.

**Порт: цепь отсутствует полностью.** Ни `0x665921`, ни `0x6CBFCD`, ни `0x6F4FE5`, ни
`0x6F4BD0`, ни `0x6F4777`, ни `0x6F43B1`, ни `getAIStateType`-обёртка `0x662D70` в порте
не существуют (grep по адресам = 0). Порт не имеет ни этого поиска, ни его вызывающей цепи.

### 2.2 MaxCellsAdjustToNearestGroundCell (+0x1204) — МЁРТВЫЙ КОД в RW 2.01

```
read  [0xDE4364]+0x1204 : RW 0x6EF0F0 / 0x6EF0F5 (push) в RW 0x6EF0C1
call  0x6EF104          : → 0x6EC2AF(this, &centerCell, RW[0xDE4364]+0x1204, &outCell)
```

`0x6EF0C1` (twin 0x6EF173): `centerCell = 0x6e8dd5(&out, arg1, 1)` (cell*10+5 в out Coord3D);
`0x6EC2AF` — спиральный поиск с предикатом **RW 0x6E8885** (ячейка = `0x5e2e9c(this, 1, y, x)`;
`(cell[0xC] & 0x3f0) == 0x10`); победившую ячейку (x, y) записывает в [ebp-0x10]/[ebp-0xC];
затем out Coord3D = (cell*10 + 5.0) и вызов виртуального слота `0x1C` `[0xDE4690]` (TheTerrainLogic)
с `(x, y, 1)`.

**`0x6EF0C1` не имеет ни одного вызова в образе**: сканирование всего .text на rel32 `E8`/`E9`
дают 0 попаданий; абсолютных 4-байтовых указаний (vtable/таблицы указателей) тоже 0.
**В RW 2.01 эта функция мертва: поиск по MaxCellsAdjustToNearestGroundCell никогда не исполняется.**
Его предикат/механика ничем не отличаются от соседнего 0x6EC56B.

**Порт: отсутствует** — и отсутствует по построению, не «забыт с неверным бюджетом».

### 2.3 MaxCellsAdjustToNearestValidCell (+0x1208) — только маппинг edge-power специальной силы

```
read  [0xDE4364]+0x1208 : RW 0x6EF1A2 / 0x6EF1A7 (push) в RW 0x6EF173
call  0x6EF1B6          : → 0x6EC56B(this, &centerCell, RW[0xDE4364]+0x1208, &outCell)
```

`0x6EF173` (twin 0x6EF0C1): `centerCell = 0x6e8dd5(&out, arg1, 1)`; `0x6EC56B` — спиральный
поиск с предикатом **RW 0x6E88B1** (та же ячейка `0x5e2e9c(this, 1, y, x)`; отвергает
`bit 0x40000` (бит 18, «карта-plane») и типы ячейки `& 0xF ∈ {1, 2, 4, 5}`); out Coord3D = cell*10+5;
виртуальный слот `0x1C` TheTerrainLogic.

Единственные вызывающие `0x6EC56B` — дубликаты внутри `0x6EF173`; единственные вызывающие
**`0x6EF173` — RW 0x8C7657 и RW 0x8C7688 внутри RW 0x8C75D8** (OCLSpecialPower,
vtable 0xC738C0): оба стоят в `CREATE_AT_EDGE_NEAR_TARGET`*-блоках (`jmp`-таблица
`0x8C7727`), получают точку края карты через `TheTerrainLogic` vslot `0x34`
(findClosestEdgePoint) и передают её как Coord3D с `ecx = [0xDE4B40]+0x10` (контекст).
Это путь **Dragon Strike** (OCLSpecialPower CREATE_AT_EDGE_NEAR_TARGET_AND_MOVE_TO_LOCATION,
STOPS **S-931**, соседние S-530; в порт не перенесён — «Not ported (S-530)» в
`SpecialPowerModules.cpp`, порт чистит компетенцию S-931/S-530 lane SPELL-3).

**Порт: потребитель не существует (S-530/S-931).** Перенести бюджет некому.

### 2.4 MaxCellsFindAttackPathSideways (+0x1218) — сайд-вариант attack-поиска (корабли)

```
read  [0xDE4364]+0x1218 : RW 0x6FE37A / 0x6FE37F / 0x6FE382 (cmp ecx,[eax+0x1218]); RW 0x6FE388 (jae)
```

Инструкции стоят внутри функции **RW 0x6FDE38** (SEH-пролог @0x6FDE38; «sideways» attack path
поиск): при достижении счётчика ячеек `>= [0xDE4364]+0x1218` (2500 по умолчанию) expansion
останавливается. `0x6FDE38` — vslot **+0xC** attack-search vtable **0xC1B8D8**
(`+0x0 0x6FE7CB`, `+0x4 0x6FB869`, `+0x8 0x6FC18E`, **`+0xC 0x6FDE38`**, `+0x10 0x6FC9DA`,
`+0x14 0x6FCE1A`; в таблице vslot +0xC указан абсолютным указателем, других ссылок на
0x6FDE38 в образе нет).

`0x6FDE38` сама создаёт под-поиск через `0x6FAA2E → 0x6F817D` (vslot +0xC того же семейства).
«Наружный» к vslot +0xC вызывающий сайт этим разбором не установлен (indirect call через слот).
INFERENCE (подтверждённое портом lane ATTACKPATH-1): `0x6FDE38` — корабельный вариант attack-пути
(декомпиляция PathfinderSidewaysAttack.cpp) для sideways-расширения; порт
`AIPathfindAttack.cpp` перенёс ground-вариант (RW 0x6FC18E, бюджет MaxCellsFindAttackPath
+0x1214) и **прямо объявил ship-вариант неперенесённым** (S-784: «ships are not modelled by
the pathfinder» — см. `AIPathfindAttack.cpp:7`). Порт PK_SHIP моделирует частично
(иерархия: `AIPathfindHierarchical.cpp` expand-коридор, радиус 1 в `AIPathfindSearch.cpp`),
но vslot +0xC и этот поиск не вызываются нигде. В моде корабли есть
(corsair/dale/elven/gondor/rohan/transport, LargeShipLocomotor) — в RW корабли используют
sideways-поиск; в порте корабль атакует тем же attack-поиском, что и сухопутный юнит.

## 3. Что в порте есть с бюджетом GameData (для контраста)

| поле | offset | row (GameData table, RW проверены) | offset в RW ctor default | порт: функция | порт: поле конфига / hash |
|---|---|---|---|---|---|
| MaxCellsFindMeleeEngagementLocation | +0x11EC | RW 0xC010F0 | (значение ctor не разобрано; у порта 50) | `findMeleeEngagementLocation` (RW 0x6F37B3) | `findMeleeEngagementLimit` (:59), входит в hash |
| MaxCellsAdjustDestination | +0x11F0 | RW 0xC01100 | 400 (RW 0x6439E6/0x6439EB) | `adjustDestination` (RW 0x6FE456) | `adjustDestinationLimit` (:51), входит в hash |
| MaxCellsAdjustHordeMeleeDestination | +0x11F4 | RW 0xC01110 | 200 (RW 0x6439F1) | `adjustHordeMeleeDestination` (RW 0x6FB67A) | `adjustHordeMeleeLimit` (:52), входит в hash |
| MaxCellsAdjustTargetDestination | **+0x11F8** | **RW 0xC01120** | **400 (RW 0x6439FB)** | **нет** | **нет** |
| MaxCellsAdjustToPossibleDestination | +0x11FC | RW 0xC01130 | 400 (RW 0x643A01) | `adjustToPossibleDestination` (RW 0x6F3C87) | `adjustToPossibleLimit` (:63), входит в hash |
| MaxCellsAdjustToMeleeDestination | +0x1200 | RW 0xC01140 | 400 (RW 0x643A07) | `adjustToMeleeDestination` (RW 0x6EEBC1) | `adjustToMeleeLimit` (:58), входит в hash |
| MaxCellsAdjustToNearestGroundCell | **+0x1204** | **RW 0xC01150** | **400 (RW 0x643A0D)** | **нет** | **нет** |
| MaxCellsAdjustToNearestValidCell | **+0x1208** | **RW 0xC01160** | **400 (RW 0x643A13)** | **нет** | **нет** |
| MaxCellsPatchPath | +0x120C | RW 0xC01170 | 2000 (RW 0x643A19) | `patchPath` (RW 0x6FC9DA) | `patchPathLimit` (:53), входит в hash |
| MaxCellsFindPathLimit | +0x1210 | RW 0xC01180 | 15000 (RW 0x643A23) | `findPath` | `findPathLimit` (:54), входит в hash |
| MaxCellsFindAttackPath | +0x1214 | RW 0xC01190 | 2500 (RW 0x643A32) | `findAttackPath` (RW 0x6FC18E) | `findAttackPathLimit` (:62), входит в hash |
| MaxCellsFindAttackPathSideways | **+0x1218** | **RW 0xC011A0** | **2500 (RW 0x643A38)** | **нет** | **нет** |
| MaxCellsToExamineTowardsGoal | +0x121C | RW 0xC011B0 | 25000 (RW 0x643A3E) | `examineTowardsGoal` | `examineTowardsGoalLimit` (:64), входит в hash |

Ревизия всех 14 строк таблицы GameData выполнена сплошным чтением региона RW 0xC010F0..0xC011C0
(все 13 строк `{name, 0x42ECB2, 0, offset}`).

Все pathfinder-бюджеты в порте читаются текстовым сканом INI с жёстким требованием наличия
ключа (`getInt` в `AIPathfindConfig.cpp`, ошибка при отсутствии; LANE S-160), не таблицами
`GameLogicSettings.cpp` (там pathfinder-строк нет вовсе, только catch-all S-152). Поля
+0x11F8/+0x1204/+0x1208/+0x1218 в порте не упоминаются (grep = 0).

## 4. Почему «передать бюджет как RW» в рамках #121 нельзя

Acceptance issue требует: «each search stops at the configured cell count (a synthetic map
where the answer lies beyond a small budget and within a large one)». Это невозможно без
существования самих поисков в порте. Поиск для каждого поля:

1. **+0x11F8** — требует портирования цепи 0x66E73A ⊂ 0x66E39A (vslot +0x84 базового
   AI-интерфейса) + 0x665921 + 0x666BBC + 0x6CBBC3 + 0x6CBFCD + 0x6F4FE5 + 0x6F4BD0.
   Это общая механика обновления всех AI (owner-map класса отсутствует даже в проблемах
   репо; min/best-квадрат между 0x66C748 и 0x6695EF тоже не идентифицирован). По объёму —
   отдельный большой разбор/lane, не «только бюджет».
2. **+0x1204** — нечего портировать: RW 2.01 никогда не вызывает `0x6EF0C1`; «поведение
   RotWK для этого поля» — отсутствие поведения. Порт, портировавший поиск+мёртвую ветку,
   создал бы поведение, которого у RW нет.
3. **+0x1208** — требует завершения переноса OCLSpecialPower-edge-spawn
   (CREATE_AT_EDGE_NEAR_TARGET_AND_MOVE_TO_LOCATION, Dragon Strike), lane
   SPELL-3/STOPS S-530/S-931; это чужая незавершённая работа и отдельная механика
   (edge points TerrainLogic vslot 0x34, OCL, then + MOVE_TO_LOCATION).
4. **+0x1218** — требует портирования ship-варианта attack-поиска RW 0x6FDE38 (S-784),
   ~5.6 KB спирали с forward probe/под-поиском `0x6f6286` …; плюс решение, кто вызывает vslot +0xC
   (в порте корабли ходят тем же attack-поиском). Lane ATTACKPATH-1 объявил это не перенесённым осознанно.

Пункты 1, 3, 4 принадлежат другим остановленным работам (S-784 lane ATTACKPATH-1; S-931/S-530
lane SPELL-3; механика №1 — вообще без lane). Расширение скоупа #121 на них без решения
владельца нарушает условия задач («не трогай чужие открытые работы», «бюджеты только»).

## 5. Проверки, которыми были получены эти выводы (воспроизводимо)

```
rw.py workspace/RotWK/game.dat dis 0x6F4FE5 60   # read +0x11F8, call 0x6F4BD0
rw.py workspace/RotWK/game.dat dis 0x6F4BD0 90   # спираль, предикат 0x6F4777
rw.py workspace/RotWK/game.dat dis 0x6EF0C1 40   # read +0x1204 → 0x6EC2AF (twin)
rw.py workspace/RotWK/game.dat dis 0x6EF173 40   # read +0x1208 → 0x6EC56B
rw.py workspace/RotWK/game.dat dis 0x6FE360 60   # read +0x1218 (cmp ecx,[eax+0x1218])
# скан rel32 E8/E9 по всему .text + абсолют-скан 4 байт:
#   0x6F4BD0 ← только 0x6F5031; 0x6EC2AF ← только 0x6EF104; 0x6EC56B ← только 0x6EF1B6
#   0x6F4FE5 ← только 0x6CBFCD; 0x6EF0C1 ← 0 вызовов в образе (и 0 указателей)
#   0x6EF173 ← 0x8C7657, 0x8C7688 (⊂ 0x8C75D8, OCLSpecialPower); 0x6FDE38 ← только vtable 0xC1B8D8 vslot +0xC
# vtable 0xC1B8D8: +0x0 0x6FE7CB, +0x4 0x6FB869, +0x8 0x6FC18E, +0xC 0x6FDE38, ...
# строки GameData: dword по row → имя (0xC013D0/0xC01364/0xC01340/0xC012DC и 0xC01388/0xC013f0/0xC012fc рядом)
# ctor GameData (RW ~0x6439C8..0x643A3E): +0x11F0=400 (0x6439E6/0x6439EB), +0x11F4=200 (0x6439F1),
#   +0x11F8=400, +0x11FC=400, +0x1200=400, +0x1204=400, +0x1208=400 (0x6439E6..0x643A13),
#   +0x1214=2500, +0x1218=2500 (0x643A2D..0x643A38); [0xDE4368] ← esi (0x643AA3), [0xDE4364] — рабочий указатель (0x641DB9)
```

Оговорки: S-001 (сообщественный образ, байт-сверка с оригиналом невозможна);
вызывающий сайт vslot +0xC (инструкция `call [reg+0xC]` через 0xC1B8D8-объект) этим разбором
не установлен; 0x66E39A класс/состояние не идентифицированы (INFERENCE в §2.1);
значения INI (400/400/400/2500) — данные мода, не RW-поведение, в PR не попадают.
