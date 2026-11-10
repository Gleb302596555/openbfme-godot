# PORT-109: Upgrade/Cursor — RW map and validation status

Issue #269, task 85. Cursor and target-check instructions were read with
`tools/mod/rw.py dis` on the owner's RotWK 2.01 binary (MD5
`daed3668006cd90f01c34e5a7da1901f`). The owner posted both proofs in
[issue #269](https://github.com/Gleb302596555/openbfme-godot/issues/269)
and explicitly authorized citing them as RW facts without a local binary.
Earlier input-access and shared-predicate blockers are resolved.

## Field and reachable reader chain

UpgradeTemplate's field table row RW 0xC108F8 names Cursor (RW 0xC10A54),
uses parseAsciiString RW 0x42EE5E, and writes template +0x74.
RW 0xB5577B is unrelated APT/Lua Cursor string concatenation, not a reader.
The reader is CommandTranslator, not an Upgrade virtual getter:

1. `evaluateContextCommand`, RW 0x81E60B: action 0xC is checked first at
   RW 0x81F1E0..0x81F1F8. If that fails, RW 0x81F29D..0x81F2A9 checks
   action 0x12 with selection rule ANY (0). Failure continues to action 0x11
   at RW 0x81F33B. Port branch is immediately after evaluateCapture.
2. InGameUI selection walk RW 0x69D28F, jump table RW 0x69D5AD,
   action 0x12 -> RW 0x69D4E1 dispatches selected Drawable +0xFC to
   ActionManager RW 0x82D780. ANY succeeds at the first passing source
   (RW 0x69D572); ALL compares successes against selected count.
3. DoCommand (type 0 at RW 0x81F2AF): RW 0x81F2B4..0x81F2CD asks
   RW 0x81DF04 for the first selected object's first SPECIAL_POWER button
   whose final power type is 0x33. Absent button ends evaluation, not a
   move fallback (RW 0x81F2C0 -> 0x81F77D). Present button goes to the
   shared issue path RW 0x81F426. The port reuses issueSpecialPower.
4. Other types: RW 0x81F2D2..0x81F332 emits message 0x7D6, argument 0
   target ID, argument 1 cursor index. The FIRST selected object's mask
   (+0x28C) goes to UpgradeCenter RW 0x66F468 — not the source that passed
   ANY. The list head is +0xC, link +0x64, mask bit +0x38. Creation links
   templates at the HEAD (RW 0x66F248, existing Upgrade.h evidence), so the
   old GiveUpgradeUpdate ascending-bit helper was not equivalent. Shared
   UpgradeCenter::findUpgrade(mask) preserves creation/reload list order.
5. If found: template +0x74 goes through Mouse::getCursorIndex RW 0x5EE9CA.
   Empty returns -1 at RW 0x5EE9CE / 0x5EE9D7. Names compare without case
   against 56 cursor names at RW 0xD9E108; unknown is -1. If no upgrade,
   RW 0x81F337 pushes 1 (Normal), not -1 or Target.
6. InGameUI::createCommandHint RW 0x6A3B77: message 0x7D6 dispatch at
   RW 0x6A3F3A..0x6A3F47 to RW 0x6A405D. Argument 1 (RW 0x710C9E)
   reaches RW 0x69BD53 at RW 0x6A4081, unchanged even for -1. Selecting
   or scrolling suppress cursor changes; otherwise RW 0x69B06D sets it.

## Shared target predicate (not a cursor-only approximation)

ActionManager RW 0x82D780..0x82D7EB, in order:

- RW 0x82D783 / 0x82D78D: source and target must exist.
- RW 0x82D79A: relationship is ALLIES (2), NOT the donor's decompiled
  enemy label. The RW confirmation is authoritative.
- RW 0x82D79F: target AI pointer +0x260 exists (Object::getAIUpdateInterface).
- RW 0x82D7AB: target template IMMOBILE (KindOf 2) is clear.
- SpecialPower store lookup RW 0x69C146 must find SpecialAbilityGiveUpgrade
  (string RW 0xC362A0).
- RW 0x68C461: source's first special ability update of TYPE 0x33 must
  exist. The named template is a presence check, not the update's identity.
- RW 0x82D7E4 calls GiveUpgradeUpdate RW 0x89FE64(target).

GiveUpgradeUpdate uses the same owner mask lookup RW 0x66F468. Missing
upgrade refuses at RW 0x89FE80. A non-contained target calls
Object::affectedByUpgrade RW 0x694914; its result is returned unchanged.
ActionManager::canGiveUpgrade implements this once. Both context selection
and existing canDoSpecialPowerAtObject case 0x33 reuse it.

### Remaining stop: contained-target provider only

RW 0x89FE86 sees target +0x27C (container). Helper RW 0x89FD77 resolves
its producer ID (+0x78; Object::getProducerID), gets that object's contain,
then contain slot 0x7C (RW 0x89FDA1) yields the provider. Among retail
contains only HordeContain yields non-null there (S-7229). Null refuses;
otherwise provider slot 0xAC(upgrade), RW 0x89FEA0, decides.

The port has getHordeContainInterface, but no equivalent provider upgrade
query for the final slot 0xAC. The owner explicitly authorized implementing
non-contained targets and refusing this branch. It returns false and reports
`[S-6011] PORT-109 contained upgrade target...`; the test pins that report.
It does NOT use affectedByUpgrade on a contained member as a substitute.
Other pre-existing S-6011 delivery/scheduling/effect gaps are not closed.

## Client adaptation and donor provenance

The existing hint path uses names rather than numeric MouseCursor values.
CursorFile maps RW indices to canonical names. -1 maps to an empty name,
1 to Normal, WeaponUpgrade to 42, ArmorUpgrade to 43. Existing GodotMouse
updateCursor hides a name with no resource (including empty); no device
handler or second stream consumer was added. CommandTranslator calls its
existing hint(), preserving placement/GUI modes and hintCursor suppression.
No logic order is emitted by a hint.

DONOR BFME2 0xDB9058 (Code/GameEngine/Source/GameClient/Input/Mouse.cpp)
supplies the symbolic cursor table. DONOR BFME2 0x0026F0F0
(Common/System/Upgrade.cpp), 0x002A3273 (InGameUICreateCommandHint.cpp),
0x0042970A (GUI/ControlBar/Rva0042970AGet.cpp), 0x0041CE27
(Common/RTS/ActionManager.cpp) and 0x0049C6E1 (GiveUpgradeUpdate.cpp) were
orientation only; the owner-confirmed RW addresses above govern behavior.
INFERENCE retained from the owner: nonzero evaluate-type values correspond
to the port's DoHint / EvaluateOnly. Both use the confirmed non-command arm.

## Behavior — RW — test

All tests live in test_port_109.cpp, Port109.cmake, full openbfme_tests.

| Behavior | RW | PORT-109 test suffix |
|---|---|---|
| WeaponUpgrade cursor, no duplicate order | 0x81F2D2..0x81F332 / 0x6A4081 | WeaponUpgrade from first selected upgrade uses existing hint path |
| ANY source succeeds, first has no upgrade -> 1 | 0x69D572 / 0x81F337 | ANY success still resolves cursor from first source without upgrade to 1 |
| Empty/unknown -> -1, case-insensitive | 0x5EE9CA / 0x5EE9D7 | empty Cursor passes minus one through hint without Target fallback |
| Selecting/scrolling suppression, missing command ends | 0x69BD53 / 0x81F2C0 | selecting and scrolling suppress upgrade hints and commands do not duplicate hints |
| Shared target predicate and contained stop | 0x82D780 / 0x89FE64 | shared target predicate preserves ally AI and immobile rejection and contained stop |
| Head-first upgrade choice | 0x66F248 / 0x66F468 | mask lookup follows template head order not lowest bit |
| Existing special-power dispatch shares predicate | 0x82D925 / 0x82D780 | object power type 0x33 uses the shared predicate rather than S-7140 |
| One command, existing payload and target | 0x81F2B4 / 0x81F426 | upgrade click issues exactly one existing special power command |

## Validation status and remaining work

Full Release openbfme_tests (not an auxiliary runner), before the last rebase:
PORT-109*: 8/8 tests, 198/198 assertions PASS. Red/green: temporarily disabling
only the new context cursor branch produces 6 failed tests / 15 failed
assertions; restoring and rebuilding produces 8/8, 198/198 PASS. The fixture
uses CombatWorld to register real LogicModules and attach AIWorld.
Neighbours modgap2 give upgrade* + PORT-92*: 8/8, 255/255 PASS.

After the final rebase onto main 9377dba3 the full target completed all
1020 steps: PORT-109* 8/8, 198/198 PASS (6254 excluded by filter).
Combined PORT-109*, modgap2 give upgrade*, PORT-92*: 16/16 PASS.
PR #326 is ready for review.
No game/mod files or hash pins committed. No new simulation state: template
lookup order derives from INI loading; xfer/CRC stay unchanged. This IS an
engine change: special-power type 0x33 now admits ordinary valid targets and
multiple upgrades choose list order.

Known nightly shift, executed and confirmed: test_cbhunt.cpp:307,
`cbhunt: ActionManager::canDoSpecialPowerAtObject (RW 0x82D925): the generic tests, the yes types, and the unported per-type tests counted as S-7140`.
Exactly three old assertions at lines 329-331 fail: unported flag and two
unported 0x33 counters. The target still fails validation, but type 0x33 is
now implemented and must not report S-7140. This other port's test is NOT
modified. Watch cbhunt*, AIPOW-1*, modgap2 give upgrade*, PORT-92*, hud*,
save4*, state hash*.

Owner-visible Ennorath verification is not claimed: existing PORT-92 notes
place delivery carriers in missions/cinematics, not an ordinary skirmish
OBJECT_UPGRADE button. A carrier must actually hold a source upgrade mask;
spawning it alone is insufficient. See PR #326 for the conditional check.
Contained providers remain explicitly deferred as authorized, not claimed
ported. Remaining: owner review and optional in-game verification.
