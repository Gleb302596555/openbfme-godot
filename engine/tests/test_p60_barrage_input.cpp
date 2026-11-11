// OpenBFME. GPL-3.0.
//
// P-60 (docs/ennorath/PROBLEMS.md): the archers' Barrage (Command_ArcherBombard: Command = FIRE_WEAPON, WeaponSlot = TERTIARY,
// Options = NEED_TARGET_POS, RadiusCursorType = ScatterAreaRadiusCursor) through the input path the player uses: the control bar's
// press, the pointer over the ground, the left click. PORT-115's tests send MSG_DO_WEAPON_AT_LOCATION themselves and never pass the
// input; this file does. Retail 2.01 data (the same button, command set and InGameUI.ini rows as Ennorath's). Skips without
// ROTWK_INSTALL / BFME2_INSTALL.
//
// RotWK 2.01, InGameUI::createCommandHint (RW 0x6A3B77), the GUI command mode (RW 0x6A3CEC): a waiting command that is neither a
// context command (RW 0x75CD15) nor SPECIAL_POWER 0x18 / SPELL_BOOK 0x26 / SPECIAL_POWER_FROM_COMMAND_CENTER 0x20 / PLACE_BEACON 0x1F,
// with a target option (Options & 0x227) and a RadiusCursorType (button + 0x4C): the pointer is cursor 0 (None, RW 0x69BD53) and
// setRadiusCursor(type, power + 0x44, WeaponSlot + 0x80, valid = 1) (RW 0x6A3D65 .. 0x6A3DD9). setRadiusCursor (RW 0x6A077A): a type
// that is InGameUI.ini's RadiusCursorUseWeaponScatterRadius (InGameUI + 0x89C) takes its radius from the weapon of that slot on the
// first selected object (RW 0x6C81B0): ScatterRadius (+ 0x34) + ScatterTargetScalar (+ 0x38) (RW 0x6A0851 .. 0x6A086E).
#include "HudTestUtil.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/InGameHud.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/Weapon.h"

#include <cmath>

using namespace hudtest;

namespace
{
struct BarrageRig
{
	Rig rig;
	RecordingShellServices services;
	std::unique_ptr<InGameHud> hud;

	explicit BarrageRig(SharedWorld &s) : rig(s)
	{
		InGameHud::Config cfg{ *rig.game, *s.world, *s.mount->fs, services, rig.view, s.mouse, s.meta, nullptr };
		hud = std::make_unique<InGameHud>(cfg);
		std::string error;
		REQUIRE_MESSAGE(hud->boot(&error), error);
		rig.game->shroud().revealMapForPlayerPermanently(rig.index());
	}
	~BarrageRig() { hud.reset(); }
	void update() { hud->update(0.034); }
	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			update();
			rig.game->advance(0.2);
		}
	}
	ControlBarButton select(Object &o, const char *command)
	{
		hud->input().ui().deselectAll(false);
		hud->input().ui().selectObject(o.getID());
		// the player's click on the unit also sends the selected group to the logic (MSG_CREATE_SELECTED_GROUP, InGameUI's click path): the
		// logic's weapon commands act on that group (AICommands::selection), not on the HUD's list
		GameMessage group(MSG_CREATE_SELECTED_GROUP, rig.index());
		group.appendBooleanArgument(true);
		group.appendObjectIDArgument(o.getID());
		rig.game->dispatch().dispatch(group);
		update();
		for (const auto *buttons : { &hud->controlBar().palantirButtons(), &hud->controlBar().sideButtons() })
			for (const auto &b : *buttons)
				if (b.button && b.button->m_name == command) return b;
		return {};
	}
	ICoord2D aim(const Coord3D &where)
	{
		rig.lookAt(where);
		const ICoord2D pixel = rig.screenOf(where);
		hud->mouseMove(pixel.x, pixel.y);
		update();
		return pixel;
	}
	void click(HudInput::Button button, const ICoord2D &p)
	{
		hud->mouseButton(button, true, p.x, p.y, 0, rig.timeMs += 500);
		hud->mouseButton(button, false, p.x, p.y, 0, rig.timeMs += 40);
		update();
	}
	std::vector<GameMessage> ofType(int type) const
	{
		std::vector<GameMessage> result;
		for (const auto &m : rig.game->commands().messages())
			if (m.getType() == type) result.push_back(m);
		return result;
	}
};

std::vector<Object *> membersOf(Object &horde)
{
	std::vector<Object *> out;
	ContainModuleInterface *c = horde.getContain();
	const ContainModuleInterface::ContainedItemsList *list = c ? c->getContainedItemsList() : nullptr;
	if (list)
	{
		for (Object *m : *list)
		{
			out.push_back(m);
		}
	}
	return out;
}
} // namespace

TEST_CASE("P-60: the Barrage button of a retail Gondor archer horde waits for its target with the weapon scatter radius cursor "
		  "(RW 0x6A3D65 .. 0x6A3DD9, RW 0x6A0851 .. 0x6A086E) and the left click on the ground sends MSG_DO_WEAPON_AT_LOCATION, not a move")
{
	if (!haveWorld("P-60 barrage input")) return;
	BarrageRig h(shared());
	auto context = h.hud->enterContext();
	const Coord3D home = h.rig.freeSpot(1800, 1400, 300.0f);
	Object *horde = h.rig.make("GondorArcherHorde", home.x, home.y);
	REQUIRE(horde);
	h.frames(12); // the payload
	REQUIRE(!membersOf(*horde).empty());
	const auto b = h.select(*horde, "Command_ArcherBombard");
	REQUIRE(b.button);
	REQUIRE(b.state == ButtonState::Enabled);
	REQUIRE(b.button->m_command == GUI_COMMAND_FIRE_WEAPON);
	REQUIRE(b.button->m_radiusCursor == "ScatterAreaRadiusCursor");
	// InGameUI.ini: RadiusCursorUseWeaponScatterRadius = ScatterAreaRadiusCursor
	CHECK(h.hud->radiusCursorTemplates().usesWeaponScatterRadius("ScatterAreaRadiusCursor"));
	CHECK_FALSE(h.hud->radiusCursorTemplates().usesWeaponScatterRadius("LeapRadiusCursor"));
	const RadiusDecalTemplate *tmpl = h.hud->radiusCursorTemplates().find("ScatterAreaRadiusCursor");
	REQUIRE(tmpl);

	const Coord3D target{ home.x + 220, home.y, h.rig.logic().getGroundHeight(home.x + 220, home.y) };
	const auto pixel = h.aim(target);
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	h.update();
	CHECK(h.hud->input().ui().getGUICommand() == b.button);

	// the radius: the weapon of the button's slot on the first selected object, ScatterRadius + ScatterTargetScalar
	Object *source = h.rig.logic().findObjectByID(h.hud->input().ui().firstSelected());
	REQUIRE(source);
	REQUIRE(source->getWeapons());
	const Weapon *weapon = source->getWeapons()->weaponInSlot(b.button->m_weaponSlot);
	REQUIRE(weapon);
	const float radius = weapon->getTemplate()->m_scatterTargetScalar + weapon->getTemplate()->m_scatterRadius;
	REQUIRE(radius > 0.0f); // tolerance: a guard only (a zero radius would make the size check below vacuous); the exact value is the weapon's own ScatterRadius + ScatterTargetScalar, checked through decal.size
	CHECK(h.hud->radiusCursor().type() == "ScatterAreaRadiusCursor");
	const auto &decal = h.hud->radiusCursor().decal();
	CHECK(decal.visible);
	CHECK(decal.texture == tmpl->texture);
	CHECK(decal.size == 2.0f * radius);
	CHECK(decal.valid); // RW 0x6A3D6C: always valid
	CHECK(h.hud->input().ui().cursor() == "None");

	// pressing it again leaves the mode (RW 0x940877 .. 0x94089C) and takes the decal away
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	h.update();
	CHECK(h.hud->input().ui().getGUICommand() == nullptr);
	CHECK_FALSE(h.hud->radiusCursor().active());

	// the click
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	h.update();
	const size_t before = h.ofType(MSG_DO_WEAPON_AT_LOCATION).size();
	const size_t movesBefore = h.ofType(MSG_DO_MOVETO).size();
	h.click(HudInput::Button::Left, pixel);
	const auto shots = h.ofType(MSG_DO_WEAPON_AT_LOCATION);
	REQUIRE(shots.size() == before + 1);
	CHECK(h.ofType(MSG_DO_MOVETO).size() == movesBefore); // the symptom: the archers were sent to the spot
	const GameMessage &m = shots.back();
	CHECK(m.getArgument(0)->integer == b.button->m_weaponSlot);
	Coord3D clicked;
	REQUIRE(h.rig.view.screenToTerrain(pixel, h.rig.logic(), clicked));
	CHECK(m.getArgument(1)->location.x == clicked.x);
	CHECK(m.getArgument(1)->location.y == clicked.y);
	CHECK(m.getArgument(2)->integer == b.button->m_maxShotsToFire);
	CHECK(h.hud->input().ui().getGUICommand() == nullptr);
	CHECK_FALSE(h.hud->radiusCursor().active());
	CHECK(h.hud->input().ui().firstSelected() == horde->getID());
}

TEST_CASE("P-60: after the Barrage click the retail Gondor archers shoot the spot from where they stand (the spot is inside the "
		  "bombard range): they do not walk onto it")
{
	if (!haveWorld("P-60 barrage volley")) return;
	BarrageRig h(shared());
	auto context = h.hud->enterContext();
	const Coord3D home = h.rig.freeSpot(1800, 1400, 300.0f);
	Object *horde = h.rig.make("GondorArcherHorde", home.x, home.y);
	REQUIRE(horde);
	h.frames(12);
	const std::vector<Object *> archers = membersOf(*horde);
	REQUIRE(!archers.empty());
	const auto b = h.select(*horde, "Command_ArcherBombard");
	REQUIRE(b.button);
	REQUIRE(b.state == ButtonState::Enabled);
	const Coord3D target{ home.x + 220, home.y, h.rig.logic().getGroundHeight(home.x + 220, home.y) };
	const auto pixel = h.aim(target);
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	h.update();
	h.click(HudInput::Button::Left, pixel);
	const Coord3D start = *horde->getPosition();
	unsigned long long fired = 0;
	for (int i = 0; i < 150 && fired == 0; ++i)
	{
		h.frames(1);
		for (Object *a : archers)
		{
			if (a->getWeapons())
			{
				fired += a->getWeapons()->stats().shotsFired;
			}
		}
	}
	CHECK(fired > 0); // tolerance: some archer shot within 150 frames (30 s); PORT-115 has the exact volley on synthetic data
	const float dx = horde->getPosition()->x - target.x, dy = horde->getPosition()->y - target.y;
	const float sx = start.x - target.x, sy = start.y - target.y;
	// tolerance: the horde may shuffle to face the spot, but it stays nearer its start than the spot (it walked onto the spot before)
	CHECK(std::sqrt(dx * dx + dy * dy) > 0.5f * std::sqrt(sx * sx + sy * sy));
}
