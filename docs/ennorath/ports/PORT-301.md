# PORT-301: `ShareExperienceBehavior` — разделяемый опыт (issue #693)

Бинарник: `workspace/RotWK/game.dat` (RotWK 2.01, md5 `daed3668006cd90f01c34e5a7da1901f`, оговорка S-001).
Все адреса прочитаны `tools/mod/rw.py dis` на этом файле. Донор: BFME2 1.06 (`/tmp/open-bfme-2`,
`Code/GameEngine/Source/GameLogic/Object/Behavior/ShareExperienceBehavior.cpp`) — только для структуры, каждое поведение подтверждено адресом RotWK.

## Что делает класс в RotWK 2.01

`ShareExperienceBehavior` — модуль поведения: объект, получивший опыт (за убийство), делится им с союзниками.
Класс зарегистрирован (`engine/data/rotwk-201/module-registry.json`, create `RW 0x64BC96`, vtable `RW 0xC5EEA0`).

Цепочка retail:

| шаг | адрес | что делает |
|---|---|---|
| модуль данных | `RW 0x8830ED`, таблица полей `RW 0xC5EF30` | `Radius` + 0x08, `DropOff` + 0x0C, `Percentage` + 0x10, `ObjectFilter` + 0x14 |
| слушатель объекта | `RW 0x88305B` (вызов `RW 0x00695589`) | первый модуль объекта, у которого слот `+0xA4` интерфейса `RW 0xC5ED9C` отвечает |
| вызов раздачи | `RW 0x0069559E` | виртуальный вызов `share(value)` с опытом, который объект только что получил (ворота `value > 0` — `RW 0x0069556B`) |
| раздача | `RW 0x008832F0` | сама раздача (ниже) |
| падение расстояния | `RW 0x0088325B` | `max(0, 1 - d / Radius)` при `DropOff == 1.0`, иначе `1.0` |
| снимок | `RW 0x008E3BB1` | версия `(1, 1)` и базовый xfer (`RW 0x00850C60`) — больше ничего |

Раздача (`RW 0x008832F0`), по инструкциям:

```
00883305  movss   xmm0, [edi + 0x10]        ; Percentage
0088330A  comiss  xmm0, [0xC1B594]          ; 0.0
00883314  jbe     0x8834B3                  ; Percentage <= 0 -> ничего
0088331A  push    0x41                      ; статус 0x41 (BLOODTHIRSTY)
0088331C  call    0x44DDEC
00883325  jne     0x8834B3                  ; объект пьян -> ничего
0088332B  mov     eax, [esi + 4]            ; шаблон объекта
0088332F  mov     ebx, [eax + 0x110]        ; KindOf
00883335  shr     ebx, 0x1A                 ; бит 26 (HERO) — «источник-герой»
0088333A  and     bl, 1
```

Дальше — фильтры раздела `ThePartitionManager` (`RW 0xC1D660` — «не сам объект», `RW 0xC10E20` — «жив»,
`RW 0xC11DC0` — «союзники объекта», `RW 0xC0F374` — «никогда не срабатывает», `RW 0xBE4CC8` — `ObjectFilter` модуля),
и для каждого попадания:

```
0088341F  test    bl, bl                    ; источник-герой?
00883421  je      0x88343D
00883423  mov     eax, [0xDE412C]           ; TheGameLogic
00883428  cmp     byte ptr [eax + 0x9F], 0   ; переключатель «герои делят опыт»
0088342F  jne     0x88343D                  ; включён -> героя-получателя не пропускаем
00883431  mov     eax, [esi + 4]
00883434  test    byte ptr [eax + 0x113], 4  ; KindOf-бит 26 получателя (HERO)
0088343B  jne     0x883494                  ; -> пропустить
0088343D  mov     ecx, [esi + 0x26C]        ; ExperienceTracker получателя
00883448  je      0x883494
0088344A  call    0x79D322                  ; принимает ли опыт
00883451  je      0x883494
00883453  fld     dword ptr [ebp + 8]        ; value
00883459  fmul    dword ptr [edi + 0x10]     ; * Percentage
00883464  fstp    dword ptr [ebp + 8]
00883467  call    0x88325B                  ; падение расстояния -> [ebp - 0x44]
0088346C  fmul    dword ptr [ebp + 8]        ; * falloff
0088346F  fstp    dword ptr [ebp + 8]
0088347B  jbe     0x883494                  ; value <= 0 -> ничего
0088348F  call    0x79D833                  ; addExperiencePoints(value, true, true, true, false)
```

Значение **не восстанавливается** между получателями: следующий получает произведение предыдущего
(регистр `[ebp + 8]` переиспользуется) — так же и в порту.

## Классификация семи непроцитированных функций

`docs/ennorath/RW_FUNCTION_MAP.tsv` не цитирует семь функций класса глубины 0:

| RW (первая инструкция) | что делает | вердикт |
|---|---|---|
| `0x004B612F` | общий помощник ModuleData: слот `+0x38` vtable `RW 0xC5EEA0` (`push 0xDC62B8` — пустая строка, `call 0x435F30`); тот же слот у 319 классов, донор `0x004B69A1` | **не поведение**: общая машинерия ModuleData, не классовая |
| `0x008830A2` | `mov eax, 0xC0B55C` — строка-имя класса `ShareExperienceBehavior` | **не поведение**: идентификатор класса (в порте — имя в `bindRuntime<>`) |
| `0x008830A8` | одноразовая регистрация: бит в `0x00DE91A0`, ключ имени в `0x00DE919C` (`push 0xC0B55C`, `call 0x5487EC`) | **не поведение**: регистрация в реестре имён |
| `0x008831AC` | скалярный удаляющий деструктор поведения (`call 0x00833082`, условный `delete`) | **не поведение**: время жизни C++ |
| `0x008831C8` | `update`: отладочная трасса + `UPDATE_SLEEP_FOREVER` (`mov eax, 0x3FFFFFFF`); изменений состояния нет | **перенесено эквивалентно** (спит вечно, `RW 0x00883196` -> `RW 0x00850C32`); не перенесена только отладочная трасса — стоп `S-7545` |
| `0x0088320F` | удаляющий деструктор данных модуля (`call 0x0088322B`, условный `delete`) | **не поведение**: время жизни C++ |
| `0x008E3BB1` | `xfer`: `call 0x00404F15` (версия `(1, 1)`) + `call 0x00850C60` (базовый xfer) | **перенесено**: в порте было лишнее `u64(m_shares)` — убрано (см. ниже) |

Итог: из семи шесть — не игровое поведение, одно (обновление) уже эквивалентно перенесено, и одно (`xfer`)
перенесено с отклонением, которое исправлено.

## Чего не хватало: переключатель `TheGameLogic + 0x9F`

В порту читатель был заменён константой:

```cpp
const bool heroesShareWithHeroes = true; // TheGameLogic + 0x9F (INFERENCE: 1 in a game, RW 0x62D299)
```

Цепочка в RotWK (доказана адресами, а не выведена):

1. **Владелец.** `TheGameLogic` — глобальный указатель `0x00DE412C`: в `.text` ровно одна запись в него
   (`RW 0x0062CD16`: `mov dword ptr [0xDE412C], edi` — конструктор `GameLogic`) и 1775 чтений.
   Косвенное подтверждение: `RW 0x0040116D` делает `dec dword ptr [ecx + 0x1B4]` для объекта из `[0xDE412C]`,
   а в порте `GameLogic::m_depth` — это `RW + 0x1B4` (`engine/src/GameLogic/GameLogic.h`).
2. **Поле.** `TheGameLogic + 0x9F` — байт. Реальных обращений к `GameLogic + 0x9F` ровно пять:
   `RW 0x0062D299` (конструктор `GameLogic`: `mov byte ptr [esi + 0x9F], 1` — значение по умолчанию **1**),
   `RW 0x007BB038` (членский сеттер `GameLogic`, семейство `+0x99` / `+0x9A` / `+0x9B` / `+0x9F`),
   `RW 0x007BD3DB` (сеттер через глобаль: `mov ecx, [0xDE412C]; mov byte ptr [ecx + 0x9F], al`),
   `RW 0x00883054` (членский геттер: `mov al, byte ptr [ecx + 0x9F]; ret`),
   `RW 0x00883428` (читатель в раздаче).
   Остальные совпадения смещения `+0x9F` в `.text` — ложные или чужие: `RW 0x005F079D` (байты таблицы данных,
   не код), `RW 0x005F1563` (`cmp byte ptr [ebx + 0x9F], 0` — это `Player`, а не `GameLogic`: рядом читаются
   `Player`'s `+0x40` (флаги `0x20000` / `0x40000`) и `+0x80` / `+0x84` / `+0x88` (float'ы)),
   `RW 0x00630146` (конструктор другого класса: `mov esi, ecx` по `RW 0x630031`, дефолты `+0x9A` .. `+0x9F`;
   конструктор `RW 0x630023` с vtable `0xBFDB68` вызывается фабрикой `RW 0x441904`, где объект наследника
   получает vtable `0xBD8590`, — это не `GameLogic`).
   Сбросов флага нет: default-записи `GameLogic::init` / `reset` / `startNewGame` (`RW 0x62D1CB` .. `RW 0x62D1E0`,
   `RW 0x62D3CF` .. `RW 0x62D3E4`, `RW 0x62A22F` .. `RW 0x62A23D`) пишут только `+0x98` .. `+0x9B`; значение `1`
   ставится один раз конструктором, флаг переживает старт новой игры (порт: `m_heroExperienceSharing` не трогает
   ни `GameLogic::reset`, ни `startNewGame` — его меняет только действие 538).
3. **Писатель.** Скриптовое действие `SET_HERO_EXPERIENCE_SHARING` (ID 538 в таблице шаблонов порта,
   `engine/src/GameLogic/ScriptEngine/ScriptTemplateTables.inc`; в RotWK строка `SET_HERO_EXPERIENCE_SHARING`
   лежит по `VA 0x00C48DDC`, а её описание — «Player_/Experience/Set whether heroes share experience with
   other heroes», `VA 0x00C48D90`). Обработчик: `RW 0x007CDF99` -> `RW 0x007BD3D1`:
   `mov al, [esp + 4]; mov ecx, [0xDE412C]; mov byte ptr [ecx + 0x9F], al; ret 4` — значение параметра
   (int, `+ 8`) сравнивается с нулём (`setne`). Это ровно та же форма, что у уже перенесённого
   `OPTIONS_SET_OCCLUSION_MODE` (`RW 0x7CE207` -> `RW 0x7BD549`, `GameLogic + 0x99`).
4. **Читатель.** `RW 0x00883428` в раздаче (см. листинг выше).
5. **Сохранение.** `GameLogic::xfer` (`RW 0x00630D6F .. RW 0x00630D78`) байт `+0x9F` не читает: в `.text`
   нет ни одного обращения `+0x9F` в теле xfer'а. Значит, retail и не сохраняет флаг, и не хеширует его.
   Порту это же и нужно: флаг не в `crc` модуля (`UpdateModule::crc` хеширует только `m_nextCallFrame`,
   `m_indexInLogic`, `m_phaseInLogic`), поэтому пины `perf1` / `uber1` / `save4*` не сдвигаются.

Донор BFME2 1.06 читает то же самое место (`ShareExperienceBehavior.cpp`, строка 261):
`if (selfFlag && !((Rva00480093GameLogicView *)TheGameLogic)->m_heroExperienceSharing && (other->m_template->m_kindof[2] & 0x04000000))`.

## Что перенесено

| что | где | адрес RotWK |
|---|---|---|
| флаг `GameLogic + 0x9F` и его геттер/сеттер | `engine/src/GameLogic/GameLogic.h` | `RW 0x62D299` (по умолчанию 1), `RW 0x7BD3D1` (писатель) |
| скриптовое действие 538 `SET_HERO_EXPERIENCE_SHARING` | `engine/src/GameLogic/ScriptEngine/ScriptActions.cpp` | `RW 0x7CDF99` -> `RW 0x7BD3D1` |
| читатель в раздаче | `engine/src/GameLogic/Module/HeroAbilityModules.cpp` (`ShareExperienceBehavior::share`) | `RW 0x883428` |
| раскладка `xfer` | там же (`ShareExperienceBehavior::xfer`) | `RW 0x8E3BB1` |

`xfer` в порте писал лишний `SnapshotXfer::u64(x, m_shares)` — счётчик разделения, диагностика порта,
которого в retail нет: `RW 0x008E3BB1` пишет только версию и базовый xfer. Счётчик оставлен в памяти
(его читает тест), но больше не попадает в снимок: байты снимка не должны зависеть от счётчика, который
не состояние (`docs/SAVES.md`, раздел 6). Свою версию 1 порт писал с этим счётчиком, поэтому версия
поднята до 2, а версия 1 читается со сбросом счётчика (`docs/SAVES.md`, раздел 5: «изменение сохранённого
состояния класса поднимает `current` и добавляет ветку чтения старой раскладки»).

## Тест

`engine/tests/test_port_301.cpp`, шесть тестов `PORT-301*`:

1. герой делится опытом с союзным героем при `+0x9F = 1` (по умолчанию): `100 * 0.5 * (1 - 50 / 100) = 25`;
2. при `+0x9F = 0` герой-получатель пропускается (падает без правки);
3. при `+0x9F = 0` обычный пехотинец всё равно делится с героем (пропуск требует героя-источника);
4. скриптовое действие 538 пишет `+0x9F` (падает без правки: действие было не перенесено);
5. снимок поведения содержит только версию и базовый xfer: счётчик разделения не меняет сохранённые байты
   (падает без правки);
6. снимок версии 1 (собственная версия порта: байт версии 1 и счётчик после базового xfer) грузится:
   счётчик читается и отбрасывается (`docs/SAVES.md`, раздел 5).

## Осталось

* `S-7545` — отладочная трасса `ShareExperienceBehavior::update` (`RW 0x008831C8`): в порте нет
  `Debug`-синглтона retail'а (`0x00DC62C0`), а он — общий механизм, а не механизм этого класса.
* Мод-фича `EdainUnchainedShareExperienceAll` (все модули делят, а не первый) — это полоса Edain Unchained
  (`engine/src/Common/ModEngineFeatures.h`), к Ennorath не относится.
