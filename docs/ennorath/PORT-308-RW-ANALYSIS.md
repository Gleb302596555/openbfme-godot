# PORT-308 — разбор RotWK 2.01 (issue #700)

Источник: `workspace/RotWK/game.dat` (md5 `daed3668…`), адреса прочитаны `python tools/mod/rw.py … dis|xref` и дампом vtables. BFME2 1.06 (`/tmp/open-bfme-2`, клон `Open-BFME/Open-BFME-2`) использован только как донор имён/схемы; каждое перенесённое поведение подтверждено инструкцией RotWK.

## Классы и цепочки create → конструктор → vtable → слоты

| класс | create | конструктор данных / модуля | vtable | что дал линейный проход слотов |
|---|---|---|---|---|
| `LargeGroupBonusUpdate` | RW 0x64D0E0 | RW 0x893871 (данные), RW 0x893765 (модуль) | RW 0xC63AD8 (данные), RW 0xC63954 (модуль) | +0x00 deleting dtor RW 0x8937F4, +0x08 имя RW 0x8936B5, +0x0C xfer RW 0x8936C9, +0x10 key RW 0x893670, **+0x18 RW 0x893869 → RW 0x893810 (`notify(0)`)**; update вызывается как слот интерфейса, RW 0x8938D9 |
| `W3DLightDraw` | RW 0x4645EE | RW 0x004CF515 (данные), RW 0x004CF121 (модуль) | RW 0xBE37D0 (данные), RW 0xBE36D8 (модуль) | +0x00 RW 0x4CF5B6 → RW 0x4CF246 (dtor), +0x08 RW 0x4CF240 (имя), +0x0C RW 0x4CF488 (xfer), +0x10 RW 0x4CF4D0 (таблица полей), +0x2C RW 0x4CF2B5 (шаг мерцания), +0x40 RW 0x4CF29D (setEnabled), +0x98 RW 0x4CF5EE (reactToTransformChange) |
| `EvaAnnounceClientCreate` | RW 0x6527F1 | RW 0x8CDE22 (данные), RW 0x8CDE59 (модуль) | RW 0xC07A30 (данные), RW 0xC75980 (модуль) | +0x08 RW 0x8CDE8B (имя), +0x0C RW 0x8CDDD7 (xfer), +0x10 RW 0x8CDE91 (статическая инициализация имени), +0x2C RW 0x610FA7 (геттер `m_due == 0`), +0x30 RW 0x8CDF07 (clientUpdate), интерфейс +0x04 RW 0x8CDED6 (create) |

## Классификация «несцитированных» функций глубины 0

| функция | первая инструкция / что делает | статус |
|---|---|---|
| `LargeGroupBonusUpdate` RW 0x008267CB | `mov al,[ecx+0xA]; ret` — геттер байта чужого класса (около 0x826800, не vtable модуля) | не поведение нашего модуля |
| `LargeGroupBonusUpdate` RW 0x00893869 | `push 0; call 0x893810; ret` — слот +0x18 vtable RW 0xC63954, `notify(0)` | **поведение, не портировано** (S-1023): RW 0x893810 обходит `FlagSubObjectNames`, вызывает RW 0x672823 (`Drawable::showSubObject`, `visible=false`) и RW 0x672713 (`updateSubObjects`). DONOR BFME2 `LargeGroupBonusUpdate.cpp` (`notify(int)` / `rva004901B5`) подтверждает схему. Вызов идёт через общий слот модуля, порт такого слота не имеет — вывод за рамками задачи (общий механизм: PROCESS.md 5.7, условие «не общий механизм» не выполняется) |
| `W3DLightDraw` RW 0x004B612F | `push 0xDC62B8; call 0x435F30` — конструктор `D::StringBase` для строки имени класса | не поведение (boilerplate) |
| `W3DLightDraw` RW 0x004CF240 | `mov eax,0xBDBB08; ret` — `getModuleNameKey` | портировано как имя класса |
| `W3DLightDraw` RW 0x004CF246 | слот +0x00: снимает light, `call 0x442846`, обнуляет указатель +0xC, `call 0x7AEAFD` | портировано (`~W3DLightDraw`) |
| `W3DLightDraw` RW 0x004CF29D | слот +0x40: `setEnabled(!arg)` на light +0xC | портировано (`setEnabled` / `updatePersistent`) |
| `W3DLightDraw` RW 0x004CF2B5 | слот +0x2C: `phase += 1/[0xD9F608]`, порог мерцания, flicker/pulse, запись интенсивности в light +0xD0 | портировано (`stepFlicker`) |
| `W3DLightDraw` RW 0x004CF4D0 | слот +0x10: кэширует таблицу RW 0xBDBB08 через `0x5487EC` | портировано (`buildFieldParse`) |
| `W3DLightDraw` RW 0x004CF580 | deleting dtor данных | не поведение |
| `W3DLightDraw` RW 0x004CF5B6 / 0x004CF5D2 | deleting dtor модуля и данных | не поведение |
| `EvaAnnounceClientCreate` RW 0x00610FA7 | `al = ([ecx+0x10] == 0)` — геттер `m_due`, слот +0x2C | геттер без читателя-mechanism; портирован как `started()` (`m_started` — другой байт) |
| `EvaAnnounceClientCreate` RW 0x008CDE8B | `mov eax,0xC0A5B4; ret` — имя класса | портировано |
| `EvaAnnounceClientCreate` RW 0x008CDE91 | статическая инициализация intern-имени `0xC0A5B4` | не поведение |

## Поля с кандидатами reader

1. **`LargeGroupBonusUpdate.AlliesOnly` (data +0x18, 18 блоков мода).** Строка таблицы RW 0xC63A28, парсер RW 0x42E558, конструктор данных RW 0x893871 пишет default Yes (`mov byte [esi+0x18],1`, RW 0x8938B8). `update` RW 0x8938D9 строит фильтр RW 0xC10E14 → RW 0x660AFE (тот же контролирующий игрок) на RW 0x89395B..0x89396E и не читает data+0x18; RW 0x893810 (`notify`) обходит только вектор имён data+0x20; xfer RW 0x8936C9 читает module +0x24/+0x28/+0x29/+0x2a; интерфейс RW 0xC6387C / 0xC63898 не обращаются к data+0x18. DONOR BFME2 (`LargeGroupBonusUpdate.cpp`) использует тот же фильтр владельца и тоже не читает поле. STOP S-7291 (Port 110) уже закрыт вердиктом «читателя нет». ВНИМАНИЕ: default port'а был `false`, тогда как RW пишет `true`; это исправлено в PORT-308 с тестом. В 20 блоках мода поле задано явно, поэтому поведение Эннората не меняется.
2. **`W3DLightDraw.Radius` (data +0x2C, 38 блоков).** `buildFieldParse` RW 0x4CF4D0 → таблица RW 0xBDBB08 → `Radius` в +0x2C; конструктор данных RW 0x4CF515 пишет 20.0f (`movss [eax+0x2C],xmm1`, RW 0x4CF52C). Читатель — конструктор модуля RW 0x4CF121: `fld dword [edi+0x2C]` (RW 0x4CF208) → `fstp [eax+0x104]` (RW 0x4CF216), константа 1.0f RW 0x4CF20E → `+0x100` (RW 0x4CF21C), флаг FAR_ATTENUATION RW 0x4CF227. Кандидаты RW 0x4CF29D / RW 0x4CF2B5 — это слоты +0x40 / +0x2C того же модуля, но они читают data +0x30/+0x34/+0x38/+0x3C/+0x40 (Intensity и flicker/pulse), не Radius; RW 0x442846 / RW 0x4CF246 / RW 0x4CF580 — dtor и освобождает light. Порт: `W3DLightDraw::lightRadius()` → `LightPulsePool::addPersistent(..., farAttenStart = 1.0f, farAttenEnd = radius, farAttenuation = true)` (RW 0x4CF208, 0x4CF21C, 0x4CF227).
3. **`EvaAnnounceClientCreate.CreateFakeRadarEvent` (data +0x1B, 32 блока).** VERDICT S-7212 (Port 60, issue #170, `docs/ennorath/port60-evaannounce-rw.md`) подтверждён повторно: clientUpdate RW 0x8CDF07 читает +0x08/+0x0C/+0x10 (EVENT) и +0x18/+0x19/+0x1A, create RW 0x8CDED6 читает +0x14, xfer RW 0x8CDDD7 не трогает ModuleData, полный линейный проход `.text` находит только запись default RW 0x8CDE44. DONOR BFME2 — только регистрация парсера. Порт разбирает поле и не создаёт радар-событие; поведение совпадает.

## INFERENCE / что осталось

- RW 0x893810 (`notify(0)`) может вызываться через общий слот модуля вне статически видимых вызовов: конкретный вызывающий не доказан, эффект — скрытие субобъектов на клиенте (S-1023). Не переносится: это общий механизм вызова слотов, а не поле/ветка трёх задач.
- Всё остальное из глубины 0 — геттеры, name getters, deleting dtor'ы и конструкторы строк; поведения в них нет.
