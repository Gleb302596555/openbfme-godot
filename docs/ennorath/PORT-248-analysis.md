# Port 248: working analysis

This is an incomplete investigation, not a no-reader verdict. The reference is
RotWK 2.01; donor references below are BFME2 1.06, not evidence of RW behavior.

## RunOffMapWaypointName

The RW table at 0xC59698 assigns the name to module data +0x14. RW 0x86540A
requires an AI interface and module data, asks the terrain for the named
waypoint (vslot 0x88), otherwise projects the facing direction and queries the
ground height. It asks the pathfinder RW 0x6F5BB0; on failure it follows a
separate kill path, while on success it selects command 0x41 or 0x38 according
to DieOnMap, then sets fleeing and model state. DONOR BFME2 0x4622FC
(`Code/GameEngine/Source/GameLogic/Object/Behavior/RunOffMapBehavior.cpp`)
corroborates the mechanism. The port's `RunOffMapBehavior::triggerFlee` has
neither the command nor the pathfinding branches (existing S-5308). Adding only
waypoint resolution cannot produce the retail reaction; these AI commands and
pathfinding are shared mechanisms outside this task.

## Symbiote

RW 0x6512BE creates the module via ctor RW 0x8C4C82, which assigns the primary
vtable 0xC72370 and zeroes the body pointer +0x100 and ID +0x104. RW 0x8C5304
reads the data name at +0x64 and scans the GameLogic object list, compares object
names, copies the matching object's transform, stores its ID, then invokes
RW 0x8C4DE3 and RW 0x8C4CF4. DONOR BFME2 0x4C1170
(`Code/GameEngine/Source/GameLogic/Object/Body/ImmortalBody.cpp`) identifies
this as primary slot 1, loadPostProcess, rather than onObjectCreated. The port
has no module loadPostProcess dispatch (`Common/Module.h`,
`GameLogic/GameLogicObjectSnapshot.cpp`); calling it during construction would
be too early. Correct integration requires the shared loading mechanism, not a
class-local substitute. RW 0x8C4EE0 resolves the stored ID to its body.

## ClearanceGeometryIsSmall

RW parse table 0xC60590 gives GeometryInfo at data +0x190; RW 0xAD2B30 writes
its byte +4 (data +0x194), then calls RW 0xAD2860. The port instead parses a
separate bool. RW 0x887191 passes the GeometryInfo address to the would-collide
filter RW 0x67C5F3 and uses its bounding sphere at +0x14, then may walk terrain.
The collision path and geometry recalculation must be fully traced for indirect
reads of byte +4 before claiming either a behavioral gap or a no-reader verdict.
DONOR `Code/GameEngine/Include/Common/Geometry.h` identifies the byte and
`Code/GameEngine/Source/Common/System/GeometryParseDimensions.cpp` the parser.

Additional tracing: the primary vtable 0xC72370 has RW 0x8C5304 in slot 1,
which independently confirms the donor's loadPostProcess identification. The
clearance runtime table 0xC6065C has RW 0x887191 at slot 3. The would-collide
filter's vtable 0xC112C8 has RW 0x66139C in slot 1; it calls RW 0xAD2CE0.
The terrain sampler RW 0x8E047D calls the GeometryInfo copy constructor RW
0x4923F0, which copies the small byte at 0x49240B into the sampler. RW 0x8E0354
then uses the copied geometry; this copy is a reader in the literal sense but
is not by itself proof that the bit affects the result. Complete tracing of
RW 0xAD2CE0, RW 0x8E047D / 0x8E0354 and their geometry callees is still needed,
including indirect access and other users of the module data. Do not classify
the field as unused merely because the main function does not load +0x194.

Further: RW 0xC60650 slot 0 is RW 0x887013, an accessor returning the
GeometryInfo at data +0x190, so callers of this interface outside the class
must be considered. RW 0x8E047D copies it via RW 0x4923F0 and the sampler's
RW 0x8E0354 uses its shape and bounds; RW 0xAD22C0 checks the shape list and
shape type. RW 0x66139C calls collision RW 0xAD2CE0. DONOR BFME2 0x483F19
(`Code/GameEngine/Source/Common/Rva00483C35Rva00483F19.cpp`) passes the
geometry by reference to the collision filter and by copy to the sampler;
`Code/GameEngine/Source/GameLogic/Map/TerrainLogic.cpp` shows that other
GeometryInfo consumers can inspect isSmall. It remains necessary to trace
actual callers of slot 0, not assume the field is unused globally from the
local collision and sampler paths.

The interface chain is now tied to the actual instance: the class constructor
RW 0x886FB1 installs primary vtable 0xC6073C, behavior-interface vtable
0xC60680 at +0xC and geometry-interface vtable 0xC60650 at +0x50. Primary
slot +0x34 points to RW 0x886EE6; behavior-interface slot +0x60 points to
RW 0x886EF2; both return the +0x50 subobject. Its slot 0 RW 0x887013 returns
data +0x190. DONOR BFME2 `ClearanceTestingSlowDeathBehavior.cpp`, slot-13
function BFME2 0x483C69 and geometry getter 0x483D9B, confirms the shape of
the interface. A raw search of virtual call instructions is only a lead, not
a complete proof of indirect call targets. In particular no claim is made that
the returned GeometryInfo has no external consumer.

## Owner follow-up

The owner checked the pure-virtual base 0xBE1CA8 and the 30 primary-slot
+0x34 calls that test for null. None subsequently calls the returned interface's
slot 0 without arguments. Object loops RW 0x675034, 0x6750AF and 0x676AD0
pass two arguments and concern a different interface. None of the 25 calls to
RW 0x4923F0 follows this getter. The consumer was not found statically; this is
not an evidence-complete no-reader verdict. S-7409 retains the unresolved work.
