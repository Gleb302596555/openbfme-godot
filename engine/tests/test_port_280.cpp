// OpenBFME unit tests: PORT-280 (fork issue 647): ProductionSpeedBonus consumer across Player, BuildAssistant and ProductionUpdate.
//
// RotWK 2.01 facts (read with tools/mod/rw.py from game.dat):
//   * ProductionSpeedBonus::doSpecialPower (RW 0x8C7171, data table RW 0xC73620: NumberOfFrames +0x7C, SpeedMulitplier +0x80, Type +0x84):
//       factor = fstp_single(pc24Div(pc24Sub(mult, 1.0), pc24Mul(mult, -1.0)))  (RW 0x8C7191 .. 0x8C71B7, doubles RW 0xBD2C98 / 0xBDFC10);
//       for each Type name, calls controllingPlayer->setProductionSpeedBonus(name, factor, m_numberOfFrames) (RW 0x8C71B7 -> RW 0x6AF3C8);
//       then, only when getDisabledMask() == 0 (RW 0x8C71C7 .. 0x8C71D4), calls SpecialPowerModule::doSpecialPowerAtLocation(pos, options)
//       (RW 0x8C71E2 -> RW 0x89816C).
//   * Player maps: m_productionTimeChanges (+0x2B4), m_startingProductionTimeChanges (+0x2C8), m_productionTimeTimers (+0x2DC), case-sensitive
//     AsciiString keys (RW 0x42B6C1 / RW 0x4052F9):
//       - Player::init (RW 0x6B0239): copies PlayerTemplate::m_productionTimeChanges (+0x100, parsed by RW 0x5FD7D9) to +0x2B4 and +0x2C8 and sets
//         +0x2DC[name] = 0 (RW 0x6B05C9 .. 0x6B0634), or clears all three when pt is null (RW 0x6B06C9 .. 0x6B06E5).
//       - Player::setProductionSpeedBonus (RW 0x6AF3C8): inserts +0x2C8[name] = 0.0f only when absent; overwrites +0x2DC[name] = frames and
//         +0x2B4[name] = bonus.
//       - Player::removeProductionSpeedBonus (RW 0x6AF429): when name is in +0x2B4, erases name from +0x2C8, +0x2DC and +0x2B4.
//       - Player::getProductionTimeChange (RW 0x6AF484): returns +0x2B4[name] if present, else 0.0f.
//       - Player::update (RW 0x6AF269, timer walk RW 0x6AF28E .. 0x6AF2DD, called from PlayerList::update RW 0x6A84DB at phase 6): for each entry in
//         +0x2DC with timer != 0, decrements timer by 1; reaching 0 restores +0x2B4[name] = +0x2C8[name].
//       - Player::xfer (RW 0x6B205F, maps RW 0x6B2B8D .. 0x6B2BBB via RW 0x6AFD2D / RW 0x6AFE32): persists all three maps across save/load.
//   * BuildAssistant::calcTimeToBuild (RW 0x73C39E): reads player->getProductionTimeChange(tt.getName()) at RW 0x73C3EC and computes
//     trunc(pc24Mul(pc24Add(1.0f, pct), frames)); ProductionUpdate::update (RW 0x8A1B9F) recomputes totalProductionFrames (>= 1) every frame.
//     UpgradeTemplate::calcTimeToBuild (RW 0x66F1A8) does not consult +0x2B4.
//   * ProductionUpdate::update (RW 0x8A1B9F, speed-bonus block RW 0x8A1C20 .. 0x8A1D4F): when controllingPlayer is non-null:
//       - SetBonusModelConditionOnSpeedBonus (data +0x41, RW 0x8A1C30): toggles UPGRADE_ECONOMY_BONUS (bit 332) on bonus < 0.0f for BonusForType (+0x44).
//       - SpeedBonusAudioLoop (data +0x48, parsed by RW 0x73B217 -> RW 0x73AA94 where "NoSound" clears): when non-empty, keeps handle +0x12C playing
//         (object id + owning player index, RW 0x8A1CFB / RW 0x8A1D0D) iff bonus < 0.0f && !isEffectivelyDead() && !testStatus(UNDER_CONSTRUCTION),
//         else removes it (RW 0x8A1D35); dtor (RW 0x8A198B / RW 0x8A19FF) removes +0x12C; xfer (RW 0x8A3111 / RW 0x8A35A6) transfers it.
//
// Synthetic INI only; no retail or mod archives required. GPL-3.0.

#include "doctest.h"

#include "IniTestUtil.h"
#include "ProdTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/PlayerTemplate.h"
#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SpellStores.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{

std::uint32_t floatBits(float v)
{
	std::uint32_t u = 0;
	std::memcpy(&u, &v, sizeof(u));
	return u;
}

std::uint32_t playerHash(const Player &p)
{
	StateHasher h;
	p.crc(h);
	return h.value();
}

int findFlag(const char *const *names, const char *name)
{
	for (int i = 0; names[i]; ++i)
	{
		if (std::strcmp(names[i], name) == 0)
		{
			return i;
		}
	}
	FAIL_CHECK("unknown bit flag: " << name);
	return 0;
}

int modelConditionBit(const char *name)
{
	return findFlag(TheModelConditionNames, name);
}

unsigned objectStatusBit(const char *name)
{
	return (unsigned)findFlag(TheObjectStatusNames, name);
}

unsigned disabledBit(const char *name)
{
	static const char *const kDisabledNames[] = {
		"DEFAULT", "DISABLED_USER_PARALYZED", "DISABLED_EMP", "DISABLED_HELD", "DISABLED_PARALYZED", "DISABLED_UNMANNED",
		"DISABLED_UNDERPOWERED", "DISABLED_FREEFALL", "DISABLED_TEMPORARILY_BUSY", "DISABLED_SCRIPT_DISABLED",
		"DISABLED_SCRIPT_UNDERPOWERED", "DISABLED_USER_FROZEN", nullptr
	};
	return (unsigned)findFlag(kDisabledNames, name);
}

std::vector<std::uint8_t> makeWavMs(double ms)
{
	const std::uint32_t rate = 22050;
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

std::string bytesOf(const std::vector<std::uint8_t> &b)
{
	return std::string(b.begin(), b.end());
}

const char kAudioIni[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n"
	"  AmbientStreamFolder = AmbientStreams\n  SoundsExtension = wav\n  SampleCount2D = 4\n  SampleCount3D = 6\n  StreamCount = 3\n"
	"  MinSampleVolume = 2%\n  GlobalMinRange = 5000\n  GlobalMaxRange = 5000000\n  TimeToFadeAudio = 300\n"
	"  AmbientStreamHysteresisVolume = 10\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n"
	"  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent DefaultSoundEffect\n  Volume = 100\n  Priority = high\n  Limit = 4\n  MinRange = 160\n  MaxRange = 640\n"
	"  PitchShift = 0 0\n  PlayPercent = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"MusicTrack DefaultMusicTrack\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  SubmixSlider = music\nEnd\n"
	"DialogEvent DefaultDialog\n  Volume = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"AmbientStream DefaultAmbientStream\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  MinRange = 100\n  MaxRange = 1000\n"
	"  SubmixSlider = ambient\nEnd\n"
	"AudioEvent TestPitLoop\n  Control = loop\n  Sounds = pitloop\n  Type = world everyone\n  MaxRange = 5000\n  Limit = 4\nEnd\n"
	"AudioEvent TestOwnerPitLoop\n  Control = loop\n  Sounds = ownerloop\n  Type = world player\n  MaxRange = 5000\n  Limit = 4\nEnd\n";

struct LogicAudioOwners : AudioOwnerResolver
{
	const GameLogic *logic = nullptr;
	bool objectPosition(std::uint32_t objectId, Coord3D *pos) const override
	{
		const Object *o = logic ? logic->findObjectByID((ObjectID)objectId) : nullptr;
		if (!o || !pos)
		{
			return false;
		}
		*pos = *o->getPosition();
		return true;
	}
	bool drawablePosition(std::uint32_t, Coord3D *) const override { return false; }
};

struct LoopAudioRig
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> mgr;
	LogicAudioOwners owners;
	double t = 0.0;

	LoopAudioRig()
	{
		const std::string wav = bytesOf(makeWavMs(5000.0));
		fx.mount({
			{ "data\\audio\\sounds\\pitloop.wav", wav },
			{ "data\\audio\\sounds\\ownerloop.wav", wav },
			{ "data\\ini\\none.txt", "x" },
		});
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", kAudioIni);
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 4280u);
		mgr->setOwnerResolver(&owners);
		mgr->update(0.0);
		AudioApi::install(mgr.get());
	}
	~LoopAudioRig()
	{
		if (mgr)
		{
			mgr->clearOwnerResolver(&owners);
		}
		AudioApi::install(nullptr);
	}

	void bindLogic(const GameLogic &logic) { owners.logic = &logic; }

	void run(double ms)
	{
		const double end = t + ms;
		while (t < end)
		{
			t += 10.0;
			device->setTime(t);
			mgr->update(t);
		}
	}

	size_t startsOf(const std::string &needle) const
	{
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

const char kPort280Ini[] =
	"Rank 1\n  SkillPointsNeededDefault = 0\nEnd\n"
	"Rank 2\n  SkillPointsNeededDefault = 100\nEnd\n"
	"Upgrade Upgrade_TimedTest\n  Type = PLAYER\n  BuildCost = 50\n  BuildTime = 4.0\nEnd\n"
	"SpecialPower SP_DoubleSpeed\n  Enum = SPECIAL_SPELL_BOOK_CALL_THE_HORDE\n  ReloadTime = 10000\nEnd\n"
	"SpecialPower SP_WulfgarSpeed\n  Enum = SPECIAL_TRIGGER_ATTRIBUTE_MODIFIER\n  ReloadTime = 10000\nEnd\n"
	"SpecialPower SP_CallTheHorde20x\n  Enum = SPECIAL_SPELL_BOOK_CALL_THE_HORDE\n  ReloadTime = 10000\nEnd\n"
	"SpecialPower SP_QuadSpeed\n  Enum = SPECIAL_SPELL_BOOK_CALL_THE_HORDE\n  ReloadTime = 10000\nEnd\n"
	"SpecialPower SP_PermSpeed\n  Enum = SPECIAL_SPELL_BOOK_CALL_THE_HORDE\n  ReloadTime = 10000\nEnd\n"
	"SpecialPower SP_HalfSpeed\n  Enum = SPECIAL_SPELL_BOOK_CALL_THE_HORDE\n  ReloadTime = 10000\nEnd\n"
	"Object FastGrunt\n"
	"  BuildCost = 50\n"
	"  BuildTime = 10.0\n"
	"  CommandPoints = 2\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
	"End\n"
	"Object FastArcher\n"
	"  BuildCost = 60\n"
	"  BuildTime = 6.0\n"
	"  CommandPoints = 2\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
	"End\n"
	"Object HordeFighter\n"
	"  BuildCost = 100\n"
	"  BuildTime = 20.0\n"
	"  CommandPoints = 2\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
	"End\n"
	"Object TinyScout\n"
	"  BuildCost = 10\n"
	"  BuildTime = 1.0\n"
	"  CommandPoints = 1\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
	"End\n"
	"Object UntouchedPikeman\n"
	"  BuildCost = 50\n"
	"  BuildTime = 10.0\n"
	"  CommandPoints = 2\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
	"End\n"
	"Object SpeedCaster\n"
	"  KindOf = SELECTABLE HERO\n"
	"  Behavior = ProductionSpeedBonus ModuleTag_Double\n"
	"    SpecialPowerTemplate = SP_DoubleSpeed\n"
	"    AvailableAtStart = Yes\n"
	"    NumberOfFrames = 40\n"
	"    SpeedMulitplier = 2.0\n"
	"    Type = FastGrunt FastArcher\n"
	"  End\n"
	"  Behavior = ProductionSpeedBonus ModuleTag_Wulfgar\n"
	"    SpecialPowerTemplate = SP_WulfgarSpeed\n"
	"    AvailableAtStart = Yes\n"
	"    NumberOfFrames = 150\n"
	"    SpeedMulitplier = 15.0\n"
	"    Type = HordeFighter\n"
	"  End\n"
	"  Behavior = ProductionSpeedBonus ModuleTag_CallHorde\n"
	"    SpecialPowerTemplate = SP_CallTheHorde20x\n"
	"    AvailableAtStart = Yes\n"
	"    NumberOfFrames = 12\n"
	"    SpeedMulitplier = 20.0\n"
	"    Type = HordeFighter TinyScout FastGrunt\n"
	"  End\n"
	"  Behavior = ProductionSpeedBonus ModuleTag_Quad\n"
	"    SpecialPowerTemplate = SP_QuadSpeed\n"
	"    AvailableAtStart = Yes\n"
	"    NumberOfFrames = 8\n"
	"    SpeedMulitplier = 4.0\n"
	"    Type = FastGrunt\n"
	"  End\n"
	"  Behavior = ProductionSpeedBonus ModuleTag_Perm\n"
	"    SpecialPowerTemplate = SP_PermSpeed\n"
	"    AvailableAtStart = Yes\n"
	"    NumberOfFrames = 0\n"
	"    SpeedMulitplier = 2.0\n"
	"    Type = FastGrunt\n"
	"  End\n"
	"  Behavior = ProductionSpeedBonus ModuleTag_Slow\n"
	"    SpecialPowerTemplate = SP_HalfSpeed\n"
	"    AvailableAtStart = Yes\n"
	"    NumberOfFrames = 10\n"
	"    SpeedMulitplier = 0.5\n"
	"    Type = FastGrunt\n"
	"  End\n"
	"End\n"
	"Object BonusPit\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    MaxQueueEntries = 5\n"
	"    SetBonusModelConditionOnSpeedBonus = Yes\n"
	"    BonusForType = FastGrunt\n"
	"    SpeedBonusAudioLoop = TestPitLoop\n"
	"  End\n"
	"  Behavior = QueueProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:-20.0 Z:0.0\n"
	"    NaturalRallyPoint = X:30.0 Y:-40.0 Z:0.0\n"
	"  End\n"
	"End\n"
	"Object PlainBarracks\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  CommandSet = Port280BarracksSet\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    MaxQueueEntries = 5\n"
	"  End\n"
	"  Behavior = QueueProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:-20.0 Z:0.0\n"
	"    NaturalRallyPoint = X:30.0 Y:-40.0 Z:0.0\n"
	"  End\n"
	"End\n"
	"Object OtherTypePit\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    SetBonusModelConditionOnSpeedBonus = Yes\n"
	"    BonusForType = UntouchedPikeman\n"
	"  End\n"
	"End\n"
	"Object EmptyTypePit\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    SetBonusModelConditionOnSpeedBonus = Yes\n"
	"  End\n"
	"End\n"
	"Object FlagOffPit\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    SetBonusModelConditionOnSpeedBonus = No\n"
	"    BonusForType = FastGrunt\n"
	"  End\n"
	"End\n"
	"Object NoSoundPit\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    BonusForType = FastGrunt\n"
	"    SpeedBonusAudioLoop = nosound\n"
	"  End\n"
	"End\n"
	"Object OwnerOnlyLoopPit\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    BonusForType = FastGrunt\n"
	"    SpeedBonusAudioLoop = TestOwnerPitLoop\n"
	"  End\n"
	"End\n"
	"CommandButton Command_BuildFastGrunt\n  Command = UNIT_BUILD\n  Object = FastGrunt\n  Options = CANCELABLE\nEnd\n"
	"CommandButton Command_BuildFastArcher\n  Command = UNIT_BUILD\n  Object = FastArcher\n  Options = CANCELABLE\nEnd\n"
	"CommandButton Command_BuildHordeFighter\n  Command = UNIT_BUILD\n  Object = HordeFighter\n  Options = CANCELABLE\nEnd\n"
	"CommandButton Command_BuildTinyScout\n  Command = UNIT_BUILD\n  Object = TinyScout\n  Options = CANCELABLE\nEnd\n"
	"CommandButton Command_BuildUntouchedPikeman\n  Command = UNIT_BUILD\n  Object = UntouchedPikeman\n  Options = CANCELABLE\nEnd\n"
	"CommandButton Command_BuyTimedTest\n  Command = PLAYER_UPGRADE\n  Upgrade = Upgrade_TimedTest\n  Options = CANCELABLE\nEnd\n"
	"CommandSet Port280BarracksSet\n"
	"  1 = Command_BuildFastGrunt\n"
	"  2 = Command_BuildFastArcher\n"
	"  3 = Command_BuildHordeFighter\n"
	"  4 = Command_BuildTinyScout\n"
	"  5 = Command_BuildUntouchedPikeman\n"
	"  6 = Command_BuyTimedTest\n"
	"End\n";

struct Port280World : prodtest::ProdWorld
{
	SpellStores stores;

	Port280World()
		: stores(w.keys)
	{
		stores.install();
		SpellStores::registerBlocks(w.fx.env.blocks);
		SpecialPowerModules::registerAll(w.modules);
		SpellBookPowers::registerAll(w.modules);
		load(kPort280Ini);
		upgrades.assign(upgradeCenter);
		alice()->getMoney()->deposit(10000u);
		bob()->getMoney()->deposit(10000u);
	}
	~Port280World()
	{
		logic->reset();
		stores.uninstall();
	}

	Player *bob()
	{
		Player *p = players.findPlayerWithName("Bob");
		REQUIRE(p != nullptr);
		return p;
	}

	const ThingTemplate *tmpl(const char *name) const
	{
		const ThingTemplate *t = w.things.findTemplate(name);
		REQUIRE_MESSAGE(t != nullptr, name);
		return t;
	}

	Object *spawn(const char *name, Team *team, float x = 100.0f, float y = 100.0f)
	{
		Object *o = make(name, team);
		REQUIRE_MESSAGE(o != nullptr, name);
		const Coord3D pos{ x, y, 0.0f };
		o->setPosition(&pos);
		return o;
	}

	ProductionSpeedBonus *bonusPower(Object *caster, const char *moduleTag)
	{
		BehaviorModule *bm = caster->findModuleByTag(w.keys.nameToKey(moduleTag));
		REQUIRE_MESSAGE(bm != nullptr, moduleTag);
		auto *pb = dynamic_cast<ProductionSpeedBonus *>(bm);
		REQUIRE_MESSAGE(pb != nullptr, moduleTag);
		return pb;
	}

	ProductionUpdate *prodModule(Object *producer)
	{
		auto *p = dynamic_cast<ProductionUpdate *>(producer->getProductionUpdate());
		REQUIRE(p != nullptr);
		return p;
	}

	int countObjects(const char *templateName, const Player *owner) const
	{
		int n = 0;
		for (const Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getTemplate()->getName() == templateName && o->getControllingPlayer() == owner)
			{
				++n;
			}
		}
		return n;
	}

	int buildFrames(const char *unitName, const Object *producer, const Player *p) const
	{
		return BuildAssistant::calcTimeToBuild(*tmpl(unitName), p, producer, -1, logic->productionSettings(), *logic);
	}
};

} // namespace

TEST_CASE("PORT-280 end-to-end: ProductionSpeedBonus cuts calcTimeToBuild and queue duration per player and expires after NumberOfFrames")
{
	Port280World f;
	Object *caster = f.spawn("SpeedCaster", f.teamOf("Alice"));
	Object *barracks = f.spawn("PlainBarracks", f.teamOf("Alice"), 150.0f, 100.0f);
	Object *bobBarracks = f.spawn("PlainBarracks", f.teamOf("Bob"), 300.0f, 100.0f);

	// Baseline: 5 logic frames per second (FastGrunt 10s = 50f, FastArcher 6s = 30f, UntouchedPikeman 10s = 50f)
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 50);
	CHECK(f.buildFrames("FastArcher", barracks, f.alice()) == 30);
	CHECK(f.buildFrames("UntouchedPikeman", barracks, f.alice()) == 50);

	// Cast 2.0x for 40 frames on FastGrunt + FastArcher: (2.0 - 1.0) / (2.0 * -1.0) = -0.5f (0xBF000000).
	// ProductionSpeedBonus itself adds 0 unported increments (only SpecialPowerModule::doSpecialPowerAtLocation's single base-trigger stop fires).
	ProductionSpeedBonus *dbl = f.bonusPower(caster, "ModuleTag_Double");
	dbl->doSpecialPower(0);
	CHECK(dbl->unportedEffects() == 1);
	CHECK(floatBits(f.alice()->getProductionTimeChange("FastGrunt")) == 0xBF000000u);
	CHECK(floatBits(f.alice()->getProductionTimeChange("FastArcher")) == 0xBF000000u);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 40);
	CHECK(f.alice()->getProductionTimeTimer("FastArcher") == 40);
	CHECK(f.alice()->getProductionTimeChange("UntouchedPikeman") == 0.0f);
	CHECK(f.alice()->getProductionTimeChange("fastgrunt") == 0.0f); // RW 0x42B6C1 / RW 0x4052F9: AsciiString keys are case-sensitive

	// Per-player isolation: Bob is unaffected
	CHECK(f.bob()->getProductionTimeChange("FastGrunt") == 0.0f);
	CHECK(f.buildFrames("FastGrunt", bobBarracks, f.bob()) == 50);

	// Alice's build times halve for the two listed templates only
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 25);
	CHECK(f.buildFrames("FastArcher", barracks, f.alice()) == 15);
	CHECK(f.buildFrames("UntouchedPikeman", barracks, f.alice()) == 50);

	// Real ProductionUpdate queue: FastGrunt finishes on frame 25 instead of frame 50, while Bob's stays mid-build
	ProductionUpdate *alicePu = f.prodModule(barracks);
	ProductionUpdate *bobPu = f.prodModule(bobBarracks);
	REQUIRE(alicePu->queueCreateUnit(f.tmpl("FastGrunt"), -1, alicePu->requestUniqueUnitID(), 0, false, "", false));
	REQUIRE(bobPu->queueCreateUnit(f.tmpl("FastGrunt"), -1, bobPu->requestUniqueUnitID(), 0, false, "", false));

	f.frames(24);
	CHECK(f.countObjects("FastGrunt", f.alice()) == 0);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 16);

	f.frames(1); // frame 25: Alice's bonused grunt spawns; Bob's still has 25 frames left
	CHECK(f.countObjects("FastGrunt", f.alice()) == 1);
	CHECK(f.countObjects("FastGrunt", f.bob()) == 0);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 15);

	// Frame 39 after cast: 1 tick left, bonus still live
	f.frames(14);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 1);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 25);

	// Frame 40 after cast (RW 0x6AF29D .. 0x6AF2C8): timer hits 0 and restores the 0.0f starting baseline
	f.frames(1);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == 0.0f);
	CHECK(f.alice()->getProductionTimeChange("FastArcher") == 0.0f);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 50);
	CHECK(f.buildFrames("FastArcher", barracks, f.alice()) == 30);

	// Ennorath exact PC24 factors (RW 0x73C3F3 truncates whole seconds before multiplying by LOGICFRAMES_PER_SECOND = 5):
	// Wulfgar 15x -> -14/15 (0xBF6EEEEF), HordeFighter 20s -> trunc(1.3333s) = 1s = 5f;
	// Call the Horde 20x -> -0.95f (0xBF733333), HordeFighter 20s -> trunc(1.0s) = 1s = 5f, TinyScout 1s -> trunc(0.05s) = 0f clamped to 1 frame in queue
	f.bonusPower(caster, "ModuleTag_Wulfgar")->doSpecialPower(0);
	CHECK(floatBits(f.alice()->getProductionTimeChange("HordeFighter")) == 0xBF6EEEEFu);
	CHECK(f.buildFrames("HordeFighter", barracks, f.alice()) == 5);

	f.bonusPower(caster, "ModuleTag_CallHorde")->doSpecialPower(0);
	CHECK(floatBits(f.alice()->getProductionTimeChange("HordeFighter")) == 0xBF733333u);
	CHECK(f.buildFrames("HordeFighter", barracks, f.alice()) == 5);
	CHECK(f.buildFrames("TinyScout", barracks, f.alice()) == 0);
	REQUIRE(alicePu->queueCreateUnit(f.tmpl("TinyScout"), -1, alicePu->requestUniqueUnitID(), 0, false, "", false));
	f.frames(1);
	CHECK(f.countObjects("TinyScout", f.alice()) == 1);
}

TEST_CASE("PORT-280 in-flight production: casting mid-build accelerates queued unit and expiry mid-build restores remaining time")
{
	Port280World f;
	Object *barracks = f.spawn("PlainBarracks", f.teamOf("Alice"), 150.0f, 100.0f);
	ProductionUpdate *pu = f.prodModule(barracks);

	// Queue unbonused FastGrunt (50 frames) and build 10 frames (progress 10 / 50)
	REQUIRE(pu->queueCreateUnit(f.tmpl("FastGrunt"), -1, pu->requestUniqueUnitID(), 0, false, "", false));
	f.frames(10);
	CHECK(f.countObjects("FastGrunt", f.alice()) == 0);

	// Cast 2.0x (target becomes 25 frames): remaining 15 frames complete the in-flight entry (RW 0x8A1EBC recomputes target each frame)
	f.alice()->setProductionSpeedBonus("FastGrunt", -0.5f, 20);
	f.frames(14);
	CHECK(f.countObjects("FastGrunt", f.alice()) == 0);
	f.frames(1);
	CHECK(f.countObjects("FastGrunt", f.alice()) == 1);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 5);

	// Queue a second FastGrunt with 5 frames of 2.0x left: after 5 frames timer expires, target returns to 50f, finishes on frame 50
	REQUIRE(pu->queueCreateUnit(f.tmpl("FastGrunt"), -1, pu->requestUniqueUnitID(), 0, false, "", false));
	f.frames(5);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 50);
	f.frames(44); // 49 frames built of 50
	CHECK(f.countObjects("FastGrunt", f.alice()) == 1);
	f.frames(1); // 50th frame spawns
	CHECK(f.countObjects("FastGrunt", f.alice()) == 2);
}

TEST_CASE("PORT-280 PlayerTemplate baseline, recast overwrite, permanent timer 0, slowdown, disabled caster, remove and upgrade isolation")
{
	Port280World f;

	// Seed a PlayerTemplate static ProductionTimeChange (-20% -> FastGrunt 50f becomes 40f) and re-init Alice (RW 0x6B05C9 .. 0x6B0634)
	PlayerTemplate faction;
	faction.m_productionTimeChanges["FastGrunt"] = -0.2f;
	Team *aliceTeam = f.alice()->getDefaultTeam();
	f.alice()->init(&faction, 10000u);
	f.alice()->setDefaultTeam(aliceTeam);
	f.economy().initAllCommandPoints();

	Object *caster = f.spawn("SpeedCaster", aliceTeam);
	Object *barracks = f.spawn("PlainBarracks", aliceTeam, 150.0f, 100.0f);
	ProductionUpdate *pu = f.prodModule(barracks);

	// Static template modifier has timer 0 and never decays on Player::update (RW 0x6AF297)
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.2f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 40);
	f.frames(10);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.2f);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 40);

	// Timed 2.0x (-0.5f, 10f) then mid-timer recast 4.0x (-0.75f, 8f) overwrite +0x2B4/+0x2DC while keeping +0x2C8 == -0.2f (RW 0x6AF3E7)
	f.alice()->setProductionSpeedBonus("FastGrunt", -0.5f, 10);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 25);
	f.frames(6);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 4);

	f.bonusPower(caster, "ModuleTag_Quad")->doSpecialPower(0);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.75f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 8);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 10); // RW 0x73C3F3: trunc(0.25 * 10s) = 2s -> 10 frames

	f.frames(7);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.75f);
	f.frames(1); // 8th frame restores PlayerTemplate's -0.2f baseline, not 0.0f
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.2f);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 40);

	// NumberOfFrames = 0 sets a permanent bonus (timer 0 never expires); removeProductionSpeedBonus (RW 0x6AF429) erases all three entries
	f.bonusPower(caster, "ModuleTag_Perm")->doSpecialPower(0);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.5f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	f.frames(20);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.5f);

	f.alice()->removeProductionSpeedBonus("NeverExisted"); // absent key is a no-op
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.5f);
	f.alice()->removeProductionSpeedBonus("FastGrunt");
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == 0.0f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 50);

	// SpeedMulitplier = 0.5 -> (0.5 - 1.0) / -0.5 = +1.0f slows build time from 50f to 100f
	f.bonusPower(caster, "ModuleTag_Slow")->doSpecialPower(0);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == 1.0f);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 100);
	f.frames(10);
	CHECK(f.buildFrames("FastGrunt", barracks, f.alice()) == 50);

	// Disabled caster (RW 0x8C71B7 runs before RW 0x8C71C7): bonus is recorded on Player, base doSpecialPowerAtLocation (reload timer) is skipped,
	// and ProductionSpeedBonus adds 0 unported effects across both Type entries
	ProductionSpeedBonus *dbl = f.bonusPower(caster, "ModuleTag_Double");
	const unsigned readyBefore = dbl->getReadyFrame();
	caster->setDisabled(disabledBit("DISABLED_EMP"), f.logic->getFrame() + 100u);
	dbl->doSpecialPower(0);
	CHECK(dbl->unportedEffects() == 0);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.5f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 40);
	CHECK(dbl->getReadyFrame() == readyBefore);

	// Upgrade production uses UpgradeTemplate::calcTimeToBuild (RW 0x66F1A8: 4.0s = 20 frames) and ignores Player +0x2B4
	f.alice()->setProductionSpeedBonus("Upgrade_TimedTest", -0.95f, 100);
	const UpgradeTemplate *upg = f.upgradeCenter.findUpgrade("Upgrade_TimedTest");
	REQUIRE(upg != nullptr);
	REQUIRE(pu->queueUpgrade(upg));
	f.frames(19);
	CHECK_FALSE(f.alice()->hasUpgradeComplete(upg));
	f.frames(1);
	CHECK(f.alice()->hasUpgradeComplete(upg));
}

TEST_CASE("PORT-280 ProductionUpdate SetBonusModelConditionOnSpeedBonus toggles UPGRADE_ECONOMY_BONUS for BonusForType")
{
	Port280World f;
	const int econBonusBit = modelConditionBit("UPGRADE_ECONOMY_BONUS");
	CHECK(econBonusBit == 332); // RW 0x8A1C59 / RW 0x8A1C6B: word 10 mask 0x1000

	Object *caster = f.spawn("SpeedCaster", f.teamOf("Alice"));
	Object *matchPit = f.spawn("BonusPit", f.teamOf("Alice"), 150.0f, 100.0f);
	Object *otherPit = f.spawn("OtherTypePit", f.teamOf("Alice"), 200.0f, 100.0f);
	Object *emptyPit = f.spawn("EmptyTypePit", f.teamOf("Alice"), 250.0f, 100.0f);
	Object *flagOffPit = f.spawn("FlagOffPit", f.teamOf("Alice"), 300.0f, 100.0f);
	Object *bobPit = f.spawn("BonusPit", f.teamOf("Bob"), 350.0f, 100.0f);
	Object *ownerlessPit = f.logic->newObject(f.tmpl("BonusPit"), nullptr, ObjectStatusMaskType{});
	REQUIRE(ownerlessPit != nullptr);

	f.frames(1);
	CHECK_FALSE(matchPit->testModelCondition(econBonusBit));
	CHECK_FALSE(otherPit->testModelCondition(econBonusBit));
	CHECK_FALSE(emptyPit->testModelCondition(econBonusBit));
	CHECK_FALSE(flagOffPit->testModelCondition(econBonusBit));
	CHECK_FALSE(bobPit->testModelCondition(econBonusBit));
	CHECK_FALSE(ownerlessPit->testModelCondition(econBonusBit));

	// Cast 20x for 12 frames on FastGrunt: only Alice's matching BonusPit lights UPGRADE_ECONOMY_BONUS
	f.bonusPower(caster, "ModuleTag_CallHorde")->doSpecialPower(0);
	f.frames(1); // phase 6 decrements 12 -> 11; phase 7 updates ProductionUpdate with bonus = -0.95f < 0
	CHECK(matchPit->testModelCondition(econBonusBit));
	CHECK_FALSE(otherPit->testModelCondition(econBonusBit));
	CHECK_FALSE(emptyPit->testModelCondition(econBonusBit));
	CHECK_FALSE(flagOffPit->testModelCondition(econBonusBit));
	CHECK_FALSE(bobPit->testModelCondition(econBonusBit));
	CHECK_FALSE(ownerlessPit->testModelCondition(econBonusBit));

	// Full 12 bonused frames light UPGRADE_ECONOMY_BONUS; phase 6 of frame 12 expires timer to 0, and frame 13's ProductionUpdate::update clears the bit
	f.frames(11);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	CHECK(matchPit->testModelCondition(econBonusBit));
	f.frames(1);
	CHECK_FALSE(matchPit->testModelCondition(econBonusBit));

	// A slowdown (bonus = +1.0f > 0) does not set UPGRADE_ECONOMY_BONUS (RW 0x8A1C50 tests bonus < 0.0f)
	f.bonusPower(caster, "ModuleTag_Slow")->doSpecialPower(0);
	f.frames(1);
	CHECK_FALSE(matchPit->testModelCondition(econBonusBit));
}

TEST_CASE("PORT-280 ProductionUpdate SpeedBonusAudioLoop: start, single hold, construction/death gates, NoSound, player ownership and dtor")
{
	LoopAudioRig audio;
	Port280World f;
	audio.bindLogic(*f.logic);
	const unsigned underConstruction = objectStatusBit("UNDER_CONSTRUCTION");

	Object *caster = f.spawn("SpeedCaster", f.teamOf("Alice"));
	Object *pit = f.spawn("BonusPit", f.teamOf("Alice"), 150.0f, 100.0f);
	Object *silent = f.spawn("PlainBarracks", f.teamOf("Alice"), 200.0f, 100.0f);
	Object *noSound = f.spawn("NoSoundPit", f.teamOf("Alice"), 250.0f, 100.0f);
	ProductionUpdate *pitPu = f.prodModule(pit);
	ProductionUpdate *silentPu = f.prodModule(silent);
	ProductionUpdate *noSoundPu = f.prodModule(noSound);

	// SpeedBonusAudioLoop = nosound cleared data + 0x48 at parse time (RW 0x73B217 -> RW 0x73AA94)
	const auto *noSoundData = dynamic_cast<const ProductionUpdateModuleData *>(noSoundPu->getModuleData());
	REQUIRE(noSoundData != nullptr);
	CHECK(noSoundData->m_speedBonusAudioLoop.empty());

	f.frames(1);
	audio.run(50.0);
	CHECK_FALSE(pitPu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pitPu->speedBonusAudioKey()) == 0);
	CHECK(audio.startsOf("pitloop") == 0);

	// Activate speed bonus (12 frames): BonusPit starts TestPitLoop once and keeps the handle; PlainBarracks & NoSoundPit stay silent
	f.bonusPower(caster, "ModuleTag_CallHorde")->doSpecialPower(0);
	f.frames(1);
	audio.run(50.0);
	CHECK(pitPu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pitPu->speedBonusAudioKey()) != 0); // tolerance: audio handles are opaque non-zero ids
	CHECK(audio.startsOf("pitloop") == 1);
	CHECK_FALSE(silentPu->speedBonusAudioHeld());
	CHECK_FALSE(noSoundPu->speedBonusAudioHeld());

	// Subsequent active frames do not re-add the loop while it is playing (RW 0x8A1CE3 -> RW 0x8A1D33)
	f.frames(3);
	audio.run(150.0);
	CHECK(pitPu->speedBonusAudioHeld());
	CHECK(audio.startsOf("pitloop") == 1);

	// UNDER_CONSTRUCTION (RW 0x8A1CD3) stops the loop; finishing construction restarts it while the bonus remains active
	pit->setStatus(underConstruction, true);
	f.frames(1);
	audio.run(50.0);
	CHECK_FALSE(pitPu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pitPu->speedBonusAudioKey()) == 0);

	pit->setStatus(underConstruction, false);
	f.frames(1);
	audio.run(50.0);
	CHECK(pitPu->speedBonusAudioHeld());
	CHECK(audio.startsOf("pitloop") == 2);

	// Effectively dead (RW 0x8A1CC6) stops the loop; clearing restores it
	pit->friend_setEffectivelyDead(true);
	f.frames(1);
	audio.run(50.0);
	CHECK_FALSE(pitPu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pitPu->speedBonusAudioKey()) == 0);

	pit->friend_setEffectivelyDead(false);
	f.frames(1);
	audio.run(50.0);
	CHECK(pitPu->speedBonusAudioHeld());
	CHECK(audio.startsOf("pitloop") == 3);

	// Phase 6 of frame 12 (4 frames remain) resets the bonus to 0.0f; frame 13's ProductionUpdate::update removes the loop handle (RW 0x8A1D35)
	f.frames(4);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);
	f.frames(1);
	audio.run(50.0);
	CHECK_FALSE(pitPu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pitPu->speedBonusAudioKey()) == 0);

	// Destroying a producer while its loop is held removes the handle in ~ProductionUpdate (RW 0x8A19FF)
	f.alice()->setProductionSpeedBonus("FastGrunt", -0.5f, 30);
	Object *doomed = f.spawn("BonusPit", f.teamOf("Alice"), 150.0f, 200.0f);
	const std::uint32_t doomedKey = f.prodModule(doomed)->speedBonusAudioKey();
	f.frames(1);
	audio.run(50.0);
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, doomedKey) != 0); // tolerance: audio handles are opaque non-zero ids
	f.logic->destroyObject(doomed);
	f.frames(1);
	audio.run(50.0);
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, doomedKey) == 0);

	// RW 0x8A1D0D forwards controllingPlayer->getPlayerIndex() into AudioEventRTS: a `world player` loop plays for Alice and is culled for Bob
	int localIdx = f.bob()->getPlayerIndex();
	AudioWorldQueries q;
	q.localPlayerIndex = [&] { return localIdx; };
	q.playerExists = [&](int idx) { return idx >= 0 && idx < f.players.getPlayerCount(); };
	q.relationship = [](int a, int b) { return a == b ? AR_ALLIES : AR_ENEMIES; };
	audio.mgr->setWorldQueries(q);

	Object *ownerPit = f.spawn("OwnerOnlyLoopPit", f.teamOf("Alice"), 150.0f, 260.0f);
	REQUIRE(ownerPit != nullptr);
	f.frames(1);
	audio.run(50.0);
	CHECK(audio.startsOf("ownerloop") == 0);

	localIdx = f.alice()->getPlayerIndex();
	f.frames(1);
	audio.run(50.0);
	CHECK(audio.startsOf("ownerloop") == 1);
}

TEST_CASE("PORT-280 snapshot xfer and CRC: untouched compatibility, active Player timers, duplicate key check and ProductionUpdate loop restore")
{
	LoopAudioRig audio;
	Port280World f;
	audio.bindLogic(*f.logic);

	PlayerTemplate faction;
	faction.m_productionTimeChanges["FastGrunt"] = -0.2f;
	Team *aliceTeam = f.alice()->getDefaultTeam();
	f.alice()->init(&faction, 10000u);
	f.alice()->setDefaultTeam(aliceTeam);
	f.economy().initAllCommandPoints();

	// Untouched player saves version 2 and restores initial CRC once a timed bonus finishes
	const std::uint32_t untouchedCrc = playerHash(*f.alice());
	XferSave untouchedSave;
	f.alice()->xfer(untouchedSave, f.teams);
	REQUIRE_FALSE(untouchedSave.data().empty());
	CHECK(untouchedSave.data()[0] == 2);

	f.alice()->setProductionSpeedBonus("FastGrunt", -0.5f, 9);
	f.alice()->setProductionSpeedBonus("FastArcher", -0.75f, 4);
	const std::uint32_t activeCrc = playerHash(*f.alice());
	CHECK(activeCrc != untouchedCrc); // tolerance: StateHasher distinguishes active bonus state from untouched baseline

	XferSave activeSave;
	f.alice()->xfer(activeSave, f.teams);
	REQUIRE_FALSE(activeSave.data().empty());
	CHECK(activeSave.data()[0] == 5);

	// Advance past both expirations: maps return to faction baseline, CRC returns to untouchedCrc and save version returns to 2
	f.frames(9);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.2f);
	CHECK(f.alice()->getProductionTimeChange("FastArcher") == 0.0f);
	f.alice()->removeProductionSpeedBonus("FastArcher");
	CHECK(playerHash(*f.alice()) == untouchedCrc);
	XferSave postClearSave;
	f.alice()->xfer(postClearSave, f.teams);
	CHECK(postClearSave.data()[0] == 2);

	// Reload active v5 snapshot: both timers, deltas and FastGrunt's -0.2f baseline come back and finish on their exact remaining ticks
	{
		XferLoad load(activeSave.data());
		f.alice()->xfer(load, f.teams);
	}
	CHECK(playerHash(*f.alice()) == activeCrc);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.5f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 9);
	CHECK(f.alice()->getProductionTimeChange("FastArcher") == -0.75f);
	CHECK(f.alice()->getProductionTimeTimer("FastArcher") == 4);

	f.frames(4);
	CHECK(f.alice()->getProductionTimeChange("FastArcher") == 0.0f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 5);
	f.frames(5);
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.2f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);

	// Loading a pre-v5 snapshot over an active bonus resets maps to m_template
	f.alice()->setProductionSpeedBonus("FastGrunt", -0.9f, 50);
	{
		XferLoad load(untouchedSave.data());
		f.alice()->xfer(load, f.teams);
	}
	CHECK(f.alice()->getProductionTimeChange("FastGrunt") == -0.2f);
	CHECK(f.alice()->getProductionTimeTimer("FastGrunt") == 0);

	// Corrupt v5 stream with duplicate key in m_productionTimeChanges throws XferException
	{
		f.alice()->setProductionSpeedBonus("DupA", -0.5f, 5);
		f.alice()->setProductionSpeedBonus("DupB", -0.5f, 5);
		XferSave dupSave;
		f.alice()->xfer(dupSave, f.teams);
		std::vector<std::uint8_t> bytes = dupSave.data();
		bool patched = false;
		for (size_t i = 0; i + 4 <= bytes.size(); ++i)
		{
			if (std::memcmp(&bytes[i], "DupB", 4) == 0)
			{
				bytes[i + 3] = 'A';
				patched = true;
				break;
			}
		}
		REQUIRE(patched);
		XferLoad bad(bytes);
		CHECK_THROWS_AS(f.alice()->xfer(bad, f.teams), XferException);
		f.alice()->removeProductionSpeedBonus("DupA");
		f.alice()->removeProductionSpeedBonus("DupB");
	}

	// Clear FastGrunt's static -0.2f template baseline so BonusPit starts unbonused, then verify v1 untouched, v2 while held, and load restore / stop
	f.alice()->removeProductionSpeedBonus("FastGrunt");
	Object *pit = f.spawn("BonusPit", aliceTeam, 150.0f, 100.0f);
	ProductionUpdate *pu = f.prodModule(pit);

	XferSave silentSave;
	pu->xfer(silentSave);
	REQUIRE_FALSE(silentSave.data().empty());
	CHECK(silentSave.data()[0] == 1);

	f.alice()->setProductionSpeedBonus("FastGrunt", -0.5f, 3);
	f.frames(1);
	audio.run(50.0);
	REQUIRE(pu->speedBonusAudioHeld());

	XferSave heldSave;
	pu->xfer(heldSave);
	REQUIRE_FALSE(heldSave.data().empty());
	CHECK(heldSave.data()[0] == 2);

	// Bonus ends and loop stops; loading heldSave brings the held loop back
	f.frames(3);
	audio.run(50.0);
	REQUIRE_FALSE(pu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pu->speedBonusAudioKey()) == 0);

	const size_t startsBefore = audio.startsOf("pitloop");
	{
		XferLoad load(heldSave.data());
		pu->xfer(load);
	}
	CHECK(pu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pu->speedBonusAudioKey()) != 0); // tolerance: audio handles are opaque non-zero ids
	audio.run(50.0);
	CHECK(audio.startsOf("pitloop") == startsBefore + 1);

	// Loading the v1 silent snapshot drops the held loop cleanly
	{
		XferLoad load(silentSave.data());
		pu->xfer(load);
	}
	CHECK_FALSE(pu->speedBonusAudioHeld());
	CHECK(AudioApi::heldSound(AudioApi::HELD_PRODUCTION_SPEED_BONUS_LOOP, pu->speedBonusAudioKey()) == 0);
}
