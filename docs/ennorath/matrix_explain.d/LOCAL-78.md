# Matrix rows that passed on main and are red on Local 78's matrix (PR 587, batch 2026-11-16b; reasons from the agent's report, checked by Claude)

- `WildMarauderSwords Command_AngmarTrollChargeAttack` (FactionWild): not the test: main bdda1229 (Local 75) ported ActionManager::canDoSpecialPowerAtObject, RW 0x82DAE9 refuses SPECIAL_ATTRIBUTEMOD_CANCELDISGUISE (0x85) at an object, so the click sends nothing; whether Ennorath's Marauder charge fires on a clicked enemy in the original is the owner's open question.
- `DwarvenNarin Command_ramridercaptain` (FactionDwarves): NO-FIRE in the full 11-faction run only, PASS alone (FACTB_ONLY): the scene's free spot for a ground target after main aa2bb0c9 (PORT-438) lands on one the power refuses; a scene fault from main, follow-up task.
- `GoblinBillTomBertHorde Command_SpecialAbilitySpawnKochtopf` (FactionWild): as DwarvenNarin above (PORT-438 free spot), follow-up task.
- `HaradSuladanNew Command_SpecialAbilitySuladanSummonGuard` (FactionEvilmen): as DwarvenNarin above (PORT-438 free spot), follow-up task.
