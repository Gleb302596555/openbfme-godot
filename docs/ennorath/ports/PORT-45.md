# PORT-45: ActiveBody / DamageCreationList — разбор RotWK 2.01 (issue #149)

Бинарник: `game.dat` md5 `daed3668006cd90f01c34e5a7da1901f` (оговорка S-001). Все адреса прочитаны
`tools/mod/rw.py dis` на этом файле. Данные мода не публикуются (только имена).

## Что делает поле в RotWK

`DamageCreationList = <OCL> <damage type> [<death side>]` — **три** токена, а не «OCL + тип + кость/состояние».
Строка таблицы полей ActiveBodyModuleData RW 0xC71E38 (offset 0 в таблице, но это не адрес члена: парсер
пишет в вектор по `+0x58`, см. ниже), парсер RW 0x8C3F1F.

| шаг парсера | адрес | что делает |
|---|---|---|
| OCL | RW 0x8C3F37 → RW 0x73A368 | `INI::parseObjectCreationList`: TheObjectCreationListStore (`0xDE370C`) → `findObjectCreationList` (RW 0x5EFFE7); строка «NONE» и неизвестное имя дают NULL |
| тип повреждения | RW 0x8C3F40 .. 0x8C3F5B | токен → `scanIndexList` (RW 0x42B999) по списку RW 0xDB2AF4 = `NONE`(0), `CATAPULT_ROCK`(1), `TREBUCHET_ROCK`(2) |
| death side | RW 0x8C3F61 .. 0x8C3F7C | токен → `scanIndexList` по списку RW 0xDB2B04 = `NONE`(0), `FRONT_DESTROYED`(1), `RIGHT_DESTROYED`(2), `BACK_DESTROYED`(3), `LEFT_DESTROYED`(4) |
| запись | RW 0x8C3F81 .. 0x8C3F88 | `push_back` 12-байтовой записи в вектор данных модуля `+0x58` (`lea ecx,[ebx+0x58]`, RW 0x6E0A42) |

Запись: `+0x00` ObjectCreationList*, `+0x04` тип (индекс списка RW 0xDB2AF4), `+0x08` death side (индекс
списка RW 0xDB2B04). Строка «тип» — не DamageType, а FXTrigger (тот же список, что `Weapon::FXTrigger`,
порт `TheFXTriggerNames`, RW 0xDA14FC: `NONE`, `CATAPULT_ROCK`, `TREBUCHET_ROCK`).

Конструктор данных RW 0x8C373F конструирует вектор по `+0x58` (RW 0x8C3786 `call 0x478CBE`).
Конструктор модуля RW 0x8C3841 копирует его в экземпляр `+0xE0` (вектор 12-байтовых записей; RW 0x8C390B
и RW 0x8C38A1 — те же `lea [esi+0xE0]`), деструктор RW 0x8C3AED освобождает его (RW 0x8C3B24).

### Читатель 1: doDamageFX (RW 0x8C2F02), слот 18 основной vtable

```
8C2F75  mov  eax, [ebx+0xE0]        ; начало вектора экземпляра
...
8C2F88  mov  ecx, [ebp-0x14]        ; запись + 0x04 (тип)
8C2F8B  cmp  ecx, [edx+0x2C]        ; == DamageInfo + 0x2C (m_creationType; из input + 0x28)
8C2F8E  jne  next
8C2F90  xor  ecx, ecx
8C2F92  cmp  [ebp-0x10], ecx        ; запись + 0x08 (death side) == 0
8C2F95  jne  next
8C2F97  cmp  [ebp-0x18], ecx        ; запись + 0x00 (OCL) != NULL
8C2F9A  je   next
8C2F9C  push ecx                    ; 0  (lifetime)
8C2F9D  push ecx                    ; 0  (secondary position)
8C2F9E  push [ebx+8]                ; сам объект — «primary»
8C2FA1  mov  ecx, [ebp-0x18]        ; OCL
8C2FA4  call 0x5F0126              ; ObjectCreationList::create (порт: ObjectCreationList::create)
8C2FAC  add  eax, 0xc               ; следующая запись
8C2FAF  cmp  eax, [ebx+0xE4]        ; конец вектора
```

- Ветка FX (throttle, `[ebx+0xFC]` = m_curDamageFX, RW 0x8C2F1E `je 0x8C2F75`) переходит в цикл, когда
  DamageFX нет или throttle его пропустил: цикл — не часть FX-ветки, он идёт всегда (RW 0x8C2F75 —
  общая точка после `je`).
- Создаётся только запись с `death side == 0` (`NONE`), т.е. третьего токена в данных мода нет ни у одного
  шаблона; записи с `FRONT_DESTROYED` и т.п. этим путём **не** создаются (их создаёт читатель 2, только у
  стен).
- `create(primary = сам объект, secondary = 0, lifetime = 0)`: в порту
  `ObjectCreationList::create(logic, &obj, *obj.getPosition())` — та же перегрузка, что у
  TransitionDamageFX / WeaponDelivery (RW 0x5F00CA → 0x5F0126).
- Вызывается из `attemptDamage`: RW 0x8C3FA3, вызов слота 18 (`doDamageFX`) — порт
  `ActiveBody::doDamageFX(const DamageInfo &)` в конце `attemptDamage` (там же уже стоит FX-часть).

`m_creationType` (DamageInfo input `+0x28`, в записи `+0x2C` от DamageInfo) — порт `DamageInfoInput::m_fxTrigger`
(`D+0x2C` в порту; в записи разница: `[edx+0x2C]` = input `+0x28`). Источник значения — `Weapon::FXTrigger`
(порт `Weapon::m_fxTrigger`, парсер RW 0x42... список RW 0xDA14FC), копируется в DamageInfo (порт:
`VitalDamage`/`resolveWeaponDamage` берут `weapon.m_fxTrigger` → `in.m_fxTrigger`, `GameLogic/Object/Damage.cpp:235`).

### Читатель 2: ветка «стороны повреждения» (RW 0x8C2EA9 .. 0x8C2EEF)

```
8C2EA9  mov  [ebx+0x34], esi        ; m_field34 = стадия 1..4 (сторона, по которой пришёлся урон)
8C2EAC  cmp  byte ptr [ebp+8], 0    ; аргумент «initial» (applyInitialDamageState) -> цикл пропускается
8C2EB5  mov  eax, [ebx+0xE0]
8C2EC5  mov  ecx, [ebp-0x1c]        ; запись + 0x08 (death side)
8C2EC8  cmp  ecx, [ebx+0x34]        ; == стадия (1 FRONT, 2 RIGHT, 3 BACK, 4 LEFT)
8C2ECD  mov  ecx, [ebp-0x24]        ; запись + 0x00 (OCL)
8C2ED0  test ecx, ecx               ; != NULL
8C2ED4  push 0 / push 0 / push [ebx+8]
8C2EDB  call 0x5F0126               ; OCL::create
```

Это BFME2 `setCorrectDamageState`-ветка сторон (BFME2 decomp: `m_stage == m_field34`; стадия считается из
долей здоровья/урона по сторонам, `m_damageStateValues[0..3]`, порог `0.75 * (maxHealth * 0.25)`).
В порту ветки сторон у структур нет (`StructureBody` и разбиение урона по сторонам не перенесены), поэтому
читатель 2 остаётся `STOPS`-строкой; записи с death side != 0 сейчас не создаются ничем.

## Что перенесено (эта задача)

- `ActiveBodyModuleData::m_damageCreationList` — типизированный вектор записей
  `ActiveBodyModuleData::DamageCreation { ocl name, fx trigger index, death side index }`, парсер
  RW 0x8C3F1F (два `scanIndexList`, OCL разрешается через TheObjectCreationListStore, как парсер RW 0x73A368).
- `ActiveBody::doDamageFX` — цикл RW 0x8C2F75 .. 0x8C2FB8: для каждой записи `trigger == info.m_input.m_fxTrigger
  && deathSide == 0 && ocl != null` → `ocl->create(logic, &obj, *obj.getPosition())` (RW 0x5F0126).
- Состояния нет: вектор — данные шаблона (ModuleData), в экземпляре RotWK он только копия (RW 0x8C390B), и
  копия не меняется; `xfer` не нужен (как у остальных ModuleData-векторов).

## Что не перенесено

- Читатель 2 (ветка сторон структуры, RW 0x8C2EBD .. 0x8C2EEF) — нет перенесённого разбиения урона по
  сторонам (`m_damageStateValues`, поле `+0x34` стадии). Записи с `FRONT_DESTROYED` и др. в данных мода есть
  (91 шаблон StructureBody), но создавать их нечем, пока не перенесены стороны. Строка STOPS.
- Throttle-условия FX-части (RW 0x8C2F20 .. 0x8C2F72) уже перенесены ранее (lane FX-2).

## Сохранение (xfer)

`ActiveBody::xfer` (RW 0x8C34xx): `+0xCC` (4 dwords, RW 0x8C3567 .. 0x8C358B), `+0xF0` (RW 0x8C3594), `+0xEC` (RW 0x8C35A1) — списки
частиц и связанный id. Вектор записей создания (`+0xE0`) в `xfer` **не входит**: он данные шаблона, в экземпляре только копия
конструктора (RW 0x8C390B) и в игре не меняется. Порт: новый `xfer` не нужен, нового состояния симуляции нет (PROCESS.md 15.5 не
затрагивается).

## Соседние места, которые правка трогает

- `GameLogic/FXEvents.cpp` — строка S-681 отчёта о неперенесённом: OCL-половина снята, сторонняя половина оставлена со ссылкой на S-149.
- `docs/STOPS.md` — S-149 (OCL-половина закрыта, сторонняя ветка открыта) и S-681 (то же).
- `engine/tests/test_logic_modules.cpp` — проверка `DamageCreationList` переведена на типизированные записи (без
  ObjectCreationList-стора имена OCL — пустые, как NULL в RW 0x5EFFE7).
