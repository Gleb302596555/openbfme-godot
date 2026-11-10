# PORT-85 (issue #233): `UnitSpecificSounds` / `VoicePrimaryWeaponMode` и `VoiceSecondaryWeaponMode` — анализ RotWK 2.01

Рабочая запись по issue #233. Все адреса прочитаны линейно через `tools/mod/rw.py dis|xref` из RotWK 2.01 (`game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`, оговорка `S-001`) и сверены по форме с декомпилом BFME2 1.06 (`PickAndPlayUnitVoiceResponse.cpp`, `GameLogicDispatch.cpp`, `ObjectWeaponSetFlags.cpp`, `CommandButtonIsReady.cpp`). Выдержки INI и игровые файлы не приводятся.

**Вердикт (без кода движка, стоп `S-7285`):** в RotWK 2.01 строки `VoicePrimaryWeaponMode` и `VoiceSecondaryWeaponMode` (и парная `VoiceTertiaryWeaponMode`) читаются **исключительно** в ветке `0x43A` (`MSG_SWITCH_WEAPONS`, `RW 0x8DF74F`) диспетчера голосов `RW 0x8DEDBB`, которую вызывают и шлют только кнопки `GUI_COMMAND_SWITCH_WEAPON` (`0x1B`, `RW 0x941204`) и `GUI_COMMAND_TOGGLE_WEAPON` (`0x23`, `RW 0x941257`). Переключение наборов оружия пехоты и кавалерии (лук ↔ меч и др.) в RotWK 2.01 и в Ennorath 1.9.8.2 идёт через `GUI_COMMAND_TOGGLE_WEAPONSET` (`0x2D`, `RW 0x9412C8`) и сообщение `0x457` (`MSG_WEAPONSET_TOGGLE`):
1. клиентский путь `RW 0x9412C8` → `RW 0x941A22` после отправки `0x457` вызывает только `RW 0x75D22E(sourceObj, 1)` (обновление индекса иконки/надписи кнопки `+0xFC`) и выходит в `RW 0x941ABB` без обращения к голосам и аудио;
2. `RW 0x8DEDBB` на тип `0x457` не отвечает (`0x457 - 0x3E9 = 0x6E > 0x6D` → `ja 0x8DFEA4`), и ни один из 56 вызовов `RW 0x8DEDBB` в `game.dat` не передаёт `0x457`;
3. обработчик `0x457` в `GameLogic` (`RW 0x77B529..0x77B6AC`) переключает флаг `WEAPONSET_TOGGLE_1` (`0x18`) без вызовов аудио;
4. в Ennorath 1.9.8.2 все объекты с `VoicePrimaryWeaponMode` / `VoiceSecondaryWeaponMode` переключают оружие кнопками `TOGGLE_WEAPONSET` (`0x457`), поэтому реплики смены режима недостижимы и в оригинальном RotWK 2.01, и в порте; команды `SWITCH_WEAPON` / `TOGGLE_WEAPON` и производитель `0x43A` остаются отдельной задачей `S-290` / `S-703`.

## 1. Единственные ссылки на `Voice*WeaponMode` в `game.dat` (`0x43A` -> `RW 0x8DF74F`)

Сплошной поиск по секциям `game.dat` и `rw.py xref` показывают по одной копии каждой строки в `.rdata` и ровно по одному обращению в `.text`:

| строка `.rdata` | VA строки (файловое смещение) | единственная ссылка в `.text` | слот оружия (`*info->slotPtr`) |
|---|---|---|---|
| `"VoicePrimaryWeaponMode"` | `RW 0xC77B14` (`0x877B14`) | `RW 0x8DF7C9` (`push 0xC77B14`) | `0` (`PRIMARY_WEAPON`) |
| `"VoiceSecondaryWeaponMode"` | `RW 0xC77B2C` (`0x877B2C`) | `RW 0x8DF7A4` (`push 0xC77B2C`) | `1` (`SECONDARY_WEAPON`) |
| `"VoiceTertiaryWeaponMode"` | `RW 0xC77B48` (`0x877B48`) | `RW 0x8DF776` (`push 0xC77B48`) | `2` (`TERTIARY_WEAPON`) |

Все три обращения лежат внутри одной ветки `PickAndPlayUnitVoiceResponse` (`RW 0x8DEDBB`):
- заголовок низкого switch по `msgType` (`RW 0x8DF28A..0x8DF2B6`): `ecx = msgType - 0x3E9` (`RW 0x8DF2A0`), проверка `cmp ecx, 0x6D; ja 0x8DFEA4` (`RW 0x8DF2A6..0x8DF2A9`), индекс в байтовой таблице `RW 0x8E01CD` (`RW 0x8DF2AF`), переход по таблице `RW 0x8E0191` (`RW 0x8DF2B6`);
- для `msgType = 0x43A` (`1082`, `MSG_SWITCH_WEAPONS`): `0x43A - 0x3E9 = 0x51`, `byte [0x8E01CD + 0x51] = 0x0D`, `dword [0x8E0191 + 0x34] = 0x8DF74F`;
- тело `RW 0x8DF74F..0x8DF7FA`: читает `PickAndPlayInfo*` из `[ebp + 0x10]` (`RW 0x8DF74F..0x8DF756`), берёт указатель на слот `[info + 0x0C]` (`RW 0x8DF75C..0x8DF761`), разыменовывает `*slotPtr` (`RW 0x8DF767`) и разветвляется: `0` → `RW 0x8DF7C9`, `1` → `RW 0x8DF7A4`, `2` → `RW 0x8DF776`, иначе (или при `NULL`) → выход без голоса `RW 0x8DFEA4`. Выбранное имя передаётся в `RW 0x8DDB2A` (поиск в селекторах и карте `UnitSpecificSounds` шаблона `RW 0x674FFE` → `RW 0x73EDD0`).

## 2. Кто шлёт `0x43A` и вызывает `RW 0x8DEDBB(0x43A)`

В `ControlBar::processCommandUI` (`RW 0x940435`, switch `RW 0x9408B6..0x9408C9`, байтовая таблица `RW 0x941B63`, таблица переходов `RW 0x941AC3`) сообщение `0x43A` формируют только две команды кнопки:

| команда `CommandButton` | код `GUICommandType` | селектор `0x941AC3` | ветка `processCommandUI` | вызов голоса | доступность в `ControlBar` |
|---|---|---|---|---|---|
| `SWITCH_WEAPON` | `0x1B` (`27`) | `15` | `RW 0x941204..0x941252`: `appendMessage(0x43A)` (`RW 0x94120C..0x941212`), адрес слота кнопки `[esi + 0x80]` в `[info + 0x0C]` (`RW 0x94121F..0x941231`), аргумент слота в поток (`RW 0x94124A..0x940FA9`) | `call 0x8DEDBB` на `RW 0x941242` | `RW 0x94375C` (в порте не перенесено: `S-290`, `Restricted`) |
| `TOGGLE_WEAPON` | `0x23` (`35`) | `19` | `RW 0x941257..0x9412C3`: проверка `sourceObj != NULL`, `appendMessage(0x43A)` (`RW 0x941267..0x94126D`), `[esi + 0x80]` в `[info + 0x0C]`, аргумент слота в поток, переход на `RW 0x941A22` | `call 0x8DEDBB` на `RW 0x94129E` | `RW 0x9437E7` (в порте не перенесено: `S-290`, `Restricted`) |

## 3. Полная проверка пути `TOGGLE_WEAPONSET` (`0x457`): клиент, `RW 0x8DEDBB` и `GameLogic`

### 3.1. Клиентский путь `RW 0x9412C8` → `RW 0x941A22`

1. Перед основным switch по типу команды `ControlBar::processCommandUI` (`RW 0x940812..0x940862`) запрашивает у `sourceObj` модуль `AutoAbilityBehavior` (`RW 0xC0BD98`) и при совпадении с текущей кнопкой (`RW 0x85D741`) вызывает `RW 0x8DEDBB` с событиями `0x7E5` / `0x7E4` (`RW 0x8DFE14` / `RW 0x8DFDB6` — звуки кнопки `SetAutoAbilityUnitSound` `RW 0x75D137` и `UnsetAutoAbilityUnitSound` `RW 0x75D0F8`). К `Voice*WeaponMode` этот пролог не обращается.
2. Команда `GUI_COMMAND_TOGGLE_WEAPONSET` (`0x2D` = `45`, селектор `28` в `RW 0x941AC3`, доступность `RW 0x942FC6`) переходит на **`RW 0x9412C8`**:
   - `RW 0x9412C8..0x9412D5`: `mov ecx, [0xDE6398]; mov eax, [ecx]; push 0x457; call [eax + 0x48]` — создаёт в `TheMessageStream` сообщение `0x457` (`1111`, `MSG_WEAPONSET_TOGGLE`); скан `.text` по 32-битному непосредственному операнду `0x00000457` подтверждает, что `RW 0x9412D0` — единственное место в `game.dat`, где формируется константа сообщения `0x457` (остальные вхождения байтов `57 04 00 00` — обращения к полям `[reg + 0x457]`);
   - `RW 0x9412D8..0x9412E6`: передаёт `sourceObj ? [sourceObj + 0x74] : 0` в `GameMessage::appendObjectIDArgument` (`call 0x71111A`);
   - `RW 0x9412EB..0x9412F3`: при `sourceObj == NULL` уходит на эпилог `RW 0x941ABB` (возврат `1`), иначе `jmp 0x941A22`.
3. Хвост **`RW 0x941A22..0x941A2C`**:
   - `push 1; push ebx; mov ecx, esi; call 0x75D22E; jmp 0x941ABB`.
   - `RW 0x75D22E..0x75D321` (`ret 8`) обновляет только индекс кадра иконки/текста кнопки `[esi + 0xFC]` (обнуляет при длине вектора изображений `[esi + 0xEC]` меньше `2`; иначе считает `0`/`1` или слот по битам опций `[esi + 0x1C]`: `0x1000000` через `RW 0x68BE7D` + `RW 0x75CDC4`, `0x2000000`, `0x800000`; ср. донор `CommandButtonIsReady.cpp:113-155`). Ни `RW 0x8DEDBB`, ни `TheAudio`, ни `TheEva` отсюда не вызываются.

### 3.2. Поведение `RW 0x8DEDBB` на `msgType = 0x457`

- Пролог `RW 0x8DEDEB..0x8DEE0B` кладёт `msgType` в `[ebp - 0x54]` и вызывает `RW 0x8DD74F`, который выделяет только `0x42C` (`MSG_ENTER`, `RW 0x8DD76E`) и enter-state типы `0x7E6`/`0x7E7`/`0x7E8`/`0x7EC` (`RW 0x8DD63C`).
- На разветвлении `RW 0x8DF28A..0x8DF2A9`:
  - `0x457 < 0x461`: переходы `jg 0x8DF934` (`RW 0x8DF292`) и `je 0x8DF89D` (`RW 0x8DF298`) не срабатывают;
  - `ecx = 0x457 - 0x3E9 = 0x6E` (`RW 0x8DF2A0`);
  - `cmp ecx, 0x6D; ja 0x8DFEA4` (`RW 0x8DF2A6..0x8DF2A9`): верхняя граница низкой таблицы — `0x3E9 + 0x6D = 0x456` (`MSG_DO_SPELLBOOK_SPECIAL_POWER`, `RW 0x8DF7FE`). Для `0x457` условие `0x6E > 0x6D` истинно, происходит безусловный уход на default-выход `RW 0x8DFEA4` (пустой выбор, возврат `0`, звук не воспроизводится).
- Верхнее дерево `RW 0x8DF934..0x8DFEA4` (включая таблицу `RW 0x8E023B`) обрабатывает только `0x464`, `0x46A` и клиентские события `0x7DA..0x7EC`.
- Проверка контекста перед всеми **56** инструкциями `call 0x8DEDBB` в `game.dat` подтверждает, что ни один вызывающий не передаёт `0x457`.

### 3.3. Исполнение `0x457` в `GameLogic` (`RW 0x77B529`)

Ветка `MSG_WEAPONSET_TOGGLE` в `GameLogic::dispatchMessage` (`RW 0x77B529..0x77B6AC`, уже перенесена в `WeaponSetToggle.cpp`) читает аргумент `ObjectID` (`RW 0x710C9E`), вычисляет целевое состояние флага `WEAPONSET_TOGGLE_1` (`0x18`, байт `Object + 0x38F` бит `0`, `RW 0x68BE7D`) и для объектов выбранной группы игрока (`RW 0x771A36`) вызывает `Object::setWeaponSetFlag` / `clearWeaponSetFlag` (`RW 0x691059` / `RW 0x691106`) либо их ордовые обёртки (`RW 0x6945DA` / `RW 0x694636`), обновляя `WeaponSet::updateWeaponSet` (`RW 0x6C99E2`), условия модели `0x1BD..0x1BF` (`RW 0x8E2C0F`), простой атакующего неподвижного ИИ (`RW 0x5E821A`), статус `IGNORE_AI_COMMAND` (`RW 0x6907BD`) и `FiringTracker` (`RW 0x8E302A`). Вызовов голосов или аудио на логической стороне также нет.

## 4. Сводка по данным Ennorath 1.9.8.2 и границы `S-290`

Проход по архиву INI Ennorath 1.9.8.2 (`data/ini` и `bases`, без выдержек содержимого):
- `VoicePrimaryWeaponMode` — **31** активная строка в **29** файлах объектов (плюс объявление по умолчанию в `default/object.ini`), `VoiceSecondaryWeaponMode` — **31** активная строка в тех же **29** файлах, `VoiceTertiaryWeaponMode` — **0** активных строк в объектах; все 29 файлов относятся к пехоте, кавалерии и сценарным юнитам, ни одной осадной машины или катапульты среди них нет;
- переключение оружия пехоты/кавалерии в моде оформлено кнопками `Command = TOGGLE_WEAPONSET` (**94** активных определения в `commandbutton.ini` и `includes/commandbutton.inc`), то есть идёт по пути `0x457`, где ни оригинал RotWK 2.01, ни порт реплику режима не произносят;
- `Command = SWITCH_WEAPON` в моде отсутствует (**0** определений), а `Command = TOGGLE_WEAPON` определён только у **4** кнопок осадных машин (`Command_ToggleCatapultWeapon`, `Command_ToggleStoneThrowerWeapon`, `Command_ToggleFortressCatapultWeapon` в `commandbutton.ini` и `Command_ToggleHobbitBallisteWeapon` в `includes/commandbutton.inc`, причём слоты `Command_ToggleFortressCatapultWeapon` и остальных в `commandset.ini` / `includes/commandset.inc` закомментированы, а у самих катапульт строк `Voice*WeaponMode` нет).

Следствие для порта:
- переносить `VoicePrimaryWeaponMode` / `VoiceSecondaryWeaponMode` на `0x457` (`TOGGLE_WEAPONSET`) нельзя — это разошлось бы с RotWK 2.01;
- ветка `0x43A` (`RW 0x8DF74F`), производитель `0x43A` (`RW 0x941204` / `RW 0x941257`) и правила доступности `SWITCH_WEAPON` / `TOGGLE_WEAPON` (`RW 0x94375C` / `RW 0x9437E7`) остаются за пределами #233 в рамках `S-290` и `S-703` и записаны проектным стопом `S-7285`;
- вердикт удерживают синтетические тесты `PORT-85*` (`engine/tests/test_port_85.cpp`).

## 5. Границы анализа (`INFERENCE`)

- `S-001`: разбор выполнен по образу `game.dat` с известными комьюнити-секциями; перехватчиков вне статического образа мы не предполагаем (`INFERENCE`: внешние runtime-хуки вне `game.dat` статическим анализом не исключены).
- По данным мода проверены все файлы `data/ini/**` и `bases/**` архива 1.9.8.2 (`INFERENCE`: возможные встроенные в сторонние `.map` файлы `map.ini` вне этого набора отдельно не проверялись).
