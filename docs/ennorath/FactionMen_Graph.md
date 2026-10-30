# FactionMen - generated graph and spell table, Ennorath 1.9.8 (tools/mod/ennorath_docs.py; hand-kept state is in FactionMen.md)

[Plan](../ENNORATH_FACTION_PLAN.md). Rows are **INVENTORIED** (found in the mounted data); logic, UI and visuals are **TODO** unless a row says otherwise. The graph walks reference fields of the mounted INI text; it is an over-approximation, not the engine's parse.

## Profile (E0)

- mount order (lowest first): BFME2 1.06 `ini.big`, `_patch101.big`, `_patch103.big`, `bases.big` < RotWK 2.01 `INI.big`, `Bases.big`, `_patch201ini.big` < the Ennorath overlay's `.big` files
- INI/INC text and castle layouts (`.bse`) only; this walk does not read models, textures, sounds or maps
- `overlay/____enrth_data.big` SHA-256 `5a1995f0b382b1fe4d05814279ec7bc31cca1d145ffce7edb6d1d6b4fae946e9`

## Graph size

| Kind | Reached |
|---|---:|
| castletemplate | 1 |
| commandbutton | 404 |
| commandset | 151 |
| fxlist | 250 |
| modifierlist | 199 |
| module | 840 |
| object | 329 |
| objectcreationlist | 93 |
| objectreskin | 37 |
| playertemplate | 1 |
| science | 14 |
| specialpower | 180 |
| upgrade | 182 |
| weapon | 274 |

Unresolved tokens in reference-like fields: 456 (most are keywords such as `PRIMARY` / `OR` or engine-defined upgrades; the list is in the ignored dump).

## Start (A)

- `side`: `Men`
- `playableside`: `Yes`
- `startingbuilding`: `MenFortress`
- `startmoney`: `0`
- `intrinsicsciences`: `SCIENCE_GOOD`
- `intrinsicsciencesmp`: `SCIENCE_MEN`
- `initialupgrades`: `Upgrade_MenFaction    Upgrade_AllFactionUpgrade`
- `spellbookmp`: `GondorSpellBook`
- `purchasesciencecommandsetmp`: `GondorSpellStoreCommandSet`
- `buildableheroesmp`: `CreateAHeroDummy GondorGandalf_mod GondorBoromir_mod GondorAragorn_mod GondorFaramir_mod GondorBeregond RohanPippin_mod GondorImrahil`
- `buildableringheroesmp`: `RingHeroDummy`
- `defaultplayeraitype`: `MenSkirmishAI`
- `startingunit0`: `MenPorter`
- `startingunit1`: `MenPorter`
- `startingunit2`: `AIEmitterPlaceholder`

## Spell book: store `GondorSpellStoreCommandSet` -> cast set `GondorSpellBookCommandSet` (book object `GondorSpellBook`)

| Buy slot | Purchase button | Science | Cost | Prerequisites | Cast slot | Cast button | SpecialPowers needing this science | Module (type / tag) | Refs | Status |
|---:|---|---|---:|---|---:|---|---|---|---|---|
| 1 | `Command_PurchaseSpellHornGondors` | `SCIENCE_HornMen` | 5 | SCIENCE_GOOD OR SCIENCE_MEN OR SCIENCE_ARNOR | 4 | `Command_SpellBookHornGondors` | `SpellBookRallyingCall`<br>`SpellBookRallyingCallAI` | `SpecialPowerModule` / `ModuleTag_RallyingCall` | AttributeModifier=SpellBookRallyingCallModifier | INVENTORIED; behaviour TODO |
| 2 | `Command_PurchaseSpellHealMod` | `SCIENCE_RebuildMen` | 5 | SCIENCE_GOOD OR SCIENCE_MEN OR SCIENCE_ARNOR | 3 | `Command_SpellBookHealMod` | `SpellBookHeal` | `PlayerHealSpecialPower` / `ModuleTag_Heal` | HealOCL=OCL_HealSpellHordeReplenishPing | INVENTORIED; behaviour TODO |
| 3 | `Command_PurchaseSpellBaumeisterGondors` | `SCIENCE_HealMen` | 5 | SCIENCE_GOOD OR SCIENCE_MEN OR SCIENCE_ARNOR | 5 | `Command_SpellBookBaumeisterGondors` | `SpellBookRebuild`<br>`SpellBookRebuildAI` | `PlayerHealSpecialPower` / `ModuleTag_Rebuild` | AttributeModifier=SpellBookGondorRepairLoneTowerFire | INVENTORIED; behaviour TODO |
| 4 | `Command_PurchaseSpellLehenAllies` | `SCIENCE_LehenAllies` | 10 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_HornMen | 9 | `Command_SpellBookLehenAllies` | `SpellBookLehenAllies` | `OCLSpecialPower` / `ModuleTag_LehenAllies` | OCL=OCL_SpawnLehenGondors | INVENTORIED; behaviour TODO |
| 5 | `Command_PurchaseSpellWaelderIthiliens` | `SCIENCE_ElvenWoodMen` | 9 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_HornMen OR SCIENCE_MEN SCIENCE_RebuildMen | 7 | `Command_SpellBookWaelderIthiliens` | `SpellBookWlderIthilliens` | `ElvenWoodSpecialPower` / `ModuleTag_WlderIthilliens` | ElvenGroveObject=ElvenGrove, ElvenWoodOCL=OCL_ElvenWoodSeed | INVENTORIED; behaviour TODO |
| 6 | `Command_PurchaseSpellGandalftheWhite` | `SCIENCE_EinsamerTurmMen` | 10 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_RebuildMen OR SCIENCE_MEN SCIENCE_HealMen | 6 | `Command_SpellBookGandalftheWhite` | `SpellBookGandalftheWhite` | `PlayerUpgradeSpecialPower` / `ModuleTag_SpellBookGandalfWhite` | UpgradeName=Upgrade_GandalfWhite | INVENTORIED; behaviour TODO |
| 7 | `Command_PurchaseSpellLoneTowerMen_Edain` | `SCIENCE_GandalftheWhite` | 9 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_HealMen | 8 | `Command_SpellBookLoneTower_Edain` | `SpellBookBeistandDolAmroths`<br>`SpellBookSpawnLoneTower_Edain` | `OCLSpecialPower` / `ModuleTag_SpawnLoneTowerGondorEdain` | OCL=OCL_GondorSpellBookSpawnLoneTower | INVENTORIED; behaviour TODO |
| 8 | `Command_PurchaseSpellDieGraueSchaar` | `SCIENCE_GraueSchaar` | 15 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_ElvenWoodMen OR SCIENCE_MEN SCIENCE_LehenAllies | 10 | `Command_SpellBookDieGraueSchaar` | `SpellBookDieGraueSchaar`<br>`SpellBookDieGraueSchaarMM` | `OCLSpecialPower` / `ModuleTag_SummonDieGraueScharGondor` | OCL=OCL_SpawndieGraueScharGondorEgg | INVENTORIED; behaviour TODO |
| 9 | `Command_PurchaseSpellCloudBreak` | `SCIENCE_CloudBreakMen` | 13 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_EinsamerTurmMen OR SCIENCE_MEN SCIENCE_ElvenWoodMen | 11 | `Command_SpellBookCloudBreak` | `SpellBookCloudBreak` | `CloudBreakSpecialPower` / `ModuleTag_CloudBreak` | SunbeamObject=CloudBreakSunbeam | INVENTORIED; behaviour TODO |
| 10 | `Command_PurchaseSpellAdler` | `SCIENCE_Adler` | 18 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_EinsamerTurmMen OR SCIENCE_MEN SCIENCE_GandalftheWhite | 12 | `Command_SpellBookAdler` | `SpellBookAdler`<br>`SpellBookGondorRitter` | `OCLSpecialPower` / `ModuleTag_Adler` | OCL=OCL_SpawnAdler | INVENTORIED; behaviour TODO |
| 11 | `Command_PurchaseSpellArmyoftheDead` | `SCIENCE_ArmyoftheDead` | 25 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_CloudBreakMen OR SCIENCE_MEN SCIENCE_GraueSchaar | 13 | `Command_SpellBookArmyoftheDead` | `SpellBookArmyoftheDead` | `OCLSpecialPower` / `ModuleTag_SummonDead` | OCL=OCL_GondorArmyofTheDeadEgg | INVENTORIED; behaviour TODO |
| 12 | `Command_PurchaseSpellSummonRohanGondor` | `SCIENCE_RohanAntwortet` | 25 | SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_Adler OR SCIENCE_MEN SCIENCE_CloudBreakMen | 14 | `Command_SpellBookSummonRohanGondor` | `SpellBookSummonRohanMM`<br>`SpellBookSummonRohanGondor`<br>`SpellBookSummonRohanGondorAI` | `OCLSpecialPower` / `ModuleTag_SummonRohanGondor` | OCL=OCL_SpawnRohanGondorEgg, UpgradeName=Upgrade_RohanAllies | INVENTORIED; behaviour TODO |

Powers that need one of these sciences but have no button in the cast set (AI, MM-map or hidden variants): `SpellBookRallyingCallAI`, `SpellBookRebuildAI`, `SpellBookBeistandDolAmroths`, `SpellBookDieGraueSchaarMM`, `SpellBookGondorRitter`, `SpellBookSummonRohanMM`, `SpellBookSummonRohanGondorAI`. Each needs its own reachability answer.
