# Port 302 — InactiveBody: разбор семи собственных функций RotWK

Все адреса прочитаны через `python tools/mod/rw.py workspace/RotWK/game.dat dis <адрес>`.
Донор BFME2 1.06 (`workspace/BFME2/game.dat`, `git clone` Open-BFME-2 в `/tmp/open-bfme-2`) использован только для имён и порядка слотов; где RW отличается, описано по RW.

## Таблица функций

| RW | Слот | Что делает по инструкциям | Состояние в порте |
|---|---|---|---|
| `0x008C18F1` | primary `0xC71978` +8 (имя модуля) | `mov eax, 0xC0AA40; ret` — указатель на строку `InactiveBody`. Состояния не меняет. | Сервис: имя зарегистрировано в `StructureModules::registerAll`; отдельного геттера нет и он не нужен. |
| `0x008C191D` | body `0xC718C8` +4 (`attemptHealing`) | `0x8C1921`: null-указатель → выход; `0x8C1925`: тип 7 (HEALING) → `0x8C1932` обнулить output +0x70/+0x74, `noEffect` (+0x78) = true, `ret 4`; иначе `0x8C192B/0x8C192E` — vcall слота +0 (`attemptDamage`) с тем же `DamageInfo`. | Поведение есть в `InactiveBody::attemptHealing`, но первая инструкция не процитирована; добавляем цитату. |
| `0x008C19A6` | primary +0x10 (`getModuleNameKey`) | `0x8C19B0`: однократная инициализация guard `0xDE9AE4`, `nameToKey("InactiveBody")` (`0x5487EC`), результат в `0xDE9AE0`. Никакого игрового состояния. | Сервис: ключ имени даёт `Module::getModuleNameKey()`. |
| `0x008C19EB` | body +0x80 | `0x8C19F2..0x8C1A03`: записать два нулевых float через скрытый указатель результата (zero Coord2D, PORT-78). | Поведение есть: `BodyModuleInterface::getGrabOffset()` по умолчанию `Coord2D{}`; цитата добавляется рядом с объявлением. |
| `0x008C1A59` | primary +0 (scalar deleting destructor) | `0x8C18D8` восстанавливает vtables `0xC71978/0xC72F08/0xC718C8`, `jmp 0x8C18AC` в базовый деструктор; ветка `test [esp+8],1` (`0x8C1A61`) вызывает `operator delete`. | Сервис: деструктор C++ и управление памятью порта. |
| `0x008C1AC3` | body +0x3C | `0x8C1AD4..0x8C1AE3`: скопировать четыре нулевых байта в скрытый результат и вернуть его. Это `ArmorSetFlags` (у ActiveBody слот +0x3C — `0x8C3A91`, читает слово из `ActiveBody+0xE0`; `DONOR BFME2 0x008BD97A`, `ActiveBody::rva004BF8F5` в `Code/GameEngine/Source/GameLogic/Object/Body/ActiveBody.cpp`). Слоты +0x34/+0x38 у InactiveBody — `0x9F3A3C` (`ret 4`): `setArmorSetFlag`/`clearArmorSetFlag` не меняют флаги, и вызов ArmorUpgrade (`RW 0x8B761E`, `call [eax+0x34]` по `0x8B76A5`) остаётся без эффекта. | **Не портировано.** Порт хранит флаги на `Object` (`Object::setArmorSetFlag`), поэтому upgrade на объекте с InactiveBody меняет `armorSetFlags()`, чего в RW нет. |
| `0x008C1AEA` | primary +12 (override регистрации тела) | `0x8C1AF4` вызывает `0x404F15` на объекте (vcall +0x28 с двумя `true`), `0x8C1AFC` — базовый помощник `0x993419`, затем `0x8C1B09`: Object vcall +0x90 с указателем на `this+0x18` (`m_dieCalled`). | Порт регистрацию тела делает через `getBody()`, а флаг `m_dieCalled` живёт в модуле; **`INFERENCE`**: смысл Object vslot +0x90 по бинарнику не доказан, отдельная правка не делается (стоп S-7548). |

## Поведение соседних слотов, проверенное по ходу разбора

- body +0x10..+0x20 — `0x70D41A` (`fld [0xC1B594]`, константа 0): здоровье 0, `BODY_PRISTINE`.
- body +0x78 — `0x851E97` (`xor eax,eax; ret`), `getGrabFXName()` = null; +0x90 — `0x70D41A`, `getGrabDamage()` = 0.
- body +0x74 (GrabObject) — `0x8C1B14`, возвращает пустую строку `0xBD0C3F`; +0x6C — `0x8C18C5` (умножение скаляра).
- primary +4 (`getModuleNameKey`) — `0x8C19A6`, primary +8 (`getModuleName`) — `0x8C18F1`: обе строки регистрации совпадают.

## Границы переноса

`0x008C1AEA` требует значения Object vslot +0x90 (адрес, где объект сохраняет указатель на флаг смерти); без доказанного читателя это было бы `INFERENCE`, меняющее поведение, видимое игроку. Вынесено в стоп S-7548, остальные шесть функций либо сервис, либо уже портированы и получают цитату первой инструкции.
