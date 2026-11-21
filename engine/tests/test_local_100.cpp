// OpenBFME. GPL-3.0.
// LOCAL-100 (fork issue 766): upgrade modules uncited functions (editions: both RotWK 2.01 and BFME2 1.06).
// Sources:
//   - GiveUpgradeUpdate::initiateIntentToDoSpecialPower: RW 0x0089FC9D / DONOR BFME2 0x0049C51A
//   - GiveUpgradeUpdate::allowsSpecialPowerUse: RW 0x0089FCD6 / DONOR BFME2 0x0049C553 (GiveUpgradeUpdateSlot8.cpp)
//   - WallUpgradeUpdate::canResetUpgrades: RW 0x008AEBA6 / DONOR BFME2 0x004AB4AB (model condition 0x45 ACTIVELY_BEING_CONSTRUCTED, RW 0x46E918)
//   - Ennorath INI: data/ini/object/goodfaction/structures/men/barracks.ini, marketplace.ini, fortress.ini

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameLogic/Construction.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/WallUpgradeUpdate.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char kGiveObjects[] =
	"SpecialPower TestGivePower100\n Enum = SPECIAL_GIVE_UPGRADE\n ReloadTime = 30000\nEnd\n"
	"Object TestCarrier100\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule PowerTag\n SpecialPowerTemplate = TestGivePower100\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = GiveUpgradeUpdate GiveTag\n SpecialPowerTemplate = TestGivePower100\n StartAbilityRange = 100\n"
	" IgnoreFacingCheck = Yes\n ApproachRequiresLOS = No\n UnpackTime = 200\n PreparationTime = 200\n PackTime = 200\n DeliverUpgrade = Yes\n End\nEnd\n"
	"Object TestRecipient100\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = LocomotorSetUpgrade UpgradeTag\n TriggeredBy = Upgrade_Barracks2\n End\nEnd\n";

struct GiveWorld100 : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previous = TheSpecialPowerStore;
	GiveWorld100()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string err = w.load(kGiveObjects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~GiveWorld100() { TheSpecialPowerStore = previous; }
};

const char kWallObjects[] =
	"Object TestWallSegment100\n KindOf = STRUCTURE WALK_ON_TOP_OF_WALL WALL_UPGRADE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 500\n End\nEnd\n"
	"Object TestWallTower100\n KindOf = STRUCTURE UPGRADE_GARRISON\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 500\n End\n"
	" Behavior = WallUpgradeUpdate WallTag\n End\nEnd\n";
} // namespace

// RW 0x0089FCD6 / DONOR BFME2 0x0049C553: allowsSpecialPowerUse returns false while target is alive,
// and true when no target is set or the target is effectively dead (+0x458 bit 0).
TEST_CASE("LOCAL-100 GiveUpgradeUpdate: allowsSpecialPowerUse rejects live target and allows unset or dead target (RW 0x0089FCD6)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		GiveWorld100 w;
		w.logic->setEdition(edition);
		CAPTURE(gameEditionName(edition));

		Object *carrier = w.unit("TestCarrier100", 'A', 300, 300);
		Object *recipient = w.unit("TestRecipient100", 'A', 380, 300);
		REQUIRE(carrier);
		REQUIRE(recipient);
		const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
		REQUIRE(u);
		carrier->giveUpgrade(u);

		auto *give = dynamic_cast<GiveUpgradeUpdate *>(carrier->findModule("GiveUpgradeUpdate"));
		REQUIRE(give);

		// No target yet -> allowed (RW 0x89FCE6 -> true)
		CHECK(give->allowsSpecialPowerUse());

		// Initiate with a live target -> disallowed while target is alive (RW 0x89FCFD .. 0x89FD0A -> false)
		give->initiateIntentToDoSpecialPower(give->abilityData()->m_specialPowerTemplate, recipient, nullptr, 0, 0);
		CHECK_FALSE(give->allowsSpecialPowerUse());

		// Target becomes effectively dead -> allowed again (RW 0x89FD01 bit 0 -> true)
		recipient->friend_setEffectivelyDead(true);
		CHECK(give->allowsSpecialPowerUse());
	}
}

// RW 0x0089FC9D / DONOR BFME2 0x0049C51A: while searching (+0x89) with a live target (+0x40),
// initiateIntentToDoSpecialPower returns early and delivers to the existing target.
TEST_CASE("LOCAL-100 GiveUpgradeUpdate: initiateIntentToDoSpecialPower ignores re-initiation while searching with live target (RW 0x0089FC9D)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		// Subcase A: second initiation while searching with a live target must be ignored (RW 0x89FCA3 .. 0x89FCB9).
		{
			GiveWorld100 w;
			w.logic->setEdition(edition);
			CAPTURE(gameEditionName(edition));

			Object *carrier = w.unit("TestCarrier100", 'A', 300, 300);
			Object *first = w.unit("TestRecipient100", 'A', 305, 300);
			Object *second = w.unit("TestRecipient100", 'A', 305, 305);
			REQUIRE(carrier);
			REQUIRE(first);
			REQUIRE(second);
			const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
			REQUIRE(u);
			carrier->giveUpgrade(u);

			auto *give = dynamic_cast<GiveUpgradeUpdate *>(carrier->findModule("GiveUpgradeUpdate"));
			REQUIRE(give);

			give->initiateIntentToDoSpecialPower(give->abilityData()->m_specialPowerTemplate, first, nullptr, 0, 0);
			give->initiateIntentToDoSpecialPower(give->abilityData()->m_specialPowerTemplate, second, nullptr, 0, 0);
			w.frames(20);
			CHECK(first->hasUpgrade(u));
			CHECK_FALSE(second->hasUpgrade(u));
		}

		// Subcase B: once the first target is destroyed, findObjectByID returns null and re-initiation delivers to `second`.
		{
			GiveWorld100 w;
			w.logic->setEdition(edition);
			CAPTURE(gameEditionName(edition));

			Object *carrier = w.unit("TestCarrier100", 'A', 300, 300);
			Object *first = w.unit("TestRecipient100", 'A', 305, 300);
			Object *second = w.unit("TestRecipient100", 'A', 305, 305);
			REQUIRE(carrier);
			REQUIRE(first);
			REQUIRE(second);
			const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
			REQUIRE(u);
			carrier->giveUpgrade(u);

			auto *give = dynamic_cast<GiveUpgradeUpdate *>(carrier->findModule("GiveUpgradeUpdate"));
			REQUIRE(give);

			give->initiateIntentToDoSpecialPower(give->abilityData()->m_specialPowerTemplate, first, nullptr, 0, 0);
			w.logic->destroyObject(first);
			give->initiateIntentToDoSpecialPower(give->abilityData()->m_specialPowerTemplate, second, nullptr, 0, 0);
			w.frames(20);
			CHECK(second->hasUpgrade(u));
		}
	}
}

// RW 0x008AEBA6 / DONOR BFME2 0x004AB4AB: canResetUpgrades (called from update RW 0x8AEBD3) returns true
// when the attached wall is alive (!isEffectivelyDead) and not ACTIVELY_BEING_CONSTRUCTED (model condition 0x45, RW 0x46E918).
TEST_CASE("LOCAL-100 WallUpgradeUpdate: canResetUpgrades checks !isEffectivelyDead and !ACTIVELY_BEING_CONSTRUCTED (RW 0x008AEBA6)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		combattest::CombatWorld w;
		const std::string err = w.w.load(kWallObjects);
		REQUIRE_MESSAGE(err.empty(), err);
		w.logic->setEdition(edition);
		CAPTURE(gameEditionName(edition));

		Object *wall = w.unit("TestWallSegment100", 'A', 300, 300);
		Object *tower = w.unit("TestWallTower100", 'A', 310, 300);
		REQUIRE(wall);
		REQUIRE(tower);
		const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
		REQUIRE(u);
		tower->giveUpgrade(u);
		REQUIRE(tower->hasUpgrade(u));

		auto *wu = dynamic_cast<WallUpgradeUpdate *>(tower->findModule("WallUpgradeUpdate"));
		REQUIRE(wu);

		// First frame scans and attaches to `wall`.
		w.frames(1);
		REQUIRE(wu->attachedObjectID() == wall->getID());

		// Mark wall as ACTIVELY_BEING_CONSTRUCTED (model condition 0x45) and trigger rubble countdown (40 frames).
		const int kActively = Construction::modelConditionIndex("ACTIVELY_BEING_CONSTRUCTED");
		REQUIRE(kActively == 0x45);
		wall->setModelConditionState(kActively, true);
		wu->onBodyDamageStateChange(nullptr, BODY_PRISTINE, BODY_RUBBLE);

		// After 45 frames (> 40 countdown), reset must still be blocked by ACTIVELY_BEING_CONSTRUCTED (RW 0x8AEBC1).
		w.frames(45);
		CHECK(tower->hasUpgrade(u));

		// Clear ACTIVELY_BEING_CONSTRUCTED, but mark wall effectively dead (+0x458 bit 0) -> still blocked (RW 0x8AEBB8).
		wall->setModelConditionState(kActively, false);
		wall->friend_setEffectivelyDead(true);
		w.frames(10);
		CHECK(tower->hasUpgrade(u));

		// Wall alive and built (even with KindOf WALL_UPGRADE) -> canResetUpgrades() returns true and resets upgrades.
		wall->friend_setEffectivelyDead(false);
		w.frames(10);
		CHECK_FALSE(tower->hasUpgrade(u));
	}
}

// Ennorath INI: GondorBarracks (barracks.ini CommandSetUpgrade), GondorMarketPlace (marketplace.ini CostModifierUpgrade),
// upgrade grant and removal.
TEST_CASE("LOCAL-100 ENNORATH scenario: CommandSetUpgrade and CostModifierUpgrade on Ennorath Gondor structures")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-100 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	const int p1 = game.index();
	Player *player = game.game->players().getNthPlayer(p1);
	REQUIRE(player != nullptr);
	std::string err;

	// 1. CommandSetUpgrade on GondorBarracks (barracks.ini): Level2 swaps command set, Level3 conflicts and swaps again, resetUpgrades clears it.
	Object *barracks = game.game->createObject("GondorBarracks", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(barracks != nullptr, err);
	const UpgradeTemplate *lvl2 = TheUpgradeCenter->findUpgrade("Upgrade_GondorBarracksLevel2");
	const UpgradeTemplate *lvl3 = TheUpgradeCenter->findUpgrade("Upgrade_GondorBarracksLevel3");
	REQUIRE(lvl2 != nullptr);
	REQUIRE(lvl3 != nullptr);
	CHECK(barracks->getCommandSetOverride().empty());
	barracks->giveUpgrade(lvl2);
	const std::string csLvl2 = barracks->getCommandSetOverride();
	CHECK_FALSE(csLvl2.empty());
	barracks->giveUpgrade(lvl3);
	const std::string csLvl3 = barracks->getCommandSetOverride();
	CHECK_FALSE(csLvl3.empty());
	CHECK(csLvl3 != csLvl2);
	barracks->resetUpgrades();
	CHECK(barracks->getCommandSetOverride().empty());

	// 2. CostModifierUpgrade on GondorMarketPlace (marketplace.ini): Upgrade_MarketplaceUpgradeIronOre adds discount, resetUpgrades removes it.
	Object *market = game.game->createObject("GondorMarketPlace", p1, Coord3D{ 1850.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(market != nullptr, err);
	const UpgradeTemplate *ironOre = TheUpgradeCenter->findUpgrade("Upgrade_MarketplaceUpgradeIronOre");
	REQUIRE(ironOre != nullptr);
	const size_t beforeDiscounts = player->upgradeDiscounts().size() + player->costModifiers().size();
	market->giveUpgrade(ironOre);
	const size_t afterDiscounts = player->upgradeDiscounts().size() + player->costModifiers().size();
	CHECK(afterDiscounts == beforeDiscounts + 1);
	market->resetUpgrades();
	const size_t removedDiscounts = player->upgradeDiscounts().size() + player->costModifiers().size();
	CHECK(removedDiscounts == beforeDiscounts);
}
