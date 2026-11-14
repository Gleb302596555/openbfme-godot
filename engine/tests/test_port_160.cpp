// OpenBFME unit tests for PORT-160 (issue #378): the EVA that tells the local player that a building is being taken, that it was taken, and
// that its own camp fell — P-91 of docs/ennorath/PROBLEMS.md ("no voices when a building is captured, being captured or lost") plus the
// camp-destroyed events of CastleMemberBehavior.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001):
//   * the predefined event names and their indices are the table at RW 0xBF2168 (ported in Common/Audio/EvaEvents.cpp, read with rw.py by
//     lane AUDIO-2): index 16 "BuildingBeingStolen", 17 "BuildingStolen", 9 "CampDestroyed", 0xA "AllyCampDestroyed", 8
//     "EnemyCampDestroyed"; TheEva reports and resolves them by name (RW 0x5DD9EE) and plays the record's side sound.
//   * a camp's abandonment reports the events its CastleMemberBehavior data names (+ 0x08 / + 0x0C / + 0x10, parser RW 0x797F69): RW
//     0x79CB47 -> RW 0x79CB5C calls RW 0x799440, whose readers are RW 0x79951F (owner), RW 0x799560 (ally) and RW 0x7995C2 (attacker),
//     chosen against the local player, and RW 0x7995DB reports the chosen one at the castle (batch-10 rows 5-7, read from game.dat by the
//     2026-11-09 audit).
//   * the capture reports, as the BFME2 1.06 decompilation has them (DONOR): "BuildingBeingStolen" when the preparation starts on a locally
//     controlled target (Object/Update/SpecialAbilityUpdate.cpp:1347 for type 0x1D, :1364 for 0x1A, whose enum values 0x10 / 17 are the
//     table's indices) and "BuildingStolen" when the capture lands on one (Object/Update/SpecialAbilityUpdateTriggerAbilityEffect.cpp:784,
//     EVA_CAPTURED = 17), both before the team changes. INFERENCE: the RW 2.01 instructions inside RW 0x853D31 .. 0x853ED0 and RW 0x8544F2 ..
//     0x854772 were not read in this session's sandbox (no game.dat); see docs/ennorath/PORT160_RW_ANALYSIS.md.
//
// Synthetic data only: no RotWK or Ennorath file is mounted. What TheEva accepted and played is read back from its own records
// (`Eva::lastPlayedPosition`: the index of RW 0xBF2168 and the position the report carried) and from `Eva::counters()`.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameClient/Eva.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"

#include <memory>
#include <string>

namespace
{
// Audio.ini: the default record and the five events, each with its own SideSound for "Alpha" (the fixture's FactionA). The records are named
// and indexed as RW 0xBF2168 has them, so a played index names the event without depending on the sound files.
const char kPort160Audio[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n"
	"  SoundsExtension = wav\n  SampleCount2D = 4\n  SampleCount3D = 4\n  StreamCount = 3\n  MinSampleVolume = 2%\n  DefaultSoundVolume = 100%\n"
	"  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent EvaDefaultSound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaCampOwnerSound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaCampAllySound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaCampAttackerSound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaBeingStolenSound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaStolenSound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"PredefinedEvaEvent DefaultEvaEvent\n  Priority = 5\n  TimeBetweenEventsMS = 20000\n  ExpirationTimeMS = 1500\n"
	"  SideSound\n    Side = Alpha\n    Sound = EvaDefaultSound\n  End\nEnd\n"
	"PredefinedEvaEvent BuildingBeingStolen\n  Priority = 7\n  TimeBetweenEventsMS = 20000\n  AlwaysPlayFromHomeBase = Yes\n"
	"  SideSound\n    Side = Alpha\n    Sound = EvaBeingStolenSound\n  End\nEnd\n"
	"PredefinedEvaEvent BuildingStolen\n  Priority = 6\n  TimeBetweenEventsMS = 20000\n  AlwaysPlayFromHomeBase = Yes\n"
	"  SideSound\n    Side = Alpha\n    Sound = EvaStolenSound\n  End\nEnd\n"
	"PredefinedEvaEvent CampDestroyed\n  Priority = 8\n  TimeBetweenEventsMS = 20000\n"
	"  SideSound\n    Side = Alpha\n    Sound = EvaCampOwnerSound\n  End\nEnd\n"
	"PredefinedEvaEvent AllyCampDestroyed\n  Priority = 6\n  TimeBetweenEventsMS = 20000\n"
	"  SideSound\n    Side = Alpha\n    Sound = EvaCampAllySound\n  End\nEnd\n"
	"PredefinedEvaEvent EnemyCampDestroyed\n  Priority = 4\n  TimeBetweenEventsMS = 20000\n"
	"  SideSound\n    Side = Alpha\n    Sound = EvaCampAttackerSound\n  End\nEnd\n";

// The camp flag (its CastleMemberBehavior writes out the RW defaults 9 / 0xA / 8 as the names of RW 0xBF2168, so nothing rests on an unset
// string), the keep of its castle, the capture ability and the two buildings the mechanisms act on.
const char kPort160Objects[] =
	"Object PCampFlag\n"
	"  KindOf = IMMOBILE CASTLE_CENTER\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
	"    CampDestroyedOwnerEvaEvent = CampDestroyed\n"
	"    CampDestroyedAllyEvaEvent = AllyCampDestroyed\n"
	"    CampDestroyedAttackerEvaEvent = EnemyCampDestroyed\n"
	"  End\n"
	"  Behavior = CastleBehavior ModuleTag_castle\n"
	"    CastleToUnpackForFaction = Alpha P160Camp\n"
	"    InstantUnpack = Yes\n"
	"    FadeTime = 0.2\n"
	"  End\n"
	"End\n"
	"Object P160Keep\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE CASTLE_KEEP\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
	"  End\n"
	"End\n"
	"SpecialPower SpecialPowerCaptureInfantry\n"
	"  Enum = SPECIAL_INFANTRY_CAPTURE_BUILDING\n"
	"  ReloadTime = 0\n"
	"  ObjectFilter = ANY +STRUCTURE\n"
	"End\n"
	"Object P160Capturer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Power\n"
	"    SpecialPowerTemplate = SpecialPowerCaptureInfantry\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_Ability\n"
	"    SpecialPowerTemplate = SpecialPowerCaptureInfantry\n"
	"    PreparationTime = 20000\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"End\n"
	"Object P160Hall\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE CAPTURABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n";

// The fixture's world: combat's world (the locomotor store, the map and the AI the ability's approach needs — combattest::CombatWorld) with
// the port's AudioManager on a simulated device and TheEva installed on top of it. Both live on this thread, so a logic report runs TheEva
// here at once, as inside the game's audio owner.
struct Port160World : combattest::CombatWorld
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> audio;
	std::unique_ptr<Eva> eva;
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	double now = 0.0;
	unsigned reported = 0;

	Port160World()
	{
		// the fixture's own environment (the one INIEnvironment its objects load through) takes the stores' blocks
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string objectsError = w.load(kPort160Objects, INI_LOAD_OVERWRITE, "port160.ini");
		REQUIRE_MESSAGE(objectsError.empty(), objectsError);
		ini.registerBlocks(fx.env.blocks);
		const std::string audioError = initest::loadError(fx.env, "audio.ini", kPort160Audio);
		REQUIRE_MESSAGE(audioError.empty(), audioError);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 32u << 20);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		audio = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::RotWK_GameDat_LCG, 1u);
		audio->enableEventLog(true);
		audio->update(0.0);
		AudioApi::install(audio.get());
		eva = std::make_unique<Eva>(ini, *audio);
		eva->setLocalSide("Alpha");
		eva->setHomeBase(Coord3D{ 500.0f, 500.0f, 0.0f }, true);
		eva->setLocalPlayerIndex(alice()->getPlayerIndex());
		AudioApi::installEva(eva.get());
		logic->players().setLocalPlayer(alice());
		CastleTemplate layout;
		layout.name = "P160Camp";
		layout.version = 5;
		layout.entries.push_back({ "", "P160Keep", 0.0f, 0.0f, 0.0f, 0.0f, 40, 1 });
		logic->castleTemplates().add(layout);
	}
	~Port160World()
	{
		AudioApi::installEva(nullptr);
		AudioApi::install(nullptr);
		TheSpecialPowerStore = savedPowers;
	}
	Player *alice() { return players.findPlayerWithName("Alice"); }
	Object *building(const char *name, Player &owner, float x, float y)
	{
		Object *o = spawn(name, x, y, 0.0f, owner.getDefaultTeam());
		REQUIRE(o != nullptr);
		o->friend_onBuildComplete();
		return o;
	}
	void reset()
	{
		audio->takeEventLog();
		reported = eva->counters().reported;
	}
	// how many events TheEva accepted since the last reset (an unknown name, a disabled Eva or a duplicate inside its window is not counted)
	unsigned reportedSinceReset() const { return eva->counters().reported - reported; }
	void advance(double ms)
	{
		for (double step = 0.0; step < ms; step += 50.0)
		{
			now += 50.0;
			device->setTime(now);
			audio->update(now);
			eva->update(now);
		}
	}
	// which record of RW 0xBF2168 TheEva played since the last advance, and where the report said it happened
	bool playedAt(int index, float x, float y)
	{
		Coord3D at;
		if (!eva->lastPlayedPosition(index, &at))
		{
			return false;
		}
		return at.x == x && at.y == y;
	}
	static CastleBehavior *castleOf(Object *o) { return dynamic_cast<CastleBehavior *>(o->findModule("CastleBehavior")); }
	static SpecialAbilityUpdate *abilityOf(Object *o) { return dynamic_cast<SpecialAbilityUpdate *>(o->findModule("SpecialAbilityUpdate")); }
};
} // namespace

TEST_CASE("PORT-160 names: the capture and camp events are the predefined events of RW 0xBF2168 at their indices")
{
	Port160World f;
	CHECK(f.ini.eva.findIndex("BuildingBeingStolen", false) == 16);
	CHECK(f.ini.eva.findIndex("BuildingStolen", false) == 17);
	CHECK(f.ini.eva.findIndex("CampDestroyed", false) == 9);
	CHECK(f.ini.eva.findIndex("AllyCampDestroyed", false) == 0xA);
	CHECK(f.ini.eva.findIndex("EnemyCampDestroyed", false) == 8);
}

TEST_CASE("PORT-160 camp: an abandoned camp reports the owner / ally / attacker event of its CastleMemberBehavior data against the local player "
		  "(RW 0x79CB47 -> 0x79CB5C -> 0x799440; readers 0x79951F / 0x799560 / 0x7995C2; report 0x7995DB -> 0x5DD9EE)")
{
	Port160World f;
	Player *bob = f.players.findPlayerWithName("Bob");
	REQUIRE(bob);

	SUBCASE("the local player owns the camp: the owner's event (data + 0x08, RW 0x79951F)")
	{
		Object *flag = f.building("PCampFlag", *f.alice(), 300.0f, 300.0f);
		CastleBehavior *castle = Port160World::castleOf(flag);
		REQUIRE(castle);
		// the flag's own data names the three events (the RW defaults 9 / 0xA / 8 written out in the INI)
		const CastleMemberBehavior *member = dynamic_cast<const CastleMemberBehavior *>(flag->findModule("CastleMemberBehavior"));
		REQUIRE(member);
		REQUIRE(member->data());
		CHECK(member->data()->m_campDestroyedOwnerEvaEvent == "CampDestroyed");
		CHECK(member->data()->m_campDestroyedAllyEvaEvent == "AllyCampDestroyed");
		CHECK(member->data()->m_campDestroyedAttackerEvaEvent == "EnemyCampDestroyed");
		f.frames(1);
		REQUIRE(castle->state() == CastleBehavior::STATE_UNPACKED);
		f.reset();
		castle->abandonByPlayer();
		CHECK(f.reportedSinceReset() == 1u);
		f.advance(30.0);
		CHECK(f.playedAt(9, 300.0f, 300.0f));  // CampDestroyed, at the camp
		CHECK(!f.playedAt(0xA, 300.0f, 300.0f)); // not the ally's
		CHECK(!f.playedAt(8, 300.0f, 300.0f));  // not the attacker's
	}

	SUBCASE("an ally of the local player owns the camp: the ally's event (data + 0x0C, RW 0x799560)")
	{
		f.alice()->setPlayerRelationship(bob, ALLIES); // the fixture's skirmish setup puts the two slot players on different teams
		Object *flag = f.building("PCampFlag", *bob, 300.0f, 300.0f);
		CastleBehavior *castle = Port160World::castleOf(flag);
		REQUIRE(castle);
		f.frames(1);
		REQUIRE(castle->state() == CastleBehavior::STATE_UNPACKED);
		f.reset();
		castle->abandonByPlayer();
		CHECK(f.reportedSinceReset() == 1u);
		f.advance(30.0);
		CHECK(f.playedAt(0xA, 300.0f, 300.0f)); // AllyCampDestroyed
		CHECK(!f.playedAt(9, 300.0f, 300.0f));  // not the owner's
		CHECK(!f.playedAt(8, 300.0f, 300.0f));  // not the attacker's
	}

	SUBCASE("an enemy owns the camp: the attacker's event (data + 0x10, RW 0x7995C2)")
	{
		Object *flag = f.building("PCampFlag", *bob, 300.0f, 300.0f);
		CastleBehavior *castle = Port160World::castleOf(flag);
		REQUIRE(castle);
		f.frames(1);
		REQUIRE(castle->state() == CastleBehavior::STATE_UNPACKED);
		REQUIRE(f.alice()->getRelationship(bob) == ENEMIES);
		f.reset();
		castle->abandonByPlayer();
		CHECK(f.reportedSinceReset() == 1u);
		f.advance(30.0);
		CHECK(f.playedAt(8, 300.0f, 300.0f));   // EnemyCampDestroyed
		CHECK(!f.playedAt(9, 300.0f, 300.0f));  // not the owner's
		CHECK(!f.playedAt(0xA, 300.0f, 300.0f)); // not the ally's
	}
}

TEST_CASE("PORT-160 capture: the preparation on a locally controlled building reports BuildingBeingStolen once (index 16), the capture reports "
		  "BuildingStolen (index 17); a building of anyone else reports neither")
{
	Port160World f;
	Player *bob = f.players.findPlayerWithName("Bob");
	REQUIRE(bob);
	Team *bobTeam = bob->getDefaultTeam();
	REQUIRE(bobTeam);
	Team *aliceTeam = f.alice()->getDefaultTeam();
	REQUIRE(aliceTeam);

	SUBCASE("the local player's building is taken by an enemy: the warning at the start, the loss at the capture")
	{
		Object *hall = f.building("P160Hall", *f.alice(), 300.0f, 300.0f);
		Object *capturer = f.building("P160Capturer", *bob, 320.0f, 300.0f);
		SpecialAbilityUpdate *u = Port160World::abilityOf(capturer);
		REQUIRE(u);
		const SpecialPowerTemplate *spt = u->abilityData()->m_specialPowerTemplate;
		REQUIRE(spt);
		REQUIRE(spt->m_type == 0x1D);
		f.reset();
		u->initiateIntentToDoSpecialPower(spt, hall, nullptr, 0, 0);
		REQUIRE(u->isActive());
		// the preparation begins on the next logic frame and reports at once, before its frames run (donor SpecialAbilityUpdate.cpp:1347)
		f.frames(1);
		CHECK(f.reportedSinceReset() == 1u);
		f.advance(30.0);
		CHECK(f.playedAt(16, 300.0f, 300.0f)); // BuildingBeingStolen, at the building being taken
		// then the preparation ends, the capture lands and the loss is reported (donor EVA_CAPTURED = 17)
		f.reset();
		for (int i = 0; i < 250 && hall->getTeam() != bobTeam; ++i)
		{
			f.frames(1);
		}
		REQUIRE(hall->getTeam() == bobTeam);
		CHECK(f.reportedSinceReset() == 1u);
		f.advance(30.0);
		CHECK(f.playedAt(17, 300.0f, 300.0f)); // BuildingStolen
	}

	SUBCASE("the enemy's building is taken by the local player: neither event (the donor's isLocallyControlled guard), but the capture happens")
	{
		Object *hall = f.building("P160Hall", *bob, 300.0f, 500.0f);
		Object *capturer = f.building("P160Capturer", *f.alice(), 320.0f, 500.0f);
		SpecialAbilityUpdate *u = Port160World::abilityOf(capturer);
		REQUIRE(u);
		const SpecialPowerTemplate *spt = u->abilityData()->m_specialPowerTemplate;
		REQUIRE(spt);
		f.reset();
		u->initiateIntentToDoSpecialPower(spt, hall, nullptr, 0, 0);
		REQUIRE(u->isActive());
		f.frames(1);
		CHECK(f.reportedSinceReset() == 0u);
		for (int i = 0; i < 250 && hall->getTeam() != aliceTeam; ++i)
		{
			f.frames(1);
		}
		REQUIRE(hall->getTeam() == aliceTeam); // the capture itself is unchanged
		CHECK(f.reportedSinceReset() == 0u);
		f.advance(30.0);
		CHECK(!f.playedAt(16, 300.0f, 500.0f));
		CHECK(!f.playedAt(17, 300.0f, 500.0f));
	}
}
