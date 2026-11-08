# Партия 3: 30 полей из задачи [#12](https://github.com/Gleb302596555/openbfme-godot/issues/12)

Задача: по каждому полю ответить, читает ли его RotWK (`game.dat` 2.01) и какая подсистема, чтобы
решить, портировать поле или записать стоп. Порядок строк — как в таблице задачи.

Инструмент: `workspace/scratch/rw.py` (образ, таблицы полей, `dis`/`xref`/`rows`) и
`workspace/scratch/calc.py` (трассировка «регистр = указатель на шаблон» по `mov rX,[rY+4]` с
проверкой чтения `[rX+offset]`). Образ: `workspace/RotWK/game.dat`, md5
`daed3668006cd90f01c34e5a7da1901f`, ImageBase `0x400000`, `.text` `0x401000..0xBD0000`.

Правило отчёта: адрес `RotWK 0xADDR` — прочитанная инструкция; вывод по аналогии или по именам
помечен `INFERENCE`; где чтения не доказано — `не определено`.

Правило доказательства (по замечаниям ревью): **отсутствие ссылок на геттер ничего не доказывает** —
компилятор встраивает геттеры, а читатель берёт член шаблона напрямую через `[this+N]->field`.
Поэтому вердикт «читает» ставится только по прочитанной инструкции чтения внутри функции, которая
держит шаблон модуля (цепочка create → ctor → vtable → слот), а «не определено» — когда такая
цепочка не прослежена. Вердикт «RotWK не использует» ставится только при отсутствии самого поля.

## Сводка

| # | поле | кто разбирает | читает ли RotWK | доказательство |
|---|---|---|---|---|
| 1 | `VisionSideOverride` | `0xC5BB50` +0x278, `0x42EEFA` | **читает (клиент)** | `0x872BAB` `mov eax,[ecx-0x118]`; `0x872BB1` `fld [eax+0x278]`; слот 151 (байт +0x25C) vtable `0xC5B1F8`/`0xC5BE98`/`0xC5CFA8`, вызовы `0x694B79`, `0x6641EF`, `0x65BE37` |
| 2 | `ShadowOpacityEnd` | `0xBE0A78` +0xF0, `0x42ECB2` | **читает (клиент)** | `0x4C4F83` `push [eax+0xF0]` → `0xA3CFA4` (float→int) в `0x4C4D49` |
| 3 | `ShadowOpacityFadeInTime` | `0xBE0A78` +0xE4, `0x42ED00` | **читает (клиент)** | `0x4C4F98` `fmul [eax+0xE4]` |
| 4 | `ShadowOpacityFadeOutTime` | `0xBE0A78` +0xEC, `0x42ED00` | **читает (клиент)** | `0x4C4F7D` `fmul [eax+0xEC]` |
| 5 | `ShadowOpacityStart` | `0xBE0A78` +0xE0, `0x42ECB2` | **читает (клиент)** | `0x4C4F69` `push [eax+0xE0]` → `0x73228B`; `0x4C4FB2` `push [eax+0xE0]` → `0x7323CE` |
| 6 | `HasSuspension` | `0xBF43E8` +0xDC, `0x42E558` | **читает (клиент)** | `0x6770DA` `mov al,[eax+0xDC]` в `0x677026` (ветка Appearance 1/8 — FOUR_WHEELS/TREADS); флаг проверяется в `0x677182` и `0x677630` |
| 7 | `ShowPips` | `0xC59F30` +0x80, `0x42E558` | **читает (клиент)** | `0x5F505B` `mov al,[esi+0x80]`; `0x852858`/`0x852860` `lea ecx,[edi+0x80]` |
| 8 | `CrushDamageWeaponTemplate` | `0xC72E18` +0x68, `0x73AE79` | **не определено** | смещение 0x68 слишком частое для precision; цепочка до потребителя не прослежена |
| 9 | `DamageWeaponTemplate` | `0xC72E18` +0x64, `0x73AE79` | **не определено** | то же |
| 10 | `VoiceEnterStateInitiateCaptureBuilding` | — | **RotWK не использует** | строки в образе нет; в таблицах `0xC75D44`/`0xC75E20` поля нет |
| 11 | `ParticleSystem` | `0xBF6E60` +0x24, `0x42EE5E` | **не определено** | `0x5F0689` покрывает 40/40, но это конструктор/копия; читателя по классу не выделить |
| 12 | `PitchDamping` | `0xBF43E8` +0x98, `0x42ED00` | **читает (клиент)** | `0x670B72`, `0x673354`, `0x6770AB`, `0x67B81A` — `movss xmm,[eax+0x98]`, где `eax=[arg+4]` — шаблон локомотора |
| 13 | `PitchStiffness` | `0xBF43E8` +0x90, `0x42ED00` | **читает (клиент)** | `0x670B62`, `0x67333A`, `0x677091`, `0x67B800` — `movss xmm,[eax+0x90]` |
| 14 | `RollDamping` | `0xBF43E8` +0x9C, `0x42ED00` | **читает (клиент)** | `0x670B7A`, `0x673361`, `0x6770B8`, `0x67B827` — `movss xmm,[eax+0x9C]` |
| 15 | `RollStiffness` | `0xBF43E8` +0x94, `0x42ED00` | **читает (клиент)** | `0x670B6A`, `0x673347`, `0x67709E`, `0x67B80D` — `movss xmm,[eax+0x94]` |
| 16 | `Cursor` | `0xC0FD68` +0x74, `0x42EE5E` | **не определено** | копия `0x66FA3C` (26/26), запись в ctor `0x66F888` (`0x66F909` `mov [esi+0x74]`); потребителя нет |
| 17 | `ForwardAccelerationPitchFactor` | `0xBF43E8` +0xB0, `0x42ED00` | **читает (клиент)** | `0x67337B` `movss xmm0,[eax+0xB0]` → `[ebp-0x30]` → `0x6736E0` `mulss xmm0,[ebp-0x30]` → `0x6736E9` `movss [ecx+0x1C]` (`ecx = DrawableLocoInfo`), затем используется дальше по функции |
| 18 | `LateralAccelerationRollFactor` | `0xBF43E8` +0xB4, `0x42ED00` | **читает (клиент)** | `0x673388` `movss xmm0,[eax+0xB4]` → `[ebp-0x34]` → `0x673752` `mulss xmm0,[ebp-0x34]` → `0x673757` `movss xmm1,[eax+0x24]`/`subss` (вклад в крен) |
| 19 | `MorphCondition` | `0xC66090` +0x18, `0x89A90F` | **не определено** | чтения только в полных копиях: `0x4B3ECD` (12/12), `0x72B84F` (12/12) |
| 20 | `CashBackPercent` | `0xC5EFE8` +0xD4, `0x42EEFA` | **не определено** | слот 0 vtable `0xC5F5F0` читает `+0xD4`, но как `mov edi,[ebx+0xD4]` и сравнивает с `+0xD8` — это вектор другого класса (`0x88460B`), не `SlaughterHordeContain`; потребитель float `+0xD4` не найден и не опровергнут |
| 21 | `ApproachUntilMembersInRange` | `0xC55960` +0xC5, `0x42E558` | **читает (логика)** | `0x8C3465` `lea ecx,[edi+0xC5]`; `0x85214D` `cmp byte [eax+0xC5],0`; `0x97BBF7` `cmp byte [esi+0xC5],0` |
| 22 | `BeingBuiltSound` | `0xC306B8` +0x14, `0x73B217` | **не определено** | чтений с признаками класса Castle не найдено |
| 23 | `CanSwoop` | `0xC16DD8` +0x83, `0x42E558` | **не определено** | геттер `0x6CA152` `mov al,[ecx+0x83]` — 0 ссылок; других чтений `+0x83` у `WeaponTemplate` не найдено, но и функции-потребителя не прослежено (см. ниже) |
| 24 | `TrackMarks` | `0xBE11C0` +0x3C, `0x4B65BA` | **читает (клиент)** | `0x4C4E6C` `lea ecx,[eax+0x3C]` + `call 0x401E64`; `0x4C4EA0` `mov eax,[eax+0x3C]` |
| 25 | `TrackMarksLeftBone` | `0xBE11C0` +0x44, `0x42EE5E` | **читает (клиент)** | `0x4C4E92` `mov ecx,[eax+0x44]` + `add ecx,8` |
| 26 | `TrackMarksRightBone` | `0xBE11C0` +0x48, `0x42EE5E` | **читает (клиент)** | `0x4C4E7B` `mov ecx,[eax+0x48]` + `add ecx,8` |
| 27 | `Ambient` | `0xBE3638` +0x8, `0x42EF99` | **читает (клиент)** | `0x4CF9C1` `fdivr [ebx+8]` в `0x4CF884`; ctor `0x4CF543` `movss [eax+8]` |
| 28 | `Diffuse` | `0xBE3638` +0x14, `0x42EF99` | **читает (клиент)** | `0x4CFA2C` `movss xmm0,[ebx+0x14]` + `mulss xmm0,[ebp-4]` → `0x567773` |
| 29 | `UseTerrainSmoothing` | `0xBF43E8` +0xED, `0x42E558` | **читает (клиент)** | `0x670586` `mov al,[eax+0xED]` в `0x670570`, где `eax=[ebp+0xC]`, затем `[eax+4]` — шаблон; вызовы `0x676EB0`, `0x67713B` |
| 30 | `ShadowOffsetX` | `0xBE0A78` +0x14 / `0xDA3DF8` +0x500 | **читает (клиент)** | `0x4B93AD` `movss xmm0,[esi+0x500]` (объектный шаблон); в `0xBE0A78` не читается |

Счёт: **читает** — 21 (1–7, 12–15, 17, 18, 21, 24–30), **не определено** — 8 (8, 9, 11, 16, 19, 20, 22, 23),
**RotWK не использует** — 1 (10). Ни одного вердикта «не читает» не осталось.

По `ShadowOffsetX` разобраны оба смещения: объектное читается, в `ModelConditionState` — нет.

## Что это значит для порта

- **Портится (видимое клиенту)**: тени (`ShadowOpacity*`, `ShadowOffsetX`), следы колёс
  (`TrackMarks*`), свет (`Ambient`, `Diffuse`), `VisionSideOverride`, `ShowPips`,
  `UseTerrainSmoothing` (посадка на рельеф), `ApproachUntilMembersInRange` (логика
  спецспособности), и **физика подвески локомотора**: `PitchStiffness`, `RollStiffness`,
  `PitchDamping`, `RollDamping`, `HasSuspension` (ветка FOUR_WHEELS/TREADS), а также
  `ForwardAccelerationPitchFactor`/`LateralAccelerationRollFactor` (ветка SHIP).
- **Ждёт решения владельца (`не определено`)**: `CrushDamageWeaponTemplate`/`DamageWeaponTemplate`,
  `ParticleSystem`, `Cursor`, `MorphCondition`, `BeingBuiltSound`, `CashBackPercent`, `CanSwoop`.
  Для них нужен разбор реестра данных модулей (create → ctor → vtable → слот → чтение члена),
  а не поиск по смещению: отрицательный результат здесь не доказан.

## Подробно по спорным полям

### Физика локомотора (#6, #12–15, #17, #18): читателя нашёл дизасм, а не геттеры

Прежняя версия отчёта опиралась на поиск ссылок на геттеры `0x6700BE..0x6701E9` и на этом
основании объявляла поля нечитаемыми. Это ошибка: геттеры встраиваются, а значения берутся прямо
из шаблона локомотора. Настоящий читатель — семейство `Drawable::calcPhysicsXform*`, на которое
переключается диспетчер `0x67BDC0` по `Appearance` (`[template+0x74]`):

```
0067BDFF  call  dword ptr [eax + 0x7c]       ; TheGameClient->...
0067BE02  mov   ebx, eax
0067BE1D  mov   eax, [esi+4]                 ; esi = Locomotor, [+4] = LocomotorTemplate*
0067BE20  mov   eax, [eax+0x74]              ; Appearance
0067BE23  cmp   eax, 1   je 0x67BE77         ; FOUR_WHEELS -> 0x677026
0067BE2A  cmp   eax, 3   jle 0x67BE6A        ; 2,3 (HOVER, WINGS) -> 0x670B11
0067BE2F  cmp   eax, 4   je 0x67BE5D         ; FOUR_LEGS_HUGE -> 0x67B7AA
0067BE34  cmp   eax, 5   je 0x67BE50         ; GIANT_BIRD -> 0x670E09
0067BE39  cmp   eax, 8   je 0x67BE77         ; TREADS -> 0x677026
0067BE3E  cmp   eax, 9   jne 0x67BE86        ; SHIP -> 0x6732DC
0067BE43  ...            call 0x6732DC
```

Индексы `Appearance` взяты из таблицы имён `0xD9DEEC`: 0 `TWO_LEGS`, 1 `FOUR_WHEELS`, 2 `HOVER`,
3 `WINGS`, 4 `FOUR_LEGS_HUGE`, 5 `GIANT_BIRD`, 6 `HORDE`, 7 `HUGE_TWO_LEGS`, 8 `TREADS`, 9 `SHIP`.
Диспетчер вызывается из `0x67BF39` (`call 0x67BDC0`), то есть из `Drawable::applyPhysicsXform`.

Собственные чтения шаблона по функциям (трассировка `calc.py`: регистр помечается как указатель на
шаблон после `mov rX,[rY+4]`, доступ снимается при перезаписи регистра):

| функция | Appearance | BFME2-двойник | читаемые члены шаблона |
|---|---|---|---|
| `0x677026` | 1, 8 (FOUR_WHEELS, TREADS) | `calcPhysicsXformWheels` 0x276CFB | `+0x88`, `+0x8C`, `+0x90`, `+0x94`, `+0x98`, `+0x9C`, `+0xB8`, `+0xDC`, `+0xE0` |
| `0x670B11` | 2, 3 (HOVER, WINGS) | `calcPhysicsXformTreads` 0x270817 | `+0x88`, `+0x90`, `+0x94`, `+0x98`, `+0x9C`, `+0xB8` |
| `0x67B7AA` | 4 (FOUR_LEGS_HUGE) | `calcPhysicsXformHugeFourLegs` 0x27B47F | `+0x88`, `+0x90`, `+0x94`, `+0x98`, `+0x9C`, `+0xB8` |
| `0x6732DC` | 9 (SHIP) | `rva00272FB6` 0x272FB6 | `+0x88`, `+0x8C`, `+0x90`, `+0x94`, `+0x98`, `+0x9C`, `+0xA8`, `+0xB0`, `+0xB4`, `+0xB8`, `+0x114`, `+0x118`, `+0x11C`, `+0x120` |
| `0x670E09` | 5 (GIANT_BIRD) | `rva00270B0F` 0x270B0F | членов шаблона не читает (берёт `[ai+0x188]` и `+0x600` у объекта) |

Разбиение по `Appearance` и набор читаемых смещений совпадают с BFME2 один в один: в
`DrawableCalcPhysicsXform.cpp` (retail 0x27BA95) диспетчер описан как «wheels (cases 1/8), treads
(cases 2/3), HugeFourLegs (case 4), rva00270B0F (case 5), rva00272FB6 (case 9)», а заголовки
`DrawablePhysicsXformWheels.cpp` / `Treads.cpp` / `Ship.cpp` перечисляют те же члены шаблона. Это
`INFERENCE`-подтверждение; вердикты ниже стоят на адресах RotWK.

Конкретные инструкции для полей партии:

```
0067706F  movss xmm0, dword ptr [eax + 0x88]   ; AccelerationPitchLimit
00677091  movss xmm0, dword ptr [eax + 0x90]   ; PitchStiffness   (#13)
0067709E  movss xmm0, dword ptr [eax + 0x94]   ; RollStiffness    (#15)
006770AB  movss xmm0, dword ptr [eax + 0x98]   ; PitchDamping     (#12)
006770B8  movss xmm0, dword ptr [eax + 0x9c]   ; RollDamping      (#14)
006770DA  mov      al,    byte ptr [eax + 0xdc]; HasSuspension    (#6)
```

`eax` здесь получен как `0x677069 mov eax,[ebp+8]; 0x67706C mov eax,[eax+4]`, где `[ebp+8]` —
переданный `Locomotor*`, а `+4` — `LocomotorTemplate*`. Хвост `DrawableLocoInfo` (`0x58` байт,
`0x677042 push 0x58`, конструктор `0x670392`, запись `[esi+0x13C]`) — та же структура, что и в
BFME2 (`DrawableLocoInfo`, ctor 0x00270098). Значения сразу складываются в локальные слоты
(`[ebp-0x40]` и т. д.) и дальше используются в пружинно-демпферных уравнениях.

`HasSuspension` (`+0xDC`) не просто читается: флаг сохраняется в `[ebp-0xD]` и проверяется в двух
местах — `0x677182` `cmp byte ptr [ebp-0xd],0` (ветка с обнулением `[locoInfo+0x54]` и
инкрементом `[locoInfo+0x50]` — счётчик кадров в воздухе) и `0x677630` (ветка с
`[locoInfo+0x50]`/`+0x54`). То есть это переключатель подвески, а не мёртвое поле.

**`ForwardAccelerationPitchFactor` (#17) и `LateralAccelerationRollFactor` (#18)** живут только в
ветке SHIP (`0x6732DC`):

```
0067337B  movss xmm0, dword ptr [eax + 0xb0]   ; ForwardAccelerationPitchFactor
00673383  movss dword ptr [ebp - 0x30], xmm0
00673388  movss xmm0, dword ptr [eax + 0xb4]   ; LateralAccelerationRollFactor
00673390  movss dword ptr [ebp - 0x34], xmm0
...
006736DB  movss xmm1, dword ptr [ecx + 0x1c]   ; ecx = [ebx+0x13C] = DrawableLocoInfo
006736E0  mulss xmm0, dword ptr [ebp - 0x30]   ; * ForwardAccelerationPitchFactor
006736E5  subss xmm1, xmm0
006736E9  movss dword ptr [ecx + 0x1c], xmm1   ; DrawableLocoInfo +0x1C
006736BE  mov   ecx, dword ptr [ebx + 0x13c]
00673752  mulss xmm0, dword ptr [ebp - 0x34]   ; * LateralAccelerationRollFactor
00673757  movss xmm1, dword ptr [eax + 0x24]   ; DrawableLocoInfo +0x24
0067375C  subss xmm1, xmm0
00673763  movss dword ptr [eax + 0x24], xmm1    ; крен
```

Оба поля участвуют в накоплении наклона/крена от ускорения и пишутся в `DrawableLocoInfo`
(`+0x1C` и `+0x24`), поэтому вердикт «читает», а не «не определено».

Сверка с BFME2 (`Open-BFME-2`): `DrawablePhysicsXformWheels.cpp` (retail 0x276CFB) прямо
перечисляет читаемые члены — «accel pitch limit +0x88, bounce kick +0x8C, pitch/roll stiffness
+0x90/+0x94, pitch/roll damping +0x98/+0x9C, uniform axial damping +0xB8, suspension flag +0xDC,
maximum wheel extension +0xE0»; `DrawablePhysicsXformShip.cpp` (retail 0x272FB6, SHIP) —
«+0xA8/+0xB0/+0xB4 forward velocity / forward / lateral acceleration coefficients … +0x114..+0x120
bobbing amplitudes», что совпадает с чтениями в `0x6732DC` один в один. Это подтверждающий
`INFERENCE`, но вердикт стоит на адресах RotWK, а не на нём.

### `UseTerrainSmoothing` (#29): читатель — `0x670570`, а не геттер

Геттер `0x6701E4` (`0x6701E7` `mov al,[eax+0xED]`) действительно никем не адресуется. Настоящий
читатель — начало функции `0x670570`, куда шаблон приходит вторым аргументом:

```
00670570  mov   eax, 0xb87737
00670575  call  0xa3cef0
0067057D  mov   eax, dword ptr [ebp + 0xc]   ; Locomotor*
00670583  mov   eax, dword ptr [eax + 4]     ; LocomotorTemplate*
00670586  mov   al,   byte ptr [eax + 0xed]  ; UseTerrainSmoothing
0067058C  test  al, al
```

Вызовы: `0x676EB0` (из `0x676E5D`, ветка `Appearance == 1`) и `0x67713B` (из `0x677026`, ветка
FOUR_WHEELS). Оба идут в отрисовку/посадку на рельеф; сама привязка к отрисовке — `INFERENCE`, но
само чтение доказано адресом `0x670586`.

### `CashBackPercent` (#20): цепочка доведена до слота, но слот читает чужое поле

Таблица `0xC5EFE8` (3 строки) отдаётся парсеру `0x42EEFA` из `0x8834D3`
(`push 0xc5efe8`), а `ModuleFactory` создаёт данные через `0x64BD5A` (`news 0xEC`,
ctor `0x8842AF`, vtable `0xC5F570`) и поведение через `0x64BDE9` (`news 0x110`, ctor `0x884391`,
vtable `0xC5F5F0`). В ctor `0x8842E2` `movss [esi+0xD4], xmm0` пишет значение по умолчанию
(`0xBD869C`), то есть поле существует и инициализировано.

Слоты обеих vtable проверены на чтение `+0xD4`: единственное попадание — слот 0 `0xC5F5F0`
(`0x88442C` → `0x88460B`):

```
00884626  mov   ebx, dword ptr [edi + 4]
0088462E  mov   edi, dword ptr [ebx + 0xd4]   ; начало вектора
0088464C  cmp   edi, dword ptr [ebx + 0xd8]   ; конец вектора
```

Это `mov` целого указателя с последующим сравнением с `+0xD8`, то есть итерация по контейнеру
другого класса, а не чтение `float CashBackPercent`. Потребитель float по `+0xD4` в просмотренных
слотах не найден, но и не опровергнут (слоты вызываются из других мест, часть путей не разобрана),
поэтому вердикт — **не определено**. Утверждение прежней версии, что `0x443939` — «таймер, а не
шаблон», остаётся верным, но оно ничего не говорит о самом поле.

### `CanSwoop` (#23): геттер без ссылок — не доказательство

В таблице `0xC16DD8` поле `CanSwoop` = `+0x83` (парсер `0x42E558`, bool), строка имени
`0xC17CF0`. Блок геттеров `WeaponTemplate` начинается около `0x6C9B84`; геттер поля —
`0x6CA152` `mov al, byte ptr [ecx+0x83]; ret`, ссылок на него **ноль**. Сплошной поиск по `+0x83`
даёт 40 инструкций, и все они в модулях контейнеров/поведений (`0x5F4C6B`, `0x852782`, `0x85393B`,
`0x8669B1`, …), а не в `WeaponTemplate`.

По правилу ревью этого недостаточно: нужен разбор `Weapon::fire`/состояний ИИ, где шаблон оружия
берётся через `[weapon+…]`. Такой разбор в этой партии не доведён до адреса чтения, поэтому
вердикт — **не определено** (ранее стояло «не читает»).

## Как проверялось

1. Трассировка по регистрам (`calc.py`): инструкция `mov rX,[rY+4]` помечает `rX` как указатель на
   шаблон (в `Locomotor` поле `+4` — `LocomotorTemplate*`, в модулях так же), затем собираются все
   обращения `[rX+offset]` с разделением на чтение и запись по `capstone` access-битам; запись в
   регистр снимает пометку, `ret` очищает состояние.
2. Отдельно проверено, что все `calcPhysicsXform*` достижимы от диспетчера `0x67BDC0` (список
   веток выше), а сам диспетчер — от `0x67BF39` в `applyPhysicsXform`.
3. Для `UseTerrainSmoothing` — прямой `xref` на `0x670570` (два вызова) и на геттер `0x6701E4`
   (пусто).
4. Для `CashBackPercent` — цепочка `ModuleFactory` → ctor данных/поведения → vtable → перебор всех
   слотов на чтение `+0xD4`.
5. Для `CanSwoop` — перебор всех геттеров блока `0x6C9B84..0x6CA200` и сплошной поиск инструкций
   с `+0x83`.

## Осталось

- Довести до потребителя 8 полей `не определено` (8, 9, 11, 16, 19, 20, 22, 23 — из них 20 и 23
  уже с частичной цепочкой): нужен разбор реестра данных модулей по цепочке
  create → ctor → vtable → слот, а не поиск по смещению.
- В порт ничего не внесено: это отчёт об исследовании, кода он не меняет.
