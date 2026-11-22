# PORT-238: legacy Angmar Witch-king ring variants (partial inventory)

This is not `AngmarWitchkingNew` (`PORT238_WITCHKING_NEW_TRACE.md`). Literal declarations come from the extracted Ennorath 1.9.8.3 archive (MD5 `c53b6dea885cab044f80e783565f41f3`). Effective mount/inheritance, actual availability, Ennorath 1.9.8.4/1.9.8.5 equivalence, all powers and visuals remain unverified. Paths below are relative to `data/ini`.

## Declared forms and hand-over

`object/evilfaction/units/angmar/angmarwitchking.ini` defines `AngmarWitchking_mod` (`:22`), `AngmarWitchking_modRing` child (`:1422`), `AngmarWitchking_Mounted` child (`:2202`) and `AngmarWitchking_MountedRing` child of the mounted form (`:2990`). The separately named `AngmarWitchking_mod_RingHero` child (`:3769`) is **not** the same object as `AngmarWitchking_modRing`; its effective inheritance and creation route are unresolved. The AI map inventory names the base `AngmarWitchking_mod` as a creation candidate, not proof of this sequence (`PORT238_AI_RING_SCRIPTS.md`).

Base ring entry grants `Upgrade_RingHero Upgrade_FortressRingHero`, but the auto-command triggers on `Upgrade_RingHero` alone and toggles to `AngmarWitchking_modRing` (`:1269–1318`); the mounted base also declares a ring toggle to the **foot** ring form (`:2941–2965`). The ring foot/mounted variants declare toggles in both directions (`:2121–2144`, `:3674–3697`) with seven named timer-sync powers. The Carn Dûm child variants (`:3773–3961`) have their own directed targets. Do not assume ring delivery while mounted selects the mounted-ring variant, or that those literal declarations resolve unchanged after loading. Shared S-7321, S-7446 and S-861 remain; the bounded legacy-style synthetic test below does not exercise effective inheritance or in-game delivery.

The base declares a death ring OCL gated by both upgrades (`:1326–1334`); the ring foot and ring mounted declarations each have `CreateObjectDie OCL_TheOneRing` with no literal requirement (`:2103–2106`, `:3706–3709`). Both declare respawn as base (`:2156`, `:3721`). Exact tag replacement, duplicate prevention, death/respawn behavior and assets remain untested. The two ring draws declare `RandomTexture = ku_witchkingring.tga 0 ku_witchking.tga` and respectively `kuanghk_skn` / `kuhk_hrs` (`:1424–1430`, `:2992–2997`); none of this proves loaded models, textures, animation or correct display.

## Command sets and declared parent modules

The ring forms name `AngmarWitchKingmodCommandSet_OneRing` and toggle-weapon `AngmarWitchKingmodChangeCommandSet_OneRing` (`:2163–2175`, `:3729–3739`; `includes/commandset.inc:10714–10742`). The first has stance, weapon-set switch, mount, a NONPRESSABLE ring indicator, Screech and Curse; the second has weapon-set switch, Schattenklinge, Grausamer Ansturm, Halo and Verdorbene Magie. Button-to-power names are taken from `includes/commandbutton.inc`, not inferred from text labels. Parent declarations below are leads, not proof that all inherited modules or conditions are effective in either child:

| Power | Literal parent evidence | Remaining |
| --- | --- | --- |
| `SpecialAbilityScreech` | Level-3 unpause and `SpecialAbilityUpdate`, duplicate `UnpackTime` rows 3000 then 2000 (`angmarwitchking.ini:1129–1151`) | Effective parser result, terror/emotion effects, timing and visuals |
| `SpecialAbilityCurseEnemy` | Level-8 unpause and `CurseSpecialPower`, 100%, range 200 (`:1157–1185`) | Target eligibility, recharge and FX; new Witch-king results are not this form's proof |
| `SpecialAbilityAngmarWitchkingSchattenklinge` | Level-5 HeroMode, `HKFeuerSchwertBonusSpell`, 30000 ms (`:968–987`) | Duration, stats, actual model/weapon change |
| `SpecialAbilityGrausamWitchking` | Two named triggers with `RequiresAllTriggers`, weapon `GrausamerAnsturmWeapon` (`:994–1017`) | Ownership/scope of both upgrades, targeting and weapon effects |
| `SpecialAbilityWordOfPower` | Level 10, weapon `HaloAngmars` (`:1026–1050`) | Area, nested effects, actual damage and presentation |
| `SpecialAbilityVerdorbeneMagieAngmars` | `OCLSpecialPower` creates `OCL_MorgothsDmonSummoning` (`:1189–1198`) | OCL, spawn lifecycle, ownership, controls and effects |
| Passive / mount / weapon toggle | Parent dread aura and its own level-1 status, ring child aura override (`:1055–1087`, `:2111–2119`); mount and two command-set swapper declarations above | Real recipients, timer continuity, command availability, selection and visuals |

The main `eaadf3c31` matrix has observations for **new** Witch-king names; those must not be attributed to these legacy object names. This table is an inventory to drive subsequent tests, not a ported verdict. Shared S-856, S-920, S-924 and client/FX limits apply where relevant.

## Synthetic delivery characterization

`PORT-238 legacy Witch-king-style foot and mounted ring delivery chooses foot form`
uses separate synthetic infantry/cavalry sources and a synthetic foot destination;
it deliberately does not load the child objects or test cavalry admission. Each
source has both OBJECT grants, an unpaused starter, a one-upgrade auto-command
and the seven literal synchronized power names. Across six subcases (two source
forms × direct fortress-only, direct ring-only, or direct contain delivery),
the second upgrade alone leaves the source in place; the first alone and
two-upgrade delivery choose the synthetic foot destination. The delivered ring
is destroyed, ownership/team persists, two present named timers (`Screech`,
`CurseEnemy`) copy and an unlisted timer does not. The other five names are
declared as templates but do not have synthetic modules; their runtime copying
is not proven. Selection, pathfinding, real container admission, subsequent
mounting, inheritance/overrides, actual powers, death and assets are not covered.

On the consistent isolated core/headers `3f0105c8` plus the separately built
upstream PORT-286 parser, this test passed **238/238 assertions**; all
`PORT-238*` passed **36/36 cases, 2422/2422 assertions**. Changing only the
synthetic auto-trigger to the fortress upgrade makes the test fail (214/220
assertions): fixture sensitivity control, not a pre-fix engine test. Full
executable after main `1472d0359` is still rebuilding.
