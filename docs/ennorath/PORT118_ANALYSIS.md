# PORT-118 / issue #302 — статический разбор атаки на крепость

**Статус:** статический разбор завершён; **нужна проверка в игре после Port 164**. Игровой прогон на установке Ennorath делает владелец проекта. Этот документ не заявляет, что Drogoth уже проверен в игре.

## Объект и границы задачи

Решение владельца: проверять атаку по отдельной targetable цитадели/keep, а не по корню `CastleBehavior`. Корень остаётся `UNATTACKABLE`, как в RotWK; обходить этот статус нельзя.

В Ennorath 1.9.8.2 данные Arnor находятся в `data/ini/object/goodfaction/structures/arnor/arnorfortress.ini`:

- `ArnorFortress` — размещаемый корень, у которого в обычном состоянии `Model = None`; его `CastleBehavior` распаковывает отдельные объекты.
- `ArnorFortressCitadel` — самостоятельный объект с рисуемой моделью `arrfort`. Его `KindOf` содержит `STRUCTURE`, `CASTLE_KEEP` и `SELECTABLE`, но не `UNATTACKABLE`.
- У цитадели есть центральная BOX-геометрия и `HighTowerGeom` (BOX, радиус 40, высота 100). Её `GeometryContactPoint = X:0 Y:0 Z:180 Swoop`; остальные активные контактные точки в этом объекте имеют Z=0.

`CastleBehavior::onBuildComplete` ставит runtime-статус `UNATTACKABLE` своему объекту-корню (RW 0x798238). При unpack цитадель получает отдельный Object ID. Это не основание переносить статус корня на keep.

## Какой объект получает правый клик

В порте точечный order-click идёт из `CommandTranslator` через `HudObjects::pickObject` к pick ray установленному `InGameHud`. Проверка drawn-model ray находится в `GameClient/DrawablePick`; объект без модели не даёт попадания. Поэтому на видимой модели Arnor-крепости кандидатом на атаку является рисуемая `ArnorFortressCitadel`, а не невидимый `ArnorFortress` root. Уже существующий QA-1 тест `engine/tests/test_qa1_pick.cpp` проверяет этот механизм на Men fortress: click выбирает отдельную цитадель и показывает её command set.

В RotWK контекстный order обрабатывается `evaluateContextCommand` (RW 0x81E60B); pick рисуемых объектов проходит через `W3DView::pickDrawable` (RW 0x48AB28) и scene cast (RW 0x471A5B). По данным Arnor, видимая модель принадлежит отдельному keep, а root в штатном состоянии модели не рисует. Это согласуется с тем, что команда по видимой модели должна целиться в цитадель. Точное поведение в реальной Arnor-сцене остаётся частью проверки владельца после Port 164; QA-1 воспроизводит Men, а не полную Ennorath-сцену.

RW 0x81E691 — соседний читатель `maskObject`, относящийся к masked rebuild-hole объектам (S-1427); он не описывает обычную распакованную цитадель. Для текущей цели root/citadel важны draw-based picking и отдельные Object ID.

## Drogoth: order acceptance отдельно от доставки удара

В Ennorath `Object Drogoth` выбирает `NazgulPlowAttack` во SECONDARY против `STRUCTURE`. Оружие: `AttackRange = 24`, `MeleeWeapon = No`, `CanSwoop = Yes`, `ChaseWeapon = Yes`; у структурного DamageScalar есть ненулевое значение. `GiantBirdAIUpdate` задаёт `SpecialContactPoints = Swoop`. FellBeast locomotor и его swoop-настройки в архиве содержат `PreferredHeight = 121` и Swoop-параметры.

Проверки следует разделять:

1. Контекстный клик может выдать `MSG_DO_ATTACK_OBJECT` по вражеской цели, если цель атакуемая и выбранный юнит способен атаковать (`CommandTranslator::evaluateContextCommand`; RW 0x81E60B). Для targetable `ArnorFortressCitadel` объектных причин отказать по `UNATTACKABLE` в данных нет.
2. GiantBird принимает команду атаки на объект и заводит Swoop (RW 0x76D995 -> 0x76A139). Это принятие команды ещё не означает выстрел.
3. В Swoop цель проверяется отдельно; root со статусом `UNATTACKABLE` выходит до атаки (RW 0x76DF1D), а обычный наземный `ObjectWeapons::canAttackObject` отвергает такой target через проверку статуса (RW 0x6C9207). Эти результаты для корня соответствуют RotWK и не должны «исправляться».
4. Доставка происходит только когда GiantBird достигает ударной фазы: `AIGiantBirdAttack` проверяет расстояние до цели и затем вызывает выстрел по объекту (RW 0x76B668; weapon fire RW 0x69200A). Синтетический PORT-118 ранее подтвердил, что летающий GiantBird может нанести урон отдельному targetable Keep; это не является прогоном Drogoth по реальным данным Ennorath.

## Контактная точка после Port 164

Старое объяснение, которое рассматривало mode 0 как общий путь/fallback точки удара Drogoth, здесь не используется: оно устарело относительно Port 164 (PR #416, issue #387). По новому пути GiantBird сначала ищет на цели метку из `SpecialContactPoints` через RW 0x667C76; если подходящей метки нет, fallback использует `GeometryInfo::getBestContactPoint` mode 1 (RW 0xAD3187 -> 0xAD3340, через RW 0x690D20). Mode 1 берёт сохранённую высшую `GeometryContactPoint` строки цели и ограничивает Z максимальной высотой её геометрии; это не прежняя оценка `position + height * 0.5`.

В Ennorath-данных Drogoth просит метку `Swoop`, и `ArnorFortressCitadel` содержит точку с меткой `Swoop`. В файле также видно, что это самая высокая точка по Z (180), а `HighTowerGeom` достигает 100. Следовательно, статически данные не указывают на отсутствие нужной метки; fallback mode 1 также выбрал бы высшую строку и ограничил бы её Z геометрией. Но это только проверка совместимости INI-данных с post-Port-164 lookup/fallback. Она **не подтверждает**, что реальная игра доходит до attack-state и наносит урон.

Port 164 затрагивает общий механизм GiantBird-контактных точек, а не только Drogoth. Поскольку PR #416 ещё не слит на момент подготовки этого документа, проверку поведения следует проводить на сборке после его слияния; не сравнивать результаты с прежним mode-0-only описанием как с текущим ожидаемым путём.

## Вывод и оставшаяся проверка

Статические данные согласуются с targetable цитаделью и именованным `Swoop`-путём; подтверждённого расхождения порт/RW, требующего правки поведения в PORT-118, не найдено. Оставшийся статус — **нужна проверка в игре после Port 164**.

Проверка владельца: в Ennorath-скирмише выбрать летающего Drogoth, правым кликом приказать атаковать вражескую Arnor-крепость, подтвердить фактическую цель (keep/citadel, не root), дождаться Swoop/выстрела и убедиться по здоровью, что цитадель получает урон. Если в игре приказ принят, но удара/урона нет, записать имя и ID фактической цели и состояние Drogoth/цели; затем отдельно сопоставить attack-state и доставку удара, не меняя `UNATTACKABLE` у корня.

## Основания

- Ennorath INI archive 1.9.8.2: `data/ini/object/goodfaction/structures/arnor/arnorfortress.ini`, `data/ini/object/evilfaction/units/wild/drogoth.ini`, `data/ini/weapon.ini`, `data/ini/locomotor.ini`.
- Port 164 / fork issue #387: https://github.com/Gleb302596555/openbfme-godot/pull/416 (contact-point lookup changes; in-game check intentionally remains pending).
- Existing code/test references: `engine/src/GameClient/InGameHud.cpp`, `GameClient/DrawablePick.cpp`, `GameClient/MessageStream/CommandXlat.cpp`, `GameLogic/Module/GiantBirdAIUpdate.cpp`, `GameLogic/Object/ObjectGeometry.cpp`; `engine/tests/test_qa1_pick.cpp` and `engine/tests/test_port_118.cpp`.
