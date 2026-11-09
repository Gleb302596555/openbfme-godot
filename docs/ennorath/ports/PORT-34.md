# PORT-34: высота руин здания (`DefaultStructureRubbleHeight`, `StructureRubbleHeight`) и ветка `WALK_ON_TOP_OF_WALL` (issue #119)

Бинарник: `workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f` (оговорка S-001: образ сообщества).
Все адреса, ветки, непосредственные вызовы, указатели vtable, записи таблиц полей и строки ниже прочитаны
`tools/mod/rw.py ... dis|xref|str` и точечным чтением PE-образа (pefile) по этому файлу.
Имена, взятые из декомпиляции BFME2 или из порта, помечены `INFERENCE`; адрес инструкции и смещение поля
при этом доказаны RW. Значений INI мода, байт мода и игровых архивов в этом файле нет.

Этот файл — документ раунда r2 (issue #119): что уже в `main` (Port 34, коммиты `d062fd31`, `92dd606f`),
что поправлено в r2 и чем это подтверждено. Разделы 1–5 — исходный разбор; раздел 6 — правки r2;
раздел 7 — разбор `0x8C4CF4` (только анализ, кода нет).

## 1. Поля и строки таблиц

| что | RW | как доказано |
|---|---|---|
| `GameData` / `DefaultStructureRubbleHeight` | **строка `0xC004E0`**, поле `GlobalData + 0xAE4`, парсер `INI::parseReal` **`0x42ED00`** | имя строки — указатель `0xC0280C`; `0x42ED00`: `getNextToken 0x42DC9F` → `scanReal 0x42EAAD` → `fstp [field]` |
| `Object` / `StructureRubbleHeight` | **строка `0xDA4688`**, поле `ThingTemplate + 0x607`, парсер `INI::parseUnsignedByte` **`0x42EB75`** | строка 137 таблицы полей объекта `0xDA3DF8` (`0xDA3DF8 + 16*137 = 0xDA4688`), имя `0xC2563C`, fn `0x42EB75`, смещение `0x607` |
| значение по умолчанию `+0xAE4` | `1.0f` (`movss xmm0, [0xBD1908]` в конструкторе `GlobalData`, RW `0x643054..0x643062`) | `[0xBD1908]` = `0x3F800000` |

Обе строки сверены с `field-tables.json` по всем 191 строкам таблицы `0xDA3DF8` — совпало построчно.

## 2. `ActiveBody::setCorrectDamageState` (RW `0x8C2A5D`) — ветка стены

Ветка `WALK_ON_TOP_OF_WALL` (`KindOf` 60, байт `0x10F` бит `0x10`):

| RW | инструкция | что делает |
|---|---|---|
| `0x8C2A86`..`0x8C2A93` | `mov esi,[ebx+8]` / `mov eax,[esi+4]` / `test byte [eax+0x10F],0x10` / `je 0x8C2CCF` | шаблон — стена? нет → обычная ветка STRUCTURE |
| `0x8C2A99` | `cmp dword [ebx+0x30],3` / `jge 0x8C2AB3` | новый статус тела — RUBBLE? (входит в руины) |
| `0x8C2AB3`..`0x8C2AC4` | `mov eax,[0xDE4364]` / `fld [eax+0xAE4]` / `call 0x68B2CB` | высота = `DefaultStructureRubbleHeight`. **Байт шаблона `+0x607` не читается вообще**, поле статуса `[ebx+0x30]` не трогается |
| `0x8C2A9F`..`0x8C2AB1` | `cmp edi,3` / `jne 0x8C2AC9` / `add eax,0xA0` / `push eax` / `call 0x699349` | выход из руин: в объект кладётся **GeometryInfo шаблона** (`tt + 0xA0`) |
| `0x8C2AC9`..`0x8C2B02` | общий хвост: `call 0x6E85FB` (объект из pathfind-карты), `call 0x62684D(obj,1,4)`, `call 0x62684D(obj,1,0x4D)` | общий для обеих ветвей, вызывается **один раз** |

`0x699349` = `Object::setGeometryInfo(const GeometryInfo &)`: `0x463DFC` копирует GeometryInfo в `[obj + 0xA8]`
(`+0x04 .. +0x28`, два вектора форм `+0x2C` / `+0x38` через `0x463C74` / `0x463D42`, `+0x44 .. +0x58`), затем
`0x68B244(0)` — «геометрия изменилась», без пересчёта pathfind. Это **не** `0xAD1920 + 0x68B2CB` (как у обычного
здания): возвращается весь GeometryInfo шаблона, то есть и флаги форм, и высоты.

Для не-стены всё как было: вход `0x8C2DDA..0x8C2E13` (`movsx eax, byte [tt+0x607]`, `> 0` — байт, иначе
`[[0xDE4364]+0xAE4]`), выход `0x8C2D87..0x8C2DAE` (`0xAD1920` по `tt + 0xA0` → `0x68B2CB`).

`0x6E85FB` (= `Object::geometryChanged(true)`-часть; в порте `AIWorld::removeObjectFromPathfindMap`) —
`push 0; push 0; push 0; push obj; call 0x936B7D` — общий механизм, не относится к теме PORT-34.

**Не портировано здесь (S-342):** «Bookend»-механизм стен — статус `BOOKENDING` `0x53` и `CAN_NOT_WALK_ON` `0x4D`,
принудительный `[body+0x30] = 0` с последующим `= 3` (RW `0x8C2D69..0x8C2D82`), поиск BookendUpdate
(RW `0x8C2CEF..0x8C2D82`), блоки StatusFX / StatusDamageFX (RW `0x8C2B07`, `0x8C2B28`, `0x8C2C04`), поиск
draw-модулей клиента (RW `0x8C2C8F..0x8C2CCA`, S-142), модификаторы атрибутов (RW `0x8C2C64..0x8C2EF1`),
удаление апгрейдов и чистка пула атрибутов из `0x8C1BB7`.

## 3. Порт: где это лежит

| RW | порт |
|---|---|
| `0x8C2A5D` | `engine/src/GameLogic/Module/ActiveBody.cpp`, `ActiveBody::updateDamageState` |
| `0x68B2CB` (`GeometryInfo::setHeight` → `0xAD2C60`) | `Object::setGeometryMaxHeight` (`Object.cpp`) |
| `0x699349` (`setGeometryInfo`) | `Object::setGeometryInfoFrom(const Object &)` и (r2) `Object::setGeometryInfoFrom(const ThingTemplate &)` |
| `0x68B244` | `Object::geometryChanged` |
| `0x6E85FB` / `0x6E85E9` | `AIWorld::removeObjectFromPathfindMap` / `addObjectToPathfindMap` |
| `0x8C1B42` (`movsx byte [tt+0x607]`) | `ThingTemplate::structureRubbleHeight` |
| `0xAD1920` | `ObjectGeometry::maxHeightAbovePosition` |

## 4. Тесты

`engine/tests/test_port_34.cpp`, фильтр `-tc="PORT-34*"`:

1. `PORT-34: DefaultStructureRubbleHeight lowers active shapes on entering RUBBLE ...` — вход и выход из руин
   обычного здания (высота 105 → 8 → 105), в том числе запрос раздела 3D-сферы партиции сверху.
2. `PORT-34: StructureRubbleHeight overrides DefaultStructureRubbleHeight for 1..127 ...` — знаковый байт
   (`movsx`): 19, 127, 0, 128, 200, `ChildObject`, BFME1-таблица (`S_UnsignedByte`).
3. `PORT-34: SPHERE mutates majorRadius while BOX and CYLINDER mutate height ...` — форма `0xAD2C60`.
4. Тесты сохранения/CRC (см. файл): руины после загрузки — `hasGeometryHeights()` и высота 11.
5. **r2:** `PORT-34 r2: a destroyed WALK_ON_TOP_OF_WALL structure drops to DefaultStructureRubbleHeight ...`
6. **r2:** `PORT-34 r2: a WALK_ON_TOP_OF_WALL structure leaves RUBBLE through RW 0x699349 ...`
7. **r2:** `PORT-34 r2: a non-wall structure still takes its template StructureRubbleHeight when > 0 ...`

## 5. INI

Ни один объект в `data/ini` не задаёт `StructureRubbleHeight`; `DefaultStructureRubbleHeight = 8.0`
(`data/ini/gamedata.ini:60`). Поэтому для ванильного контента правило «у стены всегда GameData» совпадает
с «у стены байт 0 → GameData»; различие видно только на объекте, который байт задаёт — это и есть тест 5.

## 6. Раунд r2 (issue #119)

### 6.1 Шаг 1 — адреса строк в комментариях (поведение не меняется)

Комментарии Port 34 ссылались на `0xC83380` (`StructureRubbleHeight`) и `0xBFF348`
(`DefaultStructureRubbleHeight`). Оба адреса неверны:

* `0xC83380` — ASCII, хвост посторонней строки (`...sion Velocity`), не запись таблицы полей;
* `0xBFF348` — код (`+0 = 0x43B160`, `+8 = 0x63F826`), не запись таблицы.

Верные адреса — `0xDA4688` и `0xC004E0` (раздел 1). Заодно исправлено в строке `docs/ennorath/NIGHT_WATCH.md`
упоминание `INI::parseReal RW 0x42EA00`: `0x42EA00` — середина процедуры форматирования сообщения, парсер
строки `0xC004E0` — `0x42ED00` (его и цитирует `GameLogicSettings.cpp`).

Файлы: `engine/src/Common/Thing/ThingTemplate.cpp`, `ThingTemplate.h`, `engine/tests/test_port_34.cpp`,
`docs/ennorath/NIGHT_WATCH.md`.

### 6.2 Шаг 2 — одна ветка руин для стен и не-стен

`main` (Port 34) закрывал руины только для не-стен: `nonWallStructure` отсекал оба блока, у стены не менялась
высота руин и не было выхода через `0x699349`. r2 переписывает `ActiveBody::updateDamageState` так, чтобы
у обеих ветвей был **один** блок руин, как в RW (раздел 2), а разница выражалась одной строкой высоты и одним
вызовом выхода:

* вход в руины: `rubbleHeight = nonWallStructure && tplRubble > 0 ? tplRubble : settings().defaultStructureRubbleHeight`
  — у стены байт шаблона не читается никогда (RW `0x8C2A99` → `0x8C2AC4`), хвост общий;
* выход из руин: стена — `Object::setGeometryInfoFrom(*tt->getFinalOverride())` (RW `0x8C2A9F` → `0x8C2AAC`,
  `0x699349` на `tt + 0xA0`), не-стена — прежние `0xAD1920 + 0x68B2CB`;
* общего хвоста (pathfind, `NO_COLLISIONS`, опыт, command points) по-прежнему ровно один.

Новый метод `Object::setGeometryInfoFrom(const ThingTemplate &)`: `m_geometrySource` (или `nullptr`, если шаблон
свой), пустые `m_geometryActive`, `m_hasGeometryHeights = false`, `m_ownGeometry.reset()`, `++m_geometryVersion`,
`geometryChanged(false)` — ровно то, что делает `0x463DFC` + `0x68B244(0)`. Старый
`setGeometryInfoFrom(const Object &)` теперь выражен через него (копия флагов и высот объекта остаётся сверху).

Тесты 5–7: тест 5 (стена с `StructureRubbleHeight = 41` падает до 8.0) **падает без этой правки**; тест 6
проверяет именно путь `0x699349` (стена с собственными флагами формы и высотой 25 уходит в руины и после
ремонта возвращается к геометрии шаблона: высота 10, флаг `TowerTop` снова `0`, `keepsOwnGeometry() == false`);
тест 7 фиксирует, что у не-стены ничего не поехало (33 из байта, 8 из GameData, обратно 105 через `0xAD1920`).

Механизм Bookend (`0x53` / `0x4D`) в этой ветке не нужен: обе руинные ветки доходят до него (RW `0x8C2AC9`),
но это отдельная тема S-342, и в r2 он не портируется.

## 7. `0x8C4CF4` — класс и слот (только анализ, кода нет; коммит `PORT-34.md` шага 3)

Класс: **`SymbioticStructuresBody`** (порт: `engine/src/GameLogic/Module/StructureModules.h`, класс
`SymbioticStructuresBody`, фильтр `structure bodies: SymbioticStructuresBody*`). Цепочка create → ctor →
vtable → слот:

| шаг | RW | содержимое |
|---|---|---|
| create в реестре модулей | `0x6512BE` (`engine/data/rotwk-201/module-registry.json`, класс `SymbioticStructuresBody`) | `call 0xA3CEF0` (проверка стека), `push 0x10C` (размер объекта), `call 0x42F6E0` (аллокатор), `call 0x8C4C82` |
| конструктор | `0x8C4C82` | `call 0x8C3841` (база `ActiveBody`), затем `mov dword [esi], 0xC72370`, `[esi+0xC] = 0xC722B0`, `[esi+0x10] = 0xC72200`, обнуление `+0x100`, `+0x104` |
| vtable | **`0xC72370`** | ставится конструктором строкой выше; второй экземпляр того же класса — `0x8C4BE8` с теми же тремя указателями (`+0xC` / `+0x10`) |
| таблица данных | `0xC71D68` (общая с `ActiveBody` / `StructureBody` / `ImmortalBody` / `HighlanderBody`) + `0xC0602C` | реестр модулей |
| слот, в котором живёт функция | **`+0x60` (слот 24) = `0x8C4F2D`** | `0xC72370 + 0x60` = `0x008C4F2D`; `0x8C4F2D` → `0x8C4DE3`, `0x8C4EE0`, затем `[[esi+0x100]+0x10]->vslot 0x10`: «вернуть здоровье симбиота» |

`0x8C4CF4` **сама в vtable не стоит** (dword `0x008C4CF4` в образе не встречается ни разу); это обычная
приватная функция класса, которую зовут два его же метода:

* `0x8C5388` — из слота **`+0x04` (слот 1) `0x8C5304`**, в ветке «симбиот найден»: взять его `+0x88`
  (имя шаблона), сравнить с `tt + 0x64` (`m_symbiote` таблицы данных), записать id в `+0x104`, затем `0x8C4CF4`.
  Метод — `setModuleData` / повторная инициализация данных модуля;
* `0x8C53E7` — из `0x8C53D0`, где `edi = ecx - 0x10`, то есть **второй базовый подобъект** (`+0x10`,
  `0xC72200`), и `ecx - 0x10 + 0x104` = `[this + 0x114]`: область симбиота (видимо `ObjectID symbioteID`).

Что делает `0x8C4CF4` (RW `0x8C4CF4..0x8C4DE0`):

1. `0x449681([0xDE412C], [esi+0x104])` — поиск объекта по id (`ObjectID` → `Object *`); нет — выход;
2. `0x5E3BA5(obj, obj + 0x10C)` — копирование структуры из симбиота в `[this + 0x10C]` (вероятно `ObjectID` + признак);
3. `vslot +0x60` у симбиота (`0x68D950`) — «эффективно мёртв» = `getHealth() <= 0`: `0x68D950` и есть
   `Object::setEffectivelyDead(bool)` (см. `docs/CAMPAIGN-M0-END.md:138`, RK `0x8C31A5`);
4. `[[this+0x10]+0x24]->vslot 0x24` — статус тела симбиота; если это **не** `RUBBLE` (3):
   `0xAD1920(tt симбиота + 0xA0)` → `0x68B2CB`, затем `0x62684D(obj,4,0)` (снять `NO_COLLISIONS`);
   если `RUBBLE`: `movsx byte [tt+0x607]`, `> 0` — байт, иначе `[[0xDE4364]+0xAE4]` → `0x68B2CB`, затем
   `0x62684D(obj,4,1)` (поставить `NO_COLLISIONS`);
5. `0x6E85FB` / `0x6E85E9` — объект из pathfind-карты и обратно.

То есть у `SymbioticStructuresBody` **нет своего правила высоты руин**: он читает статус тела симбиота и
применяет ровно те же две величины (`tt + 0x607` при `> 0`, иначе `GlobalData + 0xAE4`), что и обычная
ветка `setCorrectDamageState` (RW `0x8C2DDA..0x8C2E13`). Это **не** ветка стены: байт шаблона здесь читается,
`0x699349` не вызывается, функции Bookend нет. Поэтому отдельного правила в порт не добавляется, а общий
стоп — `docs/STOPS.md` **S-7191**.

Ограничение метода: `xref 0x8C4CF4` даёт ровно два вызова выше; регион кода сам по себе ничего не доказывает,
поэтому класс доказан цепочкой create → ctor → vtable → слот (таблица), а не соседством адресов.

## 8. Остаток (Remaining)

* Bookend-механизм стен (`0x53` / `0x4D`, принудительный статус тела, поиск BookendUpdate) — S-342, отдельно.
* StatusFX / StatusDamageFX переходов урона (RW `0x8C2B07`, `0x8C2B28`, `0x8C2C04`) — S-342.
* Модификаторы атрибутов и FX хвоста (RW `0x8C2C64..0x8C2EF1`) — S-342.
* `SymbioticStructuresBody::updateGeometry` (RW `0x8C4CF4`) и связывание симбиота (`+0x104` пишется
  в `0x8C537B`, читается в `0x8C4CFB` / `0x8C4D15`) — S-7191.
