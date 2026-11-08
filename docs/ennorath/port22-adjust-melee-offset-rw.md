# PORT-22: SiegeDeploy / adjustMeleeOffset — разбор RotWK 2.01 (issue #78)

Бинарник: `game.dat` md5 daed3668 (caveat S-001). Все адреса прочитаны `tools/mod/rw.py dis` / `xref` на этом файле.

Задача: `SiegeDeploySpecialPower` (RW 0x8C9D06, точка подхода к стене) гнёт смещение док-точки через `Pathfinder::adjustMeleeOffset`
(RW 0x6EA0EC), если не выставлен `SkipAdjustPosition`. В порте этой функции не было (стоп S-4941), а в меле-поиске она уже была выписана
inline (`AIPathfindMelee.cpp`, бывшие строки 338 .. 353). Ниже — что делает RW в обеих функциях и в обеих точках вызова.

## 1. Кто вызывает RW 0x6EA0EC

`xref 0x6EA0EC` даёт ровно два `call`:

| адрес вызова | функция | кто это |
|---|---|---|
| RW 0x6F3905 | внутри RW 0x6F37B3 | меле-поиск точки боя: ветка цели STRUCTURE / WALK_ON_TOP_OF_WALL |
| RW 0x8C9E7A | внутри RW 0x8C9D06 | `SiegeDeploySpecialPower::computeApproachPoint` (точка подхода осадной лестницы / башни) |

Больше вызовов нет: обе точки — один и тот же код, значит и в порте он должен быть одним.

## 2. Сигнатура RW 0x6EA0EC

Пролог RW 0x6EA0EC .. RW 0x6EA10B, эпилог RW 0x6EA1D9 `ret 0xc`: `this` в ECX, три аргумента на стеке.

| аргумент | регистр | свидетельство |
|---|---|---|
| `unit` | `[ebp+0x8]`, в EAX | RW 0x6EA108: `[eax+0x260]` (указатель на AI объекта) и `eax+0x38` (его позиция) |
| `target` | `[ebp+0xc]`, в EBX | RW 0x6EA0F3 / RW 0x6EA0FB (`ecx = ebx; call 0x68BBE0` — слой объекта), RW 0x6EA12F (`ecx = ebx; call 0x68C9AC` — позиция объекта) |
| `v` | `[ebp+0x10]`, в ESI | RW 0x6EA136 .. RW 0x6EA153 читают `[esi]`, `[esi+4]`, `[esi+8]` как вектор; RW 0x6EA1BD .. RW 0x6EA1D4 пишут масштаб обратно в `[esi]`, `[esi+4]`, `[esi+8]` |

`this` (Pathfinder) сохраняется в `[ebp-4]` (RW 0x6EA0F7) и нужен только для вызова ячейки: RW 0x6EA144 `mov ecx, [ebp-4]` перед
`call 0x5E2EF2` в RW 0x6EA188.

## 3. Ветки RW 0x6EA0EC

| адрес | что делает |
|---|---|
| RW 0x6EA0F2 .. RW 0x6EA105 | `ebx = target`; `ecx = ebx; call 0x68BBE0` (слой цели, `Object + 0x428`) → `[ebp+0xc]` (первый аргумент ячейки); `esi = v` |
| RW 0x6EA108 .. RW 0x6EA125 | **ветка контактной точки (не перенесена, S-787)**: `ecx = [unit + 0x260]` (AI) → если не ноль: `push 0; push unit+0x38; push v; push target; call 0x667C76` (`AIUpdateInterface::findNearestLabeledContactPointOnTarget(target, v, unit's position, preferred = 0)`); `test al,al; jne 0x6EA1D9` — найдена: **v заменён контактной точкой**, выход |
| RW 0x6EA12B .. RW 0x6EA183 | тело цикла: `0x68C9AC(target, &p)` — позиция цели в `[ebp-0x10 .. -8]`; `xmm3 = (float)edi` (счётчик i); `p.x += v.x * i`, `p.y += v.y * i`, `p.z += v.z * i` (три `mulss` и три `addss`, результат в `[ebp-0x1c .. -0x14]`) |
| RW 0x6EA188 .. RW 0x6EA18F | `call 0x5E2EF2(layer, &p)` (ячейка; внутри RW 0x6E8DD5 с центрированием и RW 0x5E2E9C): `test eax,eax; je 0x6EA1D9` — ячейки нет (за краем сетки): **v не меняется**, выход |
| RW 0x6EA191 .. RW 0x6EA1AE | `eax = [eax+0xc]` (слово ячейки); `ecx = [target + 4]` (шаблон цели); `test byte [ecx+0x10f], 0x10` — цель `WALK_ON_TOP_OF_WALL`? У такой цели: `shr eax,4; and eax,0x3f` (поле слоя ячейки) и `call 0x6E82B3` = «слой 0x11 .. 0x40», т.е. ячейка на стене: **истина → следующая итерация**; ложь → клетка найдена (RW 0x6EA1B1) |
| RW 0x6EA1E0 .. RW 0x6EA1E4 | иначе: `and al, 0xf; cmp al, 4` — тип ячейки (младший ниббл) `!= 4` (OBSTACLE) → клетка найдена; `== 4` → следующая итерация |
| RW 0x6EA1B1 .. RW 0x6EA1D4 | **нашли**: `v.x *= (float)i`, `v.y *= (float)i`, `v.z *= (float)i` (три `mulss`, `i` в `xmm0`), выход. `i = 0` тоже считается — смещение от цели, стоящей на свободной клетке, обнуляется |
| RW 0x6EA1E6 .. RW 0x6EA1EA | `inc edi; cmp edi, 0x14; jl 0x6EA12B` — 20 шагов (i = 0 .. 19); после 20 шагов внутри препятствия **v остаётся как был** |

Смысл: `v` — смещение от центра цели; шаг, на котором линия выходит из препятствия, даёт длину смещения. Крайние случаи дают 0 (цель стоит
на свободной клетке) и «не тронуто» (20 клеток препятствия подряд).

## 4. Точка вызова 1: RW 0x6F3905 (меле-поиск, уже перенесена)

RW 0x6F38B9 `mov eax, [ebx+4]` (шаблон цели), RW 0x6F38E1 `test byte [eax+0x108], 0x80` (STRUCTURE), RW 0x6F38EA `test byte [eax+0x10f], 0x10`
(WALK_ON_TOP_OF_WALL), RW 0x6F38F3 `test byte [eax+0x11a], 0x40` (WALL_UPGRADE → ветка не берётся), затем RW 0x6F38FC .. RW 0x6F3905:

```
mov ecx, [ebp-0x10]     ; this = Pathfinder
lea eax, [ebp-0x1c]     ; &v  (направление из RW 0x6F38A7, нормализованное и умноженное на 10 в RW 0x6F38AC .. 0x6F38DC)
push eax
push ebx                ; target
push esi                ; unit
call 0x6EA0EC
```

Порт: `AIPathfindMelee.cpp`, `Pathfinder::findMeleeEngagementLocation` — та же ветка, тот же `v`; после правки вызывает общую функцию.

## 5. Точка вызова 2: RW 0x8C9D06 (точка подхода осадного деплоя)

Аргументы RW 0x8C9D06: `[ebp+0xc]` — объект дока (стена, EDI), `[ebp+0x10]` — `Coord3D *direction`, `[ebp+0x14]` — `bool *docked`;
`this` в ECX = модуль (EBX), его данные — `[ebx+4]`, объект — `[ebx+8]`.

| адрес | что делает |
|---|---|
| RW 0x8C9D1D .. RW 0x8C9D3B | `edi = dock`; `[ebp-0x20 .. -0x18] = dock->position` (`[edi+0x38]`, `+0x3c`, `+0x40`) — будущая точка подхода |
| RW 0x8C9D66 .. RW 0x8C9D75 | `0x68BDA5(dock, "SiegeDockingBehavior")` → `esi` — поведение дока (нет: выход с позицией стены) |
| RW 0x8C9D7C .. RW 0x8C9D99 | DEPLOYING (0x40) снять; `[esi+0x20]` — интерфейс дока, слот 2 (`+8`) = `reserveFor(unit id)` (RW 0x85CD20); `< 0` → `*docked = 0` |
| RW 0x8C9D9E .. RW 0x8C9DD9 | DEPLOYING поставить; `0x85C94A(index, &[ebp-0x38])` → точка входа **копируется в `[ebp-0x20 .. -0x18]`**; `0x85C987(index)` (направление входа) → `*direction`; `*docked = 1` |
| RW 0x8C9DF4 .. RW 0x8C9E2D | `offset = точка входа - dock->position` (`subss`), `длина² = z*z + y*y + x*x` (порядок продуктов: z, y, x) |
| RW 0x8C9E31 .. RW 0x8C9E38 | `comiss` с RW 0xBD1908 (1.0); `jbe 0x8C9F97` — **не больше 1.0: точка подхода остаётся точкой дока**, остальные шаги пропускаются |
| RW 0x8C9E3E .. RW 0x8C9E5F | `offset *= 0.1` (RW 0xBD83D4, три `mulss`), в `[ebp-0x2c .. -0x24]` |
| RW 0x8C9E64 .. RW 0x8C9E68 | `cmp byte [data + 0x22], 0` (`SkipAdjustPosition`); **не ноль → 0x8C9EB2** (смещение не гнётся) |
| RW 0x8C9E6A .. RW 0x8C9E7F | `push &offset; mov eax,[RW 0xDE4B40]; mov ecx,[eax+0x10] (ThePathfinder); push edi; push [ebx+8]; call 0x6EA0EC` — `adjustMeleeOffset(unit = deploying object, target = dock, v = &offset)` |
| RW 0x8C9E7F .. RW 0x8C9EAD | точка подхода = `dock->position + offset` (три `addss` в `[ebp-0x20 .. -0x18]`) |
| RW 0x8C9EB2 .. RW 0x8C9EC6 | общая часть: `CopyCoord` / нормализация смещения (RW 0x405553) → `direction`-подобный вектор для шага |
| RW 0x8C9ECB .. | шаг 5.0 (RW 0xBDAE58) в цикле до 10 раз, пока `isPointOnWall` (RW 0x6EA857) истинно (запрос стены — issue #56), затем `ExtraWallDistance` (данные + 0x2C) вдоль направления |

Итог: при `SkipAdjustPosition No` и смещении длиннее 1.0 RW вызывает RW 0x6EA0EC с объектом, доком и смещением, а точку подхода считает
как «позиция дока + изогнутое смещение». Больше ничего в модуле смещение не меняет.

## 6. Что перенесено, что нет

Перенесено (issue #78): тело RW 0x6EA0EC целиком (цикл i = 0 .. 19, ячейка цели на её слое, выход из препятствия, масштаб `v * i`,
два крайних случая) — одна функция `Pathfinder::adjustMeleeOffset`, её зовут обе ветки RW: меле-поиск (как раньше) и точка подхода
`SiegeDeploySpecialPower::computeApproachPoint`.

Ветка контактной точки (RW 0x6EA10B .. RW 0x6EA125, `AI + 0x260` и RW 0x667C76) перенесена основной веткой раньше (TOP20 #26) и с этой правкой живёт
внутри той же функции: её зовут оба вызова RW. В данных мода `SpecialContactPoints` есть только у `WorkerAIUpdate` / `HordeWorkerAIUpdate` (Repair),
`GiantBirdAIUpdate` (Swoop) и мины (`Bomb`), у доков и стен — нет, поэтому осадная точка вызова RW этой ветки в Эннорате не достигает.

Не перенесено и остаётся стопом S-787:

- RW 0x6E82B3 (слой ячейки 0x11 .. 0x40) применяется только к цели `WALK_ON_TOP_OF_WALL`; в порте этот тест читается как тест типа 4
  (OBSTACLE). В порте поверхность стены лежит в флаге препятствия наземной сетки, а поле слоя занято самими кусками стен (WALL-1 / WALL-2).

INFERENCE: для STRUCTURE и куска стены `0x68BBE0` (слой объекта) отвечает «земля», поэтому замена `LAYER_GROUND` вызывающего на слой
цели читает те же клетки; тесты PORT-22 держат это числами.

## 7. Тесты ветки

`engine/tests/test_port_22.cpp` (`-tc="PORT-22*"`, без файлов игры):

- `PORT-22 pathfind: adjustMeleeOffset scales the offset by the first of i = 0 .. 19 steps ...` — сама функция: 0 на свободной клетке,
  без изменений после 20 клеток препятствия, `v * 2` на выходе из длинной стены;
- `PORT-22 melee engagement: a structure target's step ...` — соседняя ветка RW 0x6F3905 не изменилась: `(185, 155)` при цели-здании;
- `PORT-22 siege deploy: SkipAdjustPosition No ...` — точка подхода `(521, 505)` вместо док-точки `(545, 505)` (падает без правки);
- `PORT-22 siege deploy: SkipAdjustPosition Yes ...` — док-точка `(545, 505)` не тронута.
