# Field triage 7/25 — issue #17

Статический аудит 30 полей со статусом `parsed-unused` из issue #17. Цель: для точного поведения Ennorath 1.9.8.2 на RotWK 2.01 определить, читает ли оригинальный движок каждое поле. Код игры не менялся.

## Контекст и метод

- Бинарник: только `workspace/RotWK/game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`. Образ несёт оговорку S-001 (community-modified image); все адреса ниже — RW VA этого образа.
- Инструкции читались через `python tools/mod/rw.py workspace/RotWK/game.dat dis <addr>`. Полный линейный индекс `.text` покрывал `RW 0x401000`–`RW 0xBCFFFE` (2 702 224 инструкции, включая skipdata); отрицательные выводы дополнительно проверялись адресным `dis`, vtable-установками и ссылками в данных PE.
- Строки FieldParse взяты из точных PE-строк имён и сверены с `engine/data/rotwk-201/field-tables.json` / `module-registry.json`. Смещения членов — raw offsets строки таблицы, без искусственного `+0x10`.
- Данные: сначала распакованный Ennorath 1.9.8.2 (`/tmp/ennorath-ini`, INI вне репозитория). Все 30 ключей найдены как активные присваивания; закомментированные строки отброшены. Ваниль как источник перекрытых модом полей не потребовалась. Значения из INI/game.dat в отчёт не включены.
- Виртуальные цепочки замыкались через фабрику модуля, конструктор, установку interface/vtable и конкретный слот. Для коротких геттеров искались указатели на функцию в секциях данных PE и вызовы через соответствующий слот.

Вердикты: `RotWK не использует` — все обращения к члену относятся к парсингу, конструктору, копированию, уничтожению или сохранению; `RotWK читает (логика)` / `RotWK читает (клиент)` — есть игровое/клиентское чтение; `не определено` — цепочка не замкнута.

## Таблица

| # | Поле | Вердикт | RW-адреса | Действие читателя / причина отсутствия чтения | INFERENCE |
|---:|---|---|---|---|---|
| 1 | `Locomotor/Speed` | RotWK не использует | T `RW 0xBF4488`, P `RW 0x73A4B6`, `+0x20`; ctor `RW 0x5E4378`; copy `RW 0x5E4696`; assign `RW 0x5E320B`; runtime max-speed `RW 0x5E3F49` | Парсер velocity пишет template `+0x20`. В замыкании LocomotorTemplate член только конструируется/копируется. `getMaxSpeedForCondition` `RW 0x5E3F49` берёт скорость из LocomotorSet/AI (`+0x1F8`) и повреждённый вариант template `+0x24`, но не template `+0x20`. Одноимённый `Speed` строки LocomotorSet `RW 0xBF4BE0` читается в `RW 0x5E9B77` и не является этим членом. | да: отрицательное замыкание по классу; разделение двух таблиц `Speed` |
| 2 | `BuildingBehavior/GlowWindowName` | RotWK читает (клиент) | T `RW 0xC56F90`, P `RW 0x42EED6`, `+0x20`; data ctor `RW 0x8594ED`; update `RW 0x85935C`; выбор вектора `RW 0x85946C`; итерация `RW 0x8592F6`, `RW 0x85931A`; scheduler `RW 0x62EA9C` | Вектор строк — третий из четырёх window-списков (`data+0x20`). Update перебирает имена и выставляет model condition через `RW 0x672823` — визуальное состояние окна. Update-interface `RW 0xC56EF0` ставится в ctor `RW 0x8592B2`; sleepy scheduler вызывает slot 0 в `RW 0x62EA9C`. | нет |
| 3 | `FlammableUpdate/FireFXList` | RotWK читает (клиент) | T `RW 0xC62EA0`, P `RW 0x890CD9`; запись в `data+0x28` `RW 0x890D82`; update slot `RW 0x8904B8`; вызов inner `RW 0x89059D`; чтение вектора `RW 0x89031F`, `RW 0x890431`; FX `RW 0x890403`, `RW 0x890416` | Контейнерный парсер добавляет FX-имена в `data+0x28`. Update `RW 0x8904B8` → `RW 0x8901AB` итерирует вектор и запускает FXList (`RW 0x494615` / `RW 0x4B1B5A`) — клиентский эффект горения. | нет |
| 4 | `HighlanderBody/GrabOffset` | RotWK читает (логика) | T `RW 0xC71E18`, P `RW 0x42F298`, `+0x40`; getter `RW 0x8C1DC2`; чтение `RW 0x8C1DCC`, `RW 0x8C1DD0`; body vtable `RW 0xC71ED0` slot `0x80`; caller `RW 0x66ABC2` | Геттер возвращает два float (`+0x40/+0x44`) через hidden buffer. Вызов через body slot `0x80` в `RW 0x66ABC2` использует смещение при grab-действии. | нет |
| 5 | `HordeTransportContain/EnterSound` | RotWK не использует | T `RW 0xC59F40`, P `RW 0x73B217`, `+0x38`; onContaining `RW 0x865F9A`; voice-row lookup `RW 0x865C98` → `RW 0x676A8A` | Парсер пишет audio store reference. Игровой onContaining проигрывает voice row `0x2F` через drawable/UnitVoice (`RW 0x676A8A`), а не `data+0x38`. В замыкании OpenContain/Transport/HordeTransport чтений `+0x38` как звука нет. | да: отрицательное замыкание; семантика voice-row подтверждена портом UnitVoiceResponse |
| 6 | `HordeTransportContain/ExitSound` | RotWK не использует | T `RW 0xC59F50`, P `RW 0x73B217`, `+0x3C`; onRemoving `RW 0x8680F4`; voice-row lookup `RW 0x865CB1` → `RW 0x676A8A` | Аналогично EnterSound: onRemoving использует voice row `0x30`, не `data+0x3C`. Чтений члена как звука в замыкании контейнеров нет. | да: отрицательное замыкание |
| 7 | `TransportContain/CollidePickup` | RotWK читает (логика) | T `RW 0xC5A020`, P `RW 0x42E558`, `+0x81`; reader `RW 0x8659D8`; чтение `RW 0x865A3B`; vtable `RW 0xC5D650`; install `RW 0x86B6D2`, `RW 0x87A2D2`, `RW 0x87F528`; pickup call `RW 0x865A49` | При collide проверяется bool `+0x81`; при true вызывается contain slot `0x9C` (подбор/помещение). Интерфейс ставится у TransportContain, HordeTransportContain и SiegeEngineContain. Внешний диспетчер ObjectCollide известен по порту AIWorld, конкретный RW call-site диспетчера не изолирован. | да: только внешнее звено диспетчера collide |
| 8 | `W3DScriptedModelDraw/MinLODRequired` | RotWK не использует | T `RW 0xBE1380`, P `RW 0x42E956`, userData `RW 0xD9E6BC`, `+0x64`; data ctor `RW 0x4C8658` | Index-list парсер пишет enum LOD. В клиентском кластере W3DModelDraw член только инициализируется; игрового/клиентского сравнения с текущим LOD не найдено. Совпадения `+0x64` в других классах (AnimationStateInfo, векторы) не относятся к этому члену. | да: отрицательное замыкание по классу |
| 9 | `W3DScriptedModelDraw/ParticlesAttachedToAnimatedBones` | RotWK читает (клиент) | T `RW 0xBE14B0`, P `RW 0x42E558`, `+0x108`; reader `RW 0x4C72BE`; чтение `RW 0x4C7795`; вызов `RW 0x4C77A5` | Bool выбирает клиентскую ветку привязки частиц к анимированным костям (draw slot `0x6C`). | нет |
| 10 | `FXParticleSystem/UseMaximumHeight` | RotWK не использует | динамическая регистрация `RW 0x5F35D0`, `RW 0x5FAE70`; bool `+0x83`; ctor `RW 0x5F4576`; copy `RW 0x5F507F`, `RW 0x5FC0E6`; xfer/save `RW 0x5F4732` | Статической FieldParse-строки нет; имя регистрируется динамически. Все обращения к `+0x83` — ctor/copy/xfer. Потребителя в ParticleSystem runtime не найдено; справочный decomp знает только parse/writeINI. | да: отрицательное замыкание |
| 11 | `Object/LiveCameraOffset` | RotWK читает (клиент) | T `RW 0xDA4638`, P `RW 0x42F247`, `+0x630`; getter `RW 0x484E23`; W3DView `RW 0x48A72D`; вызов getter `RW 0x48A857`; vtable `RW 0xBDD490` slot `0x1D0`; caller `RW 0x6A21BF` | Геттер копирует Coord3D из ThingTemplate `+0x630..+0x638`; W3DView использует его при привязке камеры к объекту. | нет |
| 12 | `Object/LiveCameraPitch` | RotWK читает (клиент) | T `RW 0xDA4648`, P `RW 0x42ED00`, `+0x63C`; getter `RW 0x484E40`; прямое чтение `RW 0x48A882`; W3DView `RW 0x48A72D`; caller `RW 0x6A21BF` | Pitch читается из template `+0x63C`, переводится в радианы (`RW 0xBD1900`) и задаёт ориентацию камеры. | нет |
| 13 | `KeepObjectDie/StayOnRadar` | RotWK читает (клиент) | T `RW 0xC06C3C`, P `RW 0x42E558`, `+0x3C`; onDie `RW 0x889FA7`; чтение `RW 0x88A000`; Radar::addObject `RW 0x6D9042` вызывается в `RW 0x88A01A`; die dispatch `RW 0x698FA1` | После смерти bool решает, оставить ли объект на радаре. Radar — клиентское состояние. Die-interface `RW 0xC617A4` slot 0 ставится в ctor `RW 0x88A056`; Object::friend_onDie вызывает slot 0 в `RW 0x698FA1`. | нет |
| 14 | `HordeContain/ComboHorde` | RotWK читает (логика) | T `RW 0xC5BBA0`, P `RW 0x872654`, `+0x198`; helper `RW 0x87043B`; чтение вектора `RW 0x870451`, `RW 0x870469`; callers `RW 0x871EC1`, `RW 0x871F67`, `RW 0x8765CC` | Helper ищет запись ComboHorde по имени ThingTemplate; вызовы в HordeContain используют результат для combine/dissolve логики (в т.ч. поиск шаблона `RW 0x6D1305`). | нет |
| 15 | `SiegeEngineContain/ShowPips` | RotWK читает (клиент) | T `RW 0xC5A010`, P `RW 0x42E558`, `+0x80`; reader `RW 0x866411`; чтение `RW 0x86642E`; contain interface `RW 0xC5D668` slot `0x150`; install `RW 0x87F51E`; caller `RW 0x677A47` | Bool gate для отображения pips пассажиров; при false функция возвращает нули, при true заполняет счётчики/список для UI. | нет |
| 16 | `Locomotor/PitchInDirectionOfZVelFactor` | RotWK не использует | T `RW 0xBF46B8`, P `RW 0x42ED00`, `+0xA0`; ctor `RW 0x5E447F`; copy `RW 0x5E4786`; assign `RW 0x5E32FB` | В template-замыкании только ctor/copy/assign. Чтения `+0xA0` в `RW 0x5E4DFA`–`RW 0x5E4E89` относятся к runtime Locomotor (lift в giant-bird hover `RW 0x5E4CD3`), не template. | да: отрицательное замыкание; исключение ложного runtime-совпадения |
| 17 | `Object/GeometryOther` | не определено | T `RW 0xDA4478`, P `RW 0xAD4310`, `+0xFC`; parser append `RW 0xAD44C0`–`RW 0xAD44CF`; ctor/dtor `RW 0x73F90A`, `RW 0x73FC7A`; shape consumers `RW 0xAD2860`, `RW 0x936B7D` | Парсер строит временную GeometryInfo, вызывает bounding `RW 0xAD2860` и добавляет элемент в вектор по store. Потребители форм есть, но перенос именно template `+0xFC` в runtime shape-list объекта не изолирован одной цепочкой. | да: точка остановки — merge parse-time vector → object geometry |
| 18 | `CreateObjectDie/DebrisPortionOfSelf` | RotWK не использует | T `RW 0xC61130`, P `RW 0x42EE5E`, `+0x3C`; ctor `RW 0x889020`, `RW 0x88902F`; dtor `RW 0x889078`; onDie family `RW 0x888BAE` | Строка только конструируется/очищается. Die-логика читает OCL-вектор `+0x40` (`RW 0x888F22`), но не `+0x3C`. | да: отрицательное замыкание |
| 19 | `RunOffMapBehavior/RunOffMapWaypointName` | RotWK читает (логика) | T `RW 0xC596B8`, P `RW 0x42EE5E`, `+0x14`; triggerFlee `RW 0x86540A`; имя `RW 0x86543D`; waypoint lookup `RW 0x865441`; caller `RW 0x699319` | triggerFlee ищет named waypoint; при отсутствии — fallback от позиции/направления. Interface `RW 0xC59644` slot 0 ставится в `RW 0x8652B0`; AIRampageState вызывает его в `RW 0x699319`. | нет |
| 20 | `TransportContain/FadeFilter` | RotWK читает (клиент) | T `RW 0xC5AD68`, P `RW 0x76392F`, `+0x168`; onContaining `RW 0x86A943`; filter accepts `RW 0x86AAF5`, `RW 0x86AAFB`; fade `RW 0x86AB87` | Фильтр решает, затухает ли drawable пассажира при помещении; далее вызывается Drawable fade. Contain slot `0x58` у Transport/HordeTransport ведёт в `RW 0x86A943`. | нет |
| 21 | `W3DQuadrupedDraw/LeftFrontFootBone` | RotWK читает (клиент) | T `RW 0xBE1A80`, P `RW 0x42EE5E`, `+0x188`; getFootLocations `RW 0x4C9A39`; проверки `RW 0x4C9A51`, `RW 0x4C9AE1`; строка `RW 0x4C9AFB`; vtable `RW 0xBE1AD0` slot 47; caller `RW 0x4C543E` | Имя кости проверяется на пустоту и используется для получения bone position при клиентском foot placement. | нет |
| 22 | `W3DQuadrupedDraw/LeftRearFootBone` | RotWK читает (клиент) | T `RW 0xBE1AA0`, P `RW 0x42EE5E`, `+0x190`; getFootLocations `RW 0x4C9A39`; проверка `RW 0x4C9A76`; цикл по 4 костям `RW 0x4C9AD4`–`RW 0x4C9AFB`; caller `RW 0x4C543E` | Та же цепочка getFootLocations, индекс задней левой кости. | нет |
| 23 | `W3DQuadrupedDraw/RightFrontFootBone` | RotWK читает (клиент) | T `RW 0xBE1A90`, P `RW 0x42EE5E`, `+0x18C`; getFootLocations `RW 0x4C9A39`; проверка `RW 0x4C9A63`; цикл `RW 0x4C9AD4`–`RW 0x4C9AFB`; caller `RW 0x4C543E` | Та же цепочка getFootLocations, индекс передней правой кости. | нет |
| 24 | `W3DQuadrupedDraw/RightRearFootBone` | RotWK читает (клиент) | T `RW 0xBE1AB0`, P `RW 0x42EE5E`, `+0x194`; getFootLocations `RW 0x4C9A39`; проверка `RW 0x4C9A85`; цикл `RW 0x4C9AD4`–`RW 0x4C9AFB`; caller `RW 0x4C543E` | Та же цепочка getFootLocations, индекс задней правой кости. | нет |
| 25 | `Locomotor/MaxOverlappedHeight` | RotWK не использует | T `RW 0xBF4638`, P `RW 0x42ED00`, `+0xA4`; ctor `RW 0x5E4487`; copy `RW 0x5E4792`; assign `RW 0x5E3307` | Только ctor/copy/assign. Совпадения `+0xA4` в `RW 0x5E4ED9`–`RW 0x5E4F25` — runtime yaw giant-bird hover, не template. | да: отрицательное замыкание; исключение ложного runtime-совпадения |
| 26 | `Locomotor/RudderCorrectionDegree` | RotWK не использует | T `RW 0xBF48C8`, P `RW 0x42ED00`, `+0x10C`; ctor `RW 0x5E45B9`; copy `RW 0x5E491E`; assign `RW 0x5E3493` | В template-замыкании только ctor/copy/assign; игрового читателя нет. Прочие `+0x10C` — bitflags/vtable других классов. | да: отрицательное замыкание |
| 27 | `Locomotor/RudderCorrectionRate` | RotWK не использует | T `RW 0xBF48D8`, P `RW 0x42ED00`, `+0x110`; ctor `RW 0x5E45C1`; copy `RW 0x5E492A`; assign `RW 0x5E349F` | Только ctor/copy/assign; прочие `+0x110` — flags других объектов. | да: отрицательное замыкание |
| 28 | `Locomotor/SlideIntoPlaceTime` | RotWK не использует | T `RW 0xBF47F8`, P `RW 0x73A403`, `+0xCC`; ctor `RW 0x5E4566`; copy `RW 0x5E480A`; assign `RW 0x5E337F` | Только ctor/copy/assign. `RW 0x5E8B47` читает `+0xCC` матрицы/пути, не LocomotorTemplate. | да: отрицательное замыкание |
| 29 | `Locomotor/SwoopAccelerationRate` | RotWK не использует | T `RW 0xBF4958`, P `RW 0x42ED00`, `+0x130`; ctor `RW 0x5E45E9`; copy `RW 0x5E498A`; assign `RW 0x5E34FF` | Только ctor/copy/assign; прочие `+0x130` относятся к другим классам (particle/UI/vtable). | да: отрицательное замыкание |
| 30 | `Locomotor/SwoopStandoffHeight` | RotWK не использует | T `RW 0xBF4938`, P `RW 0x42ED00`, `+0x128`; ctor `RW 0x5E4528`; copy `RW 0x5E4972`; assign `RW 0x5E34E7` | Только ctor/copy/assign; игрового читателя template `+0x128` нет. | да: отрицательное замыкание |

## Сводка

- Всего полей: **30**.
- `RotWK не использует`: **13** — #1, 5, 6, 8, 10, 16, 18, 25, 26, 27, 28, 29, 30.
- `RotWK читает (логика)`: **4** — #4, 7, 14, 19.
- `RotWK читает (клиент)`: **12** — #2, 3, 9, 11, 12, 13, 15, 20, 21, 22, 23, 24.
- `не определено`: **1** — #17 `GeometryOther`.
- Читаемых полей всего: **16**.

## Кандидаты TOP20 среди читаемых

Приоритет для переноса поведения Ennorath (не редактирует общий TOP20):

1. `TransportContain/CollidePickup` — подбор при столкновении, логика контейнеров.
2. `HordeContain/ComboHorde` — combine/dissolve орд, логика.
3. `RunOffMapBehavior/RunOffMapWaypointName` — бегство с карты по waypoint, логика AI.
4. `HighlanderBody/GrabOffset` — позиция grab-действия, логика.
5. `TransportContain/FadeFilter` — визуальное затухание пассажиров, клиент.
6. `SiegeEngineContain/ShowPips` — UI pips осадных орудий.
7. `KeepObjectDie/StayOnRadar` — видимость трупа/объекта на радаре.
8. `FlammableUpdate/FireFXList` — клиентские эффекты горения.
9. `BuildingBehavior/GlowWindowName` — визуальные состояния окон зданий.
10. `W3DScriptedModelDraw/ParticlesAttachedToAnimatedBones` — привязка частиц к костям.
11. `Object/LiveCameraOffset` — live-камера объекта.
12. `Object/LiveCameraPitch` — live-камера объекта.
13. `W3DQuadrupedDraw/LeftFrontFootBone` — foot placement четвероногих.
14. `W3DQuadrupedDraw/RightFrontFootBone` — foot placement четвероногих.
15. `W3DQuadrupedDraw/LeftRearFootBone` — foot placement четвероногих.
16. `W3DQuadrupedDraw/RightRearFootBone` — foot placement четвероногих.

## Выводы INFERENCE

- Отрицательные вердикты по редким locomotor-полям, `Speed`, `MinLODRequired`, `UseMaximumHeight`, `EnterSound`, `ExitSound`, `DebrisPortionOfSelf` опираются на замыкание класса и отсутствие игрового читателя в полном `.text`-скане; это помечено как INFERENCE, а не как одно прямое доказательство «ничего нет».
- `EnterSound`/`ExitSound`: звук в onContaining/onRemoving идёт через UnitVoice rows `0x2F`/`0x30` (`RW 0x676A8A`), а не через одноимённые члены контейнера. Семантика row lookup подтверждена существующим портом UnitVoiceResponse.
- `PitchInDirectionOfZVelFactor` и `MaxOverlappedHeight`: совпадения смещений `+0xA0/+0xA4` в giant-bird hover `RW 0x5E4CD3` являются runtime-полями Locomotor, не template; исключены по this-указателю и контексту функции.
- `CollidePickup`: чтение и vtable-установка доказаны; внешнее звено ObjectCollide-диспетчера известно по порту AIWorld, но конкретный RW call-site диспетчера не изолирован.
- `GeometryOther`: parse-time построение форм доказано; перенос именно `+0xFC` в runtime geometry объекта не замкнут.
- Все выводы относятся к предоставленному образу с оговоркой S-001.

## Незакрытые цепочки

1. `Object/GeometryOther` (`не определено`): остановка на переходе от вектора, заполняемого парсером `RW 0xAD4310`, к runtime shape-list объекта. Потребители форм `RW 0xAD2860` / `RW 0x936B7D` найдены, но прямое чтение template `+0xFC` в этой цепочке не выделено.
2. `TransportContain/CollidePickup`: вердикт «читает (логика)» дан по доказанному чтению `RW 0x865A3B` и установленному collide-интерфейсу; остаточное звено — конкретный RW-адрес внешнего ObjectCollide-диспетчера, который вызывает slot 0.
3. `Locomotor/Speed`: строка `RW 0xBF4488` и парсер `RW 0x73A4B6` подтверждены, но прямая ссылка на базу таблицы в коде не найдена (`ptrrefs` пуст); отрицательный вывод опирается на замыкание обращений к члену, а не на xref таблицы.
