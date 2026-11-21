# PORT-238: WildSmaugNew ring forms (partial trace)

Sources: extracted Ennorath 1.9.8.3 INI archive MD5 `c53b6dea885cab044f80e783565f41f3`, RW 2.01 `game.dat` MD5 `daed3668006cd90f01c34e5a7da1901f`. Paths below are relative to `data/ini`. Literal declarations are not an effective-load, game, full ability or visual proof. This is distinct from `WildSmaug_mod_RingHero` in `wildsmaug.ini`.

## Entry, forms and death

`object/evilfaction/units/wild/wildsmaug_new.ini:2` defines `WildSmaugNew`. At `:376–390` its citadel contain takes a carrier marked `HOLDING_THE_RING`, destroys `TheDroppedRing` and lists two OBJECT ring upgrades. `:639–674` unpauses `SpecialAbilityGandalfGeprfte` on `Upgrade_RingHero`; the automatic command requires both ring upgrades and names `WildSmaugNewRingFly`. The replacement declares unpack 6000 ms, preparation 800 ms, pack 2000 ms; its timer-sync line is **commented**. Base lifetime is 120000 ms (`:401–405`); its ring drop lists `UpgradeRequired = Upgrade_RingHero Upgrade_FortressRingHero` (`:442–445`). The carrier-attachment macro explicitly excludes all three named SmaugNew forms (`_gamedata.inc:31`), not necessarily delivery via another carrier. Existing generic delivery/toggle tests do not prove this exact sequence, lifetime race or game load; S-7321, S-7446 and S-861 remain.

`WildSmaugNewRingFly` (`:809`) and `WildSmaugNewRing` (`:1693`) are separate Objects and explicitly name one another in `EquivalentTo`. Both declare unconditional `CreateObjectDie` with `OCL_TheOneRing` (`:1636–1638`, `:2434–2436`). Fly's giant-bird death and faded slow-death paths differ from ground's normal/faded slow deaths; death type, ring drop timing and both respawn/lifetime consequences are not proven. `Command_SmaugKriechen` requires a target position (`includes/commandbutton.inc:17538`), while `Command_SmaugFliegen` does not (`:17566`). Fly→ground names five synchronized powers including `SpecialAbilityWormtongueCorrodeAllegiance` (`:1549–1563`); ground→fly lists four and omits that fifth power (`:2374–2384`). Both require level 1 in their own `UnpauseSpecialPowerUpgrade`. Matrix at main `c47a4984` reports replacements in both directions; this does not verify target positioning, timers or visuals.

## Commands and modules

Fly's `WildSmaugNewCommandSet` (`includes/commandset.inc:15745`) has ground toggle, fireball, dragon curse, fireflight and incinerate; ground's `WildSmaugNewBodenCommandSet` (`:15759`) has flight toggle, NONPRESSABLE money info, dragon curse and fireflight. Standard stance/attack-move/stop are not individually audited.

| Item | Literal observation and limits |
| --- | --- |
| Fireball | Fly level-1 `WeaponFireSpecialAbilityUpdate` fires `SmaugFireball` (`wildsmaug_new.ini:1362–1388`); matrix observed a hit/meta-impact. Projectile, all damage, level gate and animation remain unverified. Ground also declares a fireball module but its named command set does not list the button. |
| Fireflight | Both forms declare level-6 `SpecialAbilityDrogothFireflight` with `SmaugFireflight` (`:1393–1422`, `:2232–2261`); matrix observed hits/meta-impact on both. Actual flight, weapon timings, damage and visuals are not independently tested. |
| Incinerate | Fly level-10 `SpecialAbilityDrogothIncinerate` fires `SmaugNewIncinerate` (`:1429–1453`); current matrix reports ORDER (red in full sweep, PASS alone), not a gameplay failure or success verdict. Ground also has the module but not the button in its named command set. |
| Dragon curse | Both forms use level-3 `SpecialAbilitySmaugDragonCurse` / `SmaugDragonCurseWeapon` (`:1330–1356`, `:2151–2176`). Matrix observed `SmaugAttachSpy` creation, not its attachment or complete curse lifecycle. S-1361 / S-920 / S-924 remain relevant. |
| Enemy fear / money | Both declare an active `SauronFear` enemy aura with radius 350 (`:1318–1327`, `:2140–2149`); ground declares an active `SmaugPassiveOutlawLeadership` aura radius 300 (`:2413–2420`) and money-info button. Matrix observed aura applications / NONPRESSABLE indicator. Filtering, modifiers, resources and visuals are not independently tested; S-856 remains. |
| Other modules | Fly declares level-7 `DominateEnemySpecialPower` for heroes (`:1492–1518`); ground declares level-5 `SpecialAbilityDominateEnemySmaug` with `PermanentlyConvert = Yes` and a different non-hero filter (`:2330–2356`). Neither appears directly in the named ring command sets. Generic temporary Will is tested in `PORT238_SAURON_TRACE.md`, but it does not prove either Smaug path, permanent conversion, or command reachability. |

## Same-owner treasure-trove pulse

Base and both ring forms declare repeating `FireWeaponUpdate` / `SmaugRingauswirkungWeapon` (`:392–399`, `:1641–1649`, `:2454–2462`). `includes/weapon.inc:44468–44484` names radius 15000 and `NONE +WildTreasureTrove +WildTreasureTrove_Player SAME_PLAYER`; the modifier `BoromirRingauswirkungKaserne` is category WEAPON, duration 5000 ms, and sets `USER_6` (`includes/attributemodifier.inc:8054–8058`). Generic RW pulse and modifier calls (`0x88F554`, `0x90EAF9 -> 0x68F1A8`) are traced in `PORT238_ISILDUR_TRACE.md`; upstream PORT-286 documents model-condition application and expiry in `docs/re/rotwk/PORT-286.md`. S-924 remains for forced weapon delivery inference.

Synthetic `PORT-238 Smaug-style ring pulse*` uses two differently named same-owner structures, a same-named structure owned by a **different allied player**, and an unrelated same-owner structure. After publishing targets, it observes `USER_6` on both own matching templates, not on the allied player's or unrelated structure, and sees a second pulse; 34/34 assertions on matching isolated `3f0105c8` core/headers with only upstream PORT-286's parser change separately compiled. Replacing `SAME_PLAYER` with `ALLIES` in the fixture fails 2/34 checks (fixture sensitivity, not a code-fix control). The test does not establish actual trove inheritance, command behavior, expiry after stopping, radius at real map scale, asset rendering or mod loading.

## Appearance

Fly declares `WUSmaugF_SKN` and jaw/tail weapon bones (`:823–832`); ground declares `JHDrag_SKN`, textures and idle smoke/fire particles (`:1702–1724`). No actual mesh, animation, shadows, particle or condition-priority rendering has been compared to Ennorath; shared LOCAL-95 work is not proof of visual parity.
