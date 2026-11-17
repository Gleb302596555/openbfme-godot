# Port 238: candidate inventory (unfinished)

Metadata-only lexical inventory of Ennorath 1.9.8.3 INI archive (MD5 `c53b6dea885cab044f80e783565f41f3`). Paths are relative to `data/ini/object`; line numbers count LF. This is **not** the required complete inventory and assigns **no** ported/absence verdict. It ignores commented lines, records literal declarations, and does not expand includes, resolve overrides/inheritance or validate whether definitions are loaded. Ring-bearing names without the listed suffixes (for example `AngmarWitchkingNewRing`) and differently named Frodo forms are excluded. No ability, upgrade or appearance has been fully audited merely by appearing here.

## Explicit names matching ring-form suffixes

A dash means no direct declaration found, not the absence of inherited command sets. A command set value within a nested module may be a conditional replacement; resolve its context before use.

| Object | Parent | Direct command-set references | Source |
| --- | --- | --- | --- |
| `AngmarWitchking_mod_RingHero` | `AngmarWitchking_mod` | — | `evilfaction/units/angmar/angmarwitchking.ini:3769` |
| `ElvenGaladriel_RingHero` | `ElvenGaladriel` | — | `goodfaction/units/elven/galadriel.ini:467` |
| `GondorArvedui_OneRing` | — | `ArnorArveduiModCommandSet_OneRing` | `goodfaction/units/arnor/arnorarvedui.ini:948` |
| `GondorBoromir_OneRing` | — | `BoromirModCommandSet_OneRing` | `goodfaction/units/gondor/gondorboromir.ini:1378` |
| `GondorGandalfGeprfte_RingHero` | `GondorGandalfGeprfte` | — | `goodfaction/units/gondor/gondorgandalfgeprfte.ini:1540` |
| `GondorGandalfVerfhrte_RingHero` | `GondorGandalfVerfhrte` | — | `goodfaction/units/gondor/gondorgandalfverfhrte.ini:1426` |
| `GondorIsildur_OneRing` | — | `GondorIsildurCommandSet_OneRing` | `goodfaction/units/arnor/arnorisildur.ini:989` |
| `ImladrisElrond_RingHero` | — | `ElrondMODRingCommandSet`, `ElrondMODRingCommandSetUp` | `goodfaction/units/imladris/imladriselrond.ini:2023` |
| `IsengardSarumanRing_RingHero` | `IsengardSarumanRing` | — | `evilfaction/units/isengard/saruman.ini:3128` |
| `LothlorienGaladriel_RingHero` | `LothlorienGaladriel` | — | `goodfaction/units/lothlorien/lothloriengaladriel.ini:3528` |
| `LothlorienGaladrielRingablehnen` | — | `LothlorienGaladrielCommandSet_Valar` | `goodfaction/units/lothlorien/lothloriengaladriel.ini:1860` |
| `LothlorienGaladrielRingannehmen` | — | `LothlorienGaladrielCommandSet_Ring` | `goodfaction/units/lothlorien/lothloriengaladriel.ini:1091` |
| `MordorSauron_mod_RingHero` | `MordorSauron_mod` | — | `evilfaction/units/mordor/mordorsauron.ini:7183` |
| `MordorSauron_RingHero` | `MordorSauron` | — | `evilfaction/units/mordor/sauron.ini:519` |
| `NeutralGollum_RingHero` | `NeutralGollum` | — | `neutral/neutralunits.ini:1421` |
| `RohanRadagast_RingHero` | `RohanRadagast` | `RohanRadagastRingCommandSet` | `goodfaction/units/rohan/rohanradagast.ini:1245` |
| `RohanRadagastaufdemAdler_RingHero` | — | `RohanRadagastaufAdlerRingCommandSet` | `goodfaction/units/rohan/rohanradagast.ini:2468` |
| `Theoden_OneRing` | — | `RohanTheodenringCommandSetEnd` | `goodfaction/units/men/theoden.ini:1653` |
| `WildSmaug_mod_RingHero` | `WildSmaug_mod` | — | `evilfaction/units/wild/wildsmaug.ini:1686` |

## Direct `UpgradeForRingEntry` declarations

These are lexical hits, not all effective recipients; includes, default values, multiple definitions and ChildObject inheritance are unresolved. A same-file match does not establish that the object is an available hero or that its ring path works.

| Enclosing declaration | Source |
| --- | --- |
| `AngmarWitchkingNew` | `evilfaction/units/angmar/new/angwk.ini:1000` |
| `AngmarWitchking_mod` | `evilfaction/units/angmar/angmarwitchking.ini:1315` |
| `ArnorArveduiNew` | `goodfaction/units/arnor/new/arnorarveduinew.ini:1546` |
| `ArnorCampKeep` | `goodfaction/structures/arnor/arnorcampsandcastles.ini:259` |
| `DwarfMausoleum` | `goodfaction/structures/dwarven/mausoleum.ini:412` |
| `EreborThrone` | `civilian/ereborbuildings.ini:5609` |
| `EvilmenFortressCitadel` | `evilfaction/structures/evilmen/evilmenfortress.ini:700` |
| `GondorArvedui` | `goodfaction/units/arnor/arnorarvedui.ini:772` |
| `GondorBoromir_mod` | `goodfaction/units/gondor/gondorboromir.ini:1280` |
| `GondorCampKeep` | `goodfaction/structures/men/campsandcastles.ini:263` |
| `GondorGandalf` | `goodfaction/units/men/gandalf.ini:1597` |
| `GondorGandalfWhite_mod` | `goodfaction/units/gondor/gondorgandalf.ini:2789` |
| `GondorGandalf_mod` | `goodfaction/units/gondor/gondorgandalf.ini:1496` |
| `GondorIsildur_Mod` | `goodfaction/units/arnor/arnorisildur.ini:906` |
| `HaradAlatar_mod` | `evilfaction/units/harad/evilalatar.ini:1025` |
| `ImladrisElrond` | `goodfaction/units/imladris/imladriselrond.ini:1783` |
| `IsengardSarumanKorrumpierte` | `evilfaction/units/isengard/saruman.ini:2972` |
| `IsengardSaruman_Mod` | `evilfaction/units/isengard/saruman.ini:1298` |
| `LothlorienGaladriel` | `goodfaction/units/lothlorien/lothloriengaladriel.ini:961` |
| `LothlorienGaladrielNachRing` | `goodfaction/units/lothlorien/lothloriengaladriel.ini:4337` |
| `MordorGorthaur` | `evilfaction/units/mordor/mordorsauron.ini:2311` |
| `MordorNekromantReborn` | `evilfaction/units/mordor/necromantreborn.ini:744` |
| `MordorNekromantSauron` | `evilfaction/units/mordor/mordorsauron.ini:1036` |
| `MordorNekromantWotr` | `evilfaction/units/mordor/mordornecromantwotr.ini:735` |
| `RohanRadagast` | `goodfaction/units/rohan/rohanradagast.ini:965` |
| `RohanTheoden_mod` | `goodfaction/units/men/theoden.ini:1381` |
| `WOR_ArnorCastleBaseKeep` | `goodfaction/structures/arnor/arnorcampsandcastles.ini:902` |
| `WOR_EreborThrone` | `civilian/ereborbuildings.ini:6096` |
| `WildShelob_mod` | `evilfaction/units/wild/wildkankra.ini:1527` |
| `WildSmaugNew` | `evilfaction/units/wild/wildsmaug_new.ini:388` |

## Active textual includes of `FortressRingFunc.inc`

These are candidates for the shared ring-drop functionality; inclusion alone does not prove whether the containing object is instantiated, receives a ring or has an effective module after overrides.

| Enclosing declaration | Source |
| --- | --- |
| `ArnorCampKeep` | `goodfaction/structures/arnor/arnorcampsandcastles.ini:299` |
| `ArnorFortressCitadel` | `goodfaction/structures/arnor/arnorfortress.ini:1275` |
| `DwarvenFortressCitadel` | `goodfaction/structures/dwarven/dwarvenfortress.ini:1194` |
| `DwarvenThroneCitadel` | `goodfaction/structures/dwarven/throne.ini:350` |
| `EvilmenFortressCitadel` | `evilfaction/structures/evilmen/evilmenfortress.ini:746` |
| `GondorCampKeep` | `goodfaction/structures/men/campsandcastles.ini:303` |
| `RohanCitadel` | `goodfaction/structures/rohan/rohanfortress.ini:610` |
| `WOR_ArnorCastleBaseKeep` | `goodfaction/structures/arnor/arnorcampsandcastles.ini:907` |
| `WOR_EreborThrone` | `civilian/ereborbuildings.ini:6101` |

## What remains

Resolve the actual load graph, effective object inheritance and overrides, shared includes, create/die/transform links, explicit ring objects and all command sets / powers / upgrades / visual conditions. Trace each through parser, RW and port, with per-item ported/stale/proven-no-reader/left-with-stop verdicts. Shared StealthUpdate selection remains S-7445 and generic admission remains S-7321; neither is a blanket verdict for these candidates.
