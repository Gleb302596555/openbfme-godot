// OpenBFME. LOCAL-126 tests (fork issue 810):
//   - CastleBehavior::unpack with unknown/empty faction side (RW 0x798F70 -> "kq" -> RW 0x72D72F: 0 objects, no error)
//   - W3DScriptedModelDraw::showSubObject without a model (RW 0x4C3B25 / RW 0x4BA74F: silent, queued op)
//   - Locomotor Z_RELATIVE_TO_HIGHEST_LAYER (RW 0x5E76AC case 7 -> RW 0x5E7A10..0x5E7ACC)
//   - ControlBar::isUnderConstructionContext WALL_UPGRADE + DAMAGE_MULT == 0.0 (RW 0x71EBDA / RW 0x71EE48 -> 0x68C82D)
//   - LiveScripting::objectAttributeModifier RESIST_FEAR (RW 0x7369AE / RW 0x736A09 -> 0x68C818) & objectDescription (RW 0x7366FE -> 0x69005F)
//   - Ennorath scenario for IsengardFortress, AgandaurLightSphere1/2/3 (BattleDroneLocomotor), ElvenLegolas / IsengardCitadelUnpack.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "IniTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "Common/ArchiveFileSystem.h"
#include "GameClient/ControlBar.h"
#include "GameClient/LiveScripting.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/LuaScriptEngine.h"

#include <cmath>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
class Local126LocoHost : public LocomotorHost
{
public:
	Coord3D pos{ 0.0f, 0.0f, 0.0f };
	Coord3D pending{ 0.0f, 0.0f, 0.0f };
	bool hasPending = false;
	float angle = 0.0f;
	float setSpeed = 100.0f;
	float groundZ = 0.0f;
	float highestZ = 0.0f;
	LocomotorMatrix transform{};

	void place(float x, float y, float z)
	{
		pos = Coord3D{ x, y, z };
		transform = LocomotorMatrix{};
		transform.m[0][0] = std::cos(angle);
		transform.m[0][1] = -std::sin(angle);
		transform.m[1][0] = std::sin(angle);
		transform.m[1][1] = std::cos(angle);
		transform.m[2][2] = 1.0f;
		transform.m[0][3] = x;
		transform.m[1][3] = y;
		transform.m[2][3] = z;
	}

	Coord3D getPosition() const override { return pos; }
	float getAngle() const override { return angle; }
	Coord2D getUnitDirectionVector2D() const override { return Coord2D{ std::cos(angle), std::sin(angle) }; }
	float getBoundingRadius() const override { return 5.0f; }
	LocomotorMatrix getTransform() const override { return transform; }
	void setTransform(const LocomotorMatrix &m) override { transform = m; }
	bool hasPendingPosition() const override { return hasPending; }
	Coord3D getPendingPosition() const override { return pending; }
	void setPendingPosition(const Coord3D &p) override { pending = p; hasPending = true; }
	float locomotorSetSpeed() const override { return setSpeed; }
	int damageState() const override { return 0; }
	int movementPenaltyDamageState() const override { return 2; }
	float crewPowerMultiplier() const override { return 1.0f; }
	bool speedAttributeModifier(float &) const override { return false; }
	bool isInRiver(float, float) const override { return false; }
	bool isTurnLimited() const override { return false; }
	bool isChargeOrdered() const override { return false; }
	bool physicsMotionDisabled() const override { return false; }
	bool hasPhysicsModule() const override { return false; }
	bool zMotionSuppressed() const override { return false; }
	unsigned logicFrame() const override { return 10; }
	bool containerAllowsBackingUp() const override { return false; }
	float groundHeightAt(float, float) const override { return groundZ; }
	float highestLayerHeightAt(float, float) const override { return highestZ; }
	LocomotorPath *getPath() override { return nullptr; }
	bool testObjectStatus(int) const override { return false; }
	bool testModelCondition(int) const override { return false; }
	void setModelCondition(int, bool) override {}
	void clearAndSetModelConditions(const std::vector<int> &, const std::vector<int> &) override {}
	void notifyWheelsStopped() override {}
	bool hasHordeContain() const override { return false; }
	void hordeBeginReform() override {}
	void hordeEndReform() override {}
	bool hordeFormationReady(float) const override { return true; }
	bool thingWaitsForFormation() const override { return false; }
	bool isUnderwater(float, float, float *) const override { return false; }
	bool isDisabledHeld() const override { return false; }
	bool isKindOfSubmarine() const override { return false; }
};
struct Local126ModWorld : CombatWorld
{
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	Local126ModWorld(const char *ini, const char *name)
	{
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		const std::string err = w.load(ini, INI_LOAD_OVERWRITE, name);
		REQUIRE_MESSAGE(err.empty(), err);
		frames(1);
	}
	~Local126ModWorld() { TheAttributeModifierStore = savedModifiers; }
};
} // namespace

// RW 0x798F70 -> nameToKey("kq") (RW 0xC30C08) -> RW 0x72D72F returns false:
// CastleBehavior::unpack when the owner's side is not listed in CastleToUnpackForFaction creates 0 objects and succeeds with no error.
TEST_CASE("LOCAL-126 CastleBehavior::unpack unknown faction creates 0 objects with no error (RW 0x798F70 / 0x79B903)")
{
	const char *kIni =
		"Object Local126Castle\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 30\n"
		"  GeometryHeight = 20\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"  End\n"
		"  Behavior = CastleBehavior ModuleTag_Castle\n"
		"    CastleToUnpackForFaction = Isengard IsengardCamp\n"
		"    UnpackDelayTime = 0.0\n"
		"  End\n"
		"End\n";

	CombatWorld cw;
	const std::string err = cw.w.load(kIni, INI_LOAD_OVERWRITE, "local126_castle.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	// Neutral player has empty side "" (not in CastleToUnpackForFaction).
	Player *neutral = cw.logic->players().getNeutralPlayer();
	REQUIRE(neutral != nullptr);
	CHECK(neutral->getSide().empty());

	const ThingTemplate *tt = cw.w.things.findTemplate("Local126Castle");
	REQUIRE(tt != nullptr);
	Object *castleObj = cw.logic->newObject(tt, neutral->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(castleObj != nullptr);
	const Coord3D pos{ 100.0f, 100.0f, 0.0f };
	castleObj->setPosition(&pos);
	auto *cb = dynamic_cast<CastleBehavior *>(castleObj->findModule("CastleBehavior"));
	REQUIRE(cb != nullptr);

	CHECK(cb->unpack(true));
	CHECK(cb->lastError().empty());
	CHECK(cb->ownedObjects().empty());
}

// RW 0x4C3B25 (showSubObject) and RW 0x4BA74F (RW 0x4BA75B..0x4BA76D):
// hiding/showing a sub-object before a model is bound records the op silently with no error.
TEST_CASE("LOCAL-126 W3DScriptedModelDraw hide/show sub-object without model emits no error (RW 0x4C3B25 / 0x4BA74F)")
{
	drawtest::Harness h;
	W3DModelDrawModuleData *data = h.parse("DefaultModelConditionState\n  Model = None\nEnd\nEnd\n");
	REQUIRE(data != nullptr);
	w3dsynth::MemoryFileSource mfs;
	WW3DAssetManager am(mfs);
	WW3DDrawAssets assets(am);
	W3DClientRandom rnd(RandomAlgorithm::RotWK_GameDat_LCG);
	W3DScriptedModelDraw d(*data, assets, rnd);
	d.hideSubObject("arrow");
	d.showSubObject("FELLOWSHIPBADGE");
	CHECK(d.errors().empty());
}

// RW 0x5E76AC case 7 -> RW 0x5E7A10..0x5E7ACC (DONOR BFME2 LocomotorRva001E7ECA.cpp:328):
// ZAxisBehavior = RELATIVE_TO_HIGHEST_LAYER damps z toward highestLayerHeightAt(x, y) + PreferredHeight
// with step = ((pos.z + (aim - pos.z) * damping) - pos.z) / (LOGICFRAMES_PER_SECOND * 2.0f).
TEST_CASE("LOCAL-126 Locomotor Z_RELATIVE_TO_HIGHEST_LAYER moves z toward highest layer + PreferredHeight (RW 0x5E7A10)")
{
	const char *kLocoIni =
		"Locomotor Local126SphereLoco\n"
		"  Surfaces = AIR\n"
		"  Speed = 100\n"
		"  Acceleration = 60\n"
		"  Braking = 10\n"
		"  MinTurnSpeed = 60\n"
		"  PreferredHeight = 15\n"
		"  PreferredHeightDamping = 0.5\n"
		"  AllowAirborneMotiveForce = Yes\n"
		"  ZAxisBehavior = RELATIVE_TO_HIGHEST_LAYER\n"
		"  Appearance = HOVER\n"
		"End\n";

	CombatWorld cw;
	const std::string err = cw.w.load(kLocoIni, INI_LOAD_OVERWRITE, "local126_loco.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	REQUIRE(TheLocomotorStore != nullptr);
	const LocomotorTemplate *lt = TheLocomotorStore->findLocomotorTemplate("Local126SphereLoco");
	REQUIRE(lt != nullptr);
	CHECK(lt->m_zAxisBehavior == Z_RELATIVE_TO_HIGHEST_LAYER);

	Locomotor loco(lt);
	Local126LocoHost host;
	host.place(10.0f, 10.0f, 5.0f);
	host.groundZ = 0.0f;
	host.highestZ = 20.0f; // elevated wall/platform above ground

	// aim = highestZ (20) + PreferredHeight (15) = 35
	// step = ((5 + (35 - 5) * 0.5) - 5) / (5 * 2) = 15 / 10 = 1.5
	// new matrix z = 5 + 1.5 = 6.5; pending z = 2 * 6.5 - 5 = 8.0
	CHECK_NOTHROW(loco.locomotorMoveTowardsPosition(host, Coord3D{ 50.0f, 10.0f, 35.0f }, 40.0f, 10.0f));
	CHECK(host.hasPending);
	CHECK(host.pending.z == 8.0f);

	// Neighboring check: when highestLayer == ground (0), aim = 0 + 15 = 15 -> step = ((5 + (15-5)*0.5) - 5)/10 = 0.5 -> pending z = 6.0
	Locomotor loco2(lt);
	Local126LocoHost host2;
	host2.place(10.0f, 10.0f, 5.0f);
	host2.groundZ = 0.0f;
	host2.highestZ = 0.0f;
	CHECK_NOTHROW(loco2.locomotorMoveTowardsPosition(host2, Coord3D{ 50.0f, 10.0f, 15.0f }, 40.0f, 10.0f));
	CHECK(host2.pending.z == 6.0f);
}

// RW 0x71EBDA (RW 0x71EE29..0x71EE61, RW 0x71EE48 -> RW 0x68C82D(3, &amount, 0, 1)):
// a WALL_UPGRADE object whose DAMAGE_MULT attribute modifier product equals 0.0f is in CB_CONTEXT_UNDER_CONSTRUCTION.
TEST_CASE("LOCAL-126 ControlBar::isUnderConstructionContext reads DAMAGE_MULT for WALL_UPGRADE (RW 0x71EBDA / 0x71EE48)")
{
	const char *kIni =
		"ModifierList Local126ZeroDamage\n"
		"  Category = SPELL\n"
		"  Modifier = DAMAGE_MULT 0%\n"
		"  Duration = 60000\n"
		"End\n"
		"ModifierList Local126HalfDamage\n"
		"  Category = LEADERSHIP\n"
		"  Modifier = DAMAGE_MULT 50%\n"
		"  Duration = 60000\n"
		"End\n"
		"Object Local126WallUpgrade\n"
		"  KindOf = STRUCTURE SELECTABLE WALL_UPGRADE\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 10\n"
		"  GeometryMinorRadius = 10\n"
		"  GeometryHeight = 10\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
		"  End\n"
		"End\n"
		"Object Local126PlainStructure\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 10\n"
		"  GeometryMinorRadius = 10\n"
		"  GeometryHeight = 10\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
		"  End\n"
		"End\n";

	Local126ModWorld cw(kIni, "local126_dmgmult.ini");

	Object *wallUp = cw.unit("Local126WallUpgrade", 'A', 100.0f, 100.0f);
	Object *plain = cw.unit("Local126PlainStructure", 'A', 140.0f, 100.0f);
	REQUIRE(wallUp != nullptr);
	REQUIRE(plain != nullptr);

	// Without modifier: not under construction.
	CHECK_FALSE(ControlBar::isUnderConstructionContext(*wallUp));

	// With DAMAGE_MULT 50% (0.5f != 0.0f): still not under construction.
	REQUIRE(wallUp->addAttributeModifier("Local126HalfDamage", -1));
	CHECK_FALSE(ControlBar::isUnderConstructionContext(*wallUp));

	// With DAMAGE_MULT 0% (0.0f): WALL_UPGRADE enters under-construction context (RW 0x71EE48..0x71EE61).
	REQUIRE(wallUp->addAttributeModifier("Local126ZeroDamage", -1));
	CHECK(ControlBar::isUnderConstructionContext(*wallUp));

	// Neighboring check: non-WALL_UPGRADE with DAMAGE_MULT 0% does NOT enter under-construction context.
	REQUIRE(plain->addAttributeModifier("Local126ZeroDamage", -1));
	CHECK_FALSE(ControlBar::isUnderConstructionContext(*plain));
}

// RW 0x7369AE (RW 0x736A09 -> RW 0x68C818, RESIST_FEAR) and RW 0x7366FE (RW 0x69005F, ObjectDescription):
// LiveScripting implements objectAttributeModifier (sum) and objectDescription on live GameLogic objects.
TEST_CASE("LOCAL-126 LiveScripting objectAttributeModifier (RESIST_FEAR, RW 0x736A09) and objectDescription (RW 0x7366FE)")
{
	const char *kIni =
		"ModifierList Local126FearImmune\n"
		"  Category = SPELL\n"
		"  Modifier = RESIST_FEAR 100%\n"
		"  Duration = 60000\n"
		"End\n"
		"Object Local126Soldier\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 5\n"
		"  GeometryHeight = 10\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
		"  End\n"
		"End\n";

	Local126ModWorld cw(kIni, "local126_lua.ini");

	Win32BIGFileSystem fs;
	NameKeyGenerator keys;
	LiveScripting scripting(*cw.logic, fs, keys);

	Object *u = cw.unit("Local126Soldier", 'A', 50.0f, 50.0f);
	REQUIRE(u != nullptr);

	// 1. objectDescription (RW 0x7366FE -> 0x69005F)
	LuaGameHost::ObjectDescription desc;
	REQUIRE(scripting.objectDescription((int)u->getID(), &desc));
	CHECK(desc.templateName == "Local126Soldier");
	CHECK_FALSE(desc.playerName.empty());

	// 2. objectAttributeModifier RESIST_FEAR (type 4, RW 0x736A09 -> 0x68C818 -> 0x804F39)
	float bonus = -1.0f;
	CHECK_FALSE(scripting.objectAttributeModifier((int)u->getID(), 4, &bonus));

	REQUIRE(u->addAttributeModifier("Local126FearImmune", -1));

	bonus = 0.0f;
	REQUIRE(scripting.objectAttributeModifier((int)u->getID(), 4, &bonus));
	CHECK(bonus == 1.0f);
}

// Ennorath scenario (data/ini/includes/locomotor.inc, agandaurlightsphere.ini, isengardfortress.ini):
// real mod objects IsengardFortress (neutral unpack) and AgandaurLightSphere1/2/3 (BattleDroneLocomotor).
TEST_CASE("LOCAL-126 ENNORATH scenario: IsengardFortress and AgandaurLightSphere1/2/3")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-126 ENNORATH scenario"))
	{
		return;
	}
	hudtest::SharedWorld &s = world.shared;
	auto scope = s.world->enterContext();

	// 1. AgandaurLightSphere1/2/3 use BattleDroneLocomotor with ZAxisBehavior = RELATIVE_TO_HIGHEST_LAYER
	REQUIRE(TheLocomotorStore != nullptr);
	const LocomotorTemplate *droneLoco = TheLocomotorStore->findLocomotorTemplate("BattleDroneLocomotor");
	REQUIRE(droneLoco != nullptr);
	CHECK(droneLoco->m_zAxisBehavior == Z_RELATIVE_TO_HIGHEST_LAYER);
	Locomotor loco(droneLoco);
	Local126LocoHost host;
	host.place(0.0f, 0.0f, 5.0f);
	host.highestZ = 10.0f;
	CHECK_NOTHROW(loco.locomotorMoveTowardsPosition(host, Coord3D{ 50.0f, 0.0f, 25.0f }, 50.0f, 10.0f));
	CHECK(host.hasPending);

	// 2. IsengardFortress owned by neutral ("") unpacks with 0 objects and no error (RW 0x798F70)
	const ThingTemplate *fort = s.world->things().findTemplate("IsengardFortress");
	REQUIRE(fort != nullptr);
	structtest::Arena arena(s, "FactionMen", "FactionMordor");
	Player *neutral = arena.logic.players().getNeutralPlayer();
	REQUIRE(neutral != nullptr);
	Object *fortObj = arena.logic.newObject(fort, neutral->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(fortObj != nullptr);
	const Coord3D fpos{ 100.0f, 100.0f, 0.0f };
	fortObj->setPosition(&fpos);
	if (auto *cb = dynamic_cast<CastleBehavior *>(fortObj->findModule("CastleBehavior")))
	{
		CHECK(cb->unpack(true));
		CHECK(cb->lastError().empty());
	}
}
