# Партия 3: 30 полей из задачи [#12](https://github.com/Gleb302596555/openbfme-godot/issues/12)

Задача: по каждому полю ответить, читает ли его RotWK (`game.dat` 2.01) и какая подсистема, чтобы
решить, портировать поле или записать стоп. Порядок строк — как в таблице задачи.

Инструмент: `tools/mod/rw.py` (`dis`, `xref`) плюс вспомогательные прогоны из
`workspace/scratch/` (`rw.py` — образ и таблицы полей, `scan2.py`/`final.py` — читатели по таблице и
смещению, `rare.py` — редкие члены, `anchor.py` — связка «парсер → конструктор»).
Образ: `workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, ImageBase `0x400000`.

Правило отчёта: адрес `RotWK 0xADDR` — прочитанная инструкция; вывод по аналогии или по именам
помечен `INFERENCE`; где чтения нет — `не определено`.

## Сводка

| # | поле | кто разбирает | читает ли RotWK | доказательство |
|---|---|---|---|---|
| 1 | `VisionSideOverride` | `0xC5BB50` +0x278, `0x42EEFA` | **читает (логика/клиент)** | аксессор `0x872BAB` `fld [eax+0x278]` в слоте 151 (байт +0x25C) vtable `0xC5B1F8`, `0xC5BE98`, `0xC5CFA8`; вызовы слота: `0x694B79` (`call [eax+0x25C]; fstp [ebp+0x14]`), `0x6641EF`, `0x65BE37` |
| 2 | `ShadowOpacityEnd` | `0xBE0A78` +0xF0, `0x42ECB2` | **читает (клиент)** | `0x4C4F83` `push [eax+0xF0]` (→ `0xA3CFA4`, float→int) и `0x4C4F7D` `fmul [eax+0xEC]` в `0x4C4D49` |
| 3 | `ShadowOpacityFadeInTime` | `0xBE0A78` +0xE4, `0x42ED00` | **читает (клиент)** | `0x4C4F98` `fmul [eax+0xE4]` — тот же блок |
| 4 | `ShadowOpacityFadeOutTime` | `0xBE0A78` +0xEC, `0x42ED00` | **читает (клиент)** | `0x4C4F7D` `fmul [eax+0xEC]` |
| 5 | `ShadowOpacityStart` | `0xBE0A78` +0xE0, `0x42ECB2` | **читает (клиент)** | `0x4C4F69` `push [eax+0xE0]` (→ `0x73228B`) и `0x4C4FB2` `push [eax+0xE0]` (→ `0x7323CE`) |
| 6 | `HasSuspension` | `0xBF43E8` +0xDC, `0x42E558` | **не читает** | обращения только в конструкторе `0x5E4326`, копиях `0x5E31DF`/`0x5E4653`, разборе `0x5E3D02`; геттер `0x6701E4` нигде не адресуется |
| 7 | `ShowPips` | `0xC59F30` +0x80, `0x42E558` | **читает (клиент)** | `0x5F4F97` `mov al, [esi+0x80]` на структуре контейнера (4 из 7 редких членов `0x7D..0x85`), `0x852858` `lea ecx,[edi+0x80]` (3 из 3) |
| 8 | `CrushDamageWeaponTemplate` | `0xC72E18` +0x68, `0x73AE79` | **не определено** | таблица из 3 строк, смещения 0x64/0x68 встречаются в 778/842 функциях — precision не работает |
| 9 | `DamageWeaponTemplate` | `0xC72E18` +0x64, `0x73AE79` | **не определено** | то же |
| 10 | `VoiceEnterStateInitiateCaptureBuilding` | — | **RotWK не использует** | строки в образе нет; в таблицах `0xC75D44`/`0xC75E20` поля нет |
| 11 | `ParticleSystem` | `0xBF6E60` +0x24, `0x42EE5E` | **не определено** | `0x5F0689` покрывает 40/40, но это конструктор/копия; настоящих читателей по классу не выделить |
| 12 | `PitchDamping` | `0xBF43E8` +0x98, `0x42ED00` | **не читает** | геттер `0x6700E6` — 0 ссылок |
| 13 | `PitchStiffness` | `0xBF43E8` +0x90, `0x42ED00` | **не читает** | геттер `0x6700D2` — 0 ссылок |
| 14 | `RollDamping` | `0xBF43E8` +0x9C, `0x42ED00` | **не читает** | геттер `0x6700F0` — 0 ссылок |
| 15 | `RollStiffness` | `0xBF43E8` +0x94, `0x42ED00` | **не читает** | геттер `0x6700DC` — 0 ссылок |
| 16 | `Cursor` | `0xC0FD68` +0x74, `0x42EE5E` | **не определено** | копия `0x66FA3C` (26/26), запись в конструкторе `0x66F888` (`0x66F909`); потребителя нет |
| 17 | `ForwardAccelerationPitchFactor` | `0xBF43E8` +0xB0, `0x42ED00` | **не читает** | геттер `0x670104` — 0 ссылок |
| 18 | `LateralAccelerationRollFactor` | `0xBF43E8` +0xB4, `0x42ED00` | **не читает** | геттер `0x67010E` — 0 ссылок |
| 19 | `MorphCondition` | `0xC66090` +0x18, `0x89A90F` (BannerCarrierUpdate) | **не определено** | чтения только в полных копиях блока: `0x4B3ECD` (12/12), `0x72B84F` (12/12) |
| 20 | `CashBackPercent` | `0xC5EFE8` +0xD4, `0x42EEFA` | **не читает** | `0x443939` держит `[esi+0xD4..0xDC]` как таймер по `[esi+0xC]`/`[esi+0x10]` (см. ниже) — это не шаблон |
| 21 | `ApproachUntilMembersInRange` | `0xC55960` +0xC5, `0x42E558` | **читает (логика)** | `0x8C3465` `lea ecx,[edi+0xC5]` (fn `0x8C3388`, слот `0xC71B78`), `0x85214D` `cmp byte [eax+0xC5],0`, `0x97BBF7` `cmp byte [esi+0xC5],0` |
| 22 | `BeingBuiltSound` | `0xC306B8` +0x14, `0x73B217` | **не определено** | чтений с признаками класса CastleMemberBehavior не найдено |
| 23 | `CanSwoop` | `0xC16DD8` +0x83, `0x42E558` | **не читает** | обращения только у копий `0x6CE39C`, `0x6CDE89`, `0x5FBE28`; `0x852858` `lea ecx,[edi+0x83]` — `INFERENCE` |
| 24 | `TrackMarks` | `0xBE11C0` +0x3C, `0x4B65BA` | **читает (клиент)** | `0x4C4E6C` `lea ecx,[eax+0x3C]` + `call 0x401E64` (непустой список), далее `0x4C4EA0` `mov eax,[eax+0x3C]` |
| 25 | `TrackMarksLeftBone` | `0xBE11C0` +0x44, `0x42EE5E` | **читает (клиент)** | `0x4C4E92` `mov ecx,[eax+0x44]` + `add ecx,8` (строка → char*) |
| 26 | `TrackMarksRightBone` | `0xBE11C0` +0x48, `0x42EE5E` | **читает (клиент)** | `0x4C4E7B` `mov ecx,[eax+0x48]` + `add ecx,8` |
| 27 | `Ambient` | `0xBE3638` +0x8, `0x42EF99` (W3DLightDraw) | **читает (клиент)** | `0x4CF9C1` `fdivr [ebx+8]` в `0x4CF884` — доля пульсации; конструктор `0x4CF515` `0x4CF543` `movss [eax+8], xmm0` |
| 28 | `Diffuse` | `0xBE3638` +0x14, `0x42EF99` | **читает (клиент)** | `0x4CFA2C` `movss xmm0,[ebx+0x14]` + `mulss xmm0,[ebp-4]` (яркость) → `0x567773`; конструктор `0x4CF552` |
| 29 | `UseTerrainSmoothing` | `0xBF43E8` +0xED, `0x42E558` | **читает (клиент)** | `0x6701E7` (в `0x67012C`) и `0x670586` (в `0x670570`) `mov al, byte [eax+0xED]`; вызовы `0x676EB0`, `0x67713B` — `INFERENCE`: отрисовка/посадка на рельеф |
| 30 | `ShadowOffsetX` | `0xBE0A78` +0x14 / `0xDA3DF8` +0x500 | **читает (клиент)** | `0x4B93AD` `movss xmm0,[esi+0x500]` (объектный шаблон); в `0xBE0A78` не читается |

Счёт: **читает** — 12 (1–5, 7, 21, 24–30), **не читает** — 9 (6, 12–15, 17, 18, 20, 23),
**не определено** — 6 (8, 9, 11, 16, 19, 22), **RotWK не использует** — 1 (10).
По `ShadowOffsetX` разобраны оба смещения: объектное читается, в `ModelConditionState` — нет.

## Что это значит для порта

- **Портится (видимое клиенту)**: тени (`ShadowOpacity*`, `ShadowOffsetX`), следы колёс
  (`TrackMarks*`), свет (`Ambient`, `Diffuse`), `VisionSideOverride` (сторона обзора/туман),
  `ShowPips` (значки контейнера), `UseTerrainSmoothing` (посадка на рельеф),
  `ApproachUntilMembersInRange` (логика спецспособности).
- **Можно писать стоп «RotWK тоже не использует»**: `PitchStiffness`, `PitchDamping`,
  `RollStiffness`, `RollDamping`, `ForwardAccelerationPitchFactor`, `LateralAccelerationRollFactor`,
  `HasSuspension` (7 полей физики локомотора), `CashBackPercent`, `CanSwoop` — с адресами геттеров
  и доказательством, что на них нет ссылок.
- **Ждёт решения владельца (`не определено`)**: `CrushDamageWeaponTemplate`/`DamageWeaponTemplate`,
  `ParticleSystem`, `Cursor`, `MorphCondition`, `BeingBuiltSound` — потребитель не прослежен;
  нужно либо разбирать реестр модулей, либо писать стоп `S-<номер>`.

## Подробно по спорным полям

### Локомотор: почему «не читает»

Таблица `0xBF43E8` плотная: 88 строк на смещениях `0x14..0x151` шагом 4 байта. Поэтому подсчёт
«сколько членов таблицы задел регистр» ничего не доказывает — почти любая структура такого размера
попадает в 40–60 смещений. Доказательство строится иначе.

1. Все обращения к членам `0x88..0xED` внутри `.text` лежат в пяти местах: конструктор `0x5E4326`,
   копии `0x5E31DF` и `0x5E4653`, разбор `0x5E3D02` (`0x5E3E29`, `0x5E3FED`) и присваивания по
   умолчанию в `0x5E44xx`.
2. В `0x6700BE .. 0x6701E9` подряд стоит блок аксессоров `mov eax,[ecx+4]; fld [eax+0xOFF]; ret`
   для 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA8, 0xB0, 0xB4, 0xBC, 0xDC, 0xE0, 0xED; рядом
   `0x670122` (0xDC) и `0x6701E4` (0xED) — байтовые варианты.
3. Указателей на эти аксессоры в образе **нет ни одного** (поиск little-endian по всему
   `game.dat`): их не зовут ни прямым `call`, ни через таблицу. Единственный способ попасть в них —
   вычисленный адрес, которого в образе нет.

Вывод: поля локомотора заполняются, копируются и разбираются, но не читаются.
`UseTerrainSmoothing` (`0xED`) — исключение, см. #29.

### Тени: два разных набора смещений

**`ModelConditionState` (`0xBE0A78`, 24 члена)**: `ShadowOpacityStart/FadeIn/Peak/FadeOut/End` =
`0xE0/0xE4/0xE8/0xEC/0xF0`. Пишут парсеры `0x42ECB2` (uint) и `0x42ED00` (real). Читает рендер
тени в `0x4C4D49`:

```
004C4F69  push  [eax+0xe0]        ; ShadowOpacityStart -> 0x73228B
004C4F7A  mov   eax, [ebp+8]
004C4F7D  fmul  [eax+0xec]        ; FadeOutTime * 1/fps
004C4F83  push  [eax+0xf0]        ; ShadowOpacityEnd  -> 0xA3CFA4 (float->int)
004C4F98  fmul  [eax+0xe4]        ; FadeInTime  * 1/fps
004C4FA0  push  [eax+0xe8]        ; ShadowOpacityPeak -> 0xA3CFA4
004C4FB2  push  [eax+0xe0]        ; ShadowOpacityStart
004C4FBC  call  0x7323CE          ; общий сеттер времени появления/исчезания тени
```

Отрисовка состояния — `0x4B92D3` (читает `0x4F8/0x4FC/0x500/0x504/0x510/0x514` и `0x5F2`,
зовёт `0x49A2EB`).

**Объектный шаблон (`0xDA3DF8`, 170 членов)**: `ShadowOpacityStart/Peak/End` =
`0x5BC/0x5C0/0x5C4`, `ShadowSizeX/Y` = `0x4F8/0x4FC`, `ShadowOffsetX/Y` = `0x500/0x504`,
`FadeIn/OutTime` = `0x508/0x50C`, `SunAngle` = `0x510`, `MaxHeight` = `0x514`. Единственный
настоящий читатель — `0x6D1D80`: `0x6D2433` `[eax+0x5BC]`, `0x6D244B` `[eax+0x5C4]`,
`0x6D2217` `[eax+0x508]`, `0x6D2223` `[eax+0x50C]` (далее копия в `[ebx+…]`). Второй экземпляр
блока — `0x73FB7A` (только запись).

Для теней нужны **оба** набора: состояние модели и объект, иначе картинка разойдётся с оригиналом.

### `ShowPips` (#7)

Таблица `0xC59F30` (23 члена, OpenContain/HordeContain). По редким членам (`0x7D..0x85`):
`0x867E1B` (7 из 7 редких) и `0x851D42` (6 из 7) — конструкторы, только запись;
`0x5F4F97` — реальное чтение `mov al,[esi+0x80]`, у него 4 из 7 редких членов контейнера;
`0x852858` — `lea ecx,[edi+0x80]`, 3 из 3. Функция `0xB3E780` (`mulss xmm0,[esi+0x80]`, 4 чтения)
работает с другой структурой: у неё `[esi+0x4..0xAC]` заполнено без пропусков, а у контейнера
`0x40..0x58` — не так. Значит чтение есть, но рисование значка подтверждается только через
`INFERENCE` (по смыслу `ShowPips`), а не одним адресом с классом.

### `VisionSideOverride` (#1)

`0xC5BB50` +0x278 (43 члена, HordeContain/AODHordeContain/HorseHordeContain), парсер `0x42EEFA`;
пишет конструктор `0x878EE5` (`0x8790C8` `movss [esi+0x278]`). Читает аксессор:

```
00872BAB  mov eax, dword ptr [ecx - 0x118]
00872BB1  fld dword ptr [eax + 0x278]
00872BB7  ret
```

Слот **151** (байт `+0x25C`) в vtable `0xC5B1F8` (295 слотов), `0xC5BE98` (295), `0xC5CFA8` (273).
Вызовы слота: `0x694B79` (`mov eax,[esi]; mov ecx,esi; call [eax+0x25C]; fstp [ebp+0x14]` —
результат используется как `float`), `0x6641EF` (`call [eax+0x25C]; test al,al`), `0x65BE37`.
Соседние аксессоры того же блока: `0x872B9E` (0x274), `0x872BB8` (0x238).

### `TrackMarks*` (#24–26)

Один блок в `0x4C4D49`, шаблон берётся как `[esi+4]`:

```
004C4E69  mov  eax, [esi+4]        ; template
004C4E6C  lea  ecx, [eax+0x3c]     ; TrackMarks (list)
004C4E6F  call 0x401E64            ; список непуст?
004C4E74  test al, al
004C4E78  mov  eax, [esi+4]
004C4E7B  mov  ecx, [eax+0x48]     ; TrackMarksRightBone
004C4E87  add  ecx, 8              ; char* из игровой строки
004C4E92  mov  ecx, [eax+0x44]     ; TrackMarksLeftBone
004C4E99  add  ecx, 8
004C4EA0  mov  eax, [eax+0x3c]     ; TrackMarks
```

Таблица W3DTruckDraw `0xBE11C0` подтверждает смещения: `TrackMarks` +0x3C (`0x4B65BA`),
`TrackMarksLeftBone` +0x44 и `TrackMarksRightBone` +0x48 (`0x42EE5E`).

### `Ambient` / `Diffuse` (#27–28): цепочка до рендера

`Ambient` +0x8 и `Diffuse` +0x14 живут в шаблоне модуля `W3DLightDraw`, таблица `0xBE3638`.
Конструктор данных модуля `0x4CF515` заполняет весь блок:

```
004CF522  mov   dword ptr [eax], 0xbe37d0    ; vtable W3DLightDraw
004CF543  movss dword ptr [eax + 8], xmm0    ; Ambient   = 0
004CF552  movss dword ptr [eax + 0x14], xmm0 ; Diffuse   = 0
004CF557  movss dword ptr [eax + 0x28], xmm0 ; Specular  = 0
004CF52C  movss dword ptr [eax + 0x2c], xmm1 ; Radius
```

Читает их функция обновления `0x4CF884` (экземплярная часть, `ebx = [esi+4]` — шаблон):

* `0x4CF9C1` `fdivr dword ptr [ebx+8]` — `Ambient` участвует в расчёте доли пульсации яркости
  (рядом `fild [ebx+0x20]`, `fld/fcomip`, ветка на `0x567F18`);
* `0x4CFA2C` `movss xmm0,[ebx+0x14]` (`Diffuse`), `0x4CFA4C` `[ebx+0x18]`, `0x4CFA56` `[ebx+0x1C]`
  — три компоненты цвета умножаются на текущую яркость `[ebp-4]` и уходят в `0x567773`
  (объект-источник света), после чего `fld1` (полная яркость).

Сам `0x4CF884` вызывается из тонких обёрток `0x4CFC4C` и `0x4CFC54` — это слоты `+0x0AC` и
`+0x0A8` vtable экземпляра `0xBE37D0`. Значит, свет читается и передаётся в отрисовку.

### `CashBackPercent` (#20): почему «не читает»

`0x443939` — единственный кандидат с настоящим чтением `[esi+0xD4]`:

```
0044393F  cmp  byte ptr [esi+0xd8], 0
00443946  movss xmm1, dword ptr [esi+0xd4]
00443968  sub  eax, dword ptr [esi+0xdc]     ; eax = параметр - [esi+0xdc]
00443973  fild dword ptr [ebp+8]
0044397E  fmul dword ptr [0xbd88a0]
00443984  fst   dword ptr [esi+0xd4]         ; накопление
```

Это структура прогресса по времени (`[esi+0xC]`, `[esi+0x10]`, `[esi+0xD4..0xDC]`), а не
`SlaughterHordeContain` из `0xC5EFE8`; совпадение смещения 0xD4 — случайное. Значения
`CashBackPercent` из INI никто не читает.

### `Cursor` (#16), `ParticleSystem` (#11), `MorphCondition` (#19), `BeingBuiltSound` (#22)

Во всех четырёх случаях найденные обращения — конструктор, копия и разбор:
`0x66FA3C` (26/26, копия `UpgradeTemplate`), `0x66F888` (`0x66F909` `mov [esi+0x74]`),
`0x5F0689` (40/40, `ParticleSystem`), `0x4B3ECD` и `0x72B84F` (12/12, полные копии блока
`BannerCarrierUpdate`), `0x89A765` (разбор). Ни одна из них не потребляет значение. Смещения
`0x64/0x68` (`PorcupineFormationBodyModule`) встречаются в 778/842 функциях — precision по ним не
работает. Вердикт `не определено`, а не «не использует»: отрицательный результат здесь не доказан.

## Осталось

- Довести до потребителя 6 полей `не определено` (8, 9, 11, 16, 19, 22): нужен разбор реестра
  данных модулей, а не поиск по смещению — это отдельная задача.
- По 9 полям «не читает» нужно решение владельца: писать стоп `RotWK тоже не использует` с
  адресами геттеров (или проверять в игре, если есть подозрение на вычисленный адрес).
- В порт ничего не внесено: это отчёт об исследовании, кода он не меняет.
