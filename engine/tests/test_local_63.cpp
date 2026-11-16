// OpenBFME unit tests for Local 63 (issue #508, P-129; plus P-132 CaptureFlag visibility check):
// Dozer construction completion on a CastleBehavior structure clears only AWAITING_CONSTRUCTION and extends
// PARTIALLY_CONSTRUCTED and ACTIVELY_BEING_CONSTRUCTED for LOGICFRAMES_PER_SECOND (5) frames via SMCHelper
// so structures with DefaultModelConditionState { Model = None } do not drop to an invisible model before
// CastleBehavior unpacks on the finished site.
//
// TARGET FACTS (both editions: RotWK 2.01 game.dat and DONOR BFME2 1.06 game.dat):
//   * RW 0x88DEFD..0x88DF51 / BFME2 0x88AC08..0x88AC5C (DozerAIUpdateCtor.cpp:2358-2369):
//     When DozerAIUpdate completes construction of `goalObject`, it checks goalObject->findModule("CastleBehavior").
//     - If CastleBehavior is present: clears AWAITING_CONSTRUCTION (0x43) and calls
//       setSpecialModelConditionState(PARTIALLY_CONSTRUCTED (0x44), 5) and
//       setSpecialModelConditionState(ACTIVELY_BEING_CONSTRUCTED (0x45), 5).
//     - Otherwise: clears AWAITING_CONSTRUCTION, PARTIALLY_CONSTRUCTED, and ACTIVELY_BEING_CONSTRUCTED immediately.

#include "doctest.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "ProdTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LiveGame.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameLogic/BuildPlacement.h"
#include "GameLogic/Construction.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/hanim.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace
{
class NoRandom : public W3DDrawRandom
{
public:
	int value(int lo, int) override { return lo; }
	float real(float lo, float) override { return lo; }
	std::vector<std::string> unverified() const override { return { "LOCAL-63 test double" }; }
};

struct DrawCheck
{
	w3dsynth::MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	drawtest::Harness harness;
	NoRandom random;
	W3DModelDrawModuleData *data = nullptr;
	std::unique_ptr<W3DScriptedModelDraw> draw;

	explicit DrawCheck(const char *drawIni)
	{
		using namespace w3dsynth;
		fs.Add("art\\w3d\\l6\\l63_skl.w3d", hierarchyChunk("L63_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("BASE", 0, 0, 0, 0) }));
		auto makeBox = [](const char *name) {
			std::vector<std::uint8_t> bytes;
			SynthMesh m;
			m.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 } };
			m.Triangles = { 0, 1, 2 };
			m.Container = name;
			m.Name = "BODY";
			append(bytes, meshChunk(m));
			append(bytes, hlodChunk(name, "L63_SKL", { { std::string(name) + ".BODY", 1 } }));
			return bytes;
		};
		fs.Add("art\\w3d\\l6\\l63_bld.w3d", makeBox("L63_BLD"));
		fs.Add("art\\w3d\\l6\\l63_done.w3d", makeBox("L63_DONE"));
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		data = harness.parse(drawIni);
		REQUIRE_MESSAGE(data != nullptr, harness.error);
		W3DScriptedModelDraw::Options o;
		o.buildBones = false;
		draw.reset(new W3DScriptedModelDraw(*data, *drawAssets, random, nullptr, o));
	}

	std::string syncFromObject(const Object &obj)
	{
		std::string conds;
		const char *names[] = {
			"AWAITING_CONSTRUCTION",
			"PARTIALLY_CONSTRUCTED",
			"ACTIVELY_BEING_CONSTRUCTED",
			"CONSTRUCTION_COMPLETE",
		};
		for (const char *n : names)
		{
			if (obj.testModelCondition(Construction::modelConditionIndex(n)))
			{
				if (!conds.empty())
				{
					conds += ' ';
				}
				conds += n;
			}
		}
		const ModelConditionFlags f = harness.parseFlags(conds.c_str());
		REQUIRE_MESSAGE(harness.error.empty(), harness.error);
		draw->setModelConditionFlags(f);
		return draw->frame().model ? draw->frame().modelName : std::string();
	}
};

const char kSynthObjects[] =
	"Object Local63Dozer\n"
	"  KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS INFANTRY DOZER\n"
	"  Behavior = DozerAIUpdate ModuleTag_DozerAI\n"
	"  End\n"
	"End\n"
	"Object Local63Unpack\n"
	"  BuildTime = 10.0\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE CASTLE_CENTER\n"
	"  Behavior = GettingBuiltBehavior ModuleTag_GB\n"
	"  End\n"
	"  Behavior = CastleBehavior ModuleTag_Castle\n"
	"    CastleToUnpackForFaction = TestFactionOther Layout_Unused\n"
	"    InstantUnpack = Yes\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"    InitialHealth = 1000\n"
	"    MaxHealthDamaged = 500\n"
	"    MaxHealthReallyDamaged = 100\n"
	"  End\n"
	"End\n"
	"Object Local63Plain\n"
	"  BuildTime = 10.0\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Behavior = GettingBuiltBehavior ModuleTag_GB\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"    InitialHealth = 1000\n"
	"    MaxHealthDamaged = 500\n"
	"    MaxHealthReallyDamaged = 100\n"
	"  End\n"
	"End\n";

const char kUnpackDraw[] =
	"DefaultModelConditionState\n  Model = None\nEnd\n"
	"ModelConditionState = AWAITING_CONSTRUCTION\n  Model = L63_BLD\nEnd\n"
	"ModelConditionState = ACTIVELY_BEING_CONSTRUCTED PARTIALLY_CONSTRUCTED\n  Model = L63_BLD\nEnd\n"
	"End\n";

const char kPlainDraw[] =
	"DefaultModelConditionState\n  Model = L63_DONE\nEnd\n"
	"ModelConditionState = ACTIVELY_BEING_CONSTRUCTED PARTIALLY_CONSTRUCTED\n  Model = L63_BLD\nEnd\n"
	"End\n";

GameMessage selectMsg(int player, ObjectID id)
{
	GameMessage m(MSG_CREATE_SELECTED_GROUP, player);
	m.appendBooleanArgument(true);
	m.appendObjectIDArgument(id);
	return m;
}

GameMessage dozerMsg(int player, const ThingTemplate &t, const Coord3D &loc, float angle)
{
	GameMessage m(MSG_DOZER_CONSTRUCT, player);
	m.appendIntegerArgument((int)t.getTemplateID());
	m.appendLocationArgument(loc);
	m.appendRealArgument(angle);
	return m;
}

bool isDrawableVisible(const Drawable *d, std::string *modelOut = nullptr, std::string *stateOut = nullptr)
{
	if (!d)
	{
		return false;
	}
	for (const DrawEntry &e : d->entries())
	{
		if (e.kind == W3D_DRAWKIND_MODEL && e.draw)
		{
			const W3DDrawFrame fr = e.draw->frame();
			if (modelOut)
			{
				*modelOut = fr.modelName;
			}
			if (stateOut)
			{
				*stateOut = fr.animationStateName;
			}
			// GodotGameWorld::refresh_view: shown = f.model && !e.moduleHidden && !fogHidden && !e.draw->hiddenByFade()
			// and opacity must be positive to render visibly.
			if (fr.model != nullptr && !e.moduleHidden && !e.draw->hiddenByFade() && d->drawOpacity() > 0.0f) // tolerance: > 0 opacity is visible
			{
				return true;
			}
		}
	}
	return false;
}
} // namespace

TEST_CASE("LOCAL-63 synthetic: Dozer completion holds PARTIALLY_CONSTRUCTED and ACTIVELY_BEING_CONSTRUCTED for 5 frames only on CastleBehavior structures (RW 0x88DEFD / BFME2 0x88AC08)")
{
	prodtest::ProdWorld w;
	w.load(kSynthObjects);

	const int awaiting = Construction::modelConditionIndex("AWAITING_CONSTRUCTION");
	const int partial = Construction::modelConditionIndex("PARTIALLY_CONSTRUCTED");
	const int active = Construction::modelConditionIndex("ACTIVELY_BEING_CONSTRUCTED");
	const int complete = Construction::modelConditionIndex("CONSTRUCTION_COMPLETE");

	Object *dozer = w.make("Local63Dozer", w.teamOf("Alice"));
	Object *unpack = w.make("Local63Unpack", w.teamOf("Alice"));
	Object *plain = w.make("Local63Plain", w.teamOf("Alice"));
	REQUIRE(dozer != nullptr);
	REQUIRE(unpack != nullptr);
	REQUIRE(plain != nullptr);

	for (Object *s : { unpack, plain })
	{
		s->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
		s->setConstructionPercent(99.0f);
		Construction::setModelConditions(*s, {}, { "AWAITING_CONSTRUCTION", "PARTIALLY_CONSTRUCTED", "ACTIVELY_BEING_CONSTRUCTED" });
	}

	DrawCheck unpackDraw(kUnpackDraw);
	DrawCheck plainDraw(kPlainDraw);
	CHECK(unpackDraw.syncFromObject(*unpack) == "L63_BLD");
	CHECK(plainDraw.syncFromObject(*plain) == "L63_BLD");

	Construction::completeConstruction(*unpack, dozer);
	Construction::completeConstruction(*plain, dozer);

	// Non-castle structure clears all three construction flags immediately and shows finished model.
	CHECK_FALSE(plain->testModelCondition(awaiting));
	CHECK_FALSE(plain->testModelCondition(partial));
	CHECK_FALSE(plain->testModelCondition(active));
	CHECK(plain->testModelCondition(complete));
	CHECK(plainDraw.syncFromObject(*plain) == "L63_DONE");

	// CastleBehavior structure clears AWAITING_CONSTRUCTION, but extends PARTIALLY_CONSTRUCTED &
	// ACTIVELY_BEING_CONSTRUCTED for 5 frames (F .. F+4), preserving build model rather than Model = None.
	CHECK_FALSE(unpack->testModelCondition(awaiting));
	CHECK(unpack->testModelCondition(partial));
	CHECK(unpack->testModelCondition(active));
	CHECK(unpack->testModelCondition(complete));
	CHECK(unpackDraw.syncFromObject(*unpack) == "L63_BLD");

	for (int f = 1; f < LOGICFRAMES_PER_SECOND; ++f)
	{
		w.logic->runLogicFrame();
		CHECK(unpack->testModelCondition(partial));
		CHECK(unpack->testModelCondition(active));
		CHECK(unpackDraw.syncFromObject(*unpack) == "L63_BLD");
	}

	// Expiry happens on frame F + 5 (LOGICFRAMES_PER_SECOND elapsed).
	w.logic->runLogicFrame();
	CHECK_FALSE(unpack->testModelCondition(partial));
	CHECK_FALSE(unpack->testModelCondition(active));
	CHECK(unpackDraw.syncFromObject(*unpack).empty());
}

TEST_CASE("LOCAL-63 ennorath P-129: Isengard unpack buildings stay visible across construction completion through instant unpack")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-63 P-129 Isengard unpack"))
	{
		return;
	}
	const char *templates[] = { "IsengardUrukPitUnpack", "IsengardArmoryUnpack" };
	for (const char *tmplName : templates)
	{
		INFO("template = " << tmplName);
		hudtest::Rig game(world.shared, "map mp fall back 4p", "FactionIsengard");
		GameLogic &logic = game.logic();
		Player *me = game.local;
		me->getMoney()->deposit(50000u, nullptr, false);

		std::string err;
		const Coord3D site = game.freeSpot(1800, 1800, 220.0f);
		Object *porter = game.game->createObject("IsengardPorter", game.index(), Coord3D{ site.x - 120.0f, site.y, site.z }, 0.0f, &err);
		REQUIRE_MESSAGE(porter != nullptr, err);
		const ThingTemplate *tt = logic.things().findTemplate(tmplName);
		REQUIRE(tt != nullptr);
		tt = tt->getFinalOverride();
		logic.runLogicFrame();

		std::set<ObjectID> before;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			before.insert(o->getID());
		}
		game.game->commands().append(selectMsg(game.index(), porter->getID()));
		game.game->commands().append(dozerMsg(game.index(), *tt, site, 0.0f));

		Object *unpack = nullptr;
		for (int i = 0; i < 300 && !unpack; ++i)
		{
			logic.runLogicFrame();
			for (int c = 0; c < 6; ++c)
			{
				game.game->refreshClient(1000.0 / 30.0, (double)c / 6.0);
			}
			for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
			{
				if (!before.count(o->getID()) && o->getTemplate()->getFinalOverride() == tt)
				{
					unpack = o;
				}
			}
		}
		REQUIRE_MESSAGE(unpack != nullptr, "porter did not place foundation");
		const ObjectID unpackId = unpack->getID();

		ObjectID keepId = INVALID_ID;
		bool sawCompletion = false;
		int framesAfterUnpack = 0;
		bool startedRising = false;
		int invisibleSamples = 0;
		int completeFrame = -1;

		for (int step = 0; step < 3000 && framesAfterUnpack < 10; ++step)
		{
			logic.runLogicFrame();
			unpack = logic.findObjectByID(unpackId);
			if (unpack)
			{
				if (!sawCompletion && !unpack->isUnderConstruction() && unpack->getConstructionPercent() < 0.0f)
				{
					sawCompletion = true;
					completeFrame = (int)logic.getFrame();
				}
				if (keepId == INVALID_ID)
				{
					if (CastleBehavior *cb = dynamic_cast<CastleBehavior *>(unpack->findModule("CastleBehavior")))
					{
						if (cb->state() == CastleBehavior::STATE_UNPACKED && cb->keepId() != INVALID_ID)
						{
							keepId = cb->keepId();
						}
					}
				}
			}
			Object *keep = keepId != INVALID_ID ? logic.findObjectByID(keepId) : nullptr;
			if (keep)
			{
				++framesAfterUnpack;
			}

			for (int c = 0; c < 6; ++c)
			{
				game.game->refreshClient(1000.0 / 30.0, (double)c / 6.0);
				const Drawable *ud = game.game->drawables().findByObject(unpackId);
				const Drawable *kd = keepId != INVALID_ID ? game.game->drawables().findByObject(keepId) : nullptr;
				std::string uModel, uState, kModel, kState;
				const bool uVis = isDrawableVisible(ud, &uModel, &uState);
				const bool kVis = isDrawableVisible(kd, &kModel, &kState);

				if (uVis && unpack && unpack->isUnderConstruction() && unpack->getConstructionPercent() > 0.0f) // tolerance: rising started
				{
					startedRising = true;
				}

				if (startedRising && (unpack || keep))
				{
					if (!uVis && !kVis)
					{
						++invisibleSamples;
						if (invisibleSamples <= 8)
						{
							std::printf("LOCAL-63 P-129 [%s] INVISIBLE f=%u c=%d (completeF=%d) uPct=%.1f uVis=%d(model='%s',fade=%d,op=%.2f) kVis=%d(model='%s',fade=%d,op=%.2f)\n",
							            tmplName, (unsigned)logic.getFrame(), c, completeFrame,
							            unpack ? unpack->getConstructionPercent() : -99.0f,
							            uVis ? 1 : 0, uModel.c_str(), ud ? (ud->fade().hidden ? 1 : 0) : -1, ud ? ud->drawOpacity() : -1.0f,
							            kVis ? 1 : 0, kModel.c_str(), kd ? (kd->fade().hidden ? 1 : 0) : -1, kd ? kd->drawOpacity() : -1.0f);
						}
					}
				}
			}
		}

		CHECK(sawCompletion);
		CHECK(startedRising);
		CHECK(keepId != INVALID_ID);
		Object *keep = keepId != INVALID_ID ? logic.findObjectByID(keepId) : nullptr;
		REQUIRE(keep != nullptr);
		CHECK_FALSE(keep->isDestroyed());
		CHECK(invisibleSamples == 0);
	}
}

TEST_CASE("LOCAL-63 ennorath P-132: CaptureFlag stays visible across START_CAPTURE, CANCEL_CAPTURE and CAPTURED")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-63 P-132 CaptureFlag probe"))
	{
		return;
	}
	hudtest::Rig game(world.shared, "map mp fall back 4p", "FactionIsengard");
	GameLogic &logic = game.logic();
	Player *neutral = game.game->players().getNeutralPlayer();
	REQUIRE(neutral != nullptr);

	std::string err;
	const Coord3D flagSpot = game.freeSpot(1800, 1800, 120.0f);
	Object *flag = game.game->createObject("CaptureFlag", neutral->getPlayerIndex(), flagSpot, 0.0f, &err);
	REQUIRE_MESSAGE(flag != nullptr, err);
	const Coord3D unitSpot = game.freeSpot(flagSpot.x, flagSpot.y - 160.0f, 80.0f);
	Object *horde = game.game->createObject("IsengardFighterHorde", game.index(), unitSpot, 0.0f, &err);
	REQUIRE_MESSAGE(horde != nullptr, err);
	horde = testutil::completeHorde(horde);
	REQUIRE(horde != nullptr);

	auto stepFrames = [&](int n, const char *phase, int &invisCount) {
		for (int i = 0; i < n; ++i)
		{
			logic.runLogicFrame();
			for (int c = 0; c < 6; ++c)
			{
				game.game->refreshClient(1000.0 / 30.0, (double)c / 6.0);
				const Drawable *fd = game.game->drawables().findByObject(flag->getID());
				std::string model, state;
				if (!isDrawableVisible(fd, &model, &state))
				{
					++invisCount;
					if (invisCount <= 5)
					{
						std::printf("LOCAL-63 P-132 INVISIBLE phase=%s f=%u c=%d model='%s' state='%s' fadeHidden=%d op=%.2f\n",
						            phase, (unsigned)logic.getFrame(), c, model.c_str(), state.c_str(),
						            fd ? (fd->fade().hidden ? 1 : 0) : -1, fd ? fd->drawOpacity() : -1.0f);
					}
				}
			}
		}
	};

	Object *outpost = game.game->createObject("Outpost", neutral->getPlayerIndex(), Coord3D{ flagSpot.x + 120.0f, flagSpot.y, flagSpot.z }, 0.0f, &err);
	REQUIRE_MESSAGE(outpost != nullptr, err);
	int invis = 0;
	stepFrames(15, "idle", invis); // let Shroud reveal CaptureFlag

	// Find the capture SpecialPowerModule (type 0x1D = SPECIAL_INFANTRY_CAPTURE_BUILDING) on the horde.
	const SpecialPowerModule *capture = nullptr;
	for (const std::unique_ptr<BehaviorModule> &m : horde->modules())
	{
		const SpecialPowerModule *sp = dynamic_cast<const SpecialPowerModule *>(m.get());
		if (sp && sp->getSpecialPowerTemplate() && sp->getSpecialPowerTemplate()->m_type == 0x1D)
		{
			capture = sp;
			break;
		}
	}
	REQUIRE(capture != nullptr);
	auto issueCapture = [&]() {
		GameMessage cast(MSG_DO_SPECIAL_POWER_AT_OBJECT, game.index());
		cast.appendIntegerArgument((int)capture->getSpecialPowerTemplate()->getID());
		cast.appendObjectIDArgument(flag->getID());
		cast.appendIntegerArgument(0);
		cast.appendObjectIDArgument(horde->getID());
		game.game->commands().append(cast);
	};
	issueCapture();
	stepFrames(2, "ordered", invis);

	const int startBit = Construction::modelConditionIndex("START_CAPTURE");
	bool started = false;
	for (int i = 0; i < 400 && !started; ++i)
	{
		stepFrames(1, "approaching", invis);
		started = flag->testModelCondition(startBit);
	}
	REQUIRE_MESSAGE(started, "horde did not start capturing CaptureFlag");
	stepFrames(3, "capturing", invis);


	// Interrupt capture by moving away.
	horde->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ unitSpot.x, unitSpot.y - 200.0f, unitSpot.z }, CMD_FROM_PLAYER);
	stepFrames(20, "cancelled", invis);
	CHECK_FALSE(flag->testModelCondition(startBit));

	// Capture to completion.
	issueCapture();
	stepFrames(2, "reordered", invis);

	for (int i = 0; i < 900 && flag->getControllingPlayer() != game.local; ++i)
	{
		stepFrames(1, "capturing_to_end", invis);
	}
	REQUIRE_MESSAGE(flag->getControllingPlayer() == game.local, "CaptureFlag was not captured");
	stepFrames(20, "captured", invis);
	const Drawable *fdAfter = game.game->drawables().findByObject(flag->getID());
	REQUIRE(fdAfter != nullptr);
	bool isengardFlagHidden = false;
	for (const DrawEntry &e : fdAfter->entries())
	{
		if (e.kind == W3D_DRAWKIND_MODEL && e.draw)
		{
			for (const std::string &h : e.draw->frame().hiddenSubObjects)
			{
				if (h.find("FLAG_ISENGARD") != std::string::npos)
				{
					isengardFlagHidden = true;
				}
			}
		}
	}
	CHECK_FALSE(isengardFlagHidden);

	CHECK(invis == 0);
}
