# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 69 flagged, 0 of them red

By rule: L1 66, L4 3


## Changes since 0beb2b14: 5 buttons moved, 2 turned red

- Men `MenPorterRepair` `Command_RepairAI`: PASS -> NO-SEND — SpecialRepairStructure: the click on its target sent nothing (target GondorBarracks at 650,3152,103, owner Player_1 [pixel 512,384, collisio
- Wild `MordorBalrogAI` `Command_SpecialAbilityBalrogBreathAI`: ORDER -> NO-EFFECT — SpecialAbilityBalrogBreath (target GondorFighterHorde) fired but WeaponFireSpecialAbilityUpdate SpecialWeapon MordorBalrogBreath: no result 
- Rohan `RohanTheoden_mod` `Command_SpecialAbilityKingsFavorEdainTheo`: ORDER -> PASS
- Rohan `Theoden_OneRing` `Command_SpecialAbilityKingsFavorEdainTheoRing`: ORDER -> PASS
- Wild `WildPorterSpecAI` `Command_RepairAI`: PASS -> ORDER
