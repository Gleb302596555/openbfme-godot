// PORT-233 (issue #556): the tracker's viewer-relative visibility, the respawn health above 100 %, the ground cell's wall layer, the
// PassiveAreaEffect's NonStackable gate, the score keeper's level-up counts and the skill point scalars.
// RotWK RW 0x8B5186..0x8B5195 passes the tracker owner's player to Object::isVisibleTo at RW 0x694CCC (mode 0).
// The NonStackable gate of PassiveAreaEffectBehavior (RW 0x887C1A..0x887C34) reads the body's last heal frame (PORT-293's stamp: written by
// the healing at RW 0x8C30BA, read through body vslot 0x48, RW 0x5DC48C); AutoHealBehavior's own gate (RW 0x855780..0x8557A2) is PORT-293's.

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/ScoreKeeper.h"
#include "Common/StateHash.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/Module/AreaScanModules.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SpellStores.h"

#include <vector>

namespace
{
const char kPort233Objects[] =
	"Object PORT233Disguiser\n"
	"  KindOf = INFANTRY SELECTABLE DISGUISER\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = StealthUpdate ModuleTag_Stealth\n"
	"  End\n"
	"End\n"
	"Object PORT233RespawnHero\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = RespawnBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = RespawnUpdate ModuleTag_Respawn\n"
	"    RespawnRules = AutoSpawn:No Cost:1000 Time:60000 Health:150%\n"
	"  End\n"
	"End\n"
	// a self-healing hero: Radius 0 (RW 0x8558C0's last branch heals the object itself), NonStackable, HealingDelay 1000 ms (5 frames),
	// StartHealingDelay 200 ms (1 frame): a hit wakes the heal one frame later, so the heal that follows a hit inside the HealingDelay window is the
	// one NonStackable refuses (RW 0x85579A: the body's last heal frame + HealingDelay > now)
	"Object PORT233Healer\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AutoHealBehavior ModuleTag_Heal\n"
	"    StartsActive = Yes\n"
	"    HealingAmount = 10\n"
	"    HealingDelay = 1000\n"
	"    StartHealingDelay = 200\n"
	"    NonStackable = Yes\n"
	"  End\n"
	"End\n"
	// the well: PassiveAreaEffectBehavior with NonStackable, PingDelay 3000 ms (15 frames), 2 % of the max health per second
	"Object PORT233Well\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = PassiveAreaEffectBehavior ModuleTag_Area\n"
	"    EffectRadius = 200\n"
	"    HealPercentPerSecond = 2%\n"
	"    PingDelay = 3000\n"
	"    NonStackable = Yes\n"
	"  End\n"
	"End\n"
	"Object PORT233Patient\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	// the hero and the trooper of the score keeper test: a level with a rank above 1 (RW 0x79DAEC: `cmp dword ptr [edi], 1; jle`)
	"Object PORT233Hero\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object PORT233Trooper\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	// the structure of the score keeper test: RW 0x79DAFD reads the template's STRUCTURE kind bit (the word at + 0x108, bit 7) and keeps the
	// level-up count out of the keeper
	"Object PORT233Structure\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n";

// the level chains of the score keeper test (the ExperienceLevel block, RW 0x689920). The first level has rank 1: the object's initObject applies it
// (RW 0x693F27 -> RW 0x79D7AD gainLevel -> RW 0x79D76D applyLevel -> onLevelReached), and a rank above 1 is the counting condition (RW 0x79DAEC)
const char kPort233Levels[] =
	"ExperienceLevel PORT233HeroLevel\n"
	"  TargetNames = PORT233Hero\n"
	"  RequiredExperience = 1\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel PORT233HeroLevel2\n"
	"  TargetNames = PORT233Hero\n"
	"  RequiredExperience = 50\n"
	"  Rank = 2\n"
	"End\n"
	"ExperienceLevel PORT233HeroLevel3\n"
	"  TargetNames = PORT233Hero\n"
	"  RequiredExperience = 500\n"
	"  Rank = 3\n"
	"End\n"
	"ExperienceLevel PORT233HeroLevel4\n"
	"  TargetNames = PORT233Hero\n"
	"  RequiredExperience = 5000\n"
	"  Rank = 4\n"
	"End\n"
	"ExperienceLevel PORT233TrooperLevel\n"
	"  TargetNames = PORT233Trooper\n"
	"  RequiredExperience = 1\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel PORT233TrooperLevel2\n"
	"  TargetNames = PORT233Trooper\n"
	"  RequiredExperience = 50\n"
	"  Rank = 2\n"
	"End\n"
	"ExperienceLevel PORT233StructureLevel\n"
	"  TargetNames = PORT233Structure\n"
	"  RequiredExperience = 1\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel PORT233StructureLevel2\n"
	"  TargetNames = PORT233Structure\n"
	"  RequiredExperience = 50\n"
	"  Rank = 2\n"
	"End\n"
	// the skill point scalar table (S-524): the PlayerScience looks it up by name (RW 0x782A0C)
	"ExperienceScalarTable PlayerSkillPointsScalarTable\n"
	"  Scalars = 1.0 0.5 0.25\n"
	"End\n";

struct Port233World : movetest::MoveWorld
{
	// the level chains (the ExperienceLevel block needs its own store: TheExperienceLevelSystem)
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	Port233World()
		: movetest::MoveWorld(200, 200, kPort233Objects)
	{
		TheExperienceLevelSystem = &levels;
		levels.registerBlocks(w.fx.env.blocks);
		const std::string err = w.load(kPort233Levels, INI_LOAD_OVERWRITE, "levels.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port233World() { TheExperienceLevelSystem = savedLevels; }
};

// the skill point scalar table's test needs the spell stores (the ranks are the skill points' cap, RW 0x782B47)
struct Port233SpellWorld : Port233World
{
	SpellStores stores;
	Port233SpellWorld()
		: Port233World()
		, stores(w.keys)
	{
		stores.install();
		SpellStores::registerBlocks(w.fx.env.blocks);
		const std::string err = w.load("Rank 1\n  SkillPointsNeededDefault = 0\nEnd\n"
									   "Rank 2\n  SkillPointsNeededDefault = 0\nEnd\n"
									   "Rank 3\n  SkillPointsNeededDefault = 1000\nEnd\n",
									   INI_LOAD_OVERWRITE, "ranks.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port233SpellWorld() { stores.uninstall(); }
};

struct Port233Wall : pathtest::TestObject
{
	PathfindWallGeometry wallGeometry;
	const PathfindWallGeometry *getWallGeometry() const override { return &wallGeometry; }
};
PathfindWallSurface port233WallTop()
{
	PathfindWallSurface surface;
	PathfindWallTriangle a, b;
	a.v[0] = { 150.0f, 90.0f, 60.0f };
	a.v[1] = { 250.0f, 90.0f, 60.0f };
	a.v[2] = { 250.0f, 110.0f, 60.0f };
	b.v[0] = { 150.0f, 90.0f, 60.0f };
	b.v[1] = { 250.0f, 110.0f, 60.0f };
	b.v[2] = { 150.0f, 110.0f, 60.0f };
	surface.triangles = { a, b };
	surface.loX = 150.0f;
	surface.loY = 90.0f;
	surface.hiX = 250.0f;
	surface.hiY = 110.0f;
	surface.height = 60.0f;
	return surface;
}
} // namespace

TEST_CASE("PORT-233 tracker visibility uses the viewer's relationship to a disguised object's apparent owner (RW 0x694CCC, 0x8B5186)")
{
	Port233World w;
	Object *viewer = w.make("Walker", w.teamOf("Alice"));
	Object *disguiser = w.make("PORT233Disguiser", w.teamOf("Bob"));
	Object *asAlly = w.make("Walker", w.teamOf("Alice"));
	Object *asEnemy = w.make("Walker", w.teamOf("Bob"));
	REQUIRE(viewer != nullptr);
	REQUIRE(disguiser != nullptr);
	REQUIRE(asAlly != nullptr);
	REQUIRE(asEnemy != nullptr);
	StealthUpdate *stealth = StealthUpdate::of(*disguiser);
	REQUIRE(stealth != nullptr);

	// A disguise as an ally is hidden from that viewer by RW 0x694E87..0x694ED1; the old viewer-less
	// DISGUISER shortcut returned true regardless of the apparent owner's relationship.
	stealth->disguiseAsObject(asAlly);
	REQUIRE(stealth->disguisePlayerIndex() == asAlly->getControllingPlayer()->getPlayerIndex());
	disguiser->setStatus((unsigned)CombatNames::status("STEALTHED"), true);
	disguiser->setStatus((unsigned)CombatNames::status("DETECTED"), false);
	CHECK_FALSE(EmotionModules::visibleToScans(*disguiser, viewer->getControllingPlayer()));

	// Neighbor: a disguise as an enemy remains visible to this viewer (same RW branch).
	stealth->disguiseAsObject(asEnemy);
	CHECK(EmotionModules::visibleToScans(*disguiser, viewer->getControllingPlayer()));

	// Existing callers without a viewer retain their legacy query contract.
	CHECK(EmotionModules::visibleToScans(*disguiser));
}

TEST_CASE("PORT-233 RespawnUpdate above 100 percent raises max health without changing revive reference (RW 0x8C47DE / 0x8C47F6)")
{
	Port233World w;
	Object *hero = w.make("PORT233RespawnHero", w.teamOf("Alice"));
	REQUIRE(hero != nullptr);
	ActiveBody *body = dynamic_cast<ActiveBody *>(hero->getBodyModule());
	RespawnUpdate *respawn = dynamic_cast<RespawnUpdate *>(hero->findModule("RespawnUpdate"));
	REQUIRE(body != nullptr);
	REQUIRE(respawn != nullptr);
	CHECK(body->getMaxHealth() == 500.0f);
	respawn->onRevived(nullptr);
	CHECK(body->getMaxHealth() == 750.0f);
	CHECK(body->getReviveReference() == 500.0f); // RW 0x8C47F6 changes max health (+0x10), not the revive reference (+0x2C)
	CHECK(body->getHealth() == 750.0f);
}

TEST_CASE("PORT-233 taunt scan wall exclusion reads the ground cell layer, not its connect layer (RW 0x6EA857)")
{
	pathtest::SyntheticTerrain terrain{ 40, 20 };
	pathtest::TestWorld world;
	Port233Wall wall;
	wall.id = 50;
	wall.pos = { 200.0f, 100.0f, 0.0f };
	wall.structure = true;
	wall.kinds.insert(PK_WALK_ON_TOP_OF_WALL);
	wall.geometry.type = PATHFIND_GEOMETRY_BOX;
	wall.geometry.majorRadius = 50.0f;
	wall.geometry.minorRadius = 30.0f;
	wall.wallGeometry.walls.push_back(port233WallTop());
	world.objects[wall.id] = &wall;

	Pathfinder pathfinder(pathtest::testConfig(), &world);
	pathfinder.newMap(terrain);
	pathfinder.addObjectToPathfindMap(wall);
	const Coord3D onWall{ 200.0f, 100.0f, 60.0f };
	const PathfindCell *wallCell = pathfinder.getCell(LAYER_GROUND, &onWall);
	REQUIRE(wallCell != nullptr);
	CHECK(isWallLayer(wallCell->getLayer()));
	CHECK(wallCell->getConnectLayer() == LAYER_INVALID);
	CHECK(EmotionModules::isOnWallLayer(pathfinder, onWall));
	CHECK_FALSE(EmotionModules::isOnWallLayer(pathfinder, Coord3D{ 300.0f, 100.0f, 0.0f }));
}

// PORT-233: the PassiveAreaEffect NonStackable gate. The well heals the patient every PingDelay frames; a heal from another path inside the
// PingDelay window (a direct heal, RW 0x690532, which the sole-benefactor gate of RW 0x690584 does not see) makes the well's next pulse refuse the
// patient: RW 0x887C2C reads the body's last heal frame, RW 0x887C32 compares it with the current frame, RW 0x887C34 jumps past the heal and its FX.
TEST_CASE("PORT-233 the PassiveAreaEffect NonStackable gate refuses the heal of a body healed inside the window (RW 0x887C1A .. 0x887C34)")
{
	Port233World w;
	Object *well = w.make("PORT233Well", w.teamOf("Alice"));
	Object *patient = w.make("PORT233Patient", w.teamOf("Alice"));
	REQUIRE(well != nullptr);
	REQUIRE(patient != nullptr);
	PassiveAreaEffectBehavior *area = dynamic_cast<PassiveAreaEffectBehavior *>(well->findModule("PassiveAreaEffectBehavior"));
	ActiveBody *body = dynamic_cast<ActiveBody *>(patient->getBodyModule());
	REQUIRE(area != nullptr);
	REQUIRE(body != nullptr);
	const float maxHealth = body->getMaxHealth();
	body->friend_setHealthRaw(maxHealth * 0.5f); // wounded without a hit: the 4 second damage gate (RW 0x68C933) stays open
	const float onePulse = maxHealth * 0.02f / 5.0f * 15.0f; // RW 0x887C39: MaxHealth * (percent / 5) * PingDelay

	UnsignedInt pulse = 0;
	bool pulsed = false;
	for (int i = 0; i < 40 && !pulsed; ++i)
	{
		w.frames(1);
		if (area->heals() == 1)
		{
			pulse = w.logic->getFrame();
			pulsed = true;
		}
	}
	REQUIRE(pulsed);
	// tolerance: the heal is a float sum of the max health, the percent and the PingDelay (RW 0x887C39)
	CHECK(body->getHealth() == doctest::Approx(maxHealth * 0.5f + onePulse));
	CHECK(body->getLastHealingFrame() == pulse);

	// a direct heal five frames after the pulse (RW 0x690532: no sole benefactor, so the well's own gate is the only one)
	w.frames(5);
	patient->attemptHealing(5.0f, patient);
	const float afterDirect = body->getHealth();
	// tolerance: the heal is a float sum of the max health, the percent, the PingDelay and the direct heal (RW 0x887C39)
	CHECK(afterDirect == doctest::Approx(maxHealth * 0.5f + onePulse + 5.0f));
	CHECK(body->getLastHealingFrame() == w.logic->getFrame());

	// the next pulse (pulse + PingDelay) is inside the window: the heal is refused (RW 0x887C34)
	w.frames(10);
	REQUIRE(w.logic->getFrame() == pulse + 15);
	CHECK(area->heals() == 1);
	// tolerance: the refused pulse leaves the float health of the body as it was
	CHECK(body->getHealth() == doctest::Approx(afterDirect));

	// the pulse after that is outside the window: the heal runs again
	w.frames(15);
	REQUIRE(w.logic->getFrame() == pulse + 30);
	CHECK(area->heals() == 2);
	// tolerance: the heal is a float sum of the max health, the percent and the PingDelay (RW 0x887C39)
	CHECK(body->getHealth() == doctest::Approx(afterDirect + onePulse));
}

// PORT-233: the level-up counts of the owner's score keeper (S-630 / S-631).
// RW 0x79DAEC .. RW 0x79DB46: a rank above 1 on an object that is not a STRUCTURE and never leveled before counts in the owner's keeper: a HERO goes to
// + 0xD0 (RW 0x79DB50) and an object that is not a horde member to + 0xD4 (RW 0x79DB6B), both behind the keep-score switch (GameLogic + 0x98).
TEST_CASE("PORT-233 a level-up counts in the owner's ScoreKeeper (RW 0x79DB50 / RW 0x79DB6B)")
{
	Port233World w;
	Object *hero = w.make("PORT233Hero", w.teamOf("Alice"));
	Object *trooper = w.make("PORT233Trooper", w.teamOf("Alice"));
	REQUIRE(hero != nullptr);
	REQUIRE(trooper != nullptr);
	ExperienceTracker *heroTracker = hero->getExperienceTracker();
	ExperienceTracker *trooperTracker = trooper->getExperienceTracker();
	REQUIRE(heroTracker != nullptr);
	REQUIRE(trooperTracker != nullptr);
	ScoreKeeper &keeper = w.players.findPlayerWithName("Alice")->getScoreKeeper();
	// the first level (rank 1) is applied at the object's creation (RW 0x693F27): its rank is not above 1, so nothing counts yet
	CHECK(heroTracker->getRank() == 1);
	CHECK(keeper.heroesVetted() == 0);
	CHECK(keeper.unitsVetted() == 0);

	// the hero's second level has rank 2: RW 0x79DAEC (`cmp dword ptr [edi], 1; jle`) takes the counting branch
	heroTracker->addExperiencePoints(60.0f, false, false, false);
	CHECK(heroTracker->getRank() == 2);
	CHECK(keeper.heroesVetted() == 1); // RW 0x79DB1D .. RW 0x79DB25: + 0xD0 += 1
	CHECK(keeper.unitsVetted() == 0);

	// the trooper's second level has rank 2 and it is no horde member: RW 0x79DB3E .. RW 0x79DB46: + 0xD4 += 1
	trooperTracker->addExperiencePoints(60.0f, false, false, false);
	CHECK(trooperTracker->getRank() == 2);
	CHECK(keeper.heroesVetted() == 1);
	CHECK(keeper.unitsVetted() == 1);

	// the switch (GameLogic + 0x98, the keeper's own `enabled`) stops the count: RW 0x79DB55 .. RW 0x79DB5C
	keeper.setEnabled(false);
	heroTracker->clearGainedLevel(); // the level-up flag (RW + 0x20) so the next level counts again
	heroTracker->addExperiencePoints(1000.0f, false, false, false);
	CHECK(heroTracker->getRank() == 3);
	CHECK(keeper.heroesVetted() == 1);
	CHECK(keeper.unitsVetted() == 1);

	// with the switch back on the next level counts again
	keeper.setEnabled(true);
	heroTracker->clearGainedLevel();
	heroTracker->addExperiencePoints(10000.0f, false, false, false);
	CHECK(heroTracker->getRank() == 4);
	CHECK(keeper.heroesVetted() == 2);
	CHECK(keeper.unitsVetted() == 1);
}

// PORT-233: the exclusions of the level-up count and the keeper's own state of the two counts.
// RW 0x79DAFD reads the template's STRUCTURE kind bit (the word at + 0x108, bit 7) and jumps past the count; RW 0x79DB2C pushes the status
// HORDE_MEMBER (0x26 = 38) and tests it (RW 0x44DDEC is the status bit test) and jumps past the unit count. The two counts are hashed with the
// keeper (PROCESS 15.5) and transferred with the keeper's xfer version 1.2 (the retail window is (1, 12): the counters are among its ints).
TEST_CASE("PORT-233 a horde member and a structure do not count in the ScoreKeeper, and the two counts are hashed and xferred")
{
	Port233World w;
	Object *trooper = w.make("PORT233Trooper", w.teamOf("Alice"));
	Object *structure = w.make("PORT233Structure", w.teamOf("Alice"));
	REQUIRE(trooper != nullptr);
	REQUIRE(structure != nullptr);
	ScoreKeeper &keeper = w.players.findPlayerWithName("Alice")->getScoreKeeper();
	ExperienceTracker *trooperTracker = trooper->getExperienceTracker();
	ExperienceTracker *structureTracker = structure->getExperienceTracker();
	REQUIRE(trooperTracker != nullptr);
	REQUIRE(structureTracker != nullptr);

	// RW 0x79DB2C .. RW 0x79DB35: the status HORDE_MEMBER keeps the level-up out of + 0xD4
	trooper->setStatus((unsigned)CombatNames::status("HORDE_MEMBER"), true);
	trooperTracker->addExperiencePoints(60.0f, false, false, false);
	CHECK(trooperTracker->getRank() == 2);
	CHECK(keeper.unitsVetted() == 0);
	CHECK(keeper.heroesVetted() == 0);

	// RW 0x79DAF7 .. RW 0x79DB04: the template's STRUCTURE bit keeps the level-up out of both counts
	structureTracker->addExperiencePoints(60.0f, false, false, false);
	CHECK(structureTracker->getRank() == 2);
	CHECK(keeper.unitsVetted() == 0);
	CHECK(keeper.heroesVetted() == 0);

	// the counts are hashed: a keeper with a count and one without hash differently
	StateHasher before, after;
	keeper.crc(before);
	keeper.addUnitVetted(1);
	keeper.crc(after);
	CHECK(before.value() != after.value());

	// and transferred with the keeper's xfer version 1.2
	XferSave save;
	keeper.xfer(save);
	REQUIRE(save.data()[0] == 2); // the keeper's xfer version byte
	XferLoad load(save.data());
	ScoreKeeper loaded;
	loaded.xfer(load);
	CHECK(loaded.unitsVetted() == 1);
	CHECK(loaded.heroesVetted() == 0);

	// a stream of the previous version (1.1, before PORT-233) has no such field: the keeper loads both counts as zero. The two counts sit in the
	// middle of the block (the skill points and the maps follow them), so the old stream is built by finding the counters' offset: two keepers that
	// differ only in the heroes count differ only there
	ScoreKeeper plain;
	ScoreKeeper counted;
	counted.addHeroVetted(0x0BADF00D);
	XferSave plainSave, countedSave;
	plain.xfer(plainSave);
	counted.xfer(countedSave);
	REQUIRE(plainSave.data().size() == countedSave.data().size());
	std::size_t offset = 0;
	while (offset < plainSave.data().size() && plainSave.data()[offset] == countedSave.data()[offset])
	{
		++offset;
	}
	// tolerance: `offset` locates `m_heroesVetted` in ScoreKeeper::xfer without pinning the byte count of preceding ScoreKeeper fields
	REQUIRE(offset < plainSave.data().size());
	std::vector<std::uint8_t> oldStream = countedSave.data();
	oldStream[0] = 1;                                              // the keeper's xfer version byte
	oldStream.erase(oldStream.begin() + offset, oldStream.begin() + offset + 8); // the two counts are gone
	XferLoad oldLoad(oldStream);
	ScoreKeeper oldLoaded;
	oldLoaded.xfer(oldLoad);
	CHECK(oldLoaded.heroesVetted() == 0);
	CHECK(oldLoaded.unitsVetted() == 0);
}

// PORT-233: the skill point scalar table (S-524).
// RW 0x782AA4 .. RW 0x782B0F: the points are scaled by the PlayerSkillPointsScalarTable's scalar of the rank above the table's base rank (the index is
// clamped to the last scalar; a negative index scales nothing) when the caller asks for the scalar, the game's kind is not 3 (a skirmish, RW 0x782AD0)
// and the table exists (the constructor RW 0x782A0C looks it up by name in the store RW 0xDE4704).
TEST_CASE("PORT-233 the PlayerSkillPointsScalarTable scales the skill points outside a skirmish (RW 0x782AA4 .. RW 0x782B0F)")
{
	Port233SpellWorld w;
	PlayerScience science;
	science.bind(nullptr, SpellGameMode{ false, false, 0 }, nullptr); // a campaign: the game's kind is not 3
	science.reset();
	REQUIRE(science.setRankLevel(2));
	CHECK(science.getSkillPoints() == 0.0f);
	science.addSkillPoints(100.0f, true);
	CHECK(science.getSkillPoints() == 50.0f); // the rank 2's scalar (the index is the rank above the base rank 1): 0.5

	// without the scalar asked for, the points are not scaled (RW 0x782AC5: `cmp byte ptr [ebp + 0xc], 0; je`)
	PlayerScience plain;
	plain.bind(nullptr, SpellGameMode{ false, false, 0 }, nullptr);
	plain.reset();
	REQUIRE(plain.setRankLevel(2));
	plain.addSkillPoints(100.0f, false);
	CHECK(plain.getSkillPoints() == 100.0f);

	// a skirmish never scales (RW 0x782AD0: `cmp dword ptr [eax + 0x114], 3; je`)
	PlayerScience skirmish;
	skirmish.bind(nullptr, SpellGameMode{ true, false, 3 }, nullptr);
	skirmish.reset();
	REQUIRE(skirmish.setRankLevel(2));
	skirmish.addSkillPoints(100.0f, true);
	CHECK(skirmish.getSkillPoints() == 100.0f);
}
