# PORT-188 (issue #454): `SpecialAbilityUpdate::initiateIntentToDoSpecialPower` Sibling-Abort Check (`P-115`)

Target binary: RotWK 2.01 `game.dat` (11,346,944 bytes, MD5 `daed3668006cd90f01c34e5a7da1901f`).
Donor reference: BFME2 1.06 decomp (`Open-BFME/Open-BFME-2`, `src/gameengine/gamelogic/module/SpecialAbilityUpdateInitiateIntent.cpp`, `0x004500A3`).
Mod data: Ennorath 1.9.8.2 (`data/ini/includes/commandbutton.inc`, `data/ini/object/goodfaction/units/rohan/rohanradagast.ini`, `data/ini/includes/objectcreationlist.ini`, `data/ini/object/goodfaction/projectile.ini`, `data/ini/includes/attributemodifier.inc`).

## Symptom (`P-115`)

When Radagast in eagle form (`RohanRadagastaufdemAdler`) casts his ultimate ("Word of Poultry", `Command_SpecialAbilityRadagastConvertEnemyAdler`), the voice/FX plays and the button becomes disabled, yet no enemy infantry in the target radius is replaced by animals and the 360 s special-power recharge never starts. On foot (`RohanRadagast`, `Command_SpecialAbilityRadagastConvertEnemy`), the same ultimate works normally.

## RotWK 2.01 Disassembly & BFME2 1.06 Donor

### 1. `SpecialAbilityUpdate::initiateIntentToDoSpecialPower` (`RW 0x852E8F .. 0x8530AE`)

After setting `m_withinRange = false` (`RW 0x852EE8`: `mov byte ptr [esi + 0x99], bl` with `ebx == 0`), storing `m_targetPos` / `m_targetID`, calling `wakeUpIfSleeping` (`RW 0x853057`), and setting `m_active = true` (`RW 0x85305E`: `mov byte ptr [esi + 0x9A], 1`), `SpecialAbilityUpdate` iterates the owner object's `BehaviorModule *` array (`obj + 0x22C`, `RW 0x853062 .. 0x8530A4`):

- `RW 0x853069 .. 0x853076`: `sp = (*mod)->getSpecialPowerUpdateInterface()` (`vtable + 0x64`); skip if `sp == nullptr`.
- `RW 0x85307A .. 0x853083`: `sp->isSpecialAbility()` (`sp vtable + 0x04`, `RW 0x8BD372` -> `1`); skip if `false`.
- `RW 0x853085 .. 0x853092`: adjust `sp` (`this + 0x20`) back to `BehaviorModule` (`add edi, -0x20`), reload `ecx = edi + 0x20`, and call `sp->isPowerCurrentlyInUse(nullptr)` (`push ebx` where `ebx == 0`, `call [eax + 0x18]`); skip if `false`.
- `RW 0x853094 .. 0x85309F`: if `edi != esi` (`other != this`), call `other->finishAbility(false, true)` (`push 1; push ebx; mov ecx, edi; call 0x854325`).

BFME2 1.06 donor (`SpecialAbilityUpdateInitiateIntent.cpp`, `0x004500A3`) has the identical loop (`isSpecialAbility()` -> `isPowerCurrentlyInUse(nullptr)` -> `if (other != this) other->finishAbility(0, 1)`).

### 2. `SpecialAbilityUpdate::isPowerCurrentlyInUse(const CommandButton *)` (`RW 0x851C92`)

All 11 `SpecialPowerUpdateInterface` vtables in `RotWK/game.dat` whose slot `+0x1C` is `SpecialAbilityUpdate::getSpecialPowerTemplate` (`0x851E83`) — base `0xC63E4C` and all 10 subclasses (`0xC64658`, `0xC6475C`, `0xC647E4`, `0xC6490C`, `0xC65244`, `0xC67B28`, `0xC6BCB8`, `0xC6BF00`, `0xC74B70`, `0xC769C8`) — share `+0x04 = 0x8BD372` (`isSpecialAbility`), `+0x08 = 0x9D4099` (`isActive`, returning `m_active` at `module + 0x9A`), and `+0x18 = 0x851C92` (`isPowerCurrentlyInUse`, no subclass overrides):

- When `cmd == nullptr` (`RW 0x851C9A .. 0x851CE8` skips the `NEED_SPECIAL_POWER_SCIENCE` / `CONTEXTMODE_COMMAND` branches):
  - If `m_packingState == STATE_NONE` (`0` at `module + 0x6C`), returns `false` (`RW 0x851CE8 .. 0x851CEF`).
  - If `SpecialPowerType` is `0x1D` (`SPECIAL_INFANTRY_CAPTURE_BUILDING`) or `0x1A` (`SPECIAL_BLACKLOTUS_CAPTURE_BUILDING`), returns `true` (`RW 0x851CFD .. 0x851D0A`).
  - Otherwise returns `m_withinRange` (`module + 0x99`, `RW 0x851D0E`).

### 3. OpenBFME Divergence

`SpecialAbilityModules.cpp:614` checked `other->isActive()` (`sp vtable + 0x08`) instead of `other->isPowerCurrentlyInUse()` (`sp vtable + 0x18`). Because `initiateIntentToDoSpecialPower` sets `m_active = true` immediately while leaving `m_packingState = STATE_NONE` and `m_withinRange = false` until `SpecialAbilityUpdate::update()` reaches `StartAbilityRange` and begins unpacking, OpenBFME prematurely aborted any sibling `SpecialAbilityUpdate` initiated earlier on the same frame before it was actually in use.

## Ennorath 1.9.8.2 Data Chain

1. **Foot vs. Eagle CommandButton (`commandbutton.inc:2428 .. 2456`)**:
   - Foot Radagast's `Command_SpecialAbilityRadagastConvertEnemy` has **no** `CommandTrigger`.
   - Eagle Radagast's `Command_SpecialAbilityRadagastConvertEnemyAdler` (`SpecialPower = SpecialAbilityRadagastWordOfPoultry`, `Options = NEED_TARGET_POS CONTEXTMODE_COMMAND`, `DisableOnModelCondition = USER_50`) adds `CommandTrigger = Command_SpecialAbilityConvertEnemyTracker` (`SpecialPower = SpecialAbilityActivateeDummy`, `ReloadTime = 1`, `Options = 0`).
2. **Same-Frame Dual Dispatch (`CommandXlat.cpp:1264 .. 1344`)**:
   - Clicking the ground or an enemy with `Command_SpecialAbilityRadagastConvertEnemyAdler` emits two `MSG_DO_SPECIAL_POWER_AT_LOCATION` messages in the same frame: first `SpecialAbilityRadagastWordOfPoultry`, then `SpecialAbilityActivateeDummy`.
3. **Six-Module Setup on `RohanRadagastaufdemAdler` (`rohanradagast.ini:2299 .. 2352`)**:
   - `ModuleTag_CreateAHeroWordOfPoultryStarter_Level2` (`SpecialPowerModule`, `SpecialAbilityRadagastWordOfPoultry`, `ReloadTime = 360000`, `UpdateModuleStartsAttack = Yes`).
   - `ModuleTag_WordOfPoultryUpdate_Level2` (`ReplaceObjectUpdate`, `SpecialAbilityRadagastWordOfPoultry`, `UnpackTime = 800`, `PreparationTime = 1`, `PackTime = 100`, `StartAbilityRange = 300`, `MustFinishAbility = Yes`).
   - `ModuleTag_ConvertEnemyStarter` (`SpecialPowerModule`, `SpecialAbilityActivateeDummy`, `UpdateModuleStartsAttack = Yes`).
   - `ModuleTag_ConvertEnemyDummyMover` (`ActivateModuleSpecialPower`, `SpecialAbilityActivateeDummy`, `StartAbilityRange = 300.0`, `UnpackTime = 0`, `TriggerSpecialPower = ModuleTag_ConvertEnemyShadowTrigger OBJECTPOS`).
   - `ModuleTag_ConvertEnemyTwoStarter` (`SpecialPowerModule`, `SpecialAbilityActivateeDummy3`, `UpdateModuleStartsAttack = Yes`) and `ModuleTag_ConvertEnemyShadowTrigger` (`OCLSpecialPower`, `SpecialAbilityActivateeDummy3`, `IdleWhenStartingPower = Yes`, `OCL = OCL_DrogothShadowRad` -> `RohanRadagastNoUseAbility`, `Lifetime = 55000` + `RadagastConvertFixer` `Duration = 3000` -> `ModelCondition = USER_50` for ~58 s).
4. **Pre-Fix Failure vs. Post-Fix Behavior**:
   - **Pre-fix**: Message 1 activated `ModuleTag_WordOfPoultryUpdate_Level2` (`m_active = true`, `m_packingState = STATE_NONE`, `m_withinRange = false`). Message 2 immediately ran `ModuleTag_ConvertEnemyDummyMover::initiateIntentToDoSpecialPower`, saw `other->isActive() == true`, and called `finishAbility(false, true)` on `WordOfPoultryUpdate_Level2` before frame 0. Only `ConvertEnemyDummyMover` executed, spawning `RohanRadagastNoUseAbility` (`USER_50` -> button `Restricted`) while `WordOfPoultryUpdate_Level2` never unpacked, replaced no enemies, and never started its 360 s recharge.
   - **Post-fix**: `WordOfPoultryUpdate_Level2->isPowerCurrentlyInUse()` is `false` during same-frame initiation, so both sibling updates run: `RohanRadagastaufdemAdler` flies into `StartAbilityRange = 300` if needed, unpacks (800 ms), replaces enemies within `ReplaceRadius = 140` (`CREATE_A_HERO_WORD_OF_POULTRY_RADIUS_L2`), and starts the 360 s (`1800`-frame) recharge. Note on mod UI: while `RohanRadagastNoUseAbility` + `RadagastConvertFixer` apply `USER_50` for the first ~58 s, `DisableOnModelCondition = USER_50` renders the button `Restricted`; once `USER_50` expires, the remaining ~302 s of the 360 s recharge are visible as `NotReady` (`~16% -> 100%`) until `Enabled`.

## Neighbor Audit

- `Object::abortAbilitiesInUse` (`Object.cpp:1005`, `RW 0x693919`): already checks `sp->isSpecialAbility() && sp->isPowerCurrentlyInUse()`.
- `Object::hasActiveSpecialPower` (`Object.cpp:1016`, `RW 0x8D2180`): checks `sp->isActive()` (`+0x08`) per retail.
- `SpecialPowerModule::initiate` (`SpecialPowerModules.cpp:1049`, `RW 0x897E87`): calls `sp->initiateIntentToDoSpecialPower(...)` per retail.
- `CommandButtonHuntUpdate::updateHunt` (`CommandButtonHuntUpdate.cpp:183`, `RW 0x85EE8F`): checks `sp->isActive()` (`+0x08`) per retail.
- All 24 non-`PURCHASE_SCIENCE` `CommandTrigger` buttons in Ennorath 1.9.8.2 audited; none rely on aborting a non-in-use sibling `SpecialAbilityUpdate` on the same frame.
