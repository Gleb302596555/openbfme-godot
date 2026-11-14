# PORT-187 (issue #453): P-114 разворот орла после удара, P-117 промах Крылатого зверя

Разбор по RotWK 2.01 (`game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`, оговорка S-001), дизассемблер
`tools/mod/rw.py dis`, донор BFME2 1.06 в `/tmp/open-bfme-2`. Файл — рабочие заметки сессии; итог переносится в
код, тесты `PORT-187*` и строку `NIGHT_WATCH.md`, когда расхождение доказано.

## 0. Что уже прочитано и совпадает с портом (кандидаты отброшены)

| что | адрес RW | порт | вывод |
|---|---|---|---|
| переходы машины 0x3F2 Swoop → 0x3F4 Attack → 0x3F5 FollowThru → 0x3F5/0 | `RW 0x76971F .. 0x7698xx` (`defineState` 0x8DBE1D) | `GiantBirdAIUpdate::makeStateMachine` | совпадает: `STATE_SWOOP` success→`STATE_ATTACK`, failure→idle; `STATE_ATTACK`→`STATE_FOLLOW_THRU`; `STATE_FOLLOW_THRU` success→idle, failure→`STATE_SWOOP` (виток повторного захода идёт через провал FollowThru, как в RW) |
| выбор маски пути на входе в Swoop | `RW 0x76DEB6 .. 0x76DEF5`: бит 2 или бит 3 флагов (`obj AI +0x57c`) → `LOWER_END` (0xDE7910); иначе `m_level` → `MASK_DIRECT` (0xDE7918), иначе `MASK_A` (0xDE7908) | `AIGiantBirdSwoopState::onEnter` | совпадает |
| clamp поворота и сглаживание | `RW 0x5E94A6 .. 0x5E9520`: `getMaxTurnRate` (0x5E372D), `fabs(angle)`, `bigTurn = |angle| > 3 * maxTurn`, `turn = MIN(MAX(angle,-max),max)`, `+0x600 = turn*0.2 + old*0.8` | `Locomotor::giantBirdFollowPath` | совпадает, включая константы 0xBDD42C = 3.0, 0xBDAD78 = 0.2, 0xBDE8D8 = 0.8 |
| наклон/скорость после поворота | `RW 0x5E9547 .. 0x5E9610` (`4 * speed` против длины до конца пути 0x5E3C12; `pitch * -1` 0xBD19DC, `+1.5` 0xBDE8C8; ускорение 0x5E40AD / торможение 0x5E40F2; `1 - pitch*0.4`) | то же | совпадает |
| точка «вперёд» для FollowThru | `RW 0x76E5C1`: ось X трансформации (obj +8, +0x18, +0x28 — столбец X Matrix3D), шаги `FollowThroughCheckStep` до `FollowThroughDistance` (x2 при `longer`), z контрольной точки `p.z + s * FollowThroughGradient`, тест `RW 0x76E757`, `shortened` при недостижении половины | `AIGiantBirdFollowThruState::throughPoint` | совпадает (градиент, шаг, «укорочено», фильтр STRUCTURE+MONSTER+MACHINE через `RW 0x66158D(0,7,10,11)` / `RW 0x70B8C7`) |
| построение пути FollowThru | `RW 0x76E4C9 .. 0x76E550`: `duelMode != 2` → z = земля + PreferredHeight и x,y = среднее с позицией; `goal` в машину (`RW 0x8DB810` с FLT_MAX); маска `MASK_A` / `MASK_A_LOWER_START`; `AI +0x57C &= ~8`; `RW 0x769EA5` | `AIGiantBirdFollowThruState::onEnter` | совпадает |
| порог удара по контакту | `RW 0x76B744 .. 0x76B794`: `range = max(speed * 3, 8.5)` (0xC2E190), `RW 0x5E3BE4` (квадрат расстояния), x87 `range^2` против `dist^2`, `jbe` → бить, иначе `-2` (FAILURE) | `AIGiantBirdAttackState::onEnter` | совпадает — **не** источник P-117 |
| точка контакта | `RW 0x76B6FF .. 0x76B73F`: `push 0`, позиция владельца (obj +0x38), out, victim → `RW 0x667C76`; провал → `GetGameLogicRandomValue(0, 0xBC614E, line 0x37B)` и `RW 0x690BD2` (mode 1) | `contactPointOn(..., 0x37B)` | совпадает по форме; четвёртый аргумент `RW 0x667C76` — открытый вопрос S-7342, **не трогается** этой задачей |
| обновление точки прицела в Swoop | `RW 0x76E154`: ABOUT_TO_HIT; кость оружия (`RW 0x6CB81A`), иначе `RW 0x667C76` (линия 0x294) + `RW 0x690BD2`; `4 * speed` против дистанции → ABOUT_TO_HIT; сдвиг > 10 или force, затем `8 * speed` против дистанции → новая точка, в duel 0 z = земля + PreferredHeight, `RW 0x769EA5(MASK_A / LOWER_END)` | `AIGiantBirdSwoopState::refreshTarget` | совпадает |
| обновление Swoop-состояния | `RW 0x76DF1D`: `(1 - scale) * speed`, `scale *= 0.8`, `refreshTarget(0)`, ветка `m_level` → `RW 0x76A80A(-5.0, 1)`, иначе WEBBED/still и `RW 0x76A933(5.0, &m_pos, 0)`; круг `boundingCircleRadius`, при `dist^2 < r^2 * 0.5` → `refreshTarget(1)`; `arrived` | `AIGiantBirdSwoopState::update` | совпадает |

## 1. Открытые места (где порт ещё может расходиться — следующая сессия)

1. **`RW 0x5E91E2`** (путь GIANT_BIRD): «70 от якоря пути» — порт сравнивает `4900 < d2` (`RW 0x5E3BE4` x87,
   константа `0xBF4B4C`), RW — `0x5E9339` тот же вызов; сверить порядок `fcompi` и как ставится/снимается
   `MOVING` (порт ставит условие через `setCondition`, RW — `or`/`and` по `obj +0x110/0x113` с вызовом
   `RW 0x68B53C`). Разница только в визуальном состоянии, на траекторию не влияет.
2. **`RW 0x76A933` / `RW 0x76A80A`** (`steer`) — порт вызывает `checkAhead` и строит путь к `goal` (или якорю);
   для `r == 2` (объект выше пути) RW понижает на 1.0 и берёт `MASK_A`, порт это делает (`0xBD1908` = 1.0).
   Здесь расхождения не найдено, но веткa `r == 1` (`+5.0`, `MASK_A_LOWER_START`) в RW зовёт `RW 0x70C0AD`
   (setPositionZ) только после проверки — сверить ещё раз при правке.
3. **`RW 0x778413` (computeAerialPath), «заякорная» точка**: `back = min(3 * dist, 1500)` от позиции **против**
   направления X трансформации; при маске 4 и «значительно выше» она опускается на `back`, при маске 0x80 —
   `(x, y, 1000)`. Порт это повторяет; именно эта точка даёт U-образную первую секцию сплайна, когда цель
   оказывается позади. Здесь и лежит геометрия «полного круга»: следующий шаг — покадровое сравнение заголовка
   и цели после удара (см. §2).

## 2. Что нужно сделать дальше (конкретно)

1. Снять покадрово (тест `PORT-187`) после удара: состояние машины, `goalType`, `pathDistance`, курс объекта,
   курс из `RW 0x5E91E2` (`relativeAngle2D` вперёд/назад, средний угол, `turn`), и сравнить с RW-формулой,
   перечитав `RW 0x5E93BD .. 0x5E9462` (уже прочитано: `relativeAngle2D(behind) + pi` и `relativeAngle2D(ahead)`,
   среднее по числу найденных точек).
2. P-117: удар засчитывается при `dist^2 <= max(speed*3, 8.5)^2` **до** выстрела. Если орел/назгул проходит
   контактную точку между кадрами (за кадр путь `speed`), порог `speed*3` — единственная защита. Проверить
   `RW 0x76BA8B` (update атакующего состояния) на повторную попытку выстрела при `WEAPON_READY_TO_FIRE`.

## 3. Прицеливание и Крылатый зверь (P-117) — прочитано в этой сессии

| шаг | RW | вывод |
|---|---|---|
| точка контакта для режима 1 | `RW 0x690BD2`: `RW 0xAD3340` копирует `GeometryInfo +0x44` (лучшая `GeometryContactPoint`), `z` прижимается к `RW 0xAD1920` (max высоты), всегда `mov al, 1` (0xAD3373) | порт совпадает (`ObjectGeometry::worldspaceBestContactPoint` + `contactPointOn`); «не попадает» не из-за точки |
| фильтр `RW 0x66158D(0, 7, 10, 11)` + `RW 0x70B8C7` | `0x70B8C7` — `isKindOfMulti` (И по всем битам), аргументы 0, 7, 10, 11 | порт: STRUCTURE, MONSTER, MACHINE через `isKind` — совпадает |
| обновление атаки | `RW 0x76BA8B`: `flags & 8` иначе FAILURE; жертва мертва/нет → SUCCESS; оружия нет → FAILURE; `RW 0x6CDCE7` статус → `jne` (не 0) → снова статус, `cmp eax,3` (RELOADING_CLIP) → SUCCESS, иначе CONTINUE; при 0 → `RW 0x69036C`, `RW 0x69200A` (выстрел) и `obj +0x11C |= 0x8000000` (ABOUT_TO_HIT) | порт совпадает построчно |
| вход в атаку | `RW 0x76B668`: `0x61C = 1`; `flags >> 3 & 1` → CONTINUE; не объект → `RW 0x69036C` + `0x6920AD(позиция)` → SUCCESS; объект: `RW 0x667C76` (arg 0), провал → seed + `0x690BD2`; порог `max(speed*3, 8.5)`; при `dist^2 > range^2` → FAILURE (−2) | порт совпадает |

## 4. Следствие по P-117 (гипотеза для проверки тестом)

Обе ветки прицела совпадают, поэтому «промах» может быть не промахом, а **пропуском кадра удара**: удар
засчитывается только в `onEnter` и в `update`, пока стоит `WEAPON_READY_TO_FIRE`; если за кадр прохода
контактная точка оказалась дальше `max(speed*3, 8.5)`, состояние уходит в FAILURE → FollowThru → снова Swoop,
и виток повторяется — визуально это «пролетел мимо». Проверка в тесте `PORT-187`: на синтетическом INI с
числами мода (`Speed` набора SUPERSONIC 160, `TurnTime` 3500, `Acceleration` 400, `Braking` 1000,
`PreferredHeight` 121) прогнать полный проход по зданию и убедиться, что **каждый** виток Swoop даёт ровно
один выстрел (`strikes()` растёт на 1 на виток), а не «пролёт без выстрела».

## 5. Следствие по P-114 (гипотеза для проверки тестом)

Полный круг после удара — это U-образная первая секция сплайна `RW 0x778413`: контрольные точки `behind`
(позиция минус `min(3 * dist, 1500)` по оси X трансформации), `start`, `goal`, `terminal`. Когда после
follow-through якорь пути оказывается **за** птицей, `behind` лежит ещё дальше за ней, и `RW 0x5E91E2`
разворачивает объект по средней из двух точек (`relativeAngle2D(behind) + pi` и `relativeAngle2D(ahead)`) —
это и есть «полный круг» вместо разворота на цель. Проверка: тест `PORT-187` снимает по кадрам
`relativeAngle2D` (уже перенесён, `RW 0x4B3D8D`: acos точки/вектора, знак по векторному произведению) и
средний угол `RW 0x5E9443 .. 0x5E9462`, и проверяет, что на первом кадре после FollowThru средний угол
направлен **на цель** (|угол| <= turn rate), а не на `behind`.

## 6. Прочитано дополнительно: состояние после удара (P-114) и цель «вперёд»

| что | RW | вывод |
|---|---|---|
| `isWellAbove` (RW 0x769F24) | `preferredHeight * 2` против высоты над землёй, или `z > threshold` и высота `> 0.3 * ph` | порт совпадает |
| `doLocomotor` (RW 0x76D25F) | `goalType == 1` → `getMaxSpeedForCondition` (0x5E3F49) и `LOC RW 0x5E91E2(obj, speed, &pathDistance +0x5F4, spline +0x58C)`, результат в `+0x5F8 reached`; `goalType == 2` → `0x5E98D6`, затем `0x644FD0` угла объекта и цели, `0xBD83D4` = 0.1, при разнице **> 0.1** — без idle; `goalType == 3` → `0x76A9CB` (separation) и `0x5E4CD3` (hover), `reached = 1`; затем `obj +0x11C bit 0x8000000` = AIRBORNE_TARGET по `(float)AirborneTargetingHeight < heightAboveTerrain` | порт совпадает, включая **знак** сравнения 0.1 (в порту `abs(...) < 0.1` → idle, как и RW `jbe`) |
| `update` (RW 0x76CDFB) | по состояниям: 0x3ED / 0x3F4 — атака, `dw +0x5DC = 1`; 0x3EF .. 0x3F3, 0x3E9..0x3EC, 0x3F5..0x3FA, 0x3FB — `0x76D1AC` (locomotor); 0x400 — `0x5DC = 3`; прочее — ничего | порт совпадает |

### 6.1 Следствие: «полный круг» — это плановая секция пути, а не ошибка поворота

`RW 0x778413` строит первые две контрольные точки как `behind = position - min(3*dist, 1500) * X_axis` и
`position`. Первая секция Catmull-Rom (RW 0x7139E6) между ними направлена **против** курса. После
follow-through птица летит дальше по курсу; если цель осталась за кормой, `behind` оказывается ещё дальше за
спиной, и объект сначала летит в эту сторону — визуально «полный круг» перед повторным заходом. Каждый
повторный заход строит путь заново (RW 0x76DEF5 в onEnter Swoop), поэтому круг повторяется каждый раз.

Единственное место, где RotWK мог бы этого избежать — выбор курса в `RW 0x5E91E2`: он усредняет
`relativeAngle2D(behind) + pi` и `relativeAngle2D(ahead)` (RW 0x5E93BD .. 0x5E9462) — масок, якоря или
«разворота на цель» там нет. Значит по RW это **то же поведение**: подтверждать правку нужно не догадкой, а
сравнением покадровой траектории мода против RotWK; без установленной игры такой проверки у арены нет —
это вопрос к владельцу (`status=stop`, если решение не находится из бинарника).
