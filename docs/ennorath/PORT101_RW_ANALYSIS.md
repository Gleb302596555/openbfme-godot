# PORT-101: `Object/SelectionPriority` — RotWK 2.01 RW analysis (issue #261)

Verdict: **reachable runtime consumer verified in RotWK 2.01**. `ThingTemplate + 0x64C` (`SelectionPriority`) is parsed as an integer (not an enum), defaults to `0x7FFFFFFF` (`INT_MAX`), is copied by `ThingTemplate::copyFrom`, and has **one** live runtime reader: `addDrawableToList` (`RW 0x7108F2`), the callback passed by `SelectionTranslator::leftClick` (`RW 0x83C340`) to `W3DView::iterateDrawablesInRegion` (`RW 0x485B80`) / `RW 0x6A0A5E`.

All addresses below are from retail RotWK 2.01 `game.dat` (md5 `daed3668006cd90f01c34e5a7da1901f`), disassembled directly with `tools/mod/rw.py` / `capstone` + `pefile`.

---

## 1. Field table, type, default, and inheritance (`ThingTemplate + 0x64C`)

- **Object field table row (`RW 0xDA49D8`)**:
  - Name string pointer: `RW 0xC27300` -> `"SelectionPriority"`
  - Parser: `RW 0x42EC5E` (`INI::parseInt`), `userData = 0`, offset `+0x64C` (`1612` bytes) in `ThingTemplate`.
  - **Type is `int`, NOT an enum**: Ennorath's `SELECTION_PRIORITY_CREBAIN`, `SELECTION_PRIORITY_MORDOR_WORKER`, and `SELECTION_PRIORITY_PORTER` are `#define` macros in `data/ini/_gamedata.inc:1452-1454` (`1`, `2`, `3`), expanded before `INI::parseInt`.
- **`ThingTemplate::ThingTemplate` (`RW 0x73FB7A`)**:
  - At `RW 0x740069..0x740074`: `mov eax, 0x7fffffff` -> `mov dword ptr [esi + 0x64c], eax`. Default priority is `INT_MAX` (`2147483647`).
- **`ThingTemplate::copyFrom` (`RW 0x6D1D80`)**:
  - At `RW 0x6D271E..0x6D2725`: `mov ecx, dword ptr [eax + 0x64c]` -> `mov dword ptr [ebx + 0x64c], ecx`. Preserved across `ChildObject` / `ObjectReskin` / `DefaultThingTemplate` unless overridden.
- **Exhaustive `.text` scan of all 17 occurrences of immediate/displacement `0x64c`**:
  1. `RW 0x740074`: `ThingTemplate` constructor default (`0x7FFFFFFF`).
  2. `RW 0x6D271E` / `RW 0x6D2725`: `ThingTemplate::copyFrom`.
  3. `RW 0x710463` / `RW 0x710475`: two adjacent dead getters (`0` callers in the binary).
  4. `RW 0x710A41`: `addDrawableToList` (the **only** live reader).
  5. `RW 0xB1AD90`: `jmp 0xb1b3f7` relative branch displacement (`0x64c` bytes), not a field access.
  6. Remaining 10 occurrences: unrelated structures / stack frames / float constants (`0x4445B2`, `0x5AE271`, `0x6A0DD9`, `0x6B00E0`, `0x6B95AC`, `0x90DFEC`, `0x90E1DF`, `0xA64B5E`, `0xB02FE0`, `0xB44234`).

---

## 2. Call chain and every branch

### 2.1 `SelectionTranslator::leftClick` (`RW 0x83C340`)

1. Builds selection region `IRegion2D` (`RW 0x83C37E` `call 0x710c9e`), sets `isPointClick = (region.lo.x == region.hi.x && region.lo.y == region.hi.y)` (`RW 0x83C38C..0x83C3A0`).
2. Initializes `SelectionInfo` (`RW 0x710888` at `RW 0x83C3BB`):
   - `m_bestPriority` (`+0x40`) = `-1` (`RW 0x7108A9`: `or dword ptr [esi + 0x40], 0xffffffff`).
   - `m_isPointClick` (`+0x4`) = `[InGameUI + 0x8b8]` (`RW 0x83C3D2` also passes `ebp+0xb`, `isDragSelecting`).
   - Calls `getPickTypesForContext` (`RW 0x71083F`) + `RW 0x7107F0` to populate `m_mustBe` (`+0x8`, setting `SELECTABLE` bit `1`, `SHRUBBERY` / `ROCK` / `FORCEATTACKABLE` when active, and when `m_mustBe` is for non-drag at `RW 0x7108E7` sets `ALWAYS_SELECTABLE` bit `58`).
3. On drag (`isPointClick == false`, `RW 0x83C3E1`) or `[InGameUI + 0x924] != 0` (`RW 0x83C3D8`):
   - `RW 0x83C3E7`: `or byte ptr [ebp - 0x9f], 1` -> sets bit `104` (`BASE_FOUNDATION`) in `m_mustNotBe` (`+0x24`).
   - `RW 0x83C3EE`: `or byte ptr [ebp - 0xa3], 0x10` -> sets bit `76` (`CLICK_THROUGH`) in `m_mustNotBe` (`+0x24`).
4. Dispatches region iteration with callback `addDrawableToList` (`RW 0x7108F2`):
   - `InGameUI + 0x924 != 0` -> `RW 0x6A0A5E` (`RW 0x83C40A`).
   - Normal path -> `TheTacticalView->iterateDrawablesInRegion(&region, addDrawableToList, &info)` (`RW 0x485B80` at `RW 0x83C44C`).

### 2.2 Point click (`isPoint == true`) vs drag region (`isPoint == false`)

- **Point click (`RW 0x485BA5..0x485D8B`, or `< 3` verts / zero-area in `RW 0x6A0AA6..0x6A0AD9`)**:
  1. Calls `W3DView::pickDrawable` (`RW 0x48AB28`, `RW 0x485CB8` -> `RTS3DScene::castRay` `RW 0x471A5B` -> `RW 0x489BE6`).
  2. `W3DView::pickDrawable` returns **at most one** `Drawable*` (`onlyDrawableToTest`) using collision tiers (`0x184` > `0x104` > `0x84` > `0x14` > `pickType`) and nearest ray hit `t`. **It never reads `ThingTemplate + 0x64C`.**
  3. Calls `addDrawableToList(onlyDrawableToTest, &info)` once (`RW 0x485D6F`, or `RW 0x6A0B8A`).
  4. Therefore, when two selectable objects with `SelectionPriority >= -1` overlap under a single click point, **RotWK picks by collision tier / ray distance `t`, NOT by `SelectionPriority`**. `SelectionPriority` only affects a point click if the picked object (or its promoted `HORDE`) has `SelectionPriority < -1` (e.g. `-2`), in which case `addDrawableToList` rejects it (`return 2` at `RW 0x710A47`) and nothing is selected.

- **Drag region (`isPoint == false`)**:
  1. `RW 0x485CD8` (and `RW 0x6A0AE5`) starts at `TheGameClient->firstDrawable()` and walks `Drawable::m_nextDrawable` (`+0x104`, `RW 0x485D95` / `RW 0x6A0BA8`).
  2. `GameClient::registerDrawable` (`RW 0x645487` -> `RW 0x6454A5`) calls `Drawable::prependToList` (`RW 0x6717E0`), inserting newly created drawables at the **head** of `m_drawableList`. Consequently, `iterateDrawablesInRegion` visits drawables in **reverse creation order (newest-created first, oldest-created last)**.
  3. Each drawable whose projected screen point lies inside the region (`RW 0x485CD0..0x485D5B`) is passed to `addDrawableToList`.

- **Hover & order pick**:
  - `SelectionTranslator::mouseMoved` / hover (`RW 0x83CC13`) and order evaluation use `W3DView::pickDrawable` (`RW 0x48AB28`) directly without `addDrawableToList` or `+0x64C`.

### 2.3 `addDrawableToList` (`RW 0x7108F2..0x710A50`)

Branch-by-branch behavior for each candidate `Drawable *draw` (`ebx`) and `SelectionInfo *info` (`edi`):

1. `RW 0x7108F9`: `if (!info->m_drawables) return 2;`
2. `RW 0x71090A..0x710930`: `draw->getTemplate()->m_kindOf` (`+0x108`) checked with `BitFlags::testAny` (`RW 0x661359`): must match `m_mustBe`, must not match `m_mustNotBe` (on drag: excludes `BASE_FOUNDATION` and `CLICK_THROUGH`); else `return 2`.
3. `RW 0x710936..0x71095C`: if `!m_allowHidden`, reject `status 0x49` / template `+0x109 & 0x40` (`return 2`).
4. `RW 0x710962..0x71096B`: `canSelectDrawable(draw)` (`RW 0x671A3F`) -> requires `obj = draw->getObject()` non-null and `Object::isSelectable()` (`RW 0x68DE58`: `ALWAYS_SELECTABLE`, else not `UNSELECTABLE`, not effectively dead without `KeepSelectableWhenDead`, not `DRONE`, `m_isSelectable` or `WALK_ON_TOP_OF_WALL` under special power); else `return 2`. Note that `canSelectDrawable` runs **before** `SelectionPriority`, so unselectable objects/props/dead units never affect `m_bestPriority`.
5. `RW 0x710971..0x710977`: if `obj == nullptr`, jumps to `0x710A29` (`push_back`) skipping priority.
6. `RW 0x71097D..0x7109CC`: local player shroud `> 2` (`FOGGED`/`SHROUDED`) -> `return 2`; drawable or container invisibility `== 5` -> `return 2`.
7. `RW 0x7109CE..0x710A09`: `HORDE` promotion — if `obj->m_producerID` (`+0x78`) resolves to a `HORDE` (`+0x115 & 0x20`), if `producer->testStatus(3 /* UNSELECTABLE */)` return `2`; else replace `obj = producer` and `draw = producer->getDrawable()` **before** reading `SelectionPriority`.
8. `RW 0x710A0C..0x710A4F`: `SelectionPriority` filter and priority promotion:
   - Reads `tpl = obj->getTemplate()`. If `!tpl`, `priority = -1` (`RW 0x710A13`); else `priority = *(const int *)(tpl + 0x64C)` (`RW 0x710A41`).
   - `if (priority < -1) return 2;` (`RW 0x710A47`).
   - `if (priority > info->m_bestPriority)` (`RW 0x710A16..0x710A25`): calls `List::clear()` (`RW 0x6459CB`), sets `info->m_bestPriority = priority`, marks `cleared = true`.
   - `jle 0x710a29` (`RW 0x710A19`): **when `priority <= info->m_bestPriority` (including strictly lower priorities `>= -1` visited *after* a higher-priority object), it does NOT return `2` — it falls through to `RW 0x710A29` (`push_back(draw)`)!**
   - Returns `1` if `cleared`, `0` otherwise (`RW 0x710A34..0x710A40`).

**Practical consequence of reverse-creation iteration + `jle 0x710a29`**:
- If an army (`priority = INT_MAX`) exists first and `Crebain` (`1`) or `MordorWorker` (`2`) is spawned afterward, `iterateDrawablesInRegion` visits `Crebain`/`Worker` **first** (`m_bestPriority = 1` or `2`), then visits the army (`INT_MAX > m_bestPriority`), clears `m_list`, and selects **only the army**.
- Conversely, if `MordorWorker` (`2`) was spawned before the army (`INT_MAX`), `iterateDrawablesInRegion` visits the army first (`m_bestPriority = INT_MAX`) and then visits `MordorWorker` (`2 <= INT_MAX`), keeping **both** `[army, worker]`.

---

## 3. Ennorath 1.9.8.2 INI usage and current port state

- **`data/ini/_gamedata.inc:1452-1454`**:
  - `#define SELECTION_PRIORITY_CREBAIN 1`
  - `#define SELECTION_PRIORITY_MORDOR_WORKER 2`
  - `#define SELECTION_PRIORITY_PORTER 3`
- **17 active Object definitions in `data/ini/`**:
  - `1` (`SELECTION_PRIORITY_CREBAIN`): `angmarharbringer.ini`, `barrowremaint.ini`, `heuschrecken.ini`, `mordorbats.ini`, `goodfactionsubobjects.ini`, `arnorfortress.ini`, `captainofdale.ini`, `dwarvencarc.ini`, `dwarvengrimbeorn.ini` (x4), `faramirscout.ini`, `neutralunits.ini` (x2).
  - `2` (`SELECTION_PRIORITY_MORDOR_WORKER`): `haradlumberworker.ini`, `worker.ini` (orc slaves).
  - `3` (`SELECTION_PRIORITY_PORTER`): commented out on `Porter`, `angagandaur`, `arnorharvester`.
- **Current port (`openbfme-godot`)**:
  - `engine/data/rotwk-201/field-tables.json:17876` already maps `"SelectionPriority"` -> `0x42ec5e` (`INI::parseInt`, offset `1612` = `0x64C`), and `ThingFactory` parses/copies it into `ThingTemplate::m_fields` (with `INIMacro` expanding `_gamedata.inc` `#define`s).
  - Missing in port:
    1. `ThingTemplate::selectionPriority()` typed getter (default `INT_MAX` / `0x7FFFFFFF`, reading `getFinalOverride()->findField("SelectionPriority")`).
    2. In `HudObjects::objectsInRegion`:
       - Point branch: reject `selectionPriority < -1` (`RW 0x710A47`), keeping `W3DView::pickDrawable` (`pickForSelection`) priority-agnostic for `>= -1`.
       - Drag branch: iterate reverse creation order (`ctx.logic.getLastObject()` -> `getPrevObject()`, matching `Drawable::prependToList` `RW 0x6717E0`), gate on `isSelectable(*obj, ctx.ui.getGUICommand())` (`RW 0x710964`) and `!isKindOfName("CLICK_THROUGH") && !isKindOfName("BASE_FOUNDATION")` (`RW 0x83C3E7..0x83C3EE`) before priority, apply `m_bestPriority = -1` / `< -1` reject / `> bestPriority` clear / `<= bestPriority` append on the promoted `stand` (`RW 0x710A0C..0x710A4F`).
- **Unported sibling path**: `InGameUI + 0x924` formation-drag dispatch (`RW 0x83C40A -> RW 0x6A0A5E`) is not ported in `SelectionTranslator` / `HudInput`.
