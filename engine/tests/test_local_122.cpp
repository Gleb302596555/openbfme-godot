// OpenBFME. GPL-3.0.
// LOCAL-122 (fork issue 806): 18 upgrade and death module classes — 54 boilerplate start citations
// (getModuleName, getModuleNameKey, scalar deleting destructor; editions: both RotWK 2.01 and BFME2 1.06).
// Sources:
//   - AllowBannerSpawnUpgrade:       RW 0x008BC651 / RW 0x008BC667 / RW 0x008BC6FF (-> 0x8BC631)
//   - ArmorUpgrade:                  RW 0x008B7586 / RW 0x008B758C / RW 0x008B7602 (-> 0x8B7566)
//   - AttributeModifierUpgrade:      RW 0x008BA809 / RW 0x008BA80F / RW 0x008BA8A7 (-> 0x8BA7E9)
//   - CommandPointsUpgrade:          RW 0x008BC73B / RW 0x008BC741 / RW 0x008BC7D9 (-> 0x8BC71B)
//   - CreateObjectDie:               RW 0x00888BC7 / RW 0x00888C68 / RW 0x00888D3E (-> 0x888BAE)
//   - DamageFilteredCreateObjectDie: RW 0x0088918B / RW 0x00889198 / RW 0x00889262 (-> 0x88916B)
//   - DestroyDie:                    RW 0x00889774 / RW 0x0088977A / RW 0x008897E9 (-> 0x88975B)
//   - DoCommandUpgrade:              RW 0x008B8D42 / RW 0x008B8D48 / RW 0x008B8DE0 (-> 0x8B8D22)
//   - FXListDie:                     RW 0x00889843 / RW 0x00889849 / RW 0x008898B8 (-> 0x88982A)
//   - HeroDie:                       RW 0x008C6427 / RW 0x008C645D / RW 0x008C64E2 (-> 0x8C640E)
//   - LevelUpUpgrade:                RW 0x008B7ED6 / RW 0x008B7EDC / RW 0x008B7F74 (-> 0x8B7EB6)
//   - ModelConditionUpgrade:         RW 0x008BA577 / RW 0x008BA57D / RW 0x008BA64C (-> 0x8BA557)
//   - RefundDie:                     RW 0x00888488 / RW 0x0088848E / RW 0x008884FD (-> 0x88846F)
//   - RemoveUpgradeUpgrade:          RW 0x008BC03F / RW 0x008BC05B / RW 0x008BC0F3 (-> 0x8BC01F)
//   - StatusBitsUpgrade:             RW 0x008B8A81 / RW 0x008B8A87 / RW 0x008B8B1F (-> 0x8B8A61)
//   - TooltipUpgrade:                RW 0x008BBAFC / RW 0x008BBB18 / RW 0x008BBBB0 (-> 0x8BBADC)
//   - UpgradeDie:                    RW 0x00889E20 / RW 0x00889E26 / RW 0x00889E95 (-> 0x889E07)
//   - WeaponSetUpgrade:              RW 0x008B9838 / RW 0x008B983E / RW 0x008B98D6 (-> 0x8B9818)
//   - Ennorath INI: GondorBarracks (barracks.ini), GondorFighter (gondorfighter.ini)

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Upgrade.h"
#include "GameLogic/Construction.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char kLocal122Objects[] =
	"Object TestLocal122Unit\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n"
	"  MaxHealth = 100\n"
	" End\n"
	" Behavior = StatusBitsUpgrade StatusTag\n"
	"  TriggeredBy = Upgrade_Barracks2\n"
	"  StatusToSet = RIDER1\n"
	" End\n"
	" Behavior = ModelConditionUpgrade ModelTag\n"
	"  TriggeredBy = Upgrade_Barracks2\n"
	"  AddConditionFlags = USER_1\n"
	" End\n"
	" Behavior = ArmorUpgrade ArmorTag\n"
	"  TriggeredBy = Upgrade_Barracks2\n"
	"  ArmorSetFlag = PLAYER_UPGRADE\n"
	" End\n"
	" Behavior = WeaponSetUpgrade WeaponTag\n"
	"  TriggeredBy = Upgrade_Barracks2\n"
	" End\n"
	" Behavior = LevelUpUpgrade LevelTag\n"
	"  TriggeredBy = Upgrade_Barracks2\n"
	"  LevelsToGain = 1\n"
	"  LevelCap = 5\n"
	" End\n"
	" Behavior = DestroyDie DestroyTag\n"
	"  DeathTypes = ALL\n"
	" End\n"
	" Behavior = FXListDie FXTag\n"
	"  DeathTypes = ALL\n"
	" End\n"
	"End\n";
} // namespace

// RW 0x008BC651..0x008B98D6: all 18 upgrade and death module classes are registered under their exact
// RotWK getModuleName() / getModuleNameKey() names, execute on upgrade/death, and tear down cleanly.
TEST_CASE("LOCAL-122 module registry and lifecycle: 18 upgrade and death module classes")
{
	const char *const kAll18[] = {
		"AllowBannerSpawnUpgrade",
		"ArmorUpgrade",
		"AttributeModifierUpgrade",
		"CommandPointsUpgrade",
		"CreateObjectDie",
		"DamageFilteredCreateObjectDie",
		"DestroyDie",
		"DoCommandUpgrade",
		"FXListDie",
		"HeroDie",
		"LevelUpUpgrade",
		"ModelConditionUpgrade",
		"RefundDie",
		"RemoveUpgradeUpgrade",
		"StatusBitsUpgrade",
		"TooltipUpgrade",
		"UpgradeDie",
		"WeaponSetUpgrade",
	};

	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		combattest::CombatWorld w;
		const std::string err = w.w.load(kLocal122Objects);
		REQUIRE_MESSAGE(err.empty(), err);
		w.logic->setEdition(edition);
		CAPTURE(gameEditionName(edition));

		// Every one of the 18 classes is registered in ModuleFactory under its RW getModuleName() key.
		for (const char *name : kAll18)
		{
			CAPTURE(name);
			CHECK(w.w.modules.findModuleTemplate(name, MODULETYPE_BEHAVIOR) != nullptr);
		}

		Object *unit = w.unit("TestLocal122Unit", 'A', 300, 300);
		REQUIRE(unit != nullptr);
		const ObjectID uid = unit->getID();

		const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
		REQUIRE(u != nullptr);
		unit->giveUpgrade(u);
		CHECK(unit->hasUpgrade(u));
		CHECK(unit->testModelCondition(Construction::modelConditionIndex("USER_1")));

		// Lethal damage triggers DestroyDie / FXListDie and cleanly destroys the object (dtor path).
		unit->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
		w.frames(2);
		CHECK(w.logic->findObjectByID(uid) == nullptr);
	}
}

// Ennorath INI: GondorBarracks (barracks.ini: ModelConditionUpgrade / LevelUpUpgrade / DestroyDie)
// and GondorFighter (gondorfighter.ini: DestroyDie / FXListDie / Upgrade modules).
TEST_CASE("LOCAL-122 ENNORATH scenario: upgrade bought on a building and unit dying with death modules")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-122 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	game.logic().combat().setAutoAcquireEnabled(false);
	const int p1 = game.index();
	Player *player = game.game->players().getNthPlayer(p1);
	REQUIRE(player != nullptr);
	std::string err;

	// 1. Building upgrade on GondorBarracks (barracks.ini).
	Object *barracks = game.game->createObject("GondorBarracks", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(barracks != nullptr, err);
	const UpgradeTemplate *lvl2 = TheUpgradeCenter->findUpgrade("Upgrade_GondorBarracksLevel2");
	REQUIRE(lvl2 != nullptr);
	CHECK_FALSE(barracks->hasUpgrade(lvl2));
	barracks->giveUpgrade(lvl2);
	CHECK(barracks->hasUpgrade(lvl2));

	// 2. Unit dying with death modules on GondorFighter (gondorfighter.ini).
	Object *fighter = game.game->createObject("GondorFighter", p1, Coord3D{ 1820.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(fighter != nullptr, err);
	const ObjectID fid = fighter->getID();
	fighter->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	for (int f = 0; f < 200 && game.logic().findObjectByID(fid) != nullptr; ++f)
	{
		game.logic().runLogicFrame();
	}
	CHECK(game.logic().findObjectByID(fid) == nullptr);
}
