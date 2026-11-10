# PORT-106 · W3DScriptedModelDraw / MultiPlayerOnly — разбор RW 2.01

Поле: `MultiPlayerOnly` (INI-строка RW 0xBE0EAC, 20 определений в моде, 14 в Ennorath-файлах).
Статус audit: `parsed-unused` (читателя в порте нет, поле парсится в `W3DModelDrawModuleData::m_multiPlayerOnly`, `W3DModelDraw.cpp:851`, `W3DModelDraw.h:313`).

## 1. Таблица разбора — какое поле у какого класса

Строка FieldParse = { name, parse proc, user data, offset } (16 байт).
Все три ссылки на имя RW 0xBE0EAC прочитаны (скан указателя по образу):

| row | name | fn | offset | следующая строка | класс |
|---|---|---|---|---|---|
| RW 0xBE1680 | MultiPlayerOnly | parseBool RW 0x42E558 | **0x15D** | AffectedByStealth (0x15E) | **таблица рисования** (таблица RW 0xBE1320, строка 54; соседи ZWriteDisableOverride 0x15C → MultiPlayerOnly 0x15D → AffectedByStealth 0x15E, HighDetailOnly 0x15F — раскладка `W3DModelDrawModuleData` подтверждена донором) |
| RW 0xC11D40 | MultiPlayerOnly | parseBool RW 0x42E558 | 0x102 | — | ExperienceLevels (`m_multiPlayerOnly +0x102`, порт `ExperienceLevels.cpp:170`) |
| RW 0xDA7048 | MultiPlayerOnly | parseBool RW 0x42E558 | 0x64 | PrimaryKey | AwardSystem ThingStatistic (донор `AwardSystemManagerParseThingStat.cpp`, offset 0x64) |

Скан строк по смещению 0x15D по всему образу: только **две** — MultiPlayerOnly (таблица рисования) и AllowEnemies (0xC592B0, `FakePathfindPortalBehaviour`-семейство, строка в таблице Wall/SiegeWaypoints с BonePrefix/WayPoint/TopAttackPos; не класс рисования).
Донор BFME2: `ConstIntGettersAddr.cpp:502` { "MultiPlayerOnly", parseBool, 0, 0x15D } — та же раскладка.

## 2. Данные модуля рисования

- Данные: create RW 0x4640CD → `new(0x188)` → конструктор данных RW 0x4C85E9 — **размер W3DScriptedModelDrawModuleData = 0x188**.
- Раскладка (донор BFME2 `W3DModelDrawModuleData.cpp:218`, RW-пишущий код): `m_zWriteDisableOverride +0x15C`, **`m_multiPlayerOnly +0x15D`**, `m_affectedByStealth +0x15E`, `m_highDetailOnly +0x15F`, `AssetList m_160[2]` кончается на 0x188.
- Дефолт false: запись RW **0x4C8736** (`mov byte [esi+0x15d], bl` внутри конструктора данных RW 0x4C85E9; донор: конструктор `m_multiPlayerOnly(false)`, BFME2 0xC8EEF).
- Класс-владелец поля: INI-имя блока `W3DScriptedModelDraw` (audit row), фабрика регистрации `Draw = W3DScriptedModelDraw` (RW 0x464B4A: name RW 0xBDBBB0, create-объекта RW 0x464092 → `new(0x2E8)` → **конструктор W3DScriptedModelDraw RW 0x4C04A3**).

## 3. Читатель — W3DModelDraw::doDrawModule RW 0x4C72BE

Полный скан .text по 4-байтному смещению 0x15D (все формы `[reg+0x15D]`, byte/movzx/cmp/test): **5 попаданий** — конструктор (пишущий), ложный декод (0x4C72E2 = тело cmp выше), и три чтения, классы которых НЕ данные рисования (см. §5). Единственный читатель данных рисования — doDrawModule.

### К каким классам относится (цепочка create → конструктор → vtable → слот)

- `W3DScriptedModelDraw` ctor RW 0x4C04A3 пишет primary vtable **RW 0xBDFFE8**; слот +0x2C (doDrawModule, по W3DLaserDraw.h слот +0x2C) = **RW 0xBE0014 = 0x4C72BE** → базовый doDrawModule. Т.е. **для всех `Draw = W3DScriptedModelDraw` (юниты, здания Эннората, цитадель Карн-Дума, ферма Арнора) ветка выполняется**.
- Прямых `call 0x4C72BE` в образе нет (dispatch только через vtable); всего пять vtable-слотов содержат 0x4C72BE (0xBDC83C, 0xBE0014, 0xBE1BC4, 0xBE1F4C, 0xBE3C6C — соседние слоты идентичны).
- Свой doDrawModule (RW 0x4B7664, содержит трековые ворота PORT-69 RW 0x4B79C9..0x4B7A02) стоит в трёх других vtable (слоты 0xBE1AD8, 0xBE1E60, 0xBE3B80 — семейство Horde/иных draw) и **не вызывает базовый** (0x4C72BE не встречается как call/jmp) — ветка MultiPlayerOnly в нём не читается (скан 0x15D подтверждает).

### Что делает каждая ветка (RW 0x4C72BE, `ebx` = модуль, `edi` = [ebx+4] = данные модуля)

```
004C72C7  cmp [ebx+0xA0], 0                ; ворота 1 (не мои): если +0xA0 == 0 и
004C72D0  cmp [ebx+0xA4], 0 / jne 0x4C78D4 ;      [ebx+0xA4] != 0 → ret (путь без push edi)
004C72DE  mov edi, [ebx+4]                 ; данные модуля рисования
004C72E1  cmp byte [edi+0x15D], 0          ; === MultiPlayerOnly ===
004C72EB  je  0x4C7312                     ; false → к HighDetailOnly
004C72ED  mov ecx, [0xDE412C]              ; TheGameLogic
004C72F3  call 0x441B7C                    ; GameLogic::isInMultiplayerGame
004C72F8  test al, al
004C72FA  je  0x4C78D3                     ; НЕ мультиплеер → ret (не рисует)
004C7300  mov eax, [0xDE4364]              ; TheWritableGlobalData
004C7305  cmp byte [eax+0xD45], 0
004C730C  jne 0x4C78D3                     ; GlobalData+0xD45 != 0 → ret
004C7312  cmp byte [edi+0x15F], 0          ; соседние ворота HighDetailOnly (не мои):
004C731B  cmp [ebx+0xBC], 0 / jne 0x4C78D3 ;      true && [ebx+0xBC] != 0 → ret
004C7328  mov [ebx+0xA0], 0                ; тело начинается дальше (transform,
                                            ;  processAnimations RW 0x4BFaf4, частицы,
                                            ;  турель/отдача, fade, drawIDs, FX RW 0x4BCE68)
004C78D3/0x4C78D4 — общий epilogue (ret)
```

### isInMultiplayerGame RW 0x441B7C (доказано телом)

```
mov eax, [ecx+0x110]   ; game mode
cmp eax, 1 / je true   ; 1 = MODE_LAN
cmp eax, 5 / je true   ; 5 = MODE_INTERNET
xor eax, eax / ret     ; всё остальное (в т.ч. 2 = MODE_SKIRMISH!) → false
```

Порт: `EconomySettings.h:86-87` (`MODE_LAN = 1, MODE_SKIRMISH = 2`, `MODE_INTERNET = 5`, `gameMode +0x110`, RW 0x441B7C/0x625456) — **одиночный skirmish (2) НЕ считается мультиплеером для этой ветки** (в отличие от `Economy::isMultiplayerGame` RW 0x625456 = {1,2,5}, который используется ExperienceLevels/экономикой — это РАЗНЫЕ предикаты).

### GlobalData +0xD45 — что это и откуда

- **Не INI-поле**: скан строк FieldParse по 0xD45 по всему образу — 0 совпадений (и у донора: `GlobalData.h:378` `+0x0D45, 1 bytes, field0D45` — без имени).
- Пишущие (полный скан .text по смещению 0xD45):
  1. **RW 0x6435AF** — конструктор GlobalData (дефолты: `D24=1, D28=0x5A, D2C=1, D44=0, D45=0(b1), D46=1`, рядом `D40=1.0f`) → всегда 0. (Совпадает с донором: ctor BFME2 0x00236F18 пишет 0.)
  2. **RW 0x919F7B..0x919FB6** — в конце жизни Apt-экрана «картинки» (конструктор класса RW 0x91A6C9: vtable RW 0xC7CBB8, снапшот глобалов в свои +0x434/+0x435/+0x436 из +0x9BD/+0xD45/+0x68; строки 'GUI:OnPictureTaken' рядом): восстановление снапшота обратно в GlobalData (+0x9BD ← +0x434, **+0xD45 ← +0x435**, +0x68 ← +0x436) и принудительная запись +0x9A5=1. Восстановление = та же valeur, что была до входа → **не может установить 1**.
- ⇒ В RotWK 2.01 байт +0xD45 **всегда 0** (инвариант, доказан сканом писателей).
- Читатели (для полной картины, не для порта): сама ветка doDrawModule; графические пути RW 0x467EBD (два размера 0xDAC0/0x3FFFC), RW 0x4E698E (0x14 или 2 — донор `Rva000E6FC8`: селектор плотности), RW 0x5F5168, RW 0x9D6D2A, RW 0x91A725 (снапшот экрана), донорские `Rva0007BA35` (shadow map 0x1000), `TransportContain` payload.

## 4. Смысл ветки

`MultiPlayerOnly = Yes` → в режимах, НЕ равных {LAN, INTERNET} (skirmish против ИИ, кампания, одиночная игра), doDrawModule возвращает на первом кадре: модуль рисования не выполняет НИ ОДНОГО шага тела (ни transform, ни processAnimations, ни частиц, ни турели/отдачи, ни fade, ни drawIDs, ни FX-событий RW 0x4BCE68). В мультиплеере {1,5} тело идёт (при GlobalData+0xD45 == 0, что всегда).

## 5. Три «посторонних» чтения +0x15D (НЕ данные рисования)

| RW | что | почему не W3DModelDrawModuleData |
|---|---|---|
| 0x5AF3FD | `movzx edx, [esi+0x15D]` рядом с 0x15C/0x15E → float | у `esi` есть поля ≥0x2D8 (Com-указатели +0x2d0/2d4/2d8, float +0x24c..) — данные рисования кончаются на **0x188** (create RW 0x4640CD) |
| 0x6BAEB6 | `cmp [edi+0x15D], bl` в fn из vtable RW 0xC14534 (слот 0) | `edi` = `this` сервиса LivingWorld-уведомлений (конструктор RW 0x6BA21C, пять vptr, регистрация в 0xDE9E7C/0xDE88C8); порт цитирует этот же адрес: `LivingWorldLogic.cpp:4139` / `.h:416` — `StrategicNotice::PLAYER_DEFEATED` (LW:AllyDefeated/EnemyDefeated) |
| 0x8A6BB8 | геттер `mov al, [eax+0x15D]` (слот 14 vtable RW 0xC68D00 интерфейса на +0x20) | класс = **BroadcastStealthUpdate** (имя RW 0xC0B844, конструктор 0x8A6AB7); `eax = [ecx-0x1c] = [this+4]` = **указатель на ЕГО собственные данные** (Module ctor RW 0x708270: арг2 → +4 = ModuleData*), т.е. чужие данные модуля, не W3DModelDrawModuleData; INI-блок `BroadcastStealthUpdate` в данных мода отсутствует |

## 6. Донор BFME2 (донор подтверждает механизм, RW — источник истины)

- `W3DModelDrawDoDrawModule.cpp:326` (донор, тело `doDrawModule` BFME2 0xC7BC4): `if (w3DModelDrawModuleData->m_b15D) { if (!TheGameLogic->isInMultiplayerGame()) return; if (TheWritableGlobalData->m_bD45) return; }` — **байт-в-байт** с RW 0x4C72E1..0x4C730C; `m_b15D` = 0x15D, `m_bD45` = GlobalData+0xD45, дальше `m_b15F && m_BC` (HighDetailOnly) — тот же порядок.
- DONOR BFME2 `Code/GameEngine/Include/Common/GlobalData.h:378` (+0x0D45 без имени), `W3DModelDrawModuleData.cpp:218` (раскладка).

## 7. Что делает порт сейчас

- Поле парсится (`W3DModelDraw.cpp:851`), не читается (audit `parsed-unused`).
- `doDrawModule` в порте отсутствует как функция (конвейер разобран: Drawable::advanceAnimation → e.draw->advance; LiveFX-проходы; финальное показ/скрытие `GodotGameWorld.cpp:1829` `shown = f.model && !moduleHidden && !fogHidden && !hiddenByFade`).
- Мультиплеер-предикаты в порте: `Economy::isMultiplayerGame` RW 0x625456 = {1,2,5}; предиката {1,5} (RW 0x441B7C) нет (упоминание только в комментарии `Network.cpp:2167`).
- GlobalData+0xD45 в порте не смоделирован.
