# Port 90: containment sounds — investigation in progress

Sources: RotWK 2.01 `game.dat` (S-001), instructions read with `tools/mod/rw.py dis`;
DONOR BFME2 0x0046279C / 0x0046282B (`Code/GameEngine/Source/GameLogic/Object/Contain/OpenContainSounds.cpp`).

* RW 0x867F56 constructs OpenContain, setting the main vtable to 0xC59C68 and the contain interface vtable to 0xC59AF0; GarrisonContain's RW 0x87B1C0 calls that constructor and installs main vtable 0xC5C810. Both main tables map slot 0x54 to RW 0x8658A3 and 0x58 to RW 0x865932. OpenContain's contain table slot 0x14C maps to RW 0x867AA1.
* The field table places EnterSound at module data +0x38 and ExitSound at +0x3C. RW 0x8658B6 checks the module's load-sound enabled byte +0xDD; RW 0x8658C8 skips if the current frame equals +0x80. Otherwise, when data is non-null, RW 0x8658D9 constructs an event from +0x38, RW 0x8658EB uses the container object ID, and RW 0x86590C calls audio slot 0x64. RW 0x86591E records the frame even if data is null. Disabled skips the frame update.
* RW 0x86594E skips unloading if the frame equals +0x7C; otherwise RW 0x86595C uses data +0x3C, RW 0x86596E uses the container object ID, RW 0x86598F calls audio slot 0x64, and RW 0x8659A1 stores the frame, including when data is null. The two last-frame counters are independent.
* RW 0x86804E / 0x868051 initialize those counters to zero and RW 0x868098 initializes +0xDD to true. RW 0x867AA5 changes +0xDD through contain interface slot 0x14C. Snapshot RW 0x869265 / 0x869270 transfers both counters and RW 0x869476 transfers the enable byte.
* RW 0x86754C calls main slot 0x54 after a successful base add; RW 0x865F73 calls main slot 0x58 during base removal. RW 0x87CD96 invokes the separate onContaining callback for a garrison member; this is not itself the module-data sound call. RW 0x87D021 invokes main slot 0x74 for the horde base-add path. Further derived paths and enable/disable callers still require examination.
* In the port, `OpenContainRuntime.cpp` currently routes both main slots 0x54 and 0x58 to `recalcApparentControllingPlayer`, conflating these with GarrisonContain's separate slot 0x50. The parsed sounds are not played there. `TransportContainRuntime.cpp` and `SiegeEngineContainRuntime.cpp` already identify RW enable/disable calls but currently omit them; these need confirmation before implementation.

The unrelated callbacks RW 0x865F9A / 0x8680F4 use object-specific audio getters RW 0x865C98 / 0x865CB1, not module data +0x38 / +0x3C. The current comments identifying those callbacks as the module-data sounds need correction in the implementation step.

Additional caller evidence:

* DONOR BFME2 `HordeSiegeEngineContainCreatePayload.cpp`, function 0x0047CB89, identifies slot 0x14C as `enableLoadSounds`. RW 0x880016 sends false, RW 0x880112 calls the base add for each accepted crew object, and RW 0x88012F sends true. RW 0x87EEC0 / 0x87EFDB similarly bracket SiegeEngineContain crew creation; RW 0x86A264 / 0x86A2B1 bracket each TransportContain initial-payload entry. These are not calls in the separate transport grab path.
* The port's crew and initial-payload loops exist in `SiegeEngineContainRuntime.cpp` and `TransportContainRuntime.cpp` but omit the toggle. A correct implementation must wire the contain interface's slot rather than only the two base add/remove sites.
* DONOR BFME2 `CaveContain.cpp` has a separate unload-sound caller. CaveContain is not found in the port's containment runtime; whether its RotWK counterpart and other direct callers require a stop remains to be verified. A scan of all RW indirect calls through slot 0x14C finds the six calls above alongside unrelated interfaces; it is not by itself proof that all callers have been classified.

## Scope conflict discovered

RW 0x872811 calls TransportContain's constructor from HordeContain's constructor; RW 0x87282B / 0x87283F install primary table 0xC5B5F8 and contain table 0xC5B480. The latter's slots 0x9C / 0xA4 / 0x14C resolve to the shared OpenContain implementations RW 0x8674C2 / 0x8665BE / 0x867AA1. The former's slots 0x54 / 0x58 resolve to RW 0x8658A3 / 0x865932. RW 0x872D29 then calls the unrelated onContaining callback RW 0x865F9A before HordeContain's rank/slot logic. Thus the shared sound mechanism applies to HordeContain too, not only OpenContain's ported heirs.

In the port `HordeContain` is an independent `ContainModuleInterface` implementation (`HordeContainRuntime.h:182`), not an `OpenContain` subclass; `HordeContainRuntime.cpp:387` implements its own add and member/rank placement and `:977` its own removal, while `HordeContainModuleData::m_transport.m_open` stores the same parsed sound fields. Implementing only OpenContain's path would miss this class; merging or sharing its add/remove mechanism affects horde-member and rank paths, which overlap the separately assigned horde-ranks task. No solution is selected without the owner's direction.
