# Port 116: investigation in progress

Scope: #300, P-54 and P-70. This is not yet a complete diagnosis of P-54.
The Ennorath INI archive was inspected locally; game and mod data are not
included here. Isengard's citadel and Mordor's fortress each declare
`CitadelSlaughterHordeContain`, with a passenger filter, same-player override,
and no allied/enemy/neutral entry. Other faction fortresses and a number of
other structures and units also declare this module. Its passenger filter's
macro selects infantry/cavalry and has exclusions: matching it and ownership
is essential, not a special case for one faction.

## Entry

DONOR BFME2 0x00480661 (`SlaughterHordeContainIsValidContainerFor.cpp`):
first try CanAlwaysEnter, requiring either no status mask or an overlap on the
container; if its filter matches, accept; otherwise use the horde-garrison
base. RW 0x8838C3–0x883932 confirms the same branches: filter validity at
0x8838D3, status test at 0x8838E4/0x8838F7, player and filter test at
0x883900–0x883916, success 0x883918, base call 0x88391C–0x88392A.
The port already has this in `SlaughterHordeContain::isValidContainerFor`.

DONOR BFME2 0x00462977 (`OpenContainIsValidContainerFor.cpp`) (BFME2 named method):
common gates, passenger filter and relationship. RW 0x86603B–0x86620B
confirms dead/defected/ring/enabled gates, combo-horde restriction, filter,
mounted cavalry exclusion, CAN_ENTER_ANYTHING bypass and enemy/neutral/ally
relationship branches. In particular same player + own override accepts at
RW 0x8661A1–0x8661BA. These gates are implemented in
`OpenContain::openContainAllows`. RW 0x87D0E0–0x87D1A5 wraps the garrison
base, optionally tests path, then branches on horde interface and capacity,
horde-member status and membership, or remaining room. See
`GarrisonContain::isValidContainerFor`.

The port already has a synthetic individual-troop actual MSG_ENTER test in
`test_port_55.cpp`, and horde slaughter tests; these are not evidence that the
whole P-54 interactive scenario works. The first divergence explaining the
reported Isengard failure has not been established. The actual mod horde,
object selection, path checks and group-order path still require tracing.

## Cursor

DONOR BFME2 0x00429E11 (`CommandXlat.cpp`, evaluateContextCommand): enter action tests
selected objects, then uses getEnterMessage for the hint; DONOR BFME2 0x0041C5F6
(`ActionManagerSpecialPowerChecks.cpp`, getEnterMessage) branches on contest,
ring and slaughter. RW 0x81EEBC–0x81EEF1 confirms the action-8 test and hint
call via 0x82D75F to 0x82CEFB. RW 0x82CF04–0x82CF22 defaults when selection
or contain is absent; RW 0x82CF2A–0x82CF77 yields contest for an occupied
contain and an eligible other-player entrant; RW 0x82CF81–0x82CFAF tests
COMMANDCENTER and the entire selection for HOLDING_THE_RING; RW 0x82CFB1–
0x82CFC1 queries contain slot 0x30 and returns slaughter hint 0xCB, else 0xB9;
RW 0x82CFCD returns ring hint 0xCC. The port already has this decision in
`ActionManager::getEnterMessage`, but `CommandTranslator::evaluateContextCommand`
currently bypasses it and chooses EnterFriendly or DeliverRing directly.

DONOR BFME2 0x002A3273 (`InGameUICreateCommandHint.cpp`): slaughter hint maps
to cursor index 0x35. RW 0x6A3F4D–0x6A3F5C dispatches through table 0x6A4099;
table entry 0xCB resolves to RW 0x6A4055 (push 0x35), and entry 0xCC to RW
0x6A4059 (push 0x36). The RW cursor-name table at 0xD9E108 maps index 0x35
to `SendToDeath` (string RW 0xBF5D9C). This establishes the P-70 cursor
selection difference, not the cause of the reported red cross in the image.

### Further entry checks (not yet a P-54 diagnosis)

`test_port_55.cpp` also contains `PORT-55 enter command sends a whole horde
through the shared member callback`: it submits MSG_ENTER, advances the
synthetic world, and checks removal and two payouts. It does not exercise the
interactive context evaluation or Isengard's data. The Ennorath object
`IsengardCitadelNew` has the contain; `IsengardFighterHorde` is an infantry
horde with transport slots. Some other Isengard hordes have COMBO_HORDE,
which RW 0x86609C–0x8660BB rejects unless the target has GARRISON or
FS_CASH_PRODUCER; this is a retail restriction, not proof of the owner's case.

DONOR BFME2 0x0029C823 (`InGameUICanSelectedObjectsDoAction.cpp`) action 8 calls
canEnterObject with path checking and rejects the occupied-special result.
RW 0x81EEBF–0x81EECA asks action 8 with additionalChecking=1; RW
0x69D401–0x69D432 passes mode 0 and checkPath=1 and rejects the occupied
result. The port's `CommandTranslator::evaluateContextCommand` currently
passes checkPath=false. This can permit a context action that retail refuses,
not by itself explain the reported failure to enter. The subsequent
`GarrisonObjectGroupOrder` and AI states perform their own path/admission
checks. Do not claim this discrepancy as the P-54 cause without a reproducer.

Cursor-name table 0xD9E108: RW index 0x0E is EnterFriendly, 0x0F is
EnterAggressive, 0x35 is SendToDeath and 0x36 is DeliverRing. The 0xB9,
0xBA, 0xCB and 0xCC dispatch entries at 0x6A4099 lead respectively to RW
0x6A4031, 0x6A4035, 0x6A4055 and 0x6A4059.
