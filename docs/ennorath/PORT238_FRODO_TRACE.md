# Port 238: Frodo / dropped ring, partial trace

This is a bounded trace of literal Ennorath 1.9.8.3 declarations, not a
complete effective-object or ability/visual audit. References are relative to
`data/ini`; archived INI MD5 `c53b6dea885cab044f80e783565f41f3`.

## Literal links

* `object/goodfaction/units/men/frodo.ini:1085` declares `RohanFrodo_mod`;
  its direct `CommandSet` at 1911 is `RohanFrodoModRingCommandSet_Galadriel`.
  That set's slot 3 is `Command_OneRing` (`includes/commandset.inc:9978`;
  button definition `includes/commandbutton.inc:20613`).
* The object declares `StealthUpdate` at 2242 and `OneRingPenaltyUpdate` at
  2262. Its penalty module names `EyeOfSauronFromRing`, declared in
  `object/evilfaction/units/mordor/eyeofsauron.ini:146`.
* `GaladrielRohanFrodo_mod` is a child at 2369. It declares a `GrantUpgradeCreate`
  with `Upgrade_RingHero` and `Upgrade_FortressRingHero`, the same command set,
  and `CreateObjectDie` with `OCL_TheOneRing`. `RohanFrodoRing` is a separate
  object at 2385, with its own stealth/penalty and death OCL declarations;
  inheritance and the circumstances that spawn these forms remain unresolved.
* `objectcreationlist.ini:9566` defines `OCL_TheOneRing` as creating
  `TheDroppedRing`; `crate.ini:1039` declares it with `ONE_RING`, an
  `AttachUpdate` setting parent status `HOLDING_THE_RING`, and a death OCL.
  This is a definition graph, not proof that every route creates, drops or
  transfers the ring in a running game.

## Bounded behavior status

| Piece | Status and evidence | Remaining limits |
| --- | --- | --- |
| ONE_RING command, single eligible stealth module | Ported in this branch for availability and dispatch: RW `0x942733`, `0x940435`, `0x7761DD`; `PORT-238*` passed in isolated runner | Multi-module selection S-7773; full suite and in-game test pending; visuals S-1041 |
| Dropped ring attachment | Existing port implements the scan and parent status (`AttachUpdate.cpp`); RW `0x89515C` selects and records parent, `0x8953BB` follows or kills the object after parent death. Isolated `PORT-124*`: 3 cases, 68 assertions passed | S-1361 covers horde member selection / anchor details and Eva name validation; PORT-124 uses a synthetic name-only filter, not the Ennorath macro; no Ennorath end-to-end claim |
| Ring-wear penalty and Eye lifecycle | Existing port `OneRingPenaltyUpdate.cpp`; RW `0x89D647` schedules spawn/movement based on ring state and `0x89D41B` takes off the ring, clears CAN_STEALTH and kills the Eye | `modgap1 OneRingPenaltyUpdate*` passed in an isolated runner: 2 cases, 36 assertions. Lookup S-7773 and client effects S-1041 remain; not a full visual audit |
| `TimeFrozenFromPenalty` | Existing closed S-5604 / PORT-30 verdict: parsed data `+0x1C`, no RW reader found by that prior audit; donor's data view has the slot and constructor zero | We re-read RW `0x89D647`, `0x89D41B`, `0x89D4D2` and donor data/constructor, **not** a new complete whole-image proof. Isolated `PORT-30*`: 3 cases, 1014 assertions passed; tests alone do not prove no reader |
| CreateObjectDie / OCL | PORT-237 (merged in main, commit `2a54572d`) closed S-980: `CreateObjectDie::onDie` in `ExtraDieModules.cpp` passes the dead object and killer to `ObjectCreationList::create`, as RW `0x888F04 -> 0x5F0126`; `PORT-237 CreateObjectDie...` covers that forwarding | This closes the shared object-pair context gap, not the full Frodo/OCL lifecycle or actual game route. PORT-237 and this branch have not been re-run on the post-merge full binary |
| Two names in `GrantUpgradeCreate` | Ported bounded parser/create behavior: RW field table `0xC70158` calls `0x42EE5E` / `0x42E757` (one unquoted token), `0x8BD15E` checks the construction mask; the synthetic `PORT-238*` test exercises two tokens, default and explicit mask, and build completion. Literal definitions of both upgrades are OBJECT (`upgrade.ini:181,198`) | Test explicitly calls build completion; ChildObject load and actual creation/build lifecycle remain to be checked in context |
| Other commands, upgrades and model states | Partially enumerated below, not completely audited | Enumerate remaining modules, upgrades, creation paths and model states; do not treat naming or a parser entry as proof of behavior |

## Galadriel command set: bounded follow-up

`RohanFrodoModRingCommandSet_Galadriel` (`includes/commandset.inc:9975`)
explicitly lists toggle weapon, ONE_RING, Sam leadership, Phial, Mithril,
capture, attack-move, stop and stance commands. Parent and child each name the
set; this does **not** establish the child's effective inherited modules or its
actual creation path.

* **Phial — partial ported / gap:** `Command_SpecialAbilityPhialOfGaladrielGeschenke`
  names `SpecialAbilityPhialOfGaladriel`; reload is 30000 ms (`specialpower.ini:79`).
  The parent declares the `Upgrade_GeschenkeLoriens` unpause, paused starter,
  2000 ms unpack, terror radius 150, `LoseStealthOnTrigger=Yes`, and
  `DisableWhenWearingTheRing=Yes` (`frodo.ini:2290–2315`). The BFME2 donor's
  `ModelConditionSpecialAbilityUpdate.cpp` supplies a class/data/filter lead,
  not proof of RW behavior. In RW, registration at `0x659624` references factory
  `0x64D349`, which calls constructor `0x8943CB`; the constructor installs
  vtable `0xC63F38`, slot `+0x44` = `0x89463A`. The latter scans enemies and
  invokes emotion 6 for terror (`0x894701`, `0x89472D`). Base condition check
  `0x8515E4` tests the disable field and the stealth module's worn flag; the port
  has the corresponding `conditionsAllow()` and `emotionPulse()` paths. A
  synthetic `PORT-238 Phial*` test checks `allowsSpecialPowerUse()` before,
  during and after a ring toggle, then directly initiates the power and checks
  an in-range enemy's terror request/source while ally and far enemy get none.
  With the fixture flag changed to `No`, the worn-ring assertion fails as
  expected (34/35 assertions); that is fixture sensitivity, not a code-fix
  control. It does not test a button press while worn, upgrade/unpause,
  `LoseStealthOnTrigger`, visuals, actual data loading or child inheritance.
* **Shared off-map filter — stale on current main:** RW `0x89469D` includes
  filter `0xC0F374`, slot `+4` = `0x660E93`, comparing caster and target
  `Object +0x458` bit 3. This was missing on our previous base `3f0105c8`.
  Port 252 / issue #600, commit `cc18ee79`, added the comparison to shared
  `emotionPulse()` in `SpecialAbilityModules.cpp`, with an off-map regression
  in `test_port_252.cpp`. It also serves `SummonReplacementSpecialAbilityUpdate`.
  The newly merged code and regression have not yet been run on this branch;
  no in-game effect has been measured. The earlier S-856 wording that the bit
  is never set is stale for current port. Issue #645 is a different modifier path.
* **Mithril — bounded self/scan behavior:** the parent declares an initially inactive
  `AttributeModifierAuraUpdate` gated by `Upgrade_Level_10`, range 1,
  `AllowSelf=Yes`, filter `NONE +RohanFrodo_mod` (`frodo.ini:2180–2187`);
  the separate `RohanFrodoRing` directly declares the same aura fields
  (`:3479–3487`). `FrodoMithrilhemd` is LEADERSHIP, `ARMOR 100%`, duration
  3000 ms (`includes/attributemodifier.inc:2708`); its command is a
  NONPRESSABLE indicator for `SpecialAbilityFakeLeadership2`. RW `0x89F42D`
  runs the filtered partition query at `0x89F677`, then if `AllowSelf` is set
  appends the source at `0x89F688–0x89F692`; the apply step `0x89F114` does
  not repeat that named ObjectFilter test. The port's `HeroModules.cpp`
  implements this separation. Synthetic `PORT-238 Frodo-style Mithril*`
  directly grants a stand-in level upgrade to two separately named source
  templates: the source receives the modifier even when its own template name
  is not the scan filter; a nearby matching same-player target receives it,
  while a different-name target and an enemy matching target do not. **60/60**
  assertions, isolated suite **45/45 cases, 3021/3021 assertions** on matched
  `3f0105c8` core/headers with separately compiled PORT-286 parser; changing
  synthetic `AllowSelf` to No fails two self checks (**58/60**). This is fixture
  sensitivity, not a pre-fix control. It does not test effective child
  inheritance, actual level-grant, armor combat, expiry/refresh, FX or game.
* **Sam leadership — bounded synthetic activation/targeting:** the parent aura
  declares `StartsActive = Yes`, `TriggeredBy = Upgrade_Level_1`,
  `FrodoForSamBonus`, refresh 2500 ms, range 200 and filter
  `NONE +RohanSam_mod ALLIES` (`frodo.ini:2166–2174`); its modifier declares
  `DAMAGE_MULT 150%` and 3000 ms (`includes/attributemodifier.inc:4400–4405`).
  The command is a NONPRESSABLE indicator. The BFME2 donor
  `AttributeModifierAuraUpdateCtor.cpp` leads to RW ctor `0x89ED5C`, whose
  `StartsActive` branch `0x89EDAD–0x89EDBB` applies the upgrade without waiting
  for a named trigger; port `HeroModules.cpp` mirrors it. A synthetic case,
  without granting the fixture level-one upgrade, checks the first pulse and a
  same-player in-range matching Sam modifier with damage multiplier 1.5; the
  self, far Sam, enemy Sam and nearby other name get no modifier. A second
  pulse occurs after `13 + source ID % 5` frames (2500 ms rounded to frames,
  RW `0x89F6BD`), and the matching target again has multiplier 1.5. Expanded
  test **40/40**, isolated suite **46/46 cases, 3061/3061 assertions** on
  matching `3f0105c8` core/headers plus the separately compiled PORT-286 parser.
  Changing fixture `StartsActive` to No in the earlier version failed **27/29**
  with early REQUIRE; changing only fixture RefreshDelay to 2000 ms in the
  expanded version fails **39/40**. These are fixture sensitivity controls,
  not engine-fix controls. Distinct allied players, target equivalence/
  inheritance, actual damage, between-pulse expiry and visuals remain untested.
* **Weapon toggle and ordinary orders — not read against RW in this trace.**
  The presence of their buttons is not proof of effective weapons or controls.

The upstream `origin/main` `68383364` automated matrix reports Phial PASS after
52 frames with one emotion-6 observation, the two passive indicators as
NONPRESSABLE, and a weapon-set toggle. It is a different run/base from the older
local matrix's DISABLED Phial; neither proves real mod loading, ring-on/off,
visuals, balance or RW equivalence.

The earlier `PORT-124*`, `PORT-30*` and penalty results in this file predate the
merge of main `e32b67a3` and use a temporary isolated executable. An isolated
`PORT-238*` runner linked against matching merged `3f0105c8` core and headers
passed 15/15 cases, 403/403 assertions (Phial: 35/35); this is **not** the full
`openbfme_tests` binary or an in-game run. No game or mod files were committed.
