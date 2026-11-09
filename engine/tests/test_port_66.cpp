// OpenBFME. PORT-66 (issue #180): the GameData AutoFire / AutoSmoke / AutoAflame particle systems, the fires and smoke of a damaged or burning object.
//
// What RotWK does (game.dat 2.01, read through tools/mod/rw.py):
//   * the 21 rows of the GameData table RW 0xC00150 .. 0xC00290 (AutoFireParticle{Small,Medium,Large}{Prefix,System,Max}, the same three for AutoSmoke,
//     AutoAflameParticle{Prefix,System,Max}) write GlobalData +0x9F0 .. +0xA40; the Prefix and System rows go through parseAsciiString RW 0x42EE5E, the Max
//     rows through parseInt RW 0x42EC5E.
//   * the reader is ActiveBody::updateBodyParticleSystems, body interface slot 0x2C: the interface table RW 0xC71ED0 has RW 0x8C1E4E at +0x2C (its entries
//     +0x4C / +0x50 are createParticleSystems RW 0x8C23EA / deleteAllParticleSystems RW 0x8C258F), and the module constructor RW 0x8C4920 stores that
//     table at module + 0x10 (RW 0x8C493A).
//   * RW 0x8C1E4E: seven static template lookups (RW 0x5F889B over the GlobalData names, guards 0xDE9B04, cells 0xDE9B00 .. 0xDE9AE8); the object's
//     status 0xA AFLAME (RW 0x44DDEC) swaps the templates (fire small := medium, fire medium := large, fire large := large, the three smoke := fire small)
//     and sets the count modifier 2 (RW 0x8C1FDD .. 0x8C2000, else the plain set and 1 at RW 0x8C2002 .. 0x8C202A); deleteAllParticleSystems (vslot 0x50);
//     then createParticleSystems (vslot 0x4C) for the small, medium and large fire and smoke prefixes with Auto*Max * the modifier (RW 0x8C203D ..
//     0x8C20F2); and for an aflame object one more call for AutoAflameParticlePrefix with AutoAflameParticleMax * 2 (RW 0x8C20F5 .. 0x8C2126).
//   * createParticleSystems RW 0x8C23EA: no template -> nothing (RW 0x8C2401); the bones are Object::getMultiLogicalBonePosition RW 0x68C650 (start index
//     1, 16 at most) of the prefix; the cap is min(the bones found, the count handed in); per system a CLIENT random bone among the unused ones
//     (RW 0x6D32E4), createParticleSystem RW 0x5F526A, setPosition RW 0x5F2F2D at the bone, attachToObject RW 0x5F32D7, the ID kept on the body's list
//     (+0xC8, deleted by RW 0x8C258F).
//   * the callers of the slot through the object's body module (Object + 0x25C): StructureCollapseUpdate RW 0x8A8029 .. 0x8A8031 (the FINAL phase, after
//     the POST_RUBBLE flags) and RW 0x8A80FD .. 0x8A8105 (ALMOST_FINAL), StructureToppleUpdate RW 0x8A9AA0 .. 0x8A9AA8, RubbleRiseUpdate RW 0x8A8964 ..
//     0x8A896C (not ported, no RubbleRiseUpdate class in the port), FlammableUpdate RW 0x8901DA .. 0x8901E2 (tryToIgnite, right after the AFLAME status is
//     set) and RW 0x89011C .. 0x890124 (stopBurning, right after it is cleared).
//
// The tests: synthetic data (combattest::CombatWorld), no retail files. The binary facts are read from the image when RW_GAME_DAT is set (the GameData
// table's rows and their offsets, the interface table's slot 0x2C and its stub, the module constructor's store).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "PeImage.h"

#include "GameClient/FXPlayback.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Module/StructureModules.h"

#include <cstring>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
// the seven entries of the event, in the binary's call order
enum Entry
{
	kFireSmall = 0,
	kFireMedium,
	kFireLarge,
	kSmokeSmall,
	kSmokeMedium,
	kSmokeLarge,
	kAflame
};

// the GameData rows of the port, the values a mod's gamedata.ini writes (synthetic names)
const char *kAutoGameData =
	"GameData\n"
	"  ForceModelsToFollowTimeOfDay = No\n"
	"  ForceModelsToFollowWeather = No\n"
	"  UnitDamagedThreshold = 50%\n"
	"  UnitReallyDamagedThreshold = 25%\n"
	"  DefaultStartingCash = 1000\n"
	"  AutoFireParticleSmallPrefix = FIRESMALL\n"
	"  AutoFireParticleSmallSystem = FireSmallSys\n"
	"  AutoFireParticleSmallMax = 2\n"
	"  AutoFireParticleMediumPrefix = FIREMED\n"
	"  AutoFireParticleMediumSystem = FireMedSys\n"
	"  AutoFireParticleMediumMax = 3\n"
	"  AutoFireParticleLargePrefix = FIRELARGE\n"
	"  AutoFireParticleLargeSystem = FireLargeSys\n"
	"  AutoFireParticleLargeMax = 4\n"
	"  AutoSmokeParticleSmallPrefix = SMOKESMALL\n"
	"  AutoSmokeParticleSmallSystem = SmokeSmallSys\n"
	"  AutoSmokeParticleSmallMax = 5\n"
	"  AutoSmokeParticleMediumPrefix = SMOKEMED\n"
	"  AutoSmokeParticleMediumSystem = SmokeMedSys\n"
	"  AutoSmokeParticleMediumMax = 6\n"
	"  AutoSmokeParticleLargePrefix = SMOKELARGE\n"
	"  AutoSmokeParticleLargeSystem = SmokeLargeSys\n"
	"  AutoSmokeParticleLargeMax = 7\n"
	"  AutoAflameParticlePrefix = AFLAME\n"
	"  AutoAflameParticleSystem = AflameSys\n"
	"  AutoAflameParticleMax = 1\n"
	"End\n";

// PORT-66's synthetic probe: a structure whose collapse leaves a rubble object, a FlammableUpdate object, and a body class whose slot 0x2C is the empty stub
const char *kPort66Objects =
	"Object P66Barracks\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 75\n"
	"    MaxHealthReallyDamaged = 50\n"
	"  End\n"
	"  Behavior = StructureCollapseUpdate ModuleTag_Collapse\n"
	"    MinCollapseDelay = 0\n"
	"    MaxCollapseDelay = 0\n"
	"    MinBurstDelay = 0\n"
	"    MaxBurstDelay = 0\n"
	"    BigBurstFrequency = 0\n"
	"    MaxShudder = 0.0\n"
	"    CollapseDamping = 0.5\n"
	"    DestroyObjectWhenDone = No\n"
	"  End\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"Object P66Torch\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 75\n"
	"  End\n"
	"  Behavior = FlammableUpdate ModuleTag_Flammable\n"
	"    AflameDuration = 5000\n"
	"    AflameDamageAmount = 0\n"
	"  End\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"End\n"
	"Object P66Inert\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Body = InactiveBody ModuleTag_Body\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"End\n";

// the bones of the probe's model: FIRESMALL01 .. FIRESMALL03, FIREMED01 .. FIREMED02, one SMOKELARGE01 and one AFLAME01 (model space, RW 0x68C650's units)
class P66Bones : public ProjectileLaunchOffsets
{
public:
	bool launchOffset(const Object &, int, int, float[12]) override { return false; }
	int multiLogicalBones(const Object &, const std::string &prefix, const std::array<std::uint32_t, 19> &, int maxBones, float (*out)[12]) override
	{
		const int count = boneCount(prefix);
		const int n = maxBones < count ? maxBones : count;
		for (int i = 0; i < n; ++i)
		{
			for (int k = 0; k < 12; ++k)
			{
				out[i][k] = (k == 0 || k == 5 || k == 10) ? 1.0f : 0.0f;
			}
			out[i][3] = xOf(prefix);
			out[i][7] = (float)i;
			out[i][11] = 1.0f;
		}
		return n;
	}
	static int boneCount(const std::string &prefix)
	{
		if (prefix == "FIRESMALL")
		{
			return 3;
		}
		if (prefix == "FIREMED")
		{
			return 2;
		}
		if (prefix == "SMOKELARGE" || prefix == "AFLAME")
		{
			return 1;
		}
		return 0;
	}
	static float xOf(const std::string &prefix)
	{
		if (prefix == "FIRESMALL")
		{
			return 10.0f;
		}
		if (prefix == "FIREMED")
		{
			return 20.0f;
		}
		if (prefix == "SMOKELARGE")
		{
			return 30.0f;
		}
		return 40.0f;
	}
	int calls = 0;
};

struct P66World : CombatWorld
{
	P66Bones bones;
	explicit P66World()
	{
		std::string err;
		REQUIRE_MESSAGE(w.load(kPort66Objects, INI_LOAD_OVERWRITE, "port66.ini").empty(), "port66.ini");
		REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(kAutoGameData, logic->settings(), &err), err);
		combat().setLaunchOffsets(&bones);
	}
	~P66World() { combat().setLaunchOffsets(nullptr); }
	std::vector<FXEvent> events() const { return logic->fxEvents().frameEvents(); }
	// the body particle events of `id`, in the order they were made
	std::vector<FXEvent> bodyEvents(ObjectID id) const
	{
		std::vector<FXEvent> out;
		for (const FXEvent &e : logic->fxEvents().frameEvents())
		{
			if (e.kind == FXEvent::BODY_PARTICLE_SYSTEMS && (id == INVALID_ID || e.primary == id))
			{
				out.push_back(e);
			}
		}
		return out;
	}
};

GameLogicSettings &settingsOf(P66World &w) { return w.logic->settings(); }

void clearSettings(GameLogicSettings &s)
{
	s.autoFireSmall = GameLogicSettings::AutoBodyParticles();
	s.autoFireMedium = GameLogicSettings::AutoBodyParticles();
	s.autoFireLarge = GameLogicSettings::AutoBodyParticles();
	s.autoSmokeSmall = GameLogicSettings::AutoBodyParticles();
	s.autoSmokeMedium = GameLogicSettings::AutoBodyParticles();
	s.autoSmokeLarge = GameLogicSettings::AutoBodyParticles();
	s.autoAflame = GameLogicSettings::AutoBodyParticles();
}

// the seven entries the way the binary fills them for `aflame` (RW 0x8C1FDD .. 0x8C202F) from the settings
void expectEntries(const GameLogicSettings &s, bool aflame, std::vector<std::string> &systems, std::vector<int> &caps)
{
	const GameLogicSettings::AutoBodyParticles *e[7] = { &s.autoFireSmall, &s.autoFireMedium, &s.autoFireLarge, &s.autoSmokeSmall,
		&s.autoSmokeMedium, &s.autoSmokeLarge, &s.autoAflame };
	systems.clear();
	caps.clear();
	const int k = aflame ? 2 : 1;
	systems.push_back(aflame ? s.autoFireMedium.system : s.autoFireSmall.system);
	systems.push_back(aflame ? s.autoFireLarge.system : s.autoFireMedium.system);
	systems.push_back(s.autoFireLarge.system);
	systems.push_back(aflame ? s.autoFireSmall.system : s.autoSmokeSmall.system);
	systems.push_back(aflame ? s.autoFireSmall.system : s.autoSmokeMedium.system);
	systems.push_back(aflame ? s.autoFireSmall.system : s.autoSmokeLarge.system);
	systems.push_back(s.autoAflame.system);
	for (int i = 0; i < 7; ++i)
	{
		caps.push_back(e[i]->maxSystems * k);
	}
}

// the number of systems a client makes for one entry: min(the bones found, the cap) (RW 0x8C23EA)
int expectedSystems(const FXEvent &e, int entry)
{
	if (entry >= (int)e.particleSystems.size())
	{
		return 0;
	}
	const FXEvent::BodyParticleEntry &p = e.particleSystems[(size_t)entry];
	if (p.system.empty())
	{
		return 0;
	}
	const int bones = (int)p.bones.size();
	return bones < p.maxSystems ? bones : p.maxSystems;
}
} // namespace

TEST_CASE("PORT-66 gamedata: the 21 Auto* rows are read (rw 0xC00150 .. 0xC00290, GlobalData +0x9F0 .. +0xA40) and leave the unapplied set")
{
	P66World w;
	const GameLogicSettings &s = settingsOf(w);
	// RW 0xC00150 .. 0xC00290: the field table's own order, three rows per size, the offsets 8 apart
	CHECK(s.autoFireSmall.prefix == "FIRESMALL");
	CHECK(s.autoFireSmall.system == "FireSmallSys");
	CHECK(s.autoFireSmall.maxSystems == 2);
	CHECK(s.autoFireMedium.prefix == "FIREMED");
	CHECK(s.autoFireMedium.system == "FireMedSys");
	CHECK(s.autoFireMedium.maxSystems == 3);
	CHECK(s.autoFireLarge.prefix == "FIRELARGE");
	CHECK(s.autoFireLarge.system == "FireLargeSys");
	CHECK(s.autoFireLarge.maxSystems == 4);
	CHECK(s.autoSmokeSmall.prefix == "SMOKESMALL");
	CHECK(s.autoSmokeSmall.system == "SmokeSmallSys");
	CHECK(s.autoSmokeSmall.maxSystems == 5);
	CHECK(s.autoSmokeMedium.prefix == "SMOKEMED");
	CHECK(s.autoSmokeMedium.system == "SmokeMedSys");
	CHECK(s.autoSmokeMedium.maxSystems == 6);
	CHECK(s.autoSmokeLarge.prefix == "SMOKELARGE");
	CHECK(s.autoSmokeLarge.system == "SmokeLargeSys");
	CHECK(s.autoSmokeLarge.maxSystems == 7);
	CHECK(s.autoAflame.prefix == "AFLAME");
	CHECK(s.autoAflame.system == "AflameSys");
	CHECK(s.autoAflame.maxSystems == 1);
	// the rows are read, not recorded as unapplied (S-152's accounting)
	for (const char *n : { "AutoFireParticleSmallPrefix", "AutoFireParticleSmallSystem", "AutoFireParticleSmallMax", "AutoFireParticleMediumPrefix",
			 "AutoFireParticleMediumSystem", "AutoFireParticleMediumMax", "AutoFireParticleLargePrefix", "AutoFireParticleLargeSystem",
			 "AutoFireParticleLargeMax", "AutoSmokeParticleSmallPrefix", "AutoSmokeParticleSmallSystem", "AutoSmokeParticleSmallMax",
			 "AutoSmokeParticleMediumPrefix", "AutoSmokeParticleMediumSystem", "AutoSmokeParticleMediumMax", "AutoSmokeParticleLargePrefix",
			 "AutoSmokeParticleLargeSystem", "AutoSmokeParticleLargeMax", "AutoAflameParticlePrefix", "AutoAflameParticleSystem", "AutoAflameParticleMax" })
	{
		CHECK_MESSAGE(s.unappliedFields.count(std::string("GameData.") + n) == 0, n);
	}
	// the GlobalData constructor's zero (BFME2 decomp GlobalData.cpp:1558-1564): a GameData without the rows has no bones and no template
	GameLogicSettings none;
	std::string err;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData("GameData\n  ForceModelsToFollowTimeOfDay = No\n  ForceModelsToFollowWeather = No\n"
														  "  UnitDamagedThreshold = 50%\n  UnitReallyDamagedThreshold = 25%\n  DefaultStartingCash = 1\nEnd\n",
						none, &err),
		err);
	CHECK(none.autoFireSmall.system.empty());
	CHECK(none.autoFireSmall.maxSystems == 0);
	CHECK(none.autoAflame.prefix.empty());
}

TEST_CASE("PORT-66 events: a body names the seven entries in the binary's order with the aflame swap and the doubled caps (rw 0x8C1E4E)")
{
	P66World w;
	Object *tower = w.unit("P66Torch", 'A', 400, 300);
	REQUIRE(tower != nullptr);
	// not aflame: the plain templates, the modifier 1 (RW 0x8C2002 .. 0x8C202A), six calls (RW 0x8C20F5 skips the seventh)
	tower->getBodyModule()->updateBodyParticleSystems();
	std::vector<FXEvent> ev = w.bodyEvents(tower->getID());
	REQUIRE(ev.size() == 1);
	REQUIRE(ev[0].particleSystems.size() == 6u);
	std::vector<std::string> systems;
	std::vector<int> caps;
	expectEntries(settingsOf(w), false, systems, caps);
	for (int i = 0; i < 6; ++i)
	{
		INFO(i);
		CHECK(ev[0].particleSystems[(size_t)i].system == systems[(size_t)i]);
		CHECK(ev[0].particleSystems[(size_t)i].maxSystems == caps[(size_t)i]); // Auto*Max * 1
	}
	// the bones: the prefix's numbered bones, in the numbered order (RW 0x68C650 / RW 0x672A73), with their model-space positions
	REQUIRE(ev[0].particleSystems[kFireSmall].bones.size() == 3u);
	CHECK(ev[0].particleSystems[kFireSmall].bones[0].name == "FIRESMALL01");
	CHECK(ev[0].particleSystems[kFireSmall].bones[2].name == "FIRESMALL03");
	// exact: the pin's bone matrices are exact binary fractions (the translation copied out of element 3 / 7 / 11 of RW 0x68C650's matrix)
	CHECK(ev[0].particleSystems[kFireSmall].bones[2].position.x == 10.0f);
	CHECK(ev[0].particleSystems[kFireSmall].bones[2].position.y == 2.0f);
	REQUIRE(ev[0].particleSystems[kFireMedium].bones.size() == 2u);
	CHECK(ev[0].particleSystems[kFireMedium].bones[1].name == "FIREMED02");
	CHECK(ev[0].particleSystems[kSmokeSmall].bones.empty()); // SMOKESMALL has no bones on this model
	CHECK(ev[0].particleSystems[kSmokeLarge].bones.size() == 1u);
	// aflame: fire small := FireMedSys, fire medium := FireLargeSys, the three smoke := FireSmallSys, the caps doubled, and the seventh entry
	// (RW 0x8C1FDD .. 0x8C2000, RW 0x8C20F5 .. 0x8C2126)
	tower->setStatus(10, true);
	tower->getBodyModule()->updateBodyParticleSystems();
	ev = w.bodyEvents(tower->getID());
	REQUIRE(ev.size() == 1); // the frame's list: one event per call
	REQUIRE(ev[0].particleSystems.size() == 7u);
	expectEntries(settingsOf(w), true, systems, caps);
	for (int i = 0; i < 7; ++i)
	{
		INFO(i);
		CHECK(ev[0].particleSystems[(size_t)i].system == systems[(size_t)i]);
		CHECK(ev[0].particleSystems[(size_t)i].maxSystems == caps[(size_t)i]); // Auto*Max * 2
	}
	CHECK(ev[0].particleSystems[kFireSmall].system == "FireMedSys");
	CHECK(ev[0].particleSystems[kSmokeSmall].system == "FireSmallSys");
	CHECK(ev[0].particleSystems[kAflame].system == "AflameSys");
	CHECK(ev[0].particleSystems[kAflame].maxSystems == 2); // AutoAflameParticleMax 1 * 2
	// the systems a client makes: min(the bones found, the cap); an empty template or no bones makes nothing (RW 0x8C2401 / RW 0x8C2458)
	CHECK(expectedSystems(ev[0], kFireSmall) == 2);  // 3 bones, the cap 2 * 2 = 4
	CHECK(expectedSystems(ev[0], kFireMedium) == 2); // 2 bones
	CHECK(expectedSystems(ev[0], kFireLarge) == 0);  // FIRELARGE has no bones
	CHECK(expectedSystems(ev[0], kSmokeSmall) == 0);
	CHECK(expectedSystems(ev[0], kSmokeLarge) == 1);
	CHECK(expectedSystems(ev[0], kAflame) == 1);
}

TEST_CASE("PORT-66 body: a body class whose slot 0x2C is the empty stub makes nothing (rw 0x9F3A3C), a null bone provider answers no bones")
{
	P66World w;
	Object *inert = w.unit("P66Inert", 'A', 400, 350);
	REQUIRE(inert != nullptr);
	REQUIRE(inert->getBodyModule() != nullptr);
	const unsigned long long before = BodyModuleInterface::bodyParticleCallsIgnored();
	inert->getBodyModule()->updateBodyParticleSystems();
	CHECK(BodyModuleInterface::bodyParticleCallsIgnored() == before + 1); // RW 0x9F3A3C (`ret`): the call reaches the body and nothing happens
	CHECK(w.bodyEvents(inert->getID()).empty());
	// without a bone provider (a peer without a drawable) the body still emits, with no bones: no system can be made
	w.combat().setLaunchOffsets(nullptr);
	Object *torch = w.unit("P66Torch", 'A', 420, 300);
	REQUIRE(torch != nullptr);
	torch->getBodyModule()->updateBodyParticleSystems();
	const std::vector<FXEvent> ev = w.bodyEvents(torch->getID());
	REQUIRE(ev.size() == 1);
	REQUIRE(ev[0].particleSystems.size() == 6u);
	for (const FXEvent::BodyParticleEntry &p : ev[0].particleSystems)
	{
		CHECK(p.bones.empty());
	}
	CHECK(expectedSystems(ev[0], kFireSmall) == 0);
}

TEST_CASE("PORT-66 flammable: ignition and the quench call the body's slot 0x2C (rw 0x8901E2 / rw 0x890124)")
{
	P66World w;
	Object *torch = w.unit("P66Torch", 'A', 440, 300);
	REQUIRE(torch != nullptr);
	FlammableUpdate *flam = nullptr;
	for (const std::unique_ptr<BehaviorModule> &m : torch->modules())
	{
		if (FlammableUpdate *f = dynamic_cast<FlammableUpdate *>(m.get()))
		{
			flam = f;
		}
	}
	REQUIRE(flam != nullptr);
	REQUIRE_FALSE(torch->testStatus(10));
	flam->tryToIgnite();
	// RW 0x8901D5 sets the status, RW 0x8901DA .. 0x8901E2 calls the body: the event is already the aflame one
	CHECK(torch->testStatus(10));
	std::vector<FXEvent> ev = w.bodyEvents(torch->getID());
	REQUIRE(ev.size() == 1);
	CHECK(ev[0].particleSystems.size() == 7u);
	CHECK(ev[0].particleSystems[kAflame].system == "AflameSys");
	CHECK(ev[0].particleSystems[kFireSmall].system == "FireMedSys");
	// the quench (onHealing with the FlameDamageLimit passed): the status clears first, then the body is called again with the plain set
	const std::string limitField = "FlameDamageLimit";
	(void)limitField;
	DamageInfo heal;
	heal.m_input.m_amount = 1000.0f;
	heal.m_input.m_damageType = DAMAGE_HEALING;
	flam->onHealing(heal);
	CHECK_FALSE(torch->testStatus(10));
	ev = w.bodyEvents(torch->getID());
	REQUIRE(ev.size() == 1);
	CHECK(ev[0].particleSystems.size() == 6u); // the aflame entry is gone (RW 0x8C20F5's test)
	CHECK(ev[0].particleSystems[kFireSmall].system == "FireSmallSys");
	CHECK(ev[0].particleSystems[kSmokeSmall].system == "SmokeSmallSys");
}

TEST_CASE("PORT-66 collapse: the collapse's final phase calls the body's slot 0x2C once the rubble flags are set (rw 0x8A8031)")
{
	P66World w;
	Object *barracks = w.unit("P66Barracks", 'A', 460, 300);
	REQUIRE(barracks != nullptr);
	const ObjectID id = barracks->getID();
	// a collapse needs a killing hit: the fall reaches the ground after ~5 frames (gravity -2.56 per frame squared, CollapseDamping 0.5)
	barracks->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	std::vector<FXEvent> seen;
	for (int i = 0; i < 40; ++i)
	{
		for (int phase = 1; phase <= 4; ++phase)
		{
			w.logic->update(phase); // one engine tick (RW 0x62E4E8, phases 1..6; 1..4 moves and collapses the object)
		}
		for (const FXEvent &e : w.bodyEvents(id))
		{
			seen.push_back(e);
		}
		if (!w.logic->findObjectByID(id))
		{
			break;
		}
	}
	// the rubble state was reached and the body named its fires for it; without the fix no such event exists (the collapse never called the slot)
	REQUIRE_FALSE(seen.empty());
	int aflameEntry = 0;
	for (const FXEvent &e : seen)
	{
		if (e.particleSystems.size() == 7u)
		{
			++aflameEntry;
		}
		// a body with no AFLAME status names exactly the six plain entries (RW 0x8C20F5 skips the seventh)
		REQUIRE(e.particleSystems.size() == 6u);
	}
	// the barracks has no AFLAME status, so every call is the six-entry set
	CHECK(aflameEntry == 0);
	CHECK(seen[0].particleSystems[kFireSmall].system == "FireSmallSys");
	CHECK(seen[0].particleSystems[kFireLarge].maxSystems == 4);
}

TEST_CASE("PORT-66 hash: the auto particle entries are simulation state (rw 0x8C1E4E reads them); an empty set leaves the hash of a retail game alone")
{
	P66World w;
	auto hashWith = [&](void (*mutate)(GameLogicSettings &)) {
		P66World f;
		mutate(settingsOf(f));
		StateHasher h;
		settingsOf(f).crc(h);
		return h.value();
	};
	const std::uint32_t base = hashWith([](GameLogicSettings &) {});
	CHECK(hashWith([](GameLogicSettings &) {}) == base);
	CHECK(hashWith([](GameLogicSettings &s) { s.autoFireSmall.system = "Other"; }) != base);
	CHECK(hashWith([](GameLogicSettings &s) { s.autoFireSmall.prefix = "OTHER"; }) != base);
	CHECK(hashWith([](GameLogicSettings &s) { s.autoAflame.maxSystems = 3; }) != base);
	CHECK(hashWith([](GameLogicSettings &s) { s.autoSmokeLarge.maxSystems = 8; }) != base);
	// an empty set hashes the same as another empty set: a game whose data names none keeps its hash whatever the defaults are
	P66World a, b;
	clearSettings(settingsOf(a));
	clearSettings(settingsOf(b));
	settingsOf(b).autoFireSmall.prefix = "X"; // a non-empty set differs
	settingsOf(b).autoFireSmall.prefix.clear();
	StateHasher ha, hb;
	settingsOf(a).crc(ha);
	settingsOf(b).crc(hb);
	CHECK(ha.value() == hb.value());
}
