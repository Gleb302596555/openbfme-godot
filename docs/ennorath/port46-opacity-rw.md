# PORT-46: ToggleMountedSpecialAbilityUpdate / OpacityTarget — разбор RotWK 2.01 (issue #150)

Бинарник: `game.dat` md5 `daed3668…` (оговорка S-001). Все адреса прочитаны `tools/mod/rw.py <game.dat> dis|xref|str`, таблицы полей — дампом 16-байтных строк по адресу из `xref`. Ниже «данные» = `ModuleData`, «модуль» = рантайм-объект `SpecialAbilityUpdate`, `+0x…` у данных и модуля — смещения в RW, а не в порте.

## Три таблицы с строкой OpacityTarget

| таблица | строка | адрес строки | парсер | смещение в данных | класс-владелец |
|---|---|---|---|---|---|
| RW `0xC05A18` | RW `0xC05A18` | имя RW `0xC05A04` = `OpacityTarget` | `0x42ED00` (`INI::parseReal`) | `+0xD0` | `ToggleMountedSpecialAbilityUpdate` |
| RW `0xC05B48` | RW `0xC05B48` = RW `0xC05AF8` + 5·0x10 | та же строка | `0x42ED00` | `+0xE0` | `SummonReplacementSpecialAbilityUpdate` (вторая половина его таблицы из 10 строк) |
| RW `0xC05C10` | RW `0xC05C10` | та же строка | `0x42ED00` | `+0xD4` | `SpecialDisguiseUpdate` |

Значения `+0xD0` / `+0xE0` / `+0xD4` проверены в самих строках таблиц; `0x42ED00` — общий парсер вещественных полей (xref из строки таблицы).

## Цепочка «create → конструктор → vtable → слот» для ToggleMounted

| шаг | адрес | что стоит |
|---|---|---|
| имя в реестре | RW `0xC0B488` = строка `ToggleMountedSpecialAbilityUpdate`; `push 0xC0B488` RW `0x659676` | регистрация модуля в `0x6570FE` (RW `0x65969B`) |
| фабрики | `push 0x64F3B1` RW `0x659688`, `push 0x64F376` RW `0x65968D` | `0x64F3B1` — `newModuleData`: аллокация `0xE8`, конструктор данных `0x8B1364` (RW `0x64F3D5`); `0x64F376` — `newModule`: аллокация `0x90`, конструктор модуля `0x8B10DC` (RW `0x64F3A0`) |
| таблица полей | RW `0x64F35B` → `push 0xC05A18` RW `0x64F36B` → `0x42B8D7` RW `0x64F370` | сначала базовый конструктор `0x851B5D`, затем парсер строк RW `0xC05A18`; строка 0 пишет `+0xD0` |
| конструктор данных | RW `0x8B1364`, `movss [esi+0xD0], xmm0` RW `0x8B1384` | `+0xD0` по умолчанию 0.0 (`xorps` RW `0x8B136D`) |
| конструктор модуля | RW `0x8B10DC`: `call 0x851D42` RW `0x8B10E7`, `mov [esi], 0xC6BDA0` RW `0x8B10FE` | главный vtable RW `0xC6BDA0`; `+0x88` — create-интерфейс `0xC6BCA8`, `+0x20` — special-power-update `0xC6BCB8` |
| слот | RW `0xC6BDA0 + 0x50` = RW `0x8B1142` | `handlePackingProcessing` (RW `0x851E9F` — базовый, перекрыт) |
| вызывающий | RW `0x854F7A` .. `0x854F7E` в `SpecialAbilityUpdate::update` RW `0x854DF7` | `mov eax,[ebx]` (`ebx` = модуль, RW `0x854E02`), `call [eax+0x50]`, `test al,al` → early return `0x85535E`, пока идёт упаковка / распаковка |

Базовый `handlePackingProcessing` RW `0x851E9F`: decrement `+0x28` (RW `0x851EB8`), `mov al,1` RW `0x852073`, когда кадры ещё идут; на последнем кадре unpacking return false (RW `0x851FEC`), на последнем кадре packing вызывает vslot `0x4C` (`finishAfterPacking`, RW `0x85202B`) и всё равно возвращает true.

## Ветки fade RW `0x8B1142` (ToggleMounted)

| адрес | что делает |
|---|---|
| `0x8B114C` | `call 0x851E9F` — базовая обработка кадров упаковки / распаковки, `bl = al` |
| `0x8B1153` .. `0x8B1155` | `bl == 0` → `0x8B11FA`: **никакой записи opacity в этом кадре** (последний кадр unpacking) |
| `0x8B115B` | `cmp [esi+0x30], 2`: состояние 2 = UNPACKING |
| `0x8B1164` .. `0x8B117F` | UNPACKING: `from = 1.0` (DAT RW `0xBD1908`, `movss` RW `0x8B1164`), `to = данные +0xD0` (RW `0x8B1177`), `total = данные +0x88` = UnpackTime (RW `0x8B116C`) |
| `0x8B1181` .. `0x8B119C` | иначе (PACKING): `from = данные +0xD0` (RW `0x8B1181`), `to = 1.0` (RW `0x8B1194`), `total = данные +0x84` = PackTime (RW `0x8B1189`) |
| `0x8B119F` .. `0x8B11C1` | `fild [esi+0x28]` (кадров осталось), `fild total`, обе беззнаковые с поправкой `+2^32` (DAT RW `0xBD8698`, RW `0x8B11AB` / `0x8B11BB`) |
| `0x8B11C1` .. `0x8B11D7` | x87 PC24: `fdivp` = left/total, `fsubr 1.0` = `q = 1 - left/total`, `fld to; fsub from; fmulp st(1); fadd from` → `(to - from) * q + from`, `fstp dword` в локальный `[ebp-8]` |
| `0x8B11DA` .. `0x8B11E1` | `mov ecx,[esi+8]` (объект), `call 0x70E013` (`mov eax,[ecx+0x84]; ret` = `Object::getDrawable`), `test eax,eax` — без drawable записи нет |
| `0x8B11E3` .. `0x8B11F0` | второй `call 0x70E013`, `movss [eax+0xB0], xmm0` — **drawable +0xB0 = вычисленная opacity** |
| `0x8B11F8` | `mov al,bl`: возвращает тот же busy-флаг, что базовый обработчик |

Отдельно от fade (конец способности):

| адрес | что делает |
|---|---|
| `0x8B1219` .. `0x8B1238` (`finishAbility`, vslot `0x34`) | `0x70E013` + `movss [eax+0xB0], 1.0` (DAT `0xBD1908`); затем `0x62684D(0x4A,0)` и базовый `0x8530BA` |
| `0x8B125F` .. `0x8B1287` (`finishAfterPacking`, vslot `0x4C`) | базовый `0x8549E7` RW `0x8B1263`, затем тот же `0x70E013` + `movss [eax+0xB0], 1.0` RW `0x8B127F`; на последнем кадре packing за ним сразу следует запись из `0x8B11F0` с `left = 0`, т.е. тоже 1.0 |

Важно: в отличие от `SpecialDisguiseUpdate`, RW **не хранит** opacity в состоянии модуля `ToggleMountedSpecialAbilityUpdate`; значение живёт в drawable `+0xB0` и пересчитывается каждый кадр из `+0x28`, `+0x30` и данных. В порте этому соответствует клиентский вызов `ObjectClientHooks::setDrawableOpacity` (`ClientEvent::SET_OPACITY` → `Drawable::setDrawableOpacity`, `Drawable +0xB0`); привязка клиентского хука к retail-обращению к drawable — **INFERENCE** (структура порта, lane SMOOTH-1), а не отдельная инструкция RW.

## Две соседние таблицы: кто их читает

### SummonReplacementSpecialAbilityUpdate (`0xC05AF8`, строка `0xC05B48`, `+0xE0`)

Цепочка: имя RW `0xC0B304` (`push` RW `0x659991`), фабрики `0x64F5AF` (данные, ctor `0x8B21DA`) и `0x64F574` (модуль, ctor `0x8B1F59`) в регистрации RW `0x6599B6`; парсер полей RW `0x64F559` берёт таблицу RW `0xC05AF8` (RW `0x64F569`). Конструктор RW `0x8B1F59` ставит vtable RW `0xC6C028` (RW `0x8B1F7B`).

- Слот `0x50` vtable `0xC6C028` = **базовый** `0x851E9F` (RW `0xC6C078`), fade-перекрытия нет.
- Конструктор данных RW `0x8B21DA` лишь обнуляет `+0xE0` (RW `0x8B2235`). Линейный разбор всех слотов vtable `0xC6C028` и кода класса RW `0x8B1E94` .. `0x8B2800` не нашёл ни одного чтения `+0xE0`: соседние `+0xE4` (`TriggerInstantlyOnCreate`), `+0xE5` (`CancelDisguiseWhenDismounting`), `+0xE8` (`MountedTemplate`), `+0xEC` (таймеры) читаются, `+0xE0` — нет.
- `finishAbility` RW `0x8B208A` (RW `0x8B2090` .. `0x8B20A9`) и `finishAfterPacking` RW `0x8B202A` (RW `0x8B2039` .. `0x8B2052`) только ставят drawable `+0xB0 = 1.0`, как и у ToggleMounted, но кривой нет.
- **Вердикт**: в RotWK 2.01 `OpacityTarget` этого класса парсится, но не читается. Никакого fade, состояния, `xfer`/CRC для него нет; порт делает то же (поле объявлено, не используется). См. стоп S-5608.

### SpecialDisguiseUpdate (`0xC05C10`, `+0xD4`)

Цепочка: имя RW `0xC0B050` (`push` RW `0x65A0A1`), фабрики `0x64F76E` (данные, ctor `0x8B4362`) и `0x64F733` (модуль, ctor `0x8B439F`) в регистрации RW `0x65A0C6`; парсер RW `0x64F718` берёт таблицу RW `0xC05C10` (RW `0x64F728`). Конструктор RW `0x8B439F` ставит vtable RW `0xC6CF88` (RW `0x8B43C4`) и обнуляет модуль `+0x8C` (RW `0x8B43DF`).

- Слот `0x50` vtable `0xC6CF88` = RW `0x8B4455` — тот же близнец `0x8B1142`: UNPACKING `1.0 → данные +0xD4` за `+0x88` (RW `0x8B4477` .. `0x8B448A`), PACKING `данные +0xD4 → 1.0` за `+0x84` (RW `0x8B4494` .. `0x8B44A7`), x87 PC24 RW `0x8B44B2` .. `0x8B44EA`, `fstp` в модуль `+0x8C`, затем drawable `+0xB0` (RW `0x8B44F0` .. `0x8B4513`).
- `finishAbility` RW `0x8B4521` возвращает модуль `+0x8C` и drawable `+0xB0` к 1.0 (RW `0x8B4545`).
- В порте (lane HERO-2) та же кривая уже считается в `SpecialDisguiseUpdate::m_opacity` (`SpecialAbilityModules.cpp`, RW `0x8B4455` в комментарии), но клиентское событие `SET_OPACITY` не шлётся. Задача #150 этот путь не меняет: только общий helper расчёта кривой. Неперенесённая доставка в drawable отмечена стопом S-5608.

## Итог для правки

1. У `ToggleMountedSpecialAbilityUpdate` есть доступный читатель `+0xD0`: RW `0x8B1142`, слот `0x50` vtable `0xC6BDA0`, вызываемый из update RW `0x854F7E`. Значит, код есть и состояние симуляции новое не нужно (RW хранит результат только в drawable).
2. Кривая: пока базовый `0x851E9F` вернул busy, UNPACKING `1.0 → OpacityTarget` за UnpackTime, PACKING `OpacityTarget → 1.0` за PackTime, линейно по `q = 1 - framesLeft/total` в арифметике x87 PC24; на последнем кадре unpacking записи нет, на последнем кадре packing и в `finishAbility` / `finishAfterPacking` — 1.0.
3. `SummonReplacementSpecialAbilityUpdate` не читает своё `+0xE0`, `SpecialDisguiseUpdate` читает `+0xD4` своей копией функции. Общий механизм — две близнецовые функции одного слота; править нужно слот ToggleMounted, рядом с уже работающим расчётом SpecialDisguise, не добавляя второй путь обработки кадров.
