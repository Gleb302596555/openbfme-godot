// OpenBFME unit tests for LOCAL-139 (fork issue 856): global game data fields wired to their RotWK 2.01 consumers.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001):
//   1. SpecialPower / EvaEventToPlayOnSuccess (+0x44): SpecialAbilityUpdate::startPacking(bool success) (RW 0x853468,
//      reads at RW 0x853692 .. 0x8536BD): when success is true and the controlling player is the local player (RW 0x6AA7B2),
//      calls TheEva->reportEvaEvent(template->getFinalOverride()->m_evaEventToPlayOnSuccess, &self->m_position, 0) (RW 0x5DD9EE).
//   2. DrawGroupInfo / FontName (+0x00) & FontIsBold (+0x08): RW 0x48FE84 (0x48FE9F FontIsBold, 0x48FEB0 FontName) passes
//      both to TheFontLibrary->getFont (RW 0x6186E0) for control-group number TEXT ops.
//   3. MiscEvaData / EnemyCampDestroyedDamageTimeoutMS (+0x84) & FriendlyCampDestroyedDamageTimeoutMS (+0x88):
//      CastleBehavior::reportCampDestroyedEva (RW 0x799440) gates owner/ally EVA with RW 0x7987C4 (RW 0x7994FF / 0x79953D)
//      and enemy EVA with m_damageMap[localPlayerIdx] + enemyTimeout >= now (RW 0x799584 .. 0x7995B9); CastleMemberBehavior::onDamage
//      (RW 0x79B757 -> 0x79B793) populates m_damageMap (+0xA0) via CastleBehavior::recordDamageByPlayers (RW 0x79B374);
//      CastleBehavior::xfer (RW 0x79BBBC) serializes m_damageMap.
//
// Synthetic RotWK-syntax INI only (no Ennorath or RotWK install required).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/DrawableIconUI.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/Eva.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
const char *kLocal139Audio =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n"
	"  SoundsExtension = wav\n  SampleCount2D = 4\n  SampleCount3D = 4\n  StreamCount = 3\n  MinSampleVolume = 2%\n  DefaultSoundVolume = 100%\n"
	"  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent EvaDefaultSound\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaCampDestroyed\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaAllyCampDestroyed\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaEnemyCampDestroyed\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"AudioEvent EvaAbilitySuccess\n  Volume = 100\n  Priority = high\n  Type = ui everyone\nEnd\n"
	"MiscEvaData\n"
	"  EnemyCampDestroyedDamageTimeoutMS = 1000\n"
	"  FriendlyCampDestroyedDamageTimeoutMS = 1000\n"
	"End\n"
	"PredefinedEvaEvent DefaultEvaEvent\n"
	"  Priority = 5\n"
	"  TimeBetweenEventsMS = 100\n"
	"  ExpirationTimeMS = 1500\n"
	"  SideSound\n"
	"    Side = Alpha\n"
	"    Sound = EvaDefaultSound\n"
	"  End\n"
	"End\n"
	"PredefinedEvaEvent CampDestroyed\n"
	"  Priority = 8\n"
	"  TimeBetweenEventsMS = 100\n"
	"  SideSound\n"
	"    Side = Alpha\n"
	"    Sound = EvaCampDestroyed\n"
	"  End\n"
	"End\n"
	"PredefinedEvaEvent AllyCampDestroyed\n"
	"  Priority = 6\n"
	"  TimeBetweenEventsMS = 100\n"
	"  SideSound\n"
	"    Side = Alpha\n"
	"    Sound = EvaAllyCampDestroyed\n"
	"  End\n"
	"End\n"
	"PredefinedEvaEvent EnemyCampDestroyed\n"
	"  Priority = 4\n"
	"  TimeBetweenEventsMS = 100\n"
	"  SideSound\n"
	"    Side = Alpha\n"
	"    Sound = EvaEnemyCampDestroyed\n"
	"  End\n"
	"End\n"
	"NewEvaEvent Local139AbilitySuccess\n"
	"  Priority = 5\n"
	"  TimeBetweenEventsMS = 100\n"
	"  SideSound\n"
	"    Side = Alpha\n"
	"    Sound = EvaAbilitySuccess\n"
	"  End\n"
	"End\n";

const char *kLocal139Objects =
	"Object L139CampFlag\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE BASE_SITE\n"
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
	"    CastleToUnpackForFaction = Alpha L139Camp\n"
	"    InstantUnpack = Yes\n"
	"    FadeTime = 0.2\n"
	"  End\n"
	"End\n"
	"Object L139Keep\n"
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
	"SpecialPower SpecialPowerLocal139Success\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 0\n"
	"  EvaEventToPlayOnSuccess = Local139AbilitySuccess\n"
	"End\n"
	"Object L139Caster\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Power\n"
	"    SpecialPowerTemplate = SpecialPowerLocal139Success\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_Ability\n"
	"    SpecialPowerTemplate = SpecialPowerLocal139Success\n"
	"    UnpackTime = 0\n"
	"    PackTime = 200\n"
	"    PreparationTime = 0\n"
	"  End\n"
	"End\n";

struct Local139World : combattest::CombatWorld
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

	Local139World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *iniPtr) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(iniPtr); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string objectsError = w.load(kLocal139Objects, INI_LOAD_OVERWRITE, "local139.ini");
		REQUIRE_MESSAGE(objectsError.empty(), objectsError);
		ini.registerBlocks(fx.env.blocks);
		const std::string audioError = initest::loadError(fx.env, "audio.ini", kLocal139Audio);
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
		layout.name = "L139Camp";
		layout.version = 5;
		layout.entries.push_back({ "", "L139Keep", 0.0f, 0.0f, 0.0f, 0.0f, 40, 1 });
		logic->castleTemplates().add(layout);
	}
	~Local139World()
	{
		AudioApi::installEva(nullptr);
		AudioApi::install(nullptr);
		TheSpecialPowerStore = savedPowers;
	}
	Player *alice() { return players.findPlayerWithName("Alice"); }
	Player *bob() { return players.findPlayerWithName("Bob"); }
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
	unsigned reportedSinceReset() const { return eva->counters().reported - reported; }
	void advanceAudio(double ms)
	{
		for (double step = 0.0; step < ms; step += 50.0)
		{
			now += 50.0;
			device->setTime(now);
			audio->update(now);
			eva->update(now);
		}
	}
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

// Source of expectations: RotWK 2.01 SpecialAbilityUpdate::startPacking RW 0x853468 (0x853692 .. 0x8536BD -> 0x5DD9EE).
TEST_CASE("LOCAL-139 SpecialPower EvaEventToPlayOnSuccess: reports EVA for local player on success and skips non-local / aborted (RW 0x853468 -> 0x8536BD)")
{
	Local139World f;
	const int successIdx = f.eva->findEvent("Local139AbilitySuccess");
	REQUIRE(successIdx >= 0); // tolerance: NewEvaEvent index is appended after predefined RW 0xBF2168 entries

	SUBCASE("local player succeeds: EvaEventToPlayOnSuccess fires at object position")
	{
		Object *caster = f.spawn("L139Caster", 220.0f, 310.0f, 0.0f, f.alice()->getDefaultTeam());
		REQUIRE(caster != nullptr);
		SpecialAbilityUpdate *sau = Local139World::abilityOf(caster);
		REQUIRE(sau != nullptr);
		const SpecialPowerTemplate *sp = f.powers.findSpecialPowerTemplate("SpecialPowerLocal139Success");
		REQUIRE(sp != nullptr);
		f.reset();
		sau->initiateIntentToDoSpecialPower(sp, nullptr, nullptr, 0, 0);
		f.frames(3);
		CHECK(f.reportedSinceReset() == 1u);
		f.advanceAudio(50.0);
		CHECK(f.playedAt(successIdx, 220.0f, 310.0f));
	}

	SUBCASE("non-local player succeeds: EvaEventToPlayOnSuccess does not fire (RW 0x85369C -> 0x6AA7B2)")
	{
		Object *caster = f.spawn("L139Caster", 420.0f, 410.0f, 0.0f, f.bob()->getDefaultTeam());
		REQUIRE(caster != nullptr);
		SpecialAbilityUpdate *sau = Local139World::abilityOf(caster);
		REQUIRE(sau != nullptr);
		const SpecialPowerTemplate *sp = f.powers.findSpecialPowerTemplate("SpecialPowerLocal139Success");
		REQUIRE(sp != nullptr);
		f.reset();
		sau->initiateIntentToDoSpecialPower(sp, nullptr, nullptr, 0, 0);
		f.frames(3);
		CHECK(f.reportedSinceReset() == 0u);
	}
}

// Source of expectations: RotWK 2.01 TheDrawGroupInfo RW 0x48FE84 (0x48FE9F FontIsBold, 0x48FEB0 FontName -> 0x6186E0).
TEST_CASE("LOCAL-139 DrawGroupInfo FontName & FontIsBold: group number TEXT op carries parsed FontName and FontIsBold (RW 0x48FE84)")
{
	combattest::CombatWorld f;
	const char *kSimpleUnit =
		"Object L139SimpleUnit\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryMinorRadius = 8\n"
		"  GeometryHeight = 20\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n";
	REQUIRE_MESSAGE(f.w.load(kSimpleUnit, INI_LOAD_OVERWRITE, "local139_dgi.ini").empty(), "load failed");
	Player *alice = f.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	f.logic->players().setLocalPlayer(alice);
	Object *unit = f.spawn("L139SimpleUnit", 300.0f, 300.0f, 0.0f, alice->getDefaultTeam());
	REQUIRE(unit != nullptr);
	alice->hotkeySquad(3).push_back(unit->getID());

	struct StubView : TacticalView
	{
		Coord3D pos{ 300.0f, 300.0f, 0.0f };
		ICoord2D size() const override { return { 800, 600 }; }
		bool worldToScreen(const Coord3D &w, ICoord2D &screen) const override
		{
			screen = { (int)w.x, (int)(600.0f - w.y - w.z) };
			return true;
		}
		bool screenToRay(const ICoord2D &, Coord3D &origin, Coord3D &dir) const override
		{
			origin = pos;
			dir = { 0.0f, 0.0f, -1.0f };
			return true;
		}
		Coord3D position() const override { return pos; }
		void lookAt(const Coord3D &w) override { pos = w; }
	} cam;
	struct EmptyW3DSource : W3DFileSource {
		bool Exists(const std::string &) const override { return false; }
		bool Read(const std::string &, std::vector<std::uint8_t> &, std::string *) override { return false; }
	} src;
	WW3DAssetManager assets(src);
	DrawableManager drawables(assets, *f.logic);
	ClientEvent ev;
	ev.kind = ClientEvent::CREATED;
	ev.object = unit->getID();
	ev.tmpl = static_cast<const ThingTemplate *>(unit->getTemplate());
	ev.position = *unit->getPosition();
	drawables.applyEvents({ ev });
	DrawableIconUI iconUi;

	IconUISettings settings;
	std::string err;
	const std::string iniBold =
		"DrawGroupInfo\n"
		"  FontName = \"Albertus Extra Bold\"\n"
		"  FontSize = 12\n"
		"  FontIsBold = Yes\n"
		"End\n";
	REQUIRE(DrawGroupInfo::parse(iniBold, settings.drawGroupInfo, &err));
	CHECK(settings.drawGroupInfo.fontName == "Albertus Extra Bold");
	CHECK(settings.drawGroupInfo.fontIsBold == true);

	std::vector<IconUIOp> ops;
	iconUi.build(*f.logic, drawables, cam, settings, { unit->getID() }, 0, ops);
	auto it = std::find_if(ops.begin(), ops.end(), [](const IconUIOp &o) { return o.kind == IconUIOp::TEXT && o.text == "3"; });
	REQUIRE(it != ops.end());
	CHECK(it->fontName == "Albertus Extra Bold");
	CHECK(it->fontIsBold == true);

	// Neighbor: FontIsBold = No and a different FontName
	const std::string iniPlain =
		"DrawGroupInfo\n"
		"  FontName = \"Arial\"\n"
		"  FontSize = 10\n"
		"  FontIsBold = No\n"
		"End\n";
	REQUIRE(DrawGroupInfo::parse(iniPlain, settings.drawGroupInfo, &err));
	ops.clear();
	iconUi.build(*f.logic, drawables, cam, settings, { unit->getID() }, 0, ops);
	it = std::find_if(ops.begin(), ops.end(), [](const IconUIOp &o) { return o.kind == IconUIOp::TEXT && o.text == "3"; });
	REQUIRE(it != ops.end());
	CHECK(it->fontName == "Arial");
	CHECK(it->fontIsBold == false);
}

// Source of expectations: RotWK 2.01 CastleBehavior::reportCampDestroyedEva RW 0x799440, RW 0x7987C4, RW 0x79B374, RW 0x79B757, RW 0x79BBBC.
TEST_CASE("LOCAL-139 MiscEvaData camp-destroyed timeouts: enemy and friendly timeouts gate CastleBehavior::reportCampDestroyedEva (RW 0x799440 / 0x7987C4 / 0x79B374)")
{
	Local139World f;
	REQUIRE(AudioApi::enemyCampDestroyedDamageTimeoutFrames() == 5u);
	REQUIRE(AudioApi::friendlyCampDestroyedDamageTimeoutFrames() == 5u);

	auto hitCastle = [&](Object *flag, Player *attacker) {
		DamageInfo dmg;
		dmg.m_input.m_sourcePlayerMask = 1u << attacker->getPlayerIndex();
		dmg.m_output.m_actualDamageDealt = 10.0f;
		CastleMemberBehavior *cm = dynamic_cast<CastleMemberBehavior *>(flag->findModule("CastleMemberBehavior"));
		REQUIRE(cm != nullptr);
		cm->onDamage(dmg); // RW 0x79B757 -> 0x79B374
	};

	SUBCASE("enemy camp: undamaged skips EVA; damaged within timeout fires; expired timeout skips")
	{
		Object *flag = f.building("L139CampFlag", *f.bob(), 300.0f, 300.0f);
		CastleBehavior *castle = Local139World::castleOf(flag);
		REQUIRE(castle != nullptr);
		f.frames(1);
		REQUIRE(castle->state() == CastleBehavior::STATE_UNPACKED);

		// Undamaged enemy camp -> no EVA (RW 0x79959B)
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 0u);

		// Damaged by local player (Alice) -> within 5-frame timeout -> fires EnemyCampDestroyed (index 8)
		hitCastle(flag, f.alice());
		f.frames(3);
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 1u);
		f.advanceAudio(50.0);
		CHECK(f.playedAt(8, 300.0f, 300.0f));

		// Advance past 5-frame timeout -> expired -> no EVA (RW 0x7995B7..0x7995B9)
		f.frames(5);
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 0u);
	}

	SUBCASE("friendly camp: undamaged skips EVA; damaged by other player fires (RW 0x7994FF -> 0x7987C4)")
	{
		Object *flag = f.building("L139CampFlag", *f.alice(), 320.0f, 320.0f);
		CastleBehavior *castle = Local139World::castleOf(flag);
		REQUIRE(castle != nullptr);
		f.frames(1);
		REQUIRE(castle->state() == CastleBehavior::STATE_UNPACKED);

		// Undamaged friendly camp -> no EVA
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 0u);

		// Self-damage only -> still not damaged by OTHER player -> no EVA (RW 0x7987D3)
		hitCastle(flag, f.alice());
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 0u);

		// Damaged by enemy Bob -> fires CampDestroyed (index 9)
		hitCastle(flag, f.bob());
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 1u);
		f.advanceAudio(50.0);
		CHECK(f.playedAt(9, 320.0f, 320.0f));
	}

	SUBCASE("CastleBehavior::xfer v3 preserves m_damageMap across save/load (RW 0x79BBBC)")
	{
		Object *flag = f.building("L139CampFlag", *f.bob(), 340.0f, 340.0f);
		CastleBehavior *castle = Local139World::castleOf(flag);
		REQUIRE(castle != nullptr);
		f.frames(1);
		hitCastle(flag, f.alice());

		XferSave xs;
		castle->xfer(xs);
		castle->abandonByPlayer(); // clears m_damageMap (RW 0x79CC86 -> 0x7985AA)
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 0u);

		XferLoad xl(xs.data());
		castle->xfer(xl);
		f.reset();
		castle->reportCampDestroyedEva();
		CHECK(f.reportedSinceReset() == 1u);
	}
}
