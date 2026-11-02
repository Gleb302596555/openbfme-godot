# Проверка партии

Файл создан `python tools/mod/ennorath_queue.py checklist --update`, вручную не правится. Один список на партию вместо отдельных просьб. Итог каждой строки записывается в `MODULE_GAPS.md` или `PROBLEMS.md`.

## Записи владельца (3)

- [ ] **P-1**. Силы Rebuild и Cloud Break у Гондора иногда не применяются
- [ ] **P-2**. Три силы Гондора не оценены сценарием: Lehen, DieGraueSchaar, SummonRohanGondor
- [ ] **P-3**. Книга Гондора: у `SpellBookWlderIthilliens` не перенесены бонусы области (S-856)

## Классы, которые нужно посмотреть в игре (32)

Сверху те, что встречаются в моде чаще.

- [ ] `PickupStuffUpdate` (347; все 11): только компьютерный игрок (SkirmishAIOnly=Yes): отряды ИИ подбирают сундуки; 692 объекта в данных, 17 живых в переписи
- [ ] `CommandButtonHuntUpdate` (84; Angmar, Arnor, Dwarves, Elves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan): охота по кнопкам (скрипты/ИИ); блок в данных пустой; в переписи 4 живых
- [ ] `GeometryUpgrade` (71; все 11): улучшение здания переключает его геометрию: след, путь юнитов, выбор; в переписи 27 живых
- [ ] `W3DBuffDraw` (50; все 11): свечение баффов (лечение, оглушение, мороз): рисуется на юнитах через `ClientBuffs` (S-4642); сам `W3DBuffDraw::doDrawModule` в обеих редакциях пустой (RW 0xBE3010 / BFME2 0xBCD178 слот 11 RW 0x9F3A3C = `ret 4`); перенесён типизированный `W3DBuffDrawModuleData` (`ModelName` +0x08, `PreDraw` +0x0C, RW 0x4CE61A / 0xBE318C, BFME2 0x4CEE4C / 0xBCD2F4) и передача `PreDraw` в `LiveFX::turnOnBuff` / `ClientBuffs::turnOn` (RW 0x707FFF / 0x4D1981, BFME2 0x70682A / 0x4D21B5)
- [ ] `AimWeaponBehavior` (32; Angmar, Dwarves, Elves, Evilmen, Imladris, Mordor, Rohan, Wild): прицеливание оружия по порогам AimLow/High и дистанциям (Haradrim, демон); эффект по декомпиляции не установлен; в переписи 49 живых
- [ ] `TerrainResourceClientBehavior` (26; все 11): круги зон ферм и дуги контуров (S-5030); перенесён `TerrainResourceClientBehavior` (RW 0x6549B1 / 0x652B8C / 0x8D0A08 / 0x8D0A58 / 0x8D0A78, BFME2 0x654C56 / 0x652DEB / 0x8CC55A / 0x8CC5AA / 0x8CC5CA), слоты 17/18 `DrawableModule` (`createSelectionFeedback` / `removeSelectionFeedback`, RW 0x672ED1 / 0x672F0D, BFME2 0x672BAB / 0x672BE7) и счётчик пользователей `TerrainResourceFeedback::show` / `hide` (RW 0x8EC271 / 0x8EC449, BFME2 0x8E7B0C / 0x8E7D1D)
- [ ] `GiantBirdSlowDeathBehavior` (19; все 11): падение орлов, назгулов на птицах, дракона: сейчас обычная смерть
- [ ] `ClickReactionBehavior` (16; Imladris, Mordor, Wild): реакция-анимация на щелчок по юниту (INFERENCE)
- [ ] `AudioLoopUpgrade` (15; все 11): звук стройки/улучшения здания, пока оно строится
- [ ] `EvacuateDamage` (13; Arnor, Dwarves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan): боевая башня при попадании указанного оружия (MordorCatapultHumanHeads) выгоняет гарнизон
- [ ] `W3DTornadoDraw` (11; Arnor, Elves, Evilmen, Imladris): смерч Радагаста, ветры Алатара, вихрь Элронда: модуль отрисовки декалей смерча и следования за объектом
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
- [ ] `OilSpillUpdate` (2; Dwarves, Mordor): масляный след и поджиг (Mordor, Dwarves): спавн хлебных крошек (OilSpill), поджог и FXEvent
- [ ] `OneRingPenaltyUpdate` (1; Elves): Око Саурона вызывается, если Фродо носит кольцо слишком долго
- [ ] `ProjectileStreamUpdate` (1; Wild): логика потока: дальность и цепочка снарядов
- [ ] `StrafeAreaUpdate` (1; Wild): дракон обстреливает область (сила Dragon Strike)
- [ ] `W3DLightDraw` (1; Mordor): световые пятна факелов, костров, вспышек (на картах)
- [ ] `W3DProjectileStreamDraw` (1; Wild): поток огня/струя (огненный дракон)
- [ ] `CritterEmitterUpdate` (0; —): птицы/мелкая живность на картах; в переписи 13 живых
- [ ] `ShipSlowDeathBehavior` (0; —): тонущие корабли

## Проверено сценарием на данных мода (4)

Логику проверяет `openbfme_tests -tc="ENNORATH scenario*"`. Смотреть в игре стоит только внешний вид и звук.

- `ModelConditionAudioLoopClientBehavior` (500)
- `EvaAnnounceClientCreate` (21)
- `SalvageCrateCollide` (6)
- `GiveUpgradeUpdate` (4)
