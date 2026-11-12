// OpenBFME unit tests. GPL-3.0.
//
// PORT-126 (issue #310): W3DQuadrupedDraw::getFootLocations (RotWK RW 0x4C9A39, ObjectDrawInterface slot 47 / +0xBC)
// tilts huge four-legged beasts (Mumakil, MordorGreatBeast) to follow terrain on their four foot bones.
// All assets (skeleton, mesh, animation) are synthesized in memory via W3dSynth; no game or mod files are required.

#include "doctest.h"
#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/LogicSnapshot.h"
#include "GameClient/RenderInterpolation.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawVariants.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DQuadrupedDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameLogic/Locomotor.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace drawtest;
using namespace objtest;
using namespace w3dsynth;

namespace
{
class FixedRandom : public W3DDrawRandom
{
public:
	int value(int lo, int hi) override { return lo; }
	float real(float lo, float hi) override { return lo; }
};

	void addQuadrupedAssets(MemoryFileSource &fs, bool includeFrontLeft = true, bool includeFrontRight = true,
		bool includeRearLeft = true, bool includeRearRight = true, bool withWalkAnim = true)
	{
		std::vector<W3dPivotStruct> pivots;
		pivots.push_back(pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0.0f, 0.0f, 0.0f));
		if (includeFrontLeft)
		{
			pivots.push_back(pivot("e_FrontLeft", 0, 24.0f, 12.0f, 0.0f));
		}
		if (includeFrontRight)
		{
			pivots.push_back(pivot("e_FrontRight", 0, 24.0f, -12.0f, 0.0f));
		}
		if (includeRearLeft)
		{
			pivots.push_back(pivot("e_BackLeft", 0, -24.0f, 12.0f, 0.0f));
		}
		if (includeRearRight)
		{
			pivots.push_back(pivot("e_BackRight", 0, -24.0f, -12.0f, 0.0f));
		}
		fs.Add("art\\w3d\\mu\\mumak_skl.w3d", hierarchyChunk("MUMAK_SKL", pivots));

		// Alternate skeleton + HLOD model for USER_1 model condition (verifies worldSpace = true defaults to pendingFlags()).
		std::vector<W3dPivotStruct> altPivots = pivots;
		if (includeFrontLeft && altPivots.size() > 1)
		{
			altPivots[1] = pivot("e_FrontLeft", 0, 30.0f, 12.0f, 2.0f);
		}
		fs.Add("art\\w3d\\mu\\mumak_alt_skl.w3d", hierarchyChunk("MUMAK_ALT_SKL", altPivots));

		SynthMesh mesh;
		mesh.Name = "BODY";
		mesh.Container = "MUMAK_SKN";
		mesh.Vertices = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
		mesh.Triangles = { 0, 1, 2 };
		std::vector<std::uint8_t> bytes;
		append(bytes, meshChunk(mesh));
		append(bytes, hlodChunk("MUMAK_SKN", "MUMAK_SKL", { { "MUMAK_SKN.BODY", 0 } }));
		fs.Add("art\\w3d\\mu\\mumak_skn.w3d", bytes);

		SynthMesh altMesh = mesh;
		altMesh.Container = "MUMAK_ALT_SKN";
		std::vector<std::uint8_t> altBytes;
		append(altBytes, meshChunk(altMesh));
		append(altBytes, hlodChunk("MUMAK_ALT_SKN", "MUMAK_ALT_SKL", { { "MUMAK_ALT_SKN.BODY", 0 } }));
		fs.Add("art\\w3d\\mu\\mumak_alt_skn.w3d", altBytes);

		if (withWalkAnim && includeFrontLeft)
		{
			// Pivot 1 (e_FrontLeft) moves forward along +X by +6.0f at frame 3 (5 frames total so 100ms does not wrap), and lifts Z by +4.0f.
			std::vector<SynthChannel> channels;
			channels.push_back(SynthChannel{ ANIM_CHANNEL_X, 1, 0, 1, { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f } });
			channels.push_back(SynthChannel{ ANIM_CHANNEL_Z, 1, 0, 1, { 0.0f, 1.5f, 3.0f, 4.0f, 4.0f } });
			fs.Add("art\\w3d\\mu\\mumak_wlka.w3d", rawAnimChunk("MUMAK_WLKA", "MUMAK_SKL", 5, 30, channels));
		}
	}

	std::unique_ptr<W3DQuadrupedDrawModuleData> parseQuadrupedBody(const std::string &body)
	{
		INIEnvironment env;
		std::unique_ptr<W3DQuadrupedDrawModuleData> out(new W3DQuadrupedDrawModuleData());
		env.blocks.registerBlock("TestQuadruped", [&](INI *ini) {
			ini->initFromINIMultiProc(out.get(), W3DQuadrupedDrawModuleData::buildFieldParse);
			while (!ini->isEOF())
			{
				ini->readLine();
			}
		});
		const std::string full = "TestQuadruped\n" + body;
		INI ini(env);
		ini.loadMemory("quad.ini", std::vector<std::uint8_t>(full.begin(), full.end()), INI_LOAD_OVERWRITE);
		return out;
	}

Matrix3D makeYawTransform(const Coord3D &pos, float angle, float scale = 1.0f)
{
	Matrix3D m;
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	m.Row[0][0] = c * scale;
	m.Row[0][1] = -s * scale;
	m.Row[0][2] = 0.0f;
	m.Row[0][3] = pos.x;
	m.Row[1][0] = s * scale;
	m.Row[1][1] = c * scale;
	m.Row[1][2] = 0.0f;
	m.Row[1][3] = pos.y;
	m.Row[2][0] = 0.0f;
	m.Row[2][1] = 0.0f;
	m.Row[2][2] = scale;
	m.Row[2][3] = pos.z;
	return m;
}

// Direct C++ model of RotWK RW 0x67B7AA (calcPhysicsXformHugeFourLegs) + RW 0x67BE90 (applyPhysicsXform)
// for a single step given four foot world positions [FL, FR, BL, BR] and object pose.
struct RwHugeFourLegsRef
{
	float pitch = 0.0f;
	float pitchRate = 0.0f;
	float roll = 0.0f;
	float rollRate = 0.0f;
	float accelPitch = 0.0f;
	float accelPitchRate = 0.0f;
	float accelRoll = 0.0f;
	float accelRollRate = 0.0f;
	float overlapZ = 0.0f;
	float overlapZVel = 0.0f;

	void step(const LocomotorTemplate &loco, const Coord3D &pos, float angle, const Coord3D points[4],
		const std::function<float(float, float)> &groundZ, float outBasis[9], Coord3D &outPos, float &outTotalPitch, float &outTotalZ)
	{
		float heights[4];
		for (int i = 0; i < 4; ++i)
		{
			if (i == 0 || points[i].x != points[i - 1].x || points[i].y != points[i - 1].y)
			{
				heights[i] = groundZ(points[i].x, points[i].y);
			}
			else
			{
				heights[i] = heights[i - 1];
			}
		}

		const float front = (heights[0] + heights[1]) * 0.5f;
		const float back = (heights[2] + heights[3]) * 0.5f;
		const float frontX = (points[0].x + points[1].x) * 0.5f;
		const float frontY = (points[0].y + points[1].y) * 0.5f;
		const float backX = (points[2].x + points[3].x) * 0.5f;
		const float backY = (points[2].y + points[3].y) * 0.5f;
		const float dx = frontX - backX;
		const float dy = frontY - backY;
		const float dist = std::sqrt(dx * dx + dy * dy);
		const float targetPitch = std::atan2(back - front, dist);

		pitchRate += (-loco.m_pitchStiffness * (pitch - targetPitch)) + (-loco.m_pitchDamping * pitchRate);
		if (pitchRate > 0.0f)
		{
			pitchRate *= 0.5f;
		}
		pitch += loco.m_uniformAxialDamping * pitchRate;
		roll += loco.m_uniformAxialDamping * rollRate;

		accelPitchRate += (-loco.m_pitchStiffness * accelPitch) + (-loco.m_pitchDamping * accelPitchRate);
		accelPitch += accelPitchRate;
		accelRollRate += (-loco.m_rollStiffness * accelRoll) + (-loco.m_rollDamping * accelRollRate);
		accelRoll += accelRollRate;

		outTotalPitch = accelPitch + pitch;
		const float totalRoll = accelRoll + roll;

		if (accelPitch > loco.m_accelerationPitchLimit) accelPitch = loco.m_accelerationPitchLimit;
		else if (accelPitch < -loco.m_accelerationPitchLimit) accelPitch = -loco.m_accelerationPitchLimit;
		if (accelRoll > loco.m_accelerationPitchLimit) accelRoll = loco.m_accelerationPitchLimit;
		else if (accelRoll < -loco.m_accelerationPitchLimit) accelRoll = -loco.m_accelerationPitchLimit;

		const float oldZ = overlapZ;
		const float wanted = (front + back) * 0.5f - pos.z;
		overlapZ += overlapZVel;
		if ((wanted >= oldZ && wanted <= overlapZ) || (wanted <= oldZ && wanted >= overlapZ))
		{
			overlapZ = wanted;
			overlapZVel = 0.0f;
		}
		else if ((wanted > overlapZ && overlapZVel < 0.0f) || (wanted < overlapZ && overlapZVel > 0.0f))
		{
			overlapZ -= overlapZVel;
			overlapZVel = 0.0f;
		}
		else if (wanted > overlapZ)
		{
			overlapZVel += 1.5f;
		}
		else
		{
			overlapZVel -= 0.5f;
		}
		outTotalZ = overlapZ;

		Matrix3D mtx;
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		mtx.Row[0][0] = c;  mtx.Row[0][1] = -s; mtx.Row[0][2] = 0.0f; mtx.Row[0][3] = pos.x;
		mtx.Row[1][0] = s;  mtx.Row[1][1] =  c; mtx.Row[1][2] = 0.0f; mtx.Row[1][3] = pos.y;
		mtx.Row[2][0] = 0.0f; mtx.Row[2][1] = 0.0f; mtx.Row[2][2] = 1.0f; mtx.Row[2][3] = pos.z;
		mtx.Translate(Vector3(0.0f, 0.0f, outTotalZ));
		mtx.Rotate_Y(outTotalPitch);
		mtx.Rotate_X(-totalRoll);
		mtx.Rotate_Z(0.0f);
		for (int r = 0; r < 3; ++r)
		{
			for (int col = 0; col < 3; ++col)
			{
				outBasis[r * 3 + col] = mtx.Row[r][col];
			}
		}
		outPos = Coord3D{ mtx.Row[0][3], mtx.Row[1][3], mtx.Row[2][3] };
	}
};

std::string quadrupedObjectIni(const std::string &name, const std::string &extraBones =
	"    LeftFrontFootBone  = e_FrontLeft\n"
	"    RightFrontFootBone = e_FrontRight\n"
	"    LeftRearFootBone   = e_BackLeft\n"
	"    RightRearFootBone  = e_BackRight\n")
{
	return
		"Object " + name + "\n"
		"  Draw = W3DQuadrupedDraw ModuleTag_01\n"
		"    ExtraPublicBone = e_FrontLeft\n"
		"    ExtraPublicBone = e_FrontRight\n"
		"    ExtraPublicBone = e_BackLeft\n"
		"    ExtraPublicBone = e_BackRight\n"
		"    DefaultModelConditionState\n"
		"      Model = MUMAK_SKN\n"
		"      Skeleton = MUMAK_SKL\n"
		"    End\n"
		"    IdleAnimationState\n"
		"      StateName = Idle\n"
		"    End\n"
		"    AnimationState = MOVING\n"
		"      Animation = Walk\n"
		"        AnimationName = MUMAK_WLKA\n"
		"        AnimationMode = LOOP\n"
		"      End\n"
		"    End\n" +
		extraBones +
		"  End\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 4.0\n"
		"  GeometryMinorRadius = 4.0\n"
		"  GeometryHeight = 20.0\n"
		"End\n";
}
} // namespace

TEST_CASE("PORT-126: W3DQuadrupedDraw::getFootLocations matches RotWK RW 0x4C9A39 / 0x4B2F19")
{
	MemoryFileSource fs;
	addQuadrupedAssets(fs, true, true, true, true, true);
	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	FixedRandom random;
	Harness harness;

	const std::string body =
		"DefaultModelConditionState\n"
		"  Model = MUMAK_SKN\n"
		"  Skeleton = MUMAK_SKL\n"
		"End\n"
		"ModelConditionState = USER_1\n"
		"  Model = MUMAK_ALT_SKN\n"
		"  Skeleton = MUMAK_ALT_SKL\n"
		"End\n"
		"ExtraPublicBone = e_FrontLeft\n"
		"ExtraPublicBone = e_FrontRight\n"
		"ExtraPublicBone = e_BackLeft\n"
		"ExtraPublicBone = e_BackRight\n"
		"IdleAnimationState\n"
		"  StateName = Idle\n"
		"End\n"
		"AnimationState = MOVING\n"
		"  Animation = Walk\n"
		"    AnimationName = MUMAK_WLKA\n"
		"    AnimationMode = LOOP\n"
		"  End\n"
		"End\n"
		"LeftFrontFootBone = E_FRONTLEFT\n" // case-insensitive HTree lookup
		"RightFrontFootBone = e_FrontRight\n"
		"LeftRearFootBone = e_BackLeft\n"
		"RightRearFootBone = e_BackRight\n"
		"End\n";

	std::unique_ptr<W3DQuadrupedDrawModuleData> quadData = parseQuadrupedBody(body);
	REQUIRE(quadData != nullptr);

	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;

	// Base W3DScriptedModelDraw returns false at ObjectDrawInterface slot 47 (RW 0x8F8014).
	W3DScriptedModelDraw baseDraw(*quadData, drawAssets, random, nullptr, opt);
	std::vector<Coord3D> feet;
	const Matrix3D renderMtx = makeYawTransform(Coord3D{ 100.0f, 50.0f, 10.0f }, 0.0f);
	CHECK_FALSE(baseDraw.getFootLocations(feet, false, renderMtx));

	// W3DQuadrupedDraw overrides getFootLocations (RW 0x4C9A39).
	W3DQuadrupedDraw quadDraw(*quadData, drawAssets, random, nullptr, opt);
	REQUIRE(quadDraw.getFootLocations(feet, false, renderMtx));
	REQUIRE(feet.size() == 4);

	CHECK(feet[0].x == 124.0f);
	CHECK(feet[0].y == 62.0f);
	CHECK(feet[0].z == 10.0f);
	CHECK(feet[1].x == 124.0f);
	CHECK(feet[1].y == 38.0f);
	CHECK(feet[1].z == 10.0f);
	CHECK(feet[2].x == 76.0f);
	CHECK(feet[2].y == 62.0f);
	CHECK(feet[2].z == 10.0f);
	CHECK(feet[3].x == 76.0f);
	CHECK(feet[3].y == 38.0f);
	CHECK(feet[3].z == 10.0f);

	// worldSpace = true requires non-null objectTransform (RW 0x4C9AA1..0x4C9AC4) and uses pristine pose.
	std::vector<Coord3D> worldFeet;
	CHECK_FALSE(quadDraw.getFootLocations(worldFeet, true, renderMtx, nullptr));
	REQUIRE(quadDraw.getFootLocations(worldFeet, true, Matrix3D(), &renderMtx));
	REQUIRE(worldFeet.size() == 4);
	CHECK(worldFeet[0].x == 124.0f);
	CHECK(worldFeet[0].y == 62.0f);
	CHECK(worldFeet[0].z == 10.0f);

	// Animated client pose (worldSpace = false): MOVING animation advances e_FrontLeft by +6 X, +4 Z at frame 3.
	quadDraw.setModelConditionFlags(flagsOf({ "MOVING" }));
	quadDraw.advance(100.0); // 3 frames at 30 fps
	REQUIRE(quadDraw.getFootLocations(feet, false, renderMtx));
	// tolerance: frame interpolation float rounding
	CHECK(feet[0].x == doctest::Approx(130.0f));
	CHECK(feet[0].y == 62.0f);
	// tolerance: frame interpolation float rounding
	CHECK(feet[0].z == doctest::Approx(14.0f));

	// worldSpace = true with conditionFlags == nullptr uses m_pendingFlags (RW +0x17C, getModelConditionFlags).
	quadDraw.setModelConditionFlags(flagsOf({ "USER_1" }));
	REQUIRE(quadDraw.getFootLocations(worldFeet, true, Matrix3D(), &renderMtx));
	CHECK(worldFeet[0].x == 130.0f);
	CHECK(worldFeet[0].y == 62.0f);
	CHECK(worldFeet[0].z == 12.0f);
	quadDraw.setModelConditionFlags(ModelConditionFlags());

	// RW 0x4B2F19 treats RootTransform / index 0 as missing bone.
	Matrix3D dummy;
	CHECK_FALSE(quadDraw.getCurrentWorldspaceClientBonePositions("ROOTTRANSFORM", renderMtx, dummy));
}

TEST_CASE("PORT-126: missing foot bone rules and sibling copy match RW 0x4C9A51..0x4C9E0E")
{
	// Missing RightFrontFootBone name -> copies LeftFrontFootBone.
	{
		MemoryFileSource fs;
		addQuadrupedAssets(fs, true, true, true, true, false);
		WW3DAssetManager assets(fs);
		WW3DDrawAssets drawAssets(assets);
		FixedRandom random;
		Harness harness;

		const std::string body =
			"DefaultModelConditionState\n  Model = MUMAK_SKN\n  Skeleton = MUMAK_SKL\nEnd\n"
			"IdleAnimationState\n  StateName = Idle\nEnd\n"
			"LeftFrontFootBone = e_FrontLeft\n"
			"LeftRearFootBone = e_BackLeft\n"
			"End\n";
		std::unique_ptr<W3DQuadrupedDrawModuleData> parsed = parseQuadrupedBody(body);
		REQUIRE(parsed != nullptr);
		W3DQuadrupedDraw draw(*parsed, drawAssets, random);
		std::vector<Coord3D> feet;
		REQUIRE(draw.getFootLocations(feet, false, makeYawTransform(Coord3D{ 0.0f, 0.0f, 0.0f }, 0.0f)));
		REQUIRE(feet.size() == 4);
		CHECK(feet[0].x == 24.0f);
		CHECK(feet[0].y == 12.0f);
		CHECK(feet[1].x == feet[0].x);
		CHECK(feet[1].y == feet[0].y);
		CHECK(feet[2].x == -24.0f);
		CHECK(feet[2].y == 12.0f);
		CHECK(feet[3].x == feet[2].x);
		CHECK(feet[3].y == feet[2].y);
	}

	// Both rear bones missing from HTree -> getFootLocations returns false even when front bones resolve.
	{
		MemoryFileSource fs;
		addQuadrupedAssets(fs, true, true, false, false, false);
		WW3DAssetManager assets(fs);
		WW3DDrawAssets drawAssets(assets);
		FixedRandom random;
		Harness harness;

		const std::string body =
			"DefaultModelConditionState\n  Model = MUMAK_SKN\n  Skeleton = MUMAK_SKL\nEnd\n"
			"IdleAnimationState\n  StateName = Idle\nEnd\n"
			"LeftFrontFootBone = e_FrontLeft\n"
			"RightFrontFootBone = e_FrontRight\n"
			"LeftRearFootBone = e_BackLeft\n"
			"RightRearFootBone = e_BackRight\n"
			"End\n";
		std::unique_ptr<W3DQuadrupedDrawModuleData> parsed = parseQuadrupedBody(body);
		REQUIRE(parsed != nullptr);
		W3DQuadrupedDraw draw(*parsed, drawAssets, random);
		std::vector<Coord3D> feet;
		CHECK_FALSE(draw.getFootLocations(feet, false, Matrix3D()));
	}
}

TEST_CASE("PORT-126: synthetic Mumakil tilts with ground on four feet on slope (matching RW) and stays unchanged on flat ground")
{
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules);
	REQUIRE(w.load(quadrupedObjectIni("TestMumakil")).empty());
	const ThingTemplate *tt = w.get("TestMumakil");
	REQUIRE(tt != nullptr);

	LocomotorTemplate mumakLoco;
	mumakLoco.m_appearance = LOCO_LEGS_FOUR_HUGE;
	mumakLoco.m_pitchStiffness = 0.1f;
	mumakLoco.m_rollStiffness = 0.1f;
	mumakLoco.m_pitchDamping = 0.9f;
	mumakLoco.m_rollDamping = 0.9f;
	mumakLoco.m_uniformAxialDamping = 1.0f;
	mumakLoco.m_accelerationPitchLimit = 0.35f;

	// Case 1: Flat ground -> body presentation transform matches un-tilted pose identically.
	{
		MemoryFileSource fs;
		addQuadrupedAssets(fs);
		WW3DAssetManager assets(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		DrawServices services(assets, w.modules, hostOpt);
		services.groundHeight = [](float /*x*/, float /*y*/, Coord3D *normal) -> float {
			if (normal) *normal = Coord3D{ 0.0f, 0.0f, 1.0f };
			return 12.5f;
		};

		std::vector<std::string> problems;
		Drawable d(services, tt, (DrawableID)1, (ObjectID)1, {}, problems);
		REQUIRE(problems.empty());

		ObjectSnapshot snap;
		snap.id = 1;
		snap.tmpl = tt;
		snap.position = Coord3D{ 100.0f, 100.0f, 12.5f };
		snap.previousPos = snap.position;
		snap.angle = 0.75f;
		snap.locomotorTemplate = &mumakLoco;

		Drawable::SyncPrep prep;
		d.prepareSync(snap, 1, 0.0, false, prep);
		d.commitSync(prep);
		d.applyPhysicsXform(snap, prep.pose);

		float presBasis[9];
		Coord3D presPos;
		d.getPresentationPose(presBasis, presPos);
		CHECK(presPos.x == 100.0f);
		CHECK(presPos.y == 100.0f);
		CHECK(presPos.z == 12.5f);
		for (int i = 0; i < 9; ++i)
		{
			// tolerance: trigonometric orientation matrix roundoff
			CHECK(presBasis[i] == doctest::Approx(d.getBasis()[i]));
		}
		REQUIRE(d.locoInfo() != nullptr);
		CHECK(d.locoInfo()->m_pitch == 0.0f);
		CHECK(d.locoInfo()->m_overlapZ == 0.0f);

		// Device transform from Drawable::entryTransform matches un-tilted pose on flat ground.
		REQUIRE(d.entries().size() == 1);
		Matrix3D entryMtx;
		CHECK_FALSE(d.entryTransform(d.entries()[0], entryMtx));
		CHECK(entryMtx.Row[0][3] == 100.0f);
		CHECK(entryMtx.Row[1][3] == 100.0f);
		CHECK(entryMtx.Row[2][3] == 12.5f);
		// tolerance: trigonometric orientation matrix roundoff
		CHECK(entryMtx.Row[2][2] == doctest::Approx(1.0f));
	}

	// Case 2: Non-linear foot-bone slope (flat at radius <= 4 geometry, stepped under +-24 foot bones)
	// proves foot bone placement drives both pitch AND overlapZ rather than GeometryMajorRadius=4 fallback.
	// Also verifies bit-exact parity over 20 frames against RotWK RW 0x67B7AA + 0x67BE90.
	{
		MemoryFileSource fs;
		addQuadrupedAssets(fs);
		WW3DAssetManager assets(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		DrawServices services(assets, w.modules, hostOpt);

		auto slopeFn = [](float x, float /*y*/) -> float {
			// Flat z = 0 for |x - 100| <= 10 (where GeometryMajorRadius=4 fallback samples),
			// sloped/elevated outside so front feet at x~124 hit z=12 and rear feet at x~76 hit z=2.
			if (x >= 115.0f) return 12.0f;
			if (x <= 85.0f) return 2.0f;
			return 0.0f;
		};
		services.groundHeight = [&](float x, float y, Coord3D *normal) -> float {
			if (normal) *normal = Coord3D{ 0.0f, 0.0f, 1.0f };
			return slopeFn(x, y);
		};

		std::vector<std::string> problems;
		Drawable d(services, tt, (DrawableID)2, (ObjectID)2, {}, problems);
		REQUIRE(problems.empty());
		REQUIRE(d.entries().size() == 1);
		auto *quadDraw = dynamic_cast<W3DQuadrupedDraw *>(d.entries()[0].draw.get());
		REQUIRE(quadDraw != nullptr);

		ObjectSnapshot snap;
		snap.id = 2;
		snap.tmpl = tt;
		snap.position = Coord3D{ 100.0f, 100.0f, 0.0f };
		snap.previousPos = snap.position;
		snap.angle = 0.0f;
		snap.locomotorTemplate = &mumakLoco;

		RwHugeFourLegsRef rw;
		for (int step = 0; step < 20; ++step)
		{
			Drawable::SyncPrep prep;
			d.prepareSync(snap, (std::uint32_t)(step + 1), 0.0, false, prep);
			d.commitSync(prep);

			// Compute RW reference feet using the current render transform before this step's physics update.
			Matrix3D preRender = makeYawTransform(prep.pose.position, prep.pose.angle, d.getInstanceScale());
			if (d.hasPhysicsXform())
			{
				const PhysicsXformInfo prevInfo = d.lastPhysicsInfo();
				preRender.Translate(Vector3(0.0f, 0.0f, prevInfo.m_totalZ));
				preRender.Rotate_Y(prevInfo.m_totalPitch);
				preRender.Rotate_X(-prevInfo.m_totalRoll);
				preRender.Rotate_Z(prevInfo.m_totalYaw);
			}
			std::vector<Coord3D> refFeet;
			REQUIRE(quadDraw->getFootLocations(refFeet, false, preRender));
			REQUIRE(refFeet.size() == 4);

			float rwBasis[9];
			Coord3D rwPos;
			float rwPitch = 0.0f;
			float rwZ = 0.0f;
			rw.step(mumakLoco, prep.pose.position, prep.pose.angle, refFeet.data(), slopeFn, rwBasis, rwPos, rwPitch, rwZ);

			d.applyPhysicsXform(snap, prep.pose);

			float gotBasis[9];
			Coord3D gotPos;
			d.getPresentationPose(gotBasis, gotPos);
			Matrix3D gotEntry;
			CHECK_FALSE(d.entryTransform(d.entries()[0], gotEntry));

			CHECK(gotPos.x == rwPos.x);
			CHECK(gotPos.y == rwPos.y);
			CHECK(gotPos.z == rwPos.z);
			for (int i = 0; i < 9; ++i)
			{
				// tolerance: identical floating-point transform equations
				CHECK(gotBasis[i] == doctest::Approx(rwBasis[i]));
				// tolerance: entryTransform writes the same presentation basis to device Matrix3D
				CHECK(gotEntry.Row[i / 3][i % 3] == doctest::Approx(rwBasis[i]));
			}
			CHECK(gotEntry.Row[0][3] == rwPos.x);
			CHECK(gotEntry.Row[1][3] == rwPos.y);
			CHECK(gotEntry.Row[2][3] == rwPos.z);
			CHECK(d.lastPhysicsInfo().m_totalPitch == rwPitch);
			CHECK(d.lastPhysicsInfo().m_totalZ == rwZ);
		}

		// Nose pitched upward (negative Y rotation lifts +X toward +Z) and body raised to average ground height (7.0).
		// tolerance: spring-damper pitch settled toward negative slope angle
		CHECK(d.lastPhysicsInfo().m_totalPitch < -0.15f);
		// tolerance: overlapZ settled near (12 + 2) / 2 = 7.0
		CHECK(d.lastPhysicsInfo().m_totalZ > 5.0f);
		// Simulation position remains strictly separate from presentation pose.
		CHECK(d.getPosition()->z == 0.0f);
	}

	// Case 3: Rotated Mumakil facing arbitrary yaw (+pi/2 -> facing +Y) on a Y-sloped hill tilts forward along +Y.
	{
		MemoryFileSource fs;
		addQuadrupedAssets(fs);
		WW3DAssetManager assets(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		DrawServices services(assets, w.modules, hostOpt);

		auto hillY = [](float /*x*/, float y) -> float { return 0.25f * y; };
		services.groundHeight = [&](float x, float y, Coord3D *normal) -> float {
			if (normal) *normal = Coord3D{ 0.0f, -0.2425f, 0.9701f };
			return hillY(x, y);
		};

		std::vector<std::string> problems;
		Drawable d(services, tt, (DrawableID)3, (ObjectID)3, {}, problems);
		REQUIRE(problems.empty());

		ObjectSnapshot snap;
		snap.id = 3;
		snap.tmpl = tt;
		snap.position = Coord3D{ 50.0f, 80.0f, hillY(50.0f, 80.0f) };
		snap.previousPos = snap.position;
		snap.angle = 1.57079632679f; // +90 deg: model +X axis points along world +Y (uphill)
		{
			const float c = std::cos(snap.angle);
			const float s = std::sin(snap.angle);
			snap.basis[0] = c; snap.basis[1] = -s; snap.basis[2] = 0.0f;
			snap.basis[3] = s; snap.basis[4] =  c; snap.basis[5] = 0.0f;
			snap.basis[6] = 0.0f; snap.basis[7] = 0.0f; snap.basis[8] = 1.0f;
		}
		snap.locomotorTemplate = &mumakLoco;

		Drawable::SyncPrep prep;
		d.prepareSync(snap, 1, 0.0, false, prep);
		d.commitSync(prep);
		d.applyPhysicsXform(snap, prep.pose);

		// Local forward vector (1, 0, 0) in device transform should point uphill (positive Z component).
		Matrix3D deviceMtx;
		d.entryTransform(d.entries()[0], deviceMtx);
		const Vector3 worldForward = deviceMtx.Rotate_Vector(Vector3(1.0f, 0.0f, 0.0f));
		// tolerance: nose pitches up along world +Y uphill slope
		CHECK(worldForward.Z > 0.0f);
		// tolerance: forward remains aligned along +Y
		CHECK(worldForward.Y > 0.9f);
	}

	// Case 4: Object without W3DQuadrupedDraw foot bones (plain W3DScriptedModelDraw) still falls back to geometry bounds.
	{
		REQUIRE(w.load(quadrupedObjectIni("TestPlainMonster", ""), INI_LOAD_OVERWRITE, "plain.ini").empty());
		const ThingTemplate *plainTt = w.get("TestPlainMonster");
		REQUIRE(plainTt != nullptr);

		MemoryFileSource fs;
		addQuadrupedAssets(fs);
		WW3DAssetManager assets(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		DrawServices services(assets, w.modules, hostOpt);
		services.groundHeight = [](float x, float /*y*/, Coord3D *normal) -> float {
			if (normal) *normal = Coord3D{ 0.0f, 0.0f, 1.0f };
			// Only elevated far outside GeometryMajorRadius=4, under +-24 foot bones
			if (x >= 115.0f) return 12.0f;
			return 0.0f;
		};

		std::vector<std::string> problems;
		Drawable d(services, plainTt, (DrawableID)4, (ObjectID)4, {}, problems);
		REQUIRE(problems.empty());

		ObjectSnapshot snap;
		snap.id = 4;
		snap.tmpl = plainTt;
		snap.position = Coord3D{ 100.0f, 100.0f, 0.0f };
		snap.previousPos = snap.position;
		snap.angle = 0.0f;
		snap.locomotorTemplate = &mumakLoco;

		Drawable::SyncPrep prep;
		d.prepareSync(snap, 1, 0.0, false, prep);
		d.commitSync(prep);
		d.applyPhysicsXform(snap, prep.pose);

		// Geometry fallback (radius 4) never reaches x=115, so overlapZ stays 0.
		CHECK(d.lastPhysicsInfo().m_totalZ == 0.0f);
		CHECK(d.lastPhysicsInfo().m_totalPitch == 0.0f);
	}

	// Case 5: Scale = 2.0 doubles foot offsets in world space so scaled feet at +-48 reach distant step while scale=1 feet (+-24) do not.
	{
		const std::string scaledIni =
			"Object TestScaledMumak\n"
			"  Scale = 2.0\n"
			"  Draw = W3DQuadrupedDraw ModuleTag_01\n"
			"    ExtraPublicBone = e_FrontLeft\n"
			"    ExtraPublicBone = e_FrontRight\n"
			"    ExtraPublicBone = e_BackLeft\n"
			"    ExtraPublicBone = e_BackRight\n"
			"    DefaultModelConditionState\n      Model = MUMAK_SKN\n      Skeleton = MUMAK_SKL\n    End\n"
			"    IdleAnimationState\n      StateName = Idle\n    End\n"
			"    LeftFrontFootBone = e_FrontLeft\n"
			"    RightFrontFootBone = e_FrontRight\n"
			"    LeftRearFootBone = e_BackLeft\n"
			"    RightRearFootBone = e_BackRight\n"
			"  End\n"
			"  Geometry = CYLINDER\n  GeometryMajorRadius = 4.0\n  GeometryMinorRadius = 4.0\n  GeometryHeight = 20.0\n"
			"End\n";
		REQUIRE(w.load(scaledIni, INI_LOAD_OVERWRITE, "scaled.ini").empty());
		const ThingTemplate *scaledTt = w.get("TestScaledMumak");
		REQUIRE(scaledTt != nullptr);

		MemoryFileSource fs;
		addQuadrupedAssets(fs);
		WW3DAssetManager assets(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		DrawServices services(assets, w.modules, hostOpt);
		services.groundHeight = [](float x, float /*y*/, Coord3D *normal) -> float {
			if (normal) *normal = Coord3D{ 0.0f, 0.0f, 1.0f };
			// Raised step at x >= 140: reached only when foot offsets (+24) are scaled by Scale=2.0 (+48 -> x=148).
			return x >= 140.0f ? 10.0f : 0.0f;
		};

		std::vector<std::string> problems;
		Drawable d(services, scaledTt, (DrawableID)5, (ObjectID)5, {}, problems);
		REQUIRE(problems.empty());

		ObjectSnapshot snap;
		snap.id = 5;
		snap.tmpl = scaledTt;
		snap.position = Coord3D{ 100.0f, 100.0f, 0.0f };
		snap.previousPos = snap.position;
		snap.angle = 0.0f;
		snap.locomotorTemplate = &mumakLoco;

		for (int step = 0; step < 2; ++step)
		{
			Drawable::SyncPrep prep;
			d.prepareSync(snap, (std::uint32_t)(step + 1), 0.0, false, prep);
			d.commitSync(prep);
			d.applyPhysicsXform(snap, prep.pose);
		}

		// front = 10, back = 0, dx = 96 -> pitch < 0 and wanted = +5 -> overlapZ steps to +1.5 on second tick
		// tolerance: non-zero pitch confirms scaled feet reached x=148 step
		CHECK(d.lastPhysicsInfo().m_totalPitch < 0.0f);
		// tolerance: positive overlapZ confirms front foot sample hit z=10
		CHECK(d.lastPhysicsInfo().m_totalZ > 0.0f);

		// Post-construction Drawable::setInstanceScale updates worldSpace foot queries live.
		REQUIRE(d.entries().size() == 1);
		auto *quadDraw = dynamic_cast<W3DQuadrupedDraw *>(d.entries()[0].draw.get());
		REQUIRE(quadDraw != nullptr);
		d.setInstanceScale(3.0f);
		Matrix3D originXform = makeYawTransform(Coord3D{ 0.0f, 0.0f, 0.0f }, 0.0f);
		std::vector<Coord3D> rescaledFeet;
		REQUIRE(quadDraw->getFootLocations(rescaledFeet, true, Matrix3D(), &originXform));
		CHECK(rescaledFeet[0].x == 72.0f);
		CHECK(rescaledFeet[0].y == 36.0f);
	}

	// Case 6: Interpolated moving/rotated Mumakil (interpolate = true -> RenderInterpolation::Pose keeps zRotation = true
	// with identity basis and yaw in pose.angle) still samples rotated foot bones and preserves yaw in getPresentationPose.
	{
		MemoryFileSource fs;
		addQuadrupedAssets(fs);
		WW3DAssetManager assets(fs);
		W3DLuaDrawScriptHost::Options hostOpt;
		DrawServices services(assets, w.modules, hostOpt);

		auto hillY = [](float /*x*/, float y) -> float { return 0.25f * y; };
		services.groundHeight = [&](float /*x*/, float y, Coord3D *normal) -> float {
			if (normal) *normal = Coord3D{ 0.0f, -0.2425f, 0.9701f };
			return hillY(0.0f, y);
		};

		std::vector<std::string> problems;
		Drawable d(services, tt, (DrawableID)6, (ObjectID)6, {}, problems);
		REQUIRE(problems.empty());

		ObjectSnapshot snap;
		snap.id = 6;
		snap.tmpl = tt;
		snap.position = Coord3D{ 50.0f, 80.0f, hillY(0.0f, 80.0f) };
		snap.previousPos = Coord3D{ 50.0f, 75.0f, hillY(0.0f, 75.0f) };
		snap.angle = 1.57079632679f; // +90 deg: facing +Y uphill
		snap.recordedAngle = snap.angle;
		snap.lastMovedFrame = 2; // smoothPose sets zRotation = true and leaves pose.basis as identity
		snap.locomotorTemplate = &mumakLoco;

		Drawable::SyncPrep prep;
		d.prepareSync(snap, 2, 0.5, true, prep);
		REQUIRE(prep.pose.zRotation);
		d.commitSync(prep);
		d.applyPhysicsXform(snap, prep.pose);

		float presBasis[9];
		Coord3D presPos;
		d.getPresentationPose(presBasis, presPos);
		// tolerance: +X local axis rotated +90 deg points along world +Y
		CHECK(presBasis[3] > 0.9f);
		Matrix3D deviceMtx;
		d.entryTransform(d.entries()[0], deviceMtx);
		const Vector3 worldForward = deviceMtx.Rotate_Vector(Vector3(1.0f, 0.0f, 0.0f));
		// tolerance: interpolated yaw +90 deg places feet along world Y so Mumakil pitches uphill
		CHECK(worldForward.Z > 0.0f);
		// tolerance: interpolated forward remains aligned with world +Y
		CHECK(worldForward.Y > 0.9f);
	}
}
