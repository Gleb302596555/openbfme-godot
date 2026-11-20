// OpenBFME unit tests (lane PORT-296, fork issue #688): the castle's crew and the CastleMemberBehavior's BeingBuiltSound.
//
// RotWK 2.01, read through tools/mod/rw.py on game.dat (md5 daed3668; every address below is a RotWK one):
//   * registerOwnedObject RW 0x79AC19 puts every owned object that FilterCrew and FilterValidOwnedEntries both accept into the castle's crew list (+ 0x68) and
//     prepares it (RW 0x79AD92 -> RW 0x798397): the member's producer becomes the castle (RW 0x798398, RW 0x68B6A1), it is disabled for good (RW 0x7983A6:
//     RW 0x692432(3) -> RW 0x6907F1(3, 0x3FFFFFFF)), it sets the model condition PREPARING (the raw bit 31 of Object + 0x114, condition 95, then the drawable /
//     AI notify RW 0x68B53C) and the statuses UNSELECTABLE (RW 0x62684D(3, 1)) and DO_NOT_PICK_ME (RW 0x62684D(0x4F, 1)).
//   * the crew's clock RW 0x79B0F1 runs in the unpacked state (RW 0x79D02A): nothing without a crew, without the keep (RW 0x449681 on + 0x38) or while the
//     keep's construction percent (Object + 0x288) is below 8.0 (RW 0xBD89BC). A released crew needs either a finished keep (99.0 percent or more, RW
//     0xBE49F0, RW 0x79B274) or a hostile hit whose record's frame (body vslot 0x44, the record's + 0x20 above 0, RW 0xC1B594) is at or after the castle's
//     unpack frame (+ 0x48) and whose source player (the record's mask + 0xC through ThePlayerList RW 0x6A85B6) is ENEMIES of the keep's player (RW 0x6ADBEB)
//     - or that player is PlyrCreeps (RW 0xBFD514) - and only once the unpack frame plus CrewPrepareTime (+ 0x38) has passed (RW 0x79B283 .. 0x79B2A8).
//     The release is releaseCrew(false) (RW 0x79A237: RW 0x798344 on every member, CrewReleaseFX + 0x48, the list cleared, DO_NOT_PICK_ME off the keep and
//     the members), then the keep is healed by 9999.0 (RW 0x690532, the castle as the source) and its construction percent becomes 100.0 (RW 0xBD88D8).
//   * while the crew list is not empty and the frame is a multiple of CrewPrepareInterval (+ 0x40, an unsigned division RW 0x79B34A) the castle plays
//     CrewPrepareFX (+ 0x44) on itself (RW 0x4B1B5A); the check runs after the release, so that frame plays nothing.
//   * setMembersModelCondition RW 0x799F0F walks the keep and the list + 0x5C and asks every object's CastleMemberBehavior (by the name key RW 0xDE8240,
//     RW 0x68BDA5) with the same pair of masks (RW 0x799F96 / 0x799FF3 -> RW 0x798057). RW 0x798057 tests the new mask's BASE_BUILD bit (condition 219):
//     with an empty data name (+ 0x14) nothing happens (RW 0x79807E); while the module's kept handle (module + 0x20) is not playing (RW 0xDE42FC vslot
//     0xD0) the event is built for the object (RW 0x6DB5B3) and added (vslot 0x64, RW 0x7980BD); when the condition is cleared only a real handle (>= 5,
//     RW 0x7980E4) is removed (vslot 0x6C) and reset to 1 (RW 0x7980F5). The port keeps the handle on the audio side (AudioApi::HELD_BEING_BUILT_LOOP).
//
// Synthetic INI only (no game files, no mod data). GPL-3.0.

#include "doctest.h"

#include "ProdTestUtil.h"
#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/StateHash.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <memory>
#include <string>
#include <vector>

using namespace prodtest;

namespace
{
// A castle whose crew filter takes one piece of its layout (FilterCrew = ANY +STRUCTURE; no retail castle ships a crew filter, so the mechanism is exercised
// by data written here: a crew member is an owned piece of the layout, the way RW 0x79AD4E classifies one), its keep and one foundation (BASE_FOUNDATION goes
// to the foundations before the crew test, RW 0x79ADA8). CrewPrepareTime is a duration in milliseconds (parser RW 0x73B33E, the INI's milliseconds times
// LOGICFRAMES_PER_MSEC_REAL 0.005 -> the module data + 0x38 in frames): 6000 ms = 30 frames; CrewPrepareInterval 2000 ms = 10 frames.
const char kCrewObjects[] =
	"Object CKKeep\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE CASTLE_KEEP\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 50\n"
	"    MaxHealthReallyDamaged = 25\n"
	"  End\n"
	"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
	"    BeingBuiltSound = CrewBeingBuilt\n"
	"  End\n"
	"End\n"
	"Object CKCrewman\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"    MaxHealthDamaged = 100\n"
	"    MaxHealthReallyDamaged = 50\n"
	"  End\n"
	"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
	"  End\n"
	"End\n"
	"Object CKHall\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE BASE_FOUNDATION\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 50\n"
	"    MaxHealthReallyDamaged = 25\n"
	"  End\n"
	"  Behavior = CastleMemberBehavior ModuleTag_CM\n"
	"  End\n"
	"End\n"
	"Object CKCastle\n"
	"  KindOf = IMMOBILE BASE_FOUNDATION CASTLE_CENTER\n"
	"  Behavior = CastleBehavior ModuleTag_castle\n"
	"    CastleToUnpackForFaction = Alpha Base_Crew\n"
	"    CastleToUnpackForFaction = Beta Base_Crew\n"
	"    FilterCrew = ANY +STRUCTURE\n"
	"    FilterValidOwnedEntries = ANY +STRUCTURE +BASE_FOUNDATION\n"
	"    CrewPrepareTime = 6000\n"
	"    CrewPrepareInterval = 2000\n"
	"    CrewPrepareFX = CrewPrepareFx\n"
	"    CrewReleaseFX = CrewReleaseFx\n"
	"    InstantUnpack = Yes\n"
	"  End\n"
	"End\n";

struct CrewWorld : ProdWorld
{
	CrewWorld()
	{
		load(kCrewObjects);
		logic->settings().buildRulesLoaded = true;
		CastleTemplate layout;
		layout.name = "Base_Crew";
		layout.version = 5;
		layout.entries.push_back({ "", "CKKeep", 0.0f, 0.0f, 0.0f, 0.0f, 40, 1 });
		layout.entries.push_back({ "", "CKCrewman", 60.0f, 0.0f, 0.0f, 0.0f, 40, 1 });
		layout.entries.push_back({ "", "CKHall", 0.0f, 60.0f, 0.0f, 0.0f, 40, 1 });
		logic->castleTemplates().add(layout);
	}
	Object *castle(const char *player = "Alice", float x = 1000.0f, float y = 1000.0f)
	{
		Object *c = make("CKCastle", teamOf(player));
		REQUIRE(c != nullptr);
		Coord3D at{ x, y, 0.0f };
		c->setPosition(&at);
		c->friend_onBuildComplete();
		frames(1); // the constructor's update unpacks the layout
		return c;
	}
	static CastleBehavior *cb(Object *o) { return dynamic_cast<CastleBehavior *>(o->findModule("CastleBehavior")); }
	Object *byId(ObjectID id) { return logic->findObjectByID(id); }
	// a hit of `amount` from `source` (the record's source mask follows the source's player, the way RW 0x910BEF writes it)
	void hit(Object &victim, Object *source, float amount)
	{
		DamageInfo d;
		d.m_input.m_amount = amount;
		d.m_input.m_damageType = DAMAGE_SLASH;
		d.m_input.m_sourceID = source ? source->getID() : INVALID_ID;
		const Player *p = source ? source->getControllingPlayer() : nullptr;
		d.m_input.m_sourcePlayerMask = p && p->getPlayerIndex() >= 0 ? (1u << p->getPlayerIndex()) : 0u;
		victim.attemptDamage(d);
	}
	// Alice and Bob are enemies (the fixture leaves every pair NEUTRAL, RW 0x6A85B6 asks the relationship)
	void enemies()
	{
		Player *a = alice();
		Player *b = players.findPlayerWithName("Bob");
		REQUIRE(a != nullptr);
		REQUIRE(b != nullptr);
		a->setPlayerRelationship(b, ENEMIES);
		b->setPlayerRelationship(a, ENEMIES);
	}
	unsigned long long fxAt(const char *site) const
	{
		const auto &per = logic->fxEvents().perSite();
		const auto it = per.find(site);
		return it == per.end() ? 0ull : it->second;
	}
	Object *keep(CastleBehavior *b) { return byId(b->keepId()); }
	Object *crewman(CastleBehavior *b)
	{
		REQUIRE(b->crew().size() == 1);
		Object *m = byId(b->crew()[0]);
		REQUIRE(m != nullptr);
		return m;
	}
};

// the names of the status and model condition tables (BitFlagNames.cpp; the binary's own lists, RW 0xD8AFF0 / RW 0xDA0E68)
int preparing() { return CombatNames::modelCondition("PREPARING"); }            // condition 95 (RW 0x7983BC: Object + 0x114 bit 31)
int baseBuild() { return CombatNames::modelCondition("BASE_BUILD"); }           // condition 219 (RW 0x79806D: mask + 0x18 bit 27)
int justBuilt() { return CombatNames::modelCondition("JUST_BUILT"); }
int unselectable() { return CombatNames::status("UNSELECTABLE"); }             // RW 0x62684D(3, .)
int doNotPickMe() { return CombatNames::status("DO_NOT_PICK_ME"); }            // RW 0x62684D(0x4F, .)
int noAttack() { return CombatNames::status("NO_ATTACK"); }                    // RW 0x62684D(5, .)
} // namespace

TEST_CASE("PORT-296: a crew member is prepared for its castle (RW 0x798397) and released when the keep is finished (RW 0x79B0F1 -> 0x79A237)")
{
	CrewWorld f;
	Object *c = f.castle();
	CastleBehavior *b = f.cb(c);
	REQUIRE(b != nullptr);
	REQUIRE(b->crew().size() == 1);
	Object *m = f.crewman(b);
	// RW 0x798398 / 0x7983A6: the castle is the producer, the member is held for good
	CHECK(m->getProducerID() == c->getID());
	CHECK(m->isDisabledByType(DISABLED_HELD));
	// RW 0x7983BC: the model condition PREPARING (condition 95) and RW 0x7983CF / 0x7983DA: the two statuses
	CHECK(m->testModelCondition(preparing()));
	CHECK(m->testStatus((unsigned)unselectable()));
	CHECK(m->testStatus((unsigned)doNotPickMe()));
	// and the keep / the other members did not get it (RW 0x799F0F walks + 0x5C, the crew is + 0x68)
	CHECK_FALSE(f.keep(b)->testModelCondition(preparing()));

	// RW 0x79B274: the keep at 99 percent or more releases the crew without any hit
	f.keep(b)->setConstructionPercent(99.0f);
	f.frames(5); // the state 0 update sleeps 5 frames (RW 0x79D0D5): the first state 4 update is the sixth frame
	REQUIRE(f.logic->getFrame() == 6);
	const ObjectID memberId = m->getID();
	CHECK(b->crew().empty());
	// RW 0x79834D .. 0x79838E: the release undoes every preparation step
	Object *released = f.byId(memberId);
	REQUIRE(released != nullptr);
	CHECK(released->getProducerID() == INVALID_ID);
	CHECK_FALSE(released->isDisabledByType(DISABLED_HELD));
	CHECK_FALSE(released->testModelCondition(preparing()));
	CHECK_FALSE(released->testStatus((unsigned)unselectable()));
	CHECK_FALSE(released->testStatus((unsigned)doNotPickMe()));
	CHECK_FALSE(released->testStatus((unsigned)noAttack()));
	// RW 0x79B2C3 / 0x79B2C8: the keep is healed and finished
	CHECK(f.keep(b)->getConstructionPercent() == 100.0f);
	// RW 0x79A337: the crew release FX on the castle, once
	CHECK(f.fxAt("CastleBehavior crew release") == 1ull);
	CHECK(f.fxAt("CastleBehavior crew prepare") == 0ull);
}

TEST_CASE("PORT-296: a hostile hit releases the crew only once CrewPrepareTime has passed (RW 0x79B0F1, 0x79B283)")
{
	CrewWorld f;
	f.enemies();
	Object *c = f.castle();
	CastleBehavior *b = f.cb(c);
	Object *m = f.crewman(b);
	Object *keep = f.keep(b);
	keep->setConstructionPercent(50.0f); // below 99: only the timer branch can release the crew
	Object *bob = f.make("CKCrewman", f.teamOf("Bob"));
	REQUIRE(bob != nullptr);
	const UnsignedInt unpackFrame = b->unpackFrame();
	// a hostile hit inside the first frames: the record's frame is at or after the unpack frame (RW 0x79B188) and its player is an enemy (RW 0x79B1C1),
	// but the crew's clock waits for the unpack frame plus the data's CrewPrepareTime (6000 ms = 30 frames)
	f.frames(10);
	REQUIRE(f.logic->getFrame() == unpackFrame + 10); // one update per frame (state 4 returns UPDATE_SLEEP_NONE)
	f.hit(*m, bob, 5.0f);
	f.frames(1);
	CHECK(b->crew().size() == 1);
	CHECK(m->testModelCondition(preparing()));
	// the record stands; the release happens on the first update at or after the 30th frame
	while (f.logic->getFrame() + 1 < unpackFrame + 30)
	{
		f.frames(1);
	}
	CHECK(b->crew().size() == 1);
	f.frames(2); // RW 0x79B283: frame >= the unpack frame + CrewPrepareTime
	CHECK(b->crew().empty());
	CHECK(keep->getConstructionPercent() == 100.0f);
	CHECK(f.fxAt("CastleBehavior crew release") == 1ull);
	CHECK_FALSE(f.byId(m->getID())->testModelCondition(preparing()));
}

TEST_CASE("PORT-296: an allied hit and a hit that asks for no damage never release the crew (RW 0x79B191, 0x79B1C1)")
{
	CrewWorld f;
	f.enemies();
	Object *c = f.castle();
	CastleBehavior *b = f.cb(c);
	Object *m = f.crewman(b);
	f.keep(b)->setConstructionPercent(50.0f);
	Object *bob = f.make("CKCrewman", f.teamOf("Bob"));
	REQUIRE(bob != nullptr);
	Object *ally = f.make("CKCrewman", f.teamOf("Alice"));
	REQUIRE(ally != nullptr);
	f.frames(40);
	REQUIRE(f.logic->getFrame() == b->unpackFrame() + 40);
	// RW 0x6ADBEB: the record's player is an ALLY, not an ENEMY
	f.hit(*m, ally, 5.0f);
	f.frames(2);
	CHECK(b->crew().size() == 1);
	// RW 0x79B191 with 0.0 (RW 0xC1B594): the record's amount must be above 0
	f.hit(*m, bob, 0.0f);
	f.frames(2);
	CHECK(b->crew().size() == 1);
	// the same enemy with damage releases it (RW 0x79B2B2)
	f.hit(*m, bob, 5.0f);
	f.frames(1);
	CHECK(b->crew().empty());
}

TEST_CASE("PORT-296: below 8 percent the crew's clock does nothing even with a hostile hit (RW 0x79B13A)")
{
	CrewWorld f;
	f.enemies();
	Object *c = f.castle();
	CastleBehavior *b = f.cb(c);
	Object *m = f.crewman(b);
	Object *keep = f.keep(b);
	Object *bob = f.make("CKCrewman", f.teamOf("Bob"));
	REQUIRE(bob != nullptr);
	keep->setConstructionPercent(7.5f); // RW 0x79B13A: 8.0 is the threshold (RW 0xBD89BC)
	f.frames(40);
	f.hit(*m, bob, 5.0f);
	f.frames(2);
	CHECK(b->crew().size() == 1);
	CHECK(keep->getConstructionPercent() == 7.5f);
	// and the member stays prepared
	CHECK(m->testModelCondition(preparing()));
	// at 8 percent the same hit releases it
	keep->setConstructionPercent(8.0f);
	f.hit(*m, bob, 5.0f);
	f.frames(1);
	CHECK(b->crew().empty());
}

TEST_CASE("PORT-296: the crew prepare FX ticks every CrewPrepareInterval frames while the crew waits (RW 0x79B32D)")
{
	CrewWorld f;
	Object *c = f.castle();
	CastleBehavior *b = f.cb(c);
	Object *keep = f.keep(b);
	keep->setConstructionPercent(50.0f); // the crew stays: the release needs 99 percent or a hostile hit
	const UnsignedInt unpackFrame = b->unpackFrame();
	// RW 0x79B34A: the frame modulo the interval (200 ms = 6 frames); the check runs every update the castle takes (one frame)
	unsigned long long expected = 0;
	for (UnsignedInt frame = f.logic->getFrame() + 1; frame <= unpackFrame + 60; ++frame)
	{
		f.frames(1);
		REQUIRE(f.logic->getFrame() == frame);
		// the state 0 update slept to the sixth frame (RW 0x79D0D5); from then on the module runs every frame (UPDATE_SLEEP_NONE)
		if (frame >= 6u && frame % 10u == 0u)
		{
			++expected;
		}
		CHECK(f.fxAt("CastleBehavior crew prepare") == expected);
	}
	CHECK(expected == 6ull); // the frames 10, 20, .. 60
	REQUIRE(b->crew().size() == 1);
}

TEST_CASE("PORT-296: CastleMemberBehavior's BeingBuiltSound follows the model condition BASE_BUILD (RW 0x798057)")
{
	// the audio manager the hook asks (RW 0xDE42FC); the fixture is test_port_93's
	initest::Fixture fx;
	AudioIniState ini;
	initest::FileList list;
	list.push_back({ "data\\audio\\sounds\\crewbuild.wav", [] {
		std::vector<std::uint8_t> out;
		const std::uint32_t rate = 22050, frames = 22050;
		auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) { out.push_back((std::uint8_t)(v >> (8 * i))); } };
		auto u16 = [&](std::uint16_t v) { out.push_back((std::uint8_t)v); out.push_back((std::uint8_t)(v >> 8)); };
		auto tag = [&](const char *t) { out.insert(out.end(), t, t + 4); };
		tag("RIFF"); u32(36 + frames * 2); tag("WAVE"); tag("fmt "); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
		tag("data"); u32(frames * 2); out.insert(out.end(), frames * 2, 0);
		return std::string(out.begin(), out.end());
	}() });
	list.push_back({ "data\\ini\\none.txt", "x" });
	fx.mount(list);
	ini.registerBlocks(fx.env.blocks);
	const std::string audioIni =
		"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n"
		"  SoundsExtension = wav\n  SampleCount2D = 2\n  SampleCount3D = 3\n  StreamCount = 3\n  MinSampleVolume = 2%\n  GlobalMinRange = 5000\n"
		"  GlobalMaxRange = 5000000\n  TimeToFadeAudio = 300\n  AmbientStreamHysteresisVolume = 10\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n"
		"  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
		"AudioEvent CrewBeingBuilt\n  Sounds = crewbuild\n  Type = world everyone\n  MaxRange = 5000\n  Limit = 0\nEnd\n";
	const std::string err = initest::loadError(fx.env, "audio.ini", audioIni);
	REQUIRE_MESSAGE(err.empty(), err);
	auto cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
	auto device = std::make_unique<SimulatedAudioDevice>(cache.get());
	auto mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1234u);
	mgr->update(0.0);
	AudioApi::install(mgr.get());
	struct Uninstall
	{
		~Uninstall() { AudioApi::install(nullptr); }
	} uninstall;

	CrewWorld f;
	Object *c = f.castle();
	CastleBehavior *b = f.cb(c);
	Object *keep = f.keep(b);
	// the keep does not carry BASE_BUILD yet: nothing sounds
	CHECK_FALSE(keep->testModelCondition(baseBuild()));
	CHECK(AudioApi::heldSound(AudioApi::HELD_BEING_BUILT_LOOP, keep->getID()) == 0u);
	// RW 0x79D024: the unpack's end sets BASE_BUILD on the keep and the members (RW 0x799F0F -> RW 0x798057)
	b->setMembersModelCondition(justBuilt(), baseBuild());
	CHECK(keep->testModelCondition(baseBuild()));
	const std::uint32_t handle = AudioApi::heldSound(AudioApi::HELD_BEING_BUILT_LOOP, keep->getID());
	CHECK(handle == 5u); // RW 0x45C74D: the manager's handle counter starts at 5 (RW 0x7980E4 removes only such a handle)
	CHECK(AudioApi::heldSounds(AudioApi::HELD_BEING_BUILT_LOOP) == 1u); // only the keep's data names the sound
	// RW 0x798094: a second call while the sound plays does not add it again
	b->setMembersModelCondition(-1, baseBuild());
	CHECK(AudioApi::heldSound(AudioApi::HELD_BEING_BUILT_LOOP, keep->getID()) == handle);
	CHECK(AudioApi::heldSounds(AudioApi::HELD_BEING_BUILT_LOOP) == 1u);
	// RW 0x79D093: the building ends, BASE_BUILD is cleared and the handle goes (RW 0x7980F2)
	b->setMembersModelCondition(baseBuild(), -1);
	CHECK_FALSE(keep->testModelCondition(baseBuild()));
	CHECK(AudioApi::heldSound(AudioApi::HELD_BEING_BUILT_LOOP, keep->getID()) == 0u);
	CHECK(AudioApi::heldSounds(AudioApi::HELD_BEING_BUILT_LOOP) == 0u);
}
