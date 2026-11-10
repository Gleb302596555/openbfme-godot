// OpenBFME. GPL-3.0.
// PORT-89 (issue #237): Object/SoundCrushing (Object::onCrush RW 0x693231 .. 0x69329C, BFME2 ObjectRva002929F9.cpp).
//
// RW facts verified in workspace/RotWK/game.dat (caveat S-001):
//   * ThingTemplate audio field table (RW 0x00C26720, 56 entries): row 0x37 at 0x00C26A90 is "SoundCrushing" (0x00C26290), parser 0x0073AD07 -> 0x0073ACAF ->
//     0x0073AB45 (NoSound clears, +SOUND: strips prefix, EVA: invalid on sound rows >= 0x21), stored at ThingTemplate + 0x2DC (+0x124 + 0x37*8).
//   * Template getter 0x00645A89; Drawable::getSoundCrushing (RW 0x0068FFEF: push 0x37; call 0x00676A8A) is called ONLY from Object::onCrush (0x0069324A).
//   * SquishCollide::onCollide (RW 0x008BFBAE) calls Object::onCrush (RW 0x008BFEA9 -> 0x0069320D) after canCrush (0x008BFE5B) and body slot 0x9C (0x008BFE88),
//     before RamPower shockwave (0x008BFEB1) and CrushWeapon / CrushRevengeWeapon.
//   * Object::onCrush (0x0069320D, esi = crusherIn, edi = victim): returns immediately if victim->isEffectivelyDead() ([edi + 0x458] & 1, RW 0x693222 .. 0x69322B).
//     Otherwise, BEFORE checking m_containedBy HORDE recursion (RW 0x6932A1 .. 0x6932BD) and before CrushKnockback / deceleration: if m_drawable != null and
//     TheAudio != null, gets SoundCrushing (0x68FFEF), constructs AudioEventRTS (0x6DA95E), sets crusherIn->getID() (0x6DA6A7), and calls TheAudio->addAudioEvent
//     (vslot 0x64). Then if m_containedBy is a HORDE, calls onCrush(*containedBy, victim) and returns (0x6932BD).
//
// Synthetic data only: no retail file is read, so the tests pass without the game and the mod.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "Horde2TestUtil.h"
#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/SquishCollide.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/UnitSpecificSound.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace combattest;
using namespace horde2test;

namespace
{
const char kPort89Objects[] =
	"#define PORT89_MACRO_SND CrushSnd\n"
	"Object Port89Rider\n"
	"  KindOf = CAVALRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 150\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 3\n"
	"  CrushWeapon = TrampleWeapon\n"
	"  CrushKnockback = 40\n"
	"  CrushZFactor = 1.0\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = CrushSnd\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Port89RiderPlusPrefix\n"
	"  KindOf = CAVALRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 150\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 3\n"
	"  CrushWeapon = TrampleWeapon\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = +SOUND:CrushSnd\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Port89RiderMacro\n"
	"  KindOf = CAVALRY SELECTABLE SCORE\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 3\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = PORT89_MACRO_SND\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Port89RiderNoSound\n"
	"  KindOf = CAVALRY SELECTABLE SCORE\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 3\n"
	"  CrushWeapon = TrampleWeapon\n"
	"  CrushKnockback = 40\n"
	"  CrushZFactor = 1.0\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = NoSound\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Port89RiderUnset\n"
	"  KindOf = CAVALRY SELECTABLE SCORE\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 3\n"
	"  CrushWeapon = TrampleWeapon\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"ChildObject Port89ChildRider Port89Rider\n"
	"End\n"
	"ChildObject Port89SilentChildRider Port89Rider\n"
	"  SoundCrushing = NoSound\n"
	"End\n"
	"Object Port89Footman\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  CrushableLevel = 0\n"
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
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port89Pikeman\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  CrushableLevel = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port89RiderHorde\n"
	"  KindOf = CAVALRY HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 200\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 2\n"
	"  CrushKnockback = 40\n"
	"  CrushZFactor = 1.0\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = NoSound\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:Port89Rider Position:X:0 Y:0\n"
	"    InitialPayload = Port89Rider 1\n"
	"    MeleeBehavior = Amoeba\n"
	"    End\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Port89RiderHordeWithSound\n"
	"  KindOf = CAVALRY HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 200\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 2\n"
	"  CrushKnockback = 40\n"
	"  CrushZFactor = 1.0\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = HordeCrushSnd\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:Port89Rider Position:X:0 Y:0\n"
	"    InitialPayload = Port89Rider 1\n"
	"    MeleeBehavior = Amoeba\n"
	"    End\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Port89SilentRiderHordeWithSound\n"
	"  KindOf = CAVALRY HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 200\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 2\n"
	"  CrushKnockback = 40\n"
	"  CrushZFactor = 1.0\n"
	"  MinCrushVelocityPercent = 40%\n"
	"  CrushDecelerationPercent = 20%\n"
	"  SoundCrushing = HordeCrushSnd\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:Port89RiderNoSound Position:X:0 Y:0\n"
	"    InitialPayload = Port89RiderNoSound 1\n"
	"    MeleeBehavior = Amoeba\n"
	"    End\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n";

struct LogicResolver : AudioOwnerResolver
{
	const GameLogic &logic;
	explicit LogicResolver(const GameLogic &l) : logic(l) {}
	bool objectPosition(std::uint32_t id, Coord3D *pos) const override
	{
		if (const Object *o = logic.findObjectByID((ObjectID)id))
		{
			*pos = *o->getPosition();
			return true;
		}
		return false;
	}
	bool drawablePosition(std::uint32_t, Coord3D *) const override { return false; }
};

struct Port89Audio
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> mgr;
	LogicResolver resolver;

	explicit Port89Audio(const GameLogic &logic)
		: resolver(logic)
	{
		fx.mount({ { "data\\audio\\sounds\\click.wav", "x" } });
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini",
			"AudioSettings\n"
			"  AudioRoot = Data\\Audio\n"
			"  SoundsFolder = Sounds\n"
			"  SoundsExtension = wav\n"
			"  SampleCount2D = 2\n"
			"  SampleCount3D = 4\n"
			"  StreamCount = 2\n"
			"  MinSampleVolume = 10\n"
			"End\n"
			"AudioEvent CrushSnd\n"
			"  Sounds = click\n"
			"  Type = world everyone\n"
			"End\n"
			"AudioEvent OverrideCrushSnd\n"
			"  Sounds = click\n"
			"  Type = world everyone\n"
			"End\n"
			"AudioEvent HordeCrushSnd\n"
			"  Sounds = click\n"
			"  Type = world everyone\n"
			"End\n");
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		device->setFileLength("Data\\Audio\\Sounds\\click.wav", 250.0);
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1u);
		mgr->setOwnerResolver(&resolver);
		mgr->enableEventLog(true);
		mgr->update(0.0);
		AudioApi::install(mgr.get());
	}
	~Port89Audio() { AudioApi::install(nullptr); }
};

struct Port89World : CombatWorld
{
	static std::string makeObjects(bool silentSound)
	{
		std::string s = kPort89Objects;
		if (silentSound)
		{
			const std::string needle = "  SoundCrushing = CrushSnd\n";
			const size_t pos = s.find(needle);
			if (pos != std::string::npos)
			{
				s.replace(pos, needle.size(), "  SoundCrushing = NoSound\n");
			}
		}
		return s;
	}
	explicit Port89World(bool silentPort89Rider = false)
		: CombatWorld(makeObjects(silentPort89Rider).c_str())
	{
		REQUIRE(w.load(kCrushWeapons, INI_LOAD_OVERWRITE, "weapon2.ini").empty());
		combat().setAutoAcquireEnabled(false);
	}
};

std::string findStop(const std::vector<std::string> &lines, const std::string &prefix)
{
	for (const std::string &s : lines)
	{
		if (s.rfind(prefix, 0) == 0)
		{
			return s;
		}
	}
	return std::string();
}
} // namespace

TEST_CASE("PORT-89: charging cavalry plays SoundCrushing once per crush at the crusher's object ID and position; NoSound and unset stay silent")
{
	SUBCASE("SoundCrushing plays once, tagged SoundCrushing, at the crusher's object and position alongside knockback and deceleration")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *rider = w.unit("Port89Rider", 'A', 200.0f, 300.0f);
		Object *foot = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(2);
		audio.mgr->takeEventLog();

		rider->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 400.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
		// Within the frame of the crush, partitionAndCollision (Phase 2) runs before Locomotor movement (Phase 3..6),
		// so SoundCrushing captures the crusher's position entering that frame.
		Coord3D riderPosAtCrush = *rider->getPosition();
		Coord3D footPosAtCrush = *foot->getPosition();
		for (int i = 0; i < 100 && w.combat().counters().crushes == 0; ++i)
		{
			riderPosAtCrush = *rider->getPosition();
			footPosAtCrush = *foot->getPosition();
			w.frames(1);
		}
		REQUIRE(w.combat().counters().crushes == 1);
		CHECK(w.combat().counters().crushWeaponShots == 1);
		CHECK(w.combat().counters().crushKnockbacks == 1);
		CHECK(w.combat().counters().crushDecelerations == 1);

		const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
		REQUIRE(log.size() == 1);
		CHECK(log[0].play == false);
		CHECK(log[0].event == "CrushSnd");
		CHECK(log[0].origin == "SoundCrushing");
		CHECK(log[0].objectId == (std::uint32_t)rider->getID());
		CHECK(log[0].positional == true);
		CHECK(log[0].position.x == riderPosAtCrush.x);
		CHECK(log[0].position.y == riderPosAtCrush.y);
		CHECK(log[0].position.z == riderPosAtCrush.z);
		CHECK(log[0].outcome == "queued");
		// Verify the position is the crusher's, not the victim's
		CHECK_FALSE(log[0].objectId == (std::uint32_t)foot->getID());
		CHECK_FALSE(log[0].position.x == footPosAtCrush.x);

		audio.device->setTime(50.0);
		audio.mgr->update(50.0);
		REQUIRE(audio.device->voices().size() == 1);
		const SimulatedAudioDevice::Voice &v = audio.device->voices().begin()->second;
		CHECK(v.start.eventName == "CrushSnd");
		CHECK(v.start.kind == VoiceKind::Sample3D);
	}

	SUBCASE("+SOUND: prefix and macro tokens resolve per RW 0x73AB45; NoSound and unset do not post audio")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *plus = w.unit("Port89RiderPlusPrefix", 'A', 200.0f, 200.0f);
		Object *macroRider = w.unit("Port89RiderMacro", 'A', 200.0f, 300.0f);
		Object *silent = w.unit("Port89RiderNoSound", 'A', 200.0f, 400.0f);
		Object *unset = w.unit("Port89RiderUnset", 'A', 200.0f, 500.0f);
		Object *foot1 = w.unit("Port89Footman", 'B', 300.0f, 200.0f);
		Object *foot2 = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		Object *foot3 = w.unit("Port89Footman", 'B', 300.0f, 400.0f);
		Object *foot4 = w.unit("Port89Footman", 'B', 300.0f, 500.0f);
		w.frames(2);
		audio.mgr->takeEventLog();

		ObjectCrush::onCrush(*plus, *foot1);
		ObjectCrush::onCrush(*macroRider, *foot2);
		ObjectCrush::onCrush(*silent, *foot3);
		ObjectCrush::onCrush(*unset, *foot4);

		const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
		REQUIRE(log.size() == 2);
		CHECK(log[0].event == "CrushSnd");
		CHECK(log[0].objectId == (std::uint32_t)plus->getID());
		CHECK(log[1].event == "CrushSnd");
		CHECK(log[1].objectId == (std::uint32_t)macroRider->getID());
	}

	SUBCASE("without AudioManager installed, empty/NoSound skips AudioApi while set SoundCrushing increments callsWithoutAudio")
	{
		Port89World w;
		AudioApi::install(nullptr);
		Object *rider = w.unit("Port89Rider", 'A', 200.0f, 300.0f);
		Object *silent = w.unit("Port89RiderNoSound", 'A', 200.0f, 400.0f);
		Object *foot1 = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		Object *foot2 = w.unit("Port89Footman", 'B', 300.0f, 400.0f);
		w.frames(2);

		const std::uint64_t c0 = AudioApi::callsWithoutAudio();
		ObjectCrush::onCrush(*silent, *foot2);
		CHECK(AudioApi::callsWithoutAudio() == c0);
		ObjectCrush::onCrush(*rider, *foot1);
		CHECK(AudioApi::callsWithoutAudio() == c0 + 1);
	}
}

TEST_CASE("PORT-89: dead victims, failed canCrush bumps, equal CrushableLevel, stunned targets, and allies play nothing")
{
	SUBCASE("dead victim returns early at RW 0x693222 before playing SoundCrushing")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *rider = w.unit("Port89Rider", 'A', 200.0f, 300.0f);
		Object *foot = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(2);
		foot->kill(DAMAGE_UNRESISTABLE, 0);
		REQUIRE(foot->isEffectivelyDead());
		audio.mgr->takeEventLog();

		ObjectCrush::onCrush(*rider, *foot);
		CHECK(audio.mgr->takeEventLog().empty());
		CHECK(w.combat().counters().crushKnockbacks == 0);
		CHECK(w.combat().counters().crushDecelerations == 0);
	}

	SUBCASE("standing rider bumps (canCrush false) without SoundCrushing")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *rider = w.unit("Port89Rider", 'A', 300.0f, 300.0f);
		w.unit("Port89Footman", 'B', 304.0f, 300.0f);
		w.frames(3);
		CHECK_FALSE(ObjectCrush::canCrush(*rider));
		CHECK(w.combat().counters().crushes == 0);
		CHECK(w.combat().counters().crushBumps == 3);
		CHECK(audio.mgr->takeEventLog().empty());
	}

	SUBCASE("equal CrushableLevel and allied targets do not crush or play SoundCrushing")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *riderA = w.unit("Port89Rider", 'A', 200.0f, 300.0f);
		w.unit("Port89Pikeman", 'B', 300.0f, 300.0f);
		w.unit("Port89Footman", 'A', 340.0f, 300.0f);
		w.frames(2);
		audio.mgr->takeEventLog();

		riderA->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 450.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
		w.frames(40);
		CHECK(w.combat().counters().crushes == 0);
		CHECK(audio.mgr->takeEventLog().empty());
	}

	SUBCASE("stunned victim is skipped by SquishCollide::onCollide (RW 0x8BFC00) and plays nothing")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *rider = w.unit("Port89Rider", 'A', 200.0f, 300.0f);
		Object *foot = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(2);
		PhysicsBehavior *phys = PhysicsBehavior::find(*foot);
		REQUIRE(phys != nullptr);
		phys->setStunned(true);
		audio.mgr->takeEventLog();

		rider->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 400.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
		w.frames(30);
		CHECK(w.combat().counters().crushes == 0);
		CHECK(audio.mgr->takeEventLog().empty());
	}
}

TEST_CASE("PORT-89: horde member plays its own SoundCrushing at its own object ID before recursing onto the horde (RW 0x693231..0x6932BD)")
{
	SUBCASE("live crush by a horde member whose horde has NoSound plays the member's SoundCrushing at the member's object ID")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *horde = w.unit("Port89RiderHorde", 'A', 200.0f, 300.0f);
		w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(3);
		std::vector<Object *> members = w.membersOf(horde);
		REQUIRE(members.size() == 1);
		Object *member = members[0];
		audio.mgr->takeEventLog();

		horde->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 420.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
		Coord3D memberPosAtCrush = *member->getPosition();
		for (int i = 0; i < 100 && w.combat().counters().crushes == 0; ++i)
		{
			memberPosAtCrush = *member->getPosition();
			w.frames(1);
		}
		REQUIRE(w.combat().counters().crushes == 1);
		CHECK(w.combat().counters().crushDecelerations == 1);

		const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
		REQUIRE(log.size() == 1);
		CHECK(log[0].event == "CrushSnd");
		CHECK(log[0].origin == "SoundCrushing");
		CHECK(log[0].objectId == (std::uint32_t)member->getID());
		CHECK(log[0].position.x == memberPosAtCrush.x);
		CHECK(log[0].position.y == memberPosAtCrush.y);
		CHECK_FALSE(log[0].objectId == (std::uint32_t)horde->getID());
	}

	SUBCASE("order proof: when both member and horde define SoundCrushing, member fires first and horde fires second")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *horde = w.unit("Port89RiderHordeWithSound", 'A', 200.0f, 300.0f);
		Object *foot = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(3);
		std::vector<Object *> members = w.membersOf(horde);
		REQUIRE(members.size() == 1);
		Object *member = members[0];
		audio.mgr->takeEventLog();

		ObjectCrush::onCrush(*member, *foot);
		const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
		REQUIRE(log.size() == 2);
		CHECK(log[0].event == "CrushSnd");
		CHECK(log[0].objectId == (std::uint32_t)member->getID());
		CHECK(log[1].event == "HordeCrushSnd");
		CHECK(log[1].objectId == (std::uint32_t)horde->getID());
	}

	SUBCASE("silent member inside a sounding horde still recurses to play the horde's SoundCrushing")
	{
		Port89World w;
		Port89Audio audio(*w.logic);
		Object *horde = w.unit("Port89SilentRiderHordeWithSound", 'A', 200.0f, 300.0f);
		Object *foot = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(3);
		std::vector<Object *> members = w.membersOf(horde);
		REQUIRE(members.size() == 1);
		audio.mgr->takeEventLog();

		ObjectCrush::onCrush(*members[0], *foot);
		const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
		REQUIRE(log.size() == 1);
		CHECK(log[0].event == "HordeCrushSnd");
		CHECK(log[0].objectId == (std::uint32_t)horde->getID());
	}
}

TEST_CASE("PORT-89: ChildObject inheritance and map.ini final template overrides apply to SoundCrushing")
{
	Port89World w;
	Port89Audio audio(*w.logic);
	Object *childInherits = w.unit("Port89ChildRider", 'A', 200.0f, 200.0f);
	Object *childNoSound = w.unit("Port89SilentChildRider", 'A', 200.0f, 300.0f);
	Object *overridden = w.unit("Port89Rider", 'A', 200.0f, 400.0f);
	REQUIRE(w.w.load("Object Port89Rider\n  SoundCrushing = OverrideCrushSnd\nEnd\n", INI_LOAD_CREATE_OVERRIDES, "map.ini").empty());
	Object *foot1 = w.unit("Port89Footman", 'B', 300.0f, 200.0f);
	Object *foot2 = w.unit("Port89Footman", 'B', 300.0f, 300.0f);
	Object *foot3 = w.unit("Port89Footman", 'B', 300.0f, 400.0f);
	w.frames(2);
	audio.mgr->takeEventLog();

	CHECK(UnitSpecificSound::templateSound(*childInherits->getTemplate()->getFinalOverride(), "SoundCrushing") == "CrushSnd");
	CHECK(UnitSpecificSound::templateSound(*childNoSound->getTemplate()->getFinalOverride(), "SoundCrushing").empty());
	CHECK(UnitSpecificSound::templateSound(*overridden->getTemplate()->getFinalOverride(), "SoundCrushing") == "OverrideCrushSnd");

	ObjectCrush::onCrush(*childInherits, *foot1);
	ObjectCrush::onCrush(*childNoSound, *foot2);
	ObjectCrush::onCrush(*overridden, *foot3);

	const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
	REQUIRE(log.size() == 2);
	CHECK(log[0].event == "CrushSnd");
	CHECK(log[0].objectId == (std::uint32_t)childInherits->getID());
	CHECK(log[1].event == "OverrideCrushSnd");
	CHECK(log[1].objectId == (std::uint32_t)overridden->getID());
	SUBCASE("templateSound and ambient soundField share macro expansion, +SOUND: stripping, EVA:/NoSound filtering, and map.ini overrides across sibling rows")
	{
		Port89World w;
		REQUIRE(w.w.load(
					 "#define PORT89_CRUSH_MACRO +SOUND:MacroCrushSnd\n"
					 "#define PORT89_AMBIENT_MACRO +SOUND:MacroAmbientSnd\n"
					 "Object Port89RowParity\n"
					 "  KindOf = INFANTRY\n"
					 "  Body = ActiveBody ModuleTag_Body\n"
					 "    MaxHealth = 100\n"
					 "    MaxHealthDamaged = 66\n"
					 "    MaxHealthReallyDamaged = 33\n"
					 "    InitialHealth = 100\n"
					 "  End\n"
					 "  SoundCrushing = PORT89_CRUSH_MACRO\n"
					 "  SoundOnDamaged = +SOUND:DamagedStripSnd\n"
					 "  SoundImpact = EVA:NotAllowedOnSoundRows\n"
					 "  SoundAmbient = PORT89_AMBIENT_MACRO\n"
					 "  SoundAmbientDamaged = NoSound\n"
					 "End\n",
					 INI_LOAD_OVERWRITE, "object_row_parity.ini")
					.empty());
		const ThingTemplate *tt = w.w.things.findTemplate("Port89RowParity");
		REQUIRE(tt != nullptr);
		CHECK(UnitSpecificSound::templateSound(*tt, "SoundCrushing") == "MacroCrushSnd");
		CHECK(UnitSpecificSound::templateSound(*tt, "SoundOnDamaged") == "DamagedStripSnd");
		CHECK(UnitSpecificSound::templateSound(*tt, "SoundImpact").empty());
		CHECK(UnitSpecificSound::templateSound(*tt, "SoundAmbient") == "MacroAmbientSnd");
		CHECK(UnitSpecificSound::templateSound(*tt, "SoundAmbientDamaged").empty());

		Object *obj = w.unit("Port89RowParity", 'A', 200.0f, 300.0f);
		REQUIRE(obj != nullptr);
		REQUIRE(obj->getBodyModule() != nullptr);
		CHECK(LiveGameAudio::ambientSoundFor(*obj) == "MacroAmbientSnd");
		DamageInfo dmg;
		dmg.m_input.m_amount = 40.0f;
		dmg.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		obj->attemptDamage(dmg);
		REQUIRE(obj->getBodyModule()->getDamageState() == BODY_DAMAGED);
		CHECK(LiveGameAudio::ambientSoundFor(*obj) == "MacroAmbientSnd"); // damaged is NoSound -> pristine fallback

		REQUIRE(w.w.load("Object Port89RowParity\n  SoundCrushing = OverrideSnd\n  SoundAmbient = OverrideAmbientSnd\nEnd\n",
					 INI_LOAD_CREATE_OVERRIDES, "map.ini")
					.empty());
		CHECK(UnitSpecificSound::templateSound(*tt, "SoundCrushing") == "OverrideSnd");
		CHECK(LiveGameAudio::ambientSoundFor(*obj) == "OverrideAmbientSnd");
	}
}

TEST_CASE("PORT-89: SoundCrushing does not touch simulation state hashes and stops S-580 / S-707 / S-1242 reflect the port")
{
	auto runHashTrace = [](bool silentTemplate, bool withAudio) {
		Port89World w(silentTemplate);
		std::unique_ptr<Port89Audio> audio;
		if (withAudio)
		{
			audio = std::make_unique<Port89Audio>(*w.logic);
		}
		else
		{
			AudioApi::install(nullptr);
		}
		Object *rider = w.unit("Port89Rider", 'A', 200.0f, 300.0f);
		w.unit("Port89Footman", 'B', 300.0f, 300.0f);
		w.frames(2);
		rider->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 400.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
		std::vector<std::uint32_t> hashes;
		for (int i = 0; i < 40; ++i)
		{
			w.frames(1);
			hashes.push_back(w.hash());
		}
		return hashes;
	};

	const std::vector<std::uint32_t> hWithAudio = runHashTrace(false, true);
	const std::vector<std::uint32_t> hNoAudio = runHashTrace(false, false);
	const std::vector<std::uint32_t> hNoSound = runHashTrace(true, true);
	CHECK(hWithAudio == hNoAudio);
	CHECK(hWithAudio == hNoSound);

	Port89World w;
	const std::string s580 = findStop(w.combat().report(), "[S-580]");
	REQUIRE_FALSE(s580.empty());
	const size_t notPortedPos = s580.find("NOT ported:");
	REQUIRE_FALSE(notPortedPos == std::string::npos);
	CHECK_FALSE(s580.substr(0, notPortedPos).find("SoundCrushing") == std::string::npos);
	CHECK(s580.substr(notPortedPos).find("SoundCrushing") == std::string::npos);

	const std::vector<std::string> audioStops = LiveGameAudio::acceptanceStops();
	REQUIRE(audioStops.size() == 9);
	CHECK(audioStops[1].rfind("[S-707]", 0) == 0);
	const size_t donePos = audioStops[1].find("done by lane");
	REQUIRE_FALSE(donePos == std::string::npos);
	CHECK(audioStops[1].substr(0, donePos).find("SoundCrushing") == std::string::npos);
	CHECK_FALSE(audioStops[1].substr(donePos).find("SoundCrushing") == std::string::npos);
	CHECK_FALSE(audioStops[1].find("S-701") == std::string::npos);
	CHECK(audioStops[8].rfind("[S-1242]", 0) == 0);
}
