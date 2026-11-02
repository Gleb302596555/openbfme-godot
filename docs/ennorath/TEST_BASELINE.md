# Базовый список падений

Правило в [PROCESS.md](PROCESS.md), раздел 7: падение перед `land` допускается, только если тест записан здесь с тем же сообщением. Записи добавляются только по полному прогону на чистом `main`. Если тест исправлен, запись убирается в той же ветке.

Основание: полный прогон `1ac41052` (2026-11-02), перепроверка на `2af911bb` и на `0b58d290`. На `b9581453` полный прогон ещё не сделан: при первом таком прогоне список нужно обновить.

## Окружение этой машины

| Тест | Причина |
|---|---|
| `audio decode: MP3` (5 тестов) | нет `lame` и `mpg123` |
| `binary: AptMainMenu`, `binary: AptSkirmish` | нет `game.dat` в `ROTWK_INSTALL` |
| `win1 msvcr71 real` | нет `msvcr71.dll` в `ROTWK_INSTALL` |

## Падают на чистом main, не исправлены

| Тест | Место или сообщение |
|---|---|
| `backlog8 retail: the FLOATING_Z horde members...` (P-31) | `test_hud_horde2_retail.cpp:991`, `memberIds(archers).size() == 6` |
| `MODS-1` (3 теста: `overridden Apt movie`, `parseMod`, `precedence`) | `test_mods1_mount.cpp:270`, `:110`, `:171` |
| `script4 conditions: BUILT_BY_PLAYER...` | `test_script4.cpp:446`, `PLAYER_HAS_SCIENCEPURCHASEPOINTS` |
| `hero abilities retail: FactionIsengard` | `test_hero_retail.cpp:1461`, `unexplained == 0` |
| `online16-fix` (3 теста), `online3 spectators`, `onlineux1 r2` | `remove_all`: файл занят другим процессом |
| `online2: a classic room's relay log...` | `test_online_relay.cpp:334`, digest |
| `relay4: valid one-human room logs...` | `test_online_relay.cpp:750`, digest |

## Нестабильные

| Тест | Как ведёт себя |
|---|---|
| `fbsim1 retail: a right press on a CANCELABLE...` | падает в наборе (`test_hud6.cpp:787`), отдельно проходит |
| `net retail hud5` | падает время от времени |
