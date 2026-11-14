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
