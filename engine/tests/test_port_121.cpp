// OpenBFME unit tests. GPL-3.0. PORT-121 (issue #305): a builder's water flies upward when he puts out a fire (P-61).
//
// The behaviour under test is the LOCAL TRANSFORM a ParticleSysBone system is created with, and therefore the direction its particles leave in.
// Every expectation is worked out from the RotWK instructions and from the mod's own data, never read back from the code under test:
//
//   * The mod's data (Ennorath 1.9.8.2): every porter's UNPACKING state (PACKING_TYPE_1) carries `ParticleSysBone = BUCKET PorterFireWater
//     FollowBone:YES` (`object/goodfaction/units/men/porter.ini:99`, and the same line in the other porter files); `FXParticleSystem
//     PorterFireWater` (`fxparticlesystem.ini:54349`) emits through `EmissionVelocity = OrthoEmissionVelocity` with X -2..2, Y -2..5, Z 2..10 -
//     the jet runs along the EMITTER's local +Z.
//   * RW 0x5F4C6B (ParticleSystem::generateParticleInfo): the velocity the emission module draws is rotated by the system's transform
//     (`info.vel = M * vel`), so the jet's direction is the system transform's local +Z.
//   * RW 0x4C6514 (the ParticleSysBone creation): a bone with a FollowBone record takes the RENDER OBJECT's bone transform - RW 0x4C678F
//     `mov ecx,[ebx+0x50]` / `call [eax+0xCC]` (Get_Bone_Transform by index) and RW 0x4C67B6 `call 0x5F2F55` (`setLocalTransform`); a record
//     without FollowBone takes its translation and Get_Z_Rotation (RW 0x4C69BE / RW 0x4C6A31 `call 0x5F2F2D` / RW 0x4C6A4B `call 0x5F31B0`).
//     The render object holds the pose the draw SHOWS (the hierarchy's posed pivots), not the hierarchy's bind pose - so the system is born on
//     the animated bone, arm included.
//   * The port's old path read the bind pose (`LiveFX::modelBone`: HTreeClass::Base_Pose), so a bucket bone whose bind transform carries no arm
//     rotation left the jet on the model's own +Z - straight up for an upright builder. That is P-61.
//
// Synthetic data only (W3dSynth): a skeleton whose HAND sits at (0, 0, 2) with an unrotated bind pose, and an animation that moves the hand to
// (0, 3, -1) and pitches it 90 degrees about X, so the two poses cannot be confused: the bind pose's local +Z is world +Z (upward), the animated
// one's is world -Y (forward).

#include "doctest.h"

#include "FxTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/LiveFX.h"
#include "GameClient/ParticleBoneFollow.h"
#include "GameClient/ParticleSys.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace drawtest;
using namespace w3dsynth;

namespace
{

const double kHalfPi = 3.14159265358979323846 / 2.0;

// the client random of the draw module: a fixed answer, so only the pose can move a system
class FixedRandom : public W3DDrawRandom
{
public:
	int value(int lo, int hi) override { return lo == hi ? lo : (lo + hi) / 2; }
	float real(float lo, float hi) override { return (lo + hi) * 0.5f; }
};

// the pivot's matrix out of a pose the hierarchy evaluates - the test's own arithmetic, not the code under test
struct PoseOf
{
	HTreePose pose;
	explicit PoseOf(const W3DScriptedModelDraw &draw)
	{
		W3DDrawPoseView v;
		draw.poseView(v);
		REQUIRE(draw.currentModel() != nullptr);
		draw.currentModel()->Tree->Anim_Pose(Matrix3D(), static_cast<const HAnimClass *>(v.clipKey0), v.frame0, pose);
	}
	Matrix3D at(int pivot) const
	{
		REQUIRE(pose.Transform.size() == 2); // the synthetic hierarchy is ROOTTRANSFORM + HAND, nothing else
		return pose.Transform[(std::size_t)pivot];
	}
};

Matrix3D bindPoseAt(const W3DScriptedModelDraw &draw, int pivot)
{
	HTreePose base;
	REQUIRE(draw.currentModel() != nullptr);
	draw.currentModel()->Tree->Base_Pose(Matrix3D(), base);
	REQUIRE(base.Transform.size() == 2); // the synthetic hierarchy is ROOTTRANSFORM + HAND, nothing else
	return base.Transform[(std::size_t)pivot];
}

// a system's transform applied to a velocity the way RW 0x5F4C6B does (the 3x3 part alone)
Coord3D rotateVelocity(const Matrix3D &M, const Coord3D &v)
{
	return Coord3D{ (M.Row[0][2] * v.z + M.Row[0][1] * v.y) + M.Row[0][0] * v.x, (M.Row[1][2] * v.z + M.Row[1][1] * v.y) + M.Row[1][0] * v.x,
		(M.Row[2][2] * v.z + M.Row[2][1] * v.y) + M.Row[2][0] * v.x };
}

// the synthetic soldier: ROOTTRANSFORM (0) -> HAND (1). The bind pose leaves the hand unrotated at (0, 0, 2); HAND_ANIM moves it to (0, 3, -1)
// and pitches it 90 degrees about X, so its local +Z turns from world +Z into world -Y.
struct Draw
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	FixedRandom random;
	std::unique_ptr<W3DScriptedModelDraw> draw;
	W3DModelDrawModuleData *data = nullptr;

	explicit Draw(const std::string &moduleFlags, bool withAnimation = true)
	{
		fs.Add("art\\w3d\\ab\\abunit_skl.w3d",
			hierarchyChunk("ABUNIT_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("HAND", 0, 0.0f, 0.0f, 2.0f) }));
		std::vector<std::uint8_t> bytes;
		SynthMesh body;
		body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
		body.Triangles = { 0, 1, 2, 0, 2, 3 };
		body.Container = "ABUNIT_SKN";
		body.Name = "BODY";
		append(bytes, meshChunk(body));
		append(bytes, hlodChunk("ABUNIT_SKN", "ABUNIT_SKL", { { "ABUNIT_SKN.BODY", 1 } }));
		fs.Add("art\\w3d\\ab\\abunit_skn.w3d", bytes);
		// HAND_ANIM, 11 frames at 30 fps: the hand's translation (0, 3, -1) and a 90 degree pitch about X (+X = sin 45, +W = cos 45)
		std::vector<SynthChannel> channels;
		channels.push_back(SynthChannel{ ANIM_CHANNEL_Y, 1, 0, 1, std::vector<float>(11, 3.0f) });
		channels.push_back(SynthChannel{ ANIM_CHANNEL_Z, 1, 0, 1, std::vector<float>(11, -1.0f) });
		std::vector<float> q;
		for (int i = 0; i < 11; ++i)
		{
			q.push_back((float)std::sin(kHalfPi / 2.0));
			q.push_back(0.0f);
			q.push_back(0.0f);
			q.push_back((float)std::cos(kHalfPi / 2.0));
		}
		channels.push_back(SynthChannel{ ANIM_CHANNEL_Q, 1, 0, 4, q });
		fs.Add("art\\w3d\\ab\\abunit_handanim.w3d", rawAnimChunk("ABUNIT_HANDANIM", "ABUNIT_SKL", 11, 30, channels));
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		const std::string idle = withAnimation
			? "IdleAnimationState\n  StateName = STATE_Idle\n  Animation = HANDA\n    AnimationName = ABUNIT_HANDANIM\n    AnimationMode = LOOP\n  End\nEnd\n"
			: "IdleAnimationState\n  StateName = STATE_Idle\nEnd\n";
		data = harness.parse(moduleFlags +
			"DefaultModelConditionState\n  Model = ABUNIT_SKN\n  Skeleton = ABUNIT_SKL\nEnd\n" + idle + "End\n");
		REQUIRE_MESSAGE(data, harness.error);
		W3DScriptedModelDraw::Options o;
		o.buildBones = false;
		draw.reset(new W3DScriptedModelDraw(*data, *drawAssets, random, nullptr, o));
		REQUIRE(draw->errors().empty());
	}
};

} // namespace

// ---- the pose source of the creation (RW 0x4C6514) ------------------------------------------------------------------------------
TEST_CASE("PORT-121 poseBone: the creation's bone is the draw's CURRENT pose, not the bind pose (RW 0x4C678F, RW 0x4C69BE)")
{
	Draw d("ParticlesAttachedToAnimatedBones = Yes\n");
	const Matrix3D animated = PoseOf(*d.draw).at(1);
	const Matrix3D bind = bindPoseAt(*d.draw, 1);
	Matrix3D got;
	REQUIRE(ParticleBoneFollow::poseBone(*d.draw, 1.0f, 1, got));
	// the translation is the animation's, not the hierarchy's
	CHECK(got.Row[0][3] == doctest::Approx(animated.Row[0][3]).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[1][3] == doctest::Approx(animated.Row[1][3]).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[2][3] == doctest::Approx(animated.Row[2][3]).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[1][3] == doctest::Approx(3.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[2][3] == doctest::Approx(-1.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(bind.Row[1][3] == doctest::Approx(0.0f).epsilon(1e-5)); // // tolerance: float32 engine value vs the test's own float32 re-derivation (the bind pose's hand, which the old path used)
	CHECK(bind.Row[2][3] == doctest::Approx(2.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	// and the axes: the animated bone's local +Z (where the ortho jet runs) is world -Y; the bind pose's is world +Z
	CHECK(got.Row[0][2] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[1][2] == doctest::Approx(-1.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[2][2] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(bind.Row[0][2] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(bind.Row[1][2] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(bind.Row[2][2] == doctest::Approx(1.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	// the drawable scale multiplies the whole matrix (RW 0x478180, read by the creation at RW 0x4C68F7 .. 0x4C697E)
	Matrix3D scaled;
	REQUIRE(ParticleBoneFollow::poseBone(*d.draw, 2.0f, 1, scaled));
	CHECK(scaled.Row[1][2] == doctest::Approx(-2.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(scaled.Row[1][3] == doctest::Approx(6.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	// pivot 0 is the root, which the record never names (RW 0x4C671A treats index 0 as "no such bone")
	Matrix3D root;
	CHECK_FALSE(ParticleBoneFollow::poseBone(*d.draw, 1.0f, 0, root));
}

TEST_CASE("PORT-121 poseBone: a state without an animation answers the bind pose (RW 0x4BDC2D poses nothing)")
{
	Draw d("", false);
	Matrix3D got;
	REQUIRE(ParticleBoneFollow::poseBone(*d.draw, 1.0f, 1, got));
	CHECK(got.Row[2][3] == doctest::Approx(2.0f).epsilon(1e-5)); // // tolerance: float32 engine value vs the test's own float32 re-derivation (the hierarchy's own hand translation)
	CHECK(got.Row[1][3] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(got.Row[2][2] == doctest::Approx(1.0f).epsilon(1e-5)); // // tolerance: float32 engine value vs the test's own float32 re-derivation (no pitch: local +Z is world +Z)
}

// ---- the jet: the particle velocity is the system transform's local +Z -----------------------------------------------------------
// PorterFireWater's EmissionVelocity is an OrthoEmissionVelocity with X -2..2, Y -2..5, Z 2..10 (fxparticlesystem.ini:54349): its jet is the
// emitter's own +Z (always 2..10, so never zero). RW 0x5F4C6B then rotates the drawn velocity by the system's transform. The exact numbers are
// taken from a second system with a single-valued jet (Z 6 6), which no random can move.
TEST_CASE("PORT-121 jet: a system created on the animated bone sends the water where the builder faces (RW 0x5F4C6B + RW 0x4C67B6)")
{
	const std::string ini = fxtest::systemText("PorterFireWater",
								  "    Lifetime = 20 20\n    BurstCount = 1 1\n    BurstDelay = 0 0\n",
								  "  EmissionVolume = PointEmissionVolume\n  End\n"
								  "  EmissionVelocity = OrthoEmissionVelocity\n    X = -2 2\n    Y = -2 5\n    Z = 2 10\n  End\n") +
		fxtest::systemText("JetSix", "    Lifetime = 20 20\n    BurstCount = 1 1\n    BurstDelay = 0 0\n",
			"  EmissionVolume = PointEmissionVolume\n  End\n"
			"  EmissionVelocity = OrthoEmissionVelocity\n    X = 0 0\n    Y = 0 0\n    Z = 6 6\n  End\n");
	fxtest::Sim sim(ini);

	Draw d("ParticlesAttachedToAnimatedBones = Yes\n");
	const Matrix3D animated = PoseOf(*d.draw).at(1);
	const Matrix3D bind = bindPoseAt(*d.draw, 1);

	// the exact jet: the emitter's own (0, 0, 6) turned by the two transforms
	const FXParticleSystem::ParticleSystemID six = sim.spawn("JetSix");
	FXParticleSystem::ParticleSystem *sixSys = sim.mgr->findParticleSystemByID(six);
	REQUIRE(sixSys);
	// the creation's local transform (RW 0x5F2F55), with no parent transform (RW 0x5F32A7, the walk's setSkipParentXfrm)
	sixSys->setLocalTransform(animated);
	sixSys->setSkipParentXfrm(true);
	sim.step();
	REQUIRE(!sim.particles(six).empty());
	const FXParticleSystem::Particle &p = *sim.particles(six)[0];
	const Coord3D turned = rotateVelocity(animated, Coord3D{ 0.0f, 0.0f, 6.0f });
	CHECK(p.vel.x == doctest::Approx(turned.x).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(p.vel.y == doctest::Approx(turned.y).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(p.vel.z == doctest::Approx(turned.z).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	// the animated bone pitches the hand 90 degrees about X, so its local +Z is world -Y: the water leaves horizontally
	CHECK(p.vel.x == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(p.vel.y == doctest::Approx(-6.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(p.vel.z == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	// the bind pose's matrix - what the creation read before this lane - leaves the same jet along world +Z: straight up. That is P-61
	const Coord3D up = rotateVelocity(bind, Coord3D{ 0.0f, 0.0f, 6.0f });
	CHECK(up.x == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(up.y == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation
	CHECK(up.z == doctest::Approx(6.0f).epsilon(1e-5)); // tolerance: float32 engine value vs the test's own float32 re-derivation

}

// ---- the neighbouring behaviour: a record without FollowBone, and the score of where the creation read the bone -------------------
TEST_CASE("PORT-121 neighbour: the bind pose answers where the pose cannot, and a record without FollowBone keeps its position form")
{
	Draw d("ParticlesAttachedToAnimatedBones = Yes\n");
	// a pivot the hierarchy does not have: the pose cannot answer, so the creation falls back to the bind pose read of the bone name
	Matrix3D missing;
	CHECK_FALSE(ParticleBoneFollow::poseBone(*d.draw, 1.0f, 7, missing));
	// a null clip (a state without an animation) answers the bind pose, which is what the draw shows then
	Matrix3D bind;
	REQUIRE(ParticleBoneFollow::poseBone(*d.draw, 1.0f, 1, bind));

	// the record's FollowBone is what picks the matrix form: the walk of RW 0x4B6D5F ends on the end-of-draw argument (1, RW 0x4C6D78) at the
	// first record whose FollowBone is off (RW 0x4B6DD5), so a position-only system is left where the creation put it
	fxtest::Sim sim(fxtest::systemText("PorterFireWater",
		"    Lifetime = 20 20\n    BurstCount = 1 1\n    BurstDelay = 0 0\n", fxtest::kPointOrtho));
	const FXParticleSystem::ParticleSystemID id = sim.spawn("PorterFireWater");
	FXParticleSystem::ParticleSystem *sys = sim.mgr->findParticleSystemByID(id);
	REQUIRE(sys);
	Matrix3D placed; // the creation's position-only form: identity rotation, the bone's translation
	placed.Row[2][3] = 2.0f;
	sys->setLocalTransform(placed);
	std::vector<ParticleBoneFollow::Record> records(1);
	records[0].system = id;
	records[0].entry = 0;
	records[0].boneName = "hand";
	records[0].boneIndex = 1;
	records[0].followBone = false;
	records[0].checkDrawable = false;
	ParticleBoneFollow::Pose pose;
	pose.draw = d.draw.get();
	pose.scale = 1.0f;
	pose.follow = true;
	ParticleBoneFollow::Counters counters;
	ParticleBoneFollow::followFrame(records, *sim.mgr, &pose, 1, &counters, 1);
	CHECK(counters.followed == 0);

	// the counters of the report keep the two creation paths apart
	LiveFX::Stats stats;
	CHECK(stats.bonesCreatedOnPose == 0);
	CHECK(stats.bonesCreatedOnBindPose == 0);
}
