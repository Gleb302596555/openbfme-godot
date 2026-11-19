# Port 238: Boromir ring path, partial trace

Literal Ennorath 1.9.8.3 archive declarations, MD5
`c53b6dea885cab044f80e783565f41f3`; paths are relative to `data/ini`.
This is a bounded object/ability trace, **not** an effective-load, complete
ability, or visual audit. No game assets are committed.

## Receive and replace

`object/goodfaction/units/gondor/gondorboromir.ini:8` declares
`GondorBoromir_mod`. Its contain `:1269–1284` accepts the dropped-ring
passenger and, for a carrier marked `HOLDING_THE_RING`, gives one OBJECT
upgrade, `Upgrade_RingHero`, and destroys `TheDroppedRing`. At `:1197–1225`
that upgrade unpauses `SpecialAbilityGandalfGeprfte` and triggers
`DoCommandUpgrade`; button `Command_SpecialAbilityGlorfindelRingSwitch` names
that same power (`includes/commandbutton.inc:32716`). The toggle declares
`MountedTemplate = GondorBoromir_OneRing` and **five** timer names: Horn,
BladeMaster, BoromirErnennung, BoromirOsgiliath and BoromirOsgiliathRing.
There is also a different Ringbergabe weapon power (`:1164–1188`), not a
synonym for this replacement; its weapon and receiving behavior are not audited
here. The parent also has an upgrade-gated ring-drop `CreateObjectDie`
(`:1246–1249`).

The donor `ToggleMountedSpecialAbilityUpdate.cpp` shows the synchronized-timer
loop and replacement. RW `0x8B8E2E -> 0x696FD2` issues the upgrade command;
`0x8B1690 -> 0x8B140D` reaches replacement, and `0x8B12BF` iterates timer
names, resolves the old and new power interfaces, and calls `0x897368`
(`0x8B134D`). See also `PORT238_SAURON_TRACE.md` for the bounded common
replacement chain and S-861. The branch's queue fix concerns the earlier
contain delivery callback; it does not eliminate S-861 or prove all replacement
state is transferred.

A synthetic `PORT-238` test grants one ring upgrade by delivery, creates a
replacement via the automatic command and checks owner team, name, removal of
the old object, **two** selected ready frames and an unlisted timer. It runs
against matching `3f0105c8` core/headers: 34/34 assertions. Removing only the
shield timer name from the fixture made its copied-ready-frame assertion fail
(33/34): this checks fixture sensitivity, **not** a negative control for a code
fix. It does not test the real five timers, their pause/shared conditions,
experience/health transfer, FX or the live INI. The other two-upgrade
necromancer replacement fixture is documented separately.

## Ring form and powers

`GondorBoromir_OneRing` is a separate `Object` at `:1378`, not a ChildObject.
Its command set is `BoromirModCommandSet_OneRing` (`:1993`); it declares
`TransportSlotCount = TRANSPORTSLOTCOUNT_HERO`, default and HERO_MODE weapon
sets both naming `BoromirSwordRing`, and a separate RAMPAGE set (`:1950–1966`).
It has `LifetimeUpdate` 300000 ms and an unconditional `CreateObjectDie` for
`OCL_TheOneRing` (`:2479–2487`); its `RespawnUpdate` names the ordinary
`GondorBoromir_mod` (`:2077–2093`). The definitions do not prove expiry, ring
drop and respawn ordering in a live game.

| Item | Bounded status | What remains |
| --- | --- | --- |
| `Command_BoromirRingLeadership` | NONPRESSABLE indicator names `SpecialAbilityFakeLeadership2` (`commandbutton.inc:32680`); the literal ring form's leadership starter names `SpecialAbilityFakeLeadership`, while its level-1 aura names `BoromirRingLeadership`, radius 200 (`frodo` is not involved; `gondorboromir.ini:2300–2319`). Modifier declares ARMOR 50%, RESIST_FEAR and RESIST_TERROR 100%, duration 3000 (`includes/attributemodifier.inc:8084`) | Do not silently equate the two power names or infer a broken indicator from the mismatch: resolve includes/defaults, trigger and actual UI. Aura behavior, filtering, the remaining S-856 anti-category interaction and rendering still need checking (S-633 itself is closed). |
| `Command_SpecialAbilityBoromirHornRing` | Ring form's `ModelConditionSpecialAbilityUpdate` and starter name `SpecialAbilityHornOfGondor02` (`:2327–2351`), reload 120000 (`specialpower.ini:68`). `BoromirGondorHornAntiCategory` literally has duration 5000 and no modifier line (`attributemodifier.ini:747`); starter's `AntiCategory` line is commented. | Upstream matrix says NO-EFFECT. The shared modifier mechanism is the explicit scope of open Port 278 / issue #645: do not patch it here or infer its resolution from the literal list. Its animation/emotion and effective load also remain unverified. |
| `Command_SpecialAbilityBoromirSchildRing` | Level-1 unpause, paused starter and `HeroModeSpecialAbilityUpdate` name `SpecialAbilityAragornBladeMaster`; buff `BoromirTapfererWiderstand` and effect duration 30000 (`:2365–2387`), reload 120000 (`specialpower.ini:2807`). The buff declares INVULNERABLE using a macro and FX (`includes/attributemodifier.inc:2152`). Common RW HeroMode factory/ctor/vtable/slots and port paths are traced in `PORT238_SAURON_TRACE.md`; matrix only observes application of this modifier. | Actual invulnerability value/macro expansion, damage behavior, upgrade, expiry, model/particles, and ring-form HUD are not tested by the Sauron synthetic Fire Form test. |
| `Command_SpecialAbilityBoromirVerteidigungderweienStadt` | `OCLSpecialPower` uses `SpecialAbilityBoromirOsgiliathRing`, `CREATE_AT_LOCATION` and `OCL_BoromirOsgiliathEgg` (`:2430–2448`). Its literal OCL makes `BoromirOsgiliathEgg` (`includes/objectcreationlist.inc:7263`); egg lifetime is 0 and its slow-death FINAL OCL follows after 2000 ms (`object/projectile.ini:6619–6638`); that second OCL declares three `GondorBoromirGuardHorde` creates (`objectcreationlist.inc:7271–7305`). Donor `OCLSpecialPower.cpp` is a lead; RW registration `0x65B4AA` -> factory `0x6518F0` -> ctor `0x8C72E9` installs interface vtable `0xC73890`, slot `+0x30` = `0x8C75D8`; location switch case 3 copies the position at `0x8C776B` and calls `0x5F00CA` via `0x8C7690`; port `SpecialPowerModules.cpp` implements that location path. | Matrix observed only the egg; the delayed three-horde lifecycle, offsets, ownership, filters and actual visuals remain untested. Shared OCL placement restrictions S-7426 and power effects S-920 may apply; do not claim a full summon. |
| `SpecialAbilityKingsFavor` | The ring-form object literally declares a LevelGrant power (`:2398–2422`) though it is not in the named ring command set. | Effective reachability, acceptance-filter expansion, XP gain and RW/port behavior are not read in this trace. |
| Ring-form passive / combat / death modules | The object also declares other modules including Elven Gift healing, `BoromirRingHealth` upgrade, dual weapon, death and respawn (`:2077–2257`, `:2469–2487`). | Their upgrade provenance, values, transfer, actual appearance and behavior are not fully audited. Standard stance, capture, attack-move and stop controls are listed in `PORT238_COMMAND_CANDIDATES.md`; they are not individually verified here. |

The `origin/main` `68383364` automated matrix has PASS for the shield modifier
and egg creation, NONPRESSABLE for leadership, and NO-EFFECT for horn. These
observations are not a player run, do not establish effective Ennorath loading
or visual parity, and do not cover the delayed summon.

Appearance declarations name default model `gobororing`, sword-bone ring
particles, and `WEAPONSET_HERO_MODE` shield particles (`:1389–1402`); other
draws and animations exist in the object. No rendering, assets, bone selection,
condition priority or particle behavior has been checked; S-090/S-095 remain.
