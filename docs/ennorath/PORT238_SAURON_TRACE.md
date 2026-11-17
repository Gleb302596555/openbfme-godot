# Port 238: Great Sauron's ring heal (bounded trace)

The links below are literal Ennorath 1.9.8.3 declarations, not an exhaustive
Sauron form, power, upgrade or visual audit (`data/ini`, archive MD5
`c53b6dea885cab044f80e783565f41f3`). `MordorGreatSauron` is declared in
`object/evilfaction/units/mordor/greatsauron.ini:3` and sets `SauronCommandSet`
at line 324. `commandset.ini:153` places `Command_SpecialAbilitySauronRingHeal`
in slot 4; `commandbutton.ini:21604` links it to
`SpecialAbilitySauronHealingNew` (`specialpower.ini:3402`).

The object declares a level-5 `UnpauseSpecialPowerUpgrade`, a paused
`SpecialPowerModule`, and `ActivateModuleSpecialPower` with
`TriggerSpecialPower = ModuleTag_AragornAthelasHeal OBJECTPOS`; the target tag
is a `PlayerHealSpecialPower` configured for `HERO`, 50% and radius 1
(`greatsauron.ini:681..710`). No full inheritance or dynamic replacement
resolution is implied by these literal links.

## An upstream creation candidate (not resolved end to end)

`necromantreborn.ini:732..753` declares `CitadelSlaughterHordeContain`
with `Upgrade_RingHero Upgrade_FortressRingHero`; both upgrades are OBJECT
(`upgrade.ini:181,198`), and the local `ModelConditionUpgrade` requires both
for `ONE_RING`. The callback at RW `0x8840FC`, particularly the vector loop at
`0x8841C2..0x88420D`, grants each according to its type. The port's
`GarrisonContain.cpp:deliverRing` does likewise. A synthetic direct contain
entry confirms both OBJECT upgrades, ring destruction and the `ONE_RING`
condition; an independent object with only the first upgrade does not acquire
that condition until the second is given. This checks a model-condition bit,
**not** a drawable, mesh, animation, real filter, or HUD delivery path.

The same file (`:686..712`) declares an unpause and `DoCommandUpgrade` for
`Upgrade_RingHero` and a `ToggleMountedSpecialAbilityUpdate` whose mounted
object is `MordorGreatSauron`; the named command is a SPECIAL_POWER button
(`includes/commandbutton.inc:20434`), whose template is `SPECIAL_TOGGLE_MOUNTED`
with 1000 ms reload (`includes/specialpower.inc:4212`). RW `0x8B8E2E` presses
the button via `0x696FD2`, and `0x8B1690` invokes replacement `0x8B140D`.
A synthetic direct contain-entry test with this sequence, the two OBJECT
upgrades and the stated unpack/pack timings observes a new friendly synthetic
mounted object and deletion of the original. An isolated negative-control
fixture with its `DoCommandUpgrade` line removed fails at the missing
replacement (1 failed test, 21/22 assertions), confirming this test depends on
that module. It does not remove or test the queue fix, nor prove real HUD
delivery, the actual mod transformation, visuals, transferred state or timing
in the loaded game. Shared replacement limitations remain S-861.

## Heal

| Bounded piece | RW and port | Test / remaining work |
| --- | --- | --- |
| Trigger dispatch | RW `0x8D235D -> 0x8D222A`: at ability trigger, resolve tag, use object's position for OBJECTPOS and call the target special-power interface's vslot `+0x30`. `ActivateModuleSpecialPower::setModulesActive` implements that branch. | `PORT-238 ring hero activated module...` uses a synthetic power and the same kind of tag link; directly initiates the update, not the button or level-up path. Tag lookup timing differs under S-1222. |
| Heal effect | RW `0x8CC574 -> 0x8CC4B4 -> 0x8CC37B`: heal at location, filtered by kind/relationship, percentage of maximum. `PlayerHealSpecialPower` implements it. | The synthetic test sees the wounded owner and friendly hero increase from 40 to 90, while an enemy hero, non-hero and distant ally do not. The test uses `IgnoreFacingCheck`, no FX, and simplified objects; it is not an in-game parity verdict. |
| Unlock and targetless HUD dispatch | RW `0x8B9676` unpauses the matching power and, unless obey-recharge is set, sets ready frame to now. The targetless SPECIAL_POWER branch emits `MSG_DO_SPECIAL_POWER` (RW `0x94156B`), handled by `SpellCommands`. | A separate synthetic HUD test manually assigns selection and gives an OBJECT upgrade, checks the paused button is not enabled, unpause and enabled state, emitted message, self-heal, `NotReady` and a ready frame 1300 logic frames after triggering (260 seconds at 5 frames/second). This does **not** prove the real level-5 upgrade is awarded, nor real mod assets or FX. |
| Level-five declaration and grant | `includes/experiencelevels.inc:19783..19866` names both `MordorGreatSauron` and `MordorGreatSauronWotr`; its ranks 1–5 require 1, 550, 800, 1400 and 2000 XP, and rank 5 names `Upgrade_Level_5` (OBJECT in `includes/upgrade.inc:84`). RW `0x79D8EF -> 0x79D141` checks thresholds; `0x79DA9D -> 0x8215C2` queues grants, `0x821368` applies the upgrade. | A separate synthetic chain with those thresholds and an equivalent OBJECT rank-5 upgrade checks 1999 XP gives rank 4 and remains paused, while 2000 XP yields rank 5 and unpauses. XP is set directly; modifiers, FX, rank 1–4 upgrades, spawning/transformation and real data loading are omitted. It is not a verdict on how Great Sauron gets XP or appears in-game. |
| Matrix heal result | The older audit had a NO-EFFECT entry. At merged main `e32b67a3`, `docs/ennorath/MATRIX.tsv` instead records `PASS`: firing after 38 frames with `PlayerHealSpecialPower` reporting 5000 health restored (S-920 noted). The button is absent from that revision's `MATRIX_GAPS.tsv`. | This is a pre-existing automated sweep result, not a run in this branch or in-game confirmation. Loaded-mod level award, effects, lifecycle and visual parity remain unverified; the branch's level and cooldown assertions were synthetic. |
| Ring passive, will, fire form and Fires of Doom | Literal buttons also occur in `SauronCommandSet`. The same upstream matrix marks passive `PASS` as NONPRESSABLE (not a press); Will `PASS` after 58 frames with target ownership changed; Fire Form `PASS` after 51 frames with an attribute modifier applied; Fires of Doom `PASS` after 64 frames with an OCL object made. These are recorded effect probes, not RW parity or in-game checks. | RW behavior, the real entry/lifecycle and appearance for each remain to be audited. All three active probes note S-920. |

Before merging main e32b67a3, the temporary isolated runner reported `PORT-238*`
12/12 cases and 276/276 assertions. A post-merge rebuild, the full executable
run and mod in-game tests remain pending.

## Will of the Ring: bounded area-domination trace

`Command_SauronWilloftheRing` is a `SPECIAL_POWER` with
`NEED_TARGET_POS CONTEXTMODE_COMMAND` (`commandbutton.ini:21576..21588`).
Its template `SpecialAbilityDominateEnemySauronRing` has enum
`SPECIAL_DOMINATE_ENEMY`, reload 280000 ms and radius cursor 120
(`includes/specialpower.inc:5548..5552`). `greatsauron.ini:598..626`
declares level-3 unpause, a paused power, and `DominateEnemySpecialPower` with
range 275, radius 120, exclusions including HERO and STRUCTURE,
`PermanentlyConvert = No`, and two FX. The matching level-3 declaration
names 800 XP and `Upgrade_Level_3` (`includes/experiencelevels.inc:19820`).
These are literal declarations, not an effective-inheritance or level-up run.

DONOR `DominateEnemySpecialPower.cpp` describes the location scan and the
per-target helper. In RotWK, the class-name registration at `0x65A94F`
uses runtime factory `0x652C4B` -> ctor `0x8D0DE6`, which installs vtable
`0xC762C8`; its slot `+0x44` is `0x8D103D`. That routine uses the radius and
location for an area partition scan when no object target is present, then
calls `0x8D0F6C` with the area flag on each result. The helper checks the
configured filter, construction, horde-producer and airborne cases before
`Object::defect` `0x699368`; the latter, on the non-permanent path, looks up
`TemporarilyDefectUpdate` and calls `0x8D0BF3` if found. The corresponding port
is `DominateEnemySpecialPower::triggerAbilityEffect` / `dominate` and
`Object::defect`. Shared defection gaps, including non-horde containment and
some AI/client paths, are S-1222.

A detail worth testing, not silently "fixing": the configured filter names
`ENEMIES NEUTRAL`, but the area scan also has an upstream relationship filter
for the caster's ENEMIES (RW filter `0xC11DC0`, port's partition chain). Thus
`NEUTRAL` in the later object filter alone is not proof a neutral-relation
object reaches the helper; actual team relationships and the direct-target
branch still matter. The literal default `data/ini/default/object.ini:210`
contains an inheritable `TemporarilyDefectUpdate` with 30000 ms duration, but
its effective presence on each target has **not** been established by this
scan. The existing synthetic `hero2` cases separately exercise temporary
direct defection and permanent area horde conversion; neither is a full test
of Sauron's temporary area ability. The pre-existing matrix `PASS` observes an
ownership change on one horde, not its eventual restoration or all exclusions.
The effects, HUD target path, real target inheritance and in-game behavior remain
open.
