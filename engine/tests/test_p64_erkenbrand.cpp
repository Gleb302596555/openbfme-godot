// OpenBFME. GPL-3.0.
//
// P-64 (docs/ennorath/PROBLEMS.md, owner's check 2026-11-12): Erkenbrand's "Time to Strike" went into recharge without showing it on the button, and he did
// not charge. Ennorath 1.9.8.2 data: object/goodfaction/units/rohan/rohanerkenbrand.ini (RohanErkenbrand_Summoned, RohanErkenbrandSummonedCommandSet slot 3
// = Command_ErkenbrandChargeAttack); includes/commandbutton.inc:42935 Command_ErkenbrandChargeAttack (FIRE_WEAPON, PRIMARY, NEED_TARGET_ENEMY_OBJECT,
// EnableOnModelCondition = MOUNTED); weapon ErkenbrandChargeWeapon (CanFireWhileCharging, AttackRange 300, DelayBetweenShots 40000). Through the player's
// input: select, press the button, click the enemy. Skips without ROTWK_INSTALL / BFME2_INSTALL / OPENBFME_ENNORATH_INSTALL.
#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/InGameHud.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/WeaponState.h"

#include <cstdio>

using namespace hudtest;

namespace
{
struct ErkRig
{
	Rig rig;
	RecordingShellServices services;
	std::unique_ptr<InGameHud> hud;
	explicit ErkRig(SharedWorld &s) : rig(s)
	{
		InGameHud::Config cfg{ *rig.game, *s.world, *s.mount->fs, services, rig.view, s.mouse, s.meta, nullptr };
		hud = std::make_unique<InGameHud>(cfg);
		std::string error;
		REQUIRE_MESSAGE(hud->boot(&error), error);
		rig.game->shroud().revealMapForPlayerPermanently(rig.index());
	}
	~ErkRig() { hud.reset(); }
	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			hud->update(0.034);
			rig.game->advance(0.2);
		}
	}
	ControlBarButton select(Object &o, const char *command)
	{
		hud->input().ui().deselectAll(false);
		hud->input().ui().selectObject(o.getID());
		GameMessage group(MSG_CREATE_SELECTED_GROUP, rig.index());
		group.appendBooleanArgument(true);
		group.appendObjectIDArgument(o.getID());
		rig.game->dispatch().dispatch(group);
		hud->update(0.034);
		for (const auto *buttons : { &hud->controlBar().palantirButtons(), &hud->controlBar().sideButtons() })
			for (const auto &b : *buttons)
				if (b.button && b.button->m_name == command) return b;
		return {};
	}
};
bool charging(const Object *o) { return o->testModelCondition(CombatNames::modelCondition("CHARGING")); }
bool mounted(const Object *o) { return o->testModelCondition(CombatNames::modelCondition("MOUNTED")); }
} // namespace

TEST_CASE("P-64 ennorath: Erkenbrand's Time to Strike through the button charges (CHARGING) and the button then shows its recharge clock (RW 0x943215 .. 0x943243)")
{
	ennorathscenario::World world;
	if (!world.load("P-64 erkenbrand"))
	{
		return;
	}
	ErkRig h(world.shared);
	auto context = h.hud->enterContext();
	Player *enemy = h.rig.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);
	const Coord3D home = h.rig.freeSpot(1800, 1400, 500.0f);
	Object *erk = h.rig.make("RohanErkenbrand_Summoned", home.x, home.y);
	REQUIRE(erk);
	Object *foe = h.rig.make("MordorFighterHorde", home.x + 450.0f, home.y, enemy);
	REQUIRE(foe);
	h.frames(10);
	std::printf("P-64 erkenbrand mounted at start: %d\n", mounted(erk) ? 1 : 0);
	ControlBarButton b = h.select(*erk, "Command_ErkenbrandChargeAttack");
	REQUIRE(b.button);
	if (b.state != ButtonState::Enabled)
	{
		// the mod's unit starts on foot or mounted per its data; the button needs MOUNTED: mount through the mod's own toggle
		const ControlBarButton t = h.select(*erk, "Command_HeroesToggleMountedErkenbrand");
		REQUIRE(t.button);
		REQUIRE(h.hud->controlBar().pressButton(t.slot, t.inPalantir));
		h.frames(30);
		b = h.select(*erk, "Command_ErkenbrandChargeAttack");
	}
	std::printf("P-64 mounted=%d button state=%d\n", mounted(erk) ? 1 : 0, (int)b.state);
	REQUIRE(b.state == ButtonState::Enabled);
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	h.hud->update(0.034);
	const Coord3D at{ foe->getPosition()->x, foe->getPosition()->y, foe->getPosition()->z + 8.0f };
	h.rig.lookAt(at);
	const ICoord2D pixel = h.rig.screenOf(at);
	h.hud->mouseMove(pixel.x, pixel.y);
	h.hud->update(0.034);
	h.hud->mouseButton(HudInput::Button::Left, true, pixel.x, pixel.y, 0, h.rig.timeMs += 500);
	h.hud->mouseButton(HudInput::Button::Left, false, pixel.x, pixel.y, 0, h.rig.timeMs += 40);
	h.hud->update(0.034);
	int chargeFrame = -1;
	for (int i = 0; i < 150 && chargeFrame < 0; ++i)
	{
		h.frames(1);
		if (charging(erk))
		{
			chargeFrame = i;
		}
	}
	CHECK(chargeFrame >= 0); // tolerance: the charge may start on any frame of the loop; -1 is the symptom (no charge)
	h.frames(20);
	const ControlBarButton after = h.select(*erk, "Command_ErkenbrandChargeAttack");
	REQUIRE(after.button);
	CHECK(after.state == ButtonState::NotReady);
	// tolerance: the clock's fraction depends on the frame the shot left; a FIRE_WEAPON button without a clock has -1 (the symptom)
	CHECK(after.timer >= 0.0f);
	CHECK(after.timer < 1.0f); // tolerance: still reloading (DelayBetweenShots 40000 ms), so below 1
}
