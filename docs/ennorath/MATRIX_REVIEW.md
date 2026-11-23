# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 69 flagged, 0 of them red

By rule: L1 66, L4 3


## Changes since ff106d0f: 2 buttons moved, 1 turned red

- Angmar `spell` `SpellBookSummonStoneGiants`: PASS -> FAIL — the HUD refused the scene's spot and every spot of the ring search (RotWK's location test, RW 0x82DFB7) (the spot's cell status 0) [AngmarDa
- Mordor `MordorGrond` `(aura) ModuleTag_DreadVisageAura`: NO-AURA -> PASS
