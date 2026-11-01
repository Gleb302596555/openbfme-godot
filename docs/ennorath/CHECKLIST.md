# Проверка партии

Файл создан `python tools/mod/ennorath_queue.py checklist --update`, вручную не правится. Один список на партию вместо отдельных просьб. Итог каждой строки записывается в `MODULE_GAPS.md` или `PROBLEMS.md`.

## Записи владельца (5)

- [ ] **P-1**. Силы Rebuild и Cloud Break у Гондора иногда не применяются
- [ ] **P-2**. Три силы Гондора не оценены сценарием: Lehen, DieGraueSchaar, SummonRohanGondor
- [ ] **P-3**. Книга Гондора: у `SpellBookWlderIthilliens` не перенесены бонусы области (S-856)
- [ ] **P-10**. Деньги за крипов на карте
- [ ] **P-16**. Анимация пещерных троллей гоблинов замирает

## Классы, которые нужно посмотреть в игре (24)

Сверху те, что встречаются в моде чаще.

- [ ] `PickupStuffUpdate` (347; все 11): только компьютерный игрок (SkirmishAIOnly=Yes): отряды ИИ подбирают сундуки; 692 объекта в данных, 17 живых в переписи
- [ ] `CommandButtonHuntUpdate` (84; Angmar, Arnor, Dwarves, Elves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan): охота по кнопкам (скрипты/ИИ); блок в данных пустой; в переписи 4 живых
- [ ] `GeometryUpgrade` (71; все 11): улучшение здания переключает его геометрию: след, путь юнитов, выбор; в переписи 27 живых
- [ ] `AimWeaponBehavior` (32; Angmar, Dwarves, Elves, Evilmen, Imladris, Mordor, Rohan, Wild): прицеливание оружия по порогам AimLow/High и дистанциям (Haradrim, демон); эффект по декомпиляции не установлен; в переписи 49 живых
- [ ] `GiantBirdSlowDeathBehavior` (19; все 11): падение орлов, назгулов на птицах, дракона: сейчас обычная смерть
- [ ] `ClickReactionBehavior` (16; Imladris, Mordor, Wild): реакция-анимация на щелчок по юниту (INFERENCE)
- [ ] `AudioLoopUpgrade` (15; все 11): звук стройки/улучшения здания, пока оно строится
- [ ] `EvacuateDamage` (13; Arnor, Dwarves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan): боевая башня при попадании указанного оружия (MordorCatapultHumanHeads) выгоняет гарнизон
- [ ] `AutoPickUpUpdate` (10; Mordor, Wild): горный тролль сам хватает и ест подходящего юнита (INFERENCE)
- [ ] `EntEnragedUpdate` (10; Elves, Rohan): энты впадают в ярость рядом с лесопилками врага или гибелью своих
- [ ] `RebuildHoleExposeDie` (10; Wild): уничтоженное логово крипа или здание оставляет яму, через время её отстраивают заново (P-29, INFERENCE по полям HoleName/HoleMaxHealth)
- [ ] `SplitHordeSpecialPower` (10; Isengard, Mordor): кнопка разделения составного отряда (Изенгард, Мордор)
- [ ] `GrabPassengerSpecialPower` (9; Mordor, Wild): тролль хватает юнита и бросает его (пещерные тролли, Рогаш); сейчас кнопка есть, эффекта нет (P-16)
- [ ] `ReplaceObjectUpdate` (6; Imladris, Rohan): способности героя-создания, заменяющие объект (Word of Poultry)
- [ ] `RebuildHoleBehavior` (5; Wild): сама яма: рабочий появляется через WorkerRespawnDelay и отстраивает объект
- [ ] `BoredUpdate` (4; Isengard, Wild): скучающий тролль сам применяет способность (INFERENCE)
- [ ] `StoreObjectsSpecialPower` (4; Dwarves, Evilmen, Imladris): запоминает отряд для телепорта
- [ ] `TeleportToCasterSpecialPower` (4; Dwarves, Evilmen, Imladris): телепорт отряда к герою-создателю
- [ ] `HordeTransportContainDamage` (3; Evilmen): мумак: урон делится на бойцов-пассажиров
- [ ] `LocomotorSetUpgrade` (3; Arnor, Evilmen): улучшение меняет набор передвижения (лазание по стенам героя-создания)
- [ ] `FreeLifeBody` (2; Arnor, Imladris): Глорфиндел воскресает на месте после смерти на 10 уровне (FreeLifeTime)
- [ ] `OneRingPenaltyUpdate` (1; Elves): Око Саурона вызывается, если Фродо носит кольцо слишком долго
- [ ] `CritterEmitterUpdate` (0; —): птицы/мелкая живность на картах; в переписи 13 живых
- [ ] `ShipSlowDeathBehavior` (0; —): тонущие корабли

## Проверено сценарием на данных мода (3)

Логику проверяет `openbfme_tests -tc="ENNORATH scenario*"`. Смотреть в игре стоит только внешний вид и звук.

- `ModelConditionAudioLoopClientBehavior` (500)
- `SalvageCrateCollide` (6)
- `GiveUpgradeUpdate` (4)
