# Port 64 — RubbleRiseUpdate (issue #176): the RW analysis

`game.dat` md5 `daed3668006cd90f01c34e5a7da1901f` (RotWK 2.01), read with `tools/mod/rw.py dis/xref/str`. Donor: `Open-BFME-2` (BFME2 1.06),
`Code/GameEngine/Source/GameLogic/Object/Update/RubbleRiseUpdate.cpp` — marked DONOR where it is the only witness; every step below was confirmed in
RotWK. Claim: a destroyed camp or castle piece, sunk below the terrain as rubble, rises back to the ground, shaking, firing FX / OCL by phase, and
ends as `POST_COLLAPSE`.

## 1. The class chain (registration -> create -> constructor -> vtable -> slots)

| What | RW |
|---|---|
| name string `"RubbleRiseUpdate"` | 0xC0B148 (3 code references: 0x659DF1, 0x8A8298 = `mov eax, 0xC0B148; ret` of the name getter, 0x8A8306 in the name-key getter) |
| factory registration (line 0xA7: two procs + module type 3 = BEHAVIOR) | 0x659DF0 (`push 0xC0B148`, then `push 3 / push name / push 0 / push 0 / push 0x6563F3 / push 0x64EAB7 / call 0x6570FE`) |
| create (`friend_newModuleInstance`) | 0x64EAB7 — `mov eax, 0xBA23F0; call 0xA3CEF0` (stack probe), `push 0x48; call 0x42F6E0` (allocate 72), `call 0x8A8325` (ctor). Instance size **0x48** |
| data create (`friend_newModuleData`) | 0x6563F3 — allocates **0xD8**, calls 0x656350 (data ctor), then `push 0x8A8AE4 / call 0x42DBBD` (parse) |
| data ctor | 0x656350 (see §2) |
| parse entry (`buildFieldParse` wrapper) | 0x8A8AE4: `push 0 / push 0xC69408 / call 0x42B8D7` then `push 8 / call 0x8D296A / push eax / call 0x42B8D7` — field table 0xC69408 with userData 0, then the DieMux table with userData 8 |
| instance ctor | 0x8A8325: base RW 0x650114? no — `call 0x653114` is the Module base; vtables `[+0]=0xC69310`, `[+0xC]=0xC69038`, `[+0x10]=0xC69304`, `[+0x20]=0xC69300`; state +0x24..+0x44 zeroed; `call 0x850C32(obj, 0x3FFFFFFF)` = setWakeFrame(UPDATE_SLEEP_FOREVER) |
| update | vtable 0xC69304 slot 0 = **0x8A85D0** (it is called with `ecx` = instance + 0x10, the UpdateModuleInterface sub-object: every state field is `[ecx - 0x10 + N]`) |
| onDie | vtable 0xC69300 slot 0 = **0x8A83F6** (called with `ecx` = instance + 0x20, the DieModuleInterface) |
| name getter | 0xC69318 -> 0x8A8297 (`mov eax, 0xC0B148; ret`) |
| xfer | module table slot 3 = **0x8A8547** (object xfer writer/reader: +0x24, +0x28, +0x2C, +0x30, +0x34, +0x38, +0x3C..+0x44) |
| doPhase | **0x8A847C** (called with `ecx` = instance base) |
| reset (from onDie) | **0x8A83CE** |
| doRubbleDoneStuff | **0x8A841C** |
| index-list helper (its own copy of `buildNonDupRandomIndexList`) | **0x8A829D** (`range` in eax, `count` on the stack, the list in ebx; `dec eax` = range - 1) |

Instance layout (all confirmed by the ctor 0x8A8325 and the xfer 0x8A8547): `+0x24` an unused word (written 0, read by nothing), `+0x28`
`m_nextBurstFrame`, `+0x2C` the state (0 waiting, 1 rising, 2 done), `+0x30` the rise velocity, `+0x34` the current height, `+0x38` the target height,
`+0x3C` a `Coord3D` (the saved start position). 0x48 bytes exactly.

## 2. Module data (RW 0x6563F3 -> 0x656350, table 0xC69408)

Data ctor 0x656350: `[+0] = 0xC09DD0` (vtable), `lea ecx, [esi+8]; call 0x8D2970` = the DieMuxData constructor at **+8**, two
`call 0xA3D28E` vector-array constructs at **+0x58** (4 x 0xC) and **+0x88** (4 x 0xC), then:

| field | offset | default | parser (table row) |
|---|---|---|---|
| MinRubbleRiseDelay | +0x38 | 0 | 0x73A429 (`parseDurationUnsignedInt`) |
| MaxRubbleRiseDelay | +0x3C | 0 | 0x73A429 |
| MinBurstDelay | +0x40 | **0x270F = 9999** | 0x73A429 |
| MaxBurstDelay | +0x44 | **not written** (heap garbage in retail, as the collapse's) | 0x73A429 |
| BigBurstFrequency | +0x48 | 0 | 0x42EC5E (`parseInt`) |
| RubbleRiseDamping | +0x4C | 0.0 | 0x42ED00 (`parseReal`) |
| RubbleHeight | +0x50 | 0.0 | 0x42ED00 |
| MaxShudder | +0x54 | 0.0 | 0x42ED00 |
| OCL | +0x58 (4 vectors) | empty | 0x8A8A82 |
| FXList | +0x88 (4 vectors) | empty | 0x8A8A1D |
| per-phase OCL count | +0xB8 | **1 each** (loop at 0x6563D6) | — |
| per-phase FX count | +0xC8 | **1 each** | — |

Note the row order of table 0xC69408 differs from the collapse's (RW 0xC69238): here MaxShudder comes after RubbleHeight, and there is no
`DestroyObjectWhenDone` / `CollapseHeight` (the collapse's rows +0xF4 / +0xF8 have no counterpart; the data is 0xD8 bytes against the collapse's 0x100).
`RubbleHeight` +0x50 is parsed and **read by no slot of the class** (§4).

Phase names (RW 0xDB0ED0, read from the table by both parsers): `INITIAL`, `DELAY`, `BURST`, `FINAL` — four, no `ALMOST_FINAL` (the collapse's
table 0xDB0E7C has five). Both parsers push every following token into `m_ocl[phase]` / `m_fx[phase]` (the 0xC-byte STL vectors; parseFXList 0x8A8A1D:
`scanIndexList(token, 0xDB0ED0)`, then per token `TheFXListStore->findFXList` (0x5E20A2) as a push_back).

## 3. onDie and the reset

`onDie` RW 0x8A83F6 (`ecx` = instance + 0x20):

```
008A83FD  mov ecx,[esi-0x18]   ; the object (instance + 8)
008A8400  mov eax,[esi-0x1c]   ; the module data (instance + 4)
008A8403  push ecx / lea ecx,[eax+8] / call 0x8D29A9   ; DieMuxData::isDieApplicable(obj, damageInfo)
008A840C  test al,al / je ret
008A8410  lea ecx,[esi-0x20] / call 0x8A83CE           ; the reset
```

The reset RW 0x8A83CE: `[+0x24]=0`, `[+0x2C]=0` (waiting), `[+0x30]=0.0`, `[+0x28]=0`, `[+0x34]=0.0`, `[+0x38]=0.0`, then
`call 0x850C32(obj, 1)` = **setWakeFrame(UPDATE_SLEEP_NONE)**. It does not touch `+0x3C` (the saved position) or `+0x20`.

## 4. update (RW 0x8A85D0) — branch by branch

`ebx` = the UpdateModuleInterface (instance + 0x10), `esi` = `[ebx-8]` = the object, `edi` = `[ebx-0xC]` = the module data.

**A. waiting (state 0) and a drawable and POST_RUBBLE**
```
008A85E9  call 0x70E013              ; Object::getDrawable
008A85EE  cmp [ebx+0x1c],0 / jne B   ; state != 0 -> B
008A85FB  test eax,eax / je B        ; no drawable -> B
008A8603  mov eax,[esi+0x110] / shr eax,0x1B / test al,1 / je B   ; model conditions at object + 0x10C: word +4 bit 27 = index 59 = POST_RUBBLE
008A8614  lea eax,[esi+0x38]; push eax ; push 0 ; call 0x8A847C            ; doPhase(INITIAL = 0, obj->getPosition())
008A8632  mov [ebx+0x1c],1                                                 ; state = rising
008A8639..0x8A866E  two random reals in [-maxShudder(+0x54), +maxShudder] via 0x6D33AB   ; the CLIENT generator (GameClientRandomValueReal)
008A8684  call 0x70E013; add eax,0x1A0                                     ; the drawable's instance matrix
0x8A868E..0x8A8717  copy it to the stack, set the translation to (draw1, draw2, 0)
008A8720  call 0x6714C0                                                    ; Drawable::setInstanceMatrix(&m, true)
008A872D  mov [ebx+0x1c],1                                                 ; state = rising (written a second time)
008A8734  call 0x8A847C                                                    ; doPhase(BURST = 2, obj->getPosition())
008A8739  mov eax,[0xDE412C]; mov eax,[eax+0x40]                           ; TheGameLogic->getFrame()
008A8741  push line 0xAB; push [edi+0x44] (MaxBurstDelay); push [edi+0x40] (MinBurstDelay); call 0x6D328E   ; GameLogicRandomValue
008A8751  m_nextBurstFrame = now + draw
008A8764  x87: fld damping(+0x4C); fsubr [0xBD2C98]=1.0 ; fld gravity(GlobalData +0xC4); fchs ; fmulp ; fmul [0xC693B0]=20.0
008A8786  m_riseVelocity(+0x20 relative to ebx) = (-gravity) * (1 - damping) * 20.0
008A8789  TheTerrainLogic (0xDE4690); 0x68BBE0 = Object::getLayer
008A87AD  call [vtable+0x1C] = getLayerHeight(x, y, layer, 0, 1)
008A87B0  m_targetHeight = the answer
008A87B6  m_currentHeight = 0
008A87BB  m_risePosition = *obj->getPosition()   (three movsd = 12 bytes)
```
The INI's `RubbleHeight` (+0x50) is **not read here or anywhere in the class**: the target is the terrain's height under the piece.

**B. rising (state 1) and a drawable**
```
008A87E3  movss xmm0,[ebx+0x20] (velocity); addss xmm0,[ebx+0x24] (currentHeight); movss [ebx+0x24],xmm0    ; SSE add
008A87FC / 0x8A8822  two random reals in [-maxShudder, +maxShudder] via 0x6D332C                           ; the LOGIC generator
0x8A884A  finalPos.x = draw2 + saved.x (x87, pc24), [0x8A885D] finalPos.y = draw1 + saved.y, [0x8A884D] finalPos.z = saved.z + currentHeight (SSE)
008A8871  call 0x70C201                                                   ; Object::setPosition(&finalPos)
008A8879  cmp now,[ebx+0x18] / jb 0x8A88C7                                ; now < nextBurstFrame -> skip the burst
008A8888  push line 0xCD; push [edi+0x48] (BigBurstFrequency); push 1; call 0x6D328E
008A8895  cmp eax,1 / jne -> push 1 (DELAY) ; else push 2 (BURST) ; call 0x8A847C with obj->getPosition()
008A88AC  push line 0xD6; push [edi+0x44]; push [edi+0x40]; call 0x6D328E ; m_nextBurstFrame += draw
008A88C7  movss xmm0,[ebx+0x28] (target) ; subss xmm0,[ebx+0x34] (saved z) ; movss xmm1,[ebx+0x24] (currentHeight)
008A88D6  comiss xmm1,xmm0 / jb B-end                                     ; currentHeight < target - savedZ -> nothing more
008A88DF  finalPos = (saved.x, saved.y, target) ; call 0x70C201            ; the piece is put on the ground
008A890B  state(+0x1c) = 2 (done)
008A8916  call 0x8A847C                                                   ; doPhase(FINAL = 3, obj->getPosition())
008A891F  call 0x8A841C                                                   ; doRubbleDoneStuff
008A892C  test byte [esi+0x113],8 / je skip ; and byte [esi+0x113],0xF7 ; call 0x68B53C    ; clear POST_RUBBLE (59) and notify the drawable
008A8940  test dword [esi+0x110],0x10000000 / jne skip ; or [esi+0x110],0x10000000 ; call 0x68B53C   ; set POST_COLLAPSE (60) and notify
008A8956  fld [esi+0x44] ; push ; call 0x70C31E                           ; Object::setOrientation(getOrientation())
008A8964  mov ecx,[esi+0x25C] ; call [vtable+0x2C]                        ; the body's updateBodyParticleSystems
008A8976  the drawable's instance matrix is copied, its translation zeroed, call 0x6714C0 (setInstanceMatrix)
008A8A0E  eax = 0x3FFFFFFF ; jmp the epilogue                              ; UPDATE_SLEEP_FOREVER
008A8A15  (the other exits) xor eax,eax ; inc eax                         ; UPDATE_SLEEP_NONE = 1
```

`doRubbleDoneStuff` RW 0x8A841C: lazily `nameToKey` of the string at 0xC0B138 (the neighbouring module name — `StructureCollapseUpdate`), then
`obj->getModule(key)` (0x68BDA5) and, when it exists, `call 0x88AD41` on it: the structure lane's bone-FX stop (already recorded as not ported, S-340).

## 5. doPhase (RW 0x8A847C)

```
008A848A  edi = [ecx+4] = the module data
008A8490  ecx = the phase (first stack argument), target = the second
008A8495  esi = data + 0xC * phase
008A849B  FX first: size = (m_fxs[phase].end - begin)/4 at [esi+0x88] / [esi+0x8C]
008A84AE  count = [edi + phase*4 + 0xC8]  (m_fxCount[phase])
008A84BC  call 0x8A829D(range = size, count, idxList)         ; the LOGIC draws, random(0, size-1), duplicates redrawn
0x8A84C9  per pick: fldz ; push 0 ; push 0 ; push target ; push m_fxs[phase][idx] ; call 0x494615   ; FXList::doFXPos(fx, target, null, 0, null)
008A84F1  OCL after: size at [esi+0x58], count = [edi + phase*4 + 0xB8]
008A850C  call 0x8A829D again
0x8A8518  per pick: ocl = m_ocls[phase][idx]; if (ocl) push 0, 0, target, [eax+8] (the object) ; call 0x5F00CA   ; ObjectCreationList::create
```
The picker 0x8A829D: `dec eax` (range -> range - 1), `push 0x110 / push 0xC69348 / ...` (the `MAX_IDX` local array), a loop drawing
`random(0, range - 1)` through 0x6D328E with `__FILE__` line 272 and rejecting repeats via `inList`; retail loops for ever if `count > range`.

## 6. What the port does with each branch

| RW branch | port |
|---|---|
| registration, the field table, the data ctor defaults | `StructureModules.cpp`: `kRubbleRiseParse`, `RubbleRiseUpdateModuleData` (`m_maxBurstDelay` starts at 0 like the collapse's, `m_minBurstDelay` 9999, counts 1) |
| update A / B arithmetic, draw order, phases | `RubbleRiseUpdate::update` with the same draws through `logic.random()` and the same `SimMath` spellings (SSE vs x87 at 24 bits as retail) |
| the target height | `RubbleRiseUpdate::layerHeightAt`: the object's layer (RW 0x68BBE0, as `EmotionModules`/`GateModules` read it) through the pathfinder's `getLayerHeight` (W3DTerrainLogic vslot 0x1C) |
| the drawable (RW 0x70E013) gate | `Object::clientHooks()` (the port's stand-in for a drawable, lane SMOOTH-1): the rise needs one, as RW's `drawable != null` does |
| the instance matrix shudder / its clearing | not ported (client) — the same call as the collapse's (S-340) |
| `doRubbleDoneStuff` | not ported (S-340, the bone-FX stop) |
| the body's particle systems | not ported (S-346) |
| the FX lists | handed to the client as `FXEvent`s with site `RubbleRise` (as the collapse's FX do), the logic draws made in retail order |
| the OCLs | the draws are made, nothing is created (the OCL lane, S-346) |
| MinRubbleRiseDelay / MaxRubbleRiseDelay | parsed and **unread**: no slot of the class reads +0x38 / +0x3C (update, onDie, reset, doPhase, xfer, both constructors read), the donor's struct leaves 0x38..0x3F unnamed too, and Ennorath sets both to 0 on all 14 objects |
| RubbleHeight (+0x50) | parsed and unread (the target is the terrain's height) |
