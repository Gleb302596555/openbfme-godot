// OpenBFME unit tests: PORT-105 (issue #265) - TransportContain ReleaseSnappyness: a fell beast or an eagle drops the unit it grabbed.
// GPL-3.0.
//
// Retail RotWK 2.01 (game.dat, caveat S-001), the whole chain and what each step does:
//   RW 0x86AD62 .. 0x86ADAE TransportContain::onRemoving, after the exit delay is stamped (RW 0x86AD3F):
//     * RW 0x86AD64 ownsDrawable = RW 0x70E013(container) and RW 0x86AD8C riderDrawable = RW 0x70E013(rider): both objects need a drawable.
//     * RW 0x86AD6E .. 0x86AD84 asks the contain's slot 0xB0 (RW 0x865C15: ObjectStatusOfContained for this rider, the name table RW 0xD8AFF0)
//       and skips the release when bit 61 (ENCLOSED) is in it: an enclosed rider is never drawn at a bone.
//     * RW 0x86AD93 .. 0x86AD9C ownerDrawable->RW 0x6719CD(riderDrawableID) -> every draw module's ObjectDrawInterface slot 0x4C: the container's
//       drawable forgets the drawable it carried (the port attaches no drawable to a bone, stop S-1104; the call has no counterpart here).
//     * RW 0x86ADA1 .. 0x86ADAE riderDrawable->RW 0x67476B(ReleaseSnappyness, the data + 0x17C).
//   RW 0x67476B Drawable::<release>(float): every draw module's ObjectDrawInterface (slot 0xA8) slot 0x50 (the interface tables RW 0xBE3B78 /
//     0xBE1AD0 / 0xBE1E58 hold RW 0x4B516C there) is called with the float, then the drawable's pending model condition sets are cleared
//     (memset +0x2A4 and +0x2F0, 0x4C bytes each).
//   RW 0x4B516C W3DModelDraw's slot 0x50: with a render object (+0x44) it copies the transform that object is drawn at (render object +0x18, twelve
//     floats) into the module's +0x290, stores the weight at +0x2C0 and TheGameClient's frame (vslot 0x7C of 0xDE4388) at +0x2C4 (RW 0x4B288B).
//   RW 0x4B6B21 .. 0x4B6BB2 (the per-draw path of the module, the same function as RW 0x4B686D's later blocks): while the weight is below 1.0
//     (RW 0x4B6B2C .. 0x4B6B3B) it stores the frame of the step (+0x2C4), adds 1/30 (RW 0xBDFC6C = 0.033333335) once per new client frame and clamps
//     to 1.0 (RW 0xBD1908); then RW 0xB27C80(module + 0x290 snapshot, the current matrix, the weight) is the drawn transform - the translation
//     lerped, the rotation slerped. At weight 1.0 the blend is off (the finish branch RW 0x4B6BAB .. 0x4B6BB2) and the module draws the object's own
//     transform. Fixing the increment and the clamp (RW 0xBDFC6C, RW 0xBD1908) is what makes the same ReleaseSnappyness reach the object's pose in
//     the same number of client frames as retail: 0.05 in 29 frames, the data's default 0.7 (RW 0xBDE0A8, TransportContain ctor) in 9.
//
// The default of ReleaseSnappyness in the module data is checked by test_horde_contain_data.cpp (0.7, RW 0xBDE0A8); here only the release path.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"
#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <string>
#include <vector>

using namespace w3dsynth;

namespace
{
// A carrier that does not enclose its riders (a fell beast, an eagle: the rider is drawn on the carrier and walks away on release) and one that
// does (ObjectStatusOfContained = ENCLOSED, RW 0xD8AFF0 bit 61): the same ReleaseSnappyness, the same slots.
const char kPort105Objects[] =
	"Object P105Carrier\n"
	"  KindOf = MONSTER SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 1\n"
	"    ReleaseSnappyness = 0.05\n"
	"  End\n"
	"End\n"
	"Object P105EnclosedPen\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 1\n"
	"    ReleaseSnappyness = 0.05\n"
	"    ObjectStatusOfContained = ENCLOSED\n"
	"  End\n"
	"End\n"
	"Object P105Soldier\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 18\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

// The hook the simulation calls where retail calls the rider's drawable (RW 0x86AD93): it records every request in call order.
struct ReleaseProbe : ObjectClientHooks
{
	struct Call
	{
		ObjectID rider = INVALID_ID;
		float snappyness = 0.0f;
	};
	std::vector<Call> calls;
	void objectCreated(Object &) override {}
	void objectDestroyed(Object &) override {}
	void releaseRiderFromBone(Object &rider, float snappyness) override { calls.push_back(Call{ rider.getID(), snappyness }); }
};

// The weight RW 0x4B6B63 gives the n-th draw after the release: the release stores ReleaseSnappyness, every new client frame adds 1/30, clamped to 1.0
// (RW 0xBDFC6C, RW 0xBD1908). Frame 0 is the client frame the release happened in (no step: RW 0x4B6B48 compares the frame stamp).
float expectedWeight(float snappyness, int drawsAfterRelease)
{
	float w = snappyness + (float)drawsAfterRelease / 30.0f;
	return w > 1.0f ? 1.0f : w;
}
} // namespace

TEST_CASE("PORT-105: TransportContain::onRemoving asks the rider's drawable for the release blend with the data's ReleaseSnappyness (RW 0x86ADA1 -> RW 0x67476B)")
{
	combattest::CombatWorld w(kPort105Objects);
	ReleaseProbe probe;
	ObjectClientHooks *before = w.logic->clientHooks();
	w.logic->setClientHooks(&probe);

	Object *carrier = w.unit("P105Carrier", 'A', 300, 300);
	Object *soldier = w.unit("P105Soldier", 'A', 330, 300);
	ContainModuleInterface *contain = carrier->getContain();
	REQUIRE(contain);
	REQUIRE(contain->addToContain(soldier));
	probe.calls.clear();

	// RW 0x86AD18 onward, the plain exit: the rider comes out and the release follows the exit delay (RW 0x86AD3F -> 0x86AD93)
	contain->removeFromContain(soldier);
	w.logic->setClientHooks(before);

	REQUIRE(probe.calls.size() == 1);
	CHECK(probe.calls[0].rider == soldier->getID());
	// the TransportContain field ReleaseSnappyness (the data + 0x17C, RW 0xC5ADC8's row)
	// tolerance: the INI real reaches a float member through the parser's atof, so the check ties the stored value to the literal's rounding of 0.05
	CHECK(probe.calls[0].snappyness == doctest::Approx(0.05f));
}

TEST_CASE("PORT-105: a container whose riders get ENCLOSED does not release them (RW 0x86AD6E .. 0x86AD84, status bit 61)")
{
	combattest::CombatWorld w(kPort105Objects);
	ReleaseProbe probe;
	ObjectClientHooks *before = w.logic->clientHooks();
	w.logic->setClientHooks(&probe);

	Object *pen = w.unit("P105EnclosedPen", 'A', 500, 500);
	Object *soldier = w.unit("P105Soldier", 'A', 530, 500);
	ContainModuleInterface *contain = pen->getContain();
	REQUIRE(contain);
	REQUIRE(contain->addToContain(soldier));
	probe.calls.clear();

	contain->removeFromContain(soldier);
	w.logic->setClientHooks(before);

	CHECK(probe.calls.empty());
	CHECK(soldier->getContainedBy() == nullptr);
}

TEST_CASE("PORT-105: after the release the drawn pose runs from the release pose to the object's own with a step of 1/30 per client frame (RW 0x4B6B21 .. 0x4B6BB2, RW 0xB27C80)")
{
	objtest::World w;
	const std::string objectIni =
		"Object P105Rider\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 5\n"
		"  GeometryHeight = 18\n"
		"End\n";
	REQUIRE(w.load(objectIni).empty());
	const ThingTemplate *tt = w.get("P105Rider");
	REQUIRE(tt != nullptr);

	MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	unsigned clientFrame = 10;
	services.clientFrame = &clientFrame;

	std::vector<std::string> problems;
	Drawable d(services, tt, (DrawableID)1, (ObjectID)10, {}, problems);
	REQUIRE(problems.empty());

	// the pose the rider was drawn at while it was carried (the container's bone under it), then the pose it takes on release
	const Coord3D carried{ 0.0f, 0.0f, 0.0f };
	const Coord3D own{ 0.0f, 300.0f, 0.0f };
	ObjectSnapshot rec;
	rec.id = (ObjectID)10;
	rec.tmpl = tt;
	rec.position = carried;
	rec.recordedPos = carried;
	rec.previousPos = carried;
	rec.nextPos = carried;
	d.syncFromSnapshot(rec, 1, 0.0, false);
	REQUIRE(d.getPresentationPosition().y == 0.0f);

	d.releaseFromBone(0.05f); // RW 0x86ADA1: the TransportContain field, as the mod writes it

	rec.position = own;
	rec.recordedPos = own;
	rec.previousPos = own;
	rec.nextPos = own;
	// the client frame of the release itself: no step yet (RW 0x4B6B48), the first blend with the weight ReleaseSnappyness
	d.syncFromSnapshot(rec, 2, 0.0, false);
	// tolerance: the drawn position is RW 0xB27C80's float lerp at weight 0.05, which is not binary-exact: one float ulp at 300 units (3.1e-5)
	CHECK(d.getPresentationPosition().y == doctest::Approx(0.05f * 300.0f));

	// one step per new client frame, 1/30 each (RW 0xBDFC6C)
	for (int k = 1; k <= 28; ++k)
	{
		++clientFrame;
		d.syncFromSnapshot(rec, 2 + k, 0.0, false);
		const float weight = expectedWeight(0.05f, k);
		// tolerance: the weight is a float sum of 1/30 steps and the position its float lerp; one float ulp at 300 units (3.1e-5)
		CHECK_MESSAGE(d.getPresentationPosition().y == doctest::Approx(weight * 300.0f), "client frame step " << k);
		CHECK(d.getPresentationPosition().z == (0.0f));
	}

	// the step that clamps the weight to 1.0 (RW 0x4B6B86): the drawn pose is the object's own, exactly
	++clientFrame;
	d.syncFromSnapshot(rec, 2 + 29, 0.0, false);
	CHECK(d.getPresentationPosition().y == (300.0f));

	// and it stays there: the blend is off from the weight 1.0 on (RW 0x4B6B34 .. 0x4B6B3B)
	const Coord3D later{ 0.0f, 320.0f, 0.0f };
	rec.position = later;
	rec.recordedPos = later;
	rec.previousPos = later;
	rec.nextPos = later;
	++clientFrame;
	d.syncFromSnapshot(rec, 40, 0.0, false);
	CHECK(d.getPresentationPosition().y == (320.0f));
}

TEST_CASE("PORT-105: the module data's default ReleaseSnappyness 0.7 reaches the object's pose in 9 client frames (RW 0xBDE0A8, RW 0x4B6B63)")
{
	objtest::World w;
	const std::string objectIni =
		"Object P105RiderDefault\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 5\n"
		"  GeometryHeight = 18\n"
		"End\n";
	REQUIRE(w.load(objectIni).empty());
	const ThingTemplate *tt = w.get("P105RiderDefault");
	REQUIRE(tt != nullptr);

	MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	unsigned clientFrame = 0;
	services.clientFrame = &clientFrame;

	std::vector<std::string> problems;
	Drawable d(services, tt, (DrawableID)2, (ObjectID)20, {}, problems);
	REQUIRE(problems.empty());

	const Coord3D carried{ 0.0f, 0.0f, 0.0f };
	const Coord3D own{ 0.0f, 300.0f, 0.0f };
	ObjectSnapshot rec;
	rec.id = (ObjectID)20;
	rec.tmpl = tt;
	rec.position = carried;
	rec.recordedPos = carried;
	rec.previousPos = carried;
	rec.nextPos = carried;
	d.syncFromSnapshot(rec, 1, 0.0, false);

	d.releaseFromBone(0.7f); // TransportContainModuleData's ctor default, RW 0xBDE0A8
	rec.position = own;
	rec.recordedPos = own;
	rec.previousPos = own;
	rec.nextPos = own;
	d.syncFromSnapshot(rec, 2, 0.0, false);
	// tolerance: the float lerp at weight 0.7, which is not binary-exact: one float ulp at 300 units (3.1e-5)
	CHECK(d.getPresentationPosition().y == doctest::Approx(0.7f * 300.0f));

	for (int k = 1; k <= 8; ++k)
	{
		++clientFrame;
		d.syncFromSnapshot(rec, 2 + k, 0.0, false);
		// tolerance: the weight is a float sum of 1/30 steps from 0.7 and the position its float lerp; one float ulp at 300 units (3.1e-5)
		CHECK_MESSAGE(d.getPresentationPosition().y == doctest::Approx(expectedWeight(0.7f, k) * 300.0f), "client frame step " << k);
	}
	// 0.7 + 9/30 = 1.0: the pose is the object's own at the ninth step
	++clientFrame;
	d.syncFromSnapshot(rec, 11, 0.0, false);
	CHECK(d.getPresentationPosition().y == (300.0f));
}

TEST_CASE("PORT-105: a drawable that was never released draws the record's pose at once (the path RW 0x6765B9 leaves unchanged)")
{
	objtest::World w;
	const std::string objectIni =
		"Object P105PlainRider\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 5\n"
		"  GeometryHeight = 18\n"
		"End\n";
	REQUIRE(w.load(objectIni).empty());
	const ThingTemplate *tt = w.get("P105PlainRider");
	REQUIRE(tt != nullptr);

	MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	unsigned clientFrame = 5;
	services.clientFrame = &clientFrame;

	std::vector<std::string> problems;
	Drawable d(services, tt, (DrawableID)3, (ObjectID)30, {}, problems);
	REQUIRE(problems.empty());

	ObjectSnapshot rec;
	rec.id = (ObjectID)30;
	rec.tmpl = tt;
	rec.position = Coord3D{ 0.0f, 100.0f, 0.0f };
	rec.recordedPos = rec.position;
	rec.previousPos = rec.position;
	rec.nextPos = rec.position;
	d.syncFromSnapshot(rec, 1, 0.0, false);

	const Coord3D next{ 0.0f, 300.0f, 0.0f };
	rec.position = next;
	rec.recordedPos = next;
	rec.previousPos = next;
	rec.nextPos = next;
	++clientFrame;
	d.syncFromSnapshot(rec, 2, 0.0, false);
	CHECK(d.getPresentationPosition().y == (300.0f));
}
