# Port 139 / issue #341 — RotWK cursor analysis

## Conclusion

For the reported plain-selection case (a locally controlled unit selected, the pointer over an allied object that is not a usable container), RotWK 2.01 intentionally displays cursor index 5, `Move` (`SCCMove` in Ennorath's `data/ini/mouse.ini`). The allied object's cursor is not chosen by an `ALLIES` special case: the decisive test in the cursor routine is whether the hovered object is controlled by the local player. A non-local target takes the normal Move fallback. The issue's described green Move cursor therefore matches RW for this case; changing it to Arrow, Select, or GenericInvalid would diverge from the retail executable.

No engine change or PORT-139 test was made. The issue does, however, expose a separate adjacent difference: for a locally controlled selectable target, RW can choose Select (13), while the current port's `CommandTranslator::hint` has no corresponding hovered-target branch. That is not a fix for the reported allied-object behavior and is left for the owner to scope separately.

## Evidence from RotWK (`workspace/RotWK/game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`)

All addresses below were read with `python tools/mod/rw.py workspace/RotWK/game.dat dis ...`.

### `InGameUI::createCommandHint`, RW 0x6A3B77–0x6A4096

- Entry at 0x6A3B77; the recorder guard at 0x6A3B8F–0x6A3B97 returns when the recorder state is 1.
- 0x6A3B9D–0x6A3BB2 resolves the hovered drawable from the UI's mouseover object ID. For incoming hint types 0xA7/0xC0, 0x6A3BCD–0x6A3BFA checks the hovered object's shroud status for the local player and rewrites the hint to 0xBD when that status is 4.
- 0x6A3C0C–0x6A3C70 determines whether the pointer is over a UI window. 0x6A3C70–0x6A3C96 obtains the hovered Object and calls `CanSelectDrawable(draw, false)` at RW 0x83BCB1. If there is exactly one selected drawable, 0x6A3C9A–0x6A3CCD obtains its Object as the source for the structure fallback.
- The mouse-mode dispatch at 0x6A3CD0–0x6A3CE6 separates normal mode (0, 0x6A3E2C), placement mode (1, 0x6A3DE4), and GUI-command mode (2, 0x6A3CEC); other modes reach 0x6A4088 without changing the cursor.
- Placement mode, 0x6A3DE4–0x6A3E27: a pointer over a UI window selects Arrow (2); hint types 0xBD/0xBE/0x432 select Build (10), and 0xB1/0xCA select InvalidBuild (11); other hint types do not select a new cursor.
- GUI-command mode, 0x6A3CEC–0x6A3D78: a UI-window hit uses Arrow; otherwise the pending command is examined. The non-context command's cursor/invalid-cursor and radius-cursor cases are distinct from the default context-command ladder. The port's source comments map these command checks; this path is not the issue's plain-selection case.
- Normal mode first returns Arrow (2) for a UI-window hit or when the first selected object is not locally controlled (0x6A3E2C–0x6A3E4B). For move / attack-move hints, 0x6A3E5F–0x6A3F26 gathers selected object IDs and asks the AI order manager whether the pointer closes an order loop; the true branch selects cursor 55 at 0x6A3F15–0x6A3F1F (Patrol).
- Move hint 0xBD, the relevant jump-table case (table at RW 0x6A4099):
  - 0x6A3F63–0x6A3F87 handles a non-selectable hovered drawable. If the one selected object is locally controlled (`isLocallyControlled`, RW 0x68B749) and its template KindOf word at +0x108 has bit 0x80 (STRUCTURE), it selects GenericInvalid (12). Otherwise execution falls to the ordinary cursor choice.
  - 0x6A3F8E–0x6A3FA6 handles a selectable hovered drawable. RW 0x68B749 tests whether that hovered object is locally controlled; only then does the template flag test at `[template + 0x10F] & 0x10` decide whether to fall through or select cursor 13 (Select at 0x6A3FFA). A target owned by an allied AI is not locally controlled, so it falls through; there is no relationship-to-ALLIES branch here.
  - 0x6A3FA8–0x6A3FE3 is the radar-window exception. On ordinary map terrain, RW 0x69ACA0's radar-window equality check is false and the routine pushes cursor index 5 at 0x6A3FE1. The cursor-index table at RW 0xD9E108 maps 5 to Move; Ennorath's Mouse.ini declares `MouseCursor Move` with `Image = SCCMove` and `Texture = SCCMove`.
- The adjacent attack-move hint path (0x6A3FE8–0x6A4003) selects Select (13) for a locally controlled selectable target and otherwise index 6. The remaining jump-table cases select their fixed cursor indices (0x6A4005–0x6A407D); 0x6A407F is the Arrow (2) exit. The selected cursor is applied through RW 0x69BD53 at 0x6A4081–0x6A4083.

### What the gates test

- `CanSelectDrawable(draw, false)`, RW 0x83BCB1: it checks the drawable/object, object and template flags, and related selection gates. The local-player equality check is conditional on the second argument; with this call's `false` argument, the ownership gate at 0x83BDC7–0x83BDD0 is skipped. Thus “selectable” and “locally controlled” are separate tests.
- `isLocallyControlled`, RW 0x68B749: loads the local player from the player list, obtains the object's controlling player via RW 0x68B678 (which follows the object owner pointer through RW 0x79FD6F), then returns whether the pointers match. This is ownership, not the relationship enum `ALLIES`.
- Action selection happens before the hint routine. RW 0x82CBD5 is the ActionManager `canEnterObject` path. If the selected unit can actually enter the hovered object's contain, the command translator can choose an Enter action instead of the generic move hint. Otherwise allied targets do not satisfy the enemy attack branch and the generic location fallback supplies move hint 0xBD. The Enterable-container exception must not be collapsed into the plain allied-target case.

## Port comparison

- `engine/src/GameClient/MessageStream/CommandXlat.cpp`, `CommandTranslator::evaluateContextCommand` (around lines 491–522), checks the existing contain/enter action before the enemy attack branch; the final no-more-specific-action location hint around lines 680–690 is Move. This agrees with the RW distinction between a valid Enter action and the generic Move hint.
- `CommandTranslator::hint` around lines 311–319 ports the RW 0x6A3F63–0x6A3F87 selected-structure / no-selectable-target Invalid cursor case.
- It does not implement RW 0x6A3F8E's local-selectable-hovered-target Select cursor case. That affects a locally controlled selectable target, not the reported non-local allied target. Do not change allied cursor behavior to compensate for this separate gap.

## Donor and data

- DONOR BFME2: `Code/GameEngine/Source/GameClient/InGameUICreateCommandHint.cpp`, RVA 0x002A3273, case 0xb3. Its readable implementation corroborates the overall branch shape, but none of the expected behavior above is inferred solely from it; the target facts are from the RW addresses listed here.
- Ennorath data: `data/ini/mouse.ini` in `ennorath-ini-1.9.8.2.zip`, `MouseCursor Move` uses `SCCMove` for both Image and Texture. This confirms what cursor index 5 looks like in the mod, not how RW chooses it.

## Test and scope decision

No PORT-139 test was added: a test that expects the reported plain allied-object case to differ from Move would encode behavior contrary to RW 0x6A3F8E–0x6A3FE3. A container-acceptance test would be a different action path (RW 0x82CBD5). A selectable own-target test would expose the separate missing Select branch and should not be used to disguise the absence of a fix for #341.

**INFERENCE:** none is needed for the plain non-container allied-target cursor conclusion; the branch and cursor index are directly confirmed in RW, and the Ennorath cursor asset name is read from the mod archive.

**Stops:** none added. Nothing was ported or newly deferred by this analysis.

**Осталось:** попросить владельца подтвердить, что #341 закрываем как соответствие RotWK для обычного союзного объекта, либо указать конкретный контекст в игре (например, принимающий юнита contain), где наблюдение относится к другому действию. Код и фильтр PORT-139 не менялись/не запускались; PR не создавался.
