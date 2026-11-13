# Port 426 (fork issue 426, P-108): Shift на кнопке найма — RW-разбор

Данные: RotWK 2.01 `workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`
(адреса виртуальные, `tools/mod/rw.py ... dis|xref|str`). Донор BFME2 1.06 —
`/tmp/open-bfme-2/Code` (только подсказка механизма; каждый шаг подтверждён RW).

## 1. Общий путь

`ControlBar::processCommandUI` (RW 0x940435) получает `(commandButton, primary, repeated)`
(стековые аргументы `[ebp+8]`, `[ebp+0xC]`, `[ebp+0x10]`); таблица команд
RW 0x941AC3 / 0x941B63, ветка `UNIT_BUILD` / `HORDE` / `REVIVE` — RW 0x940B0E.
Кнопка найма шлёт сообщение `0x417` (`MSG_QUEUE_UNIT_CREATE`), его исполняет
`GameLogicDispatch` RW 0x77A764, который зовёт слот +0x20 интерфейса производства
`ProductionUpdate::queueCreateUnit` RW 0x8A11D2. Порт уже держит обе половинки
логики: `GameLogicDispatch::queueUnitCreate` и `ProductionUpdate::queueCreateUnit`
(`int count = batch ? 5 : 1`). Не хватает только клиентского флага `batch`.

## 2. Ветки RW 0x940B0E .. 0x940C52 (левый клик, UNIT_BUILD)

| Адрес RW | Что делает |
|---|---|
| 0x940B0E `cmp ecx,0x2E; sete [ebp+8]` | признак REVIVE (команда 0x2E); у revival нет шаблона, вместо него индекс героя `[esi+0xC0]` |
| 0x940B21 `call 0x75D1DC` | шаблон юнита кнопки; 0x940B2B..0x940B56: без revival нужен ненулевой шаблон, для revival — индекс != -1, иначе выход без сообщения |
| 0x940B5C `push 0; call 0x68C327` | первый интерфейс производства выбранного объекта; нет интерфейса — выход |
| 0x940B6D `cmp [ebp+0xC],0; je 0x940C57` | `primary == 0` — правый клик: это ветка отмены (сообщение 0x418, RW 0x940C80), не найм |
| 0x940B77..0x940B8A `call [0xDE8200]+0x64` | `BuildAssistant::canMakeUnit(obj, tmpl, index)`; результат 7 (command points short) — выход молча |
| 0x940B90..0x940BB1 | обход `getBuildUpgrades` кнопки (`+0x38..+0x3C`), `0x66F5E5` поиск апгрейда, `+0x38` его маска: `edi` = upgradeID, по умолчанию -1 |
| **0x940BB3** `mov ecx,[0xDE4334]; call 0x63F122` | **`Keyboard::isShift()`** (RW 0x63F122..0x63F134: `word [ecx+0xC]`, биты 0x30 и 0x400; xref 0x940BB9). DONOR BFME2 0x00232683 (`Code/GameEngine/Source/GameClient/Input/Keyboard.cpp:595`) — тот же тест `KEY_STATE_LSHIFT|RSHIFT|SHIFT2` |
| 0x940BC2 `cmp [ebp+8],0` | REVIVE: пакетный найм запрещён |
| 0x940BC8..0x940BD7 `test ebx,ebx; cmp word [ebx+0x5F0],0` | шаблон не нулевой и `MaxSimultaneousOfType == 0` (`ThingTemplate +0x5F0` = 1520, строка таблицы полей `engine/data/rotwk-201/field-tables.json`, парсер RW 0x42EC11). DONOR BFME2 0x00539DCB (`ControlBarCommandProcessing.cpp:514`) пишет то же условие как `!revival && what && what->hero==0` — имя поля донора не переносим, RW читает именно +0x5F0 |
| 0x940BD9 `mov byte [ebp+0xC],1` | `multiple = isShift && !revival && tmpl && MaxSimultaneousOfType==0`; иначе 0x940BDF обнуляет |
| 0x940BE6..0x940BF0 | `repeated && !multiple` → `0x940485` (возврат без сообщения). DONOR BFME2 0x00539DCB (`ControlBarCommandProcessing.cpp:515`, `if (repeated && !multiple) return CBC_COMMAND_NOT_USED`). Порт `repeated` не моделирует — стоп S-7326 |
| 0x940BF6 `cmp [esi+0x14],4; sete al` | пятый аргумент сообщения = (команда == 4): в RotWK это выбор второй очереди производства (RW 0x77A7F9..0x77A818) |
| 0x940C06 `push 0x417; call [eax+0x48]` | `MSG_QUEUE_UNIT_CREATE` |
| 0x940C10 / 0x940C24 / 0x940C36 / **0x940C3E** / 0x940C48 | аргументы: bool revival, int templateID\|buildIndex, int upgradeID, **bool multiple**, bool command==4 |

Правый клик (RW 0x940C57..0x940CE7) читает Shift повторно (0x940CBD)
и тем же условием `!revival && tmpl && [tmpl+0x5F0]==0` (0x940CC6..0x940CD8)
ставит `all` сообщения 0x418: в порту это уже `ControlBar::cancelQueued`
(RW 0x940CD0), менять не нужно.

## 3. Dispatcher RW 0x77A764

- 0x77A7A1 `0x6CFE6C` — шаблон по 16-битному id + final override;
- 0x77A7C6 arg2 → `edi` (value30/upgradeID), 0x77A7D1 arg3 → `[ebp-0x28]` (**batch**),
  0x77A7DA arg4 → `al` (вторая очередь);
- 0x77A7F9..0x77A818: arg4 != 0 → `0x68C327(producer,1)`, иначе `(producer,0)`;
- 0x77A820 `call [edx+8]` — `requestUniqueUnitID`;
- 0x77A829..0x77A83C `call [edx+0x20]` — `queueCreateUnit(tmpl, buildIndex, id, value30,
  batch, "", false)` (семь аргументов, batch — пятый, `[ebp+0x18]` тела RW 0x8A11D2).

## 4. Механизм пяти юнитов RW 0x8A11D2

| Адрес RW | Что делает |
|---|---|
| 0x8A1205 `call [0xDE8200]+0x64` | `canMakeUnit` **один раз** до цикла (деньги/очки/очередь проверяются для одного юнита) |
| 0x8A1244..0x8A125C | цена: герой по индексу (0x780AD3) либо `calcCostToBuild` (0x73C25F) |
| **0x8A1260..0x8A1270** `cmp byte [ebp+0x18],al; setne al; lea eax,[eax*4+1]` | **count = batch ? 5 : 1**. DONOR BFME2 0x0049DA34 (`ProductionUpdateQueue.cpp:1919`, `Int count = five ? 5 : 1`) |
| 0x8A1284..0x8A128D | на каждом шаге `m_queue.size() < MaxQueueEntries` (`[data+0x28]`), иначе возврат `true` с тем, что уже поставлено |
| 0x8A1292 `call 0x7B17EF` | `Money::withdraw(cost, ScoreKeeper, 1)` — списание **ограничено остатком денег** (RW 0x7B1809 `cmova`), запись в статистику 0x79DFC2 |
| 0x8A12B2..0x8A13B0 | запись очереди на 0x54 байта, тип 1 (юнит) либо 3 (build index), `addToProductionQueue` 0x8A0C99 |
| 0x8A13B8 `dec count`; **0x8A13C0 `cmp [player+0x90+4],ebx; jb 0x8A1278`** | если денег осталось меньше цены одного юнита — пакет **обрывается**, уже поставленные записи остаются; 0x8A13C9 `count != 0` — следующий шаг |

Итог для «денег на два» (цена 100, денег 200, `MaxQueueEntries >= 5`): в очередь
встают **два** юнита, деньги 0. Для денег 250 — три (третье списание упирается в
остаток 50 и цикл выходит). Ограничение очереди обрывает пакет раньше пяти.
Порт `engine/src/GameLogic/Module/ProductionUpdate.cpp:398` (`queueCreateUnit`)
уже повторяет все четыре правила: count=5, проверка длины очереди, списание через
`Money::withdraw` с зажимом, выход по `money < cost`.

## 5. Где порт расходится

`ControlBar::pressButton(int slot, bool inPalantir)` не получает состояние Shift;
в `GUI_COMMAND_UNIT_BUILD` (`engine/src/GameClient/ControlBarCommandProcessing.cpp:170`)
четвёртый аргумент сообщения всегда `false`. Поэтому `batch` в
`ProductionUpdate::queueCreateUnit` всегда 0 и Shift-клик ставит одного юнита.
Соседний путь `rightPressButton` / `cancelQueued` Shift уже получает
(RW 0x940CD0). Второй путь найма (REVIVE, RW 0x940435 → 0x940B1B) пакетов не
делает по RW 0x940BC2 и в порту тоже не меняется.

**Правка**: один существующий путь — `pressButton` получает `shift`, по
RW 0x940BB3..0x940BE2 вычисляет `multiple` и отдаёт его четвёртым аргументом
`MSG_QUEUE_UNIT_CREATE` (RW 0x940C3E). Состояние Shift пробрасывают три штатных
вызывающих: хоткей `InGameHud.cpp:157`, левый клик Палантира
`InGameHud.cpp:322` и радиальное меню `ControlBarRadialMenu.cpp:293`
(у последнего `shift` уже есть в сигнатуре). Никакого второго сообщения,
второго цикла и новой симуляционной переменной не добавляется; `xfer` не меняется
(флаг живёт только в клиентском сообщении, как и в RotWK).

## 6. Не перенесено

- `repeated` (RW 0x940BE6..0x940BF0): порт не различает автоповтор зажатой
  клавиши, поэтому ветка «повтор без Shift не шлёт сообщение» не смоделирована —
  стоп S-7326.
