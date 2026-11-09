# Port 78: vendor grab (issue #210)

Source: RotWK 2.01 `game.dat` (S-001); DONOR BFME2 0x45108D, `Code/GameEngine/Source/GameLogic/Object/Update/SpecialAbilityUpdateTriggerAbilityEffect.cpp`. Addresses below are RW, checked linearly with `rw.py dis`.

* RW 0x8540A1–0x854115: only ability 0x27 / 0x28, first trigger and a contain; absent target attempts the separate tree path (outside this change).
* RW 0x854115–0x854156 selects CHUNK_VENDOR for 0x28 or ROCK_VENDOR for 0x27. Otherwise RW 0x854353–0x8543B2 retains the normal contain path, including removal from a horde and GRAB_AND_KILL condition.
* RW 0x85415C–0x85418C: a missing body returns; empty GrabObject skips creation but not damage or FX. RW 0x85418E–0x8541F7 looks up the template, creates with null team and zero status, and, on success, sets the grabber as producer and adds the created object to the grabber's contain. No offset is used to place the object.
* RW 0x8541FD–0x854247 constructs DamageInfo, sets source 0, damage type 5 (STRUCTURAL), damage FX override 7, death type 1 (NONE), amount from body slot 0x7C, and invokes Object::doAttemptDamage RW 0x697E50. This still happens when the grab name is empty or creation fails.
* RW 0x85424C–0x85434B copies the grabber's position, fetches the vendor offset through slot 0x80, rotates it by the grabber's matrix Z angle (RW 0xB261F0 `fpatan`, rounded to float at RW 0x85428A) and adds it to that position. It passes the result as both positions, the grabber drawable transform, and 100.0 (constant RW 0xBD88D8) to FXList::doFXPos RW 0x494615, with GrabFX from body slot 0x78. This is an FX position, not a spawn position. RW 0x494615 and 0x5E2254–0x5E2268 forward all arguments to the nuggets. The existing POSITION_FX event drops the secondary position and speed; per owner decision this remains a documented stop, not an FXEvents change.
* ActiveBody data parse table RW 0xC71D68 holds GrabObject +0x2C, GrabFX +0x38, GrabDamage +0x3C and GrabOffset +0x40. Create data RW 0x651186 and create module RW 0x65114B lead to constructors RW 0x8C373F and 0x8C3841. Body interface vtable RW 0xC720F0 slots 0x74/0x78/0x7C/0x80 resolve to RW 0x8C281E/0x8C1DB4/0x8C1DBB/0x8C1DC2, reading those four fields. The caller is RW 0x854170/0x854342/0x85422B/0x854279, not merely the getters.

Second trigger and tree behavior are existing separate paths and are not changed by this issue. No new simulation state is required by this branch: creation and containment use the existing object and contain state.

## Ennorath check boundary

In the supplied Ennorath 1.9.8.2 INI snapshot, a search for `SPECIAL_GRAB_CHUNK` found no occurrence; the examined `RohanEntBase` has no grab special-power module. Its grab animation names alone therefore do not establish the issue's proposed Ent/building playtest. A separate ROCK_VENDOR path exists in the data alongside a troll with `SPECIAL_GRAB_PASSENGER`, but accessibility of that exact pair in the game's cheat panel and the full action sequence have not yet been established. Do not claim an Ent/building in-game check without resolving this; synthetic tests exercise the RW branch independently.
