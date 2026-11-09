# PORT-60: EvaAnnounceClientCreate / CreateFakeRadarEvent — разбор RotWK 2.01 (issue #170)

Бинарник: `game.dat` md5 `daed3668…` (оговорка S-001). Адреса прочитаны `tools/mod/rw.py workspace/RotWK/game.dat dis|xref`, таблицы полей — дампом 16-байтных строк, полный линейный проход по `.text` — отдельным сканером (2 701 800 инструкций, capstone, каждый операнд `[reg±disp]` с `disp == 0x1B` просмотрен вручную). «Данные» = `ModuleData`, `+0x…` — смещения в RW, а не в порте.

## Вердикт

**RotWK 2.01 не читает поле `CreateFakeRadarEvent` (данные модуля `+0x1B`) ни в одной достижимой точке — ни в коде класса, ни в унаследованных слотах, ни в общих путях (xfer, TheEva, радар).** Поле разбирается парсером по строке таблицы, записывается конструктором данных и больше никогда не читается. BFME2 1.06 ведёт себя так же (декомпилированный источник: только регистрация парсера; в комментарии донора сказано «What `CreateFakeRadarEvent` does is not established here»). Переносить нечего: порт продолжает разбирать поле (как RW) и не создаёт никакого радар-события — поведение совпадает с RotWK 2.01 и с BFME2 1.06. Код движка не менялся, сохранения, xfer и crc не менялись. Стоп S-7212.

## Строка таблицы и парсер

Таблица `EvaAnnounceClientCreateModuleData` — RW `0xC758E8` (на неё ссылается только `buildFieldParse` RW `0x8CDE48` → `0x42B8D7`):

| строка | имя | парсер | userData | смещение |
|---|---|---|---|---|
| RW `0xC758E8` | AnnouncementEventEnemy | `0x5DE588` (EvaEvent) | 0 | `+0x08` |
| RW `0xC758F8` | AnnouncementEventAlly | `0x5DE588` | 0 | `+0x0C` |
| RW `0xC75908` | AnnouncementEventOwner | `0x5DE588` | 0 | `+0x10` |
| RW `0xC75918` | DelayBeforeAnnouncementMS | `0x73A429` (unsigned int) | 0 | `+0x14` |
| RW `0xC75928` | OnlyIfVisible | `0x42E558` (bool) | 0 | `+0x18` |
| RW `0xC75938` | CountAsFirstSightedAnnoucement | `0x42E558` | 0 | `+0x19` |
| RW `0xC75948` | UseObjectsPosition | `0x42E558` | 0 | `+0x1A` |
| **RW `0xC75958`** | **CreateFakeRadarEvent** | **`0x42E558` (bool)** | **0** | **`+0x1B`** |

`0x42E558` — общий парсер bool (`0x42DC9F` — чтение токена, `0x42CE52` — bool, запись байта). В порту: `EvaAnnounceClientCreate.cpp`, kFields, `{"CreateFakeRadarEvent", INI::parseBool, nullptr, EA_OFF(fakeRadar)}` — поле разбирается с момента появления парсера (`EvaAnnounceClientCreate.cpp:20`).

## Цепочка «create → конструктор → vtable → слот»

| шаг | адрес | что стоит |
|---|---|---|
| регистрация в фабрике | RW `0x65BC48` .. `0x65BC64` (`addModule` `0x6570FE`) | тип `0x800` (`push 0x800`, RW `0x65BC48`), `push 2`, имя `0xC0A5B4` = `EvaAnnounceClientCreate` (RW `0x65BC3B`), `friend_newModuleData` RW `0x652829` (RW `0x65BC51`), `friend_newModuleInstance` RW `0x6527F1` (RW `0x65BC56`) |
| friend_newModuleInstance | RW `0x6527F1` | единственный вызов конструктора модуля `0x8CDE59` — RW `0x652818` |
| friend_newModuleData | RW `0x652829` | единственный вызов конструктора данных `0x8CDE22` — RW `0x65284A` |
| конструктор данных | RW `0x8CDE22` | vtable `0xC07A30` (RW `0x8CDE32`); `+0x08/+0x0C/+0x10 = -1`; `+0x14..+0x1B = 0`, **`+0x1B = 0` — RW `0x8CDE44`** (единственное обращение к `+0x1B` в коде класса) |
| конструктор модуля | RW `0x8CDE59` | базовый `0x764B27` (RW `0x8CDE64`); `[this+0xC]=0xC7581C` временно (RW `0x8CDE69`), `[this]=0xC75980` (RW `0x8CDE74`), `[this+0xC]=0xC75978` (RW `0x8CDE7A`); `m_due (+0x10) = 0` (RW `0x8CDE70`); `m_started (+0x14) = 0` (RW `0x8CDE81`) |
| vtable главная | RW `0xC75980` | 15 слотов, все прочитаны линейно (см. ниже); `+0x30` = `0x8CDF07` (clientUpdate) |
| vtable интерфейса (+0x0C) | RW `0xC75978` | 2 слота: `0x63F3BF` (no-op), `+0x04` = `0x8CDED6` (create slot) |
| вызывающий create slot | ModuleFactory `newModuleInstance` выбран этой vtable при создании объекта; слот зовётся один раз на создании (caller'ы — общий код создания объектов, в RW тот же код вызывает `[vtable+0x04]` у create-интерфейсов) | `0x8CDED6`: один раз, `m_started = 1`, `m_due = min(max(2, данные+0x14) + TheGameLogic (0xDE412C) +0x40`, иначе `7`) |
| xfer | RW `0x8CDDD7` | `xferVersion{1,2}` (`call [eax+0x28]`), xfer базы `ObjectModule` `0x7082E2`, `xferBool` `m_started` (`+0x14`, `[eax+0x90]`), при версии ≥ 2 `xferUnsignedInt` `m_due` (`+0x10`, `[eax+0x78]`). **Данные модуля не сохраняются, `+0x1B` не сериализуется** |

Слоты главной vtable `0xC75980` (все прочитаны линейно):

| слот | адрес | функция | читает данные `+0x1B` |
|---|---|---|---|
| `+0x00` | RW `0x764D3B` | dtor → `0x8CD422`, `0x42F6A0` при бите `[esp+8]&1` | нет |
| `+0x04` | RW `0x63F3BF` | `ret` (no-op) | — |
| `+0x08` | RW `0x8CDE8B` | имя класса: `mov eax, 0xC0A5B4` | — |
| `+0x0C` | RW `0x8CDDD7` | xfer (см. выше) | нет |
| `+0x10` | RW `0x8CDE91` | статическая инициализация intern имени `0xC0A5B4` | — |
| `+0x14..+0x20` | RW `0x63F3BF` | `ret` ×4 | — |
| `+0x24` | RW `0x851E97` | общий countdown-слот (decrement `[esi+0x28]`, при 0 — `0x6650CC` и vslot `+0x4C`) | нет (200 инструкций, 0 попаданий `+0x1B`) |
| `+0x28` | RW `0x851E97` | тот же | нет |
| `+0x2C` | RW `0x610FA7` | `al = ([ecx+0x10] == 0)` (`+0x10` модуля, не данных) | — |
| `+0x30` | RW `0x8CDF07` | **clientUpdate** (см. ниже) | **нет** |
| `+0x34` | RW `0x4C9323` | общая очистка списка (`lea ecx,[ebp+0x1B]` — адрес стекового локала) | нет |
| `+0x38` | RW `0x4C9323` | то же | нет |

Слот интерфейса `+0x04` (RW `0xC75978`): `0x8CDED6` (см. выше) — читает только `данные +0x14`.

### clientUpdate RW `0x8CDF07` по инструкциям

`m_due = [this+0x10]`; 0 → выход; `TheGameLogic` `0xDE412C` `+0x40` > `m_due` → выход; `[this+0x10] = 0`; `esi` = данные `[this+4]`, `ebp` = drawable `[this+8]`:

| адрес | что читается |
|---|---|
| RW `0x8CDF2F` | `[esi+0x18]` (`OnlyIfVisible`); RW `0x8CDF34` — `[ebp+0x440]` (у drawable флаг «скрыт»), иначе `call 0x67055A` (проверка по shroud) — `jne → выход` |
| RW `0x8CDF4C` | `[ebp+0xFC]` — объект drawable; `0 → выход` |
| RW `0x8CDF56` | `[esi+0x19]` (`CountAsFirstSightedAnnoucement`) → `[ebp+0x114] |= 0x80` |
| RW `0x8CDF62` | `call 0x68B678` (relationship, отношение объекта к локальному игроку) |
| RW `0x8CDF6F` .. `0x8CDF74` | локальный игрок `0xDE4928` `+0x10` — если управляет объектом, событие = `[esi+0x10]` (`AnnouncementEventOwner`) |
| RW `0x8CDF7C` | `call 0x6ADC43` (relationship); `0x8CDF83` .. `0x8CDF8A`: `≤1` → `[esi+0x08]` (`AnnouncementEventEnemy`); `==2` → `[esi+0x0C]` (`AnnouncementEventAlly`); иначе `-1` |
| RW `0x8CDF9C` .. `0x8CDF9F` | событие `-1` → выход |
| RW `0x8CDFA1` | `[esi+0x1A]` (`UseObjectsPosition`) → первый аргумент-позиция = `call 0x676711` от drawable; второй — та же позиция |
| RW `0x8CDFB6` .. `0x8CDFC0` | `TheEva` `0xDE3670`; `call 0x5DD9EE` = `reportEvaEvent(event, pos1, pos2)` |

`+0x1B` не встречается. RNG/state/радар не трогаются: вся презентация — вызов `TheEva->reportEvaEvent`.

## Глобальный поиск читателя `+0x1B`

1. **Линейный проход `.text`** (2 701 800 инструкций): все операнды `[reg±0x1B]` — 744 попадания, просмотрены вручную по кластерам:
   - `0x8C7xxx .. 0x8DF100` (тот же TU): `lea reg,[ebp+0x1B]` — взятие адреса стекового локала для упаковки строк в xfer (общий helper MSVC, вынесен для каждого класса: `0x8C80AA`→`0x8C7D42`, `0x8CBC14`→`0x8CB775`, `0x8CF36E`→`0x8CF234`, `0x8CFE80`→`0x8CFD46`, `0x8D06F0`→`0x8D0505`, `0x8DE499`→`0x8DDE95`, а также `0x8C93AB`, `0x8C9360`, `0x8C938F` в `0x4C9323`), плюс единственная **запись** `+0x1B` — конструктор данных RW `0x8CDE44`;
   - прочие кластеры — то же самое в других классах (например, `0x6C0D8B`: `mov [esi+0x1B], bl` в конструкторе другого класса; кластеры вида `0x…D5A/B/C/D/E …` — тот же строковый helper xfer);
   - `0xA87C78` / `0xA87DAC` (раздел `0xA13000`, характеристика `0x42000040` — **не исполняемый**): байты данных вида `movzx eax,[edx+0x1B]`, совпадение кодировки, не код.
2. **Нет других путей к данным этого модуля**: ссылки на строку `0xC0A5B4` (`EvaAnnounceClientCreate`) — только регистрация (`0x65BC3C`) и intern имени (`0x8CDE8C`, `0x8CDEB7`); на таблицу `0xC758E8` — только `buildFieldParse` (`0x8CDE4F`); на vtable `0xC75980` — только установка в конструкторе (`0x8CDE76`); на vtable `0xC75978` — только установка (`0x8CDE7A`). Читателя «по имени/интерфейсу» нет.
3. **Радар**: `createEvent` RW `0x6D98FA` вызывается из 14 точек — `0x6D9AB7`, `0x6D9BE1` (внутри `tryEvent` / `tryUnderAttackEvent`), `0x776958`, `0x776A1B`, `0x77C8CE`, `0x7822F4`, `0x7BC239`, `0x7BC280`, `0x7C0BB6`, `0x81AD84`, `0x81AE3E`, `0x88E125` (достроенное здание), `0x8A213A`, `0x8CDD81` (`RadarPulseBehavior`, данные `RadarPulseDuration`/`RadarPulseFrequency`, создаёт `RADAR_EVENT_BEACON = 6`). **Ни одна точка не относится к EVA-анонсу и не читает данные `EvaAnnounceClientCreateModuleData`.** `internalCreateEvent` RW `0x6D93EB` зовётся только изнутри радара (`0x6D9986`, `0x6D9A65`). `TheEva::reportEvaEvent` RW `0x5DD9EE` не содержит ни одного вызова `0x6D98FA`/`0x6D93EB` (прочитан целиком: очередь EVA, приоритеты по `+0x5C/+0x60`, слоты по `0x30/0x34`-таблице).
4. **BFME2 1.06** (`Open-BFME-2`, `Code/GameEngine/Source/GameLogic/Object/Create/EvaAnnounceClientCreate.cpp`, `Update/ModuleDataBuildFieldParse.cpp:1078`): `clientUpdate` не упоминает `fakeRadar` ни разу; `EvaAnnounceClientCreateModuleData` лишь обнуляет поле в конструкторе; единственное упоминание — регистрация парсера `{ "CreateFakeRadarEvent", &INI::parseBool, 0, 0x1B }` и комментарий `docs/doxygen/api/w1022_create_client_module_factories.dox`: «What CreateFakeRadarEvent does is not established here». В коде BFME2 нет и вызова радара из EVA-анонса.

## Данные мода

Ennorath 1.9.8.2: **41** вхождение `CreateFakeRadarEvent = Yes` в **24** файлах INI. Везде комментарий: «Allow player to jump to unit/Ring after hearing announcement» (напр., `object/projectile.ini` — анонс при появлении объекта). Объекты Кольца (`crate.ini:1087`, `TheDroppedRing`) дополнительно несут `ClientUpdate = RadarMarkerClientUpdate` / `MarkerType = PingOneRing` (а второй вариант Розы — `PingBeacon`): пик на радаре, к которому прыгает камера, создаётся **этим** модулем (RW `0x8CE116` далее; `TheRadar` `0xDE4AB8`, тип пинга из `MarkerType`), и он есть в порту (lane MODGAP-2, S-2451 — movie-пинг ещё открыт). `CreateFakeRadarEvent` там же — мёртвый флаг: в оригинале его никто не читает, камера прыгает за счёт `RadarMarkerClientUpdate`.

## Вердикт и что сделано

- Код поведения не менялся; новое состояние симуляции не добавлялось; xfer/crc не менялись (порт не хранил поле и раньше; RW его не сохраняет).
- Порт по-прежнему разбирает `CreateFakeRadarEvent` (kFields, `EvaAnnounceClientCreate.cpp`) — как RW (таблица `0xC758E8`).
- Пин вердикта — `engine/tests/test_port_60.cpp` (`PORT-60*`): разбор поля из синтетического INI; идентичность решения об анонсе (`update()`) и `firstSight` при Yes/No; идентичность xfer-байт; соседние решения (owner/enemy/ally, `onlyVisible`, `usePosition`) не сдвинулись.
- Стоп **S-7212** (`docs/STOPS.md`), строка в `docs/ennorath/NIGHT_WATCH.md`.

**INFERENCE:** причины, по которым поле осталось в таблице парсера, неизвестны (скорее всего, остаток исходной системы BFME/WorldBuilder); в RotWK 2.01 наблюдаемого эффекта у него нет.

**Что значило бы «доделать»:** если владелец хочет, чтобы анонс создания действительно создавал пик радара (как написано в комментарии мода), это **новое поведение, которого нет в оригинале** — нужна отдельная задача с решением владельца (тип события `RADAR_EVENT_FAKE = 10`? «never drawn» RW `0x44DE83`; позиция объекта; кому показывается), а не перенос.
