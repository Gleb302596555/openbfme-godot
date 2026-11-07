# PORT-8: PhysicsBehavior / OrientToFlightPath — разбор RotWK 2.01 (issue #61)

Бинарник: `game.dat` md5 daed3668 (caveat S-001). Все адреса прочитаны `tools/mod/rw.py dis` на этом файле.
Функция: шаг полёта внутри `PhysicsBehavior::update` (RW 0x79350E, vtable 0xC306B8).

## Раскладка регистров в шаге

В `update` (RW 0x79350E) `ecx` указывает на подобъект UpdateModule: его vtable 0xC306B8 стоит в модуль + 0x10
(конструктор RW 0x792B69), поэтому `ebx = модуль + 0x10`, и смещения ebx-относительные:

| обращение | поле модуля | свидетельство |
|---|---|---|
| `[ebx - 0xC]` | указатель на данные (`this + 4`) | RW 0x79351C; конструктор RW 0x792BF1 читает `[esi+4]` |
| `[ebx - 8]` | объект (`this + 8`) | RW 0x793520 (edi = объект: `+0x11C` — модели, RW 0x68B53C) |
| `[ebx + 0x10]`, `[ebx + 0x14]` | список точек, vector end (`this + 0x20`) | RW 0x7935B4; конструктор RW 0x792B8B (`lea ecx,[esi+0x20]`, ctор вектора) |
| `[ebx + 0x40]` | шаг (`this + 0x50`) | RW 0x793515 / 0x7935C0 |
| `[ebx + 0x44]` | **счётчик отскоков** (`this + 0x54`, dword) | конструктор RW 0x792BE0; `inc dword [esi+0x54]` в bounce RW 0x79323F |
| `[ebx + 0x48]` | таймер оглушения (`this + 0x58`) | RW 0x793534 |
| `[ebx + 0x4C]` / `[ebx + 0x4E]` | флаг оглушения / флаг отскока (`+0x5C` / `+0x5E`) | RW 0x793517 / 0x793526 |

Точки — Coord3D по 12 байт (`imul ecx, ecx, 0xC`, RW 0x7935FF).

## Ветки шага полёта (RW 0x7935F7 .. 0x793786)

| адрес | что делает |
| ---| ---|
| 0x7935F7 .. 0x793602 | `esi` = `&point[step]` |
| 0x793605 .. 0x79360C | данные + 0x41 (`OrientToFlightPath`) == 0 → 0x7936FE (только setPosition) |
| 0x793612 .. 0x793616 | данные + 0x40 (`TumbleRandomly`) != 0 → 0x7936FE |
| 0x79361C .. 0x79361E | шаг <= 0 → 0x7936FE |
| 0x793624 + 0x793654 | `cmp [ebx+0x44], 0` (счётчик отскоков, dword) с флагами до 0x793654; `jle` при <= 0 |
| 0x793628 .. 0x79364F | `dir = point[step] - point[step-1]` (subss), на стек `[ebp-0x14 .. -0xC]` (x, y, z) |
| 0x793654 .. 0x793670 | при отскоках > 0: **z направления заменяется** на `[RW 0xDE4364]+0xC4` (GlobalData Gravity) `*` данные + 0x4C (`GravityMult`) `*` DAT 0xBD88D8 = **100.0** (mulss) |
| 0x793675 .. 0x79368F | длина² = `(x*x + y*y) + z*z` (SSE mulss/addss), `ucomiss` с DAT 0xC1B594 = 0.0 |
| 0x793696 .. 0x79369F | `lahf; test ah, 0x44; jnp 0x7936C3`: переход только при РАВЕНСТВЕ с 0 — длина² == 0 пропускает масштабирование (NaN масштабируется; тот же идиом, что RW 0x68F57A и `CombatQueries.cpp`) |
| 0x7936A1 .. 0x7936C0 | `inv = WWMath::Inv_Sqrt(длина²)` (RW 0x441C56 — быстрый обратный корень, S-452); `dir.x/y/z *= inv` (x87 `fmul` + `fstp dword`, PC24) |
| 0x7936C3 .. 0x7936EB | позиция = `point[step]` (`[ebp-0x20 .. -0x18]`); `Matrix3D::buildTransformMatrix` (RW 0xB26610) строит матрицу 3x4 |
| 0x7936F0 .. 0x7936F7 | объект → **RW 0x70BA76** (`setTransformMatrix(&матрица)`): копия матрицы в объект + 8 (0x70BB21 .. 0x70BB62), позиция из столбца переноса (объект + 0x38 / 0x3C / 0x40, 0x70BB65 .. 0x70BB7C), угол = `atan2(X.y, X.x)` (RW 0xB261F0: `fld [ecx+0x10]; fld [ecx]; fpatan`) в объект + 0x44 (0x70BB81), очистка объект + 0x5C, реакция на смену трансформа (vslot 0x14, старая матрица / позиция / угол) |
| 0x7936FC | `jmp 0x793707` — в этой ветке setPosition НЕ вызывается (позицию ставит сама матрица) |
| 0x7936FE .. 0x793702 | иная ветка: объект → **RW 0x70C201** (`setPosition(&point[step])`) |
| 0x793707 .. 0x793786 | следующая позиция ИИ (obj + 0x198 / + 0x1A6; в последней точке `2 * point - позиция`, DAT 0xBD889C = 2.0) — не относится к задаче, перенесено раньше |

## RW 0xB26610 (buildTransformMatrix: позиция + направление → матрица 3x4)

Аргументы: `ecx` = выходная матрица, `[esp+8]` = `&pos`, `[esp+0xC]` = `&dir` (dir уже нормирован в 3D вызывающим).

1. `len2 = fsqrt(dir.x*dir.x + dir.y*dir.y)` (x87: `fmul/faddp/fsqrt`, итог `fstp dword`).
2. Сравнение `len2` с 0.0 (DAT 0xC1B594): равенство → `(x̂, ŷ) = (1.0, 0)` (xmm2 остаётся DAT 0xBD1908 = 1.0, xmm1 = 0);
   иначе `inv2 = 1.0 / len2` (divss), `x̂ = inv2 * dir.x`, `ŷ = inv2 * dir.y` (mulss).
3. Матрица: единичная с переносом `pos` (столбец переноса повороты не трогают), затем повороты строк:
   - крен по Z (пары столбцов 0,1 каждой строки): `(c0, c1) → (c0*x̂ + c1*ŷ, c1*x̂ − c0*ŷ)`;
   - тангаж (пары 0,2): `(c0, c2) → (c0*len2 + c2*dir.z, c2*len2 − c0*dir.z)` (`xmm0 = −dir.z`, `subss`).
4. Итог (при |dir| = 1): столбец X = `dir`, Y = `(−ŷ, x̂, 0)`, Z = X × Y, перенос = `pos`. Ось X объекта смотрит вдоль
   направления; свободного крена нет (рупоромба — Y всегда горизонтален). При `dir = 0`: столбцы X и Z вырождаются в 0,
   Y = (0, 1, 0), перенос = `pos` (формулы выше при len2 = 0 и dir.z = 0).

Эта же матрица строится в шаге BezierProjectileBehavior (порт `ProjectileModules.cpp`, комментарий «Matrix3D::buildTransformMatrix
(RW 0xB26610)»), и RW 0x70BA76 соответствует портовому `Thing::setTransform` (угол `atan2(X.y, X.x)` — RW 0xB261F0).

## TumbleRandomly (вторая половина S-782)

- Поле: данные + 0x40, строка таблицы RW 0xC30538, парсер RW 0x42E558 (bool), конструктор данных RW 0x792559 обнуляет.
- Линейный разбор кода модуля (RW 0x792000 .. 0x793900 — конструктор данных и модуля, fling, flyTo, buildCurve, stun, standUp,
  sleep, endOfFlight, rest, bounce, update): данные + 0x40 читаются **только** в RW 0x793612, + 0x41 — только в RW 0x793608.
  Других чтений нет; других вызовов постановки трансформа в функциях модуля нет (единственные `0xB26610` / `0x70BA76` — ветка выше,
  `0x70C201` — setPosition).
- Значит, в этой функции `TumbleRandomly` делает ровно одно: подавляет `OrientToFlightPath` (шаг становится обычным `setPosition`,
  угол не меняется). Кода «случайного кувырка» в модуле PhysicsBehavior у RotWK 2.01 нет.
- Одноимённое поле BezierProjectileBehavior (данные + 0x48, RW 0x85F57E; OrientToFlightPath там + 0x49, RW 0x85F56E) устроено так же —
  гейт с переходом на setPosition-ветку RW 0x85F69E — и уже перенесено как гейт (`ProjectileModules.cpp`).
- INFERENCE (по имени поля): «случайное вращение» логика 2.01 не делает ни для одного из двух модулей; если в ретейле тело и кувыркается,
  это не эта функция и не этот модуль. В данных Эннората `TumbleRandomly` у PhysicsBehavior не задан ни разу (аудит полей: у
  BezierProjectileBehavior — 622 определения, у PhysicsBehavior — 0).

## Данные мода (Эннорат 1.9.8.2 поверх ванили)

`PhysicsBehavior` + `OrientToFlightPath = Yes` — ровно один шаблон: `BuildingGenBitTemplate`
(`data/ini/object/civilian/civiliansubobjects.ini:244`, блок `AllowBouncing = Yes`, `OrientToFlightPath = Yes`,
`KillWhenRestingOnGround = Yes`, `GravityMult = 1.0`) и его рескины `BuildingGen01BitA..D`, `BuildingGen02BitA..D`, ...
— обломки, которые отлетают от разрушаемых зданий (`OCL_Gen01_Chunk1` c `Disposition = SEND_IT_FLYING BUILDING_CHUNKS LIKE_EXISTING`,
`objectcreationlist.ini:27`; например `GondorBuildingGeneral01`, `DamageCreationList = OCL_Gen01_Chunk1 CATAPULT_ROCK FRONT_DESTROYED`,
`civilianbuildings.ini:2070`). Остальные пять файлов с `OrientToFlightPath` — блоки `BezierProjectileBehavior` (уже перенесённый модуль).

## Что переносится по этому разбору

Вся ветка ориентации `PhysicsBehavior::update` (RW 0x7935F7 .. 0x7936F7): три гейта, вычисление направления (с заменой z на
отскоках), нормализация Inv_Sqrt (RW 0x441C56) и матрица RW 0xB26610 → `Object::setTransform` (RW 0x70BA76). Иная ветка остаётся
`setPosition` (RW 0x70C201). `TumbleRandomly` переносится как гейт (его поведение в RW — это и есть гейт). Не переносится по-прежнему:
клиппинг посадки (RW 0x6EF8A8), пошаговое обновление pathfinder (RW 0x6F0741), статус 70 (RW 0x693919), лишние слова fling
(RW 0x6CF590), клиентские обновления drawable и другие вызывающие fling (их лейны).
