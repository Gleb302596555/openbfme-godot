# PORT-107: Object UpgradeCameo1..5 — разбор RotWK 2.01 (issue #267)

Бинарник: `game.dat` md5 `daed3668…` (оговорка S-001). Адреса читались `tools/mod/rw.py workspace/RotWK/game.dat dis|xref`, таблицы полей — дампом 16-байтных строк, обращения к смещениям — сканами disp32/disp8/imm8 (capstone, посимвольно по `.text`), вызовы — перебором `E8`-относителей. «Данные» = `ThingTemplate`, `+0x…` — смещения в RW, а не в порте. Донор: `Open-BFME-2` (клон в песочнице), читаемый C++.

## Вердикт

**RotWK 2.01 читает `UpgradeCameo1..5` ровно в одной достижимой точке — `ProductionUpdate::completeUpgrade` RW `0x8A22D5 .. 0x8A2328`: пятёрка имён шаблона выбранного объекта прогоняется через `findUpgrade` (RW `0x66F5E5`), совпавший с завершённым апгрейдом помечает панель грязной (`TheControlBar` RW `0xDE7744 + 0x28 = 1`, запись RW `0x8A2328`).** Потребитель — `ControlBar::update` RW `0x71FD81`: при `+0x28 != 0` зовёт `evaluateContextUI` RW `0x71EBDA` (снимает байт на RW `0x71EBED`) и RW `0x71F207`.

**Панель НЕ показывает слоты камео.** Окна `ControlBar.wnd:UnitUpgrade1..5` (`ControlBar +0x8C .. +0x9C`, сделаны RW `0x71E566`, `WIN_STATUS_USE_OVERLAY_STATES`, без картинки) читаются единственной функцией — `setPortraitByImage` RW `0x71C56A .. 0x71C60D` — и та их только **прячет** в обеих ветках (портрет при этом кладётся на `CameoWindow` через `winSetEnabledImage` RW `0x715C81`). **У `0x71C56A` в 2.01 нет ни одного вызова**: ни `E8`-сайта, ни dword/imm32-указателя в образе (проверено сканом всего mapped image) — мёртвый код. Никакого show/lit/установки картинок нет нигде: строка `ControlBar.wnd:UnitUpgrade%d` (RW `0xC23154`) имеет один xref — создание RW `0x71E56F`; сеттер аксессорного блока `+0x7C` RW `0x8F4F49` (`mov [ecx+0x7C], eax; ret 4`) без вызовов и без указателей (триаж `arena-triage/batch-4.md` строка 23, статус «не определено» — закрыт этим разбором); геттера `+0x7C` в блоке RW `0x8F4F23..0x8F4F59` нет вовсе.

**Донор BFME2 1.06 — то же самое в читаемом C++:** единственное чтение полей — `ProductionUpdateQueue.cpp:1943` (тот же dirty-чек: `selectedTemplate->m_upgradeCameoUpgradeNames[i]` → `findUpgrade` → `TheControlBar->dirty=true`); окна — `ControlBar.cpp:1225` (раскладка `+0x8C`), `:1779-1781` (создание), `:2157/:2165` (прячет в `setPortraitByImage`, объявление/определение без единого вызова во всём дереве донора). Показа камео-слотов нет ни в RW, ни в BFME2.

**Что это значит для задачи:** ожидание «иконка огненных стрел на панели отряда, зажжённая после покупки» в RotWK 2.01 не существует — розница не показывает эти слоты. Переносится то, что RW реально делает с полями: dirty-чек. Показ слотов был бы **новым поведением**, а не переносом (стоп S-7294).

## Строки таблицы полей

Таблица объектов RW `0xDA41F8 .. 0xDA4238` (парсер строк `RW 0x42EE5E` = `AsciiString`, `ThingTemplate +0x…`):

| строка | имя | смещение |
|---|---|---|
| RW `0xDA41F8` | UpgradeCameo1 | `+0x7C` |
| RW `0xDA4208` | UpgradeCameo2 | `+0x80` |
| RW `0xDA4218` | UpgradeCameo3 | `+0x84` |
| RW `0xDA4228` | UpgradeCameo4 | `+0x88` |
| RW `0xDA4238` | UpgradeCameo5 | `+0x8C` |

Соседи: `SelectPortrait` `+0x74` (RW `0xDA41D8`), `ButtonImage` `+0x78` (RW `0xDA41E8`). В порту — строки `Bfme1ObjectFields.inc:169-173` (слот `S_AsciiString`, `ThingFactory.cpp:150`), разбор в `ThingFactory::parseField` (`case S_AsciiString`, `ThingFactory.cpp:526`), чтение — `ThingTemplate::findField` (`ThingTemplate.cpp:506`; незаданное поле — `monostate` → `nullptr`).

## Единственный читатель: цепочка RW `0x8A22D5 .. 0x8A2328`

Внутри `ProductionUpdate::completeUpgrade` (после выдачи апгрейда; BFME2: `ProductionUpdateQueue.cpp:1943`):

| адрес | инструкция | смысл |
|---|---|---|
| `0x8A22D5` | `mov ecx, [0xDE4830]` | `TheInGameUI` |
| `0x8A22DB/0x8A22DD` | `mov eax,[ecx]; call [eax+0x12c]` | выбранный drawable (vtable+0x12C) |
| `0x8A22E3..0x8A22EF` | `test; je 0x8A22EF; mov eax,[eax+0xFC]; jmp 0x8A22F1; xor eax,eax` | нет выбора → `eax = 0` |
| `0x8A22F1/0x8A22F3` | `test eax; je 0x8A232C` | нет объекта → выход без записи |
| `0x8A22F5` | `mov eax, [eax+4]` | шаблон объекта |
| `0x8A22FC` | `add eax, 0x7C` | `&UpgradeCameo1` |
| `0x8A2302..0x8A231F` | `push [ebp-0x38]; mov ecx,[0xDE45A0]; call 0x66F5E5; cmp eax,edi; je 0x8A2323; inc counter; add ptr,4; cmp counter,5; jl 0x8A2302` | пять раз: `TheUpgradeCenter->findUpgrade(имя_i) == завершённый (edi)` |
| `0x8A2323` | `mov eax, [0xDE7744]` | `TheControlBar` |
| `0x8A2328` | `mov byte [eax+0x28], 1` | **dirty** |

Потребитель: `ControlBar::update` RW `0x71FC08` → `0x71FD81 cmp byte [esi+0x28],0; je 0x71FDA1` → `call 0x71EBDA` (`evaluateContextUI`, первая инструкция `0x71EBED mov byte [edi+0x28],0` — очистка) + `call 0x71F207`. Донор: `ControlBarCommandProcessing.cpp:335` (`+0x28`), потребитель — тот же `update`.

## Окна панели и их единственный (мёртвый) читатель

| шаг | адрес | что стоит |
|---|---|---|
| создание | RW `0x71E560..0x71E5A0` | `lea edi,[esi+0x8C]`, имя `format("ControlBar.wnd:UnitUpgrade%d", i+1)` (`push 0xC23154` на RW `0x71E56E`, единственный xref строки), `winGetInstanceData`, `winSetStatus(0x200000)` (`WIN_STATUS_USE_OVERLAY_STATES`), без `winSetEnabledImage` |
| читатель | RW `0x71C56A .. 0x71C60D` | `setPortraitByImage`: **обе ветки** циклом `0x71C5B8` / `0x71C5FC` зовут `winHide(true)` на пяти окнах (`add esi,0x8C; mov ecx,[esi]; push 1; call 0x71552B` — `winHide`, флаг `0x10` `HIDDEN`; донор дословно: `m_rightHUDUpgradeCameos[i]->winHide(true)` в обеих ветках, `ControlBar.cpp:2157/:2165`); ветка с картинкой: `WinUnitSelected` `winHide(false)` (`push 0` на `0x71C57E`), `CameoWindow` ← `winSetEnabledImage(0, image)` RW `0x715C81`, `winClearStatus(0x80)` `RightHUD` (RW `0x7155B9`) и `winSetStatus(0x80)` `CameoWindow` (RW `0x7155AA`, `WIN_STATUS_IMAGE`) |
| вызовы `0x71C56A` | **нет** | `rw.py xref 0x71C56A` пуст; `E8`-сайтов 0; dword/imm32 `0x0071C56A` в образе 0 (включая `lea reg,imm32`); vtable-указателей 0 |

Прочие маршруты к полям закрыты: `lea/mov [reg+0x7C]` (disp32) по всему `.text` — 0 совпадений; capstone-скан disp8/imm8 `0x7C/0x84/0x88/0x8C` с не-esp/ebp базой в клиентских регионах — только чужие классы, `add reg,0x8C`-обходы — только `0x71C5B1/0x71C5F5` (`setPortraitByImage`) и ложные (`0x69EF18`, `0x70A3C1`, `0x6D76E9` и др.); строки `UnitUpgradeList1..7` (RW `0xC1EF9C..0xC1F224`) — это `teamUnitUpgradeList*` шаблонов команд (не панель), без кодовых xref; вторая строка `Upgrade%d` RW `0xC7DF1B` без xref; `RightHUDDraw`/`RightHUDImage` — без кодовых ссылок.

## Данные мода

Ennorath 1.9.8.2: **26** использований `UpgradeCameo1..5` (данные задачи #267) — напр. `mordor/mordorarcher.ini:30` `UpgradeCameo1 = Upgrade_FireArrows` (MordorArcher), `MordorGoblinArcher`, `IsengardUrukCrossbow`, `LAGondorArcher` (`Upgrade_GondorFireArrows`). В рознице эти поля меняют только dirty-чек: когда апгрейд из списка завершается и выбран объект с этим шаблоном — панель перерисовывается (её и так пересчитывает каждый кадр).

## Что сделал порт

- `ControlBar::noteUpgradeCompleted` (`ControlBar.cpp`): чтение `UpgradeCameo1..5` выбранного объекта через `findField` + `TheUpgradeCenter->findUpgrade` (RW `0x66F5E5`), запись `m_dirty` (RW `0x8A2328`); `ControlBar::update` снимает отметку (RW `0x71EBED`) — как RW, без флага поведение панели не меняется (порт пересчитывает панель каждый кадр, супермножество RW-обновления).
- `InGameHud::presentUpgradeComplete`: вызов на каждое новое уведомление о завершении, до ворота локального игрока (у RW-блока dirty проверки игрока нет).
- Комментарий вердикта над строками в `Bfme1ObjectFields.inc`.
- Пины: `engine/tests/test_port_107.cpp` (`PORT-107*`), `engine/cmake/Port107.cmake`, строка в `docs/ennorath/NIGHT_WATCH.md`, стоп **S-7294** (`docs/STOPS.md`).
- Не менялось: показ слотов (его нет и в RW), состояние симуляции, xfer/crc/хэши (флаг клиентский, вне симуляции).

**INFERENCE:** окна `UnitUpgrade1..5` — наследие правого HUD из BFME1 (макет `ControlBar.wnd`); в 2.01 их никто не показывает, `setPortraitByImage` стал мёртвым при переходе портрета на APT-палантр (`CommandUI.Portrait`, `PalantirCommandUI::portraitFor` RW `0x92FE6D`). Вызывающая сторона вне статического образа (хуки DLL) не исключена — как в S-001.

**Что значило бы «доделать»:** показывать слоты камео на панели выбранного юнита (картинка апгрейда, зажигается при владении) — поведения, которого нет в RotWK 2.01, нет и в BFME2 1.06; это отдельная задача с решением владельца (как Port 60 / #170), а не перенос.
