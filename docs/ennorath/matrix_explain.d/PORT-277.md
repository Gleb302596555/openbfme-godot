# PORT-277 — кандидаты для проверки восьми кнопок владельцем

Это **не** окончательные BY-DESIGN-вердикты и не перезапись матрицы.
По ответу в #644 Claude проверит все восемь кнопок на настоящих данных
в сцене fall back 8p на ПК владельца, когда синтетические тесты будут готовы.
До его ответа реальные строки остаются красными. В репозитории только
самостоятельно написанные синтетические формы, без байтов/блоков мода,
Scripts.ini, архивов, переменных установки и внешних фикстур.

| Объект / кнопка | Кандидат / что проверить в оригинале | Проверяемая синтетическая ветка RW |
|---|---|---|
| IsenfurtWchter / Command_IsenfurtWchterSmite | Источник без HordeContain: ожидается no-op нагета; подтвердить весь эффект кнопки, включая альтернативные модули. | 0x911A58: ранний возврат, нет урона. |
| IsenfurtWchterCreep / Command_IsenfurtWchterSmite | То же, отдельный объект и отдельный тест. | 0x911A58. |
| IsenfurtWchterHorde / Command_IsenfurtWchterSmite | Слот TERTIARY содержит метательное оружие; проверить урон по реальной цели, начало выстрела и сцену. | 0x911A58, 0x69121A, 0x875221: приказы, lock, урон через projectile/warhead бойца. |
| IsenfurtWchterHordeHerold / Command_IsenfurtWchterSmite | В прочитанных определениях кнопка и starter/updater называют разные power, имя из модулей не определено. Проверить реальный HUD/gate; не исправлять INI обходом в движке. S-7472. | 0x73B22F, 0x68C26D, 0x8969D1, 0x82D925: нет matching interface, кнопка отвергнута; принудительно доставленный нагет независим и работает. |
| DwarvenAxtschwingerHorde / Command_AxtschwingerAxeThrow | TERTIARY с projectile/warhead; проверить выпуск range-finder и реальный damage trace/цель. | 0x911A58, 0x875221. |
| AngmarDarkRangerHorde / Command_AngmarRangersIceArrowAbility | QUINARY с projectile/warhead; перед кликом проверить Upgrade_ObjectLevel1 и unpause. | 0x8B9676; 0x911A58, 0x875221. |
| GondorRangerHorde / Command_MenLongShotAbility | У dummy-оружия нет нагетов; отдельный OCLSpecialPower создаёт volley/FireWeaponUpdate. Проверить весь OCL-эффект, не ждать урона только от range-finder. | 0x911AC9, 0x875550; отдельно 0x8C75D8, 0x88F554. |
| DwarvenDain_mod / Command_SpecialAbilityDrarSpeerfeuer | Нет HordeContain; отдельный OCL → egg → delayed death OCL → projectile/FireWeaponUpdate. Проверить этот путь, а не только no-op MenLongShotWeapon. | 0x911AC9; отдельно 0x8C75D8, 0x88F554. |

## Исправление предыдущей диагностики dummy

Первый красный dummy-тест не имел связанного SECONDARY вообще: определение
оружия шло после ссылки. Теперь оружие объявлено до объектов и непустой
слот проверяется до выстрела. Сохранены проверки выбранного слота и
отсутствия урона; добавлен независимый контрпример позднего определения.
RW 0x911A58 / 0x911AC9 читает **nugget +0x14C (LockWeaponSlot)**, затем
вызывает 0x69121A до dispatch: слот PRIMARY самого extra weapon не является
выбором слота бойцов. См. [отчёт](../PORT-277.md) и тесты `PORT-277*`.

## Граница вывода

Общий SweepEffectProbe обходил также модули отдельных бойцов и мог
добавлять ожидание DAMAGE без оружия при отсутствии HordeContain или
при dummy. Это объяснение по чтению кода, не результат повторного sweep;
применимость к каждой конкретной красной строке остаётся INFERENCE.
Общий probe и функции Local 76 здесь не менялись. Проверка синтетики
после выстрела не подтверждает реальный HUD-клик или всю способность.

Самодостаточный набор после полной Release Godot OFF -j1 сборки:
**PORT-277*: 12/12 тестов, 463/463 проверки, SUCCESS**. Восемь кнопочных
сценариев отдельно: **8/8, 292/292, SUCCESS**. Случайный порядок seed=277:
**12/12, 463/463, SUCCESS**. Все восемь синтетических строк таблицы прошли;
реальные восемь кнопок этим результатом не подменяются.
