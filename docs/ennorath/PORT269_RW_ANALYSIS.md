# Port 269 / P-154: почему упавшее Кольцо видно на радаре RotWK

Issue #636. Проверено по `workspace/RotWK/game.dat` (RotWK 2.01), `workspace/BFME2/game.dat` и распакованному `ennorath-ini-1.9.8.3` (`data/ini/crate.ini`, `data/ini/object/projectile.ini`, `data/ini/default/object.ini`). Адреса ниже — виртуальные адреса `game.dat`; `rw.py` — `tools/mod/rw.py`.

## Коротко

В RotWK 2.01 Кольцо **не попадает в список объектов-блипов** радара. У `TheDroppedRing` нет поля `RadarPriority`, значит `Object::getRadarPriority` возвращает `INVALID = 0`, а `Radar::addObject` отбрасывает приоритеты `0..1`. Видимая на миникарте отметка — **радиомаркер** из INI-модуля `ClientUpdate = RadarMarkerClientUpdate` с `MarkerType = PingOneRing`. Модуль создаёт `RadarMarker`, `W3DRadar::drawEvents` двигает и рисует его как фильмовый ping. Ни имя шаблона, ни `CRATE`, ни `ONE_RING`, ни `REVEAL_TO_ALL` в этом правиле не участвуют.

Порт не имеет покадрового вызова `DrawableModule::clientUpdate`; его Godot-радар рисует `Radar::renderOverlay` → `renderObjects` из `LogicSnapshot::radar.listed`. Поэтому PORT-269 переносит то же *условие видимости* в уже существующий путь: шаблон с `RadarMarkerClientUpdate`/непустым `MarkerType` и `RadarPriority = INVALID` попадает в список объектов с приоритетом `INVALID`. Явный `NOT_ON_RADAR` и объекты без модуля сохраняют старое состояние. Внешний вид самого фильмового пинга остаётся стопом **S-7448**.

## RW: приоритет и фильтры списка объектов

| Что | Адрес | Прочитано |
|---|---|---|
| Поле `RadarPriority` | таблица `0xDA3E78`, имена `0xDA3B24`, парсер `0x42E976`, смещение `ThingTemplate + 0x600` | `INVALID = 0`, `NOT_ON_RADAR = 1`, `STRUCTURE = 2`, `UNIT = 3`, `LOCAL_UNIT_ONLY = 4`; `default/object.ini:159` задаёт `INVALID`; конструктор `0x73FFD2`, копия шаблона `0x6D259B / 0x6D25A1` |
| `Object::getRadarPriority` | `0x68EBE9..0x68EC24` | `movsx edi, byte [template+0x600]`; при `0` поднимает до `2` только garrisonable contain (`Object+0x258`, vslot `0x10`, `0x68EC05..0x68EC10`) или `CAPTURABLE` (`template+0x10E & 2`, `0x68EC14`). Никаких `CRATE / ONE_RING / REVEAL_TO_ALL` |
| `Radar::isPriorityVisible` | `0x6D8B21` | `false` для `0` и `1`, `true` для остальных |
| `Radar::addObject` | `0x6D9042..0x6D923F` | вызывает `0x68EBE9` (`0x6D905C`) и `0x6D8B21` (`0x6D9068`); структуры: `[template+0x108] & 0x80` (`STRUCTURE`), `0x20000` (`COMMANDCENTER`), `+0x116 & 0x40` (`GARRISON`), `+0x10E & 4` (`LINKED_TO_FLAG`), проверка нейтрального владельца; не-структуры: `+0x10F & 0x30` = `WALK_ON_TOP_OF_WALL | DEFENSIVE_WALL`; запись `Object+0x268` на `0x6D91C3` и сортировка по `0x68EBE9` на `0x6D91F7` |
| `W3DRadar` отрисовка объектов | `0x44F9DA..0x44FCD8` | `0x44FAAF` читает только приоритет, сравнивает с `4` (`LOCAL_UNIT_ONLY`), затем локальный/союзный фильтр `0x6A87F5`; `HERO` и `WALL_SEGMENT` влияют только на форму блипа |
| Обновление радара | `0x6D8E2B`, `0x68EC25..0x68EF00` | 64 события, forced-visible байт, `DONT_HIDE_IF_FOGGED` (`+0x122 & 0x20`); специальной ветки Кольца нет |

Полный skipdata-сweep `.text` (2 702 224 инструкции) нашёл 38 обращений к `template + 0x600`; среди точных начал функций это только парсер, геттер, конструктор/копия. Прямые вызывающие `0x68EBE9` — `0x44FAAF`, `0x6D905C`, `0x6D91F7`; `0x6D9042` — `0x68BCCD`, `0x68DA32`, `0x697DF5`, `0x69A66E`, `0x6AEBAB`, `0x6AECED`, `0x6AF758`, `0x7772F3`, `0x88A01A`. Ни один не подменяет приоритет Кольца.

## RW: KindOf не даёт правила радара

Таблица `BitFlagNames` (`0xDA0E68`, 222 имени) даёт биты: `CRATE` = индекс 48 (`+0x10E & 1`), `CAPTURABLE` = 49 (`+0x10E & 2`), `REVEAL_TO_ALL` = 87 (`+0x112 & 0x80`), `ONE_RING` = 211 (`+0x122 & 0x08`), `NEUTRALGOLLUM` = 205 (`+0x120 & 0x2000`), `DONT_HIDE_IF_FOGGED` = 213 (`+0x122 & 0x20`).

- `REVEAL_TO_ALL` читается в shroud/vision look mask `0x694ADA` (`0xFFFFF`); это уже перенесено в `engine/src/GameLogic/System/ShroudManager.cpp` (`0x694ADA` в комментариях порта), не радар.
- `CRATE` (`+0x10E & 1`) встречается в `0x71059C`, `0x7E6EA8`, `0x82E633`: счётчики / ИИ / создание, не список радара.
- Байтового теста `+0x122 & 8` для `ONE_RING` в коде нет. Единственный точный DWORD-тест `0x97CE0F` — `test dword [ecx+0x120],0x82000` (`ONE_RING | NEUTRALGOLLUM`) внутри `0x97CDA0..0x97CE4B`; функция перебирает список объектов и зовёт geometry/terrain helper `0x6C6A09` → `0x6C6975`. Точный вызывающий `0x97CDA0` не найден, но к радару цепочка отношения не имеет; «только ИИ» не утверждается.
- `0x9380F4` и соседние `test eax,0x80000` — клетки/переключатели, не шаблон.
- Строка `PingOneRing` отсутствует в бинарнике: имя приходит из INI и уходит в `CreateRadarPing` как имя клипа APT-фильма.

## RW: маркер, который действительно рисуется

| Шаг | Адрес | Факт |
|---|---|---|
| Регистрация модуля | `0x65287A` | `"RadarMarkerClientUpdate"`, `MODULETYPE_CLIENT_UPDATE = 2`, interface mask `2048` |
| Данные / `MarkerType` | `0x654AD2`, `0x8CE0B7`, `0x8CE0CA` | `MarkerType` (`INI::parseAsciiString`) лежит в `RadarMarkerClientUpdateModuleData + 8` |
| Объект модуля | `0x8CE007`, vtable `0xC759E8` | слот `+0x30` (запись `0xC75A18`) = `clientUpdate` `0x8CE116` |
| `clientUpdate` | `0x8CE116..0x8CE195` | берёт имя маркера из данных (пустое → строка `0xBD0C3F`), зовёт `TheRadar` (`0xDE4AB8`) vslot `+0x30` = `W3DRadar::createMarker` `0x44F0FA`, сохраняет `RadarMarker` через `0x8CDFC9` и двигает его в позицию drawable (`0x676711` → `0x9F9E7B`) |
| `W3DRadar::drawEvents` | `0x44DE58`, цикл маркеров `0x44EDAE` | для каждого `RadarMarker` создаёт `CreateRadarPing(id, markerType)`, двигает, освобождает на `FadeOutRadarPing` (`0x6D8711`) |

Ранее это же наблюдал Port 60: `docs/ennorath/port60-evaannounce-rw.md:89` — «пик на радаре, к которому прыгает камера, создаётся этим модулем», а `CreateFakeRadarEvent` мёртв. PR #199 зафиксировал тот же вердикт. Донор BFME2 1.06 (`Common/System/Radar.cpp`, `W3DRadar.cpp`, `W3DRadarRenderObjectList.cpp`) не содержит специальной ветки для Кольца; его `Drawable_rva00275598.cpp` показывает покадровый обход `updates150` с вызовом `clientUpdate()` (vslot `+0x30`). Точный адрес такого обхода в RotWK 2.01 этой задачей не привязан — это **INFERENCE**, отмечено в S-7448.

## Данные мода: кого добавляет то же правило

Все 14 шаблонов Ennorath 1.9.8.3 с `RadarMarkerClientUpdate` не имеют `RadarPriority`. До правки ни один не был в `radar object list` порта.

| Шаблон | Файл / строка | `KindOf` | `MarkerType` |
|---|---|---|---|
| `TheDroppedRing` | `crate.ini:1039` | `SELECTABLE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE REVEAL_TO_ALL CRATE ONE_RING` | `PingOneRing` |
| `PalantirShard`, `PalantirShard01..07` | `crate.ini:1194, 1290, 1353, 1416, 1478, 1540, 1603, 1666` | тот же набор с `ONE_RING` | `PingBeacon` |
| `TheRingNenya`, `TheRingVilya` | `crate.ini:1729, 1832` | тот же набор **без `ONE_RING`** | `PingOneRing` |
| `TheDroppedElfRing`, `TheDroppedElfRingBlue` | `crate.ini:1986, 2055` | тот же набор с `ONE_RING` | `PingOneRing` |
| `DieJagdLurtzPing` | `object/projectile.ini:5299` | `NO_COLLIDE IMMOBILE UNATTACKABLE IGNORE_FOR_VICTORY IGNORE_FOR_EVA_SPEECH_POSITION MOVE_ONLY` (нет `CRATE`, `ONE_RING`, `REVEAL_TO_ALL`) | `PingBeacon` |

**Ни один KindOf-бит не добавлен и не убран** из `RadarKinds` или фильтров порта. Правило читает только уже разбираемое INI-поле `MarkerType` существующего модуля. Убираемых из радара объектов нет: `NOT_ON_RADAR`, пустой `MarkerType`, обычный `IMMOBILE` prop и `UNIT` сохраняют прежнее состояние.

## Что сделано в порте

- `engine/src/GameClient/Radar.cpp::captureObject` (существующий путь RW `0x6D9042`): для `priority == INVALID` проверяет `hasRadarMarker(template)`, то есть нуггет `RadarMarkerClientUpdate` с непустым `MarkerType` из `ThingTemplate::clientUpdateModules()`. Если он есть, объект проходит мимо `isPriorityVisible`, но `out.priority` остаётся `INVALID` — числом RW `0x68EBE9`.
- `engine/src/GameClient/Radar.cpp::renderObjects` и `GodotInGameHud.cpp` не менялись: точка 2×2 рисуется уже существующим оверлеем. Второго пути радара нет.
- Тесты `PORT-269*` (`engine/tests/test_port_269.cpp`, `engine/cmake/Port269.cmake`) на синтетическом INI проверяют `captureObject`, настоящий `LogicSnapshot::build`, оверлей и соседей. Без правки `Radar.cpp` тест красный.

## Осталось / стоп

**S-7448**: порт показывает объектную точку, а не фильмовый `PingOneRing` / `PingBeacon` маркер с его APT-анимацией и точным поведением прыжка камеры. `DrawableModule` порта не имеет виртуального `clientUpdate`, поэтому `RadarMarkerClientUpdate::clientUpdate()` не вызывается из кадра и `Radar::drawEvents` не получает `RadarMarker`. Чтобы закрыть стоп, нужно привязать покадровый вызов слот `+0x30` в RotWK, вызвать его в цепочке `Drawable`, питать список маркеров `drawEvents` и после сравнения вида убрать объектную замену. Это общий клиентский кадровый механизм, больше объёма PORT-269; отдельная задача.
