// OpenBFME unit tests (lane PORT-80, issue #212): the stealthy-area move voice redirect (RW 0x8DDBC4, donor BFME2 0x004D990E) inside the move voice
// selection (RW 0x8DE838). When the object ordered to move has an InvisibilityUpdate whose nugget answers "stealthy at the destination"
// (InvisibilityManager::stealthyAt, RW 0x81ACAA, the RW 0x81ABDF tree test) it says the UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea row of its
// UnitSpecificSounds (the EnterState twin for an enter-state voice) instead of the plain move voice. The module may live on the ordered object or, for a
// HORDE, on the horde object (the candidates the move picker sees are the horde's members; RW 0x8DDC53 / 0x8DDC66 pass the scanned object and its
// horde's position). The scenario mirrors the Ennorath mod, whose shared elven camouflage (data/ini/object/includes/defaultinvisibilityupdate.inc) sets
// UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea = VoiceMoveToTrees on the module and the VoiceMoveToTrees row in the unit's UnitSpecificSounds.

#include "doctest.h"

#include "LogicTestUtil.h"
#include "PathfindTestUtil.h"

#include "Common/Upgrade.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/System/InvisibilityManager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
struct RecordingSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds, eva;
	std::vector<ObjectID> owners;
	bool soundExists(const std::string &name) override { return known.count(name) != 0; }
	void playSound(const std::string &name, ObjectID object, int) override
	{
		sounds.push_back(name);
		owners.push_back(object);
	}
	void reportEva(const std::string &name, const Coord3D *) override { eva.push_back(name); }
	void clear()
	{
		sounds.clear();
		eva.clear();
		owners.clear();
	}
};

// locomotors: the retail horde one and a two-legs one (the values the synthetic maps use)
const char kVoiceLocomotors[] =
	"Locomotor WalkerLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 500\n"
	"  TurnTimeDamaged = 500\n"
	"  Acceleration = 510\n"
	"  Braking = 510\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"  CloseEnoughDist = 1\n"
	"End\n"
	"Locomotor HordeLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 2000\n"
	"  TurnTimeDamaged = 2000\n"
	"  Acceleration = 500\n"
	"  Braking = 500\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = HORDE\n"
	"  MaxTurnWithoutReform = 45\n"
	"  TurnWhileMoving = No\n"
	"  WaitForFormation = Yes\n"
	"  FormationPriority = MELEE1\n"
	"  CloseEnoughDist = 1\n"
	"End\n";

const char kVoiceObjects[] =
	// the elven camouflage case (the shared include defaultinvisibilityupdate.inc sets exactly these lines)
	"Object CamoRanger\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = RangerMove\n"
	"  VoiceEnterStateMove = RangerGo\n"
	"  UnitSpecificSounds\n"
	"    VoiceMoveToTrees = RangerToTrees\n"
	"    VoiceEnterStateMoveToTrees = RangerGoToTrees\n"
	"  End\n"
	"  Behavior = InvisibilityUpdate ModuleTag_ElvenCamouflage\n"
	"    InvisibilityNugget\n"
	"      InvisibilityType = CAMOUFLAGE\n"
	"      DetectionRange = 200\n"
	"      ForbiddenConditions = AWAY_FROM_TREES MOVING FIRING_ANY\n"
	"    End\n"
	"    UpdatePeriod = 2000\n"
	"    StartsActive = Yes\n"
	"    UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea = VoiceMoveToTrees\n"
	"    UnitSpecificSoundNameToUseAsVoiceEnterStateMoveToStealthyArea = VoiceEnterStateMoveToTrees\n"
	"  End\n"
	"End\n"
	// the improved camo of the ranger hordes (menhordes.ini): camouflage near trees regardless of the other conditions
	"Object NearTreesRanger\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = RangerMove\n"
	"  UnitSpecificSounds\n"
	"    VoiceMoveToTrees = RangerToTrees\n"
	"  End\n"
	"  Behavior = InvisibilityUpdate ModuleTag_ElvenCamouflage\n"
	"    InvisibilityNugget\n"
	"      InvisibilityType = CAMOUFLAGE\n"
	"      DetectionRange = 200\n"
	"      ForbiddenConditions = MOVING FIRING_ANY\n"
	"      Options = ALLOW_NEAR_TREES\n"
	"    End\n"
	"    UpdatePeriod = 2000\n"
	"    StartsActive = Yes\n"
	"    UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea = VoiceMoveToTrees\n"
	"  End\n"
	"End\n"
	// no module at all: the redirect name is present in UnitSpecificSounds and nothing uses it
	"Object PlainRanger\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = RangerMove\n"
	"  UnitSpecificSounds\n"
	"    VoiceMoveToTrees = RangerToTrees\n"
	"  End\n"
	"End\n"
	// a module, but a nugget without AWAY_FROM_TREES and without ALLOW_NEAR_TREES: stealthyAt answers false, always
	"Object StillRanger\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = RangerMove\n"
	"  UnitSpecificSounds\n"
	"    VoiceMoveToTrees = RangerToTrees\n"
	"  End\n"
	"  Behavior = InvisibilityUpdate ModuleTag_Camouflage\n"
	"    InvisibilityNugget\n"
	"      InvisibilityType = CAMOUFLAGE\n"
	"      DetectionRange = 200\n"
	"      ForbiddenConditions = MOVING FIRING_ANY\n"
	"    End\n"
	"    UpdatePeriod = 2000\n"
	"    StartsActive = Yes\n"
	"    UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea = VoiceMoveToTrees\n"
	"  End\n"
	"End\n"
	// the redirect names a row the template does not carry: nothing fills, the plain move voice answers
	"Object QuietRanger\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = RangerMove\n"
	"  Behavior = InvisibilityUpdate ModuleTag_ElvenCamouflage\n"
	"    InvisibilityNugget\n"
	"      InvisibilityType = CAMOUFLAGE\n"
	"      DetectionRange = 200\n"
	"      ForbiddenConditions = AWAY_FROM_TREES MOVING FIRING_ANY\n"
	"    End\n"
	"    UpdatePeriod = 2000\n"
	"    StartsActive = Yes\n"
	"    UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea = VoiceMoveToTrees\n"
	"  End\n"
	"End\n"
	// the cover object the camouflage hides in
	"Object TreeGuard\n"
	"  KindOf = SHRUBBERY TREE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 3\n"
	"  GeometryHeight = 10\n"
	"End\n"
	// the ranger horde member: its own rows, no module of its own
	"Object CamoMember\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = MemberMove\n"
	"  VoiceEnterStateMove = MemberGo\n"
	"  UnitSpecificSounds\n"
	"    VoiceMoveToTrees = MemberToTrees\n"
	"    VoiceEnterStateMoveToTrees = MemberGoToTrees\n"
	"  End\n"
	"End\n"
	// the ranger horde (menhordes.ini shape): the camouflage module and its redirect live on the HORDE object; the move voice
	// candidates the picker sees are the members
	"Object CamoHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  VoicePriority = 10\n"
	"  VoiceMove = HordeMove\n"
	"  Behavior = InvisibilityUpdate ModuleTag_ElvenCamouflage\n"
	"    InvisibilityNugget\n"
	"      InvisibilityType = CAMOUFLAGE\n"
	"      DetectionRange = 200\n"
	"      ForbiddenConditions = MOVING FIRING_ANY\n"
	"      Options = ALLOW_NEAR_TREES\n"
	"    End\n"
	"    UpdatePeriod = 2000\n"
	"    StartsActive = Yes\n"
	"    UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea = VoiceMoveToTrees\n"
	"    UnitSpecificSoundNameToUseAsVoiceEnterStateMoveToStealthyArea = VoiceEnterStateMoveToTrees\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:CamoMember Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    InitialPayload = CamoMember 3\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

struct StealthyVoiceWorld : logictest::LogicWorld
{
	LocomotorStore store;
	LocomotorStore *savedStore = nullptr;
	pathtest::SyntheticTerrain terrain;
	AIWorldConfig config;
	std::unique_ptr<AIWorld> ai;
	RecordingSink sink;
	std::unique_ptr<UnitVoiceResponse> voice;

	StealthyVoiceWorld()
		: logictest::LogicWorld()
		, terrain(100, 100)
	{
		REQUIRE_MESSAGE(loadError.empty(), loadError);
		savedStore = TheLocomotorStore;
		TheLocomotorStore = &store;
		w.fx.env.blocks.registerBlock("Locomotor", [](INI *ini) { LocomotorStore::parseLocomotorTemplateDefinitionGlobal(ini); });
		LogicModules::registerAll(w.modules);
		std::string err = w.load(kVoiceLocomotors, INI_LOAD_OVERWRITE, "locomotors.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.load(kVoiceObjects, INI_LOAD_OVERWRITE, "voices.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
		config.pathfind = pathtest::testConfig();
		config.movementPenaltyDamageState = 2;
		// a horde in the selection needs its AI world to tick (the order itself is not issued here)
		ai = std::make_unique<AIWorld>(*logic, config, w.fx.env.macros);
		ai->attach();
		ai->newMap(terrain);
		for (const char *s : { "RangerMove", "RangerGo", "RangerToTrees", "RangerGoToTrees", "MemberMove", "MemberGo", "MemberToTrees", "MemberGoToTrees",
				 "HordeMove" })
		{
			sink.known.insert(s);
		}
		voice = std::make_unique<UnitVoiceResponse>(*logic, sink);
		voice->setLocalPlayerIndex(players.findPlayerWithName("Alice")->getPlayerIndex());
	}
	~StealthyVoiceWorld()
	{
		logic->reset();
		ai.reset();
		TheLocomotorStore = savedStore;
	}

	Object *place(const char *name, float x, float y)
	{
		Object *o = make(name, teamOf("Alice"), INVALID_ID, false);
		const Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		completeHorde(o);
		return o;
	}
	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			logic->runLogicFrame();
		}
	}
	UnitVoiceResponse::Info moveInfo(float x, float y)
	{
		UnitVoiceResponse::Info info;
		info.hasPosition = true;
		info.position = Coord3D{ x, y, 0.0f };
		return info;
	}
};

} // namespace

TEST_CASE("PORT-80: ordered into a stealthy area, the redirect row plays (RW 0x8DDBC4 / 0x81ACAA)")
{
	StealthyVoiceWorld w;
	Object *ranger = w.place("CamoRanger", 100.0f, 100.0f);
	w.place("TreeGuard", 610.0f, 600.0f); // the cover at the destination
	w.frames(1);
	UnitVoiceResponse::Info info = w.moveInfo(600.0f, 600.0f);
	CHECK(w.voice->pickAndPlay({ ranger->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerToTrees" });
	CHECK(w.sink.owners == std::vector<ObjectID>{ ranger->getID() });
}

TEST_CASE("PORT-80: outside a stealthy area the plain move voice plays")
{
	StealthyVoiceWorld w;
	Object *ranger = w.place("CamoRanger", 100.0f, 100.0f);
	w.place("TreeGuard", 610.0f, 600.0f);
	w.frames(1);
	UnitVoiceResponse::Info info = w.moveInfo(900.0f, 900.0f); // far from every tree
	CHECK(w.voice->pickAndPlay({ ranger->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerMove" });
}

TEST_CASE("PORT-80: already stealthy at the start: no redirect, even with a tree at the destination (RW 0x8DDC53 before RW 0x8DDC66)")
{
	StealthyVoiceWorld w;
	Object *ranger = w.place("CamoRanger", 100.0f, 100.0f);
	w.place("TreeGuard", 105.0f, 100.0f); // stealthy at the start
	w.place("TreeGuard", 610.0f, 600.0f); // stealthy at the destination too
	w.frames(1);
	UnitVoiceResponse::Info info = w.moveInfo(600.0f, 600.0f);
	CHECK(w.voice->pickAndPlay({ ranger->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerMove" });
}

TEST_CASE("PORT-80: no module, a nugget without the gate bits, ALLOW_NEAR_TREES alone")
{
	StealthyVoiceWorld w;
	Object *plain = w.place("PlainRanger", 100.0f, 100.0f);
	Object *still = w.place("StillRanger", 120.0f, 100.0f);
	Object *nearTrees = w.place("NearTreesRanger", 140.0f, 100.0f);
	w.place("TreeGuard", 610.0f, 600.0f);
	w.frames(1);
	UnitVoiceResponse::Info info = w.moveInfo(600.0f, 600.0f);
	CHECK(w.voice->pickAndPlay({ plain->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerMove" }); // the module is missing: nothing redirects
	w.sink.clear();
	CHECK(w.voice->pickAndPlay({ still->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerMove" }); // no AWAY_FROM_TREES / ALLOW_NEAR_TREES: stealthyAt says no
	w.sink.clear();
	CHECK(w.voice->pickAndPlay({ nearTrees->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerToTrees" }); // ALLOW_NEAR_TREES alone is enough (RW 0x81ACAA)
}

TEST_CASE("PORT-80: the redirect names a row the template does not carry: nothing fills, the move voice falls through (RW 0x8DDB2A)")
{
	StealthyVoiceWorld w;
	Object *ranger = w.place("QuietRanger", 100.0f, 100.0f);
	w.place("TreeGuard", 610.0f, 600.0f);
	w.frames(1);
	UnitVoiceResponse::Info info = w.moveInfo(600.0f, 600.0f);
	CHECK(w.voice->pickAndPlay({ ranger->getID() }, MSG_DO_MOVETO, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerMove" });
}

TEST_CASE("PORT-80: the enter-state move voice uses the EnterState redirect slot (RW 0x8DDC66 with the 0x208 name)")
{
	StealthyVoiceWorld w;
	Object *ranger = w.place("CamoRanger", 100.0f, 100.0f);
	w.place("TreeGuard", 610.0f, 600.0f);
	w.frames(1);
	UnitVoiceResponse::Info info = w.moveInfo(600.0f, 600.0f);
	CHECK(w.voice->pickAndPlay({ ranger->getID() }, UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, &info));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerGoToTrees" });
	w.sink.clear();
	UnitVoiceResponse::Info far = w.moveInfo(900.0f, 900.0f);
	CHECK(w.voice->pickAndPlay({ ranger->getID() }, UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, &far));
	CHECK(w.sink.sounds == std::vector<std::string>{ "RangerGo" });
}

TEST_CASE("PORT-80: the camouflage lives on the HORDE; the ordered candidates are the members and speak the member's row (RW 0x8DEA88)")
{
	StealthyVoiceWorld w;
	Object *horde = w.place("CamoHorde", 100.0f, 100.0f);
	w.place("TreeGuard", 610.0f, 600.0f);
	w.frames(1);
	const ContainModuleInterface::ContainedItemsList *members = horde->getContain()->getContainedItemsList();
	REQUIRE(members != nullptr);
	REQUIRE(members->size() == 3);
	std::vector<ObjectID> memberIds;
	for (const Object *member : *members)
	{
		memberIds.push_back(member->getID());
	}
	// the redirect: the candidates are the members, the module and its nugget live on the horde; the voice attaches to the speaking member
	UnitVoiceResponse::Info info = w.moveInfo(600.0f, 600.0f);
	CHECK(w.voice->pickAndPlay({ horde->getID() }, MSG_DO_MOVETO, &info));
	REQUIRE(w.sink.sounds.size() == 1);
	CHECK(w.sink.sounds[0] == "MemberToTrees");
	REQUIRE(w.sink.owners.size() == 1);
	CHECK(w.sink.owners[0] != horde->getID()); // the record's owner is the candidate, not the container
	CHECK(std::find(memberIds.begin(), memberIds.end(), w.sink.owners[0]) != memberIds.end());
	// outside a stealthy area the member's plain move voice plays instead
	w.sink.clear();
	UnitVoiceResponse::Info far = w.moveInfo(900.0f, 900.0f);
	CHECK(w.voice->pickAndPlay({ horde->getID() }, MSG_DO_MOVETO, &far));
	CHECK(w.sink.sounds == std::vector<std::string>{ "MemberMove" });
}

TEST_CASE("PORT-80: stealthyAt answers only near trees (RW 0x81ACAA: the nugget gate, the upgrade bit, the tree test)")
{
	StealthyVoiceWorld w;
	Object *ranger = w.place("CamoRanger", 100.0f, 100.0f);
	Object *cover = w.place("TreeGuard", 610.0f, 600.0f);
	w.frames(1);
	InvisibilityManager &invis = w.logic->invisibility();
	const Coord3D near{ 600.0f, 600.0f, 0.0f }, farAway{ 900.0f, 900.0f, 0.0f };
	InvisibilityNugget n;
	n.invisibilityType = InvisibilityNugget::CAMOUFLAGE;
	// a nugget without AWAY_FROM_TREES and without ALLOW_NEAR_TREES never answers stealthy near a tree
	n.forbiddenConditions = InvisibilityNugget::MOVING;
	CHECK_FALSE(invis.stealthyAt(*ranger, near, n));
	// ALLOW_NEAR_TREES alone opens the tree test
	n.options = InvisibilityNugget::ALLOW_NEAR_TREES;
	CHECK(invis.stealthyAt(*ranger, near, n));
	CHECK_FALSE(invis.stealthyAt(*ranger, farAway, n));
	// AWAY_FROM_TREES alone opens it, too
	n.options = 0;
	n.forbiddenConditions = InvisibilityNugget::AWAY_FROM_TREES;
	CHECK(invis.stealthyAt(*ranger, near, n));
	CHECK_FALSE(invis.stealthyAt(*ranger, farAway, n));
	// an IgnoreTreeCheckUpgrades bit on the object answers stealthy without any tree (RW 0x8097D6, before the tree test)
	n.forbiddenConditions = InvisibilityNugget::AWAY_FROM_TREES;
	n.ignoreTreeCheckUpgrades = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2")->grantMask();
	ranger->friend_orUpgradeMask(n.ignoreTreeCheckUpgrades);
	CHECK(invis.stealthyAt(*ranger, farAway, n));
	CHECK(cover != nullptr);
}
