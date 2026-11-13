# Port 116 / P-70: slaughter cursor

Scope: #300, P-70 only. The owner verified P-54 fixed in game on main
9377dba3 on 2026-11-11; this change does not claim that fix.
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
RW 0x82CFCD returns ring hint 0xCC. The port already had this decision in `ActionManager::getEnterMessage`;
before this change `CommandTranslator::evaluateContextCommand` bypassed it
and chose EnterFriendly or DeliverRing directly.

DONOR BFME2 0x002A3273 (`InGameUICreateCommandHint.cpp`): slaughter hint maps
to cursor index 0x35. RW 0x6A3F4D–0x6A3F5C dispatches through table 0x6A4099;
table entry 0xCB resolves to RW 0x6A4055 (push 0x35), and entry 0xCC to RW
0x6A4059 (push 0x36). The RW cursor-name table at 0xD9E108 maps index 0x35
to `SendToDeath` (string RW 0xBF5D9C). This establishes the P-70 cursor
selection difference, not the cause of the reported red cross in the image.

Cursor-name table 0xD9E108: RW index 0x0E is EnterFriendly, 0x0F is
EnterAggressive, 0x35 is SendToDeath and 0x36 is DeliverRing. The 0xB9,
0xBA, 0xCB and 0xCC dispatch entries at 0x6A4099 lead respectively to RW
0x6A4031, 0x6A4035, 0x6A4055 and 0x6A4059.
