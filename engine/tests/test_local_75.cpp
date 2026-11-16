// OpenBFME unit tests. LOCAL-75 (issue #537, Ennorath P-142 & P-145):
//   * P-145: ActionManager::canDoSpecialPowerAtObject (RW 0x82D925) is called unconditionally from
//     CommandTranslator::evaluateGUISpecialPower (RW 0x6A4260 -> 0x6A437A) and SpellCommands::castWithGroup
//     (RW 0x77097E -> 0x770B67 / 0x770BD3). RotWK checks SpecialPowerTemplate::m_objectFilter (RW 0x762977 /
//     0x7640C1 at 0x82D996..0x82D9D1) even when Flags omits NEEDS_OBJECT_FILTER, so Heldenkiller* (Elladan /
//     Elrohir), WeisheitderGelehrten, and HerzdesFeuers reject non-matching targets on both client and logic.
//   * P-142: Imladris library units (ImladrisLoremaster Tomes Ulmo/Yavanna/Aule — never Manve #527,
//     ImladrisLichtaltar, BruchtalGelehrter, BruchtalLichtbringerErde/Wasser/Feuer/Luft + hordes + combo hordes).
//
// GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/HudInput.h"
#include "GameClient/HudObjects.h"
#include "GameLogic/ActionManager.h"
#include "GameClient/InGameHud.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/System/ShroudManager.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{

const char kLocal75SynthData[] =
	"SpecialPower L75HeroKillerPower\n"
	"  Enum = SPECIAL_AT_VISIBLE_OBJECT\n"
	"  ReloadTime = 1000\n"
	"  ObjectFilter = ANY +HERO ENEMIES\n"
	"End\n"
	"SpecialPower L75TwinFilterPower\n"
	"  Enum = SPECIAL_AT_VISIBLE_OBJECT\n"
	"  ReloadTime = 1000\n"
	"  ObjectFilter = NONE +HERO -L75ExcludedBoss ENEMIES\n"
	"End\n"
	"SpecialPower L75GroundedPower\n"
	"  Enum = SPECIAL_AT_VISIBLE_GROUNDED_OBJECT\n"
	"  ReloadTime = 1000\n"
	"  ObjectFilter = ANY +STRUCTURE ENEMIES\n"
	"End\n"
	"SpecialPower L75AllTargetPower\n"
	"  Enum = SPECIAL_AT_VISIBLE_OBJECT\n"
	"  ReloadTime = 1000\n"
	"  ObjectFilter = ALL ENEMIES\n"
	"End\n"
	"Weapon L75KillerWeapon\n"
	"  RadiusDamageAffects = ENEMIES\n"
	"  AttackRange = 300.0\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 200\n"
	"  PreAttackDelay = 0\n"
	"  PreAttackType = PER_SHOT\n"
	"  FiringDuration = 100\n"
	"  DamageNugget\n"
	"    Damage = 100\n"
	"    Radius = 5.0\n"
	"    DelayTime = 0\n"
	"    DamageType = HERO\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"CommandButton Command_L75HeroKiller\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = L75HeroKillerPower\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  CursorName = EvilAbilityObj\n"
	"  InvalidCursorName = GenericInvalid\n"
	"End\n"
	"CommandButton Command_L75TwinFilter\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = L75TwinFilterPower\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  CursorName = EvilAbilityObj\n"
	"  InvalidCursorName = GenericInvalid\n"
	"End\n"
	"CommandButton Command_L75Grounded\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = L75GroundedPower\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  CursorName = Bombard\n"
	"  InvalidCursorName = GenericInvalid\n"
	"End\n"
	"CommandButton Command_L75NoShrub\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = L75AllTargetPower\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  CursorName = EvilAbilityObj\n"
	"  InvalidCursorName = GenericInvalid\n"
	"End\n"
	"CommandButton Command_L75ShrubOk\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = L75AllTargetPower\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT ALLOW_SHRUBBERY_TARGET\n"
	"  CursorName = EvilAbilityObj\n"
	"  InvalidCursorName = GenericInvalid\n"
	"End\n"
	"CommandButton Command_L75RockOk\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = L75AllTargetPower\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT ALLOW_ROCK_TARGET\n"
	"  CursorName = EvilAbilityObj\n"
	"  InvalidCursorName = GenericInvalid\n"
	"End\n"
	"CommandSet L75CasterSet\n"
	"  1 = Command_L75HeroKiller\n"
	"  2 = Command_L75TwinFilter\n"
	"  3 = Command_L75Grounded\n"
	"  4 = Command_L75NoShrub\n"
	"  5 = Command_L75ShrubOk\n"
	"  6 = Command_L75RockOk\n"
	"End\n"
	"Object L75Caster\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  CommandSet = L75CasterSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_HK\n"
	"    SpecialPowerTemplate = L75HeroKillerPower\n"
	"    StartsPaused = No\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_HKUpd\n"
	"    SpecialPowerTemplate = L75HeroKillerPower\n"
	"    SpecialWeapon = L75KillerWeapon\n"
	"    StartAbilityRange = 300.0\n"
	"    UnpackTime = 0\n"
	"    PreparationTime = 0\n"
	"    PackTime = 0\n"
	"    WhichSpecialWeapon = 1\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_TF\n"
	"    SpecialPowerTemplate = L75TwinFilterPower\n"
	"    StartsPaused = No\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_TFUpd\n"
	"    SpecialPowerTemplate = L75TwinFilterPower\n"
	"    SpecialWeapon = L75KillerWeapon\n"
	"    StartAbilityRange = 300.0\n"
	"    UnpackTime = 0\n"
	"    PreparationTime = 0\n"
	"    PackTime = 0\n"
	"    WhichSpecialWeapon = 2\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_GR\n"
	"    SpecialPowerTemplate = L75GroundedPower\n"
	"    StartsPaused = No\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_GRUpd\n"
	"    SpecialPowerTemplate = L75GroundedPower\n"
	"    SpecialWeapon = L75KillerWeapon\n"
	"    StartAbilityRange = 300.0\n"
	"    UnpackTime = 0\n"
	"    PreparationTime = 0\n"
	"    PackTime = 0\n"
	"    WhichSpecialWeapon = 3\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_AT\n"
	"    SpecialPowerTemplate = L75AllTargetPower\n"
	"    StartsPaused = No\n"
	"  End\n"
	"End\n"
	"Object L75Soldier\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object L75EnemyHero\n"
	"  KindOf = INFANTRY HERO SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object L75ExcludedBoss\n"
	"  KindOf = MONSTER HERO SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object L75Structure\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object L75Shrubbery\n"
	"  KindOf = SHRUBBERY IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object L75Rock\n"
	"  KindOf = ROCK IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n";

struct Local75SynthWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore commandsStore;
	CommandStore *savedCommands = TheCommandStore;
	SpellCommands spellCommands;

	Local75SynthWorld()
	{
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandsStore;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		spellCommands.registerHandlers(dispatcher);
		combat().setAutoAcquireEnabled(false);
		const std::string err = w.load(kLocal75SynthData, INI_LOAD_OVERWRITE, "local75.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}

	Object *makeAt(const char *tpl, Team *team, const Coord3D &pos)
	{
		Object *o = make(tpl, team);
		if (o) { Coord3D p = pos; o->setPosition(&p); }
		return o;
	}
	void step(int n = 1) { for (int i = 0; i < n; ++i) { logic->runLogicFrame(); } }
	~Local75SynthWorld()
	{
		TheCommandStore = savedCommands;
		TheSpecialPowerStore = savedPowers;
	}
};

struct Local75Hud
{
	hudtest::Rig rig;
	RecordingShellServices services;
	std::unique_ptr<InGameHud> hud;

	explicit Local75Hud(hudtest::SharedWorld &s, const char *faction = "FactionImladris")
		: rig(s, "map mp fall back 8p", faction)
	{
		InGameHud::Config cfg{ *rig.game, *s.world, *s.mount->fs, services, rig.view, s.mouse, s.meta, nullptr };
		hud = std::make_unique<InGameHud>(cfg);
		std::string err;
		REQUIRE_MESSAGE(hud->boot(&err), err);
		rig.game->shroud().revealMapForPlayerPermanently(rig.index());
	}
	~Local75Hud() { hud.reset(); }
	void update() { hud->update(0.034); }
	void step(int n = 1)
	{
		for (int i = 0; i < n; ++i)
		{
			update();
			rig.game->advance(0.2);
		}
	}
	ControlBarButton select(Object &o, const char *buttonName)
	{
		hud->input().ui().deselectAll(false);
		hud->input().ui().selectObject(o.getID());
		GameMessage grp(MSG_CREATE_SELECTED_GROUP, rig.index());
		grp.appendBooleanArgument(true);
		grp.appendObjectIDArgument(o.getID());
		rig.game->dispatch().dispatch(grp);
		update();
		for (const auto *list : { &hud->controlBar().palantirButtons(), &hud->controlBar().sideButtons(), &hud->controlBar().offBarButtons() })
		{
			for (const auto &b : *list)
			{
				if (b.button && b.button->m_name == buttonName)
				{
					return b;
				}
			}
		}
		return {};
	}
	ICoord2D aim(const Coord3D &where)
	{
		rig.lookAt(where);
		const ICoord2D px = rig.screenOf(where);
		hud->mouseMove(px.x - 1, px.y);
		update();
		hud->mouseMove(px.x, px.y);
		update();
		return px;
	}
	void leftClick(const ICoord2D &p)
	{
		hud->mouseButton(HudInput::Button::Left, true, p.x, p.y, 0, rig.timeMs += 500);
		update();
		hud->mouseButton(HudInput::Button::Left, false, p.x, p.y, 0, rig.timeMs += 40);
		update();
	}
};

void sendDirectAtObject(GameLogicDispatch &dispatch, int playerIndex, const SpecialPowerTemplate *sp, Object *target, unsigned options)
{
	REQUIRE(sp != nullptr);
	REQUIRE(target != nullptr);
	GameMessage msg(MSG_DO_SPECIAL_POWER_AT_OBJECT, playerIndex);
	msg.appendIntegerArgument((int)sp->getID());
	msg.appendObjectIDArgument(target->getID());
	msg.appendIntegerArgument((int)options);
	msg.appendObjectIDArgument(INVALID_ID);
	msg.appendLocationArgument(*target->getPosition());
	dispatch.dispatch(msg);
}

bool hasAttrList(const Object &o, const char *name)
{
	const auto *pool = dynamic_cast<const AttributeModifierPool *>(o.attributeModifierPoolModule());
	return pool && pool->hasList(name);
}

ICoord2D aimAtTarget(Local75Hud &h, const CommandButton &button, Object &target)
{
	for (float dz : { 9.0f, 6.0f, 12.0f, 3.0f, 15.0f })
	{
		const Coord3D where{ target.getPosition()->x, target.getPosition()->y, target.getPosition()->z + dz };
		const ICoord2D px = h.aim(where);
		if (HudObjects::pickForGuiCommand(h.hud->input().context(), px, button) == &target)
		{
			return px;
		}
	}
	return h.aim({ target.getPosition()->x, target.getPosition()->y, target.getPosition()->z + 9.0f });
}

bool pressTargetedExpectRejected(Local75Hud &h, Object &caster, const char *buttonName, Object &target)
{
	const ControlBarButton b = h.select(caster, buttonName);
	REQUIRE_MESSAGE(b.button != nullptr, buttonName);
	REQUIRE_MESSAGE(b.state == ButtonState::Enabled, buttonName);
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	REQUIRE(h.hud->input().ui().getGUICommand() == b.button);
	const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
	const ICoord2D px = aimAtTarget(h, *b.button, target);
	if (HudObjects::pickForGuiCommand(h.hud->input().context(), px, *b.button) != &target)
	{
		h.hud->input().ui().setGUICommand(nullptr);
		CHECK(HudObjects::pickForGuiCommand(h.hud->input().context(), px, *b.button) == &target);
		return false;
	}
	const bool invalidCursor = h.hud->input().ui().cursor() == MouseCursorName::GenericInvalid;
	h.leftClick(px);
	h.step(2);
	const bool stillPending = h.hud->input().ui().getGUICommand() == b.button;
	const bool noCast = h.rig.game->spellCommands().casts() == castsBefore;
	h.hud->input().ui().setGUICommand(nullptr);
	return invalidCursor && stillPending && noCast;
}

bool pressTargetedExpectAccepted(Local75Hud &h, Object &caster, const char *buttonName, Object &target)
{
	const ControlBarButton b = h.select(caster, buttonName);
	REQUIRE_MESSAGE(b.button != nullptr, buttonName);
	REQUIRE_MESSAGE(b.state == ButtonState::Enabled, buttonName);
	REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
	REQUIRE(h.hud->input().ui().getGUICommand() == b.button);
	const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
	const ICoord2D px = aimAtTarget(h, *b.button, target);
	if (HudObjects::pickForGuiCommand(h.hud->input().context(), px, *b.button) != &target)
	{
		h.hud->input().ui().setGUICommand(nullptr);
		CHECK(HudObjects::pickForGuiCommand(h.hud->input().context(), px, *b.button) == &target);
		return false;
	}
	const bool notInvalidCursor = h.hud->input().ui().cursor() != MouseCursorName::GenericInvalid;
	h.leftClick(px);
	h.step(2);
	const bool cleared = h.hud->input().ui().getGUICommand() == nullptr;
	const bool casted = h.rig.game->spellCommands().casts() > castsBefore;
	return notInvalidCursor && cleared && casted;
}

} // namespace

TEST_CASE("LOCAL-75 P-145 synthetic: canDoSpecialPowerAtObject ObjectFilter blocks non-heroes at client targeting and SpellCommands (RW 0x6A437A, 0x770B67, 0x82D925)")
{
	Local75SynthWorld w;
	Player *alice = w.playerOf('A');
	Player *bob = w.playerOf('B');
	w.players.setLocalPlayer(alice);

	Object *caster = w.makeAt("L75Caster", alice->getDefaultTeam(), { 400.0f, 400.0f, 0.0f });
	Object *soldier = w.makeAt("L75Soldier", bob->getDefaultTeam(), { 460.0f, 400.0f, 0.0f });
	Object *boss = w.makeAt("L75ExcludedBoss", bob->getDefaultTeam(), { 400.0f, 460.0f, 0.0f });
	Object *hero = w.makeAt("L75EnemyHero", bob->getDefaultTeam(), { 460.0f, 460.0f, 0.0f });
	Object *building = w.makeAt("L75Structure", bob->getDefaultTeam(), { 340.0f, 400.0f, 0.0f });
	REQUIRE(caster);
	REQUIRE(soldier);
	REQUIRE(boss);
	REQUIRE(hero);
	REQUIRE(building);
	Object *shrub = w.makeAt("L75Shrubbery", bob->getDefaultTeam(), { 340.0f, 460.0f, 0.0f });
	Object *rock = w.makeAt("L75Rock", bob->getDefaultTeam(), { 340.0f, 340.0f, 0.0f });
	REQUIRE(shrub);
	REQUIRE(rock);

	alice->selection() = { caster->getID() };
	w.step(2);

	const CommandButton *cbHK = w.commandsStore.findCommandButton("Command_L75HeroKiller");
	const CommandButton *cbTF = w.commandsStore.findCommandButton("Command_L75TwinFilter");
	const CommandButton *cbGR = w.commandsStore.findCommandButton("Command_L75Grounded");
	const CommandButton *cbNoShrub = w.commandsStore.findCommandButton("Command_L75NoShrub");
	const CommandButton *cbShrubOk = w.commandsStore.findCommandButton("Command_L75ShrubOk");
	const CommandButton *cbRockOk = w.commandsStore.findCommandButton("Command_L75RockOk");
	const SpecialPowerTemplate *spHK = w.powers.findSpecialPowerTemplate("L75HeroKillerPower");
	const SpecialPowerTemplate *spTF = w.powers.findSpecialPowerTemplate("L75TwinFilterPower");
	const SpecialPowerTemplate *spGR = w.powers.findSpecialPowerTemplate("L75GroundedPower");
	REQUIRE(cbHK);
	REQUIRE(cbTF);
	REQUIRE(cbGR);
	REQUIRE(cbNoShrub);
	REQUIRE(cbShrubOk);
	REQUIRE(cbRockOk);
	REQUIRE(spHK);
	REQUIRE(spTF);
	REQUIRE(spGR);

	// 1. Direct logic path (SpellCommands::castWithGroup -> RW 0x770B67 / 0x770BD3 -> 0x82D925):
	//    Non-hero target and excluded target must be refused even without NEEDS_OBJECT_FILTER.
	sendDirectAtObject(w.dispatcher, alice->getPlayerIndex(), spHK, soldier, cbHK->m_options);
	CHECK(w.spellCommands.refused() == 1);
	CHECK(w.spellCommands.casts() == 0);
	CHECK(soldier->getBodyModule()->getHealth() == 500.0f);

	sendDirectAtObject(w.dispatcher, alice->getPlayerIndex(), spTF, soldier, cbTF->m_options);
	CHECK(w.spellCommands.refused() == 2);
	sendDirectAtObject(w.dispatcher, alice->getPlayerIndex(), spTF, boss, cbTF->m_options);
	CHECK(w.spellCommands.refused() == 3);
	CHECK(w.spellCommands.casts() == 0);

	// Grounded sibling: airborne target refused, grounded structure accepted.
	const int airborneIdx = ObjectTemplateInfoBuilder::objectStatusIndex("AIRBORNE_TARGET");
	REQUIRE(airborneIdx >= 0); // tolerance: status bit index is assigned dynamically from loaded INI names
	building->setStatus((unsigned)airborneIdx, true);
	sendDirectAtObject(w.dispatcher, alice->getPlayerIndex(), spGR, building, cbGR->m_options);
	CHECK(w.spellCommands.refused() == 4);
	CHECK(w.spellCommands.casts() == 0);
	building->setStatus((unsigned)airborneIdx, false);

	// 2. Client HUD targeting path (CommandTranslator::evaluateGUISpecialPower -> RW 0x6A437A -> 0x82D925):
	PinholeView view({ 400.0f, 0.0f, 500.0f }, { 400.0f, 400.0f, 0.0f }, 1024, 768, 0.7f);
	MouseSettings mouse;
	MetaMap meta;
	HudInput hud(*w.logic, w.ai.get(), view, w.commands, mouse, meta);
	hud.ui().mirrorSelection({ caster->getID() });

	auto tryClick = [&](const CommandButton *cb, Object &target, bool expectAccept) {
		hud.ui().setGUICommand(cb);
		ICoord2D px{};
		REQUIRE(view.worldToScreen(*target.getPosition(), px));
		hud.framePointer(px.x, px.y, 0);
		hud.update();
		hud.mouseMove(px.x, px.y, 0);
		hud.update();
		if (expectAccept)
		{
			CHECK(hud.ui().cursor() != MouseCursorName::GenericInvalid);
		}
		else
		{
			CHECK(hud.ui().cursor() == MouseCursorName::GenericInvalid);
		}
		const size_t msgsBefore = w.commands.messages().size();
		hud.mouseButton(HudInput::Button::Left, true, px.x, px.y, 0, 1000);
		hud.mouseButton(HudInput::Button::Left, false, px.x, px.y, 0, 1040);
		hud.flush();
		if (expectAccept)
		{
			CHECK(w.commands.messages().size() == msgsBefore + 1);
			CHECK(hud.ui().getGUICommand() == nullptr);
		}
		else
		{
			CHECK(w.commands.messages().size() == msgsBefore);
			CHECK(hud.ui().getGUICommand() == cb);
		}
		hud.ui().setGUICommand(nullptr);
	};

	// Invalid targets refused on client HUD cursor + zero messages sent
	tryClick(cbHK, *soldier, false);
	tryClick(cbTF, *soldier, false);
	tryClick(cbTF, *boss, false);
	building->setStatus((unsigned)airborneIdx, true);
	tryClick(cbGR, *building, false);
	building->setStatus((unsigned)airborneIdx, false);
	tryClick(cbGR, *building, true);

	// Valid target accepted on client HUD cursor + emitted message dispatches cleanly in logic
	const size_t msgsBeforeCast = w.commands.messages().size();
	tryClick(cbHK, *hero, true);
	REQUIRE(w.commands.messages().size() == msgsBeforeCast + 1);
	w.step(5);
	CHECK(w.spellCommands.casts() == 2);
	CHECK(hero->getBodyModule()->getHealth() == 400.0f);
}

TEST_CASE("LOCAL-75 P-145 Ennorath: Heldenkiller*, WeisheitderGelehrten, and HerzdesFeuers reject non-matching targets")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-75 P-145 Ennorath"))
	{
		return;
	}

	Local75Hud h(world.shared, "FactionImladris");
	auto context = h.hud->enterContext();
	Player *enemy = h.rig.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);

	const Coord3D home = h.rig.freeSpot(1800.0f, 1400.0f, 350.0f, false,
		{ { -90.0f, 0.0f }, { 340.0f, 0.0f }, { 340.0f, 90.0f }, { 220.0f, -60.0f }, { 440.0f, -90.0f }, { 280.0f, -180.0f } }, nullptr, 500.0f);
	const Coord3D camp = h.rig.freeSpot(1000.0f, 1400.0f, 300.0f, false,
		{ { -180.0f, 0.0f }, { -180.0f, -80.0f }, { -100.0f, -80.0f }, { -100.0f, 100.0f }, { -20.0f, 100.0f } }, nullptr, 300.0f);
	Object *twins = h.rig.make("ImladrisZwillingeEdain", home.x, home.y, h.rig.local);
	Object *elrohir = h.rig.make("ImladrisZwillinge_Elrohir", home.x - 90.0f, home.y, h.rig.local);
	Object *scholar = h.rig.make("BruchtalGelehrter", camp.x - 180.0f, camp.y, h.rig.local);
	Object *unlistedAlly = h.rig.make("ImladrisPorter", camp.x - 180.0f, camp.y - 80.0f, h.rig.local);
	Object *earthMage = h.rig.make("BruchtalLichtbringerErde", camp.x - 100.0f, camp.y - 80.0f, h.rig.local);
	Object *combo = h.rig.make("BruchtalLichtbringerErdeFeuerComboHorde", camp.x - 100.0f, camp.y + 100.0f, h.rig.local);
	Object *swordAlly = h.rig.make("BruchtalSchwertmeister", camp.x - 20.0f, camp.y + 100.0f, h.rig.local);
	Object *enemySoldier = h.rig.make("GondorFighter", home.x + 340.0f, home.y, enemy);
	Object *enemyBalrog = h.rig.make("MordorBalrog", home.x + 340.0f, home.y + 90.0f, enemy);
	Object *enemyHero = h.rig.make("GondorBoromir", home.x + 220.0f, home.y - 60.0f, enemy);
	Object *enemyBldg = h.rig.make("GondorBarracks", home.x + 440.0f, home.y - 90.0f, enemy);
	REQUIRE(twins);
	REQUIRE(elrohir);
	REQUIRE(scholar);
	REQUIRE(unlistedAlly);
	REQUIRE(earthMage);
	REQUIRE(combo);
	REQUIRE(swordAlly);
	REQUIRE(enemySoldier);
	REQUIRE(enemyBalrog);
	REQUIRE(enemyHero);
	REQUIRE(enemyBldg);
	h.rig.local->addCompletedUpgrade("Upgrade_TechnologyNoldorArtThirdTome");

	for (Object *hero : { twins, elrohir })
	{
		if (ExperienceTracker *xp = hero->getExperienceTracker())
		{
			xp->gainExpForLevel(10, false);
		}
	}
	h.step(15);

	// Direct forged MSG_DO_SPECIAL_POWER_AT_OBJECT against enemySoldier must be refused for both twins powers
	{
		const SpecialPowerTemplate *spElladan = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityHeldenkillerElladan");
		const SpecialPowerTemplate *spElrohir = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityHeldenkiller");
		REQUIRE(spElladan);
		REQUIRE(spElrohir);

		const unsigned long long refBefore = h.rig.game->spellCommands().refused();
		h.select(*twins, "Command_SpecialAbilityHeldenkillerElladan");
		sendDirectAtObject(h.rig.game->dispatch(), h.rig.index(), spElladan, enemySoldier, 1u);
		CHECK(h.rig.game->spellCommands().refused() == refBefore + 1);
		sendDirectAtObject(h.rig.game->dispatch(), h.rig.index(), spElladan, enemyBalrog, 1u);
		CHECK(h.rig.game->spellCommands().refused() == refBefore + 2);

		h.select(*elrohir, "Command_SpecialAbilityHeldenkiller");
		sendDirectAtObject(h.rig.game->dispatch(), h.rig.index(), spElrohir, enemySoldier, 1u);
		CHECK(h.rig.game->spellCommands().refused() == refBefore + 3);
	}

	// Client targeting path: reject non-hero & excluded target, accept hero and deal damage
	const float boromirHpBefore = enemyHero->getBodyModule()->getHealth();
	CHECK(pressTargetedExpectRejected(h, *twins, "Command_SpecialAbilityHeldenkillerElladan", *enemySoldier));
	CHECK(pressTargetedExpectRejected(h, *twins, "Command_SpecialAbilityHeldenkillerElladan", *enemyBalrog));
	CHECK(pressTargetedExpectAccepted(h, *twins, "Command_SpecialAbilityHeldenkillerElladan", *enemyHero));
	h.step(30);
	CHECK(enemyHero->getBodyModule()->getHealth() < boromirHpBefore); // tolerance: Ennorath hero armor and regeneration determine residual health

	CHECK(pressTargetedExpectRejected(h, *elrohir, "Command_SpecialAbilityHeldenkiller", *enemySoldier));
	Object *enemyHero2 = h.rig.make("GondorBoromir", home.x + 280.0f, home.y - 180.0f, enemy);
	h.step(2);
	REQUIRE(enemyHero2 != nullptr);
	CHECK(pressTargetedExpectAccepted(h, *elrohir, "Command_SpecialAbilityHeldenkiller", *enemyHero2));

	// BruchtalGelehrter: Command_SpecialAbilityWeisheitderGelehrten rejects ImladrisLoremaster, accepts BruchtalLichtbringerErde
	CHECK(pressTargetedExpectRejected(h, *scholar, "Command_SpecialAbilityWeisheitderGelehrten", *unlistedAlly));
	CHECK(pressTargetedExpectAccepted(h, *scholar, "Command_SpecialAbilityWeisheitderGelehrten", *earthMage));

	// BruchtalLichtbringerErdeFeuerComboHorde: Command_SpecialAbilityHerzdesFeuers rejects ImladrisLoremaster, accepts BruchtalSchwertmeister
	CHECK(pressTargetedExpectRejected(h, *combo, "Command_SpecialAbilityHerzdesFeuers", *unlistedAlly));
	CHECK(pressTargetedExpectAccepted(h, *combo, "Command_SpecialAbilityHerzdesFeuers", *swordAlly));

	// BruchtalLichtbringerErde: Command_SpecialAbilityEarthMageSabotage (0x90) rejects GondorFighter, accepts GondorBarracks
	const Coord3D sabSpot = h.rig.freeSpot(home.x + 440.0f, home.y - 240.0f, 180.0f);
	Object *enemySoldier2 = h.rig.make("GondorFighter", sabSpot.x, sabSpot.y, enemy);
	h.step(2);
	REQUIRE(enemySoldier2 != nullptr);
	CHECK(pressTargetedExpectRejected(h, *earthMage, "Command_SpecialAbilityEarthMageSabotage", *enemySoldier2));
	CHECK(pressTargetedExpectAccepted(h, *earthMage, "Command_SpecialAbilityEarthMageSabotage", *enemyBldg));
}

TEST_CASE("LOCAL-75 P-142 Ennorath: Imladris library mages, BruchtalGelehrter, and Loremaster Tome choices (excluding Manve #527)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-75 P-142 Ennorath"))
	{
		return;
	}

	// 1. ImladrisLoremaster Tome choices: Ulmo (SecondTome), Yavanna (ThirdTome), Aule (FourTome).
	//    ManveChoice (FirstTome) is skipped intentionally (Local 71, #527).
	struct TomeCase
	{
		const char *button;
		const char *tomeUpgrade;
		const char *altarSet;
		const char *crystalUpgrade;
	};
	for (const TomeCase &tc : {
		TomeCase{ "Command_UpgradeLoremasterUlmoChoice", "Upgrade_TechnologyNoldorArtSecondTome", "ImladrisLichtaltarCommandSetUlmo", "Upgrade_TestBuilding_arnor" },
		TomeCase{ "Command_UpgradeLoremasterYavannaChoice", "Upgrade_TechnologyNoldorArtThirdTome", "ImladrisLichtaltarCommandSetYavanna", "Upgrade_TestBuilding_2" },
		TomeCase{ "Command_UpgradeLoremasterAlueChoice", "Upgrade_TechnologyNoldorArtFourTome", "ImladrisLichtaltarCommandSetAule", "Upgrade_FarmSetNew" } })
	{
		INFO(tc.button);
		Local75Hud h(world.shared, "FactionImladris");
		auto context = h.hud->enterContext();
		h.rig.local->depositMoney(5000);
		const Coord3D pos = h.rig.freeSpot(1600.0f, 1200.0f, 300.0f);
		Object *altar = h.rig.make("ImladrisLichtaltar", pos.x + 120.0f, pos.y, h.rig.local);
		Object *lm = h.rig.make("ImladrisLoremaster", pos.x, pos.y, h.rig.local);
		REQUIRE(altar);
		REQUIRE(lm);
		const ObjectID lmId = lm->getID();
		h.step(5);

		const ControlBarButton btn = h.select(*lm, tc.button);
		REQUIRE(btn.button != nullptr);
		REQUIRE(btn.state == ButtonState::Enabled);
		REQUIRE(h.hud->controlBar().pressButton(btn.slot, btn.inPalantir));

		const UpgradeTemplate *tome = world.shared.world->upgrades().findUpgrade(tc.tomeUpgrade);
		REQUIRE(tome != nullptr);
		bool gotTome = false;
		for (int f = 0; f < 40 && !gotTome; ++f)
		{
			h.step(1);
			gotTome = h.rig.local->hasUpgradeComplete(tome);
		}
		CHECK(gotTome);
		h.step(20); // DoCommandUpgrade -> SummonReplacementSpecialAbilityUpdate replaces Loremaster, and altar ObjectCreationUpgrade Delay = 500
		CHECK(h.rig.logic().findObjectByID(lmId) == nullptr);
		CHECK(altar->getCommandSetName() == tc.altarSet);
		const UpgradeTemplate *crys = world.shared.world->upgrades().findUpgrade(tc.crystalUpgrade);
		REQUIRE(crys != nullptr);
		CHECK(altar->hasUpgrade(crys));
	}

	// RandomChoice uses SummonReplacementSpecialAbilityUpdate -> LoremasterThingVariatonEgg (BuildVariations -> one of the 4 Tomes)
	{
		Local75Hud h(world.shared, "FactionImladris");
		auto context = h.hud->enterContext();
		const Coord3D pos = h.rig.freeSpot(1600.0f, 1200.0f, 300.0f);
		Object *altar = h.rig.make("ImladrisLichtaltar", pos.x + 120.0f, pos.y, h.rig.local);
		Object *lm = h.rig.make("ImladrisLoremaster", pos.x, pos.y, h.rig.local);
		REQUIRE(altar);
		REQUIRE(lm);
		const ObjectID lmId = lm->getID();
		h.step(5);

		const ControlBarButton btn = h.select(*lm, "Command_SpecialAbilityLoremasterRandomChoice");
		REQUIRE(btn.button != nullptr);
		REQUIRE(btn.state == ButtonState::Enabled);
		REQUIRE(h.hud->controlBar().pressButton(btn.slot, btn.inPalantir));
		h.step(30);

		CHECK(h.rig.logic().findObjectByID(lmId) == nullptr);
		bool anyTome = false;
		for (const char *tn : {
			"Upgrade_TechnologyNoldorArtFirstTome",
			"Upgrade_TechnologyNoldorArtSecondTome",
			"Upgrade_TechnologyNoldorArtThirdTome",
			"Upgrade_TechnologyNoldorArtFourTome" })
		{
			const UpgradeTemplate *ut = world.shared.world->upgrades().findUpgrade(tn);
			REQUIRE(ut != nullptr);
			anyTome = anyTome || h.rig.local->hasUpgradeComplete(ut);
		}
		CHECK(anyTome);
		CHECK(altar->getCommandSetName() != "ImladrisLichtaltarCommandSet");
	}

	// 2. BruchtalGelehrter & all 4 BruchtalLichtbringer* units/hordes
	Local75Hud h(world.shared, "FactionImladris");
	auto context = h.hud->enterContext();
	Player *enemy = h.rig.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);
	const Coord3D base = h.rig.freeSpot(1700.0f, 1500.0f, 400.0f);

	Object *scholar = h.rig.make("BruchtalGelehrter", base.x, base.y, h.rig.local);
	Object *twins = h.rig.make("ImladrisZwillingeEdain", base.x + 40.0f, base.y, h.rig.local);
	Object *earth = h.rig.make("BruchtalLichtbringerErde", base.x, base.y + 80.0f, h.rig.local);
	Object *water = h.rig.make("BruchtalLichtbringerWasser", base.x + 80.0f, base.y + 80.0f, h.rig.local);
	Object *fire = h.rig.make("BruchtalLichtbringerFeuer", base.x + 160.0f, base.y + 80.0f, h.rig.local);
	Object *air = h.rig.make("BruchtalLichtbringerLuft", base.x + 240.0f, base.y + 80.0f, h.rig.local);
	Object *enemyBldg = h.rig.make("GondorBarracks", base.x, base.y + 220.0f, enemy);
	REQUIRE(scholar);
	REQUIRE(twins);
	REQUIRE(earth);
	REQUIRE(water);
	REQUIRE(fire);
	REQUIRE(air);
	REQUIRE(enemyBldg);

	// Grant all 4 Tomes so each elemental mage switches command set and ObjectCreationUpgrade (Delay = 500ms) grants Upgrade_TestBuilding_arnor.
	for (const char *tomeName : {
		"Upgrade_TechnologyNoldorArtFirstTome",
		"Upgrade_TechnologyNoldorArtSecondTome",
		"Upgrade_TechnologyNoldorArtThirdTome",
		"Upgrade_TechnologyNoldorArtFourTome" })
	{
		h.rig.local->addCompletedUpgrade(tomeName);
	}
	h.step(10);

	// Scholar passive aura on nearby hero + hero mode + wisdom upgrade grant onto earth mage
	CHECK(hasAttrList(*twins, "ImladrisGelehrterAura"));
	{
		const ControlBarButton bHero = h.select(*scholar, "Command_Weisheitaneignen");
		REQUIRE(bHero.button != nullptr);
		REQUIRE(bHero.state == ButtonState::Enabled);
		const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
		REQUIRE(h.hud->controlBar().pressButton(bHero.slot, bHero.inPalantir));
		h.step(5);
		CHECK(h.rig.game->spellCommands().casts() == castsBefore + 1);
		CHECK(hasAttrList(*scholar, "WissenaneignenModifier"));
	}
	{
		CHECK(pressTargetedExpectAccepted(h, *scholar, "Command_SpecialAbilityWeisheitderGelehrten", *earth));
		h.step(10);
		const UpgradeTemplate *wisdom = world.shared.world->upgrades().findUpgrade("Upgrade_ImladrisGelehrtenWeisheit");
		REQUIRE(wisdom != nullptr);
		CHECK(earth->hasUpgrade(wisdom));
	}

	// Wasser des Bruinen heals damaged nearby ally
	{
		const float maxHp = earth->getBodyModule()->getMaxHealth();
		static_cast<ActiveBody *>(earth->getBodyModule())->internalChangeHealth(-maxHp * 0.5f);
		const float damagedHp = earth->getBodyModule()->getHealth();
		const ControlBarButton bHeal = h.select(*water, "Command_SpecialAbilityWasserdesBruinen");
		REQUIRE(bHeal.button != nullptr);
		REQUIRE(bHeal.state == ButtonState::Enabled);
		const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
		REQUIRE(h.hud->controlBar().pressButton(bHeal.slot, bHeal.inPalantir));
		h.step(15);
		CHECK(h.rig.game->spellCommands().casts() == castsBefore + 1);
		CHECK(earth->getBodyModule()->getHealth() > damagedHp); // tolerance: Ennorath WeisheitderGelehrten heal ticks depend on elapsed frames
	}

	// Earth Mage tier-2 Sabotage (CommandSetUpgrade + ObjectCreationUpgrade Delay = 500 -> Upgrade_TestBuilding_arnor)
	{
		CHECK(earth->getCommandSetName() == "EarthMageYavannaCommandSet");
		CHECK(pressTargetedExpectAccepted(h, *earth, "Command_SpecialAbilityEarthMageSabotage", *enemyBldg));
		h.step(15);
	}
	// Earth Mage Bombard (FIRE_WEAPON PRIMARY)
	{
		const ControlBarButton bBomb = h.select(*earth, "Command_BombardImladrisLichtbringerErde");
		REQUIRE(bBomb.button != nullptr);
		REQUIRE(bBomb.state == ButtonState::Enabled);
		REQUIRE(h.hud->controlBar().pressButton(bBomb.slot, bBomb.inPalantir));
		const ICoord2D px = h.aim(*enemyBldg->getPosition());
		h.leftClick(px);
		h.step(10);
		CHECK(h.hud->input().ui().getGUICommand() == nullptr);
	}

	// Water Mage tier-2 Flood
	{
		CHECK(water->getCommandSetName() == "ImladrisWaterMageUlmoCommandSet");
		const ControlBarButton bFlood = h.select(*water, "Command_SpecialAbilityWaterMageFlood");
		REQUIRE(bFlood.button != nullptr);
		REQUIRE(bFlood.state == ButtonState::Enabled);
		const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
		REQUIRE(h.hud->controlBar().pressButton(bFlood.slot, bFlood.inPalantir));
		const ICoord2D px = h.aim(*enemyBldg->getPosition());
		h.leftClick(px);
		h.step(10);
		CHECK(h.rig.game->spellCommands().casts() == castsBefore + 1);
	}

	// Fire Mage tier-2 FireRing
	{
		CHECK(fire->getCommandSetName() == "FireMageAuleCommandSet");
		const ControlBarButton bFire = h.select(*fire, "Command_SpecialAbilityFireMageFlood");
		REQUIRE(bFire.button != nullptr);
		REQUIRE(bFire.state == ButtonState::Enabled);
		const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
		REQUIRE(h.hud->controlBar().pressButton(bFire.slot, bFire.inPalantir));
		const ICoord2D px = h.aim(*enemyBldg->getPosition());
		h.leftClick(px);
		h.step(10);
		CHECK(h.rig.game->spellCommands().casts() == castsBefore + 1);
	}

	// Air Mage tier-2 WindMageTeleport (TeleportSpecialAbilityUpdate, SPECIAL_BALROG_WINGS)
	{
		CHECK(air->getCommandSetName() == "ImladrisAirMageManveCommandSet");
		const Coord3D startPos = *air->getPosition();
		const Coord3D destPos{ startPos.x + 100.0f, startPos.y, startPos.z };
		const ControlBarButton bTele = h.select(*air, "Command_SpecialAbilityWindMageTeleport");
		REQUIRE(bTele.button != nullptr);
		REQUIRE(bTele.state == ButtonState::Enabled);
		const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
		REQUIRE(h.hud->controlBar().pressButton(bTele.slot, bTele.inPalantir));
		const ICoord2D px = h.aim(destPos);
		h.leftClick(px);
		h.step(20);
		CHECK(h.rig.game->spellCommands().casts() == castsBefore + 1);
		const Coord3D endPos = *air->getPosition();
		const float moved = std::hypot(endPos.x - startPos.x, endPos.y - startPos.y);
		CHECK(moved > 40.0f); // tolerance: WindMageTeleport destination snaps to valid nearby terrain cell
	}

	// Combo hordes of library mages: Elbenwald (WE), WellenHoffnung (EF), Tornado (EL), FeuerderHchsten (FL), StillederSinne (WF), Flood (WL)
	const UpgradeTemplate *wisdomUpg = world.shared.world->upgrades().findUpgrade("Upgrade_ImladrisGelehrtenWeisheit");
	REQUIRE(wisdomUpg != nullptr);
	struct ComboCheck
	{
		const char *tpl;
		const char *btn;
	};
	const Coord3D comboOrigin = *enemyBldg->getPosition();
	for (const ComboCheck &cc : {
		ComboCheck{ "BruchtalLichtbringerWasserErdeComboHorde", "Command_SpecialAbilityLichtbringerElbenwald" },
		ComboCheck{ "BruchtalLichtbringerErdeFeuerComboHorde", "Command_LichtbringerWellenHoffnung" },
		ComboCheck{ "BruchtalLichtbringerErdeLuftComboHorde", "Command_LichtbringerTornado" },
		ComboCheck{ "BruchtalLichtbringerFeuerLuftComboHorde", "Command_LichtbringerFeuerderHchsten" },
		ComboCheck{ "BruchtalLichtbringerWasserFeuerComboHorde", "Command_SpecialAbilityStillederSinne" },
		ComboCheck{ "BruchtalLichtbringerWasserLuftComboHorde", "Command_LichtbringerFlood" } })
	{
		INFO(std::string(cc.tpl) << " / " << std::string(cc.btn));
		const Coord3D spot = h.rig.freeSpot(comboOrigin.x - 140.0f, comboOrigin.y, 80.0f);
		Object *ch = h.rig.make(cc.tpl, spot.x, spot.y, h.rig.local);
		REQUIRE(ch != nullptr);
		const ObjectID chId = ch->getID();
		h.step(5);
		REQUIRE(h.rig.logic().findObjectByID(chId) != nullptr);
		ch->giveUpgrade(wisdomUpg);
		h.step(2);
		const ControlBarButton b = h.select(*ch, cc.btn);
		REQUIRE(b.button != nullptr);
		REQUIRE(b.state == ButtonState::Enabled);
		const unsigned long long castsBefore = h.rig.game->spellCommands().casts();
		REQUIRE(h.hud->controlBar().pressButton(b.slot, b.inPalantir));
		const ICoord2D px = h.aim(comboOrigin);
		h.leftClick(px);
		h.step(15);
		CHECK(h.rig.game->spellCommands().casts() == castsBefore + 1);
		h.rig.logic().destroyObject(ch);
		h.step(2);
	}
}
