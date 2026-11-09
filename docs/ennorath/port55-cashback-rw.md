# PORT-55 / issue #165: CashBackPercent — остановка на общем механизме

2026-11-09, задача дашборда 30. Проверен `origin/main` =
`e4fdbf80033118ed87df47d008b42a3febdbe925`.

## Решение требуется до кода

Reader **существует и достижим**. Это не случай неиспользуемого поля.
Возврат денег находится в **общем SlaughterHordeContain::AddHordeMember**,
который цитадель наследует. В порту самого SlaughterHordeContain нет:
он остаётся `UnportedModule`, а CitadelSlaughterHordeContain наследует
не его, а GarrisonContain и реализует только доставку кольца (S-6010).

Изолированная выплата в `CitadelSlaughterHordeContain::addToContain`
создала бы второй, неполный путь вместо переноса общего механизма.
По условию задачи и PROCESS §15.3 работа остановлена **до изменения кода**.
Нужно решение владельца: разрешить в рамках #165 перенос общего
SlaughterHordeContain и наследование цитадели либо сначала поставить эту
зависимость отдельной задачей. Чужие issue/ветки не изменялись.

## Источники и воспроизведение

RotWK 2.01 `game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`.
Инструкции ниже прочитаны через `tools/mod/rw.py … dis`, линейно, включая
всё тело `0x883B4A..0x884053`, а не поиском четырёхбайтового displacement.
Пример повторения (в приватной рабочей папке с бинарником):

```text
python tools/mod/rw.py workspace/RotWK/game.dat dis 883801 23
python tools/mod/rw.py workspace/RotWK/game.dat dis 8836d3 43
python tools/mod/rw.py workspace/RotWK/game.dat dis 883b4a 560
python tools/mod/rw.py workspace/RotWK/game.dat dis 883a29 30
python tools/mod/rw.py workspace/RotWK/game.dat dis 68ddf1 34
```

BFME2 donor, только для идентификации класса/веток: репозиторий
`Open-BFME/Open-BFME-2`, каталог
`Code/GameEngine/Source/GameLogic/Object/Contain/`, файлы
`SlaughterHordeContainAddHordeMember.cpp`,
`CitadelSlaughterHordeContainRva004807C7.cpp`,
`SlaughterHordeContainRva00480281.cpp`,
`SlaughterHordeContainRva004804DD.cpp`. Выводы о RotWK ниже подтверждены RW,
адреса donor не использованы вместо инструкций RW.
Игровые файлы, данные мода и копии donor в Git не включены.

## Привязка create → ctor → vtable → слот → вызов

- Регистрация цитадели `0x6584DF..0x658507`: имя `0xC0BAE8`, create
  `0x64BDAE`, createData `0x64BDE9`.
- Create `0x64BDD8` вызывает ctor `0x883801`; тот вызывает базовый
  `0x8835AB` и в `0x883818` ставит основную vtable `0xC5F470`,
  в `0x88382C` contain-vtable `0xC5F2E8` на subobject `+0x20`.
- Create Slaughter `0x64BD49` вызывает `0x8835AB`; базовый ctor в
  `0x8835C2` ставит основную vtable `0xC5F258`, в `0x8835D6` —
  contain-vtable `0xC5F0D0` на `+0x20`.
- В обеих основных таблицах слот `+0x78` = **`0x883B4A`**.
  В обеих contain-таблицах слот `+0x9C` = `0x8836D3`.
  Это не только найденный указатель: `0x88370F` вычитает `0x20` из `this`,
  `0x88372A..0x88372F` вызывает основной слот `+0x78` при входе члена.
- Основной слот `+0x84` у Slaughter = `0x9B501B` (true), у цитадели =
  `0x883A29` (решение «съесть или доставить кольцо»).
  Его вызывает именно общий reader: `0x883B84`.
- Слот `+0x80` у цитадели = `0x8840FC` (доставка кольца), у Slaughter =
  `0x9F3A3C`. Общий путь вызывает его для несъедаемого одиночного объекта
  в `0x883F73`, а для завершившего вход отряда — в `0x88403F`.
- Данные цитадели создаются в `0x64BE0D → 0x884391 → 0x8842AF`:
  последний ctor инициализирует базовый процент по `+0xD4` (`0x8842E2`).
  Разбор цитадели `0x88354F → 0x8834C3` сначала добавляет базовую таблицу
  `0xC5EFE8` (`0x8834D3`), затем свои поля `0xC5F080` (`0x88355F`).
  Строка CashBackPercent использует parser `0x42EEFA`, offset `+0xD4`.
  Это общая таблица Slaughter, не независимый процент только цитадели.

## Ветки общего пути, существенные для переноса

| RW | Наблюдаемое действие |
|---|---|
| `0x883B5B..0x883B75` | Нет входящего объекта или его controlling player — возврат без выплаты. |
| `0x883B84`, `0x883B9A..0x883BC2` | Выплата только при true из слота `+0x84`, положительной стоимости и положительном проценте. |
| `0x883A29..0x883A7C` | Цитадель возвращает false при пересечении StatusForRingEntry с объектом; для члена орды проверяет также producer через `0x449681`. В остальных случаях true. |
| `0x883B95 → 0x68DDF1` | База выплаты: стоимость шаблона объекта (`+0x580`), с отдельной веткой опыта (`0x68DE0E..0x68DE49`). Не Object::getBuildCostPaid. |
| `0x883BB2..0x883BE0` | Достижимый reader `data+0xD4`: умножение стоимости на процент, ceil через импорт `msvcr71.dll!ceil` по `0xBD0588`, запись целого. |
| `0x883C02..0x883C32` | Условная multiplayer-ветка с дополнительным множителем. |
| `0x883C44..0x883C60` | Запрос множителя вида `0xD` у **контейнера** (`0x68C82D`); при успехе умножение выплаты. |
| `0x883C65..0x883C92` | `0x6AD8A7` со slaughter=true и шаблоном **контейнера**, затем ceil и `0x6AA858` (масштабирование денег). |
| `0x883B6B`, `0x883BEB`, `0x883CB7` | Получатель — controlling player входящего объекта: его wallet `+0x90`, не безусловно владелец здания; депозит через `0x7B18B8`. |
| `0x883CDB..0x883DB1`, `0x883DB7..0x883DFA` | Формирование floating cash text; условная выдача опыта контейнеру. |
| `0x883E2B..0x883E4B` | Процент читается ещё раз. Нулевой процент включает ветку передачи команды/улучшений, это не просто «выплата ноль». |
| `0x883EBC..0x883EE5` | Отдельный случай статуса `0x26`: базовый путь и условный учёт потери. |
| `0x883EF5..0x883F50` | Съедаемый обычный объект: Lua-событие `0xE`, kill с damage `8`, death `0x17`, destroy и учёт потери. |
| `0x883F66..0x883F73` | Несъедаемый объект: базовый путь и virtual callback `+0x80` (доставка кольца у цитадели). |
| `0x883F79..0x883FBE` | Дополнительный хвост нулевого процента: idle и обработка builder. |
| `0x883FC3..0x88403F` | Завершение входа орды: проверка слота `+0xF4`, при нулевом проценте запоминаются count/template; parent уничтожается при slaughter либо передаётся callback доставки. |

Это карта внешних веток общего метода, **не завершённая спецификация всех
вызываемых подсистем**: точные multiplayer/experience/ScaleMoney и граничная
численная семантика потребуют дальнейшего разбора до реализации.

## Сопоставление с портом и границы

- `engine/src/GameLogic/Module/GarrisonContain.h`: поле разобрано, но
  `CitadelSlaughterHordeContainBehaviorData` наследует HordeGarrisonContain.
- `engine/src/GameLogic/Object/Contain/GarrisonContain.cpp`:
  `isValidContainerFor` отказывает обычному объекту без статуса кольца;
  `addToContain` выполняет только доставку. Чтений cashBackPercent нет.
- `GarrisonContainRuntime.h`: CitadelSlaughterHordeContain наследует
  GarrisonContain; общего SlaughterHordeContain runtime нет.
- `Common/Thing/ModuleFactory.cpp`: класс без `bindModuleProc` получает
  `makeUnportedModule`; регистраций SlaughterHordeContain в runtime нет.
  Это также явно записано в S-1995. Существующий S-6010 оставлен открытым.
- `Economy::productionCostChange` и UpgradeModules уже имеют общий
  slaughter-модификатор (`0x6AD8A7`), но это только множитель, не вход,
  уничтожение, расчёт базовой стоимости, депозит или callbacks.

Затронутые будущим переносом места: оба класса контейнера, вход одиночного
объекта и членов орды, кольцевой callback, деньги/опыт, события смерти и
потери, zero-percent transfer, сохранение count/template базового Slaughter
(`0x883525..0x88353E`). Добавлять только депозит в цитадель нельзя.

## Проверки и осталось

Изменённого поведения нет, таблица «поведение — RW — тест» для реализации
не заявляется. PORT-55 тесты не добавлены и не запускались; зелёный фильтр
и соответствие в игре не заявляются. Конфигурация тестовой сборки успешна;
начатая однопоточная сборка остановлена при подтверждении scope-блокера,
до готового executable. PR реализации не открыт.

INFERENCE: соответствие полных денежных/численных и побочных эффектов
порта RotWK не установлено; изменений на основании предположения нет.
Данные Эннората в этой сессии не исследовались; сценарий владельцу не
составлялся по одной ванили или тексту issue.

Осталось: решение владельца о переносе общего SlaughterHordeContain;
после разрешения — завершить анализ зависимостей, общий runtime + xfer/crc,
тесты PORT-55 (красный/зелёный и соседний Slaughter), данные мода и PR.

## Продолжение после решения владельца (2026-11-09)

Владелец расширил #165: разрешён общий SlaughterHordeContain в GarrisonContain*,
его вход/убой/выплата/CanAlwaysEnter/callbacks и наследование цитадели.
Предыдущий scope-стоп выше — история, не действующий запрет этой части.

Проверена существующая цепочка MSG_ENTER: GarrisonCommands::enter →
AIUpdateInterface::aiEnter → AIMoveToPositionAndEnterState → aiEnterObject →
AIEnterState → contain->addToContain. Обычные войска не отсекаются отдельным
slaughter-условием; в конце GarrisonRules::canEnterObject вызывается виртуальный
isValidContainerFor. AI/command код менять не требуется по результатам чтения;
достижимость будет проверена синтетическим тестом настоящей команды.

Уточнение имени базы выплаты: +0x580 — **BountyValue**, не BuildCost.
Это тот же RW 0x68DDF1, который вызывает Economy::awardBounty. Использование
оплаченной цены не соответствовало бы RW. В тесте эти две величины различаются.
CanAlwaysEnter проверяет свой flag через RW 0x762977; требуемая маска статусов
сравнивается со статусами **контейнера** (RW 0x8838ED..0x8838F7), затем фильтр
с входящим объектом относительно игрока контейнера. Неуспех — базовый
HordeGarrisonContain::isValidContainerFor, не безусловный отказ.

## Реализация общего пути (ветка, ещё не проверено прогоном)

Коммиты `5496bd5`, `f3fd805` добавили typed-data/factory/runtime
SlaughterHordeContain на существующем GarrisonContain и наследование Citadel.
Отдельного citadel-only депозита нет. Никакие AI/command функции не изменены.

| Поведение | RW | Синтетический тест в `test_port_55.cpp` |
|---|---|---|
| Одиночный вход, BountyValue вместо BuildCost/оплаченной цены, первый ceil, однократные деньги/OnSlaughtered/deferred destroy | 0x883B4A, 0x68DDF1, 0x883BB2..0x883CB7, 0x883F22..0x883F50 | `PORT-55 ordinary citadel entry*` |
| Тот же механизм соседней бойни, неположительный процент не даёт денег | 0x883B4A, 0x883BC2 | `PORT-55 SlaughterHordeContain uses*` |
| CanAlwaysEnter: статус контейнера, bypass capacity/filter/владельца; платёж входящему игроку | 0x8838C3..0x883932, 0x883CB7 | `PORT-55 CanAlwaysEnter*` |
| Настоящий MSG_ENTER без специальной ветки AI | 0x77AC26 → 0x774EDF → contain | `PORT-55 actual enter command*`, `PORT-55 enter command sends*` |
| Убой каждого члена и завершение родителя только на последнем входе | 0x883EC9..0x88403F | `PORT-55 a slaughtered horde*` |
| Multiplayer, PRODUCTION контейнера, slaughter-cost modifier контейнера, второй ceil | 0x883C02..0x883C92 | `PORT-55 payout applies*` |
| Нулевой процент, передача команды/upgrades, счётчики прошлого отряда | 0x88373F, 0x883E2B..0x883FBE, 0x79F60E, 0x79F430 | `PORT-55 zero-percent capture*` |
| Ring predicate/callback/отложенный выход, save/load | 0x883A29, 0x8840FC, 0x884244 | `PORT-55 ring callback*` |

Новое состояние count/template включено в crc/xfer того же shared runtime.
Citadel snapshot v2 включает shared xfer; v1 читает старую ring-only базу.
ScoreKeeper получил только две узкие callback-функции для bulk transfer:
0x79F60E без enabled/counting gate, 0x79F430 только с counting gate;
существующие score-пути не изменены.

**Проверка сейчас:** изменённые GarrisonContain/ScoreKeeper объекты компилировались;
новый тестовый translation unit прошёл `g++ -fsyntax-only`. Это НЕ запуск тестов.
Полная tests-only Release сборка `openbfme_tests -j1` ещё идёт; фильтр
`PORT-55*`, red-without-fix и зелёные doctest totals пока отсутствуют. PR нет.
На Linux базовый CameraShaker.h не включает cstddef для size_t: обход только
локальный CMake `-DCMAKE_CXX_FLAGS='-include cstddef'`, изменения чужого
клиентского header в итоговом diff нет. Хэши/старые тесты не менялись.

### Сохраняемые ограничения и ночной риск

INFERENCE/неполный presentation: деньги направлены через существующий
Economy::noteIncome. Точная RW 0x883CDB..0x883DB1 позиция/рандомизация текста,
цвет и масштаб не перенесены; S-283/S-1100 не закрыты. Визуальное сходство
и соответствие в запущенном Эннорате **не проверены**.
S-1995 теперь не означает отсутствие runtime SlaughterHordeContain, S-6010
больше не является запретом обычных войск, но завершение задачи ещё требует
прогона. Общие риски: admission гарнизонов, денежные/опытные модификаторы,
score, смерть последнего члена, ownership/upgrades, старые save4 и crc.

Старые `ring delivery retail*` сохранены без изменений. В частности, тест
`fortress refuses ordinary troops*` содержит буквальный старый stopLine и
предположение об отказе обычному солдату: ожидание необходимо обсудить в PR,
не подгонять код под него. FACT-B inventory/hash pins этой веткой не обновляются.
Осталось: закончить сборку/узкий прогон, исправить выявленные ошибки,
доказать red-without-fix, проверить доставку Кольца, затем fetch/rebase main,
PR и финальный dashboard-комментарий `review` только при зелёном фильтре.

### Рецепт проверки владельцем (данные Эннората 1.9.8.2)

По `docs/CHEAT_MODE.md`: skirmish с Cheat Mode и одним компьютерным игроком;
в Esc → Cheats → Game приостановить AI соперника. Через Units / structures
создать свой **MordorFortressCitadel**, через Units — своего **MordorFighter**
(при наличии в списке только отряда — MordorFighterHorde и считать вошедших
членов отдельно). Эти реальные имена и фильтр сверены с mod INI, не с ванилью.
Не ставить уровни, production/cost upgrades или handicap. Исключить соседние
источники дохода либо сравнить контрольный одинаковый промежуток времени.
Записать кошелёк, дать обычный приказ входа в цитадель. Убедиться, что солдат
исчез, деньги выросли однократно; базовая формула для этого солдата в
неизменённом моде `ceil(1 * 1.5) = 2` до multiplayer/production/cost/handicap.
Для полной орды сумма — по реально вошедшим членам, не от BuildCost орды.
Повторить после save/load и с чужим солдатом (отказ при стандартных allow
флагах). Не проверять доставку Кольца на MordorFortressCitadel Эннората:
его ring-поля закомментированы. Ring regression проверяется отдельно
синтетическим fixture и существующими retail тестами, не выдуманным рецептом
для этого мод-объекта. Проверка владельцем пока не проведена.
