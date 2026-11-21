// OpenBFME unit tests (PORT-231, issue #554): building, producing and earning layer.
// Expected behaviour and values are proven from RotWK 2.01 game.dat (md5 daed3668):
//   - GameData::SellPercentage ctor default 1.0f (RW 0x006429AD -> 0x00643160 / 0x00643222, S-1981)
//   - CastleMemberBehavior::BeingBuiltSound (RW 0x00799F0F -> 0x00798057 -> 0x00797C40, HELD_CASTLE_BEING_BUILT = 6)
//   - ProductionUpdate::completeUnit script naming (RW 0x008A253B / 0x008A2624) & entry + 0x3C hordeTarget migration (RW 0x008A2A67 -> 0x008A171F -> 0x008A07E0)
//   - Object::attemptHealingFromSoleBenefactor KeepSelectableWhenDead tail (RW 0x00690584 -> 0x00690626, S-652)

#include "doctest.h"
#include "IniTestUtil.h"
#include "ProdTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioRequests.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameLogic/BuildAssistantSell.h"
#include "GameLogic/Construction.h"
#include "GameLogic/Map/CastleTemplates.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/ProductionQueueHordeContain.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

#include <memory>
#include <string>
#include <vector>

using namespace prodtest;

namespace
{
std::vector<std::uint8_t> makeSilenceWavMs(double ms)
{
	const std::uint32_t rate = 22050;
	// tolerance: sample count for synthetic silent audio test fixture
	const std::uint32_t frames = (std::uint32_t)(ms * rate / 1000.0);
	std::vector<std::uint8_t> out;
	auto u32 = [&](std::uint32_t v) {
		for (int i = 0; i < 4; ++i)
		{
			out.push_back((std::uint8_t)(v >> (8 * i)));
		}
	};
	auto u16 = [&](std::uint16_t v) {
		out.push_back((std::uint8_t)v);
		out.push_back((std::uint8_t)(v >> 8));
	};
	auto tag = [&](const char *t) { out.insert(out.end(), t, t + 4); };
	tag("RIFF");
	u32(36 + frames * 2);
	tag("WAVE");
	tag("fmt ");
	u32(16);
	u16(1);
	u16(1);
	u32(rate);
	u32(rate * 2);
	u16(2);
	u16(16);
	tag("data");
	u32(frames * 2);
	out.insert(out.end(), frames * 2, 0);
	return out;
}

std::string toStr(const std::vector<std::uint8_t> &b)
{
	return std::string(b.begin(), b.end());
}

const char kPort231AudioBase[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n"
	"  SoundsExtension = wav\n  SampleCount2D = 2\n  SampleCount3D = 3\n  StreamCount = 3\n  MinSampleVolume = 2%\n  GlobalMinRange = 5000\n"
	"  GlobalMaxRange = 5000000\n  TimeToFadeAudio = 300\n  AmbientStreamHysteresisVolume = 10\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n"
	"  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent DefaultSoundEffect\n  Volume = 100\n  Priority = high\n  Limit = 4\n  MinRange = 160\n  MaxRange = 640\n  PitchShift = 0 0\n  PlayPercent = 100\n"
	"  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"MusicTrack DefaultMusicTrack\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  SubmixSlider = music\nEnd\n"
	"DialogEvent DefaultDialog\n  Volume = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"AmbientStream DefaultAmbientStream\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  MinRange = 100\n  MaxRange = 1000\n  SubmixSlider = ambient\nEnd\n"
	"AudioEvent TestCastleBuildLoop\n  Sounds = castleloop\n  Control = loop\n  Type = world everyone\n  MaxRange = 5000\n  Limit = 0\nEnd\n";

struct Port231Audio
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> mgr;

	Port231Audio()
	{
		initest::FileList list;
		list.push_back({ "data\\audio\\sounds\\castleloop.wav", toStr(makeSilenceWavMs(4000.0)) });
		list.push_back({ "data\\ini\\none.txt", "x" });
		fx.mount(list);
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", kPort231AudioBase);
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1234u);
		mgr->enableEventLog(true);
		mgr->update(0.0);
		AudioApi::install(mgr.get());
	}
	~Port231Audio() { AudioApi::install(nullptr); }

	double nowMs = 0.0;
	size_t startsOf(const std::string &needle)
	{
		nowMs += 100.0;
		mgr->update(nowMs);
		size_t n = 0;
		for (const auto &e : device->log())
		{
			if (e.kind == SimulatedAudioDevice::LogEntry::Start && e.file.find(needle) != std::string::npos)
			{
				++n;
			}
		}
		return n;
	}
};

struct Port231World : ProdWorld
{
	Port231World()
	{
		logic->settings().bodyThresholdsLoaded = true;
		logic->partition().setRegion(0.0f, 0.0f, 4000.0f, 4000.0f);
	}
};
} // namespace

TEST_CASE("PORT-231: CastleMemberBehavior BeingBuiltSound loops across BASE_BUILD and stops on completion or delete (RW 0x799F0F -> 0x798057 / 0x797C40)")
{
	Port231Audio audio;
	Port231World f;
	f.load(
		"Object CastleKeep\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE CASTLE_KEEP\n"
		"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
		"    BeingBuiltSound = TestCastleBuildLoop\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"  End\n"
		"End\n"
		"Object CastlePad\n"
		"  KindOf = STRUCTURE IMMOBILE CASTLE_CENTER\n"
		"  Behavior = CastleBehavior ModuleTag_CB\n"
		"    CastleToUnpackForFaction = Alpha Base_Timed\n"
		"    FadeTime = 0.3\n"
		"    BuildTime = 0.3\n"
		"    InstantUnpack = No\n"
		"    Summoned = Yes\n"
		"  End\n"
		"End\n"
		"Object NoSoundKeep\n"
		"  KindOf = STRUCTURE IMMOBILE CASTLE_KEEP\n"
		"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
		"    BeingBuiltSound = NoSound\n"
		"  End\n"
		"End\n");

	CastleTemplate layout;
	layout.name = "Base_Timed";
	layout.version = 5;
	layout.entries.push_back({ "", "CastleKeep", 0.0f, 0.0f, 0.0f, 0.0f, 0, 1 });
	f.logic->castleTemplates().add(layout);

	Object *castle = f.make("CastlePad", f.teamOf("Alice"));
	REQUIRE(castle != nullptr);
	Coord3D pos{ 1000.0f, 1000.0f, 0.0f };
	castle->setPosition(&pos);
	castle->friend_onBuildComplete();
	CastleBehavior *cb = CastleBehavior::of(*castle);
	REQUIRE(cb != nullptr);

	// Run frames until STATE_UNPACKING, then STATE_BUILDING (BASE_BUILD set on keep)
	f.frames(1);
	REQUIRE(cb->state() == CastleBehavior::STATE_UNPACKING);
	Object *keep = f.logic->findObjectByID(cb->keepId());
	REQUIRE(keep != nullptr);
	CHECK(AudioApi::heldSound(AudioApi::HELD_CASTLE_BEING_BUILT, keep->getID()) == 0u);

	f.frames(2);
	REQUIRE(cb->state() == CastleBehavior::STATE_BUILDING);
	// tolerance: valid AudioHandle is >= AHSV_FirstHandle
	CHECK(AudioApi::heldSound(AudioApi::HELD_CASTLE_BEING_BUILT, keep->getID()) >= AHSV_FirstHandle);
	CHECK(AudioApi::heldSounds(AudioApi::HELD_CASTLE_BEING_BUILT) == 1u);
	CHECK(audio.startsOf("castleloop") == 1u);

	// Once STATE_BUILDING finishes, BASE_BUILD clears and BeingBuiltSound stops
	f.frames(3);
	REQUIRE(cb->state() == CastleBehavior::STATE_UNPACKED);
	CHECK(AudioApi::heldSound(AudioApi::HELD_CASTLE_BEING_BUILT, keep->getID()) == 0u);
	CHECK(AudioApi::heldSounds(AudioApi::HELD_CASTLE_BEING_BUILT) == 0u);

	// Destroying a member while BASE_BUILD is active stops the held loop in ~CastleMemberBehavior / onDelete (RW 0x797C40)
	Object *castle2 = f.make("CastlePad", f.teamOf("Alice"));
	Coord3D pos2{ 1600.0f, 1000.0f, 0.0f };
	castle2->setPosition(&pos2);
	castle2->friend_onBuildComplete();
	CastleBehavior *cb2 = CastleBehavior::of(*castle2);
	REQUIRE(cb2 != nullptr);
	f.frames(3);
	REQUIRE(cb2->state() == CastleBehavior::STATE_BUILDING);
	Object *keep2 = f.logic->findObjectByID(cb2->keepId());
	REQUIRE(keep2 != nullptr);
	// tolerance: valid AudioHandle is >= AHSV_FirstHandle
	CHECK(AudioApi::heldSound(AudioApi::HELD_CASTLE_BEING_BUILT, keep2->getID()) >= AHSV_FirstHandle);
	f.logic->destroyObject(keep2);
	f.frames(1);
	CHECK(AudioApi::heldSounds(AudioApi::HELD_CASTLE_BEING_BUILT) == 0u);

	// NoSound must not call AudioApi when no AudioManager is installed
	AudioApi::install(nullptr);
	const std::uint64_t withoutBefore = AudioApi::callsWithoutAudio();
	Object *nsKeep = f.make("NoSoundKeep", f.teamOf("Alice"));
	REQUIRE(nsKeep != nullptr);
	CastleMemberBehavior *nsCm = CastleMemberBehavior::of(*nsKeep);
	REQUIRE(nsCm != nullptr);
	nsCm->onParentModelConditionChanged(-1, Construction::modelConditionIndex("BASE_BUILD"));
	nsCm->onParentModelConditionChanged(Construction::modelConditionIndex("BASE_BUILD"), -1);
	CHECK(AudioApi::callsWithoutAudio() == withoutBefore);
}

TEST_CASE("PORT-231: ProductionUpdate::completeUnit script naming and entry + 0x3C hordeTarget migration (RW 0x8A253B / 0x8A2624 / 0x8A2A67 -> 0x8A171F -> 0x8A07E0)")
{
	Port231World f;
	f.load(
		"Object Sailor\n"
		"  BuildCost = 50\n"
		"  BuildTime = 1.0\n"
		"  KindOf = SELECTABLE INFANTRY\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"End\n"
		"Object SailorHorde\n"
		"  KindOf = HORDE SELECTABLE\n"
		"  Behavior = HordeContain ModuleTag_Contain\n"
		"    RankInfo = RankNumber:1 UnitType:Sailor Position:X:0 Y:0 Position:X:10 Y:0\n"
		"    InitialPayload = Sailor 2\n"
		"    Slots = 2\n"
		"  End\n"
		"End\n"
		"Object TroopShip\n"
		"  BuildCost = 100\n"
		"  BuildTime = 1.0\n"
		"  KindOf = SELECTABLE SHIP\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = HordeTransportContain ModuleTag_TC\n"
		"    ContainMax = 10\n"
		"    Slots = 10\n"
		"  End\n"
		"End\n"
		"Object QueuePort\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  CommandSet = PortSet\n"
		"  Behavior = ProductionUpdate ModuleTag_PU\n"
		"    MaxQueueEntries = 5\n"
		"  End\n"
		"  Behavior = QueueProductionExitUpdate ModuleTag_Q\n"
		"    UnitCreatePoint = X:0 Y:-20 Z:0\n"
		"    NaturalRallyPoint = X:30 Y:-50 Z:0\n"
		"  End\n"
		"  Behavior = ProductionQueueHordeContain ModuleTag_PortContain\n"
		"    ContainMax = 10\n"
		"    DestinationTemplate = TroopShip ANY +INFANTRY\n"
		"  End\n"
		"End\n"
		"CommandButton Cmd_BuildSailor\n  Command = UNIT_BUILD\n  Object = Sailor\nEnd\n"
		"CommandButton Cmd_BuildTroopShip\n  Command = UNIT_BUILD\n  Object = TroopShip\nEnd\n"
		"CommandSet PortSet\n  1 = Cmd_BuildSailor\n  2 = Cmd_BuildTroopShip\nEnd\n");

	Object *port = f.make("QueuePort", f.teamOf("Alice"));
	REQUIRE(port != nullptr);
	ProductionUpdateInterface *pu = port->getProductionUpdate();
	REQUIRE(pu != nullptr);
	const ThingTemplate *sailorT = f.w.get("Sailor");
	REQUIRE(sailorT != nullptr);

	// 1. Script naming (RW 0x8A2624): first produced unit registers name in ScriptEngine; second transfers name from first
	const ProductionID id1 = pu->requestUniqueUnitID();
	REQUIRE(pu->queueCreateUnit(sailorT, -1, id1, -1, false, "HeroBoat", false));
	f.frames(6);
	Object *firstNamed = f.logic->scriptEngine().getUnitNamed("HeroBoat");
	REQUIRE(firstNamed != nullptr);
	CHECK(firstNamed->getName() == "HeroBoat");

	const ProductionID id2 = pu->requestUniqueUnitID();
	REQUIRE(pu->queueCreateUnit(sailorT, -1, id2, -1, false, "HeroBoat", false));
	f.frames(6);
	Object *secondNamed = f.logic->scriptEngine().getUnitNamed("HeroBoat");
	REQUIRE(secondNamed != nullptr);
	CHECK(secondNamed->getID() != firstNamed->getID()); // tolerance: new unit received transferred name
	CHECK(secondNamed->getName() == "HeroBoat");
	CHECK(f.logic->scriptEngine().getUnitNamed("HeroBoat") == secondNamed);

	// Destroying the named object satisfies didUnitExist(), so the next unit transfers ownership as well
	const ObjectID prevId = secondNamed->getID();
	f.logic->destroyObject(secondNamed);
	f.frames(1);
	REQUIRE(f.logic->scriptEngine().getUnitNamed("HeroBoat") == nullptr);
	REQUIRE(f.logic->scriptEngine().didUnitExist("HeroBoat"));
	const ProductionID id3 = pu->requestUniqueUnitID();
	REQUIRE(pu->queueCreateUnit(sailorT, -1, id3, -1, false, "HeroBoat", false));
	f.frames(6);
	Object *thirdNamed = f.logic->scriptEngine().getUnitNamed("HeroBoat");
	REQUIRE(thirdNamed != nullptr);
	CHECK(thirdNamed->getID() != prevId);
	CHECK(thirdNamed->getName() == "HeroBoat");

	// 2. hordeTarget migration (RW 0x8A2A67 -> 0x8A171F -> 0x8A07E0): HORDE entering QueuePort migrates into completed TroopShip
	auto *portContain = dynamic_cast<ProductionQueueHordeContain *>(port->getContain());
	REQUIRE(portContain != nullptr);
	Object *horde = f.make("SailorHorde", f.teamOf("Alice"));
	REQUIRE(horde != nullptr);
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc != nullptr);
	const std::vector<Object *> initMembers(hc->getContainedItemsList()->begin(), hc->getContainedItemsList()->end());
	REQUIRE(initMembers.size() == 2u);
	for (Object *m : initMembers)
	{
		hc->releaseMemberForGarrison(m);
	}
	REQUIRE(portContain->addToContain(horde));
	for (Object *m : initMembers)
	{
		REQUIRE(portContain->addToContain(m));
	}
	REQUIRE(pu->getProductionCount() == 1);
	REQUIRE(pu->firstProduction()->hordeTarget == horde->getID());

	// Run frames until TroopShip completes and exits
	f.frames(6);
	CHECK(pu->getProductionCount() == 0);
	CHECK(portContain->getContainedItemsList()->empty());
	Object *ship = horde->getContainedBy();
	REQUIRE(ship != nullptr);
	CHECK(ship->getTemplate()->getName() == "TroopShip");
	for (Object *m : initMembers)
	{
		CHECK(m->getContainedBy() == horde);
	}
}

TEST_CASE("PORT-231: S-652 KeepSelectableWhenDead sole benefactor revival and S-1981 SellPercentage default 1.0f (RW 0x690626, 0x643160 -> 0x643222)")
{
	Port231World f;
	f.load(
		"Object KeepDeadStruct\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
		"  KeepSelectableWhenDead = Yes\n"
		"  BuildCost = 200\n"
		"  CommandSet = SellOnlySet\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"End\n"
		"Object HealerWorker\n"
		"  KindOf = DOZER INFANTRY\n"
		"End\n"
		"CommandButton Cmd_Sell\n  Command = SELL\nEnd\n"
		"CommandSet SellOnlySet\n  1 = Cmd_Sell\nEnd\n");

	// S-652: attemptHealingFromSoleBenefactor clears RUBBLE, sets REALLYDAMAGED and revives KeepSelectableWhenDead object
	Object *deadKeep = f.make("KeepDeadStruct", f.teamOf("Alice"));
	Object *worker = f.make("HealerWorker", f.teamOf("Alice"));
	REQUIRE(deadKeep != nullptr);
	REQUIRE(worker != nullptr);
	const int kRubble = Construction::modelConditionIndex("RUBBLE");
	const int kReallyDamaged = Construction::modelConditionIndex("REALLYDAMAGED");
	REQUIRE(kRubble != -1);
	REQUIRE(kReallyDamaged != -1);
	deadKeep->setModelConditionState(kRubble, true);
	deadKeep->friend_setEffectivelyDead(true);
	f.frames(1);

	REQUIRE(deadKeep->attemptHealingFromSoleBenefactor(10.0f, worker, 2));
	CHECK_FALSE(deadKeep->testModelCondition(kRubble));
	CHECK(deadKeep->testModelCondition(kReallyDamaged));
	CHECK_FALSE(deadKeep->isEffectivelyDead());

	// S-1981: unloaded SellPercentage uses GlobalData ctor default 1.0f (RW 0x643160 -> 0x643222) and notes no stop
	CHECK(f.logic->settings().sellPercentage == 1.0f);
	CHECK_FALSE(f.logic->settings().sellPercentageLoaded);
	Object *sellable = f.make("KeepDeadStruct", f.teamOf("Alice"));
	REQUIRE(sellable != nullptr);
	sellable->setBuildCostPaid(200.0f);
	const std::uint32_t moneyBefore = f.alice()->getMoney()->countMoney();
	f.logic->sellList().sellObject(sellable);
	f.frames(2);
	CHECK(f.alice()->getMoney()->countMoney() == moneyBefore + 200u);
	for (const std::string &stop : f.logic->report().stops)
	{
		CHECK(stop.rfind("[S-1981]", 0) != 0); // tolerance: prefix check for closed S-1981 stop
	}
}
