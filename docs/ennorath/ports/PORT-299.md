# Port 299 — PorcupineFormationBodyModule (fork issue 691)

## Итог

Код движка уже соответствует достижимому поведению RotWK 2.01: `PorcupineFormationBody` в порте — `ActiveBody`, а три поля модуля не влияют на crush или получение урона, потому что все их читатели находятся за гейтом, который безусловно возвращает `false`. Патч, заставляющий это тело отклонять crush или запускать его оружия, перенёс бы недостижимые ветви, а не retail-поведение. Владелец подтвердил в игре, что наблюдаемая потеря HP у конницы возникает по отдельному объектному `CrushRevengeWeapon` и обычным бонусам оружия против конницы, а не от этих полей. Поэтому `test_port_299.cpp` и `Port299.cmake` намеренно не добавляются; источник наблюдаемого эффекта описан ниже.

Источники: RotWK 2.01 `game.dat` (MD5 `daed3668006cd90f01c34e5a7da1901f`), все указанные адреса повторно прочитаны через `tools/mod/rw.py dis` и `xref`; Ennorath 1.9.8.3 field audit подтверждает три `parsed-unused` поля (50 определений каждого); BFME2 1.06 донор использован только для сверки, не как источник поведения RotWK. Значения INI и игровые файлы не включены.

## Цепочка класса и таблица полей

`module-registry.json` связывает фабрику RW `0x65156D` с `PorcupineFormationBodyModule`; фабрика вызывает конструктор `0x8C619A`, который вызывает `ActiveBody` (`0x8C3841`) и устанавливает runtime-vtable `0xC72FC8`, `0xC72F08` и body-interface vtable `0xC72E58`. Отдельная data-фабрика `0x6515A8` вызывает конструктор данных `0x8C63F1`, который вызывает `ActiveBodyModuleData` (`0x8C373F`), обнуляет поля `+0x64`, `+0x68`, `+0x6C` и устанавливает data-vtable `0xC73028`.

Парсер вызывается через `0x8C617F` и добавляет таблицу `0xC72E18`. Записи таблицы, считанные из retail-образа:

| Поле | Парсер RW | Смещение в data | Порт |
|---|---:|---:|---|
| `DamageWeaponTemplate` | `0x73AE79` | `+0x64` | разбирается в `CombatModules.cpp` |
| `CrushDamageWeaponTemplate` | `0x73AE79` | `+0x68` | разбирается в `CombatModules.cpp` |
| `CrusherLevelResisted` | `0x42EB2A` | `+0x6C` | разбирается в `CombatModules.cpp` |

`0x73AE79` получает `WeaponTemplate` через `TheWeaponStore`; `0x42EB2A` разбирает целое и записывает байт только в диапазоне `-128..127` (`0x42EB42..0x42EB4F`). В порте названия и типы полей уже разбираются; новых readers для них нет (`git grep` по `engine/src`).

## Четыре uncited-функции из карты

Адреса — начала функций в RotWK. Слоты primary-vtable проверены по самой таблице `0xC72FC8`, установленной конструктором `0x8C619A`.

| Начало RW | Слот / первая инструкция | Классификация | BFME2 1.06 donor и решение |
|---|---|---|---|
| `0x8C60BF` | primary slot 1; `jmp 0x8C35AF` | Перенаправление на общий `ActiveBody::loadPostProcess` (RW `0x8C35AF`), не обработчик crush-полей. Декомп-колонка карты называет `Rva004C1B7B::~Rva004C1B7B`, но этот ярлык не согласуется с первой инструкцией и фактическим vtable-слотом. | `DONOR BFME2 0x004BF40F`, `Rva004BF40FLoadPostProcess.cpp`; в donor `ActiveBody.h` это slot 1 `loadPostProcess`. Общая dispatch-механика модулей уже остановлена в S-7408, не обходить её в этом модуле. |
| `0x8C61C4` | primary slot 2; `mov eax, 0xC0A984; ret` | Возвращает имя `PorcupineFormationBodyModule` (строка найдена `rw.py str` по адресу `0xC0A984`); только идентификация/snapshot name. | `DONOR BFME2 0x004C2010`, `ConstIntGetters4.cpp` (`Rva004C2010Get`), возвращает это имя; поведения нет. |
| `0x8C61E3` | primary slot 4; lazy-init статического ключа через `0x5487EC` со строкой `0xC0A984` | `getModuleNameKey`; идентификация для пула/реестра, не gameplay. | `DONOR BFME2 0x004C2034`, `PorcupineFormationBodyModulePoolKey` в `ImmortalBody.cpp`; поведения нет. |
| `0x8C6228` | primary slot 0; `push esi; mov esi, ecx; call 0x8C61CA` | Scalar deleting destructor: `0x8C61CA` восстанавливает vtables и вызывает базовый деструктор; при установленном флаге `0x8C6228` освобождает память через `0x42F6A0`. Только жизненный цикл объекта. | `DONOR BFME2`, `PorcupineFormationBody.cpp`: derived destructor пустой; не gameplay. |

`0x8C61CA` — вызываемый dtor helper, не отдельная одна из четырёх функций. Xfer RW `0x8C5524` уже выполняет версию и `ActiveBody::xfer` (`0x8C3388`); новые состояния нет.

## Reachability полей и поведение ветвей

Гейт RW `0x8C6244` проверяет `module+8` (Object), `Object+0x27C` (contained-by) и `container+0x258` (contain interface). Если что-либо отсутствует, он идёт на `0x8C6266`; при наличии всех трёх вызывает contain-vslot `+0x7C` (`0x8C6263`) и всё равно выполняет `xor al, al; ret` на `0x8C6266..0x8C6268`. Значение виртуального вызова не используется: результат гейта всегда false. `rw.py xref 0x8C6244` находит ровно два вызова: crush-slot `0x8C62C4` и damage-slot `0x8C63CD`.

- Body-interface crush-slot RW `0x8C62BB` (`0xC72E58 + 0x9C`): при false из гейта `0x8C62CB` не прыгает в ветку `0x8C62D1`; `inc al` на `0x8C62CD` превращает 0 в 1 и эпилог `0x8C6338..0x8C633A` разрешает crush. В недостижимой ветке true: отсутствующий Object/data или нулевой byte `data+0x6C` разрешает crush; иначе `crusherLevel` вызывается по `0x695070`, signed-сравнение `data+0x6C` с уровнем на `0x8C62F6..0x8C62F9` разрешает crush при `CrusherLevelResisted < crusherLevel`; при обратном случае null `data+0x68` отказывает в crush, а ненулевой `CrushDamageWeaponTemplate` проверяется по дальности (`0x9E68DC`, helper `0x8C6269`), при попадании выстреливает через `0x6CF590`, затем отказывает в crush. Все эти чтения лежат после гейта и не исполняются.
- Body-interface damage-slot RW `0x8C63C4` (`0xC72E58 + 0x00`): после false из гейта `je 0x8C63E5` на `0x8C63D8` пропускает `call 0x8C633D`; затем вызывает `ActiveBody::attemptDamage` RW `0x8C3FA3`. Helper `0x8C633D` читает `data+0x64` на `0x8C6356`, проверяет источник/состояние/PROJECTILE и дальность и только затем может вызвать `0x6CF590`; `rw.py xref 0x8C633D` находит единственный call-site `0x8C63DC`, внутри пропускаемой ветви. Range helper `0x8C6269` имеет только два call-site (`0x8C6319`, `0x8C63A2`), оба в тех же недостижимых ветвях.

Таким образом, фактический ответ тела на crush — `allow`, а `DamageWeaponTemplate` и `CrushDamageWeaponTemplate` не стреляют через эти функции. Это локальный вывод о `PorcupineFormationBodyModule`, а не утверждение, что в игре пиковая формация не замедляет конницу или что пика не наносит ей урон другим путём.

## BFME2 donor cross-check

`DONOR BFME2 0x004C2095` (`PorcupineFormationBodyModuleSlots.cpp`) вызывает contain slot 31, если цепочка существует, затем безусловно возвращает false. Поэтому donor `rva004C2127` (`0x004C2127`) уходит по `if (!rva004C2095()) return true;` прежде чем читает `+0x6C` или `+0x68`; donor `attemptDamage` (`0x004C2230`) зовёт `rva004C21A9` только при истинном результате той же проверки, поэтому не доходит и до `+0x64`. RW подтверждает это независимо: его `0x8C6244` тоже сбрасывает результат вызова в false. У donor нет живой ветви, которую следовало бы перенести вопреки RotWK.

## Наблюдаемый урон пиков: CrushRevengeWeapon объекта

Владелец подтвердил в игре, что конница теряет HP при наезде на пикейный строй; это отдельный путь объекта, а не читатель полей `PorcupineFormationBodyModule` ([подтверждение владельца в issue #691](https://github.com/Gleb302596555/openbfme-godot/issues/691#issuecomment-7013419413)). В Ennorath у `AngmarDunedainPikeman` задан `CrushRevengeWeapon = DarkDunedainCrushRevenge` (урон 250, тип `SPECIALIST`; мумаки и великие звери Мордора исключены), а у `AngmarDunkelelfPike` — `AntiCavalryInfantryCrushRevenge`. Дополнительно обычные weapon templates пикейщиков имеют бонусы против конницы.

Месть за давку — свойство объекта `ThingTemplate`, хранимое в `Object + 0x3BC`, отдельно от `PorcupineFormationBodyModule.CrushDamageWeaponTemplate` (`data + 0x68`). Порт создаёт это оружие в `ObjectWeapons::makeCrushWeapons` по данным шаблона (RW init `0x693E19..0x693E79`) и `SquishCollide::onCollide` запускает его через `RW 0x6CF328`; соответствующая уже существующая реализация находится в `engine/src/GameLogic/Combat/ObjectWeapons.cpp` и `engine/src/GameLogic/Object/Collide/SquishCollide.cpp` (S-580). В retail оно не срабатывает, если жертва давки flanked нападающим. Замедление конницы формацией — отдельные `AttributeModifiers` (`CRUSHED_DECELERATE`, S-580). Таким образом, наблюдаемый эффект не доказывает чтение трёх полей тела.

Комментарий мода у `PikemenPorcupineCrushDamage` («10 -> 25 … Battle Wagon») — предположение авторов о причине эффекта; в RotWK 2.01 тело не исполняет соответствующую ветвь, как показано выше.

## Порт, тесты и итог

`CombatModules.h` оставляет `PorcupineFormationBody` наследником `ActiveBody` без override для `attemptDamage` или crush-refuse; `SquishCollide.cpp` учитывает always-allow результат RW. Это совпадает с достижимыми body-slot ветвями, а наблюдаемая потеря HP объясняется отдельным объектным `CrushRevengeWeapon` (см. выше). `PORT-299*` намеренно не добавляется: нет нового body-specific поведения, которое следовало бы тестировать.

Итог выбран владельцем после проверки в оригинале: три поля остаются `parsed-unused`, runtime-порт и body-specific тесты не нужны. S-7191 остаётся без изменений; общий `loadPostProcess` slot покрыт S-7408. Новые stop-фрагменты с зарезервированными S-7539..S-7541 не создаются. `docs/ennorath/port56-porcupine-rw.md` содержит исправленную классификацию slot 1 / slot 4 после повторного чтения vtable; generated function map не менялся.
