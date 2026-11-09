# PORT-44: ExperienceLevel / ShowLevelUpTint — разбор RotWK 2.01 (issue #148)

Бинарник: `game.dat` md5 daed3668 (caveat S-001). Все адреса прочитаны `tools/mod/rw.py dis` / `xref` на этом файле.
Задача: перенести «вспышку цвета» при получении уровня — `ExperienceLevel` поля `ShowLevelUpTint`, `LevelUpTintColor`,
`LevelUpTintPreColorTime`, `LevelUpTintSustainColorTime`, `LevelUpTintPostColorTime` (`LevelUpTintFrequency` / `LevelUpTintAmplitude`).
Вердикт: **поля парсятся и копируются, и их не читает ни один код RotWK 2.01; вспышки, которую можно было бы перенести, нет.**

## 1. Строки таблицы полей ExperienceLevel (RW 0xC11BF0, 22 строки)

Таблица одна на `ExperienceLevelTemplate`; парсер строки — общий `FieldParse` (bool RW 0x42E558, int RW 0x42EC5E, real RW 0x42ED00,
RGBColor RW 0x42EF99), загрузчик — `INI::parseExperienceLevelDefinition` (RW 0x688D3C-ветка, см. `docs/STOPS.md` S-632).

| поле | смещение | строка таблицы | парсер | запись |
|---|---|---|---|---|
| `ShowLevelUpTint` | +0xD8 (byte) | RW 0xC11C90 | RW 0x42E558 | байт |
| `LevelUpTintColor` | +0xDC (`RGBColor`, 3 × float) | RW 0xC11CA8 | RW 0x42EF99 (×1/255) | три float |
| `LevelUpTintPreColorTime` | +0xE8 (int) | RW 0xC11CC0 | RW 0x42EC5E | dword |
| `LevelUpTintPostColorTime` | +0xEC (int) | RW 0xC11CD8 | RW 0x42EC5E | dword |
| `LevelUpTintSustainColorTime` | +0xF0 (int) | RW 0xC11CF0 | RW 0x42EC5E | dword |
| `LevelUpTintFrequency` | +0xF4 (real) | RW 0xC11D08 | RW 0x42ED00 | float |
| `LevelUpTintAmplitude` | +0xF8 (real) | RW 0xC11D20 | RW 0x42ED00 | float |

Имена и порядок строк совпадают с `Open-BFME-2/…/ExperienceLevelSystem.cpp:576-582`; там же подтверждены смещения. Для RotWK это
**INFERENCE** только в части имён: смещения и типы прочитаны в самом `game.dat` (см. ниже).

## 2. Кто трогает эти смещения

Линейный проход по всему образу (затравки: все цели `E8`/`E9` rel32 из `.text`, все указатели на `.text` из остальных секций —
92 601 затравка, 2 666 982 декодированных инструкции) даёт ровно две функции, у которых операнд памяти использует базу плюс одно из
смещений `+0xD8 / +0xDC / +0xE8 / +0xEC / +0xF0 / +0xF4 / +0xF8`:

**Конструктор RW 0x689A4E** — обнуляет все семь полей:

| адрес | инструкция | поле |
|---|---|---|
| RW 0x689ADB | `lea ecx, [esi+0xDC]` | конструктор `RGBColor` |
| RW 0x689AE1 | `mov byte [esi+0xD8], bl` (bl = 0) | `ShowLevelUpTint` = false |
| RW 0x689AE7 | `mov dword [esi+0xE8], ebx` | `PreColorTime` = 0 |
| RW 0x689AED | `mov dword [esi+0xEC], ebx` | `PostColorTime` = 0 |
| RW 0x689AF3 | `mov dword [esi+0xF0], ebx` | `SustainColorTime` = 0 |
| RW 0x689AF9 | `movss [esi+0xF4], xmm0` (0.0f) | `Frequency` = 0 |
| RW 0x689B01 | `movss [esi+0xF8], xmm0` (0.0f) | `Amplitude` = 0 |

**Копирование RW 0x689FEA** — единственное чтение всех семи полей: значения переносятся в копию без изменений (это
`new ExperienceLevelTemplate(*final)` в map.ini-override, RW 0x68A37F):

| адрес | что читает |
|---|---|
| RW 0x68A01C | `+0xD8` |
| RW 0x68A028 | `+0xE8` |
| RW 0x68A034 | `+0xEC` |
| RW 0x68A040 | `+0xF0` |
| RW 0x68A04C | `+0xF4` |
| RW 0x68A058 | `+0xF8` |
| RW 0x689FEA … | `+0xDC` через `lea`/копию `RGBColor` |

Больше ни одного `mov`/`cmp`/`test` с базой + этими смещениями в образе нет. Проверялись и `+0x3C`-подобные короткие displacement
(проход линейный, не по 4-байтному шаблону — см. ловушку в PROCESS.md); региональный фильтр `0x688D00..0x68AC00` тоже даёт только
эти две функции. `0x688D3C` — общий аксессор загрузчика (около 180 вызовов), к тинту отношения не имеет.

## 3. Путь получения уровня

Ни один шаг не читает `+0xD8 … +0xF8`:

| адрес | что делает |
|---|---|
| RW 0x79D141 | `checkLevels`: `while (RequiredExperience <= trunc(experience))` |
| RW 0x689E3C | следующий уровень цепочки |
| RW 0x79DA9D | `onLevelReached` |
| RW 0x8215C2 | постановка гранта в очередь |
| RW 0x79DACD | `SelectionDecal` уровня (`+0xA4`) → drawable |
| RW 0x79D72A | `setRank` рангом уровня |
| RW 0x79DB50 / 0x79DB6B | счётчики level-up героя / юнита |
| RW 0x821319 | сам грант (следующий кадр, RW 0x821567) |
| RW 0x82133D | `AttributeModifiers` (`+0x30`) |
| RW 0x821368 | `Upgrades` (`+0x4C`) |
| RW 0x8211EC | `LevelUpFx` (`+0x3C`) и `LevelUpOCL` (`+0x48`) |
| RW 0x8213DB | model condition `LEVELED` (210) на `3 * LOGICFRAMES_PER_SECOND` |
| RW 0x8213F1 | `ModelConditionState` уровня (`+0x58`) |
| RW 0x89A468 | `InformUpdateModule` (`+0x100`) |
| RW 0x68F37F | `EmotionType` (`+0x104`) |

## 4. Все точки старта цветного тинта/вспышки — и ни одной с уровнем

| функция | адрес | вызовы |
|---|---|---|
| `Drawable::colorFlash` | RW 0x675749 | RW 0x6757BB (`colorTint`), RW 0x675D7D (свой update), RW 0x81E234 (диспетчер сообщений, белый на 8 кадров), RW 0x871B24, RW 0x871B6B, RW 0x8A02A3 (белая вспышка) |
| `Drawable::setCustomTintEnvelope` | RW 0x671A73 | RW 0x4D1281 (`W3DTankDraw`), RW 0x5E0160 (`TintDrawableFXNugget`), RW 0x6ED1C2 (сеттер тинта объекта) |
| `TintEnvelope::play` | RW 0x674818 | RW 0x6757AA, 0x67597D, 0x675DD1, 0x675E1C, 0x675E86, 0x675ED4, 0x67910E — все внутри `Drawable` |

Ни один вызывающий не держит `ExperienceLevelTemplate`, и ни один не вызывается из RW 0x79D141 / 0x79DA9D / 0x821319.
`0x675BC0..0x675D30` — собственная огибающая вспышки `Drawable` (её `+0xF4`/`+0xF8`), к уровню не относится.
`0x81E147` — обработчик текстового сообщения, к тинту не относится.

## 5. Данные мода Ennorath 1.9.8.2

`ShowLevelUpTint` — 255 блоков `ExperienceLevel` (из 652), цвет и три времени — 667 вхождений (типично `R:255 G:255 B:255`,
`500 / 3000 / 500`). То есть мод эти поля заполняет, но движок их не потребляет — как и в ванили.

## 6. Итог

* Вспышку цвета при level-up в 2.01 **некому исполнять**: поля только парсятся (RW 0x42E558 / 0x42EF99 / 0x42EC5E / 0x42ED00),
  обнуляются конструктором (RW 0x689AE1) и переносятся копией (RW 0x689FEA); читателей нет.
* Поэтому порт не добавляет ни отрисовки, ни нового состояния: только стоп S-7196 с вердиктом и счётчиком уровней
  (`ExperienceLevelSystem::stopLines`), плюс тесты `PORT-44*`, которые пиннят раскладку полей, копию override и то, что уровень
  с `ShowLevelUpTint = Yes` и с `No` даёт клиенту одинаковый набор событий (`LEVELED` и `LevelUpFx`).
* Что видно в игре при получении уровня — `LEVELED` (RW 0x8213DB, 3 с) и `LevelUpFx` уровня (RW 0x8211EC), их порт уже делает.
