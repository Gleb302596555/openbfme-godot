// OpenBFME unit tests: LOCAL-18 CombineHordeSpecialPower and SPECIAL_GIVE_UPGRADE_NEAREST (issue #143).
//
// Target facts (RotWK game.dat):
//   * CombineHordeSpecialPower: constructor RW 0x8CCBF0, vtable RW 0xC75420 / 0xC753B8.
//   * CombineHordeSpecialPowerModuleData: constructor RW 0x8CCC95 (m_scanRange = 100.0f default RW 0xBD88D8),
//     field table RW 0xC75398 (ScanRange at +0x7C, INI::parseReal), buildFieldParse RW 0x8CCBD0.
//   * doSpecialPower (slot 0x28 / primary 0x60):
//     - On Horde: RW 0x8CD08C relays to contained members.
//     - On Unit: RW 0x8CCCB4 searches partition within m_scanRange for nearest ally horde where
//       hci->canCombineWith(*this) is true, then initiates combine (RW 0x770F43 AI command 0x45 -> combineWith).
//   * doSpecialPowerAtLocation (RW 0x8CCEC9) / doSpecialPowerAtObject (RW 0x8CCF76): member relays.
//   * SPECIAL_GIVE_UPGRADE_NEAREST: enum 136 (0x88, RW 0xDA5C90, string RW 0xC34058).
//   * Mutual abort between SPECIAL_GIVE_UPGRADE (0x33) and SPECIAL_GIVE_UPGRADE_NEAREST (0x88) at RW 0x897EAB.
//   * GiveUpgradeUpdate::initiateIntentToDoSpecialPower (RW 0x8A006C): calls findDeliveryTarget() (RW 0x89FEBD).
//   * CommandTrigger execution in ControlBar (RW 0x94157D..0x9415FF) and CommandXlat (RW 0x81E346).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

namespace {

const char kLocal18Objects[] =
	"SpecialPower SpecialAbilityCombineWithNearest\n"
	" Enum = SPECIAL_GIVE_UPGRADE_NEAREST\n"
	" ReloadTime = 100\n"
	"End\n"
	"SpecialPower SpecialAbilityGiveUpgradeNearest\n"
	" Enum = SPECIAL_GIVE_UPGRADE_NEAREST\n"
	" ReloadTime = 100\n"
	"End\n"
	"Object HaradrimBannerDefaultRange\n"
	" KindOf = INFANTRY BANNER SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = CombineHordeSpecialPower ModuleTag_CombineWithNearest\n"
	"  SpecialPowerTemplate = SpecialAbilityCombineWithNearest\n"
	" End\n"
	"End\n"
	"Object HaradrimBannerParsedRange\n"
	" KindOf = INFANTRY BANNER SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = CombineHordeSpecialPower ModuleTag_CombineWithNearest\n"
	"  SpecialPowerTemplate = SpecialAbilityCombineWithNearest\n"
	"  ScanRange = 15000.0\n"
	" End\n"
	"End\n"
	"Object HaradrimBanner500\n"
	" KindOf = INFANTRY BANNER SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = CombineHordeSpecialPower ModuleTag_CombineWithNearest\n"
	"  SpecialPowerTemplate = SpecialAbilityCombineWithNearest\n"
	"  ScanRange = 500.0\n"
	" End\n"
	"End\n"
	"Object HaradrimHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Swordsman Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 2\n"
	"  ComboHorde = Target:HaradrimBannerHorde Result:HaradrimComboHorde InitiateVoice:NoSound\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object HaradrimBannerHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 15\n GeometryMinorRadius = 15\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 1\n"
	"  RankInfo = RankNumber:1 UnitType:HaradrimBanner500 Position:X:0 Y:0\n"
	"  InitialPayload = HaradrimBanner500 1\n"
	"  ComboHorde = Target:HaradrimHorde Result:HaradrimComboHorde InitiateVoice:NoSound\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object HaradrimHordeNoEntry\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Swordsman Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 2\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object HaradrimComboHorde\n"
	" KindOf = HORDE COMBO_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 3\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0 Position:X:20 Y:20\n"
	"  RankInfo = RankNumber:2 UnitType:HaradrimBanner500 Position:X:-20 Y:0\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object RelayMemberUnit\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = CombineHordeSpecialPower ModuleTag_MemberCombine\n"
	"  SpecialPowerTemplate = SpecialAbilityCombineWithNearest\n"
	"  ScanRange = 100.0\n"
	" End\n"
	"End\n"
	"Object RelayHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:RelayMemberUnit Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:RelayMemberUnit Position:X:-20 Y:0\n"
	"  InitialPayload = RelayMemberUnit 2\n"
	" End\n"
	" Behavior = CombineHordeSpecialPower ModuleTag_HordeCombine\n"
	"  SpecialPowerTemplate = SpecialAbilityCombineWithNearest\n"
	"  ScanRange = 100.0\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object PorterDeliverUnit\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule PowerTag\n"
	"  SpecialPowerTemplate = SpecialAbilityGiveUpgradeNearest\n"
	"  UpdateModuleStartsAttack = Yes\n"
	" End\n"
	" Behavior = GiveUpgradeUpdate GiveTag\n"
	"  SpecialPowerTemplate = SpecialAbilityGiveUpgradeNearest\n"
	"  StartAbilityRange = 100\n"
	"  IgnoreFacingCheck = Yes\n ApproachRequiresLOS = No\n"
	"  UnpackTime = 200\n PreparationTime = 200\n PackTime = 200\n DeliverUpgrade = No\n"
	" End\n"
	"End\n"
	"Object PorterRecipientUnit\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = LocomotorSetUpgrade UpgradeTag\n"
	"  TriggeredBy = Upgrade_Barracks2\n"
	" End\n"
	"End\n";

struct Local18World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previous = TheSpecialPowerStore;

	Local18World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string error = w.load(kLocal18Objects, INI_LOAD_OVERWRITE, "local18.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}

	~Local18World()
	{
		TheSpecialPowerStore = previous;
	}
};

} // namespace

TEST_CASE("LOCAL-18: CombineHordeSpecialPower INI parsing and default scan range")
{
	Local18World w;

	// Default scan range: 100.0f (RW 0xBD88D8)
	Object *defObj = w.unit("HaradrimBannerDefaultRange", 'A', 100, 100);
	REQUIRE(defObj);
	auto *defModule = dynamic_cast<CombineHordeSpecialPower *>(defObj->findModule("CombineHordeSpecialPower"));
	REQUIRE(defModule);
	CHECK(defModule->scanRange() == 100.0f);
	REQUIRE(defModule->data());
	CHECK(defModule->data()->m_scanRange == 100.0f);

	// Parsed scan range from Ennorath INI: 15000.0f
	Object *parsedObj = w.unit("HaradrimBannerParsedRange", 'A', 200, 200);
	REQUIRE(parsedObj);
	auto *parsedModule = dynamic_cast<CombineHordeSpecialPower *>(parsedObj->findModule("CombineHordeSpecialPower"));
	REQUIRE(parsedModule);
	CHECK(parsedModule->scanRange() == 15000.0f);
	REQUIRE(parsedModule->data());
	CHECK(parsedModule->data()->m_scanRange == 15000.0f);
}

TEST_CASE("LOCAL-18: Haradrim banner joins nearest Haradrim horde via ComboHorde")
{
	Local18World w;
	Object *horde = w.unit("HaradrimHorde", 'A', 300, 300);
	// Place banner horde 60 units away (within ScanRange = 500.0)
	Object *bannerHorde = w.unit("HaradrimBannerHorde", 'A', 360, 300);
	REQUIRE(horde);
	REQUIRE(bannerHorde);
	w.frames(2);

	const std::vector<Object *> bannerMembers = w.membersOf(bannerHorde);
	REQUIRE(bannerMembers.size() == 1);
	Object *banner = bannerMembers.front();
	REQUIRE(banner);

	auto *sp = dynamic_cast<CombineHordeSpecialPower *>(banner->findModule("CombineHordeSpecialPower"));
	REQUIRE(sp);

	// Activate combine special power on the banner carrier
	sp->doSpecialPower(0);

	// The combine should succeed: old hordes are destroyed, new combo horde is created
	CHECK(horde->isDestroyed());
	CHECK(bannerHorde->isDestroyed());

	// Find the resulting combo horde in the world
	bool foundCombo = false;
	for (Object *cur = w.logic->getFirstObject(); cur; cur = cur->getNextObject())
	{
		if (!cur->isDestroyed() && cur->getTemplate()->getName() == "HaradrimComboHorde")
		{
			foundCombo = true;
			CHECK(cur->getTeam() == w.teamA());
			break;
		}
	}
	CHECK(foundCombo);
}

TEST_CASE("LOCAL-18: Out of ScanRange banner does not join horde")
{
	Local18World w;
	Object *horde = w.unit("HaradrimHorde", 'A', 300, 300);
	// Place banner carrier 600 units away (outside ScanRange = 500.0)
	Object *bannerHorde = w.unit("HaradrimBannerHorde", 'A', 900, 300);
	REQUIRE(horde);
	REQUIRE(bannerHorde);
	w.frames(2);

	const std::vector<Object *> bannerMembers = w.membersOf(bannerHorde);
	REQUIRE(bannerMembers.size() == 1);
	Object *banner = bannerMembers.front();
	REQUIRE(banner);

	auto *sp = dynamic_cast<CombineHordeSpecialPower *>(banner->findModule("CombineHordeSpecialPower"));
	REQUIRE(sp);

	// Activate combine special power
	sp->doSpecialPower(0);

	// Neither horde should be destroyed or combined
	CHECK_FALSE(horde->isDestroyed());
	CHECK_FALSE(bannerHorde->isDestroyed());
}

TEST_CASE("LOCAL-18: Horde without matching entry does not combine")
{
	Local18World w;
	Object *hordeNoEntry = w.unit("HaradrimHordeNoEntry", 'A', 300, 300);
	Object *bannerHorde = w.unit("HaradrimBannerHorde", 'A', 350, 300);
	REQUIRE(hordeNoEntry);
	REQUIRE(bannerHorde);
	w.frames(2);

	const std::vector<Object *> bannerMembers = w.membersOf(bannerHorde);
	REQUIRE(bannerMembers.size() == 1);
	Object *banner = bannerMembers.front();
	REQUIRE(banner);

	auto *sp = dynamic_cast<CombineHordeSpecialPower *>(banner->findModule("CombineHordeSpecialPower"));
	REQUIRE(sp);

	sp->doSpecialPower(0);

	// Without a matching ComboHorde entry, canCombineWith returns false
	CHECK_FALSE(hordeNoEntry->isDestroyed());
	CHECK_FALSE(bannerHorde->isDestroyed());
}

TEST_CASE("LOCAL-18: Relaying doSpecialPower over horde members")
{
	Local18World w;
	Object *horde = w.unit("RelayHorde", 'A', 300, 300);
	REQUIRE(horde);
	w.frames(2);

	const std::vector<Object *> members = w.membersOf(horde);
	REQUIRE(members.size() == 2);

	auto *hordeSp = dynamic_cast<CombineHordeSpecialPower *>(horde->findModule("CombineHordeSpecialPower"));
	REQUIRE(hordeSp);

	// Check initiates count before
	const unsigned long long initiatesBefore = hordeSp->initiates();

	// Invoke doSpecialPower on the horde
	hordeSp->doSpecialPower(0);

	// Horde's baseDo was called
	CHECK(hordeSp->initiates() == initiatesBefore + 1);

	// Contained members also received initiates
	for (Object *m : members)
	{
		auto *memberSp = dynamic_cast<CombineHordeSpecialPower *>(m->findModule("CombineHordeSpecialPower"));
		REQUIRE(memberSp);
		CHECK(memberSp->initiates() == 1);
	}
}

TEST_CASE("LOCAL-18: Porter delivers upgrade to nearest valid target with SPECIAL_GIVE_UPGRADE_NEAREST")
{
	Local18World w;
	Object *porter = w.unit("PorterDeliverUnit", 'A', 300, 300);
	Object *nearRecipient = w.unit("PorterRecipientUnit", 'A', 330, 300);
	Object *farRecipient = w.unit("PorterRecipientUnit", 'A', 450, 300);
	REQUIRE(porter);
	REQUIRE(nearRecipient);
	REQUIRE(farRecipient);

	const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
	REQUIRE(upgrade);
	porter->giveUpgrade(upgrade);
	REQUIRE(porter->hasUpgrade(upgrade));
	REQUIRE(nearRecipient->affectedByUpgrade(upgrade));
	REQUIRE(farRecipient->affectedByUpgrade(upgrade));

	// Trigger special power via doSpecialPower (like button click in game)
	auto *spModule = dynamic_cast<SpecialPowerModule *>(porter->findModule("SpecialPowerModule"));
	REQUIRE(spModule);
	spModule->doSpecialPower(0);

	// Allow frames for porter to approach and deliver
	w.frames(25);

	// Nearest recipient received upgrade
	CHECK(nearRecipient->hasUpgrade(upgrade));
	CHECK_FALSE(farRecipient->hasUpgrade(upgrade));

	// Porter fades and is destroyed after delivery
	const ObjectID porterId = porter->getID();
	w.frames(50);
	CHECK(w.logic->findObjectByID(porterId) == nullptr);
}
