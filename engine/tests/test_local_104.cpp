// OpenBFME. GPL-3.0.
// LOCAL-104 (fork issue 775): stealth and detection uncited functions (editions: both RotWK 2.01 and BFME2 1.06).
// Sources:
//   - StealthUpdate::loadPostProcess: RW 0x00776E23 / DONOR BFME2 0x00374AE8 (StealthUpdateLoadPost.cpp)
//   - StealthUpdate::update disguise restore: RW 0x007779ED .. 0x00777A2F / DONOR BFME2 0x003756A8 (StealthUpdateLogic.cpp:1451)
//   - StealthUpdate::changeVisualDisguise clears +0x47: RW 0x007772FB / DONOR BFME2 StealthUpdateLogic.cpp:1216
//   - Ennorath INI: data/ini/object/goodfaction/hordes/elven/elvenhordes.ini (ElvenMirkwoodArcherHorde),
//     data/ini/object/evilfaction/structures/evilmen/evilmenharadtower.ini (HaradTower),
//     data/ini/object/evilfaction/units/angmar/angmardunkelelfarcher.ini (Dunkelelfen)

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/InvisibilityManager.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
const char kStealthObjects104[] =
	"Object TestSpy104\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = StealthUpdate StealthTag\n"
	"  StealthDelay = 0\n"
	"  DisguisesAsTeam = Yes\n"
	"  DisguiseTransitionTime = 200\n"
	"  StartsActive = Yes\n"
	" End\nEnd\n"
	"Object TestEnemy104\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\nEnd\n";

struct SpyClientHooks104 final : ObjectClientHooks
{
	struct ReplaceCall
	{
		ObjectID id = INVALID_ID;
		const ThingTemplate *tmpl = nullptr;
		bool hasColor = false;
		std::uint32_t color = 0;
	};
	std::vector<ReplaceCall> replaces;

	void objectCreated(Object &obj) override { obj.friend_bindToClient(this); }
	void objectDestroyed(Object &) override {}
	void replaceDrawable(Object &obj, const ThingTemplate *tmpl, bool hasColor, std::uint32_t color) override
	{
		replaces.push_back({ obj.getID(), tmpl, hasColor, color });
	}
};

struct StealthWorld104 : combattest::CombatWorld
{
	StealthWorld104()
	{
		const std::string err = w.load(kStealthObjects104);
		REQUIRE_MESSAGE(err.empty(), err);
	}
};
} // namespace

// RW 0x00776E23 / 0x007779ED / 0x007772FB (DONOR BFME2 0x00374AE8 / 0x003756A8):
// Loading a StealthUpdate with m_disguiseTemplate (+0x3C) != nullptr arms m_xferRestoreDisguise (+0x47),
// so the next StealthUpdate::update() calls swapDisguise() (replaceDrawable) once and clears +0x47.
// Loading an undisguised StealthUpdate (+0x3C == nullptr) does not arm +0x47 or call replaceDrawable.
TEST_CASE("LOCAL-104 StealthUpdate: loadPostProcess arms disguise drawable restore on next update (RW 0x00776E23)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		// 1. Disguised unit saved and loaded into a fresh world.
		std::vector<std::uint8_t> disguisedBytes;
		std::vector<std::uint8_t> plainBytes;
		{
			StealthWorld104 saved;
			saved.logic->setEdition(edition);
			Object *spy = saved.unit("TestSpy104", 'A', 300, 300);
			Object *plain = saved.unit("TestSpy104", 'A', 400, 300);
			Object *enemy = saved.unit("TestEnemy104", 'B', 500, 300);
			REQUIRE(spy != nullptr);
			REQUIRE(plain != nullptr);
			REQUIRE(enemy != nullptr);

			StealthUpdate *stSpy = StealthUpdate::of(*spy);
			StealthUpdate *stPlain = StealthUpdate::of(*plain);
			REQUIRE(stSpy != nullptr);
			REQUIRE(stPlain != nullptr);

			stSpy->disguiseAsObject(enemy);
			saved.frames(10);
			REQUIRE(stSpy->disguiseTemplate() == enemy->getTemplate());
			REQUIRE(stSpy->disguiseShown());
			REQUIRE(stPlain->disguiseTemplate() == nullptr);

			XferSave saveDisguised;
			stSpy->xfer(saveDisguised);
			disguisedBytes = saveDisguised.data();

			XferSave savePlain;
			stPlain->xfer(savePlain);
			plainBytes = savePlain.data();
		}

		// 2. Load into a fresh world with SpyClientHooks104 attached.
		StealthWorld104 loaded;
		loaded.logic->setEdition(edition);
		SpyClientHooks104 hooks;
		loaded.logic->setClientHooks(&hooks);

		Object *spyLoaded = loaded.unit("TestSpy104", 'A', 300, 300);
		Object *plainLoaded = loaded.unit("TestSpy104", 'A', 400, 300);
		Object *enemyRef = loaded.unit("TestEnemy104", 'B', 500, 300);
		REQUIRE(spyLoaded != nullptr);
		REQUIRE(plainLoaded != nullptr);
		REQUIRE(enemyRef != nullptr);

		StealthUpdate *stSpyLoaded = StealthUpdate::of(*spyLoaded);
		StealthUpdate *stPlainLoaded = StealthUpdate::of(*plainLoaded);
		REQUIRE(stSpyLoaded != nullptr);
		REQUIRE(stPlainLoaded != nullptr);

		loaded.frames(10); // match saved world's frame so restored wake frame fires on the next update
		{
			XferLoad r1(disguisedBytes);
			stSpyLoaded->xfer(r1);
			XferLoad r2(plainBytes);
			stPlainLoaded->xfer(r2);
		}
		REQUIRE(stSpyLoaded->disguiseTemplate() == enemyRef->getTemplate());
		REQUIRE(stPlainLoaded->disguiseTemplate() == nullptr);
		CHECK(hooks.replaces.empty());

		// First update after load restores the disguised unit's drawable once (RW 0x007779ED -> 0x00776F03);
		// the undisguised unit does not emit replaceDrawable.
		stSpyLoaded->update();
		stPlainLoaded->update();
		REQUIRE(hooks.replaces.size() == 1);
		CHECK(hooks.replaces[0].id == spyLoaded->getID());
		CHECK(hooks.replaces[0].tmpl == enemyRef->getTemplate());
		CHECK(stSpyLoaded->disguiseShown());

		// Subsequent updates do not re-emit replaceDrawable because +0x47 was cleared (RW 0x007772FB).
		stSpyLoaded->update();
		stPlainLoaded->update();
		CHECK(hooks.replaces.size() == 1);
		loaded.logic->setClientHooks(nullptr);
	}
}

// Ennorath INI: ElvenMirkwoodArcherHorde (elvenhordes.ini InvisibilityUpdate ModuleTag_ElvenCloak),
// HaradTower (evilmenharadtower.ini StealthDetectorUpdate with Upgrade_EvilmenStachelpfeile),
// Dunkelelfen (angmardunkelelfarcher.ini StealthUpdate).
TEST_CASE("LOCAL-104 ENNORATH scenario: ElvenMirkwoodArcherHorde cloak revealed by HaradTower detector and Dunkelelfen StealthUpdate")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-104 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionElves");
	const int p1 = game.index();
	Player *enemy = game.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);
	const int p2 = enemy->getPlayerIndex();
	std::string err;

	// 1. ElvenMirkwoodArcherHorde (InvisibilityUpdate, StartsActive = No): activating cloak makes it invisible.
	Object *archers = game.game->createObject("ElvenMirkwoodArcherHorde", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(archers != nullptr, err);
	InvisibilityUpdate *cloak = nullptr;
	for (const std::unique_ptr<BehaviorModule> &m : archers->modules())
	{
		if (auto *iu = dynamic_cast<InvisibilityUpdate *>(m.get()))
		{
			if (!iu->data()->m_startsActive)
			{
				cloak = iu;
				break;
			}
		}
	}
	REQUIRE(cloak != nullptr);
	cloak->setActive(true);
	for (int i = 0; i < 15; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) game.logic().update(phase);
	}
	CHECK(InvisibilityManager::clientLook(*archers, enemy) == 5); // invisible to enemy

	// 2. Enemy HaradTower (StealthDetectorUpdate, DetectionRange = 350, RequiredUpgrade = Upgrade_EvilmenStachelpfeile) reveals it.
	Object *tower = game.game->createObject("HaradTower", p2, Coord3D{ 1850.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(tower != nullptr, err);
	const UpgradeTemplate *spikes = TheUpgradeCenter->findUpgrade("Upgrade_EvilmenStachelpfeile");
	REQUIRE(spikes != nullptr);
	tower->giveUpgrade(spikes);
	for (int i = 0; i < 20; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) game.logic().update(phase);
	}
	CHECK(game.logic().invisibility().reveals() >= 1u); // tolerance: HaradTower DetectionRate=2000ms random initial delay can trigger 1-2 scans in 20 frames
	CHECK(InvisibilityManager::clientLook(*archers, enemy) != 5); // revealed to enemy (detected or stealth broken)

	// 3. Dunkelelfen (StealthUpdate, StealthDelay = 500, DetectedByAnyoneRange = 120): StealthUpdate is present and runs.
	Object *elf = game.game->createObject("Dunkelelfen", p1, Coord3D{ 2200.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(elf != nullptr, err);
	StealthUpdate *st = StealthUpdate::of(*elf);
	REQUIRE(st != nullptr);
	CHECK(st->data()->m_detectedByAnyoneRange == 120.0f);
}
