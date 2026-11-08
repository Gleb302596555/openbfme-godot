# PORT-33: AIWoTRForfeitTactic — `FarmingThreshold` и `LogicFramesTillAISelfDestructs` в RotWK 2.01 (issue #105)

Бинарник: `workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f` (оговорка S-001: образ сообщества).
Все адреса, ветки, непосредственные вызовы, указатели vtable и строки ниже прочитаны
`tools/mod/rw.py ... dis|xref|str` по этому файлу (для чтения линейно, с середины инструкции,
использован тот же PE-образ и линейный проход capstone от входа функции).
Имена, взятые из декомпиляции BFME2 или из порта, помечены `INFERENCE`; адрес инструкции и
смещение поля при этом доказаны RW. Значений INI мода, байт мода и игровых архивов в этом файле нет.

**Важно:** `xref` из `tools/mod/rw.py` ищет только в секции `.text` (RVA 0x1000 .. 0x7D0000), а код
RotWK 2.01 лежит и в других секциях (например 0x9B73xx). Поэтому перекрёстные ссылки ниже считались
своим проходом по всему образу PE (rel32 `E8`/`E9` + абсолютные imm32); в тексте это помечено
«проход по образу».

## 1. Класс и его слоты

| что | RW |
|---|---|
| конструктор | **0x9B7316** (вызывается из **0x90C2B7**, копирующий — **0x9B743A**) |
| vtable | **0xC89B74** (ставится RW 0x9B7353); имя класса рядом: 0xC89BAC = `AIWoTRForfeitTactic` |
| размер объекта | **0x68** (RW 0x90C29F: `push 0x68` перед `call 0x42F6E0` — аллокатор) |
| слот +0x00 | 0x9B72FA (scalar deleting destructor) |
| слот +0x04 (применимость) | **0x9B71B7** |
| слот +0x10 (число команд) | **0x851E97** → `xor eax, eax; ret` = **0** команд |
| слот +0x14 (сохранение) | **0x9B72A4** |
| слот +0x18 (запуск) | **0x9B7260** |
| слот +0x1C (обновление) | **0x9B7458** |
| слот +0x28, +0x34 | 0x8BD372 (`mov al, 1` — «всегда») |
| слот +0x30 (клонирование) | **0x9B7416**: аллоцирует 0x68, зовёт RW 0x9B7316, копирует **+0x58** и **+0x5C** (RW 0x9B743F .. 0x9B7448) |

Поля экземпляра (конструктор RW 0x9B7316, RW 0x9B7353 .. 0x9B736F):

| смещение | значение в конструкторе | смысл (по имени поля, `INFERENCE`) |
|---|---|---|
| +0x00 | 0xC89B74 | vtable |
| +0x24 | — | игрок-владелец (ставит RW 0x8F2330, `AITactic::m_playerToAttack`) |
| +0x58 | 0 | «сдача запущена» |
| +0x5C | `[TheSkirmishAIManager + 0x9B8]` | счётчик кадров до самоуничтожения (`LogicFramesTillAISelfDestructs`) |
| +0x60 | 0 | «сообщение уже отправлено» |
| +0x64 | 0 | сколько проверок подряд оценка была ниже `FarmingThreshold` |

RW 0x9B735D читает `[0xDE4938]` (TheSkirmishAIManager) и `[eax + 0x9B8]` — это глобальное
`SkirmishAIData` + 0x9B8 = табличное + 0x9A8 (`LogicFramesTillAISelfDestructs`; таблица RW 0xC13C18,
объект данных = менеджер + 0x10).

Сохранение (vslot 0x14, RW 0x9B72A4): версии 1 .. 2; базовый `AITactic::xfer` RW 0x8F2466; затем
+0x58 через vslot +0x90 (bool), +0x5C через vslot +0x7C (int), и при версии >= 2 (RW 0x9B72E3
`cmp byte ptr [ebp-3], 2`) ещё +0x64 тем же +0x7C. **+0x60 не сохраняется.**

## 2. Применимость — RW 0x9B71B7 (слот +0x04)

Аргумент: указатель на таргет (`ret 4`, не используется). `esi = this`.

| RW | инструкция | ветка |
|---|---|---|
| 0x9B71B7 | `mov eax, [0xDE412C]` (TheGameLogic) | |
| 0x9B71BC | `cmp dword ptr [eax+0x114], 0` | `je 0x9B71CF`; **не 0 → `al = 0`, выход** |
| 0x9B71CF..0x9B71E0 | `ecx = [0xDE4938]`; `push [esi+0x24]`; `call 0x6A950B` (TheSkirmishAIManager::findAI, запись AI игрока) | |
| 0x9B71E2..0x9B71F8 | `edi` = запись; `0x6C7EDD(edi, 0xDEBEA4)` → `ebx`; `0x6C7EDD(edi, 0xDEBEA8)` → `eax` | имя `AIWoTRForfeitTacticHasRun` и `AIWoTRForfeitTacticWillRun` (см. §3) |
| 0x9B71FD | `test eax, eax; jne 0x9B725C` | `WillRun != 0` → false |
| 0x9B7201..0x9B7215 | `ecx = [TheGameLogic+0x40]` (кадр); `cmp ecx, [TheSkirmishAIManager + 0x9B0]`; `jbe 0x9B725C` | кадр <= `LogicFramesTillRetreatChecksStart` (табличное +0x9A0) → false |
| 0x9B7217 | `test ebx, ebx; jne 0x9B725C` | `HasRun != 0` → false |
| 0x9B721B | `cmp [TheGameLogic+0x114], 3; je 0x9B725C` | род игры 3 (skirmish) → false (после первой проверки недостижимо, но в коде есть) |
| 0x9B7224..0x9B722E | `0x6AA61B(player)` (`Player::getDifficulty`); `jle 0x9B725C` | сложность EASY (0) → false |
| 0x9B7230..0x9B723D | `eax = [0xDE4928] + 0x10` (ThePlayerList, локальный игрок); `cmp ecx, eax; je 0x9B725C` | сам локальный игрок → false |
| 0x9B723F | `0x6ACEAF(me, local)`; `jne 0x9B725C` | не ENEMIES (0) к локальному игроку → false |
| 0x9B7249..0x9B7255 | `++ebx`; `0x6C7EC5(edi, 0xDEBEA8, ebx)`; `al = bl` | `WillRun = HasRun + 1`, вернуть true |

Итог: тактика применима только в **одиночном сражении Войны Кольца** (род игры 0, RW 0x779CB4),
после `LogicFramesTillRetreatChecksStart` кадров, у не-локального AI игрока сложности выше EASY,
который воюет с локальным игроком, один раз (`HasRun`), и не раньше, чем `WillRun` сброшен.

## 3. Именованные счётчики записи AI (RW 0x6C7EDD / 0x6C7EC5)

`0x6C7EDD(record, name)` = чтение значения по имени (нет имени → 0), `0x6C7EC5(record, name, value)`
= запись. Тексты имён задаёт инициализатор **0xBC9A7C .. 0xBC9ACB**: `0x40611E` (конструктор
`AsciiString` из литерала) для 0xDEBEA4 ← `0xC89C8C` (`AIWoTRForfeitTacticHasRun`), 0xDEBEA8 ←
`0xC89CA8` (`AIWoTRForfeitTacticWillRun`), 0xDEBEAC ← `0xC89CC4` (`AIWoTRForfeitTactic_FrameNextRun`).

## 4. Запуск — RW 0x9B7260 (слот +0x18)

```
rec = 0x6A950B(this->+0x24)
if (0x6C7EDD(rec, "AIWoTRForfeitTacticHasRun") == 0) {
    0x6C7EC5(rec, "AIWoTRForfeitTacticHasRun", 1);
    0x6C7EC5(rec, "AIWoTRForfeitTactic_FrameNextRun", TheGameLogic->+0x40);
}
```
(повторный запуск ничего не меняет; кадр следующей проверки — текущий кадр, поэтому первая
провка идёт уже в этом кадре на обновлении).

## 5. Обновление — RW 0x9B7458 (слот +0x1C), границы 0x9B7458 .. 0x9B7EB1

`ebx = this`, `edi` = запись AI, `[ebp-0x24] = this`, `[0xDE412C]` = TheGameLogic, `[0xDE4928]` =
ThePlayerList, `[0xDE4950]` = TheLivingWorldLogic, `[0xDE4938]` = TheSkirmishAIManager.

```
rec  = 0x6A950B(this->+0x24)                                  0x9B7470..0x9B7482
next = 0x6C7EDD(rec, "AIWoTRForfeitTactic_FrameNextRun")      0x9B7484..0x9B748C
if (this->+0x58 != 0) goto 0x9B7DAC        ; сдача уже запущена → счётчик
frame = TheGameLogic->+0x40
if (next > frame) goto 0x9B7E5F            ; троттлинг: 0x9B74A4 `ja`
0x6C7EC5(rec, "AIWoTRForfeitTactic_FrameNextRun",
         frame + SkirmishAIData[+0x9B4])   ; +0x9B4 = табличное +0x9A4 LogicFrameBetweenRetreatChecks
score = <оценка, §6>                       ; 0x9B74C2 .. 0x9B7D70
if (score >= SkirmishAIData[+0x994]) {     ; +0x994 = табличное +0x984 FarmingThreshold; 0x9B7D74 `jge`
    this->+0x64 = 0;                       ; 0x9B7DA3
    goto 0x9B7E5F
}
++this->+0x64;                             ; 0x9B7D7C
if (this->+0x64 < 3) goto 0x9B7E5F;        ; 0x9B7D7F `cmp [edi+0x64], 3`
this->+0x58 = 1;                           ; 0x9B7D8B
this->+0x5C = SkirmishAIData[+0x9B8];      ; 0x9B7D8F..0x9B7D9B (LogicFramesTillAISelfDestructs)
goto 0x9B7E5F
```

Ветка счётчика (0x9B7DAC .. 0x9B7E5A), когда +0x58 != 0, **каждый кадр**:

```
--this->+0x5C;                             ; 0x9B7DAC
if (this->+0x5C >= 70) goto 0x9B7E5F;      ; 0x9B7DAF `cmp [ebx+0x5C], 0x46`
if (this->+0x60 != 0) goto 0x9B7E5F;       ; 0x9B7DB9: сообщение уже было
this->+0x60 = 1;                           ; 0x9B7DCB
<сообщение клиента: строка 0xC132DC = "GUI:AIForfeitsInWOTRSkirmishNow", длительность 0x2710 мс
 (RW 0x9B7DC3 .. 0x9B7E5A: 0x621E3A, 0x7FF5E0, аргумент 10000)>; esi == 0 → сразу 0x9B7E5F
```

Хвост (0x9B7E5F .. 0x9B7EB1), общий для всех веток:

```
if (this->+0x5C > 0) return;               ; 0x9B7E64 `cmp [esi+0x5C], 0; jg`
if (0x441E4A(TheLivingWorldLogic)) return; ; 0x9B7E69..0x9B7E76 (игра кончилась)
0x448 (MSG_SELF_DESTRUCT) в [0xDE6398] (+vslot 0x48): сообщение с игроком this->+0x24 (+0x54)
    и аргументом-байтом 0 (RW 0x9B7E78 .. 0x9B7E94); 0x711104 = appendBooleanArgument
0x8F2006(this, 0, 1)                       ; 0x9B7E99..0x9B7E9E — конец тактики
```

Вывод: `LogicFramesTillAISelfDestructs` — сколько кадров тактика ждёт после решения о сдаче,
прежде чем отправить `MSG_SELF_DESTRUCT` своему игроку; `FarmingThreshold` — порог оценки, ниже
которого ведётся отсчёт «плохих» проверок; три проверки подряд ниже порога запускают сдачу.

Цепочка вызова подтверждена: RW 0x90C0FA выбирает список генератора +0x4C, RW 0x90C2B7 создаёт
объект, RW 0x90C2CC кладёт его в список, RW 0x90C610 .. 0x90C62F перебирает список и зовёт слот +4,
RW 0x90CB98 зовёт этот перебор; RW 0x90B8DC зовёт RW 0x8F11DA, а тот при `+0x51 != 0` и `+0x10 != 0`
делает `jmp [eax+0x1C]` = RW 0x9B7458.

### 5.1 Как нуль-командная тактика становится «готовой» (RW 0x8F11DA)

RW 0x8F11DA требует `+0x51` (готовность). Для тактики без команд её ставит старт RW 0x8F232F:
RW 0x8F234B `call [eax+0x10]` (vslot +0x10 = число команд) и **`test eax,eax; jbe 0x8F2424`**, а
0x8F2424 — `mov byte ptr [esi+0x51], 1; al = 1`. Дальше RW 0x8F11DA: `+0x54` = кадр готовности, по
истечении RW 0xDE9F08 кадров (в образе 0) `+0x10 = 1` и `jmp [eax+0x18]` (запуск RW 0x9B7260);
со следующего кадра `+0x10 != 0` → `jmp [eax+0x1C]` (обновление RW 0x9B7458).

## 6. Оценка (score) — RW 0x9B74C2 .. 0x9B7D70

Константы (проверены по байтам PE): 0xBD889C = 2.0f, 0xBDE8C8 = 1.5f, 0xBDD42C = 3.0f,
0xBD1908 = 1.0f, 0xBD1904 = 0.25f, 0xC10438 = 9.0f.

### 6.1 Накопление по игрокам (RW 0x9B758E .. 0x9B7C1A)

Цикл по ThePlayerList (0xDE4928, число +0x14, `0x6A844E(i)` — игрок). Для каждого игрока `p`:

* `if (!p) continue` (0x9B759B); `if (!0x6AAC66(p)) continue` (0x9B75AA) — только игроки
  skirmish AI (`INFERENCE`: порт читает это как `Player::isSkirmishAI()`, RW 0x6AAC66 =
  `p->+0x34` (PlayerTemplate) → байт `+0x151`).
* RW 0x9B75B0 .. 0x9B7624: поиск армии игрока в Живой Войне (`[0xDE4950]` = TheLivingWorldLogic,
  0x6B5DE0 / 0x610278 / 0x6B351E) и запись множителя 4.0 / 1.0 в `[ebp-0x84]`. Локаль `[ebp-0x84]`
  **больше нигде не читается** (проверено по всему телу: только записи 0x9B75E9/0x9B7624 и
  сравнение 0x9B75FA внутри той же ветки) — на оценку это не влияет.
* `[ebp-0x90] = (float)0x6A7AC7(p+0x60, 0)` = **расход командных очков** игрока (Player + 0x68;
  порт: `CommandPoints::getUsage()`): «количество армии».
* `rec = 0x6A9999(manager, p)` — запись AI игрока; списки: `rec + 0` армия, `+ 8` строения
  (RW 0x9B7661 / 0x9B78FB, `0x72B229` — геттер списка).
* Армия (0x9B768F .. 0x9B7882), для каждого id: `obj = 0x449681(id)`; нет → дальше; шаблон `t = obj+4`:
  - `t + 0x113 & 4` (KindOf **HERO**, бит 90 таблицы RW 0xDA0E68) → `heroQuality = max(heroQuality,
    (float)obj->+0x26C->+0x24)` (ранг ExperienceTracker) и `continue`;
  - иначе: `value = 0x73C25F(t, p, 0, -1)` (`ThingTemplate::calcCostToBuild`);
    `t + 0x115 & 0x20` (KindOf **HORDE**, бит 109) → берётся интерфейс `0x68C866(obj)` и его
    vslot **0x274** (строка); если имя не пусто и равно `0xC89C78` (`AngmarThrallMaster`) — добавки
    за улучшения не считаются; `INFERENCE`: vslot 0x274 отдаёт имя объекта-орды;
  - добавки за улучшения (0x9B7788 .. 0x9B77DD): `set = 0x71EFA2(0x69156B(obj))`
    (`CommandSet` по имени), для `i` 0 .. 32 `b = 0x80C837(set, i)`; `up = b+0x24`
    (UpgradeTemplate); если `0x691421(obj, up)` (улучшение есть у объекта) → `value += up+0x34`
    (`BuildCost`);
  - `armyQuality += ((rank - 1) * 0.25f + 1.0f) * value` (0x9B7843 .. 0x9B7870).
* Контроль карты (0x9B788B .. 0x9B78F9): `edi` = размер списка **записи + 0xC** (RW 0x9B788B `mov ecx, [ebx + 0xC]`
  → 0x72B229, экономические строения; 0x9B78A4 / 0x9B78AA `sub` / 0x9B78C0 `sar 2`), затем по списку
  `[0xDE4938] + 0xA68` (флаги захвата): для каждого id: `obj = 0x449681(id)`; `0x6ADC43(p, obj) == 2` (ALLIES)
  → `++edi`; итог прибавляется к контролю карты (0x9B7B36 `cvtsi2ss`).
* Строения `rec + 8` (0x9B78FB .. 0x9B7AE8), для каждого id: `obj`; шаблон `t`; пропуск, если
  `t+0x10E & 4` (LINKED_TO_FLAG, 50), `t+0x10E & 0x40` (UNATTACKABLE, 54), `t+0x120 & 0x10`
  (ECONOMY_STRUCTURE, 196), `t+0x10C & 4` (SUPPLY_GATHERING_CENTER, 34), `t+0x11E & 8` (SUMMONED, 179);
  - `cost = 0x73C25F(t, p, 0, -1)` + добавки за улучшения (тот же цикл 33 кнопок, 0x9B79AC .. 0x9B79EA);
  - `t+0x10F & 0x80` (FS_FACTORY, 63) и **не** `t+0x120 & 4` (DOZER_FACTORY, 194) → `cost *= 2`
    (0x9B7A01: `add eax, eax`);
  - `t+0x120 & 4` (DOZER_FACTORY) → имя шаблона `t+0x64` сравнивается с семью цитаделями
    (`0xC89C60` IsengardFortressCitadel, `0xC89C48` AngmarFortressCitadel, `0xC89C30`
    MordorFortressCitadel, `0xC89C1C` WildFortressCitadel, `0xC89C04` DwarvenFortressCitadel,
    `0xC89BF4` ElvenCitadel, `0xC89BE0` MenFortressCitadel); **совпало → к записи**, иначе
    `[ebp-0x40] = (([ebp-0x40] < 2.0f) && (0x6ACEAF(me, p) == 2)) ? 2.0f : 1.0f`
    (0x9B7A96 .. 0x9B7AC7 — тернарник с `&&`);
  - `baseStrength += (float)cost` (0x9B7ACC .. 0x9B7AD6).
* Классификация (0x9B7AEE .. 0x9B7BA9): `p == me` или `0x6ACEAF(me, p) != 0` → свои аккумуляторы
  (`[ebp-0x54]` armyQuality, `[ebp-0x64]` armyQuantity, `[ebp-0x74]` heroQuality(максимум),
  `[ebp-0x80]` mapControl, `[ebp-0x78]` baseStrength), иначе — чужие (`[ebp-0x4C]`, `[ebp-0x44]`,
  `[ebp-0x3C]`, `[ebp-0x88]`, `[ebp-0x6C]`).

### 6.2 Слагаемые (RW 0x9B7C41 .. 0x9B7D70)

| # | RW | условие | вклад |
|---|---|---|---|
| 1 | 0x9B7C41..0x9B7C75 | `armyQuality(своя) > 2.0f * чужая` / `чужая > 2.0f * своя` | `+ArmyQualityBias` / `-ArmyQualityBias` (0x994 → табличное 0x988) |
| 2 | 0x9B7C78..0x9B7CAC | `armyQuantity > 1.5f * чужая` / наоборот | `±ArmyQuantityBias` (0x99C / 0x98C) |
| 3 | 0x9B7CAF..0x9B7CE3 | `heroQuality > чужая + 3.0f` / наоборот | `±HeroQualityBias` (0x9A0 / 0x990) |
| 4 | 0x9B7CE6..0x9B7D17 | `mapControl > 2.0f * чужая` **и** `mapControl > 9.0f` / то же для чужой | `±MapControlBias` (0x9A4 / 0x994) |
| 5 | 0x9B7D1A..0x9B7D3E | `baseStrength > 1.5f * чужая` / наоборот | `±BaseStrengthBias` (0x9A8 / 0x998) |
| 6 | 0x9B7D41..0x9B7D4F | `0x9B737F(this)` | `+RingOwnershipBias` (0x9AC / 0x99C) — **только плюс** |
| 7 | 0x9B7D56..0x9B7D70 | всегда | `score = (int)((float)(сумма 1..6) + [ebp-0x40])` |

`0x9B737F` (проверка кольца): для каждого игрока списка: `0x6ACEAF(me, p) == 2` (ALLIES) и
`0x6AC2AF(p, upgrade)` (улучшение получено) для `0x66F5E5([0xDE45A0], 0xC795B0)` — улучшение
`Upgrade_RingHero` (0xC795B0). Первое совпадение → true.

## 7. Что перенесено в порт и что нет

Перенесено (lane PORT-33): применимость RW 0x9B71B7, запуск RW 0x9B7260, обновление RW 0x9B7458
(троттлинг, оценка, порог, счётчик трёх проверок, счётчик кадров, `MSG_SELF_DESTRUCT`, конец
тактики), состояние +0x58 / +0x5C / +0x60 / +0x64 и его сохранение, правило готовности нуль-командной
тактики RW 0x8F234B.

Не перенесено: клиентское сообщение `GUI:AIForfeitsInWOTRSkirmishNow` (RW 0x9B7DC3 .. 0x9B7E5A:
TheInGameUI, 10 с) — в порте у логики нет доступа к интерфейсу; строка в `docs/STOPS.md`.
Ветка Живой Войны RW 0x9B75B0 .. 0x9B7624 в порт не переносится: её результат `[ebp-0x84]` в RW
никуда не читается (§6.1).
