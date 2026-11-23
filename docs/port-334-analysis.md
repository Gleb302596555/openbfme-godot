# PORT-334: блокирующее расхождение области задачи

Задача 313, fork issue 889. Статус: анализ, не завершённый перенос.

Все восемь стартовых функций прочитаны от первой инструкции через
`tools/mod/rw.py ... dis <address> 100` по проверенному RotWK 2.01.
Досье использовано для навигации, BFME2 — только как донор.
Здесь нет дампов бинарника или выдержек INI.

## Что фактически делают адреса

Адрес ячейки ниже — найденный указатель в образе PE, не адрес начала
vtable. Полная цепочка create → constructor → vtable ещё не восстановлена;
поэтому таблица не выдаётся за завершённую классификацию всех слотов.

| Функция RW | Ячейка указателя RW | Ветки retail | Место порта / состояние |
|---|---|---|---|
| 0x8B74A6 AISpecialPowerUpdate | 0xC6DA20 | xfer versions 1..4; глобальная версия; UpdateModule; initialized только с v3; active; при load и active восстановление decision для initialized или старой версии; вложенный decision только при наличии и active, для v1 дополнительная проверка skirmish AI | `AISpecialPowerUpdate.cpp`, `xfer`: native v2, собственное описание decision и lookup кнопки; не retail layout |
| 0x894F38 AttachUpdate | 0xC6429C | xfer versions 1..3; UpdateModule; parent ID; до v2 читается/пишется **локальный отброшенный bool**, с v3 EVA event | `AttachUpdate.cpp`, `xfer`: native v1, parent ID + имя EVA. Комментарий «bool this port lacks» не означает отсутствующее runtime-поле: в RW bool расположен на стеке |
| 0x899405 AutoPickUpUpdate | 0xC65720 | xfer versions 1..2; UpdateModule; countdown и bool; второй bool с v2 | `AutoPickUpUpdate.cpp`, `xfer`: native v1, оба bool и дополнительный счётчик scans; retail old-version ветки отсутствуют |
| 0x89EBE8 OCLUpdate | 0xC673C8 | UpdateModule; выход при LightCRC; versions 1..2; два кадра, count с v2 | `OCLUpdate.cpp`, `xfer`: native v1, все три значения и validation; иной порядок версии |
| 0x8A9D7C production exit | 0xC61AA4 и 0xC697F0 | UpdateModule; выход при LightCRC; Version1; coord3D rally point и bool | `ProductionExitModules.cpp`: общий `SimpleProductionExitUpdate::xfer`, вызываемый Default и SupplyCenter; native дополнительные обёртки версий. Это не выход носильщика из лесопилки |
| 0x8B4A69 AIGateUpdate | 0xC6D07C | versions 1..2; UpdateModule; enabled, два int, loaded, trigger; на load loaded сбрасывается; с v2 глобальный int | `GateModules.cpp`, `AIGateUpdate::xfer`: native v1 и восстановление script trigger вместо retail reset; общий вопрос восстановления мира |
| 0x8B64CA EntEnragedUpdate | 0xC6D54C | version 1; UpdateModule; три uint и пять bool в фиксированном порядке | `EntEnragedUpdate.cpp`, `xfer`: поля и порядок уже присутствуют; сам по себе partial не доказывает отсутствующую механику ярости |
| 0x8CDA03 BeaconClientUpdate | 0xC7576C | Version1; ObjectModule; выход при LightCRC; particle ID и last radar pulse | `GameClient/BeaconClientUpdate.h/.cpp`: описан как client-only S-4190, собственного xfer нет; требуется сохранение клиентского мира/particle IDs |

Чтение донора: `Code/GameEngine/Source/GameLogic/Object/Update/`:
`AISpecialPowerUpdate.cpp` (DONOR BFME2 0x004B3352),
`AttachUpdate.cpp`, `AutoPickUpUpdate.cpp`, `OCLUpdate.cpp`,
`AIGateUpdateXfer.cpp`,
`ProductionExitUpdate/DefaultProductionExitUpdateXfer.cpp`;
`Code/GameEngine/Source/Common/EntEnragedUpdate.cpp`
(DONOR BFME2 0x004B23C5);
`Code/GameEngine/Source/GameClient/Drawable/Update/BeaconClientUpdate.cpp`
(DONOR BFME2 0x004C95C9).
Поведение таблицы подтверждено RW, не перенесено из донора на веру.
В частности, AISpecialPowerUpdate RotWK имеет дополнительный active gate,
поэтому донорская функция не является точной заменой.

## Почему не исправлен runtime

S-4190 уже фиксирует принятое решение D3: объектные snapshot layouts —
нативные для OpenBFME; совместимость retail требует SAVE-7 translator per class.
Добавить retail version branches в native xfer недостаточно: значение версии
приходит из другого формата, различаются родительские блоки и вложенные решения.
Также нельзя заменить существующее восстановление gate trigger сбросом bool,
не разобрав восстановление общего script world. Это не локальное исправление
обычного gate update.

Нет доказанной по данным восьми адресов независимой отсутствующей ветки
обычного update, которую можно безопасно перенести. Это **не** вердикт
«читателей нет» и **не** утверждение, что восемь классов полностью перенесены.
S-7883 фиксирует зависимость от общего механизма, а не снимает S-4190.
Нужен выбор scope: retail snapshot translator или исследование иных update
адресов вместо восьми названных xfer. Полная привязка слотов остаётся открытой.

## Проверки и карты

Код симуляции, xfer, эталоны и чужие тесты не изменены. Сдвига perf1 / uber1 /
save4* от этой ветки нет. PORT-334 тесты не добавлялись: тест, изображающий
исправление отсутствующего runtime-дефекта, дал бы ложную сдачу.

CMake/Ninja первоначально отсутствовали; установлены в игнорируемую .venv.
Конфигурация Release без Godot успешна, полная цель openbfme_tests запущена
в один поток, затем остановлена после выявления блокирующей зависимости.
Полного бинарника и результата doctest нет; статус review не заявляется.

Карты не изменены. Новых читателей полей нет; ни одна функция не объявляется
новой ported; красные кнопки не объявляются исправленными. Upstream-ветка
не нужна для отчёта без кода. PR не открыт: это остановка с вопросом, не сдача.
