// OpenBFME. MODGAP-1 tests (PROBLEMS.md P-10, stop S-260): SalvageCrateCollide on synthetic data. The target facts are RotWK game.dat
// RW 0x8C06B6 (CrateCollide::onCollide), 0x8C080B / 0x8C145D (validity), 0x8C136C (outcome), 0x8C158B (execute), 0x8C148B (money); the touches come from
// AIWorld::processCrateContacts (S-780). The expected values are the chest's own data (MinResource, Upgrade), not read back from the port.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "GameLogic/Economy.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/Module/SalvageCrateCollide.h"

using namespace combattest;

namespace
{
const char kSalvageObjects[] =
	"Object SalvPicker\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	// a chest by its own data: 250 resources, the computer players may take it
	"Object SalvChest\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    LevelUpChance = 0%\n"
	"    ResourceChance = 100%\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"    ExecuteFX = FX_TestChestPickup\n"
	"  End\n"
	"End\n"
	"Object SalvChestNoInfantry\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = INFANTRY\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	"Object SalvChestNoOwner\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbidOwnerPlayer = Yes\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	"Object SalvChestNoAIPickup\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"  End\n"
	"End\n"
	"Object SalvChestUpgrade\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    LevelUpChance = 0%\n"
	"    Upgrade = Upgrade_X\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n";

const Economy::IncomeEvent *incomeAt(CombatWorld &w, ObjectID object)
{
	const Economy::IncomeEvent *found = nullptr;
	for (const Economy::IncomeEvent &e : w.logic->economy().incomeLog())
	{
		if (e.object == object)
		{
			found = &e;
		}
	}
	return found;
}
} // namespace

TEST_CASE("modgap1 salvage: a unit that touches a chest takes its money, the chest goes, ExecuteFX plays and GUI:AddCash is logged at the chest (P-10)")
{
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChest", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	const ObjectID chestId = chest->getID();
	w.unit("SalvPicker", 'A', 305, 300); // 5 apart; the footprints (10 and 8) overlap
	const int before = w.cash('A');
	w.frames(3);
	CHECK(w.cash('A') == before + 250);
	CHECK(w.byId(chestId) == nullptr);
	const Economy::IncomeEvent *text = incomeAt(w, chestId);
	REQUIRE(text != nullptr);
	CHECK(text->amount == 250);
	CHECK(text->playerIndex == w.playerOf('A')->getPlayerIndex());
	const auto &sites = w.logic->fxEvents().perSite();
	REQUIRE(sites.count("CrateCollide") == 1);
	CHECK(sites.at("CrateCollide") == 1);
}

TEST_CASE("modgap1 salvage: a unit that does not touch the chest leaves it alone")
{
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChest", 'A', 300, 300);
	const ObjectID chestId = chest->getID();
	w.unit("SalvPicker", 'A', 330, 300); // 30 apart, footprints 10 + 8
	const int before = w.cash('A');
	w.frames(4);
	CHECK(w.cash('A') == before);
	CHECK(w.byId(chestId) != nullptr);
}

TEST_CASE("modgap1 salvage: RequiredKindOf / ForbiddenKindOf, ForbidOwnerPlayer and AllowAIPickup decide who may take a chest (RW 0x8C080B, 0x8C158B)")
{
	SUBCASE("ForbiddenKindOf = INFANTRY refuses an infantry unit")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestNoInfantry", 'A', 300, 300);
		const ObjectID chestId = chest->getID();
		w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		w.frames(4);
		CHECK(w.cash('A') == before);
		CHECK(w.byId(chestId) != nullptr);
	}
	SUBCASE("ForbidOwnerPlayer: the chest's own player may not take it, the other player may")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestNoOwner", 'A', 300, 300);
		const ObjectID chestId = chest->getID();
		w.unit("SalvPicker", 'A', 305, 300);
		w.frames(4);
		CHECK(w.byId(chestId) != nullptr);
		const int bobBefore = w.cash('B');
		w.unit("SalvPicker", 'B', 295, 300);
		w.frames(4);
		CHECK(w.cash('B') == bobBefore + 250);
		CHECK(w.byId(chestId) == nullptr);
	}
	SUBCASE("without AllowAIPickup a computer player's unit does not take the chest, a human's does")
	{
		CombatWorld w(kSalvageObjects);
		REQUIRE(w.playerOf('B')->getPlayerType() == PLAYER_COMPUTER);
		REQUIRE(w.playerOf('A')->getPlayerType() != PLAYER_COMPUTER);
		Object *chest = w.unit("SalvChestNoAIPickup", 'A', 300, 300);
		const ObjectID chestId = chest->getID();
		const int bobBefore = w.cash('B');
		w.unit("SalvPicker", 'B', 305, 300);
		w.frames(4);
		CHECK(w.cash('B') == bobBefore);
		CHECK(w.byId(chestId) != nullptr);
		const int aliceBefore = w.cash('A');
		w.unit("SalvPicker", 'A', 295, 300);
		w.frames(4);
		CHECK(w.cash('A') == aliceBefore + 250);
		CHECK(w.byId(chestId) == nullptr);
	}
}

TEST_CASE("modgap1 salvage: two units on one chest in one frame: the lower id takes it, the chest pays once")
{
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChest", 'A', 300, 300);
	const ObjectID chestId = chest->getID();
	w.unit("SalvPicker", 'A', 305, 300);
	w.unit("SalvPicker", 'B', 295, 300);
	const int aliceBefore = w.cash('A'), bobBefore = w.cash('B');
	w.frames(4);
	CHECK(w.cash('A') == aliceBefore + 250);
	CHECK(w.cash('B') == bobBefore);
	CHECK(w.byId(chestId) == nullptr);
}

TEST_CASE("modgap1 salvage: a chest with an Upgrade gives the picker's player the player upgrade and no money (RW 0x8C15F8)")
{
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestUpgrade", 'A', 300, 300);
	const ObjectID chestId = chest->getID();
	w.unit("SalvPicker", 'A', 305, 300);
	const int before = w.cash('A');
	REQUIRE_FALSE(w.playerOf('A')->hasUpgradeComplete("Upgrade_X"));
	w.frames(4);
	CHECK(w.playerOf('A')->hasUpgradeComplete("Upgrade_X"));
	CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_X"));
	CHECK(w.cash('A') == before);
	CHECK(w.byId(chestId) == nullptr);
}
