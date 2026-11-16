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

| Bounded piece | RW and port | Test / remaining work |
| --- | --- | --- |
| Trigger dispatch | RW `0x8D235D -> 0x8D222A`: at ability trigger, resolve tag, use object's position for OBJECTPOS and call the target special-power interface's vslot `+0x30`. `ActivateModuleSpecialPower::setModulesActive` implements that branch. | `PORT-238 ring hero activated module...` uses a synthetic power and the same kind of tag link; directly initiates the update, not the button or level-up path. Tag lookup timing differs under S-1222. |
| Heal effect | RW `0x8CC574 -> 0x8CC4B4 -> 0x8CC37B`: heal at location, filtered by kind/relationship, percentage of maximum. `PlayerHealSpecialPower` implements it. | The synthetic test sees the wounded owner and friendly hero increase from 40 to 90, while an enemy hero, non-hero and distant ally do not. The test uses `IgnoreFacingCheck`, no FX, and simplified objects; it is not an in-game parity verdict. |
| Unlock and targetless HUD dispatch | RW `0x8B9676` unpauses the matching power and, unless obey-recharge is set, sets ready frame to now. The targetless SPECIAL_POWER branch emits `MSG_DO_SPECIAL_POWER` (RW `0x94156B`), handled by `SpellCommands`. | A separate synthetic HUD test manually assigns selection and gives an OBJECT upgrade, checks the paused button is not enabled, unpause and enabled state, emitted message, self-heal, `NotReady` and a ready frame 1300 logic frames after triggering (260 seconds at 5 frames/second). This does **not** prove the real level-5 upgrade is awarded, nor real mod assets or FX. |
| `MATRIX_GAPS.tsv` NO-EFFECT row | This synthetic chain produces a measurable health effect; the row by itself does not demonstrate that the port cannot heal. | The actual mod UI path, level-5 upgrade award, FX and full loaded object remain unverified; the cooldown assertion is synthetic. Do not mark the original row globally stale on the basis of this isolated test. |
| Ring passive, will, fire form and Fires of Doom | Literal buttons also occur in `SauronCommandSet`. | Not audited; no verdict inferred from their names. |

The temporary isolated runner reported `PORT-238*` 9/9 cases and 204/204
assertions. The required full executable and mod in-game tests remain pending.
