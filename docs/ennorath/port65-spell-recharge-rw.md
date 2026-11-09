# PORT-65 — SpellRechargeModifierUpgrade (issue #177), разбор RotWK 2.01

game.dat md5 `daed3668006cd90f01c34e5a7da1901f`; адреса — VA, дизассемблер `tools/mod/rw.py`. Донор Open-BFME-2 (BFME2 1.06) помечен DONOR.

## 1. Класс: create → конструктор → влтаблы → слоты

| что | RW |
|---|---|
| create | RW 0x650104: `push 0x24` (модуль), `call RW 0x8B9F57` (конструктор) |
| data proc | RW 0x8BA1AB (`push 0x14C`); create данных RW 0x65013C |
| регистрация | RW 0x65A21A: строка RW 0xC0AFC4 `SpellRechargeModifierUpgrade` рядом с create/data проками |
| конструктор модуля | RW 0x8B9F57: база RW 0x863B8F (UpgradeModule); +0x10 влтабл мокса RW 0xC6EFD8; влтаблы RW 0xC6F020 (+0), RW 0xC6ED80 (+0xC поведение), RW 0xC6FD08 (+0x18 клиент), RW 0xC6EFC8 (+0x1C create-интерфейс); `mov byte [esi+0x20],1` RW 0x8B9F90 |
| данные | RW 0x8BA1AB, влтабл RW 0xC6F050: таблица RW 0xC76AD8 (extra 8) + RW 0xC6F0D0 |
| слот 8 (удаление) | RW 0x8B9FE0 |
| слот 9 (onCapture) | RW 0x8BA05E |
| слот 10 (реализация) | RW 0x8BA139 |
| слот 0x1C | RW 0x8B9F9A (скалярный деструктор; внутри RW 0x86393B — та же `mov [esi+0x20]`-обвязка, что у соседа RW 0x8B9ED8) |
| create slot 1 (onBuildComplete) | RW 0x8B9FB6 |
| mux slot 0 / 1 / 2 / 3 / 5 / 6 / 7 / 9 / 13 / 14 | RW 0x4986C4, 0x8D26BF, 0x8D26F3, 0x8D2901, 0x8D26B3, 0x63F3BF, 0x863B85, 0x5B462D, 0x863B3B, 0x863B45 (общие UpgradeMux) |

BFME2-донор (`Code/GameEngine/Source/GameLogic/Object/Upgrade/SpellRechargeModifierUpgrade.cpp`) описывает тот же класс: `upgradeImplementation` 0x004B5F9F,
`upgradeRemovalImplementation` 0x004B5E46, `rva004B5DDC` (StartsActive) 0x004B5DDC, `onCapture`; схема совпадает.

## 2. Поля (таблица RW 0xC6F0D0)

| строка | смещение | парсер RW | значение в моде |
|---|---|---|---|
| `Percentage` | +0x138, `std::vector<float>` (каждая строка добавляет элемент) | RW 0x8BA26F → `parsePercentToReal` RW 0x42EEFA + push_back RW 0x90BE00 | `-25%`, `-10%`, `-15%` |
| `StartsActive` | +0x144 | RW 0x42E558 | у Эрестора `Yes` |
| `LabelForPalantirString` | +0x148 | RW 0x42EE5E | `GUI:SpellBookRefreshReduction` |

Таблица только из `LabelForPalantirString` — RW 0xC6F0F0 (issue 177 это отмечала). Конструктор данных RW 0x8BA1AB: вектор +0x138 пуст,
+0x148 пустая строка, +0x144 = 0 (RW 0x8BA1F5).

## 3. Реализация RW 0x8BA139 (слот 10)

- `mov ecx,[esi-8]; call RW 0x68B678` — управляющий игрок объекта; нет — выход.
- `call RW 0x6AAAE0` — `inc dword [ecx+0x71C]` (счётчик наложенных SpellRechargeModifierUpgrade у игрока).
- `call RW 0x6AAAD9` — `mov eax,[ecx+0x71C]`; `dec eax`; `test eax,eax; jg` (RW 0x8BA018) — индекс не меньше 0.
- `mov eax,[esi+0x13C]; sub eax,[esi+0x138]; sar eax,2; dec eax` — последний индекс вектора; `cmp ecx,eax; jl` (RW 0x8BA03A) — индекс не больше него.
- `fld dword [ecx+eax*4]` из +0x138; `call RW 0x6AAAEE` — `movss [player+0x718], xmm0` (скидка на перезарядку).

Тот же зажим дословно в RW 0x8BA05E (onCapture, у старого владельца RW 0x8BA073 .. 0x8BA0C4 с `dec RW 0x6AAAE7`, у нового RW 0x8BA0D2 .. 0x8BA123 с
`inc RW 0x6AAAE0`), в RW 0x8BA1A2 (хвост RW 0x8B9F9A) и в RW 0x8BA139. Функция — единственный писатель Player + 0x718 во всём game.dat: xref RW 0x6AAAEE
даёт RW 0x8BA055, 0x8BA0C4, 0x8BA123, 0x8BA1A2; читатель RW 0x6AAAD2 вызывается только из RW 0x896EC7 (`SpecialPowerModule::startPowerRecharge`,
`fadd 1.0`) и RW 0x99156C (способности героев / осадные). Пустой вектор RW читал бы адрес −4 (нет данных мода с таким случаем — в порте названная ошибка).

## 4. Удаление RW 0x8B9FE0 (слот 8)

`mov eax,[esi]; call [eax]` — мокс слот 0 (выполнен ли); `je` выход. Иначе игрок RW 0x68B678, `call RW 0x6AAAE7` (`dec [ecx+0x71C]`), тот же зажим,
`call RW 0x6AAAEE`. Флаг «выполнено» здесь не сбрасывается — это делает RW 0x8D2688 (UpgradeMux::removeUpgrade) после слота 8, как и в порте.

## 5. onBuildComplete RW 0x8B9FB6 (create-интерфейс, слот 1)

`cmp byte [eax+0x144],0` (`StartsActive`); если да — `call [mux vtable+0x28]` (слот 10, реализация) и `push 1; call [mux vtable+0x24]` (слот 9,
`setUpgradeExecuted(true)`); затем `mov byte [edi+4],1` (+0x20).

## 6. onCapture RW 0x8BA05E (слот 9)

Старый владелец (если есть): `dec` счётчика и зажим; новый (если есть): `inc` и зажим; затем мокс слот 9 (`setUpgradeExecuted(true)`). Теста «выполнено»
нет (в отличие от CostModifierUpgrade RW 0x8B9C4D).

## 7. xfer RW 0x8B9D39, версия (1, 2)

`call [xfer vtable+0x28]` — версия; `cmp byte [ebp-3],2; jb` — ниже 2 байт + 0x20 не читается (`mov eax,[esi]; push [edi+0x20]; call [eax+0x90]` —
`xferBool`); затем хвост UpgradeModule (мокс, RW 0x8D27E8). Счётчик игрока и скидка — состояние игрока: RW 0x6B27C0 (`cmp byte [ebp-0x13],4; jbe`)
передаёт +0x718 и +0x71C вместе начиная с версии 5; в порте это `PlayerScience::xfer` (версия 1 → 2).

## 8. Что не подтверждено / выводы

- RW 0x8B88B0 (слот 17 мокса) — `jmp RW 0x4D0EAF`, тот же no-op, что и базовый; на поведение не влияет.
- INFERENCE: имена `m_flag20` (+0x20) и «счётчик наложенных апгрейдов» даны по донору BFME2 и по поведению, в RW символы не сохранены.
- INFERENCE: пустой `Percentage` в RW — чтение по адресу −4; порт бросает названную ошибку вместо чтения мусора (данных мода это не касается).
- Не перенесено: отображение `LabelForPalantirString` — раздел #164 (порт только хранит строку, как и CostModifierUpgrade).
