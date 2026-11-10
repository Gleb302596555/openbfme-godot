# Port 99: PlayerTemplate/LightPointsUpSound — анализ RotWK 2.01

Рабочая запись по issue #259. Адреса прочитаны через `tools/mod/rw.py dis|xref` из RotWK 2.01 (`game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`, оговорка S-001). Донор — Open-BFME-2 (декомпиляция BFME2 1.06). Значения и выдержки INI мода здесь не приводятся.

Итог коротко: единственный читатель поля в RotWK 2.01 — тело скриптового действия `PLAYER_GIVE_LIGHT_POINTS` (RW `0x7BD2D0` → RW `0x6AB148`), и он играет звук из системы light points (War of the Ring), которой в порте нет (S-531). Путь силы в скирмише (очки силы книги заклинаний, RW `0x6AA980` → RW `0x782764`) поле не читает; читателя в скирмише нет. Поэтому в движке порта ничего не меняется (ниже, раздел 6), а поведение порта (поле разбирается и хранится, но не играется) уже совпадает с RotWK для всего, что порт переносит.

## 1. Поле, таблица, место в классе

- Строка таблицы RW `0xBF8488`: имя RW `0xBF7E48` (`LightPointsUpSound`), парсер RW `0x73B217` (`parseAudioEventName`: первый токен строки, S-145), userData 0, смещение `+0x1A4`. Таблица — RW `0xBF81A8` (61 строка) плюс три строки RW `0xC33CB8`; её передаёт парсер шаблона RW `0x5FDF75` (`push 0xBF81A8` на RW `0x5FDF8E`, `push 0x154`/RW `0x7B1974` для второй таблицы на RW `0x5FDFA3`).
- Соседние строки: `ObjectiveAddedSound` RW `0xBF8498` → `+0x1A8`, `ObjectiveCompletedSound` RW `0xBF84A8` → `+0x1AC`; те же три указателя на `AudioEventRTS`.
- Класс `PlayerTemplate`: размер RW `0x1DC` (загрузчик RW `0x5FE85C` .. RW `0x5FE8A0` `push 0x1DC`, конструктор — RW `0x5FE8B9`). Конструктор RW `0x5FE3F1`, vtable RW `0xBF7D20` записывается на RW `0x5FE412`; в этой vtable один слот — удаляющий деструктор RW `0x5FDFD8` (за ним данные, строка `MultiSelectionPortrait` по RW `0xBF7D24`), как и в BFME2 1.06 (DONOR `PlayerTemplate.h`: «PlayerTemplate has no other virtual slots»).
- Конструктор обнуляет три звука: RW `0x5FE566` (`+0x1A4`), RW `0x5FE56C` (`+0x1A8`), RW `0x5FE572` (`+0x1AC`), сразу после инициализации трёх 12-байтных списков `+0x180`/`+0x18C`/`+0x198` (RW `0x5FE53C` .. RW `0x5FE561`, helper RW `0x478CBE`).
- Копирующее присваивание RW `0x5FDCF0` .. RW `0x5FDF72` переносит три звука копирующим helper'ом RW `0x4A86AC`: RW `0x5FDE7A`/RW `0x5FDE81` (`+0x1A4`), RW `0x5FDE8C`/RW `0x5FDE93` (`+0x1A8`), RW `0x5FDE9E`/RW `0x5FDEA5` (`+0x1AC`).
- Деструктор RW `0x5FE1F0` .. RW `0x5FE2EB` освобождает те же три указателя: RW `0x5FE243`/RW `0x5FE24A` (`+0x1A4`), RW `0x5FE255`/RW `0x5FE25C` (`+0x1A8`), далее `+0x1AC`.

То есть в образе есть ровно три места, которые трогают `PlayerTemplate + 0x1A4` без читателя-потребителя: конструктор (0), копирование, деструктор — плюс сама запись INI.

## 2. Полный перебор обращений к `+0x1A4`

Метод (как в Port 51): в образе найдены все вхождения четырёх байт `A4 01 00 00` — 198 штук; для каждого вхождения обратным декодированием (стартовые смещения в пределах 15 байт, `capstone`) найдена инструкция, накрывающая эти байты и использующая `disp32 = 0x1A4` либо непосредственное `0x1A4` в `add`/`lea`/`mov` (формы `[reg+0x1A4]`, `add reg,0x1A4`, `lea reg,[reg+0x1A4]`). Так как обращение к члену возможно только через disp32 или непосредственное, список полный. 121 вхождение декодировалось как такая инструкция; остальные 77 — не обращения: `0F 8? A4 01 00 00` (`jcc rel32` с целью, содержащей эти байты, напр. RW `0x401A47`, RW `0x472F2B`, RW `0x493B73`), `81 EC A4 01 00 00` (`sub esp,0x1A4`, RW `0x8DE634`, RW `0xACB486`), данные таблиц (RW `0xBF8494` — само поле `+0x1A4` строки RW `0xBF8488`; RW `0xC169F8`, RW `0xD8C75C` — массивы чисел), и `cmp dword ptr [reg+0x1A4], imm8` (`83 B? A4 01 00 00 xx`) — те декодированы в первом проходе по `modrm` и учтены.

Среди 121 обращения к `+0x1A4` в контексте `PlayerTemplate` лежат только:

| адрес RW | инструкция | что это |
|---|---|---|
| `0x5FE566` | `mov dword ptr [esi+0x1A4], ebx` | конструктор, 0 |
| `0x5FDE7A`, `0x5FDE81` | `lea eax,[esi+0x1A4]` / `lea ecx,[ebx+0x1A4]` | копирование, helper RW `0x4A86AC` |
| `0x5FE243`, `0x5FE24A` | `lea eax,[esi+0x1A4]` / `lea ecx,[ebx+0x1A4]` | деструктор |
| `0x6AB178` | `add eax,0x1A4` | **единственный читатель**, см. раздел 3 |

Остальные — чужие классы с членом по тому же смещению: вектор (RW `0x86C9CD`: `[eax+0x1A8] - [eax+0x1A4] >> 2` — `empty()` шаблона), структуры HUD/рендера (RW `0x505DFC` складывает `+0x1A4` и `+0x1AC` как целые), массивы, вызовы виртуальных методов через слот `0x1A4` (`call dword ptr [eax+0x1A4]`, 15 штук) и `add reg,0x1A4` в сгенерированном коде исключений (RW `0xB76642`, RW `0xB9F8AF` и соседи).

Геттера у поля нет: единственная «геттерная» последовательность `mov eax,[ecx+0x1A4]; ret` — RW `0x71576F`, и она принадлежит другому классу: соседние геттеры того же кластера читают `+0x18C`, `+0x190`, `+0x194`, `+0x198`, `+0x19C`, `+0x1A0`, `+0x1A8` (RW `0x715753` .. RW `0x71577D`), а сеттеры RW `0x715703` и RW `0x71572B` пишут **пары** dword в `+0x19C`/`+0x1A0` и `+0x1A4`/`+0x1A8`, то есть члены по 8 байт. В `PlayerTemplate` по `+0x198` лежит 12-байтный список, а `+0x1A4`/`+0x1A8`/`+0x1AC` — три отдельных указателя (раздел 1), и `[ecx+0x1A4]; ret` в его vtable/коде нет.

## 3. Единственный читатель: RW `0x6AB148`

Ссылок на RW `0x6AB148` в образе ровно одна (скан `E8`/`E9` по `.text`), и dword-значения `0x6AB148` в образе нет вовсе — то есть это не слот vtable и не указатель в данных, а обычная функция с одним вызывающим.

```
0x6AB148  push ebp / mov ebp,esp / (проверка стека RW 0xA3CEF0) / sub esp,0x88
0x6AB15A  push dword ptr [ebp+8]            ; аргумент — сколько light points дать
0x6AB15D  mov esi, ecx                      ; this = Player
0x6AB15F  lea ecx, [esi+0x338]
0x6AB165  call 0x8309E3                     ; трекер light points: [ecx+4] += delta, отрицательное -> 0
0x6AB16A  mov eax, [esi+0x34]               ; Player + 0x34 = PlayerTemplate (тот же член, что у перенесённого звука целей RW 0x83600D)
0x6AB16D  cmp eax, edi (0) / je -> выход
0x6AB173  cmp dword ptr [ebp+8], 0 / jle -> выход   ; только положительное приращение
0x6AB178  add eax, 0x1A4                    ; &PlayerTemplate->LightPointsUpSound
0x6AB17D  cmp dword ptr [eax], 0 / je -> выход
0x6AB181  cmp dword ptr [0xDE42FC], 0 / je -> выход  ; TheAudio
0x6AB189  push edi (0) / 0x6AB18A push eax / lea ecx,[ebp-0x94]
0x6AB191  call 0x6DA95E                     ; копия AudioEventRTS из сохранённого события
0x6AB196  push dword ptr [esi+0x54]         ; номер игрока (Player + 0x54, как RW 0x836036)
0x6AB1A2  call 0x66ECE1                     ; событию — индекс игрока
0x6AB1A7  mov ecx, [0xDE42FC] / 0x6AB1B6 call dword ptr [eax+0x64]   ; TheAudio->addAudioEvent
0x6AB1C3  call 0x6DABCB                     ; ~AudioEventRTS
```

Вызывающий — скриптовое действие RW `0x7BD2D0` (`PLAYER_GIVE_LIGHT_POINTS`):

```
0x7BD2D0  push ebp / mov ebp,esp
0x7BD2D3  mov ecx,[0xDE3BAC]                ; хранилище шаблонов игроков
0x7BD2DB  push [ebp+8] / 0x7BD2DE call 0x758F7C    ; шаблон по имени параметра 0
цикл 0x7BD2EA: mov ecx,[0xDE4928] (ThePlayerList) / lea eax,[ebp+8] / call 0x6A85EE   ; следующий игрок с этим шаблоном
0x7BD2FD  push [ebp+0xc] / mov ecx,eax / 0x7BD302 call 0x6AB148
0x7BD307  cmp [ebp+8],0 / jne -> цикл
```

Тот же разбор игрока по имени шаблона — у соседнего действия RW `0x7BD266` (`PLAYER_SCIENCE_AVAILABILITY`, в порте уже перенесено, `ScriptActionsUnits.cpp`).

Имя действия: строка RW `0xC3FE44` (`PLAYER_GIVE_LIGHT_POINTS`), описание RW `0xC3FE28` (`Player_/ Give light points`). Соседние действия того же семейства (все четыре разрешают игрока одинаково):

| действие | тело RW | что делает |
|---|---|---|
| `PLAYER_GIVE_LIGHT_POINTS` | `0x7BD2D0` | `0x6AB148`: `[Player+0x33C] += amount`, затем звук |
| `PLAYER_RESET_LIGHT_POINTS` (имя RW `0xC3FDF4`) | `0x7BD311` | `0x6AA931`: `and dword ptr [ecx+0x33C], 0` |
| уровень light points, «Gives a light point level» (имя RW `0xC3FD9C`) | `0x7BD34F` | `0x6AA939`: только при TheLightPointSystem RW `0xDE8C04`: `0x830E63(tracker, level)` |
| уровень light points, «Takes away…» (имя RW `0xC3FD40`) | `0x7BD390` | `0x6AA957`: только при RW `0xDE8C04`: `0x830BD5(tracker, level)` |

## 4. Что такое light points в 2.01

- Это отдельный трекер у игрока: `Player + 0x338`, счётчик `Player + 0x33C` (`0x8309E3` пишет `[ecx+4]`, `0x6AA931` обнуляет `+0x33C`), вектор уровней в `+0x340`/`+0x344` (`0x8309F3`). Операции уровня идут через `TheLightPointSystem` RW `0xDE8C04` (RW `0x830E63` / `0x830BD5`), то есть это система War of the Ring: в WOTR light points тратятся на способности (стоимость — `SpecialPower/LightPointCost`, RW `0x6AA91D`: `Player + 0x33C -= LightPointCost`; в порте это уже записано как непорт, S-531), в скирмише — нет.
- Очки силы книги заклинаний в скирмише — другой член: `PlayerScience + 0x1C` (`0x782764`: `[ecx+0x1C] += delta`, отрицательное -> 0). Путь скирмиша RW `0x6AA980` (`Player`-переопределение vslot 2): RW `0x79DB86` (счётчик очков для счёта, при `TheAudio+0x98`) → RW `0x782764`, затем RW `0x71CEA0` (уведомление), и поле `+0x1A4` не читается. Чит «дать power points» идёт тем же `0x782764` (порт: `CheatMode`, `PlayerScience::addSciencePurchasePoints`). Ранг/уровень поднимает RW `0x6AA9C2` (EVA повышения ранга) — тоже без `+0x1A4`.

Следствие: в скирмише звук не играет; он звучит только когда скрипт даёт light points (WOTR-карты и любые карты с этим действием), и там нужны трекер `Player + 0x338` и `TheLightPointSystem`, которых в порте нет.

## 5. Проверка донора BFME2 1.06

`git -C /tmp/open-bfme-2 grep -n LightPoints` по `Code/`: имя поля встречается только в строке таблицы полей (DONOR `Code/GameEngine/Source/GameLogic/Object/Rva001FE70FInit.cpp:81`, смещение `0x1A4`) и в заголовке класса (DONOR `Code/GameEngine/Include/Common/PlayerTemplate.h:114`, `void *m_lightPointsUpSound; // +0x1A4`). Читателя у донора в декомпиляции тоже нет; `addSciencePurchasePoints` у него описан (DONOR `Code/GameEngine/Source/GameLogic/System/LogicDispatch/headers/common/player.h:708`) — то есть та же картина: очки скирмиша есть, звук к ним не привязан.

## 6. Что это значит для порта

- Для всего, что порт переносит (скирмиш, кампания без light points), поведение уже совпадает: поле разбирается в `PlayerTemplate::m_lightPointsUpSound` (`PlayerTemplate.cpp:168`) и нигде не играется — как и в RotWK.
- Чтобы звук звучал, нужен непорт: трекер light points у игрока (`Player + 0x338`) и `TheLightPointSystem` (`SubsystemInitOrder` знает имя, но системы нет), то есть система War of the Ring из S-531, и семейство скриптовых действий RW `0x7BD2D0` / `0x7BD311` / `0x7BD34F` / `0x7BD390`. Это выходит за рамки Port 99 (по правилу issue: «if only the War of the Ring reads it, stop: out of scope»), решение — за владельцем.
- Тесты `PORT-99` держат текущее поведение: поле разбирается и хранится, а путь очков силы скирмиша (RW `0x6AA980` → RW `0x782764`) и смена уровня не заказывают ни одного звука. Это же ловит появление «второго пути», если кто-то привяжет поле к скирмишу без RW-основания.

## 7. Чего этот анализ не трогает

Файлы полей `PlayerTemplate.*` (Port 98 правит тот же класс в своих местах), таблица полей `Object`, `ScriptActions*`, `SpecialPowerModules`, звуковой слой. Правок в движке нет.
