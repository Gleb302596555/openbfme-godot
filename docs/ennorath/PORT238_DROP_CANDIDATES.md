# Port 238: literal ring-drop links (unfinished)

Metadata-only lexical scan of Ennorath 1.9.8.3 INI archive (MD5 `c53b6dea885cab044f80e783565f41f3`). Paths are relative to `data/ini`. It scans explicit, uncommented object/child `CreationList = OCL_TheOneRing*` lines and OCL `ObjectNames` lines naming `TheDroppedRing`. It does not expand includes, calculate inheritance, examine alternative creation mechanisms, resolve nested or conditional modules, or establish the active load graph. A literal declaration is a candidate link, not proof that the object holds/drops a ring or that its behavior is ported.

## Direct object links to OCL_TheOneRing*

| Declaration | OCL | Source |
| --- | --- | --- |
| `AngmarWitchkingNewRing` | `OCL_TheOneRing` | `object/evilfaction/units/angmar/new/angwk.ini:2736` |
| `AngmarWitchking_MountedRing` | `OCL_TheOneRing` | `object/evilfaction/units/angmar/angmarwitchking.ini:3707` |
| `AngmarWitchking_mod` | `OCL_TheOneRing` | `object/evilfaction/units/angmar/angmarwitchking.ini:1332` |
| `AngmarWitchking_modRing` | `OCL_TheOneRing` | `object/evilfaction/units/angmar/angmarwitchking.ini:2104` |
| `DwarvenDurin` | `OCL_TheOneRing` | `object/goodfaction/units/dwarven/dwarvendurin.ini:891` |
| `ElvenGaladriel_RingHero` | `OCL_TheOneRing` | `object/goodfaction/units/elven/galadriel.ini:480` |
| `GaladrielRohanFrodo_mod` | `OCL_TheOneRing` | `object/goodfaction/units/men/frodo.ini:2378` |
| `GondorBoromir_OneRing` | `OCL_TheOneRing` | `object/goodfaction/units/gondor/gondorboromir.ini:2480` |
| `GondorBoromir_mod` | `OCL_TheOneRing` | `object/goodfaction/units/gondor/gondorboromir.ini:1247` |
| `GondorGandalf` | `OCL_TheOneRing` | `object/goodfaction/units/men/gandalf.ini:1576` |
| `GondorGandalfGeprfte` | `OCL_TheOneRing` | `object/goodfaction/units/gondor/gondorgandalfgeprfte.ini:1470` |
| `GondorGandalfGeprfte_RingHero` | `OCL_TheOneRing` | `object/goodfaction/units/gondor/gondorgandalfgeprfte.ini:1555` |
| `GondorGandalfVerfhrte` | `OCL_TheOneRing` | `object/goodfaction/units/gondor/gondorgandalfverfhrte.ini:1360` |
| `GondorIsildur_Mod` | `OCL_TheOneRing` | `object/goodfaction/units/arnor/arnorisildur.ini:927` |
| `GondorIsildur_OneRing` | `OCL_TheOneRing` | `object/goodfaction/units/arnor/arnorisildur.ini:1826` |
| `IsengardSarumanGesegneter` | `OCL_TheOneRing` | `object/evilfaction/units/isengard/saruman.ini:5139` |
| `IsengardSarumanKorrumpierte` | `OCL_TheOneRing` | `object/evilfaction/units/isengard/saruman.ini:2988` |
| `IsengardSarumanVerfluchter` | `OCL_TheOneRing` | `object/evilfaction/units/isengard/saruman.ini:4186` |
| `IsengardSaruman_Mod` | `OCL_TheOneRing` | `object/evilfaction/units/isengard/saruman.ini:1314` |
| `LothlorienGaladriel` | `OCL_TheOneRing` | `object/goodfaction/units/lothlorien/lothloriengaladriel.ini:983` |
| `LothlorienGaladrielNachRing` | `OCL_TheOneRing` | `object/goodfaction/units/lothlorien/lothloriengaladriel.ini:4352` |
| `MordorGorthaur` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/mordorsauron.ini:2332` |
| `MordorGreatSauron` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/greatsauron.ini:834` |
| `MordorNekromantReborn` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/necromantreborn.ini:764` |
| `MordorNekromantSauron` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/mordorsauron.ini:1056` |
| `MordorNekromantWotr` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/mordornecromantwotr.ini:755` |
| `MordorSauron_RingHero` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/sauron.ini:1063` |
| `MordorSauron_mod` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/mordorsauron.ini:7081` |
| `MordorSauron_mod_RingHero` | `OCL_TheOneRing` | `object/evilfaction/units/mordor/mordorsauron.ini:7252` |
| `NeutralGollum_RingHero` | `OCL_TheOneRing` | `object/neutral/neutralunits.ini:1426` |
| `RohanFrodoRing` | `OCL_TheOneRing` | `object/goodfaction/units/men/frodo.ini:3614` |
| `RohanRadagast` | `OCL_TheOneRing` | `object/goodfaction/units/rohan/rohanradagast.ini:980` |
| `RohanRadagast_RingHero` | `OCL_TheOneRing` | `object/goodfaction/units/rohan/rohanradagast.ini:1665` |
| `RohanRadagastaufdemAdler_RingHero` | `OCL_TheOneRing` | `object/goodfaction/units/rohan/rohanradagast.ini:3092` |
| `RohanTheoden_mod` | `OCL_TheOneRing` | `object/goodfaction/units/men/theoden.ini:1348` |
| `TheDroppedRing` | `OCL_TheOneRing` | `crate.ini:1132` |
| `Theoden_OneRing` | `OCL_TheOneRing` | `object/goodfaction/units/men/theoden.ini:2503` |
| `WildShelobRing` | `OCL_TheOneRing` | `object/evilfaction/units/wild/wildkankra.ini:2922` |
| `WildShelob_mod` | `OCL_TheOneRing` | `object/evilfaction/units/wild/wildkankra.ini:1542` |
| `WildSmaugNew` | `OCL_TheOneRing` | `object/evilfaction/units/wild/wildsmaug_new.ini:443` |
| `WildSmaugNewRing` | `OCL_TheOneRing` | `object/evilfaction/units/wild/wildsmaug_new.ini:2435` |
| `WildSmaugNewRingFly` | `OCL_TheOneRing` | `object/evilfaction/units/wild/wildsmaug_new.ini:1637` |
| `WildSmaug_mod_RingHero` | `OCL_TheOneRing` | `object/evilfaction/units/wild/wildsmaug.ini:1698` |

## OCL definitions naming TheDroppedRing

| OCL | Source |
| --- | --- |
| `OCL_GaladrielDieFix` | `includes/objectcreationlist.inc:3374` |
| `OCL_GandalfDieFix` | `includes/objectcreationlist.inc:3359` |
| `OCL_TheOneRing` | `objectcreationlist.ini:9568` |
| `OCL_TheOneRingCD` | `objectcreationlist.ini:9586` |
| `OCL_TheOneRingEggEdain` | `includes/objectcreationlist.inc:9110` |
| `RingUnitSpawn` | `includes/objectcreationlist.inc:6976` |

## Carrier selection caveat

`crate.ini:1103` sets `AttachUpdate::ObjectFilter` to `CARRYTHERING_OBJECTFILTER` (`_gamedata.inc:31`). This is a kind-based filter with a long explicit exclusion list, not a closed enumeration of carriers. A hero excluded from automatic attachment can still be a ring recipient via other mechanisms; inclusion in this table does not override the filter. Expansion of macros and other overrides must be verified on the actual loading path. For remaining attachment limits see S-1361; the shared death/OCL object-pair context S-980 was closed by PORT-237 (merged main `2a54572d`), not an end-to-end verdict on these drop routes. For the partial Frodo trace see `PORT238_FRODO_TRACE.md`.
