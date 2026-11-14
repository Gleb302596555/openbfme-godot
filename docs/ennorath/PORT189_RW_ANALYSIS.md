# PORT-189 (issue #455): Hovered Toggle Command Button Tooltip Refresh

Target binary: RotWK 2.01 `game.dat` (11,346,944 bytes, md5 `daed3668006cd90f01c34e5a7da1901f`).
Donor reference: BFME2 1.06 decomp (`Open-BFME/Open-BFME-2`, `Code/GameEngine/Source/GameClient/GUI/ControlBar/...` and `Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/...`).

## Problem

When the player hovers a toggle command button on the Palantir or side bar (for example `Command_ToggleTrebAutoAcquire` / `Command_ToggleNoAutoAcquire`, `TOGGLE_WEAPONSET`, `HORDE_TOGGLE_FORMATION`, or `SPECIAL_POWER` with `TOGGLE_IMAGE_ON_WEAPONSET`) and clicks it without moving the mouse off the button, the button icon flips immediately but the tooltip (`InGameHelpBox`) stays frozen on the old name and description until the pointer leaves and re-enters the button.

## Ennorath 1.9.8.2 Scope

All 92 multi-label `CommandButton` blocks in Ennorath 1.9.8.2 `data/ini/commandbutton.ini`:
- `HORDE_TOGGLE_FORMATION`: 68 buttons (all `InPalantir = Yes`)
- `TOGGLE_WEAPONSET`: 14 buttons (all `InPalantir = Yes`)
- `SPECIAL_POWER`: 4 buttons (`TOGGLE_IMAGE_ON_WEAPONSET`, all `InPalantir = Yes`)
- `TOGGLE_NO_AUTO_ACQUIRE`: 3 buttons (`Options = TOGGLE_IMAGE_ON_WEAPON ...`, all `InPalantir = Yes`)
- `TOGGLE_STANCE`: 2 buttons (all `InPalantir = Yes`; uses stance radial submenu)
- `OBJECT_UPGRADE`: 1 button (`Command_PurchaseUpgradeMordorFortressMorgulSorcery`, `InPalantir = Yes`, `Radial = Yes`)

## RotWK 2.01 Mechanism (Two Halves)

### 1. Palantir/Side-Bar Click Drops the ControlBar Help Provider

- `OldSchoolHelpBoxContentSource` (vtable `RW 0xC4EE08`):
  - vslot 2 (`RW 0x8075E4`): returns `window->+0x1C8` (`TOOLTIPDELAY = 1` ms on `ControlBar.wnd:ButtonCommand`).
  - vslot 3 (`RW 0x807A81`): composes `CommandButtonHelp`, reading `CommandButton::getDescriptLabel()` (`RW 0x75CECD`) at `RW 0x807D9C` and `CommandButton::getTextLabel()` (`RW 0x75CE47`) at `RW 0x807EB3`, both indexed by `CommandButton +0xFC` (`m_toggleIndex`).
  - vslot 4 (`RW 0x8075F5`, equality check used by `ControlBar::showHelp` at `RW 0x8078CA`): compares ONLY `window` (`+0x08`) and `button` (`+0x0C`) — it does **not** compare `CommandButton +0xFC`.
- `ControlBar::hideHelp` (`RW 0x8076CD`, thunk `RW 0x8079BB`):
  - clears `ControlBar +0x21C` (the per-frame hover mark),
  - releases `ControlBar +0x2AC` (the active help provider, via `RW 0xA0CB16`),
  - calls `ThePalantir->hideHelpSoon()` (`RW 0x6D473D` -> `InGameHelpBoxMovieClip::hideSoon` `RW 0x92E229`: state `3 -> 4`, countdown = 5).
- Caller closure scan over all of `.text` and `.rdata`/`.data` in `game.dat`:
  - Only two callers of `RW 0x8076CD`: `RW 0x7BC5D9` (`DISABLE_INPUT`) and thunk `RW 0x8079BB`.
  - Only two callers of `RW 0x8079BB` (zero vtable/data references):
    - `RW 0x9D282D` inside `ParasiticInGameCommandButton::Impl` left-click handler (`RW 0x9D2814`, gated by enabled/ready check `RW 0x9D285E`), immediately before sending `GBM_SELECTED` (`0x4008`) to `ControlBar`;
    - `RW 0x9D28A3` inside `ParasiticInGameCommandButton::Impl` right-click handler (`RW 0x9D288A`, gated by enabled/cancelable check `RW 0x9D28D4`), immediately before sending `GBM_SELECTED_RIGHT` (`0x4009`) to `ControlBar`.
  - Donor match: BFME2 1.06 `Rva005C33DDWindowNotification.cpp` (`ControlBarSystem.cpp` vtable `0xC8C3D4` / `0xC8C438`).
  - All 4 `ParasiticInGameCommandButton` construction sites (`RW 0x9D2AED`) are Palantir arc buttons (`RW 0x93000E`, `InGameCommandButton`) and side-bar buttons (`RW 0x92F165`, `SideBarCommandButton`).
  - `InGameToggleStanceCommandButton` (vtable `RW 0xC84268`) overrides left click to `RW 0x96FAC6` (opens/closes stance radial submenu; hovering stance items replaces the provider via `bar = 5`, `RW 0x96F4FA`, and clicking sends `MSG_CHANGE_STANCE`, `RW 0x96F53C`).
  - Hotkey dispatch (`RW 0x944498`) and radial menu click (`RW 0x72A027 -> 0x7298A3`) do **not** call `RW 0x8079BB` / `RW 0x8076CD`.
- After `hideHelp()` drops the provider on click, the pointer is still over the same button:
  - Next HUD hover frame: `ControlBar::showHelp` (`RW 0x807848`) sees no provider (`+0x2AC == nullptr`), installs the provider fresh (`RW 0x807914..0x80795B`), and resets hover start time.
  - Once `now > hoverStart + 1 ms`: `showHelp` calls vslot 3 (`RW 0x807A81`), composes the **new** text and description from the updated `+0xFC`, and calls `InGameHelpBoxMovieClip::setHelp` (`RW 0x92E570 -> 0x92E362`: hides state `4 -> 2`, stores new help), which `Update` (`RW 0x92E462`) shows immediately (`2 -> 3`).

### 2. Optimistic Click Toggle Index Persistence Across HUD Frames

- `ControlBar::processCommandUI` (`RW 0x940435`) updates `CommandButton +0xFC` optimistically on click (`invert = true` via `RW 0x75D22E` / `RW 0x75D366`):
  - `GUI_COMMAND_TOGGLE_NO_AUTO_ACQUIRE` (`0x30`) and `GUI_COMMAND_TOGGLE_WEAPON` (`0x23`): `RW 0x941367`
  - `GUI_COMMAND_SPECIAL_POWER` (`0x18`): `RW 0x9414E7`
  - `GUI_COMMAND_TOGGLE_WEAPONSET` (`0x2D`) and `GUI_COMMAND_HORDE_TOGGLE_FORMATION` (`0x24`): `RW 0x941A22`
- `ControlBar::processContextSensitiveButtonClick` (`RW 0x941C14..0x941C1D`) immediately updates the button gadget image from `+0xFC`.
- `ControlBar::update` (`RW 0x71FC08`) guards `evaluateContextUI` / `populateCommand` (`RW 0x943E91`, `RW 0x9446A5`) behind `m_UIDirty` (`+0x28`, checked at `RW 0x71FD81`). Because `m_UIDirty` stays `0` between click and logic execution (and across `SPECIAL_POWER` `UnpackTime` frames), RotWK does **not** overwrite the click-predicted `+0xFC` back to the pre-click object state on intermediate 60+ FPS HUD frames.

## Port Changes

1. `engine/src/GameClient/InGameHelpBox.{h,cpp}`: `dropProvider()` also clears `m_provider = Provider()` (matching `RW 0x8076DA -> 0xA0CB16`).
2. `engine/src/GameClient/InGameHud.{h,cpp}`: added `InGameHud::dispatchPalantirButtonPress` and `InGameHud::dispatchPalantirRightButtonPress` which call `helpBox.dropProvider()` before `bar.pressButton` / `bar.rightPressButton`, and wired `m_palantirRef->setButtonHandler` and `setRightButtonHandler` through them. `AptPalantir` already enforces the `RW 0x9D285E` / `0x9D28D4` enabled/cancelable click gates and `TOGGLE_STANCE` (`RW 0x96FAC6`) routing.
3. `engine/src/GameClient/ControlBar.{h,cpp}` and `engine/src/GameClient/ControlBarCommandProcessing.cpp`: added `applyPredictedToggle` + client-only `m_pendingToggles` (`{ preClickIndex, predictedIndex }`) and `m_pendingSelection` so clicking any of the toggle commands (`TOGGLE_WEAPONSET`, `HORDE_TOGGLE_FORMATION`, `TOGGLE_NO_AUTO_ACQUIRE`, `SPECIAL_POWER`) refreshes cached `ControlBarButton` image/labels immediately and preserves the predicted `+0xFC` across 60+ FPS `ControlBar::update()` frames until the authoritative object state leaves `preClickIndex` or selection / command set / `m_dirty` changes.
4. `engine/tests/test_port_189.cpp` (`engine/cmake/Port189.cmake`, wired in `engine/CMakeLists.txt`): synthetic-INI tests verifying auto-acquire toggle (Palantir + side bar, both before and after logic ticks), weaponset / formation / special-power-with-UnpackTime toggles, right-click provider drop, and adjacent guards (steady hover, hotkey press without provider drop, selection change, stance submenu).

## Remaining Scope Note

- `GUI_COMMAND_TOGGLE_WEAPON` (`0x23`) button-click logic remains tracked under `S-290` (`ControlBarCommandProcessing.cpp:556`); its Palantir click still passes through `dispatchPalantirButtonPress` (`dropProvider()`).
