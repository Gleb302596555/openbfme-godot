# Port 282 / issue #649 — the uncited functions of `WorkerAIUpdate` and `DozerAIUpdate`

Research checkpoint, 2026-11-18. Addresses are in vanilla RotWK 2.01 `game.dat`
(md5 `daed3668006cd90f01c34e5a7da1901f`), read with `tools/mod/rw.py dis` and a
capstone "function until the first `ret`" script, caveat S-001. The BFME2 1.06 decomp
(`/tmp/open-bfme-2`) is used only as the readable twin; every link below was confirmed
in RW unless the row says otherwise.

The task: `docs/ennorath/RW_FUNCTION_MAP.tsv` lists 31 functions of `WorkerAIUpdate`
and 21 of `DozerAIUpdate` with `status = none` (`depth 0`, no citation). Every one is
classified here as **ported**, **scaffolding** (no behaviour to port: a dtor, a table,
an xfer, a debug string), **client** (a drawable / audio / UI reader), **stop**
(behaviour that exists and is not ported: a stop id is named) or **not behaviour**
(a thunk, a trap, a getter of a field the port already has).

The two vtables the two classes install, dumped from the image:

* the dozer interface, `DozerAIUpdate` `+0x4A8` → RW 0xC622D8, the worker `+0x4A8` → RW 0xC6AC18
* the primary AI table: `DozerAIUpdate` RW 0xC62070, the worker RW 0xC6ACA8

(`/tmp/vt.py`-style dumps, slots `+0x00 .. +0xB4`; the port's `DozerAIUpdate` comments
carry the same slot numbers.)

## What this port adds: the two repair orders

Two of the "none" rows were behaviour that the port was missing, both in the repair
order path, and both are ported here (condition (а) / (б) of the PROCESS 5.7 rule: same
class, not a shared mechanism):

1. **RW 0x88CB8A, `canAcceptNewRepair`** — the dozer interface's slot 0x50 (the table
   above, slot index 0x50 / 20). The port had the slot as a virtual declared and never
   overridden (`DozerAIUpdate.h` carried its address in the dozer-interface list).
   Body, byte for byte:

   | RW | instruction | meaning |
   |---|---|---|
   | 0x88CB90 | `test edi, edi` / `je 0x88cbfd` | no ordered object → refuse (0) |
   | 0x88CB98 | `call [eax+0x24]` / `cmp eax, 1` | the interface's current task, must be REPAIR (1) |
   | 0x88CBA0 | `push [esi+0xc]` / `call 0x449681` | the task's target id (+ 0xC, task 1) → `findObjectByID` |
   | 0x88CBB2 | `cmp eax, edi` / `je 0x88cbfd` | the structure being repaired is the ordered one → refuse |
   | 0x88CBB6 | `test dword [tmpl+0x108], 0x1000000` | current is a BRIDGE_TOWER (KindOf 24)? |
   | 0x88CBC6 | `test dword [tmpl+0x108], 0x1000000` | ordered is a BRIDGE_TOWER too? |
   | 0x88CBD1 | `call 0x85B874` twice | each object's bridge module id |
   | 0x88CBDF | `test esi, esi` / `je 0x88cbfd` | a NULL id on either side → refuse |
   | 0x88CBF9 | `cmp eax, edi` / `jne 0x88cc01` | equal ids → refuse |
   | 0x88CC01 | `mov al, 1` | everything else → accept |

   `0x85B874` is `Object::getBridgeID`-shaped: the template's `+0x24C` module table of
   the kind, each module asked for its bridge id through vslot 4 (`call [eax+4]`), NULL
   when the object is not a BRIDGE_TOWER or has no such module. So the branch only lets
   an order through for two towers of *different* bridges — in practice never, since the
   two towers of one bridge share their id. The port has no bridge module (S-161), so the
   call stands for "NULL" on both sides and the order for the other tower is refused, as
   in retail. The port keeps this in `DozerAIUpdate::canAcceptNewRepair`.

2. **RW 0x8AD2D3, `WorkerAIUpdate::repair`** — the worker's own dozer-interface slot
   0xB0 (the worker table RW 0xC6AC18, where the dozer's class puts 0x88BD8D). The port
   had inherited the dozer's `repair`, which is *not* what a worker runs:

   | RW | instruction | meaning |
   |---|---|---|
   | 0x8AD2EB | `call 0x82D7F2` | the ActionManager's `canRepairObject(dozer, target, source)` — the port's `canRepair` stand-in (S-1282) |
   | 0x8AD2F7 | `test byte [tmpl+0x109], 0x80` | the worker is a SWARM_DOZER (KindOf 15)? |
   | 0x8AD309 | `call [eax+0x50]` | not a swarm: the slot 0x50 test above |
   | 0x8AD310 | `call 0x68B8D3` | the structure's sole healing benefactor |
   | 0x8AD31B | `cmp eax, [edi+0x74]` | it is another object → refuse |
   | 0x8AD32B | `push 1` / `call [eax+0x30]` | the interface's newTask(REPAIR=1, structure) |

   The dozer's own body (RW 0x88BD8D) asks the slot 0x50 **first** (0x88BDA2), then
   `canRepairObject` (0x88BDB5), then the sole healer (0x68B8D3 at 0x88BDC0). The port's
   `DozerAIUpdate::repair` now does the same order, and the worker overrides it with the
   body above (`WorkerAIUpdate::repair`). Both end in `newTask(REPAIR)` on the same
   structure, which the port's `DozerAIUpdate::newTask` is.

Tests: `engine/tests/test_port_282.cpp` (six synthetic cases: no task, the same
structure refused, another structure accepted, the bridge-tower pair refused, the
swarm worker skipping both gates, a foreign sole healer refused and then accepted when
the claim expires), registered by `engine/cmake/Port282.cmake`.

## The full classification

`RW_FUNCTION_MAP.tsv` rows with `status = none`, in address order. "port" means the
port's twin of the same body, not an RW address.

### `WorkerAIUpdate` (31 rows)

| RW | class | why |
|---|---|---|
| 0x4944E7 | not behaviour | `mov [ecx+0xB8], eax` — a setter of a field the port keeps (`WorkerAIUpdate.h`, the module state) |
| 0x610A6C | not behaviour | `ret 0x10` — an empty virtual |
| 0x88BEC8 | scaffolding | `fld [eax+0x68]` — the module data's BoredTime getter (shared: the dozer's interface slot 0x08 and the worker's 0x04); the port reads the field in the ctor |
| 0x88CAB4 | refused-order tail, ported by S-1282 | the dozer interface's slot 0x54 (`canBeRepaired`-shaped): bridge id of both sides (0x85B874), then the target's "phantom" flag (0x8595BA walks the target's module list and stops at the first module whose vslot 0x30 answers non-NULL); it decides an order the ActionManager's test refused, so the port's `canRepair` stand-in (S-1282) owns it. Not in the repair order path this lane ports |
| 0x892C33 | scaffolding | `jmp 0x667BEF` — the AI base's loadPostProcess thunk; see S-7487 (the port's `Module::loadPostProcess` gap) |
| 0x8ACD22 | scaffolding | `fld [eax+0x6C]` — a getter of module data `+0x6C` (BoredTime); the port reads it in the ctor |
| 0x8ACD2C | scaffolding | `fld [eax+0x70]` — BoredRange getter; read in the ctor |
| 0x8ACE45 | scaffolding | `jmp 0x66D8F5` — the dock clearing of the base interface (`DozerAIUpdate`'s own 0x88CFFE is the twin) |
| 0x8ACEE3 | ported | `cancelTask` (slot 0x34) then the AI's walk-home (vslot 0x18 of the machine at `+0xE0`) — the port's `internalCancelTask` + `aiIdle` |
| 0x8ACEFE | ported | `isTaskPending(task)` — slot 0x18 (`+0xC + task*8` in the worker's interface layout), `m_task[task].target != INVALID_ID` |
| 0x8ACF0E | ported | `getTaskTarget(task)` — slot 0x1C |
| 0x8ACF19 | ported | `cancelTaskInternal(task)` — slot 0x34: the dock clear (slot 0x40) and the three dock points |
| 0x8ACF48 | ported | `getMostRecentCommand` — slot 0x14 (the pending task with the largest order frame) |
| 0x8ACF85 | ported | `getDockPoint(task, point)` — slot 0x44, bounds-checked `[this + 0x28 + (task*3+point)*0x10]` |
| 0x8AD106 | ported | the building sound's `finishBuildingSound` (RW 0x88BFA9's worker twin; `+0xE8` handle) |
| 0x8AD216 | scaffolding | the static registration object (RW 0xC0B0D4 table) |
| 0x8AD25B | scaffolding | returns the static table's address |
| 0x8AD261 | not behaviour | `this ? this+0x4AC : NULL` — the supply interface getter |
| 0x8AD270 | not behaviour | `this ? this+0x4B0 : NULL` — the worker interface getter |
| 0x8AD27F | not behaviour | `mov al, [ecx+0xD9]` — a byte getter |
| **0x8AD2D3** | **ported here** | **`repair` — the worker's slot 0xB0 (see above)** |
| 0x8AD533 | client | the sound of a repair (`TheAudio` vslot 0x64, a `0x6DA95E` event structure built from the ordered target) |
| 0x8AD5AE | scaffolding | scalar deleting dtor of the worker |
| 0x8ADA95 | scaffolding | ctor thunk: `sub ecx, 0x4A8` then the ctor RW 0x8ADAA0 |
| **0x8AE0FE** | **ported here** | the worker interface's own slot 0x50: the same `canAcceptNewRepair` body as the dozer's RW 0x88CB8A, recompiled for the worker interface's layout (its tasks start at +0xC, so the REPAIR target is `[esi+0x14]` instead of `[esi+0xC]`; slot 0x24 and slot 0x1C are the same). The port keeps one interface layout, so one `DozerAIUpdate::canAcceptNewRepair` serves both |
| 0x8AE17C | ported | the command's tail: the mine-clearing box drop (`IS_ATTACKING` + WEAPON_ANTI_MINE) — `WorkerAIUpdate::commandAccepted` |
| 0x8AE203 | ported | `cancelAllTasks`: for each of the 3 tasks, `isTaskPending` then `cancelTaskInternal` — `DozerAIUpdate::cancelAllTasks` |
| 0x8AE26B | refused-order tail, ported by S-1282 | the worker interface's slot 0x58, the same body as the dozer's 0x88CAB4 (bridge id, then 0x8595BA); see that row |
| 0x8AE456 | scaffolding | thunk: `sub ecx, 0x4A8` then RW 0x8AE203 (cancelAllTasks) |
| 0x8AE4A9 | ported | slot 0x30, the pair of `newTask`: it clears `+0xC0` and the three dock points, then for task 0 / 1 asks slot 0x18 (`isTaskPending`) and slot 0x34 (`cancelTaskInternal`), stores the target id `[obj+0x74]` and the order frame `TheGameLogic+0x40` into `+0xC/+0x10`, lets the AI note the command (`+0xE0` vslot 0x18), and at a finished build it draws the two position messages (`0x6CF86E`, format RW 0xC6B228) and makes the goal object (0x8AE2AC) and its `TheInGameUI` message; for another task at 0x8AE6E1 the same store plus the goal object's `kill(DAMAGE_UNRESISTABLE)`. The port's `DozerAIUpdate::newTask` (RW 0x88CCFC) is the dozer's separate body; the worker's message half is client (TheInGameUI) and its claim path is not ported (S-7489 names the ActionManager halves) |
| 0x8AE74B | ported | `internalCancelTask` — slot 0x3C: `internalTaskComplete` (slot 0x40), the task and the dock points cleared, then the AI player's note — `DozerAIUpdate::internalCancelTask` |
| 0x8AE798 | ported | `aiDoCommand`'s worker gate: `ACTIVELY_CONSTRUCTING`, `HARVEST_PREPARATION`, `HARVEST_ACTION` cleared before the gate — `WorkerAIUpdate::commandAccepted` / the base `DozerAIUpdate::commandAccepted` |
| 0x97D97C | not behaviour | `mov [ecx+0x24], eax` — a setter |

### `DozerAIUpdate` (21 rows)

| RW | class | why |
|---|---|---|
| 0x88BDDE | ported | `isTaskPending` — slot 0x18 (`m_task[task].target != INVALID_ID`) |
| 0x88BE0D | ported | `getTaskTarget` — slot 0x1C |
| 0x88BEBE | scaffolding | `fld [eax+0x64]` — RepairHealthPercentPerSecond getter; read in the ctor |
| 0x88BEC8 | ported | `getMostRecentCommand` — slot 0x14 |
| 0x88BED2 | ported | the bored range (AIData `+0x88` for a computer player, S-7332) — `DozerAIUpdate::boredRange` |
| 0x88BF1A | ported | the AI player's "dozer free" call: the goal object's `+0x33C` (the price paid) through the AI (`0xA3CFA4` is the float-to-int helper) — `AIPlayer::dozerFree`'s caller in `DozerAIUpdate::taskSucceeded` (S-1282) |
| 0x88BF7A | ported | the dozer interface's slot 0x6C, the AI's `aiExit` on the phantom structure: `[+0x568] != 0` → the interface's destroy (vslot 0x6C), else the goal object `kill(8, 0)` (RW 0x698EC3) — `DozerAIUpdate::removeUnplacedStructure` / `taskSucceeded` |
| 0x88C8CA | scaffolding | the static registration object (RW 0xC0B1E8 table) |
| 0x88C90F | scaffolding | returns the static table's address |
| 0x88C924 | not behaviour | `mov [ecx+0xBC], eax` — a setter |
| 0x88C931 | scaffolding | thunk: `sub ecx, 0x4A8` then the construct body RW 0x88C217 |
| 0x88CAB4 | refused-order tail, ported by S-1282 | the dozer interface's slot 0x54: bridge id (0x85B874), then the target's phantom flag (0x8595BA); it belongs to a refused order's tail, the port's `canRepair` stand-in owns it (S-1282) |
| **0x88CB8A** | **ported here** | **`canAcceptNewRepair` — slot 0x50 (see above)** |
| 0x88CF96 | ported | `cancelAllTasks` — slot 0x20: each of the 3 tasks cancelled through the interface's own slots, then the dock points and `+0x568` cleared |
| 0x88D0C6 | ported | `getPhantomStructure`: `findObjectByID(+0xC0)` — `DozerAIUpdate::unplacedStructure`'s object |
| 0x88D186 | stop **S-7487** | the primary table's slot 0x04, `loadPostProcess`: the AI base's loadPostProcess (RW 0x667BEF, the Lua re-registration) then, with a phantom structure id at `+0x568`, `makePhantomStructureInert`; the port has no module `loadPostProcess` hook |
| 0x88D1A1 | scaffolding | thunk: `sub ecx, 0x4A8` then `cancelAllTasks` (0x88CF96) |
| 0x88D692 | scaffolding | the scalar deleting dtor of `DozerAIUpdate` (`~DozerAIUpdate` is 0x88D1AC) |
| 0x8AE26B | refused-order tail, ported by S-1282 | the worker interface's slot 0x58, the twin of the worker's 0x88CAB4 row (see above) |
| 0x8F114D | not behaviour | a getter of `[+0x28]` |
| 0x99474B | not behaviour | a getter of `[+0x20]` |

## Left to another lane (not new stops)

* `makePhantomStructureInert` (RW 0x88D0D8) and its twin `notInert` (RW 0x88D123): the
  port already performs the same transition inline in
  `Construction::constructByDozer` / `placePhantomStructure` (PENDING_CONSTRUCTION,
  PHANTOM_STRUCTURE, health 1, out of the world and back). The only caller the map names
  is `loadPostProcess` (**S-7487**), so a ported hook would need that lane first.
* the ActionManager's `canRepairObject` (RW 0x82D7F2) and `canResumeConstructionOf`
  (RW 0x82C84C) stay the port's stand-ins (**S-7489**), and the bored wander of an AI
  player's idle dozer (RW 0x88E17F: the AI state's `AI_BORED`, the goal object's
  `getMostPerpendicular`-shaped pick at RW 0x8F0284, `0x8DBACE` for the goal object,
  RW 0x698EC3's kill) stays **S-1282**'s.
* the bridge module (`0x85B874`'s object side) stays **S-161**'s.
