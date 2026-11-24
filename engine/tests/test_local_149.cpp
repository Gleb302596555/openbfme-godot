// OpenBFME unit tests: LOCAL-149 (fork issue 882) -- HordeContain second pass.
// Every expectation is derived from RotWK 2.01 game.dat instructions (RW 0x... cited per test) and Ennorath INI data.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/Player.h"
#include "Common/Team.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Contain/OpenContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{

const char kLocal149Ini[] =
	"Object Local149Soldier\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = PlainLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local149OtherUnit\n"
	"  KindOf = VEHICLE SELECTABLE\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local149HordeMain\n"
	"  KindOf = INFANTRY HORDE SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    ObjectStatusOfContained =\n"
	"    InitialPayload = Local149Soldier 3\n"
	"    Slots = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowOwnPlayerInsideOverride = Yes\n"
	"    ThisFormationIsTheMainFormation = Yes\n"
	"    AlternateFormation = Local149HordeAlt\n"
	"    MinimumHordeSize = 1\n"
	"    RankInfo = RankNumber:1 UnitType:Local149Soldier Position:X:0 Y:0 Position:X:10 Y:0 Position:X:20 Y:0 Position:X:30 Y:0\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local149HordeAlt\n"
	"  KindOf = INFANTRY HORDE SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    ObjectStatusOfContained =\n"
	"    InitialPayload = Local149Soldier 3\n"
	"    Slots = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowOwnPlayerInsideOverride = Yes\n"
	"    ThisFormationIsTheMainFormation = No\n"
	"    AlternateFormation = Local149HordeMain\n"
	"    MinimumHordeSize = 3\n"
	"    RankInfo = RankNumber:1 UnitType:Local149Soldier Position:X:0 Y:0 Position:X:10 Y:0 Position:X:20 Y:0 Position:X:30 Y:0\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local149PlainGarrison\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowOwnPlayerInsideOverride = Yes\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local149HordeGarrison\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = HordeGarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowOwnPlayerInsideOverride = Yes\n"
	"  End\n"
	"End\n";

struct Local149World : CombatWorld
{
	Local149World()
		: CombatWorld(kLocal149Ini)
	{
	}
};

} // namespace

TEST_CASE("LOCAL-149 HordeContain::onContainedAboutToTakeDamage forwards to outer container (RW 0x0086C8B9)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - RW 0x0086C8B9 (contain slot +0x68, DONOR BFME2 rva004697A8): if owner->getContainedBy() has a contain module,
	//   forwards onContainedAboutToTakeDamage(rider, damageSource, info) to the outer container's slot +0x68; otherwise no-op.
	Local149World w;
	Object *horde = w.unit("Local149HordeMain", 'A', 100.0f, 100.0f);
	horde->friend_onBuildComplete();
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc != nullptr);
	REQUIRE(hc->getContainCount() == 3u);
	Object *member = hc->getContainedItemsList()->front();
	REQUIRE(member != nullptr);

	// 1. Uncontained horde: RW 0x0086C8C6 returns early without modifying damage.
	DamageInfo openSlash;
	openSlash.m_input.m_damageType = DAMAGE_SLASH;
	openSlash.m_input.m_amount = 50.0f;
	openSlash.m_input.m_kill = true;
	hc->onContainedAboutToTakeDamage(member, member->getBodyModule(), &openSlash);
	CHECK(openSlash.m_input.m_amount == 50.0f);
	CHECK(openSlash.m_input.m_kill);

	// 2. Horde inside plain GarrisonContain (slot +0x68 = OpenContain RW 0x00865631):
	//    forwarding zeroes ordinary damage and clears m_kill.
	Object *plainGar = w.unit("Local149PlainGarrison", 'A', 120.0f, 100.0f);
	ContainModuleInterface *pg = plainGar->getContain();
	REQUIRE(pg != nullptr);
	REQUIRE(pg->addToContain(horde));
	CHECK(horde->getContainedBy() == plainGar);

	DamageInfo garSlash;
	garSlash.m_input.m_damageType = DAMAGE_SLASH;
	garSlash.m_input.m_amount = 40.0f;
	garSlash.m_input.m_kill = true;
	hc->onContainedAboutToTakeDamage(member, member->getBodyModule(), &garSlash);
	CHECK(garSlash.m_input.m_amount == 0.0f);
	CHECK_FALSE(garSlash.m_input.m_kill);

	// 3. Horde inside HordeGarrisonContain (slot +0x68 = RW 0x0087A3A4):
	//    a member has getContain() == nullptr, so HordeGarrisonContain delegates to OpenContain RW 0x00865631.
	pg->removeFromContain(horde);
	Object *hordeGar = w.unit("Local149HordeGarrison", 'A', 160.0f, 100.0f);
	ContainModuleInterface *hg = hordeGar->getContain();
	REQUIRE(hg != nullptr);
	REQUIRE(hg->addToContain(horde));
	CHECK(horde->getContainedBy() == hordeGar);

	DamageInfo hgPierce;
	hgPierce.m_input.m_damageType = DAMAGE_PIERCE;
	hgPierce.m_input.m_amount = 35.0f;
	hgPierce.m_input.m_kill = true;
	hc->onContainedAboutToTakeDamage(member, member->getBodyModule(), &hgPierce);
	CHECK(hgPierce.m_input.m_amount == 0.0f);
	CHECK_FALSE(hgPierce.m_input.m_kill);
}

TEST_CASE("LOCAL-149 OpenContain and HordeContain iterateContained flags (RW 0x008666A1 / RW 0x0087104A)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - OpenContain::iterateContained (RW 0x008666A1): visits m_contained when (flags & 1) != 0, reverse when (flags & 8) != 0.
	// - HordeContain::iterateContained (RW 0x0087104A): when (flags & 0x10) != 0, resolves and visits m_garrisonEntering IDs
	//   via findObjectByID (RW 0x449681); otherwise delegates to OpenContain::iterateContained (RW 0x008666A1) on m_members.
	Local149World w;

	// 1. OpenContain (GarrisonContain inherits OpenContain::iterateContained):
	Object *gar = w.unit("Local149PlainGarrison", 'A', 100.0f, 100.0f);
	Object *s1 = w.unit("Local149Soldier", 'A', 100.0f, 100.0f);
	Object *s2 = w.unit("Local149Soldier", 'A', 100.0f, 100.0f);
	Object *s3 = w.unit("Local149Soldier", 'A', 100.0f, 100.0f);
	ContainModuleInterface *gc = gar->getContain();
	REQUIRE(gc != nullptr);
	REQUIRE(gc->addToContain(s1));
	REQUIRE(gc->addToContain(s2));
	REQUIRE(gc->addToContain(s3));

	std::vector<Object *> visited;
	gc->iterateContained([&](Object *o) { visited.push_back(o); }, 0u);
	CHECK(visited.empty());

	gc->iterateContained([&](Object *o) { visited.push_back(o); }, 1u);
	REQUIRE(visited.size() == 3u);
	CHECK(visited[0] == s1);
	CHECK(visited[1] == s2);
	CHECK(visited[2] == s3);

	visited.clear();
	gc->iterateContained([&](Object *o) { visited.push_back(o); }, 1u | 8u);
	REQUIRE(visited.size() == 3u);
	CHECK(visited[0] == s3);
	CHECK(visited[1] == s2);
	CHECK(visited[2] == s1);

	// Safe removal during forward iteration.
	gc->iterateContained([&](Object *o) { gc->removeFromContain(o); }, 1u);
	CHECK(gc->getContainCount() == 0u);

	// 2. HordeContain::iterateContained (RW 0x0087104A):
	Object *horde = w.unit("Local149HordeMain", 'A', 200.0f, 100.0f);
	horde->friend_onBuildComplete();
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc != nullptr);
	REQUIRE(hc->getContainCount() == 3u);

	std::vector<Object *> fwd;
	hc->iterateContained([&](Object *o) { fwd.push_back(o); }, 1u);
	REQUIRE(fwd.size() == 3u);

	std::vector<Object *> rev;
	hc->iterateContained([&](Object *o) { rev.push_back(o); }, 1u | 8u);
	REQUIRE(rev.size() == 3u);
	CHECK(rev[0] == fwd[2]);
	CHECK(rev[1] == fwd[1]);
	CHECK(rev[2] == fwd[0]);

	// Release one member into m_garrisonEntering and test flags = 0x10.
	Object *released = fwd[0];
	hc->releaseMemberForGarrison(released);
	CHECK(hc->getContainCount() == 2u);

	std::vector<Object *> entering;
	hc->iterateContained([&](Object *o) { entering.push_back(o); }, 0x10u);
	REQUIRE(entering.size() == 1u);
	CHECK(entering[0] == released);
}

TEST_CASE("LOCAL-149 HordeContain::canSetFormation counts live entering members and rejects stale IDs (RW 0x0086FFD2 / RW 0x008704DA)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - RW 0x0086FFD2 (horde slot +0x64): calls slot +0x10C (RW 0x008704DA, contained + live resolved entering members),
	//   requires count >= 1, target == AlternateFormation, and count >= MinimumHordeSize for non-main alternate.
	Local149World w;
	Object *horde = w.unit("Local149HordeMain", 'A', 100.0f, 100.0f);
	horde->friend_onBuildComplete();
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc != nullptr);
	REQUIRE(hc->getContainCount() == 3u);

	const ThingTemplate *alt = w.logic->things().findTemplate("Local149HordeAlt");
	const ThingTemplate *mainT = w.logic->things().findTemplate("Local149HordeMain");
	REQUIRE(alt != nullptr);
	REQUIRE(mainT != nullptr);

	// Wrong template rejected; matching alternate accepted with 3 members (MinimumHordeSize = 3).
	CHECK_FALSE(hc->canSetFormation(nullptr));
	CHECK_FALSE(hc->canSetFormation(mainT));
	CHECK(hc->canSetFormation(alt));

	// Release 1 of 3 members for garrison: 2 contained + 1 live entering = 3 -> still accepted!
	Object *m0 = hc->getContainedItemsList()->front();
	hc->releaseMemberForGarrison(m0);
	CHECK(hc->getContainCount() == 2u);
	CHECK(hc->getMembersWithEntering().size() == 3u);
	CHECK(hc->canSetFormation(alt));
	CHECK(hc->canToggleFormation());

	// Destroy the entering member (stale ID left or forgotten): live count drops to 2 < MinimumHordeSize (3) -> rejected.
	w.logic->destroyObject(m0);
	w.frames(1);
	CHECK(hc->getMembersWithEntering().size() == 2u);
	CHECK_FALSE(hc->canSetFormation(alt));
	CHECK_FALSE(hc->canToggleFormation());

	// Add a 3rd soldier back and verify MSG_HORDE_SET_FORMATION swaps cleanly.
	Object *extra = w.unit("Local149Soldier", 'A', 100.0f, 100.0f);
	REQUIRE(hc->addToContain(extra));
	CHECK(hc->canSetFormation(alt));

	GameLogicDispatch dispatch(*w.logic);
	HordeCommands::registerHandlers(dispatch);
	GameMessage msg(MSG_HORDE_SET_FORMATION, horde->getControllingPlayer()->getPlayerIndex());
	msg.appendObjectIDArgument(horde->getID());
	msg.appendIntegerArgument((int)alt->getTemplateID());
	dispatch.dispatch(msg);
	auto *swapped = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(swapped != nullptr);
	CHECK_FALSE(swapped->isMainFormation());
}

TEST_CASE("LOCAL-149 HordeContain::isValidContainerFor rejects HORDE and delegates to TransportContain (RW 0x0086C758 / RW 0x0086C77A)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - RW 0x0086C758 (contain slot +0x98, tail RW 0x0086C77A): rejects any object with KindOf HORDE (template +0x115 bit 0x20),
	//   even if PassengerFilter includes +HORDE; otherwise delegates to TransportContain::isValidContainerFor (RW 0x86A002).
	Local149World w;
	Object *hordeA = w.unit("Local149HordeMain", 'A', 100.0f, 100.0f);
	hordeA->friend_onBuildComplete();
	Object *hordeB = w.unit("Local149HordeMain", 'A', 140.0f, 100.0f);
	hordeB->friend_onBuildComplete();
	Object *soldier = w.unit("Local149Soldier", 'A', 110.0f, 100.0f);
	Object *vehicle = w.unit("Local149OtherUnit", 'A', 115.0f, 100.0f);

	ContainModuleInterface *hc = hordeA->getContain();
	REQUIRE(hc != nullptr);
	REQUIRE(hc->getContainCount() == 3u); // Slots = 4 -> 1 free slot

	// HORDE is always rejected (RW 0x0086C75F..0x0086C76A) even with checkCapacity = false.
	CHECK_FALSE(hc->isValidContainerFor(*hordeB, true, false));
	CHECK_FALSE(hc->isValidContainerFor(*hordeB, false, false));

	// PassengerFilter (ANY +INFANTRY +HORDE) rejects VEHICLE.
	CHECK_FALSE(hc->isValidContainerFor(*vehicle, true, false));

	// Eligible infantry fits in remaining slot (3 + 1 <= 4).
	CHECK(hc->isValidContainerFor(*soldier, true, false));

	// TEMPORARILY_DEFECTED (status 0x3E) is rejected (TransportContain RW 0x86A01A).
	const int tempDef = ObjectTemplateInfoBuilder::objectStatusIndex("TEMPORARILY_DEFECTED");
	REQUIRE(tempDef == 0x3E);
	soldier->setStatus((unsigned)tempDef, true);
	CHECK_FALSE(hc->isValidContainerFor(*soldier, true, false));
	soldier->setStatus((unsigned)tempDef, false);

	// Fill the 4th slot -> checkCapacity = true now rejects, checkCapacity = false still allows.
	REQUIRE(hc->addToContain(soldier));
	CHECK(hc->getContainCount() == 4u);
	Object *extra = w.unit("Local149Soldier", 'A', 120.0f, 100.0f);
	CHECK_FALSE(hc->isValidContainerFor(*extra, true, false));
	CHECK(hc->isValidContainerFor(*extra, false, false));
}

TEST_CASE("LOCAL-149 Ennorath GondorFighterHorde neighbor checks (RW 0x0086C758 / RW 0x0086C8B9 / RW 0x0086FFD2 / RW 0x0087104A)")
{
	// Expectations from RotWK 2.01 game.dat and Ennorath GondorFighterHorde / GondorFighterHordeBlock INI definitions.
	ennorathscenario::World w;
	if (!w.load("LOCAL-149 Ennorath GondorFighterHorde"))
	{
		return;
	}
	structtest::Arena arena(w.shared, "FactionMen", "FactionMordor");
	Object *horde = w.place(arena, "GondorFighterHorde", 0, 100.0f, 100.0f);
	REQUIRE(horde != nullptr);
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc != nullptr);
	REQUIRE(hc->getContainCount() > 0u); // tolerance: GondorFighterHorde InitialPayload size from Ennorath INI; exact equality verified against iterateContained below

	const ThingTemplate *alt = hc->alternateFormationTemplate();
	REQUIRE(alt != nullptr);
	CHECK(hc->canSetFormation(alt));

	// Neighbor check: iterateContained (flags = 1) matches contained count, and releaseMemberForGarrison moves one into flags = 0x10.
	unsigned countFwd = 0;
	hc->iterateContained([&](Object *o) { if (o) ++countFwd; }, 1u);
	CHECK(countFwd == hc->getContainCount());

	Object *m0 = hc->getContainedItemsList()->front();
	hc->releaseMemberForGarrison(m0);
	unsigned countEntering = 0;
	hc->iterateContained([&](Object *o) { if (o == m0) ++countEntering; }, 0x10u);
	CHECK(countEntering == 1u);
	CHECK(hc->canSetFormation(alt));

	// Neighbor check: HordeContain rejects another GondorFighterHorde as a passenger.
	Object *horde2 = w.place(arena, "GondorFighterHorde", 0, 150.0f, 100.0f);
	REQUIRE(horde2 != nullptr);
	CHECK_FALSE(hc->isValidContainerFor(*horde2, true, false));
}
