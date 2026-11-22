# PORT-230 / issue #553 — stop and field audit

Date: 2026-11-16. This is a reviewer-facing disposition of the 22 stop rows in scope and the three CastleBehavior fields. It reconciles the current source/docs in this checkout; it is not a new disassembly pass. `workspace/RotWK/game.dat`, the source audit bundle (including `manual-check.tsv`), and the RotWK/BFME2 installs are absent here. The Castle reader addresses below are preserved from the earlier verified RotWK 2.01 analysis in [`RW_0x79B0F1.md`](RW_0x79B0F1.md).

## Stop dispositions

| Stop | Current disposition | Boundary / reviewer note |
|---|---|---|
| S-303 | Castle member creation now uses `Construction::constructOnPlot` through `CastleBehavior::createOwnedObject`; focused castle tests verify producer identity. | This closes the creation-interface slot-7 construct path, not shared FoundationAIUpdate slot 2, FactionDecal, member damage evacuation, or WallUpgradeUpdate. |
| S-307 | WallHubBehavior and the span-construction path are ported. | Its remaining wall-gap issues are S-657; do not treat the hub implementation as proof of retail legality-code parity. |
| S-657 | Open: RotWK wall-plan legality codes and endpoint/link behavior remain incomplete. | Legality belongs to shared placement/construction mechanisms; PORT-230 does not introduce a wall-only substitute. Fork terrain termination is explicitly described as an inference. |
| S-951 | Open: the complete castle crew recovery/release path remains blocked on shared ActiveBody damage-record amount state. | All three fields are used in retail (readers verified); that does not mean their end-to-end behavior is ported. Also open: straggler relocation, alert-map behavior, and client effects. |
| S-952 | Partially ported castle behavior; shared partition overlap filtering and the horde slot-0x1C8 test remain open. | CastleMemberBehavior's +0x14 is the keep ID, not the plot occupant. FoundationAIUpdate +0x28 is the plot occupant; slot-2 effects are shared work, not an S-303-only fix. |
| S-1103 | The cited siege docking/deploy/exit portions are covered by S-4940..S-4944. | This row still includes unrelated transport/tunnel/turret/bridge gaps; the siege work does not close those. |
| S-1105 | Siege engine crew containment and deployed exit are ported; captured crew ownership remains open. | Capture enters through shared `Object::setCapturedTeam` / S-1225 passenger propagation. TransferSelection is client-side; script naming is also open. |
| S-1770 | Wall line-build command flow is ported. | Restricted-area legality is a shared placement-query boundary; line ghost rendering is client presentation. Neither gets a wall-only workaround here. |
| S-1940 | Gate state, geometry, push-out/contact, and skirmish-AI gate-list paths are ported. | Shared behavior slot 0x80 (`RW 0x68C3E6`) and AI update notification remain open across gate/wall/keep/bridge classes (same seam as S-1986). |
| S-1986 | ReplaceSelfUpgrade runs for wall replacement. | Neighbor/link discovery remains open at shared behavior slot 0x80; this is not implemented just for gates or walls. |
| S-4851 | PlayableSide parsing and its SkirmishAIManager consumer are ported. The `Object +0x4AC` retail writer is identified. | HordeContain's transform reaction sets the transient ground override and clears it after 13 frames; the port still lacks the shared HordeContain/pathfinder-layer behavior. Do not describe the writer as unknown. |
| S-4852 | Open: removed-portal handling does not reproduce the retail horde-member classification/kill response. | Requires shared HordeContain slot 0x21C plus Pathfinder's invalid-portal response; DynamicPortalBehaviour alone cannot close it. |
| S-4853 | DynamicPortalBehaviour/FakePathfindPortalBehaviour and the ladder/tower portal path are ported. | `xfer`/`crc`, Pathfinder wall-state snapshotting, and deployed-ladder save/load regression coverage exist. Exact retail slot-45 `loadPostProcess` sequencing is not claimed; malformed/missing bones and array bounds remain deliberate safe divergences. |
| S-4855 | Open: portal upgrade/removal still does not perform FoundationAIUpdate slot 2. | The real castle postern upgrade reaches this shared occupant/status/drawable interface; `Construction::constructOnPlot` covers slot 7, not slot 2. |
| S-4856 | Raised walkways are partly ported, including raised-layer snapshots. | `Pathfinder::xferWalls` serializes raised layer state; `test_walls_snapshot.cpp` covers Helm's Deep and continued state hashes. Remaining gaps include `Path::optimize`'s layer-change stop (open PR #187) and RW 0x9356DF's final pass; `UpdateLayer` mode 0 is already implemented by WALL-4. Retail snapshot byte/layout parity is not claimed. |
| S-4857 | The pathfinder portal machinery is substantially ported. | Remaining divergences include map-waypoint coverage/semantics and safe handling of malformed Via chains / bounds; see the row's pathfinder stop lines. |
| S-4940 | Retail/Ennorath ladder and tower integration uses `SiegePortalInterface`; PORT-230 corrects TopAttackPos scaling. | Stop remains for differently named/custom portal modules that do not implement the interface. This is not a claim that arbitrary custom modules are integrated. |
| S-4942 | Siege docking, deploy powers, stop power, deploy AI and named scripts are ported. | Remaining AI fields, no-drawable timing, client audio, and client bone-provider conditions are listed in the stop row. |
| S-4943 | Deployed rider exit uses the exact pathfinder wall layer at the rider's cell, falling back to the first wall layer off-wall. | The test now asserts exact layer equality, but the retail-backed case skipped in this environment because game installs are missing. Remaining path-frame/reservation details are still open. |
| S-4944 | Ported click flow handles waypoint mode before the siege branch; +0x8B0 is identified as waypoint mode. | Retail's exact RW 0x81E7A5 byte/branch test remains unverified. The next step is to trace that branch, not to identify the byte itself. |
| S-4945 | Demolisher deploy/toggle behavior is ported for routed commands. | Routing guard/hunt/enter through the shared AIUpdateInterface dispatcher would affect every AI type; this remains shared follow-up, with animation/sounds client-side. |
| S-7195 | Open: SymbioticStructuresBody's geometry update and runtime symbiote ID initialization are not ported. | Retail RW 0x8C4CF4 reads the symbiote's body/damage state and applies ordinary structure height/collision rules. No runtime S-7195 stop is currently emitted. Do not conflate this with S-7191 (Porcupine's unreachable reflected-damage readers). |

## CastleBehavior fields

| Field | Retail evidence | Verdict for this port |
|---|---|---|
| `CrewPrepareInterval` (+0x40) | Update RW 0x79B33D..0x79B34D applies a frame modulo the parsed interval; Ennorath `civilianbuildings.ini:1548` sets 12000. | `parsed-unused` → `used` in reviewer audit; behavior is not ported as part of the incomplete S-951 crew path. |
| `CrewPrepareFX` (+0x44) | Update RW 0x79B351..0x79B362 reads the field and calls `doFXObj`; Ennorath line 1547 uses `FX_EntMootPrepare`. | `parsed-unused` → `used`; the reader is real, but the complete recovery/release flow is not ported. |
| `CrewReleaseFX` (+0x48) | Non-kill `releaseCrew` RW 0x79A337..0x79A352 reads the field and calls `doFXObj`; Ennorath line 1546 uses `FX_EntMootRelease`. | `parsed-unused` → `used`; field use confirmed, runtime parity not claimed. |

Reviewer TSV rows, in the `manual-check.tsv` column order, are in [`RW_0x79B0F1.md`](RW_0x79B0F1.md). The audit bundle is not present in this checkout, so `manual-check.tsv` itself has not been edited. The missing shared ActiveBody damage-record amount must be added with the common CRC/xfer/save-version protocol before S-951 crew recovery can be implemented safely.

## Owner-visible Ennorath check

For the PORT-230 siege change: compare RotWK 2.01 and Ennorath on the same wall with an `IsengardSiegeLadder` and a `MordorSiegeTower`; inspect the approach/top-attack point. For a discriminating fixture, give the portal a nonzero `TopAttackPos.Z` and a tilted local X axis. The model predicts that the local X axis is divided by `max(abs(x), abs(y))`, its **3D** length scales `TopAttackPos.X/Y`, `Z` remains unchanged, then the point receives the world transform. This is a manual comparison to request, not a test performed in this environment.
