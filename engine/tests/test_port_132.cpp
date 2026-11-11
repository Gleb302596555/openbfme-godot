// OpenBFME unit tests: PORT-132 (issue #334) - Locomotor AeleronCorrectionDegree / AeleronCorrectionRate /
// ElevatorCorrectionDegree / ElevatorCorrectionRate: how a ship rights its roll and pitch.
// GPL-3.0.
//
// The reader of the four fields is the SHIP body of Drawable::calcPhysicsXform (RW 0x6732DC, one of the five
// suspension bodies behind the appearance dispatcher RW 0x67BE1D .. 0x67BE7D; DONOR BFME2 0x00272FB6,
// DrawablePhysicsXformShip.cpp). RotWK 2.01 reads them inside that body:
//   AeleronCorrectionRate: RW 0x67386C movss xmm0,[eax+0x120] -> RW 0x673874 addss xmm0,[ecx+0x34] -> RW 0x673879 store:
//     the roll accumulator (DrawableLocoInfo +0x34, m_yawModulator) steps by the rate; RW 0x67388C call 0xA3CF90 (sin) ->
//     RW 0x673891 fmul dword ptr [ebp+8] (AeleronCorrectionDegree, read RW 0x673847 mov ecx,[eax+0x11C], kept RW 0x673855)
//     -> RW 0x673894 fadd dword ptr [esi+4] -> RW 0x673897: PhysicsXformInfo::m_totalRoll.
//   ElevatorCorrectionRate: RW 0x67384D movss xmm0,[eax+0x118] -> RW 0x6738A5 addss xmm0,[ebp+0xc] -> RW 0x6738AA store
//     into DrawableLocoInfo +0x38 (m_pitchModulator); RW 0x6738BB call 0xA3CF84 (cos) -> RW 0x6738C0 fmul dword ptr
//     [ebp-0x40] (ElevatorCorrectionDegree, read RW 0x673858 mov ecx,[eax+0x114], kept RW 0x67385E) ->
//     RW 0x6738C5 fadd dword ptr [esi] -> RW 0x6738CD: PhysicsXformInfo::m_totalPitch.
// Parse rows: RW 0xBF48E8 (+0x114 ElevatorCorrectionDegree), RW 0xBF48F8 (+0x118 ElevatorCorrectionRate),
// RW 0xBF4908 (+0x11C AeleronCorrectionDegree), RW 0xBF4918 (+0x120 AeleronCorrectionRate), parser RW 0x42ED00
// (engine/tests/data/horde1/table_locomotor.tsv rows 71-74). Ennorath writes them from 13 ship locomotors; the values
// below are LargeShipLocomotor's.
//
// Every assertion below is an exact float equality: the expected values are the same float expressions the body
// evaluates, so no tolerance is needed and a wrong field, a swapped sin/cos or a changed order is caught.

#include "doctest.h"
#include "ObjectTestUtil.h"
#include "IniTestUtil.h"
#include "W3dSynth.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object/ObjectGeometry.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace initest;
using namespace objtest;
using namespace w3dsynth;

namespace
{
// The mod's ship locomotor values (LargeShipLocomotor): roll amplitude / roll rate, pitch amplitude / pitch rate.
const float kAeleronDegree = 0.073f;  // +0x11C
const float kAeleronRate = 0.021f;    // +0x120
const float kElevatorDegree = 0.163f; // +0x114
const float kElevatorRate = 0.016f;   // +0x118

// A drawable over a boxed ship-shaped object, and a snapshot/pose pair with no motion of its own.
struct ShipRig
{
	World w;
	std::unique_ptr<Drawable> d;
	RenderInterpolation::Pose pose;
	ObjectSnapshot snap;
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<DrawServices> services;

	ShipRig()
	{
		const std::string objIni =
			"Object TestPort132Ship\n"
			"  Geometry = BOX\n"
			"  GeometryMajorRadius = 15.0\n"
			"  GeometryMinorRadius = 8.0\n"
			"  GeometryHeight = 8.0\n"
			"End\n";
		REQUIRE(w.load(objIni).empty());
		const ThingTemplate *tt = w.get("TestPort132Ship");
		REQUIRE(tt != nullptr);

		assets = std::make_unique<WW3DAssetManager>(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		services = std::make_unique<DrawServices>(*assets, w.modules, hostOpt);
		services->groundHeight = [](float /*x*/, float /*y*/, Coord3D *normal) -> float {
			if (normal)
				*normal = Coord3D{ 0.0f, 0.0f, 1.0f };
			return 0.0f;
		};

		std::vector<std::string> problems;
		d = std::make_unique<Drawable>(*services, tt, (DrawableID)132, (ObjectID)1320, {}, problems);
		REQUIRE(problems.empty());

		pose.position = Coord3D{ 10.0f, 20.0f, 0.0f };
		pose.angle = 0.0f;
		pose.basis[0] = 1.0f;
		pose.basis[4] = 1.0f;
		pose.basis[8] = 1.0f;

		snap.position = pose.position;
		snap.previousPos = pose.position; // no motion: the velocity and acceleration branches add nothing
	}

	// A ship locomotor whose spring path is switched off, so the only source of pitch and roll is the bobbing of the
	// four correction fields (RW 0x673847 .. 0x6738CD).
	static LocomotorTemplate shipTemplate(float aeleronDegree, float aeleronRate, float elevatorDegree, float elevatorRate)
	{
		LocomotorTemplate loco;
		loco.m_appearance = LOCO_SHIP;
		loco.m_pitchStiffness = 0.0f;
		loco.m_rollStiffness = 0.0f;
		loco.m_pitchDamping = 0.0f;
		loco.m_rollDamping = 0.0f;
		loco.m_uniformAxialDamping = 0.0f;
		loco.m_accelerationPitchLimit = 0.4f;
		loco.m_forwardVelocityPitchFactor = 0.0f;
		loco.m_forwardAccelerationPitchFactor = 0.0f;
		loco.m_lateralAccelerationRollFactor = 0.0f;
		loco.m_bounceAmount = 0.0f;
		loco.m_aeleronCorrectionDegree = aeleronDegree;
		loco.m_aeleronCorrectionRate = aeleronRate;
		loco.m_elevatorCorrectionDegree = elevatorDegree;
		loco.m_elevatorCorrectionRate = elevatorRate;
		return loco;
	}

	bool step(PhysicsXformInfo &info)
	{
		return d->calcPhysicsXform(snap, pose, info);
	}
};

// INI parsing of the four rows (parser RW 0x42ED00).
struct LocomotorIniFixture
{
	Fixture fx;
	LocomotorStore store;
	LocomotorStore *savedStore;

	LocomotorIniFixture()
	{
		savedStore = TheLocomotorStore;
		TheLocomotorStore = &store;
		fx.env.blocks.registerBlock("Locomotor", [](INI *ini) { LocomotorStore::parseLocomotorTemplateDefinitionGlobal(ini); });
	}

	~LocomotorIniFixture()
	{
		TheLocomotorStore = savedStore;
	}
};
} // namespace

TEST_CASE("PORT-132: a ship's roll and pitch correction each client frame is its locomotor's four fields (RW 0x673847 .. 0x6738CD)")
{
	ShipRig rig;
	LocomotorTemplate loco = ShipRig::shipTemplate(kAeleronDegree, kAeleronRate, kElevatorDegree, kElevatorRate);
	rig.snap.locomotorTemplate = &loco;

	// The roll accumulator (DrawableLocoInfo +0x34) and the pitch accumulator (+0x38) start at 0 and step by their rate
	// before the sine / cosine is taken (RW 0x673874 -> 0x67388C, RW 0x6738A5 -> 0x6738BB), so client frame k carries the
	// rate added k times. Written out as the accumulator adds it, the expected values are exact.
	const float roll1 = kAeleronRate;
	const float roll2 = roll1 + kAeleronRate;
	const float roll3 = roll2 + kAeleronRate;
	const float roll4 = roll3 + kAeleronRate;
	const float roll5 = roll4 + kAeleronRate;
	const float pitch1 = kElevatorRate;
	const float pitch2 = pitch1 + kElevatorRate;
	const float pitch3 = pitch2 + kElevatorRate;
	const float pitch4 = pitch3 + kElevatorRate;
	const float pitch5 = pitch4 + kElevatorRate;

	PhysicsXformInfo info;
	REQUIRE(rig.step(info));
	CHECK(info.m_totalRoll == std::sin(roll1) * kAeleronDegree);   // sin, the roll amplitude (RW 0x67388C / 0x673891)
	CHECK(info.m_totalPitch == std::cos(pitch1) * kElevatorDegree); // cos, the pitch amplitude (RW 0x6738BB / 0x6738C0)

	REQUIRE(rig.step(info));
	CHECK(info.m_totalRoll == std::sin(roll2) * kAeleronDegree);
	CHECK(info.m_totalPitch == std::cos(pitch2) * kElevatorDegree);

	REQUIRE(rig.step(info));
	CHECK(info.m_totalRoll == std::sin(roll3) * kAeleronDegree);
	CHECK(info.m_totalPitch == std::cos(pitch3) * kElevatorDegree);

	REQUIRE(rig.step(info));
	CHECK(info.m_totalRoll == std::sin(roll4) * kAeleronDegree);
	CHECK(info.m_totalPitch == std::cos(pitch4) * kElevatorDegree);

	REQUIRE(rig.step(info));
	CHECK(info.m_totalRoll == std::sin(roll5) * kAeleronDegree);
	CHECK(info.m_totalPitch == std::cos(pitch5) * kElevatorDegree);
}

TEST_CASE("PORT-132: with the four fields zero nothing is corrected - no bobbing and no Z lift (RW 0x673891 / 0x6738C0)")
{
	ShipRig rig;
	LocomotorTemplate loco = ShipRig::shipTemplate(0.0f, 0.0f, 0.0f, 0.0f);
	rig.snap.locomotorTemplate = &loco;

	PhysicsXformInfo info;
	for (int frame = 0; frame < 5; ++frame)
	{
		REQUIRE(rig.step(info));
		CHECK(info.m_totalRoll == 0.0f);  // sin(phase) * 0, and the phase never moves (rate 0)
		CHECK(info.m_totalPitch == 0.0f); // cos(phase) * 0
		CHECK(info.m_totalZ == 0.0f);     // |sin(0)| * length + |sin(0)| * width, scaled, minus half the height: clamped to 0
	}
}

TEST_CASE("PORT-132: the ship body's neighbour - the velocity pitch of a moving ship is untouched (RW 0x6732DC)")
{
	ShipRig rig;
	LocomotorTemplate loco = ShipRig::shipTemplate(0.0f, 0.0f, 0.0f, 0.0f);
	loco.m_uniformAxialDamping = 1.0f;
	loco.m_forwardVelocityPitchFactor = 0.25f; // exact in binary, so the neighbour's pitch is an exact value
	rig.snap.locomotorTemplate = &loco;

	// The ship sails 2.0 per client frame along its own forward axis (pose.basis[0] = 1).
	rig.snap.previousPos = Coord3D{ rig.pose.position.x - 2.0f, rig.pose.position.y, rig.pose.position.z };

	PhysicsXformInfo info;
	REQUIRE(rig.step(info));
	CHECK(info.m_totalPitch == 0.0f); // the first frame only charges m_pitchRate in the forward velocity branch
	CHECK(info.m_totalRoll == 0.0f);

	REQUIRE(rig.step(info));
	// The second frame integrates it: m_pitchRate = -(0.25 * 2.0), m_pitch += 1.0 * m_pitchRate. With the four fields zero
	// the bobbing adds nothing, so what is left is the neighbour's pitch alone.
	CHECK(info.m_totalPitch == -0.5f);
	CHECK(info.m_totalRoll == 0.0f); // no lateral term, no invented roll
}

TEST_CASE("PORT-132: the mod's ship locomotor rows reach the ship body through the parser (RW 0xBF48E8 .. 0xBF4918)")
{
	LocomotorIniFixture fix;
	const std::string ini =
		"Locomotor TestPort132ShipLoco\n"
		"  Surfaces = GROUND\n"
		"  Speed = 20\n"
		"  ElevatorCorrectionDegree = 0.163\n"
		"  ElevatorCorrectionRate = 0.016\n"
		"  AeleronCorrectionDegree = 0.073\n"
		"  AeleronCorrectionRate = 0.021\n"
		"End\n";
	REQUIRE(loadError(fix.fx.env, "loco_port_132.ini", ini).empty());
	const LocomotorTemplate *parsed = fix.store.findLocomotorTemplate("TestPort132ShipLoco");
	REQUIRE(parsed != nullptr);
	REQUIRE(parsed->m_aeleronCorrectionDegree == kAeleronDegree);
	REQUIRE(parsed->m_aeleronCorrectionRate == kAeleronRate);
	REQUIRE(parsed->m_elevatorCorrectionDegree == kElevatorDegree);
	REQUIRE(parsed->m_elevatorCorrectionRate == kElevatorRate);

	ShipRig rig;
	LocomotorTemplate loco = *parsed; // the ship appearance selects the body that reads the four fields
	loco.m_appearance = LOCO_SHIP;
	loco.m_pitchStiffness = 0.0f;
	loco.m_rollStiffness = 0.0f;
	loco.m_pitchDamping = 0.0f;
	loco.m_rollDamping = 0.0f;
	loco.m_uniformAxialDamping = 0.0f;
	rig.snap.locomotorTemplate = &loco;

	PhysicsXformInfo info;
	REQUIRE(rig.step(info));
	CHECK(info.m_totalRoll == std::sin(kAeleronRate) * kAeleronDegree);
	CHECK(info.m_totalPitch == std::cos(kElevatorRate) * kElevatorDegree);
}
