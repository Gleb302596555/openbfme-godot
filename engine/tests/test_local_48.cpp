// OpenBFME unit tests. LOCAL-48 (issue #404, P-81 visual part):
//   1. Held objects: a grabbed orc follows the troll's animated hand bone (TRUNK01 / Trunk01) in
//      both position and 3D bone orientation, and smoothly blends out from that hand pose on release.
//   2. Held firing riders (isPassengerAllowedToFire, RW 0x865A7E) keep the animated bone position
//      and tilt while rotating XY yaw to match rider aim/logic yaw (RW 0x4B6952 -> 0x4B6960..0x4B6A40).
//   3. Torn-off interactable trees: W3DTreeDraw never puts a staticModel on a live Drawable
//      (RW 0xBE3010 slot 11 = RW 0x9F3A3C ret 4), so ModelConditionState = PASSENGER CLUB Model = None
//      on W3DScriptedModelDraw hides the whole tree mesh instead of leaving PTree01 sliding over the log.
//
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LogicSnapshot.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Contain/OpenContainRuntime.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace combattest;
using namespace w3dsynth;

namespace
{

std::vector<std::uint8_t> simpleMeshHlod(const char *sklName, const char *hlodName, const char *meshName)
{
	SynthMesh m;
	m.Name = meshName;
	m.Container = hlodName;
	m.Vertices = {
		W3dVectorStruct{ -1.0f, -1.0f, 0.0f },
		W3dVectorStruct{ 1.0f, -1.0f, 0.0f },
		W3dVectorStruct{ 0.0f, 1.0f, 1.0f }
	};
	m.Triangles = { 0, 1, 2 };
	m.Shaders = { opaqueShader() };
	m.VertexMaterials = { SynthVMat{} };
	SynthPass pass;
	pass.VertexMaterialIds = { 0 };
	pass.ShaderIds = { 0 };
	m.Passes.push_back(std::move(pass));

	std::vector<std::uint8_t> out = meshChunk(m);
	std::string subName = std::string(hlodName) + "." + meshName;
	append(out, hlodChunk(hlodName, sklName, { SynthSub{ subName, 0 } }));
	return out;
}

struct TrollHandAssets
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;

	TrollHandAssets()
	{
		// Skeleton L48TROLL_SKL: pivot 0 ROOTTRANSFORM, pivot 1 B_WAIST (0,0,10), pivot 2 TRUNK01 (5,0,10).
		std::vector<W3dPivotStruct> trollPivots(3);
		std::memset(trollPivots.data(), 0, trollPivots.size() * sizeof(W3dPivotStruct));
		setName(trollPivots[0].Name, sizeof(trollPivots[0].Name), "ROOTTRANSFORM");
		trollPivots[0].ParentIdx = 0xffffffffu;
		trollPivots[0].Rotation.Q[3] = 1.0f;
		setName(trollPivots[1].Name, sizeof(trollPivots[1].Name), "B_WAIST");
		trollPivots[1].ParentIdx = 0;
		trollPivots[1].Translation = { 0.0f, 0.0f, 10.0f };
		trollPivots[1].Rotation.Q[3] = 1.0f;
		setName(trollPivots[2].Name, sizeof(trollPivots[2].Name), "TRUNK01");
		trollPivots[2].ParentIdx = 1;
		trollPivots[2].Translation = { 5.0f, 0.0f, 10.0f };
		trollPivots[2].Rotation.Q[3] = 1.0f;
		fs.Add("art\\w3d\\l4\\l48troll_skl.w3d", hierarchyChunk("L48TROLL_SKL", trollPivots));
		fs.Add("art\\w3d\\l4\\l48troll_skn.w3d", simpleMeshHlod("L48TROLL_SKL", "L48TROLL_SKN", "BODY"));

		// 11-frame looping clip (30 fps): peak at frame 5 (166.7 ms) so stepping 5 frames lands before wrap.
		// At frame 5: TRUNK01 translates +10 in Y, +10 in Z and rotates 90 deg around X.
		constexpr int kFrames = 11;
		std::vector<float> yVals(kFrames), zVals(kFrames), qVals(kFrames * 4);
		constexpr float kHalfPi = 1.57079632679f;
		for (int f = 0; f < kFrames; ++f)
		{
			const float t = (f <= 5) ? (float(f) / 5.0f) : (float(10 - f) / 5.0f);
			yVals[f] = 10.0f * t;
			zVals[f] = 10.0f * t;
			const float half = 0.5f * kHalfPi * t;
			qVals[f * 4 + 0] = std::sin(half);
			qVals[f * 4 + 1] = 0.0f;
			qVals[f * 4 + 2] = 0.0f;
			qVals[f * 4 + 3] = std::cos(half);
		}
		fs.Add("art\\w3d\\l4\\l48troll_idle.w3d",
			rawAnimChunk("L48TROLL_IDLE", "L48TROLL_SKL", kFrames, 30, {
				SynthChannel{ ANIM_CHANNEL_Y, 2, 0, 1, yVals },
				SynthChannel{ ANIM_CHANNEL_Z, 2, 0, 1, zVals },
				SynthChannel{ ANIM_CHANNEL_Q, 2, 0, 4, qVals }
			}));

		// Swaying howdah clip for firing-rider test: at frame 5, TRUNK01 translates +6 in Z and rotates 60 deg around Z.
		std::vector<float> hzVals(kFrames), hqVals(kFrames * 4);
		constexpr float kSixty = 1.0471975512f;
		for (int f = 0; f < kFrames; ++f)
		{
			const float t = (f <= 5) ? (float(f) / 5.0f) : (float(10 - f) / 5.0f);
			hzVals[f] = 6.0f * t;
			const float half = 0.5f * kSixty * t;
			hqVals[f * 4 + 0] = 0.0f;
			hqVals[f * 4 + 1] = 0.0f;
			hqVals[f * 4 + 2] = std::sin(half);
			hqVals[f * 4 + 3] = std::cos(half);
		}
		fs.Add("art\\w3d\\l4\\l48howdah_sway.w3d",
			rawAnimChunk("L48HOWDAH_SWAY", "L48TROLL_SKL", kFrames, 30, {
				SynthChannel{ ANIM_CHANNEL_Z, 2, 0, 1, hzVals },
				SynthChannel{ ANIM_CHANNEL_Q, 2, 0, 4, hqVals }
			}));

		// Orc / archer model
		std::vector<W3dPivotStruct> orcPivots(1);
		std::memset(orcPivots.data(), 0, sizeof(W3dPivotStruct));
		setName(orcPivots[0].Name, sizeof(orcPivots[0].Name), "ROOTTRANSFORM");
		orcPivots[0].ParentIdx = 0xffffffffu;
		orcPivots[0].Rotation.Q[3] = 1.0f;
		fs.Add("art\\w3d\\l4\\l48orc_skl.w3d", hierarchyChunk("L48ORC_SKL", orcPivots));
		fs.Add("art\\w3d\\l4\\l48orc_skn.w3d", simpleMeshHlod("L48ORC_SKL", "L48ORC_SKN", "ORC"));

		// Interactable tree model
		std::vector<W3dPivotStruct> treePivots(1);
		std::memset(treePivots.data(), 0, sizeof(W3dPivotStruct));
		setName(treePivots[0].Name, sizeof(treePivots[0].Name), "ROOTTRANSFORM");
		treePivots[0].ParentIdx = 0xffffffffu;
		treePivots[0].Rotation.Q[3] = 1.0f;
		fs.Add("art\\w3d\\l4\\l48tree_skl.w3d", hierarchyChunk("L48TREE_SKL", treePivots));
		fs.Add("art\\w3d\\l4\\l48tree_skn.w3d", simpleMeshHlod("L48TREE_SKL", "L48TREE_SKN", "TREE"));

		assets = std::make_unique<WW3DAssetManager>(fs);
	}
};

class TrollBones : public ProjectileLaunchOffsets
{
public:
	bool launchOffset(const Object &, int, int, float[12]) override { return false; }
	int multiLogicalBones(const Object &, const std::string &prefix, const std::array<std::uint32_t, 19> &, int maxBones, float (*out)[12]) override
	{
		if (prefix != "TRUNK" || maxBones < 1)
		{
			return 0;
		}
		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				out[0][r * 4 + c] = (r == c) ? 1.0f : 0.0f;
			}
		}
		out[0][3] = 5.0f;
		out[0][7] = 0.0f;
		out[0][11] = 20.0f;
		return 1;
	}
};

const char kLocal48Objects[] =
	"Object L48Troll\n"
	"  KindOf = MONSTER SELECTABLE\n"
	"  Scale = 1.5\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryHeight = 40\n"
	"  Draw = W3DScriptedModelDraw ModuleTag_01\n"
	"    DefaultModelConditionState\n"
	"      Model = L48TROLL_SKN\n"
	"      Skeleton = L48TROLL_SKL\n"
	"    End\n"
	"    IdleAnimationState\n"
	"      StateName = STATE_Idle\n"
	"      Animation = IdleA\n"
	"        AnimationName = L48TROLL_IDLE\n"
	"        AnimationMode = LOOP\n"
	"      End\n"
	"    End\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 1\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    PassengerBonePrefix = PassengerBone:TRUNK KindOf:INFANTRY\n"
	"    ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE\n"
	"    ReleaseSnappyness = 0.05\n"
	"  End\n"
	"End\n"
	"Object L48FiringCarrier\n"
	"  KindOf = MONSTER SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Draw = W3DScriptedModelDraw ModuleTag_01\n"
	"    DefaultModelConditionState\n"
	"      Model = L48TROLL_SKN\n"
	"      Skeleton = L48TROLL_SKL\n"
	"    End\n"
	"    IdleAnimationState\n"
	"      StateName = STATE_Idle\n"
	"      Animation = IdleA\n"
	"        AnimationName = L48HOWDAH_SWAY\n"
	"        AnimationMode = LOOP\n"
	"      End\n"
	"    End\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 1\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    PassengerBonePrefix = PassengerBone:TRUNK KindOf:INFANTRY\n"
	"    ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE CAN_ATTACK\n"
	"  End\n"
	"End\n"
	"Object L48Orc\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Draw = W3DScriptedModelDraw ModuleTag_01\n"
	"    DefaultModelConditionState\n"
	"      Model = L48ORC_SKN\n"
	"      Skeleton = L48ORC_SKL\n"
	"    End\n"
	"    IdleAnimationState\n"
	"      StateName = STATE_Idle\n"
	"    End\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object L48InteractTree\n"
	"  KindOf = IMMOBILE SHRUBBERY OPTIMIZED_TREE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 30\n"
	"  Draw = W3DTreeDraw ModuleTag_Opt_01\n"
	"    ModelName = L48TREE_SKN\n"
	"    TextureName = PTreeShare.tga\n"
	"  End\n"
	"  Draw = W3DScriptedModelDraw ModuleTag_01\n"
	"    DefaultModelConditionState\n"
	"      Model = L48TREE_SKN\n"
	"      Skeleton = L48TREE_SKL\n"
	"    End\n"
	"    ModelConditionState = PASSENGER CLUB\n"
	"      Model = None\n"
	"    End\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

struct Local48World : CombatWorld
{
	Local48World() : CombatWorld("")
	{
		W3DDrawModules::registerTypedDrawModuleData(w.modules);
		const std::string err = w.load(kLocal48Objects, INI_LOAD_OVERWRITE, "local48.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

struct HooksGuard
{
	GameLogic &logic;
	~HooksGuard() { logic.setClientHooks(nullptr); }
};

float colLength(const float *basis, int col)
{
	const float x = basis[0 * 3 + col];
	const float y = basis[1 * 3 + col];
	const float z = basis[2 * 3 + col];
	return std::sqrt(x * x + y * y + z * z);
}

} // namespace

TEST_CASE("LOCAL-48: held orc follows troll hand bone TRUNK01 in position, 3D orientation, and release blend")
{
	TrollHandAssets hand;
	Local48World w;
	TrollBones bones;
	w.logic->combat().setLaunchOffsets(&bones);

	DrawableManager dm(*hand.assets, *w.logic);
	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	HooksGuard unbind{ *w.logic };

	Object *troll = w.unit("L48Troll", 'A', 100.0f, 200.0f);
	Object *orc = w.unit("L48Orc", 'B', 10.0f, 20.0f);
	REQUIRE(troll != nullptr);
	REQUIRE(orc != nullptr);
	orc->setOrientation(0.5f); // distinctly non-zero initial orientation before grab

	w.frames(1);
	dm.applyEvents(events.take());

	const Drawable *trollDraw = dm.findByObject(troll->getID());
	Drawable *orcDraw = dm.findByObject(orc->getID());
	REQUIRE(trollDraw != nullptr);
	REQUIRE(orcDraw != nullptr);
	CHECK(trollDraw->getInstanceScale() == 1.5f);

	ContainModuleInterface *c = troll->getContain();
	REQUIRE(c != nullptr);
	REQUIRE(c->isValidContainerFor(*orc, true, false));
	REQUIRE(c->addToContain(orc));
	w.frames(1);
	dm.applyEvents(events.take());

	const std::shared_ptr<const LogicSnapshot> snap = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	REQUIRE(snap.get() != nullptr);
	const ObjectSnapshot *orcRec = snap->find(orc->getID());
	REQUIRE(orcRec != nullptr);
	CHECK(orcRec->containedOnBone);
	CHECK(orcRec->containedBoneOwner == troll->getID());
	CHECK(orcRec->containedBone == "TRUNK01");
	CHECK_FALSE(orcRec->containedRidersCanAttack);

	// Frame 0: orc presentation follows TRUNK01 rest position (100 + 1.5*5, 200, 1.5*20) = (107.5, 200, 30)
	dm.syncTransforms(*snap, 0.0, true);
	Matrix3D boneF0;
	REQUIRE(trollDraw->boneTransform("TRUNK01", boneF0));
	float poseBasis0[9];
	Coord3D posePos0;
	orcDraw->getPresentationPose(poseBasis0, posePos0);
	CHECK(posePos0.x == doctest::Approx(boneF0.Row[0][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	CHECK(posePos0.y == doctest::Approx(boneF0.Row[1][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	CHECK(posePos0.z == doctest::Approx(boneF0.Row[2][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip

	// Advance troll hand animation by 5 frames (~166.7 ms): TRUNK01 swings +15 in Y/Z (scaled 1.5x) and rotates 90 deg around X.
	dm.advance(166.7);
	dm.syncTransforms(*snap, 0.0, true);
	Matrix3D boneF5;
	REQUIRE(trollDraw->boneTransform("TRUNK01", boneF5));
	CHECK(boneF5.Row[1][3] > 210.0f); // tolerance: lower bound confirming bone moved from y=200 along +15 swing
	CHECK(boneF5.Row[2][3] > 40.0f); // tolerance: lower bound confirming bone moved from z=30 along +15 swing

	float poseBasis5[9];
	Coord3D posePos5;
	orcDraw->getPresentationPose(poseBasis5, posePos5);
	CHECK(posePos5.x == doctest::Approx(boneF5.Row[0][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	CHECK(posePos5.y == doctest::Approx(boneF5.Row[1][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	CHECK(posePos5.z == doctest::Approx(boneF5.Row[2][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	// 3D orientation tracks the hand bone's +90 deg X tilt (local Z tilted into -Y), and stays unit-length (not 1.5x troll scale)
	CHECK(poseBasis5[1 * 3 + 2] == doctest::Approx(-1.0f).epsilon(0.02)); // tolerance: 166.7ms step slightly over 5.000 frames
	CHECK(poseBasis5[2 * 3 + 2] == doctest::Approx(0.0f).epsilon(0.02)); // tolerance: 166.7ms step slightly over 5.000 frames
	CHECK(colLength(poseBasis5, 0) == doctest::Approx(1.0f).epsilon(0.001)); // tolerance: float32 column renormalization
	CHECK(colLength(poseBasis5, 1) == doctest::Approx(1.0f).epsilon(0.001)); // tolerance: float32 column renormalization
	CHECK(colLength(poseBasis5, 2) == doctest::Approx(1.0f).epsilon(0.001)); // tolerance: float32 column renormalization

	// Release (PORT-105 + RW 0x86ADAE -> RW 0x67476B): orc snapshots the lifted bone pose and smoothly blends toward drop position.
	const Coord3D dropPos{ 115.0f, 200.0f, 0.0f };
	orc->setPosition(&dropPos);
	c->removeFromContain(orc);
	w.frames(1);
	dm.applyEvents(events.take());
	const std::shared_ptr<const LogicSnapshot> snapReleased = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	REQUIRE(snapReleased.get() != nullptr);
	const ObjectSnapshot *relRec = snapReleased->find(orc->getID());
	REQUIRE(relRec != nullptr);
	CHECK_FALSE(relRec->containedOnBone);

	dm.advance(33.34);
	dm.syncTransforms(*snapReleased, 0.0, true);
	float relBasis[9];
	Coord3D relPos;
	orcDraw->getPresentationPose(relBasis, relPos);
	// Early in ReleaseSnappyness=0.05 blend: still near the lifted bone (well above ground z=0), not snapped to ground.
	CHECK(relPos.z > 30.0f); // tolerance: mid-blend height above ground during ReleaseSnappyness=0.05
	CHECK(relPos.z < boneF5.Row[2][3]); // tolerance: blend started descending from lifted hand height
}

TEST_CASE("LOCAL-48: passenger allowed to fire keeps bone position while yaw matches rider aim (RW 0x4B6952 -> 0x4B6960)")
{
	TrollHandAssets hand;
	Local48World w;
	TrollBones bones;
	w.logic->combat().setLaunchOffsets(&bones);

	DrawableManager dm(*hand.assets, *w.logic);
	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	HooksGuard unbind{ *w.logic };

	Object *carrier = w.unit("L48FiringCarrier", 'A', 100.0f, 200.0f);
	Object *archer = w.unit("L48Orc", 'A', 100.0f, 200.0f);
	REQUIRE(carrier != nullptr);
	REQUIRE(archer != nullptr);

	w.frames(1);
	dm.applyEvents(events.take());

	ContainModuleInterface *c = carrier->getContain();
	REQUIRE(c != nullptr);
	REQUIRE(c->addToContain(archer));
	w.frames(1);
	dm.applyEvents(events.take());

	// Rotate archer in logic (simulating aim at a target +90 deg) and step a frame so recorded + current agree.
	constexpr float kAimYaw = 1.57079632679f;
	archer->setOrientation(kAimYaw);
	w.frames(1);
	dm.applyEvents(events.take());

	const std::shared_ptr<const LogicSnapshot> snap = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	REQUIRE(snap.get() != nullptr);
	const ObjectSnapshot *rec = snap->find(archer->getID());
	REQUIRE(rec != nullptr);
	CHECK(rec->containedOnBone);
	CHECK(rec->containedRidersCanAttack);

	dm.advance(166.7);
	dm.syncTransforms(*snap, 0.0, true);

	const Drawable *carrierDraw = dm.findByObject(carrier->getID());
	const Drawable *archerDraw = dm.findByObject(archer->getID());
	REQUIRE(carrierDraw != nullptr);
	REQUIRE(archerDraw != nullptr);

	Matrix3D boneMtx;
	REQUIRE(carrierDraw->boneTransform("TRUNK01", boneMtx));
	float poseBasis[9];
	Coord3D posePos;
	archerDraw->getPresentationPose(poseBasis, posePos);

	CHECK(posePos.x == doctest::Approx(boneMtx.Row[0][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	CHECK(posePos.y == doctest::Approx(boneMtx.Row[1][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	CHECK(posePos.z == doctest::Approx(boneMtx.Row[2][3]).epsilon(0.01)); // tolerance: float32 animation/matrix round-trip
	const float drawnYaw = std::atan2(poseBasis[3], poseBasis[0]);
	const float boneYaw = std::atan2(boneMtx.Row[1][0], boneMtx.Row[0][0]);
	CHECK(boneYaw > 0.5f); // tolerance: lower bound confirming swaying bone rotated ~60 deg around Z
	CHECK(drawnYaw == doctest::Approx(kAimYaw).epsilon(0.02)); // tolerance: float32 Rotate_Z atan2/sin/cos composition
}

TEST_CASE("LOCAL-48: interactable tree W3DTreeDraw leaves staticModel empty so PASSENGER CLUB Model = None hides it completely (RW 0xBE3010)")
{
	TrollHandAssets hand;
	Local48World w;
	DrawableManager dm(*hand.assets, *w.logic);
	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	HooksGuard unbind{ *w.logic };

	Object *tree = w.unit("L48InteractTree", 'N', 300.0f, 400.0f);
	REQUIRE(tree != nullptr);
	w.frames(1);
	dm.applyEvents(events.take());

	Drawable *treeDraw = dm.findByObject(tree->getID());
	REQUIRE(treeDraw != nullptr);
	REQUIRE(treeDraw->entries().size() == 2);
	CHECK(treeDraw->entries()[0].kind == W3D_DRAWKIND_TREE);
	CHECK(treeDraw->entries()[0].staticModel.empty());
	CHECK(treeDraw->entries()[1].kind == W3D_DRAWKIND_MODEL);
	REQUIRE(treeDraw->entries()[1].draw != nullptr);
	CHECK(treeDraw->entries()[1].draw->currentModel() != nullptr);

	// Troll grabFinish applies ModelConditionState = PASSENGER CLUB on the carried tree.
	tree->setModelConditionState(CombatNames::modelCondition("PASSENGER"), true);
	tree->setModelConditionState(CombatNames::modelCondition("CLUB"), true);
	w.frames(1);
	dm.applyEvents(events.take());

	CHECK(treeDraw->entries()[0].staticModel.empty());
	CHECK(treeDraw->entries()[1].draw->currentModel() == nullptr);
}
