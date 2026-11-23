# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 69 flagged, 0 of them red

By rule: L1 66, L4 3


## Changes since 2ffb0928: 17 buttons moved, 1 turned red

- Imladris `LindonTower` `Command_PurchaseUpgradeLindonReinforcement`: DISABLED -> FAIL — DISABLED: disabled by model USER_8
- Isengard `spell` `SpellBookBndnissderzweiTime`: FAIL -> PASS
- Isengard `spell` `SpellBookCrebain`: FAIL -> PASS
- Isengard `spell` `SpellBookDevastation`: FAIL -> NO-RULE — cast, but no effect seen beyond the control run: no effect of DevastateSpecialPower is asserted;  (enemies lost 1941 with the cast, 1920 wit
- Isengard `spell` `SpellBookFreezingRain`: FAIL -> PASS
- Isengard `spell` `SpellBookFueltheFires`: FAIL -> PASS
- Isengard `spell` `SpellBookIsengardCapitans`: FAIL -> PASS
- Isengard `spell` `SpellBookIsengardMinenRegen`: FAIL -> PASS
- Isengard `spell` `SpellBookPalantirVision`: FAIL -> PASS
- Isengard `spell` `SpellBookSarumanVielfarbig`: FAIL -> PASS
- Isengard `spell` `SpellBookWarChantIsengard`: FAIL -> PASS
- Isengard `spell` `SpellBookWargBow`: FAIL -> PASS
- Isengard `spell` `SpellBookWildMenAllies`: FAIL -> PASS
- Men `GondorDenethor_forFortress` `Command_SpecialAbilityDenethorNotstandssituation`: GREY-BUG -> FAIL — DISABLED: disabled by model USER_9
- Men `GondorDenethor_forFortress` `Command_SpecialAbilityDenethorSehenderStein1`: GREY-BUG -> FAIL — DISABLED: disabled by model USER_9
- Men `GondorDenethor_forFortress` `Command_SpecialAbilityDenethorSteuererlass`: GREY-BUG -> FAIL — DISABLED: disabled by model USER_9
- Men `GondorDenethor_forFortress` `Command_SpecialAbilityDenethorVerzweiflungsakt`: GREY-BUG -> FAIL — DISABLED: disabled by model USER_9
