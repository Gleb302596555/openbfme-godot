// OpenBFME unit tests. GPL-3.0.
//
// LOCAL-159 (fork issue 926): logic-only test rigs own a ShroudManager after GameLogic (destroyed first),
// matching LiveGame (RW 0x62CE75) so MSG_DO_SPECIAL_POWER_AT_LOCATION reaches ActionManager::canDoSpecialPowerAtLocation
// (RW 0x76F881 -> RW 0x82DFB7) and its shroud status query (ShroudManager RW 0xB4D9A0 -> RW 0xB4FB20).
// Expectations come from RotWK 2.01:
//   * RW 0x82E913 / 0x82E91A: SPECIAL_SPELL_BOOK_HEAL (0x10) requires CELLSHROUD_CLEAR (0) at the cast point.
//   * RW 0xB4F5D0: ShroudManager::revealMapForPlayerPermanently makes every cell CELLSHROUD_CLEAR for that player.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SpellTestAI.h"

#include "Common/INI.h"
#include "Common/SpecialPower.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/System/ShroudManager.h"

#include <memory>
#include <string>

namespace
{
const char kLocal159Ini[] =
	"SpecialPower Local159ClearSpotPower\n"
	"  Enum = SPECIAL_SPELL_BOOK_HEAL\n"
	"  ReloadTime = 1\n"
	"  RadiusCursorRadius = 50\n"
	"End\n"
	"ObjectCreationList Local159OCL\n"
	"  CreateObject\n"
	"    ObjectNames = Local159Marker\n"
	"    Count = 1\n"
	"  End\n"
	"End\n"
	"Object Local159Caster\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = OCLSpecialPower ModuleTag_OCL\n"
	"    SpecialPowerTemplate = Local159ClearSpotPower\n"
	"    OCL = Local159OCL\n"
	"    CreateLocation = CREATE_AT_LOCATION\n"
	"  End\n"
	"End\n"
	"Object Local159Marker\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

struct Local159World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore ocls{ w.keys };
	ObjectCreationListStore *savedOcls = TheObjectCreationListStore;
	SpellCommands spells;
	GameLogicDispatch dispatch{ *logic };
	Object *caster = nullptr;

	Local159World()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &ocls;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(w.modules);
		spells.registerHandlers(dispatch);
		attachShroud();
		const std::string err = w.load(kLocal159Ini, INI_LOAD_OVERWRITE, "local159.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		caster = unit("Local159Caster", 'A', 200.0f, 200.0f);
		REQUIRE(caster);
	}
	~Local159World()
	{
		TheSpecialPowerStore = savedPowers;
		TheObjectCreationListStore = savedOcls;
	}

	void castAt(const Coord3D &where)
	{
		const SpecialPowerTemplate *p = powers.findSpecialPowerTemplate("Local159ClearSpotPower");
		REQUIRE(p);
		GameMessage m(MSG_DO_SPECIAL_POWER_AT_LOCATION, caster->getControllingPlayer()->getPlayerIndex());
		m.appendIntegerArgument((int)p->getID());
		m.appendLocationArgument(where);
		m.appendObjectIDArgument(INVALID_ID);
		m.appendIntegerArgument(0);
		m.appendObjectIDArgument(caster->getID());
		dispatch.dispatch(m);
	}

	OCLSpecialPower *powerModule() const
	{
		for (const std::unique_ptr<BehaviorModule> &m : caster->modules())
		{
			if (auto *ocl = dynamic_cast<OCLSpecialPower *>(m.get()))
			{
				return ocl;
			}
		}
		return nullptr;
	}
};
} // namespace

// RW 0x76F881 -> RW 0x82DFB7 -> RW 0x82E91A (FINAL_CLEAR) -> RW 0xB4D9A0 (ShroudManager::getStatusAt):
// with MoveWorld::attachShroud, a point cast into a shrouded spot does not throw and is refused; once revealed (RW 0xB4F5D0), it succeeds.
TEST_CASE("LOCAL-159 shroud rig: MoveWorld::attachShroud reaches ActionManager RW 0xB4D9A0 shroud check without throwing")
{
	Local159World f;
	REQUIRE(f.logic->shroud() != nullptr);
	OCLSpecialPower *mod = f.powerModule();
	REQUIRE(mod);
	const Coord3D spot{ 400.0f, 400.0f, 0.0f };
	CHECK(f.logic->shroud()->getStatusAt(f.playerIndex("Alice"), spot.x, spot.y) == CELLSHROUD_SHROUDED);
	CHECK_NOTHROW(f.castAt(spot));
	CHECK(f.spells.casts() == 0);
	CHECK(f.spells.refused() == 1);
	CHECK(mod->created().empty());

	f.shroud->revealMapForPlayerPermanently(f.playerIndex("Alice"));
	CHECK(f.logic->shroud()->getStatusAt(f.playerIndex("Alice"), spot.x, spot.y) == CELLSHROUD_CLEAR);
	CHECK_NOTHROW(f.castAt(spot));
	CHECK(f.spells.casts() == 1);
	CHECK(mod->created().size() == 1u);
}

// Adjacent behavior (RW 0x62CE75 / 0xB4D9A0 / 0x82E91A): spelltest::GameAI owns a ShroudManager over its 8000x9000 terrain;
// with revealAll = false, casting SpellBookHeal (0x10, FINAL_CLEAR) into shrouded terrain is refused without throwing and succeeds after reveal.
TEST_CASE("LOCAL-159 shroud rig: retail SpellTestAI attaches ShroudManager and enforces shroud visibility (RW 0xB4D9A0)")
{
	OPENBFME_REQUIRE_START(shared);
	auto ctx = shared->world->enterContext();
	TeamFactory teams;
	PlayerList players(shared->world->nameKeys(), shared->world->playerTemplates(), teams);
	SkirmishSetup setup;
	setup.players.push_back({ "p0", "FactionMen", true, 0, 0, 1 });
	setup.players.push_back({ "p1", "FactionMordor", false, 1, 0, 2 });
	REQUIRE(players.setupSkirmish(setup).empty());
	auto logic = std::make_unique<GameLogic>(shared->world->things(), shared->world->modules(), players, RandomAlgorithm::ZH_CarryChain);
	std::string err;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::load(*shared->mount->fs, logic->settings(), &err), err);
	auto dispatch = std::make_unique<GameLogicDispatch>(*logic);
	spelltest::GameAI ai;
	ai.attach(*shared, *logic, false);
	struct Cleanup { GameLogic &l; ~Cleanup() { l.reset(); } } cleanup{ *logic };
	SpellCommands spell;
	spell.registerHandlers(*dispatch);
	SpecialPowerModules::installScienceHooks(*logic);
	Player *p0 = players.findPlayerWithName("p0");
	REQUIRE(p0);
	REQUIRE(logic->shroud() != nullptr);
	const ThingTemplate *citT = shared->world->things().findTemplate("MenFortressCitadel");
	REQUIRE(citT);
	Object *citadel = logic->newObject(citT, p0->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(citadel);
	const Coord3D citPos{ 500.0f, 300.0f, 0.0f };
	citadel->setPosition(&citPos);
	citadel->friend_onBuildComplete();
	Object *book = SpecialPowerModules::createSpellBook(*logic, *p0);
	REQUIRE(book);
	const ScienceType st = shared->world->spellStores().sciences().getScienceFromInternalName("SCIENCE_Heal");
	REQUIRE(st != SCIENCE_INVALID);
	p0->science().addSciencePurchasePoints(100);
	GameMessage buy(MSG_PURCHASE_SCIENCE, p0->getPlayerIndex());
	buy.appendIntegerArgument(p0->getPlayerIndex());
	buy.appendIntegerArgument(st);
	dispatch->dispatch(buy);
	REQUIRE(p0->hasScience(st));
	const SpecialPowerTemplate *heal = shared->world->spellStores().specialPowers().findSpecialPowerTemplate("SpellBookHeal");
	REQUIRE(heal);
	const Coord3D spot{ 3000.0f, 3000.0f, 0.0f };
	auto castHeal = [&]() {
		GameMessage m(MSG_DO_SPECIAL_POWER_AT_LOCATION, p0->getPlayerIndex());
		m.appendIntegerArgument((int)heal->getID());
		m.appendLocationArgument(spot);
		m.appendObjectIDArgument(INVALID_ID);
		m.appendIntegerArgument(0);
		m.appendObjectIDArgument(book->getID());
		dispatch->dispatch(m);
	};
	CHECK(logic->shroud()->getStatusAt(p0->getPlayerIndex(), spot.x, spot.y) == CELLSHROUD_SHROUDED);
	CHECK_NOTHROW(castHeal());
	CHECK(spell.casts() == 0);
	CHECK(spell.refused() == 1);

	ai.shroud->revealMapForPlayerPermanently(p0->getPlayerIndex());
	CHECK(logic->shroud()->getStatusAt(p0->getPlayerIndex(), spot.x, spot.y) == CELLSHROUD_CLEAR);
	CHECK_NOTHROW(castHeal());
	CHECK(spell.casts() == 1);
}
