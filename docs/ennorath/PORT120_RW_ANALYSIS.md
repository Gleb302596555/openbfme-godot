# PORT-120 / P-55: цифра усиления над усиленными юнитами (issue #304)

**Вердикт: исследовательский стоп (`status=stop`), без правки движка, без `PORT-120*`
теста и без PR.** В RotWK 2.01 (`game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`,
 caveat S-001), в BFME2 1.06 donor-first (`/tmp/open-bfme-2`) и в данных Эннората
1.9.8.2 найдены две кандидатные цепочки, но ни одну нельзя доказать как «цифру, на
которую усилен юнит» владельца. Подставлять вместо неё другой механизм нельзя
(PROCESS §7а, §15, §16): задача прямо требует `what / where / value` из RW/mod-данных,
а не угаданное значение.

Base/ветка: `arena/c821bbc6-openbfme-godot`, после `git fetch origin && git rebase
origin/main` = `d8f6cdd0`. `git diff --stat origin/main...HEAD` до правок пуст.
Полный таргет `openbfme_tests` собран на прежнем base `7907840` (`[1640/1640]`,
`exit_code=0`, `BUILD_DONE`); после rebase движок не пересобирался, потому что
**код движка не менялся**.

## 0. Что искали

P-55 (`docs/ennorath/PROBLEMS.md:604`): «Над усиленными юнитами не видно цифры
усиления», подсистема в формулировке владельца — `DrawableIconUI`. Issue #304
заранее НЕ устанавливает элемент-носитель и называет три варианта: иконка/текст
`DrawableIconUI` (RW `0x677994` contain pips, TOP20 #17), плавающий текст
`AttributeModifier`, sub-object модели. Требование: сначала данные Эннората, затем
RotWK 2.01 с адресами, donor BFME2 1.06, первое расхождение port vs RW, общий
механизм один раз для всех объектов; если причина — целый непортированный модуль
/ shared system, stop с механизмом, местами RW/port и follow-up, без заменителя.

Проверено, что в `origin/main` (`1faca4af` → `d8f6cdd0`) P-55/list 5 не
реализованы: `DrawableIconUI.cpp:966` по-прежнему сообщает «not ported: ... list 5»,
`git grep -iE 'P-55|EXLdrshp|0x677C|list 5' origin/main` находит только
`docs/ennorath/PROBLEMS.md:604` и старые стопы S-1950/S-4642. Новые
`Port121.cmake`, `test_port_121.cpp`, `PORT109_RESEARCH_STOP.md`,
`PORT117/119/115/111/106_RW_ANALYSIS.md` к P-55 отношения не имеют.

## 1. Данные Эннората (сначала mod data)

Источник: `google_drive/ennorath-ini-1.9.8.2.zip` (`1oUjqDsG_I8Eyft0Z7gHTPLDsuVXtYIW0`),
распакован в `/tmp/enn`. Архив содержит только INI/INC/BSE/AI (1893 файла), W3D/DDS/TGA
ассетов в нём нет.

Усиление лидерством задано уровнями, но **число уровня не записано отдельным INI-полем**:

- `/tmp/enn/data/ini/includes/attributemodifier.inc:2159..2167`
  `ModifierList HornvonBoromirfrVerbndete`: `Category = SPELL`,
  `DAMAGE_MULT 150%`, `ARMOR 50%`, `Duration = 60000`,
  `FX = FX_GenericLeadership60Lvl1`, `FX2 = ...Lvl2`, `FX3 = ...Lvl3`,
  `MultiLevelFX = Yes`. Аналогичный triplet у
  `attributemodifier.ini:3912 CreateAHeroHowlOfRage_Level1`.
- `/tmp/enn/data/ini/fxlist.ini:2541..2580`:
  `FXList FX_GenericLeadership60Lvl1/2/3` → `BuffNugget` `BuffType = LeaderShip`,
  `BuffInfantryTemplate = LeadershipBuffInfantryLvl1/2/3`,
  `BuffCavalryTemplate = ...Lvl1/2/3`, `BuffTrollTemplate = ...`,
  `BuffOrcTemplate = ...Lvl1/2/3`, `IsComplexBuff = No`, `BuffLifeTime = 61000`.
- `/tmp/enn/data/ini/object/system/buff.ini:56..64, 83..91, 110..118, ...`:
  `Object LeadershipBuffOrcLvl1/2/3` → `Draw = W3DBuffDraw ModuleTag_Buff01`,
  `ModelName = EXLdrshpOrc1 / EXLdrshpOrc2 / EXLdrshpOrc3`, `PreDraw = Yes`;
  у кавалерии `EXLdrshpCvlry1/2/...`, у `WKLeadershipLvl1` — `exwkleader`.
  Комментарии в самом файле говорят: «The W3DBuffDraw handles normal drawing — this
  happens in the W3DBuffBuffer, and there is no corresponding drawable or object».
- `drawgroupinfo.ini` содержит только поля group-number текста (`UsePlayerColor`,
  `ColorForText`, `DropShadowOffsetX/Y`, `FontName`, `FontSize`, `FontIsBold`).
  `grep -rniE 'Number' data/ini` даёт только `NumberImages`, `NumberTiles`,
  `NumberAllowed` (другие системы). `attributemodifier.ini/.inc` содержат `LEVEL`,
  `CRUSHABLE_LEVEL`, `MultiLevelFX`, но нет `Number`, `BoostNumber`,
  `DisplayLevel`, `ModifierLevel`. `fxparticlesystem.ini/.inc` `EXLdrshpLvl1a/2a/2b/3a.tga`
  — 40 посторонних particle systems, не под-объекты с цифрой.
- Проверка дублей `Object` (важно для цепочки A ниже): 6247 определений, 18
  повторяющихся имён — все из `livingworldicons/*.ini`, `livingworldbuildingicons.ini`,
  `livingworldbuildploticons.ini`, `livingworld.ini`, `civilianunit.ini` / `civilianbuildings.ini`:
  `5k`, `Banner`, `Building`, `Decal`, `Helm`, `Hilighted`, `HilightedRing`, `LargePip`,
  `MediumPip`, `ProducingUnit`, `RallyPoint`, `Scaffold`, `Selected`, `SelectedGlow`,
  `SmallPip`, `Soldiers`, `Soldiers_Medium`, `Soldiers_Small`. Среди
  `LeadershipBuff*Lvl1/2/3` и других buff-шаблонов дублей нет.
- Patch Notes (`1KfuXsb8...` → `/tmp/patch-note.txt`, `1K7DAe0...` →
  `google_drive/Ennorath mod Patch Note` → `/tmp/patch-note-2.txt`, 2801 строка)
  описывают уровни отрядов / казны («как в ваниле»), но не P-55, не `EXLdrshp*`,
  не «цифру усиления» и не RW-адрес.

**Вывод по данным:** уровень буста в Эннорате кодируется выбором одного из трёх
`FXList` / одного из `LeadershipBuff*Lvl*` / одного из имён W3D-моделей с
суффиксом `1/2/3`. Отдельного поля «цифра» нет. Это разветвляет поиск на две
цепочки: A — клиентский list 5 `DrawableIconUI`; B — модель `W3DBuffDraw`.

## 2. Цепочка A: list 5 — Shift + счётчик переопределений шаблона, не буст

### RW 2.01 (все адреса из `workspace/RotWK/game.dat`, `python tools/mod/rw.py ... dis/xref/str`)

Постановка в очередь (`Drawable` decorations, RW `0x679129`):

```
00679129  push ebp ; ... 00679169 call 0x6778F4  (health-box anchor)
006792BC  push 1 ; 006792BE call 0x646582      (list 1)
006792C9  push 2 ; 006792CC call 0x646582      (list 2)
006792D1  mov  ecx, [0xDE4334]
006792D7  call 0x63F122                       (Keyboard::isShift)
006792DC  test al, al ; 006792DE je 0x6792F7
006792E0  mov  eax, [edi+4]                    (Object -> ThingTemplate)
006792E3  cmp  dword ptr [eax+0xC], -1
006792E7  je  0x6792F7
006792EF  push 5 ; 006792F2 call 0x646582      (list 5)
```

`0x646582` (`lea ecx,[ecx+eax*4+0xF8] ; call 0x6463F4`) кладёт узел в список
слоя `layer`. `xref 0x646582` = `0x679297 / 0x6792BE / 0x6792CC / 0x6792F2 /
0x67932D`. `0x646019` вызывает общий диспетчер `0x679A04`:

```
00679A04  mov eax,[esp+4] ; sub eax,0 ... dec eax x5
00679A1F  call 0x677C0C                        (case 5 -> numeral)
00679A26  mov ecx,esi ; call 0x673F6D         (case 3 -> group number, list 3)
00679A2F  mov ecx,esi ; call 0x67939F         (case 2 -> veterancy)
00679A38..0x679A4F                            (case 1: health, 0x677994 contain pips)
00679A56..0x679A6D                            (case 0: 0x676346 / 0x677CF9 ...)
00679A72  ret 4                               (case 4: caption, вызовов нет)
```

Цифра list 5 (`RW 0x677C0C`, donor twin BFME2 `0x002778E1`):

```
00677C13  mov ebx,[ecx+0xFC]                  (Object)
00677C4A  call 0x68DF5B                       (тот же «unrowed» query, что и у health bar)
00677C57  mov eax,[ebx+4]                     (ThingTemplate)
00677C5B  mov edi,[eax+0xC]                   (счётчик)
00677C5E  lea eax,[edi+1] ; 00677C61 cmp eax,9 ; 00677C66 push 9 / pop eax  => min(count+1,9)
00677C69  mov ecx,[0xDE4518]                   (DisplayStringManager)
00677C73  call dword ptr [edx+0x40]           (getNumeralString)
00677C7C  fld [0xBDE8C8]  = 1.5f ; 00677C8C fld [0xBD889C] = 2.0f
00677C95  call dword ptr [eax+0x44]           (setScale(2.0, 1.5))
00677CA0  call dword ptr [eax+0x7C]           (client frame) ; +edi ; test al,0xA
00677CAB  call 0x68B678 ; 00677CB0 mov eax,[eax+0x2A0]  (player color) / else -1
00677CBD  push 0xFF000000 ; 00677CC5 call [edx+0x28] (setColor)
00677CC8  mov eax,[0xDE7720] ; +0x14/+0x18 (TheDrawGroupInfo drop offsets)
00677CDD  call dword ptr [edx+0x38]           (draw)
00677CE0  fld1 ... 00677CF1 call [eax+0x44]    (setScale(1,1))
```

`0x63F122` (`mov ax,[ecx+0xC] ; test al,0x30 ; test ah,4`) совпадает с
BFME2 `Keyboard::isShift` (`Code/GameEngine/Source/GameClient/Input/Keyboard.cpp:595`,
маски `KEY_STATE_LSHIFT 0x10 | KEY_STATE_RSHIFT 0x20 | KEY_STATE_SHIFT2 0x400`).
В порту та же маска `KEY_STATE_SHIFT` (`engine/src/GameClient/MessageStream/MetaEvent.h:73..75`,
`engine/src/GameClient/InGameHud.cpp:339`).

`float [0xBD889C] = 2.0f`, `[0xBDE8C8] = 1.5f` прочитаны прямо из PE; строка
`NUMBER:%d` RW `0xBDDDA0` имеет только xrefs `0x48FEDF`, `0x7053F2`, `0x70546B`
(стартовые позиции / 10 display strings); `rw.py str "NUMBER:"` для Buff-строк пуст.
Плавающий текст в доноре (`addFloatingText`) вызывают только деньги, bounty,
refund, cashhack, deposit, supply — не буст.

### Что такое `ThingTemplate +0x0C`

- DONOR BFME2 `Code/GameEngine/Source/GameClient/DrawableDrawIconUI.cpp:19..24, 182..183`:
  «layer 5 with shift held for templates whose +0x0C is not -1» →
  `if (TheKeyboard->isShift() && obj->getTemplate()->m_0C != -1)
  TheGameClient->addIconLayer(5, this);`.
- DONOR BFME2 `Code/GameEngine/Source/GameClient/DrawableDrawLevelNumber.cpp`:
  «draws a numeral ... in the manner of Zero Hour's drawGroupNumber ...
  min(object +0x04 record's +0x0C count + 1, 9), scaled 2.0 x 1.5 ...». Тело:
  `Int count = obj->getLevelRecord()->m_count; ... getNumeralString(number)`.
- DONOR BFME2 `Code/GameEngine/Source/Common/Thing/ThingFactoryReset.cpp:59..70, 177..189`:
  `class ThingTemplate : public Overridable { unsigned int m_matchingReplacementCount;
  ... }` — `+0x0C` после `Overridable`; `addTemplate()` на повтор имени обнуляет
  счётчик старого/нового шаблона и `_++currentTemplate->m_matchingReplacementCount`
  для каждого одноимённого шаблона с `m_templateID >= g_replacementTemplateCounter.nextID`.
  `ThingFactoryNewOverride.cpp`, `NewTemplate.cpp`,
  `ThingTemplateAssignment.cpp`, `ParseObjectDefinition.cpp` подтверждают
  override/ChildObject/ObjectReskin, не буст.
- DONOR `Common/Overridable.h`: у `+0x0C` «no recovered name» — не утверждаем
  оригинальный символ.
- RW: ctor `0x73FB7A` → `mov dword ptr [ebx+0xC], -1` (`0x73FB9A`);
  `newTemplate` `0x6D2840` вызывает `0x6D10DE` в `0x6D2868` (единственный xref).
  `0x6D10DE`: lookup по имени `0x5DD47E`, вставка в head
  (`mov [edi+0xC],esi` / `mov [esi+0xC],0` на `0x6D115B..0x6D1162`), затем цикл
  `0x6D1172..0x6D119C` по `+0x494` со сравнением имён `0x4065AA` и
  `inc dword ptr [edi+0xC]` на `0x6D1191`. Таблица полей `ThingTemplate`
  RW `0xDA3DF8` (xrefs `0x73BDF5`, `0x73BEFC`, `0x73BF50`, `0x73C143`,
  `0x73E8CA`) распарсена: 191 строка, строки со смещением `+0x0C` **нет**.
  Значит `+0x0C` не INI-поле и не уровень лидерства.

### Почему A ≠ P-55

Цифра list 5 включается **зажатым Shift**, её значение — число одноимённых
переопределений `ThingTemplate` + 1, а не `DAMAGE_MULT`, не `ARMOR`, не номер
`FX/FX2/FX3` и не `LeadershipBuff*Lvl*`. В Эннорате дубли `Object` есть только
у UI/livingworld-иконок; у `LeadershipBuff*Lvl1/2/3` и у обычных юнитов под
лидерством дублей нет, поэтому в RW они не получают list-5 request. Тест из
issue («synthetic boosted unit yields RW draw request (what, where, value)»)
на цепочке A не может быть написан честно: у усиленного юнита нет `+0x0C != -1`.

Первое расхождение port vs RW для цепочки A действительно есть и оно одно:
`engine/src/GameClient/DrawableIconUI.cpp:966` (`acceptanceStops`, S-1950),
`IconUISettings` (`DrawableIconUI.h:105..130`) не имеет `shiftHeld`,
`IconUIOp::TEXT` (`DrawableIconUI.h:58..80`) не имеет `scaleX/scaleY`,
`engine/src/GodotDevice/GodotInGameHud.cpp:1171..1198` рисует TEXT без 2.0×1.5,
`InGameHud.cpp:806` вызывает `m_iconUI.build(...)` без передачи `m_keyState`.
Но портировать list 5 как «цифру усиления» было бы **заменителем**: это добавило
бы отладочную цифру переопределений, а не boost number. Поэтому не сделано.

## 3. Цепочка B: `W3DBuffDraw` / модель — механизм есть, ассет-носитель не доказан

### RW 2.01 и donor

- `BuffFXNugget::doFXObj` (RW `0x5E2988`, donor
  `Code/GameEngine/Source/GameLogic/Object/BuffNuggetFXNugget.cpp`): выбирает
  `Buff*Template` по `KindOf` (CAVALRY / ORC / INFANTRY / TROLL / MUMAKIL / SHIP /
  MONSTER / generic), `BuffLifeTime <= 0` → `removeBuff`, complex buff →
  `addBuff(nullptr)`, horde → return; зовёт `Drawable::TurnOnBuff` RW `0x671E56`.
  В порту: `engine/src/GameClient/FXList.cpp:1083..1133` (тот же выбор, без цифры).
- `BuffManager::TurnOnBuff` RW `0x764483`, `BuffInstance::MakeNormalBuff`
  RW `0x7641A2`, `BuffLogic::addBuff` RW `0x707FFF` требует, чтобы первым draw
  module шаблона был `W3DBuffDraw` (vslot 17 RW `0x8CEF91`), иначе ошибка.
- `W3DBuffDrawModuleData`: ctor RW `0x4CE61A`, `buildFieldParse` RW `0x4CE661`,
  таблица RW `0xBE318C` (прочитана побайтно): строка 1 `ModelName` RW `0xBE2E70`,
  parser `0x42EE5E`, offset `+0x08`; строка 2 `PreDraw` RW `0xBE3184`,
  parser `0x42E558`, offset `+0x0C`; терминатор. Vtable RW `0xBE3010`, её slot 11
  (`0xBE303C`) = `0x9F3A3C` = `ret 4` (`doDrawModule` no-op, S-113/S-4642).
  DONOR `W3DBuffDraw.cpp` содержит ровно ту же таблицу из двух полей.
- `W3DBuffBuffer::addBuff` RW `0x4D3376`, `addBuffType` RW `0x4D1981` копирует
  `m_modelName` / `m_preDraw`, `drawBuffs` RW `0x4D20C6` (fill RW `0x4D1ACD`)
  строит вершины / UV / vertex color / additive `emissive * vertexColor * opacity`.
  DONOR `W3DBuffBuffer.cpp`: `MAX_BUFF_VERTEX=30000`, `MAX_BUFF_INDEX=60000`,
  `types[10]`, `buffs[10]`, `addBuff` max 1000, ALIGNED `0x800` / `Obj_Look_At`
  каждые 10 кадров — **ни одного numeral / текста / display string**.
  Web/форумные описания «Visual's for Leadership and Buff's» (BFME2 1.09),
  RJ-RotWK 1.06 selection decals, `the3rdage` `FX_GenericLeadershipLvl1/2/3`
  + `MultiLevelFX` не являются RW-доказательством и не дают test expectation.

### Порт (уже реализованный общий путь, S-4642)

`engine/src/GameClient/BuffDraw.h` (typed `W3DBuffDrawModuleData`: `m_modelName
+0x08`, `m_preDraw +0x0C`), `engine/src/GameClient/LiveFX.cpp:438 turnOnBuff`,
`engine/src/GameClient/FXPlayback.cpp:132` (`m_world->turnOnBuff(o,type,*tmpl,life)`),
`engine/src/GodotDevice/GodotGameWorld.cpp:2860 syncBuffs`: по каждому
`ClientBuffs::Buff` создаёт `W3DInstancer` instance модели `b.model` в позиции
drawable с identity basis, instance scale и opacity (`b.opacity`, hidden → 0).
Никакого TEXT/numeral запроса нет. S-4642 уже честно фиксирует неперенесённое:
additive preDraw/postDraw split и camera-facing ALIGNED meshes.

### Недостающее звено

Цифра может быть запечена в W3D-модели `EXLdrshpOrc1/2/3`,
`EXLdrshpInftry1/2/3`, `EXLdrshpCvlry1/2/3`, `exwkleader` как sub-object /
текстура / billboard. Суффикс `1/2/3` в имени модели — **INFERENCE**, что это
уровень; байты ассета это не подтверждают. Ассеты отсутствуют:

- `ennorath-ini-1.9.8.2.zip` — только INI/INC/BSE/AI;
- папка `for-arena` (`1h9QIzh3gpvSvVB4L_m9QlMs6d56tGY8S`) — 11 файлов, запросы
  `w3d|W3D|EXLdrshp|art|assets` дали 0;
- git/OpenBFME/Godot — `git grep EXLdrshp` только INI-ссылки, `find ... *.w3d` пуст;
- `strings game.dat` — нет `EXLdrshp` / leadership / buff-number строк;
- GitHub code search `EXLdrshp` — 5 INI, 0 W3D; дерево `dagor-team/updates-dagor.big-file`
  — 51 посторонний W3D;
- `image_search` по точным именам и web_search по `site:ennorathmod.ru` / «цифра
  усиления» / «цифра над юнитами» — только герои/эмблемы/скриншоты без цифры;
- единственный возможный архив с ассетами — Drive
  `1XGjCzCOYBo9V072lMu8c_ByCEgXfarY0` `OpenBFME-Ennorath-2026-11-08.zip`,
  109047911 B, md5 `8e0a40230f7c215b10fe9b56224a30b3`. Бинарно недоступен:
  `download_file` отказывает >100 MiB (104857600 B), `read_file_text` >25 MB
  extraction cap, `download_file return_download_url=true` тоже отказан,
  `fetch_page` на `uc?export=download` / `usercontent...download?confirm=t`
  возвращает «too large for Google to scan for viruses», `open?id` → 404,
  прямой `curl` → `SSL_ERROR_SYSCALL`; `list_connector_tools google_drive` не
  имеет range/stream инструмента.

Поэтому нельзя ни доказать, что модель несёт цифру, ни узнать её `what/where/value`
в терминах issue, ни добавить второй rendering path (ALIGNED/camera-facing) без
RW/mod-доказательства связи именно с P-55. Это и есть «whole shared system /
files needed» из условия stop.

## 4. Итог по первому расхождению и по тестам

| Кандидат | Первое расхождение port vs RW | Можно ли принять за P-55 |
|---|---|---|
| A, list 5 | `DrawableIconUI.cpp:966`, нет `shiftHeld` / scale 2.0×1.5 / TEXT-масштаба | Нет: RW даёт Shift-gated `m_matchingReplacementCount+1`, у buff-шаблонов Эннората дублей нет |
| B, `W3DBuffDraw` | Порт уже портировал механизм (S-4642); не хватает W3D-ассета и ALIGNED-рендера | Не доказано: RW/донор несут модель/меш/цвет, не цифру; ассеты бинарно недоступны |

`PORT-120*` не добавлен: PROCESS §15 требует ожидания теста из RW/mod-данных, а не
из INFERENCE. `hud5 icons*`, `test_spell4_buffs.cpp`, `PORT-109*`, `PORT-121*`
не менялись. Состояние симуляции, xfer, `*.hashes`, `GodotFXPlayer.*`,
`FXEvents.*`, `W3DModelDraw.cpp/.h`, `HordeContain*`, чужие Ports 98–113 и
Local 33–40 не тронуты. Ночной риск от ветки нулевой (правки движка нет),
широкий nightly прогон для этой ветки не требуется.

## 5. Почему именно stop (PROCESS §7а / §15 / §16)

1. **Поведение RW для P-55 не определено**: нет INI-поля, RW-строки, формата или
   ветки, которая называла бы «цифру усиления» как число уровня лидерства.
   `NUMBER:%d` и floating text проверены и исключены.
2. **Нужен архитектурный выбор носителя** (list 5 / floating text / W3D sub-object),
   который задача прямо оставила открытым; два из трёх исключены, третий упёрся в
   ассеты.
3. **Нужны файлы / запись оригинала**: W3D-ассеты `EXLdrshp*` и скриншот/видео,
   где видна цифра, с объектом, картой, источником буста и уровнем.

По правилу «no substitute / no second path» не построен ни generic list 5 под
видом P-55, ни новый текстовый/рендер-путь, ни угаданный `value=1/2/3`.

## 6. Что нужно от владельца, чтобы закрыть P-55

1. Запись оригинала (скриншот/видео): какой объект Эннората, какая карта / скирмиш,
   какой герой или power даёт буст, какая цифра видна (1/2/3?), видна ли она без
   зажатого Shift. Это отсечёт цепочку A окончательно.
2. W3D-ассеты `EXLdrshpOrc1/2/3`, `EXLdrshpInftry1/2/3`, `EXLdrshpCvlry1/2/3`,
   `exwkleader` (или ссылка на архив <100 MiB / расшаренный диапазон), чтобы
   прочитать sub-object / текстуру и подтвердить цифру. Текущий 109 MiB Drive zip
   бинарно недоступен инструментам сессии.
3. Если владелец знает, что цифра — это list 5 при Shift: назвать объект с
   `ThingTemplate +0x0C != -1`, который одновременно находится под лидерством;
   тогда follow-up — отдельный HUD-5 list-5 порт (S-1950/S-7319), а не P-55.
4. Если цифра запечена в модели: follow-up — импорт ассета + camera-facing
   ALIGNED-меши (S-4642) и тест, который пинит `ModelName` для
   `FX_GenericLeadership60Lvl1/2/3` → `LeadershipBuff*Lvl1/2/3` →
   `EXLdrshp*1/2/3`, без второго текстового пути.

## 7. INFERENCE (явно отделено от фактов)

- Имена `DisplayStringManager` (`[0xDE4518]`), `TheDrawGroupInfo` (`[0xDE7720]`),
  `Keyboard` (`[0xDE4334]`) восстановлены из donor BFME2 1.06 и из формы вызовов;
  в RW это глобальные указатели, оригинальные символы не читались.
- Суффикс `1/2/3` у `EXLdrshpOrc1/2/3` интерпретируется как уровень лидерства по
  связи с `LeadershipBuffOrcLvl1/2/3`, но **не доказан** как цифра в геометрии /
  текстуре: байты W3D не получены.
- Отсутствие внешнего читателя `+0x0C` вне `0x6D10DE` / `0x677C0C` / `0x6792E3`
  подтверждено xref/сканами Capstone (463 `call [reg+0x40]` sites, `[0xDE4518]`
  near-calls `0x6740af`, `0x677c73`, `0x782ced`; только `0x677c73` — этот numeral),
  но широкий 40491-function scan не считается полным доказательством; вывод
  опирается на RW ctor + таблицу полей + donor.
- S-001: `game.dat` предоставлен владельцем, внешний хук/DLL вне статического
  образа не исключён.

## 8. Команды воспроизведения

```
. .venv/bin/activate
python tools/mod/rw.py workspace/RotWK/game.dat dis 0x6792B6 25
python tools/mod/rw.py workspace/RotWK/game.dat dis 0x679A04 35
python tools/mod/rw.py workspace/RotWK/game.dat dis 0x677C0C 55
python tools/mod/rw.py workspace/RotWK/game.dat dis 0x63F122 16
python tools/mod/rw.py workspace/RotWK/game.dat dis 0x6D10DE 55
python tools/mod/rw.py workspace/RotWK/game.dat xref 0xBDDDA0
python tools/mod/rw.py workspace/RotWK/game.dat xref 0xDA3DF8
git show origin/main:docs/STOPS.md | grep -n '^| S-1950\|^| S-4642'
```

Связанные строки: `docs/STOPS.md` S-1950 (уточнён), S-1951, S-4642, новый S-7319;
`docs/ennorath/NIGHT_WATCH.md` строка вердикта; `docs/ennorath/PROBLEMS.md` P-55.
