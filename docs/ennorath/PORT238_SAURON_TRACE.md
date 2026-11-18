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

## WotR candidate route (literal declarations only)

`mordornecromantwotr.ini:676..756` declares `MordorNekromantWotr` with
ring-triggered unpause and `DoCommandUpgrade`, a toggle whose
`MountedTemplate` is `MordorGreatSauronWotr`, a ring contain granting the
same two upgrades, an all-triggers `ONE_RING` condition and a ring drop OCL
requiring both upgrades. This is a separately declared object, not a tested
alias of `MordorNekromantReborn`. The Great Sauron WotR child
(`greatsauron.ini:879..935`) declares its own weapon, respawn and death/body
modules and removes nine `ModuleTag_SauronLevel2..10` modules. Those removed
modules are parent `LevelUpUpgrade` entries triggered by respawn-level
upgrades (`:775..826`); the separate experience level entries for both names
still exist in the literal experience file. Do not infer their effective
inheritance, rank restoration, ring delivery, death/respawn behavior or
appearance from the Reborn synthetic test; WotR's route and command/visual
inventory remain to be audited and tested independently.

## Heal

| Bounded piece | RW and port | Test / remaining work |
| --- | --- | --- |
| Trigger dispatch | RW `0x8D235D -> 0x8D222A`: at ability trigger, resolve tag, use object's position for OBJECTPOS and call the target special-power interface's vslot `+0x30`. `ActivateModuleSpecialPower::setModulesActive` implements that branch. | `PORT-238 ring hero activated module...` uses a synthetic power and the same kind of tag link; directly initiates the update, not the button or level-up path. Tag lookup timing differs under S-1222. |
| Heal effect | RW `0x8CC574 -> 0x8CC4B4 -> 0x8CC37B`: heal at location, filtered by kind/relationship, percentage of maximum. `PlayerHealSpecialPower` implements it. | The synthetic test sees the wounded owner and friendly hero increase from 40 to 90, while an enemy hero, non-hero and distant ally do not. The test uses `IgnoreFacingCheck`, no FX, and simplified objects; it is not an in-game parity verdict. |
| Unlock and targetless HUD dispatch | RW `0x8B9676` unpauses the matching power and, unless obey-recharge is set, sets ready frame to now. The targetless SPECIAL_POWER branch emits `MSG_DO_SPECIAL_POWER` (RW `0x94156B`), handled by `SpellCommands`. | A separate synthetic HUD test manually assigns selection and gives an OBJECT upgrade, checks the paused button is not enabled, unpause and enabled state, emitted message, self-heal, `NotReady` and a ready frame 1300 logic frames after triggering (260 seconds at 5 frames/second). This does **not** prove the real level-5 upgrade is awarded, nor real mod assets or FX. |
| Level-five declaration and grant | `includes/experiencelevels.inc:19783..19866` names both `MordorGreatSauron` and `MordorGreatSauronWotr`; its ranks 1–5 require 1, 550, 800, 1400 and 2000 XP, and rank 5 names `Upgrade_Level_5` (OBJECT in `includes/upgrade.inc:84`). RW `0x79D8EF -> 0x79D141` checks thresholds; `0x79DA9D -> 0x8215C2` queues grants, `0x821368` applies the upgrade. | A separate synthetic chain with those thresholds and an equivalent OBJECT rank-5 upgrade checks 1999 XP gives rank 4 and remains paused, while 2000 XP yields rank 5 and unpauses. XP is set directly; modifiers, FX, rank 1–4 upgrades, spawning/transformation and real data loading are omitted. It is not a verdict on how Great Sauron gets XP or appears in-game. |
| Matrix heal result | The older audit had a NO-EFFECT entry. At merged main `e32b67a3`, `docs/ennorath/MATRIX.tsv` instead records `PASS`: firing after 38 frames with `PlayerHealSpecialPower` reporting 5000 health restored (S-920 noted). The button is absent from that revision's `MATRIX_GAPS.tsv`. | This is a pre-existing automated sweep result, not a run in this branch or in-game confirmation. Loaded-mod level award, effects, lifecycle and visual parity remain unverified; the branch's level and cooldown assertions were synthetic. |
| Ring passive, will, fire form and Fires of Doom | Literal buttons also occur in `SauronCommandSet`. The same upstream matrix marks passive `PASS` as NONPRESSABLE (not a press); Will `PASS` after 58 frames with target ownership changed; Fire Form `PASS` after 51 frames with an attribute modifier applied; Fires of Doom `PASS` after 64 frames with an OCL object made. These are recorded effect probes, not RW parity or in-game checks. | RW behavior, the real entry/lifecycle and appearance for each remain to be audited. All three active probes note S-920. |

A temporary isolated runner against the rebuilt core and test-fixture headers
at merged main `958dc704` passed `PORT-238*` 14/14 cases and 357/357 assertions,
including the later Fire Form and Will cases. It is not the full executable;
its build was stopped after the core linked and before the full executable
linked. Main was subsequently merged again at `3f0105c8`, which changed
ring-adjacent modules; the new full build and all post-`3f0105c8` test runs
remain pending. No in-game tests have been performed.

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
some AI/client paths, are S-1222. PORT-258 on main (`87d644cb`, included
in `3f0105c8`) also added the same-on/off-map area filter and resolves scanned
IDs before applying effects; its `test_port_258.cpp` has a synthetic area-filter
case. That upstream change has not yet been tested in this branch's full build.

A detail worth testing, not silently "fixing": the configured filter names
`ENEMIES NEUTRAL`, but the area scan also has an upstream relationship filter
for the caster's ENEMIES (RW filter `0xC11DC0`, port's partition chain). Thus
`NEUTRAL` in the later object filter alone is not proof a neutral-relation
object reaches the helper; actual team relationships and the direct-target
branch still matter. The literal default `data/ini/default/object.ini:210`
contains an inheritable `TemporarilyDefectUpdate` with 30000 ms duration, but
its effective presence on each target has **not** been established by this
scan. The existing synthetic `hero2` cases separately exercise temporary
direct defection and permanent area horde conversion. A new `PORT-238` test
combines a synthetic 120-radius, position-targeted temporary area domination
with a 30000 ms explicitly installed defect update: an enemy infantry unit
changes owner, retains its original team and returns after expiry; an ally,
neutral troop, enemy HERO, DOZER, STRUCTURE and distant troop stay unchanged.
The test independently confirms that the later synthetic object filter accepts
the neutral troop even though the earlier area relationship gate excludes it.
In the `958dc704` isolated core runner this case passed 47/47 assertions; a
fixture changed only to `PermanentlyConvert = Yes` failed 5/47, including
expiry and return. This tests sensitivity, not a code-fix negative control.
It omits the named Watcher exclusions, horde/member behavior, the HUD, upgrades,
FX and real effective inheritance, and directly initiates the ability with
`IgnoreFacingCheck`. The upstream matrix `PASS` observes an ownership change
on one horde, not its eventual restoration or all exclusions. Current-core,
mod and in-game behavior remain open.

## Fire Form: bounded logic and visual declarations

`SauronCommandSet` slot 5 names `Command_SpecialAbilitySauronFireForm`
(`commandset.ini:158`); its SPECIAL_POWER button links to
`SpecialAbilitySauronFlame` (`commandbutton.ini:21590`), whose enum is
`SPECIAL_HERO_MODE` and reload is 400000 ms (`specialpower.ini:3340`). The
literal level-8 entry for both Great Sauron names requires 4500 XP and grants
`Upgrade_Level_8` (`includes/experiencelevels.inc:19920..19926`). The object
uses that upgrade to unpause the power and declares a paused starter and a
`HeroModeSpecialAbilityUpdate` (`greatsauron.ini:631..677`). The 60-second
`SauronFlameBuff` declares armor 50%, speed 125%, and an FX
(`attributemodifier.ini:5405`). Actual upgrade delivery and modifier stacking
have not been tested for these declarations.

DONOR `HeroModeSpecialAbilityUpdate.cpp` describes the trigger, unpack and
expiry; its field table is in `ModuleDataBuildFieldParseChained.cpp`. RW class
registration `0x65977F` -> runtime factory `0x64D6A0` -> ctor `0x895805`
installs main vtable `0xC64680`: slot `+0x58` starts unpacking at `0x8958BD`,
slot `+0x44` triggers `0x8959B5`, slot `+0x48` expires `0x8958F8`. Unpacking
selects a HERO_MODE weapon set only if the template has one, and schedules an
expiry. Trigger calls the base, idles the AI if configured, adds the named
modifier, and sets `HERO` for the configured duration if not already set
(the default `UseUSERModelcondition` byte is false, RW data ctor `0x895907`).
The expiry clears the weapon-set flag. The corresponding port methods are in
`SpecialAbilityModules.cpp`; the existing `test_hero_expiry.cpp` exercises a
synthetic weapon-set expiry, not this Ennorath object or its rendering.

The object also declares `FireWeaponUpdate` with `HeroModeTrigger = Yes` and
two non-one-shot nuggets. RW registration `0x659012` -> factory `0x64CBF7` ->
ctor `0x88F731` installs update interface vtable `0xC629BC`, slot `+0` =
`0x88F554`. Its constructor resolves weapons and schedules the initial
`FireDelay` relative to object creation (`0x88F464`); its update first requires
the HERO bit and then checks each record's fire frame and weapon readiness.
The port implements this in `SpellEffectModules.cpp`. The named initial weapon
has damage 300, radius 150 and 80000 ms delay between shots; the other has
damage 5, radius 50 and 3000 ms delay (`weapon.ini:29500..29546`). Both have
`OneShot = No`: the "initial" name is **not** proof of a one-time shot per
activation. Forced-position-fire and unported weapon-nugget details remain
under S-924. A new `PORT-238` synthetic test directly initiates HeroMode and
checks that the HERO bit and buff appear, two synthetic pulses damage a nearby
enemy for 305 and a farther enemy for 300 while leaving an ally and an out-of-
range enemy unharmed, the shorter-delay pulse repeats, and pulses stop after
HERO and the buff expire. It uses a 5-fps fixture, synthetic armor and simplified
weapons (not the actual `NOT_SIMILAR`, structure scalar, height or FX fields),
`IgnoreFacingCheck` and no real HUD, level unlock, recast or assets. The existing
automated mod sweep only observed the modifier (matrix PASS), not either pulse.

In an **isolated pre-merge** runner built with source/fixture headers from
`8b9c4ed`, the 13 `PORT-238*` cases passed (310/310 assertions), including
this new case (34/34). As a test-sensitivity control, changing only the
synthetic `HeroModeTrigger` to `No` failed 3/34 assertions: shots and damage
occurred before activation and shots continued after expiry. This is not a
negative control of a code fix, nor a full-suite or in-game result. The same
13 cases also passed in a separate isolated runner against the freshly linked
core after main `958dc704` was merged (310/310); main `3f0105c8` was merged
later, so it does not validate the current core. The current full executable
and actual mod testing remain pending.

For appearance, the literal draw declaration names default model `MUBsauron_EA`
and a `HERO` condition model `musauronfire` with several particle bones
(`greatsauron.ini:14..35`). These declarations and the HERO logic bit are not
proof of the selected drawable state, actual model assets, particles, bone
attachment, animation or visual parity. Generic state/particle limitations
include S-090 and S-095; no actual render or in-game visual check was performed.
The `MordorGreatSauronWotr` child is declared at `:879`; inheritance and any
other overrides must be resolved before treating this as its effective look.

## Fires of Doom: bounded creation chain

The literal command set uses `Command_FiresOfDoom` at slot 6; the button is a
position-targeted SPECIAL_POWER (`commandbutton.ini:1568`), linked to
`FiresOfDoomSpecialPower` (360000 ms reload, cursor radius 200, cast range 500;
`specialpower.ini:2538`). The Great Sauron object unpauses it on
`Upgrade_Level_10` and uses `WeaponFireSpecialAbilityUpdate` with
`SauronFiresOfDoomWeapon` (`greatsauron.ini:490..518`). The literal level 10
entry requires 6000 XP and awards that upgrade for both Sauron names
(`includes/experiencelevels.inc:19960`). This is not a test of XP delivery,
button range enforcement, targeting or WotR inheritance.

The BFME2 donor constructor (`WeaponFireSpecialAbilityUpdateCtor.cpp`) is the
reference; RW registration `0x659715` -> factory `0x64D7F4` -> constructor
`0x895D98` installs main vtable `0xC64818`, slot `+0x44` = `0x8961F7`.
The constructor resolves and loads the named weapon; the trigger checks its
readiness, uses the object or position target, fires it and starts recharge.
The port implements these paths in `SpecialAbilityModules.cpp`; exact facing,
AI, client, and full timing remain outside this bounded comparison.

The declared weapon (`weapon.ini:14942`) contains a `WeaponOCLNugget` naming
`OCL_FiresOfDoom`, whose literal definition has one create-object entry with
count 1 and another with count 2 and `SpreadFormation` / `SPAWN_AROUND`
(`objectcreationlist.ini:9635..9652`). This is a declaration of intended
entries, not proof of three successfully placed objects. RW nugget ctor
`0x90F8B8` installs vtable `0xC7B410`, whose position slot `+0x18` is
`0x90F887 -> 0x5F00CA` and object slot `+0x14` is `0x90F858 -> 0x5F0126`;
`WeaponDelivery.cpp` implements the corresponding position and object
branches. Existing S-530 covers remaining OCL limitations; S-924 covers the
later forced-position weapon-fire path.

`TheFiresOfDoom` (`object/system/system.ini:4797..4879`) declares a 10000 ms
lifetime and two one-shot `FireWeaponUpdate` nuggets: immediate
`RainOfFirePhaseInitialWeapon` and `RainOfFirePhaseOneWeapon` after 2500 ms.
The former declares a Lua fear event, the latter multiple delayed damage and
MetaImpact nuggets (`weapon.ini:20869..`); S-1220, S-321, S-1600 and S-1790
remain relevant shared limitations. The initial declaration uses `EXMeteor`
and meteor particle bones; this is not a verified rendered model, animation,
particle, sound, exact hit, placement, or lifetime result. The earlier matrix
PASS observed one `TheFiresOfDoom` object made, not all declared effects or
number/positions of spawned objects. No PORT-238 end-to-end test for this
chain has been run.
