# PORT-131 (issue #333): `Object/GeometryOther` and `Object/GeometryFrontAngle` — RW analysis

## Verdict

**Stop (no reachable runtime reader in RotWK 2.01 for either field).**

- **`Object/GeometryOther`** (`ThingTemplate + 0xFC`, `Object + 0x108`): parsed into `ThingTemplate::m_geometryOther` (`std::vector<GeometryInfo>`) and copied on template assignment, never into `ThingTemplate::m_geometryInfo` (`+0xA0`). Its sole object-level accessor `Object::getGeometryInfo(int)` (`RW 0x6915AF`) has one caller (`Drawable::getGeometryInfo(int)` `RW 0x671339` at `0x671345`), which itself has **0 callers**; all template vector accessors (`RW 0x68B016`, `0xB2DED0`, `0x68AF05`, `0x68FD60`, `0x68CC85`) have **0 callers**, and `SquishCollide::onCollide` (`RW 0x8BFBAE`) reads `Object + 0xA8` (`m_geometryInfo`) only.
- **`Object/GeometryFrontAngle`** (`ThingTemplate + 0x538`, BFME2 `+0x528`): parsed by `INI::parseAngleReal` (`RW 0x42EE15`), defaulted to `0.0f` (`RW 0x73FE1E` -> `0x73FEA7`), copied in `ThingTemplate::operator=` (`RW 0x6D22A7..0x6D22AD`). Its standalone getter `RW 0x86BB4E` has **0 callers**, and its sole `.text` reader (`RW 0x86CC3F` inside `HordeContainInterface::slot 130` = `+0x208`, `RW 0x86CA78`) is dead legacy code with **0 direct callers and 0 virtual callers** in `RotWK/game.dat` (mirroring **0 callers** of `HordeContain::rva00469967` / `slot130` in `Open-BFME-2`).

Per issue #333 ("If RotWK has none (parser, constructor and copies are the whole access set): that is the result. Land the verdict as a STOPS row and a short analysis file, with no getter, no member and no test for a field nobody reads"): no engine code, getter, member, or synthetic test is added. Registered as `S-7319` in `docs/STOPS.md`.

Binary checked: RotWK 2.01 `game.dat` (MD5 `daed3668006cd90f01c34e5a7da1901f`, caveat S-001). Donor cross-check: `Open-BFME-2` (BFME2 1.06).

---

## 1. `Object/GeometryOther` (`ThingTemplate + 0xFC`, `Object + 0x108`)

### Parse rows and parser (`RW 0xAD4310`)

- Object field table `RW 0xDA3DF8`, row 104 at `RW 0xDA4478`: name `RW 0xC257A0` (`"GeometryOther"`), parser `RW 0xAD4310`, `userData = 0`, offset `+0xFC` (`std::vector<GeometryInfo>`, element size `0x5C`).
- `ObjectReskin` field table row `RW 0xC252A8`: same name, parser `RW 0xAD4310`, offset `+0xFC`.
- Distinct from `Geometry` (`RW 0xDA43DC`, parser `RW 0xAD4040`) and `AdditionalGeometry` (`RW 0xDA43EC`, parser `RW 0xAD3BC0`), both of which populate `ThingTemplate + 0xA0` (`m_geometryInfo`).
- `RW 0xAD4310..0xAD4503` (`GeometryInfo::parseGeometryOther`):
  1. `0xAD4316..0xAD432D`: constructs stack temporary `GeometryInfo(GEOMETRY_SPHERE, false, 10.0f, 10.0f, 10.0f)` via `RW 0x450429`.
  2. `0xAD4332..0xAD449B`: loops over colon-delimited sub-tokens (`RW 0x42DBF5`), reading `GeomType` (`0xD00698`, `scanIndexList` `0x42B999`), `IsSmall` (`0xD00690`, `scanBool` `0x42CE52`), `Height` (`0xBF3598`), `MajorRadius` (`0xD00684`), `MinorRadius` (`0xD00678`), `OffsetX` (`0xC82A28`), `OffsetY` (`0xC82A20`), `OffsetZ` (`0xC82A18`) (`scanReal` `0x42EAAD`).
  3. `0xAD44A2..0xAD44F8`: calls `GeometryInfo::calcBoundingStuff` (`RW 0xAD2860`) on the stack temporary and appends it to `std::vector<GeometryInfo>` at `store` (`ThingTemplate + 0xFC`) via `RW 0x4923F0` / `RW 0xAD3E90` / `RW 0x4503DF`.

### Template, Object, and Drawable lifecycle

| Site | Instruction / Call | Role |
|---|---|---|
| `ThingTemplate` ctor | `RW 0x73FC7A` (`lea ecx,[ebx+0xFC]`; `call 0x478CBE`) | Initializes empty vector `+0xFC` |
| `ThingTemplate` dtor | `RW 0x73F90A` (`lea ecx,[esi+0xFC]`; `call 0x73DDE6`) | Destroys vector `+0xFC` |
| `ThingTemplate::copyFrom` / `operator=` | `RW 0x7405FD -> 0x6D1D80` (`0x6D1ED8..0x6D1EE5 -> 0x6D1BE4`) | Copies vector `+0xFC` |
| `ThingTemplate::validate` | `RW 0x73CE7E` (`0x73CE93 -> 0xAD22C0`) | Checks `+0xA0` only, not `+0xFC` |
| `Object::Object` | `RW 0x6999E4..0x699A1E` | Copies `tt + 0xA0` to `Object + 0xA8`, sets `Object + 0x104 = &this->m_geometryInfo` (`+0xA8`), sets `Object + 0x108 = &tt->m_geometryOther` (`+0xFC`) |
| `Object::xfer` | `RW 0x698xxx` | No access to `+0x108` or `+0xFC` |
| `Object::getGeometryInfo(int)` | `RW 0x6915AF..0x6915E1` | Bounds-checks `index` against `[ecx + 0x108]`, returns element or `&this->m_geometryInfo` (`+0xA8`). **Only 1 caller**: `RW 0x671345` |
| `Drawable::getGeometryInfo(int)` | `RW 0x671339..0x67134F` | Forwards `[ecx + 0xFC]` (`Drawable::getObject()`) to `0x6915AF`. **0 callers in `game.dat`** |
| Vector / template getters | `RW 0x68B016`, `0xB2DED0`, `0x68AF05`, `0x68FD60`, `0x68CC85` | **0 callers in `game.dat`** |

### Exhaustive scan and cleared triage leads

- `SquishCollide::onCollide` (`RW 0x8BFBAE`, where Ennorath `mumakil.ini` comments `; Extra secondary geometry to be used for crushing/squishing detection purposes`): at `RW 0x8BFDE2..0x8BFE4C`, copies `self + 0xA8` (`m_geometryInfo`) and tests `other + 0xA8` via `GeometryInfo::collidesWith` (`RW 0xAD2CE0`); neither `+0x108` nor `+0xFC` is read.
- Triage `batch-7.md` row 17 stopped at `RW 0xAD2860` and `RW 0x936B7D`. Both are unrelated to `+0xFC`:
  - `RW 0xAD2860` is `GeometryInfo::calcBoundingStuff` iterating `GeometryInfo`'s own shapes vector `[esi+0x2C..+0x30]` (called at `RW 0xAD44A6` during parse and by `m_geometryInfo` mutators).
  - `RW 0x936B7D` (`RW 0x936C31`: `test byte ptr [eax+0x108], 0x80`) loads `eax = [edi+4]` (`Object::m_template` at `RW 0x936C2C`) and reads `ThingTemplate + 0x108` (`KindOf` bitmask dword 0), not `Object + 0x108`.
- Whole-`.text` displacement scans for `[reg+0xFC]` and `[reg+0x108]` find no other `ThingTemplate`/`Object` `GeometryOther` readers (`0x68A064`/`0x68A2A6` read `Drawable + 0xFC` = `m_object`; `0x68DE3F` divides by `[eax+0xFC]` on module `0xDE4704`; `0x699238`/`0x69A98F` manage `Object + 0x104` = `m_geometryInfo*`).
- **Donor BFME2 1.06 (`Open-BFME-2`)**: `GeometryParseOther.cpp` (`parseGeometryOther`), `ThingTemplate.h:119`, `ThingTemplateConstructor.cpp:164,330`, `ThingTemplateFieldTable.cpp:148`; `SquishCollide.cpp:405` calls `getGeometryInfo()` (`+0xA8`); whole-tree `git grep` finds **0 callers** of `m_geometryOther` or indexed `getGeometryInfo(int)`.

---

## 2. `Object/GeometryFrontAngle` (`ThingTemplate + 0x538`)

### Parse row, constructor, copies, and scan of `+0x538`

- Object field table `RW 0xDA3DF8`, row 171 at `RW 0xDA48A8`: name `RW 0xC253B8` (`"GeometryFrontAngle"`, sole reference in the image), parser `RW 0x42EE15` (`INI::parseAngleReal`, multiplies scanned float by `RADS_PER_DEGREE` `0.017453292f` at `RW 0xBD1900`), `userData = 0`, offset `+0x538` (absent from reskin table `0xC252xx`).
- `ThingTemplate` ctor: `RW 0x73FE1E` (`xorps xmm0, xmm0`) -> `RW 0x73FEA7` (`movss dword ptr [ebx + 0x538], xmm0`), default `0.0f`.
- `ThingTemplate::copyFrom` / `operator=`: `RW 0x6D22A7..0x6D22AD` copies `[eax + 0x538]` to `[ebx + 0x538]`.
- Standalone getter: `RW 0x86BB4E` (`fld dword ptr [ecx + 0x538]; ret`) has **0 callers** (`E8`/`E9`/abs32).
- Every `38 05 00 00` byte sequence in `.text` (15 total hits) was disassembled:
  - 14 hits are unrelated (`0x411FF8` jump `je 0x412536`, `0xB5E027` stack `[esp+0x538]`, `0x8491ED` class at `0x847ED0` with `+0x6a4..+0x6b4`, `0x6A76CA`/`0x6A7703`, `0x7D5BC2`, `0x8BE7D2`, `0xAD6043`/`0xAD6081`/`0xAD6104`, `0xAD8C78`, etc.).
  - The **sole** `.text` instruction reading `ThingTemplate + 0x538` is `RW 0x86CC3F` inside `RW 0x86CA78`.

### Dead reader: `HordeContainInterface::slot 130` (`+0x208`, `RW 0x86CA78..0x86CC83`)

- `RW 0x86CA78` occupies slot 130 (`+0x208`) of `HordeContainInterface` in three vtables: `RW 0xC5B1F8` (`HordeContain`, slot at `0xC5B400`), `RW 0xC5BE98` (`HorseHordeContain`, slot at `0xC5C0A0`), and `RW 0xC5CFA8` (`AODHordeContain`, slot at `0xC5D1B0`), overriding pure virtual `0x43B160` in base `0xC5AE48`.
- Function behavior (matching `Open-BFME-2` `HordeContain::rva00469967` in `HordeContainIface11CSlots.cpp:1615-1663`, noted there as derived from BFME1 slot 118 `0x00239E00`):
  - Checks if horde object (`[ecx - 0x114]`) is on a wall (`0x6EA857`) or ramp (`0x6E89EE`);
  - Searches within `150.0f` (`0xC041F8`) for closest object of `KindOf` `0x3C` (`WALK_ON_TOP_OF_WALL` `0xDA0F58`);
  - If wall has `SiegeDockingBehavior` with dock points (`0x85D1F9`), faces away from closest dock point (`0x85C94A`/`0x85C987`);
  - Else if not on a ramp (`RW 0x86CC36..0x86CC55`), sets orientation (`0x70C31E`) to `wall->m_orientation + wall->m_template->m_geometryFrontAngle` (`[ebx+0x44] + [[ebx+4]+0x538]`);
  - Sets `m_120 = 1` (`0x86CC60`) and wakes module (`0x850C32` at `0x86CC70`).
- **Callers in `RotWK/game.dat`: 0.**
  - **Direct / table refs**: `RW 0x86CA78` has 0 `E8`/`E9` callers and only the 3 vtable slot pointers (`0xC5B400`, `0xC5C0A0`, `0xC5D1B0`).
  - **Virtual `+0x208` calls**: all 220 occurrences of `08 02 00 00` in `.text` (including all `FF ?? 08 02 00 00` and SIB indirect calls, plus all 10 `mov reg, [reg + 0x208]` in `0x5A0000..0xB90000`) were checked; none dispatches `HordeContainInterface::slot 130` (`AIUpdateInterface` `0x8751E1`/`0x8751E9`, client/UI `0x466128` `0xBDBD44`, `0x574FD1..0x5759A1` `0xBEAA0C`, and `[0xDC78EC]` `0x601EBF` all belong to other classes). Neighboring slots `127..129` and `131..133` (`+0x1FC..+0x204`, `+0x20C..+0x214`) are called from AI/update sites (`0x748F6C`, `0x74C162`, `0x752664`, `0x756414`, `0x8751B0`), while `+0x208` is never called.
  - **Donor BFME2 1.06 (`Open-BFME-2`)**: `HordeContain::rva00469967` / `slot130` likewise has **0 callers** across the entire donor tree.

---

## 3. Port state and Ennorath data

- **Ennorath 1.9.8.2**: `GeometryOther` has 16 definitions across 8 INI files (two `GeomType:BOX ... OffsetX:32/-35` lines on `MordorMumakil`, `MordorGreatBeast`, `HaradOliphant`, etc.); `GeometryFrontAngle` has 15 definitions across 3 INI files (`civilianprop.ini`, `helmsdeepbuildings.ini`, `campsandcastles.ini`, values `-90.0`/`90.0`/`-110.0`/`-130.0`/`180.0`).
- **Port**: `ThingFactory` (`engine/data/rotwk-201/field-tables.json` + `engine/src/Common/Thing/ThingFactory.cpp`) already parses `GeometryFrontAngle` via `S_AngleReal` (`0x42EE15`) and retains `GeometryOther` (`0xAD4310`) as `S_Raw` (`RawTokens`, counted in `S-072`'s 101 raw object-table rows so `test_object_model.cpp` stays intact and `ObjectGeometry::shapesOf` stays free of `GeometryOther`). Both entries remain `S_Raw` in `Bfme1ObjectFields.inc` for BFME1 mode.
- **No behavioral test or code added**: per issue #333, neither field is read by RotWK 2.01, so inventing a reader (e.g. pushing `GeometryOther` into `ObjectGeometry::shapesOf` / `SquishCollide` or calling `slot 130` for `GeometryFrontAngle`) would diverge from retail RotWK 2.01 and BFME2 1.06.

## 4. INFERENCE / scan limits

Static analysis against `workspace/RotWK/game.dat` (caveat S-001) and `Open-BFME-2`. Dynamic runtime hooks/DLLs outside the static image, or indirect vtable indexing that computes `slot 130` (`+0x208`) without literal `0x208` displacement bytes, were not traced dynamically.
