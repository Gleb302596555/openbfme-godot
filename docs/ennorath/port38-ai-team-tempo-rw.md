# PORT-38: AIData team build tempo — разбор RotWK 2.01 (issue #123)

Бинарник: `workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f` (оговорка S-001: образ сообщества).
Все адреса, ветки, непосредственные вызовы, указатели vtable, строки и значения полей ниже прочитаны
`tools/mod/rw.py ... dis|xref|str` и `workspace/rwx.py` по этому файлу. Имена, взятые из декомпиляции BFME2,
из порта или из названий строк таблицы, помечены `INFERENCE`; адрес инструкции и смещение поля при этом
доказаны RW. Значений INI мода, байт мода и игровых архивов в этом файле нет (кроме имён полей и их значений
в `default/aidata.ini`, которые разрешены: строки INI не цитируются).

## 1. Строки таблицы AIData

Блок `AIData`: указатель `[[RW 0xDE4B40] + 0x18]` — это сам объект `AIData` (конструктор **RW 0x6FFBEF**,
разбор файла — парсер **RW 0x7017AA**). Таблица полей начинается в RW **0xC1CB20**; запись таблицы — 16 байт
(`name`, `parse`, `userData`, `offset`). Строки этой задачи (прочитаны `rwx.py`):

| строка | address RW | parse | offset | значение по умолчанию (`default/aidata.ini`) | поле |
|---|---|---|---|---|---|
| `TeamSeconds` | 0xC1CB30 | 0x42ED00 (`parseReal`) | +0x8 | 0.8 | `AIData` + 0x8 |
| `Wealthy` | 0xC1CB40 | 0x42EC5E (`parseInt`) | +0xC | 4000 | + 0xC |
| `Poor` | 0xC1CB50 | 0x42EC5E (`parseInt`) | +0x10 | 1200 | + 0x10 |
| `TeamsWealthyRate` | 0xC1CB80 | 0x42ED00 | +0x1C | 2.5 | + 0x1C |
| `TeamsPoorRate` | 0xC1CBA0 | 0x42ED00 | +0x24 | 1.1 | + 0x24 |
| `TeamResourcesToStart` | 0xC1CBB0 | 0x42ED00 | +0x28 | 0.1 | + 0x28 |

Соседи по таблице, которые в этой задаче не участвуют: `StructureSeconds` 0xC1CB20 (+0x4),
`ForceIdleMSEC` 0xC1CB60 (+0x14, разбор 0x73A429), `StructuresWealthyRate` 0xC1CB70 (+0x18),
`StructuresPoorRate` 0xC1CB90 (+0x20). Ни одна из трёх «строительных» строк не читается в образе: это
подтверждает триаж `docs/ennorath/arena-triage/batch-12.md` (строки 24-25) и наши тесты держат это как
соседнее поведение.

Конструктор **RW 0x6FFBEF** пишет 0.0f / 0 во все шесть полей, поэтому набор данных без них делит на нулевой
rate — ровно как образ (см. §4). Значений по умолчанию «как в retail» в коде порта нет и быть не должно.

## 2. Где RotWK создаёт ИИ-игрока: RW 0x8F7F2B (`AIPlayer`, vtable RW 0xC79D98, 0xA4 байта)

```
0x8F7F4A  mov dword [esi], 0xC79D98        ; vtable AIPlayer
0x8F7F70  mov byte [esi+0x15], 0
0x8F7F73  mov dword [esi+0x20], 0xA        ; 10 секунд
0x8F7FF4  mov eax, [0xDE4B40]
0x8F7FF9  mov eax, [eax+0x18]              ; AIData
0x8F7FFC  cvttss2si eax, [eax+8]           ; (int) TeamSeconds, усечение к нулю
0x8F8001  mov [esi+0x20], eax              ; база задержки в секундах
```

`0x8F8011  ret 4` — конец конструктора; далее 0x8F8014 `xor al,al; ret 8` (другой вход) и 0x8F8019/0x8F801F —
мелочи (`ret` строк). Итог: **+0x20 = `(int)AIData TeamSeconds`**, то есть retail 0.8 даёт 0 секунд
(`INFERENCE` о смысле поля — «секунды между командами»; адреса и усечение доказаны).

## 3. Таймер, который тратит задержку: слот 0x48 vtable, RW 0x8F3EFE

```
0x8F3F01  mov eax, [esi+0x10]              ; Player
0x8F3F04  cmp byte [eax+0x358], 0          ; ИИ включён?
0x8F3F0B  je 0x8F3F5B                      ; нет — выходим
0x8F3F0D  cmp byte [esi+0x14], 0           ; «готов строить команду»
0x8F3F11  jne 0x8F3F34
0x8F3F13  dec dword [esi+0x18]             ; обратный отсчёт кадров
0x8F3F1D  and dword [esi+0x28], 0
0x8F3F21  mov byte [esi+0x14], 1           ; готов
0x8F3F25  mov eax, [0xD9F608]              ; LOGICFRAMES_PER_SECOND = 5
0x8F3F2A  imul eax, eax, 3                 ; 15
0x8F3F2D  cmp ecx, eax
0x8F3F31  mov [esi+0x18], eax              ; не больше 15 кадров
0x8F3F34  dec dword [esi+0x28]
0x8F3F37  cmp dword [esi+0x28], 1
0x8F3F3B  jge 0x8F3F5B
0x8F3F3F  call 0x8F9487                    ; сборка списка/подготовка (см. §5)
0x8F3F4E  call dword [eax+0x68]            ; готов — «выбрать команду» (0x8F994F)
0x8F3F51  mov eax, [0xD9F608]
0x8F3F56  add eax, eax                     ; 10 кадров
0x8F3F58  mov [esi+0x28], eax              ; каденция
```

`[0xD9F608]` — `LOGICFRAMES_PER_SECOND` = 5 (то же значение умножается в §4). Флаг +0x14 сбрасывает §4,
флаг +0x358 у `Player` — общий признак «это компьютерный игрок» (его же ставит конструктор
`AISkimishPlayer` RW 0x8F3DF3: `[arg+0x358] = 1`).

## 4. Задержка до следующей команды: RW 0x8F805D (`AIPlayer` слот 0x58, thunk RW 0x8F3EB2)

Функция RW **0x8F805D .. 0x8F8267** (`ret` в 0x8F81AE — ранний выход; следующий `push ebp` в 0x8F8268).
Регистры: `esi = this` (AIPlayer), `edi` — узел списка команд `[esi+0x10]+0x34C`, `[ebp-0x14]` — максимум
приоритета (sentinel `0xFFFE7961`), `ecx` — `AIData`.

Проход (0x8F8082 .. 0x8F80C9): для каждого узла `ebx = node+8` вызывается слот 0x64 (`push [ebx]`, `call
[eax+0x64]`, `test al,al`) — тот же слот, что RW 0x8F417A / RW 0x8F5EEB (§6); при `true` узел добавляется в
список `[ebp-0x1C]` (0x6AE810), а `[node+8]->+0x21C` (приоритет команды `INFERENCE`: имя поля из BFME2)
сравнивается с текущим максимумом (0x8F80AB .. 0x8F80B6). Затем слот 0x5C вызывается с максимумом
(0x8F80CB .. 0x8F80CE, RW 0x8F8B8F: печатает «AI Selecting team to build» и ищет команду по индексу
`[obj+0x1C]+0x30`). Если он вернул `true` — выход с 1 (0x8F80D5). Если максимум так и остался sentinel —
выход с 0 (0x8F80E5).

Проверка «домашней точки» (0x8F81AF .. 0x8F8207): `edi = [ecx+8]`; `cmp byte [edi+0x1E8],0` — если байт
нулевой, вызывается слот 0x2C `this`; если и он дал 0, в лог уходит строка RW 0xC799F8 / RW 0xC799D4
(«Error : team '<name>' has no Home Position (or Origin).», имя из `[edi+0x14]`). Затем слот 0x24
(`push ebx` (0), `push edi`, `call [eax+0x24]`, RW 0x8F6DE9) **запускает команду** — и только после этого
пересчитывается задержка:

```
0x8F8219  mov eax, [esi+0x20]              ; база в секундах (из §2)
0x8F821C  mov edx, [esi+0x10]              ; Player
0x8F821F  mov byte [esi+0x14], 0           ; «готов» снят
0x8F8223  imul eax, [0xD9F608]             ; x 5 кадров
0x8F822A  mov [esi+0x18], eax              ; задержка по умолчанию
0x8F8233  mov ecx, [0xDE4B40] / [ecx+0x18]; AIData
0x8F8236  mov edx, [edx+0x94]              ; деньги игрока
0x8F823C  cmp edx, [ecx+0x10]              ; Poor
0x8F823F  jae 0x8F824C                     ; деньги >= Poor — не «бедный» (беззнаково)
0x8F8241  cvtsi2ss xmm0, eax
0x8F8245  divss xmm0, [ecx+0x24]           ; TeamsPoorRate
0x8F824A  jmp 0x8F825A
0x8F824C  cmp edx, [ecx+0xC]               ; Wealthy
0x8F824F  jbe 0x8F8261                     ; деньги <= Wealthy — обычная полоса (беззнаково)
0x8F8251  cvtsi2ss xmm0, eax
0x8F8255  divss xmm0, [ecx+0x1C]           ; TeamsWealthyRate
0x8F825A  cvttss2si eax, xmm0              ; усечение
0x8F825E  mov [esi+0x18], eax
0x8F8261  mov bl, 1                        ; возврат 1
```

Границы: `Poor` включительно уходит из «бедной» полосы (`jae`), `Wealthy` включительно остаётся в обычной
(`jbe`); сравнения беззнаковые. Деление — `divss`, то есть при нулевом rate получается `+inf` или `NaN`,
и `cvttss2si` даёт «integer indefinite» `0x80000000` — не «тихое» значение.

`AISkimishPlayer` (vtable RW 0xC79788, конструктор RW 0x8F3DF3, 0xA4 байта) использует те же слоты
0x24 / 0x58 / 0x5C / 0x68 и свой слот 0x64 (RW 0x8F417A) и 0x60 (RW 0x8F4091).

## 5. Денежный порог старта команды: RW 0x8F54BA (слот 0x64 у `AIPlayer`, вызывается из RW 0x8F6DE9 @0x8F6E9E)

Сигнатура по регистрам: `ecx = this` (AIPlayer), `arg1` (`[ebp+8]`) — прототип команды, `arg2` (`[ebp+0xC]`) —
флаг, `[ebp+0x10]` — out-флаг «денег не хватило».

Накопитель «сколько юнитов эта команда ставит» (0x8F54E4 .. 0x8F55B9): по списку `[arg1+0x1D8]` записей по
0x18 байт от `[arg1+0x134]`; имя юнита в `entry+0xC` разрешается RW 0x6D1305 через `[0xDE4A40]`. Для каждой
записи два вызова RW 0x8F5347: 0x8F5527 (юнит, `1`) — «можно ли», `false` уводит в 0x8F55E3 (`xor al,al`,
команда не собирается); 0x8F553C (юнит, `0`, out `[ebp-0x14]`) заполняет счётчик и, если вернул `true`,
ставит флаг `[ebp-1]` (0x8F5545). Обход объектов игрока внутри RW 0x8F5347 (0x97338F / 0x68B678 / сравнение
`[ecx+0x10]` с игроком) и смысл «min/max» записи — `INFERENCE`. В 0x8F5570 .. 0x8F55A3 выполняется:

```
0x8F5570  test byte [edi+0x11B], 0x20      ; KindOf-бит шаблона -> счётчик 0
0x8F557A  mov edx, [ecx-4]
0x8F5580  add edx, [ecx]                   ; min + max записи
0x8F5582  cvtsi2ss xmm1, eax               ; счётчик игрока (RW 0x73C25F / Player+0x758 RW 0x780AD3)
0x8F5586  cvtsi2ss xmm0, edx
0x8F558A  mulss xmm0, [0xBD869C]           ; x 0.5f
0x8F5592  mulss xmm0, xmm1
0x8F5596  cvtsi2ss xmm1, [ebp-0x10]        ; накопленное
0x8F559B  addss xmm0, xmm1
0x8F559F  cvttss2si eax, xmm0
0x8F55A3  mov [ebp-0x10], eax
```

Порог (0x8F55BF .. 0x8F55E5):

```
0x8F55BF  mov eax, [0xDE4B40]
0x8F55C4  fild dword [ebp-0x10]            ; накопленный счётчик
0x8F55C7  mov eax, [eax+0x18]
0x8F55CA  fmul dword [eax+0x28]            ; TeamResourcesToStart
0x8F55CD  call 0xA3CFA4                    ; MSVCR71 _ftol2 (FISTP qword, усечение к нулю)
0x8F55D2  mov ecx, [esi+0x10]              ; Player
0x8F55D5  cmp [ecx+0x94], eax              ; деньги
0x8F55DB  jae 0x8F55E7                     ; хватает
0x8F55DD  mov eax, [ebp+0x10]
0x8F55E0  mov byte [eax], 1                ; out-флаг «не хватает»
0x8F55E3  xor al, al                       ; возврат 0
```

Хватает — возврат 1, но только если оба флага `[ebp-1]` и `[ebp+0xC]` нулевые (0x8F55E7 .. 0x8F55F0,
`sete`-логика: «команда уже собирается / уже начата»). Смысл порога (`INFERENCE`): деньги на единицу уже
выставленных юнитов шаблонов команды.

## 6. Кто ещё зовёт слот 0x64 и что вокруг

* RW **0x8F417A** (`AISkimishPlayer`, слот 0x64) зовёт его в 0x8F423C; RW **0x8F5EEB** (`AIPlayer`, слот 0x64)
  — в 0x8F5F52. Оба входят в SEH-пролог (функции начинаются в 0x8F417A / 0x8F5EEB; байты пролога
  0x8F423C / 0x8F5F52 — это уже адреса вызовов).
* RW **0x8F6DE9** (`AIPlayer`, слот 0x24) зовёт его в 0x8F6E9E — то есть путь «старт команды» (§4) сам
  проверяет деньги, поэтому и «нет денег» = команда не начинается.
* RW **0x8F8B8F** (слот 0x5C у обеих) печатает «AI Selecting team to build» (строка RW 0xC79E10) и ищет
  команду по индексу.
* RW **0x8F994F** (слот 0x68 у `AIPlayer`) зовёт слот 0x58 и при успехе прыгает в RW 0x8F9487 (обход
  списка с копированием трёх float-векторов, читает `[obj+0x1DC]`, `[obj+0x1E8]`).
* RW **0x8F9487** — общая сборка/подготовка списка; её же зовёт таймер (§3).

## 7. Что из этого есть в порте и чего нет

Есть: `GameLogicSettings` (`engine/src/GameLogic/GameLogic.h`) — все шесть значений теперь разбираются строками
`kAIDataFields` (`GameLogicSettings.cpp`, каждая с адресом строки RW и `Port 38`); читатели — `AITeamTempo`
(`engine/src/GameLogic/SkirmishAI/AITeamTempo.h/.cpp`): `baseSeconds` (RW 0x8F7FF4 .. 0x8F8001),
`delayFrames` (RW 0x8F8219 .. 0x8F825E), `startCost` / `canStartTeam` (RW 0x8F55C4 .. 0x8F55E3). Арифметика
идёт через `SimMath` (`cvttss2si`, `sseFromInt32`, `divf32`, `ftol2Low32`), чтобы усечения, беззнаковые
сравнения и деление на нулевой rate были инструкциями образа, а не компилятора.

Нет (стоп **S-7186**): вызывающих у этих правил в порте быть не может, пока не перенесена сама сборка команд
ИИ — путь слот 0x68 RW 0x8F994F -> слот 0x58 RW 0x8F805D -> слот 0x24 RW 0x8F6DE9 -> RW 0x8F9487 и слот 0x48
RW 0x8F3EFE (§3). Для него нужны поля прототипа команды, которых у порта нет: домашняя точка `+0x1DC` и байт
«есть домашняя точка» `+0x1E8`, приоритет `+0x21C`, список юнитов `+0x134` / `+0x1D8` с записями по 0x18
байт, и производство команды (`Team::update`). Портовый `Team`/`TeamPrototype`
(`engine/src/Common/Team.h`) — подмножество для скриптов и производства, без этих полей.

Поэтому шесть значений **не** добавлены в `GameLogicSettings::crc`: читателей нет, значит два прогона с
разными значениями не могут разойтись; строка в `STOPS.md` и `NIGHT_WATCH.md` это фиксирует, а коммит,
который перенесёт сборку команд, добавит их в хэш вместе с вызовом.

## 8. INFERENCE

* «Секунды между командами», «бедный / богатый по деньгам», «деньги на юнита» — имена и смысл из названий
  строк таблицы и арифметики; адреса, смещения, ветки и константы доказаны RW.
* `+0x21C` (приоритет команды), `+0x1DC` / `+0x1E8` (домашняя точка), `[Player+0x358]` (компьютерный игрок),
  KindOf-бит `0x20` в `[template+0x11B]` — имена и трактовка из порта / BFME2-декомпиляции; в образе
  доказаны только чтения по этим смещениям.
* Что именно делает RW 0x8F9487 и как выглядит производство команды — не разбиралось (за стопом S-7186).
