// OpenBFME unit tests (lane PORT-335, fork issue 890). GPL-3.0.
//
// The last uncited own functions of AttributeModifierPoolUpdate, ObjectCreationUpgrade, ActivateModuleSpecialPower, RousingSpeechUpdate,
// BloodthirstyUpdate and ThreatFinderUpdate. Every expectation below comes from the RotWK 2.01 instruction named next to it (workspace/RotWK/game.dat
// through tools/mod/rw.py) and from the mod's own legend, not from the port's code; every test runs on synthetic INI, with no game files.
//
// The two branches this lane changed are the legend loader's (RW 0x005B4B9A): tests 1 and 2 fail without them. Tests 3 and 4 pin the two rows whose
// citation this lane added or widened (RW 0x0068C878, RW 0x00805E89) so a later change cannot drop the mechanism they name.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/INI/INIBlockStubs.h"
#include "Common/StateHash.h"
#include "Common/SubsystemLegend.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BloodthirstyUpdate.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/WeaponStores.h"

#include <set>
#include <string>
#include <vector>

namespace
{

typedef std::vector<std::string> Strs;

// ---- the legend loader (RW 0x005B4B9A), on a synthetic tree ------------------------------------------------------------

// Every synthetic file holds one block a real parser answers, so a load is observable through INI::loadedFiles.
const char *const kBody = "Armor X\nEnd\n";

struct LegendWorld : initest::Fixture
{
	WeaponStores stores;
	explicit LegendWorld(const initest::FileList &files)
		: initest::Fixture(files)
	{
		stores.install();
	}
};

std::set<std::string> lowered(const Strs &v)
{
	std::set<std::string> out;
	for (const std::string &s : v)
	{
		out.insert(s.size() >= 1 ? std::string(s) : s);
	}
	return out;
}

// RotWK's own TheThingFactory entry (Data\INI\Default\SubsystemLegendExpansion1.ini:139-145): one InitFile, one InitPath, two ExcludePaths that name
// single files, one IncludePathCinematics. The synthetic tree below is that entry with the names shortened.
const char *const kThingFactoryLegend =
	"LoadSubsystem TheThingFactory335\n"
	"  Loader = INI\n"
	"  InitFile = Data\\INI\\Default\\Object.ini\n"
	"  InitFile = Data\\INI\\CinematicWeapon.ini\n"
	"  InitPath = Data\\INI\\Object\n"
	"  ExcludePath = Data\\INI\\CinematicWeapon.ini\n"
	"  ExcludePath = Data\\INI\\Object\\Cinematic\\CinematicLocomotor.ini\n"
	"End\n"
	"LoadSubsystem TheNothing335\n"
	"  Loader = INI\n"
	"  InitFile = Data\\INI\\Excluded.ini\n"
	"  ExcludePath = Data\\INI\\Excluded.ini\n"
	"End\n";

initest::FileList legendTree()
{
	return {
		{ "Data\\INI\\Default\\Object.ini", kBody },
		{ "Data\\INI\\CinematicWeapon.ini", kBody },
		{ "Data\\INI\\Excluded.ini", kBody },
		{ "Data\\INI\\Object\\Units.ini", kBody },
		{ "Data\\INI\\Object\\Cinematic\\CinematicLocomotor.ini", kBody },
	};
}

} // namespace

TEST_CASE("PORT-335 legend: an ExcludePath names a single InitFile too, not only a directory item (RW 0x005B4C69 -> RW 0x0042C2B5)")
{
	LegendWorld fx(legendTree());
	INIBlockRecorder rec;
	SubsystemLegend sl;
	RegisterRecordingBlockStubs(fx.env.blocks, rec, { "LoadSubsystem" });
	sl.registerBlock(fx.env.blocks);
	REQUIRE(initest::loadError(fx.env, "legend.ini", kThingFactoryLegend).empty());

	INI ini(fx.env);
	SubsystemLoadOptions options; // no -cinematics, the normal launch
	SubsystemLoadReport report;
	REQUIRE(sl.loadIniFilesFromLegend("TheThingFactory335", ini, options, &report));

	// RW builds ONE exclusion vector (0x005B4C06 .. 0x005B4C42) and tests every InitFile against it (0x005B4C6E), so the InitFile the entry excludes
	// by name is not loaded; the port used to run only the IncludePathCinematics part of the vector over the InitFile list.
	CHECK(lowered(ini.loadedFiles()) == std::set<std::string>{ "Data\\INI\\Default\\Object.ini", "Data\\INI\\Object\\Units.ini" });
	// the directory walk prunes with the same vector (RW 0x005B4CAE hands it to RW 0x0042E343): the excluded file inside InitPath is not loaded either
	CHECK(report.skippedCinematics.empty()); // the entry has no IncludePathCinematics, so nothing is reported as a cinematic skip
}

TEST_CASE("PORT-335 legend: the entry's answer is true only when a load ran (RW 0x005B4C99, RW 0x005B4CD4)")
{
	LegendWorld fx(legendTree());
	INIBlockRecorder rec;
	SubsystemLegend sl;
	RegisterRecordingBlockStubs(fx.env.blocks, rec, { "LoadSubsystem" });
	sl.registerBlock(fx.env.blocks);
	REQUIRE(initest::loadError(fx.env, "legend.ini", kThingFactoryLegend).empty());

	INI ini(fx.env);
	SubsystemLoadOptions options;
	// The return byte [ebp-0xD] becomes 1 only inside the two loops, after RW 0x0042D97D / RW 0x0042E343 answered true; an InitFile the exclusion
	// vector names jumps over it (0x005B4C77). The caller (RW 0x00620E4E) reads the byte to decide whether the subsystem's hard-coded paths still run,
	// so an entry whose every file is excluded must answer false: the port answered true, having set the flag before the exclusion test.
	CHECK_FALSE(sl.loadIniFilesFromLegend("TheNothing335", ini, options, nullptr));
	CHECK(ini.loadedFiles().empty());
	CHECK(sl.loadIniFilesFromLegend("TheThingFactory335", ini, options, nullptr));
	CHECK_FALSE(ini.loadedFiles().empty());
	CHECK_FALSE(sl.loadIniFilesFromLegend("TheAbsent335", ini, options, nullptr)); // no such legend entry: RW 0x005B4BF3
}

namespace
{

// ---- the object and module worlds --------------------------------------------------------------------------------------

// CombatWorld registers the logic modules (MoveTestUtil.h:175); the hero-ability ones, which carry BloodthirstyUpdate (HeroAbilityModules.cpp:2494),
// and the attribute modifier store the pool reads are added here, before the lane's own Object blocks are parsed.
struct World335 : combattest::CombatWorld
{
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	explicit World335(const char *objects)
		: combattest::CombatWorld("")
	{
		HeroAbilityModules::registerAll(w.modules);
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		const std::string err = w.load(objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~World335() { TheAttributeModifierStore = savedModifiers; }
};

const char *const kBloodObjects =
	"Object Plain335\n"
	"  KindOf = HORDE INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n   MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n   Locomotor = WalkerLoco\n   Condition = SET_NORMAL\n   Speed = 55\n  End\n"
	"End\n"
	"Object Horde335\n"
	"  KindOf = HORDE INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n   MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n   Locomotor = WalkerLoco\n   Condition = SET_NORMAL\n   Speed = 55\n  End\n"
	"  Behavior = BloodthirstyUpdate BloodTag\n   SacrificeFilter = ALL\n   NumToSacrifice = 2\n  End\n"
	"End\n";

const char *const kPoolObjects =
	"ModifierList Pool335Leadership\n  Category = LEADERSHIP\n  Duration = 600000\n  Modifier = ARMOR 150%\nEnd\n"
	"ModifierList Pool335Weapon\n  Category = WEAPON\n  Duration = 20000\n  Modifier = RECHARGE_TIME 50%\nEnd\n"
	"Object Pool335\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n   MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n   Locomotor = WalkerLoco\n   Condition = SET_NORMAL\n   Speed = 55\n  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"
	"End\n";

AttributeModifierPool *poolOf(Object *o)
{
	auto *pool = dynamic_cast<AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate"));
	REQUIRE(pool != nullptr);
	return pool;
}

} // namespace

TEST_CASE("PORT-335 bloodthirsty: the object asks every module for interface slot 37, the first that answers wins (RW 0x0068C878)")
{
	World335 f(kBloodObjects);
	Object *plain = f.unit("Plain335", 'A', 100, 100);
	Object *horde = f.unit("Horde335", 'A', 200, 100);
	REQUIRE(plain);
	REQUIRE(horde);

	// RW 0x0068C878 walks the object's NULL-terminated BehaviorModule array (Object + 0x24C in RotWK, m_behaviors + 0x244 in the BFME2 donor) and calls
	// slot 37 (offset 0x94) of each module's + 0xC interface, taking the first non-null answer. Of the 166 vtables RotWK writes at module + 0xC exactly
	// one holds anything but the null stub RW 0x00851E97 there: BloodthirstyUpdate's RW 0xC55688, whose slot 37 is RW 0x008A18E0 (module + 0x20).
	CHECK(plain->getBloodthirstyUpdateInterface() == nullptr);
	BloodthirstyUpdate *found = horde->getBloodthirstyUpdateInterface();
	REQUIRE(found != nullptr);
	// the same module the class-name lookup and the sacrifice command's own finder answer with: the scan replaced the name, it did not add a second path
	CHECK(found == BloodthirstyUpdate::find(*horde));
	CHECK(found == dynamic_cast<BloodthirstyUpdate *>(horde->findModule("BloodthirstyUpdate")));

	// and it is not the object's first module: the AI update stands in front of it in the same array, so the answer comes from the walk and not from a
	// cached first slot
	const auto &mods = horde->modules();
	const BehaviorModule *ai = horde->findModule("AIUpdateInterface");
	REQUIRE(ai);
	size_t aiAt = mods.size();
	size_t bloodAt = mods.size();
	for (size_t i = 0; i < mods.size(); ++i)
	{
		if (mods[i].get() == ai)
		{
			aiAt = i;
		}
		if (mods[i].get() == found)
		{
			bloodAt = i;
		}
	}
	// tolerance: the array also holds the object's helper modules, whose number is not what this check is about; only the two modules' order is
	CHECK(aiAt < bloodAt);
	CHECK(horde->getAIUpdateInterface());
}

TEST_CASE("PORT-335 pool: the snapshot carries every field retail's xfer saves (RW 0x00805E89)")
{
	World335 f(kPoolObjects);
	Object *o = f.unit("Pool335", 'A', 100, 100);
	REQUIRE(o);
	AttributeModifierPool *pool = poolOf(o);

	// two lists whose Duration differs: 600000 ms and 20000 ms, and INI::parseDurationUnsignedInt (RW 0x73A429) scales ms by 0.005, i.e. 200 ms per
	// logic frame, so the entries differ in the expiry frame retail saves at entry + 8
	const unsigned frame = f.logic->getFrame();
	REQUIRE(pool->add("Pool335Leadership", -1));
	REQUIRE(pool->add("Pool335Weapon", -1));
	REQUIRE(pool->entryCount() == 2);
	const unsigned leadershipExpire = pool->expireOf("Pool335Leadership");
	const unsigned weaponExpire = pool->expireOf("Pool335Weapon");
	CHECK(leadershipExpire == frame + 3000);
	CHECK(weaponExpire == frame + 100);
	f.frames(3);
	// RW 0x00804FCC: the 15 category frames at this + 0x30 .. this + 0x68, the block retail's xfer writes as 15 dwords (0x805F8C .. 0x805FAF)
	const unsigned until = f.logic->getFrame() + 500;
	pool->disableCategories(0x000Fu, until);

	StateHasher before;
	pool->crc(before);

	XferSave save;
	pool->xfer(save);
	REQUIRE(!save.data().empty());

	// a second, untouched pool of the same template takes the snapshot: the store is the same INI, so the entries' indices resolve
	World335 g(kPoolObjects);
	Object *o2 = g.unit("Pool335", 'A', 100, 100);
	REQUIRE(o2);
	AttributeModifierPool *loaded = poolOf(o2);
	REQUIRE(loaded->entryCount() == 0);
	XferLoad load(save.data());
	loaded->xfer(load);

	// the deque retail saves at this + 0x20 (entry size 0x10, RW 0x00805F39 reads the count as (this + 0x24 - this + 0x20) >> 4)
	CHECK(loaded->listNames() == Strs{ "Pool335Leadership", "Pool335Weapon" });
	CHECK(loaded->expireOf("Pool335Leadership") == leadershipExpire);
	CHECK(loaded->expireOf("Pool335Weapon") == weaponExpire);
	// the 15 category frames: the four the mask named, and the eleven it did not
	for (int c = 0; c < 4; ++c)
	{
		CHECK(loaded->categoryDisabledUntil(c) == until);
	}
	for (int c = 4; c < 15; ++c)
	{
		CHECK(loaded->categoryDisabledUntil(c) == 0u);
	}
	// the whole state: the crc covers the entries, the wake frame this + 0x2C and both category tables, so an equal crc says nothing was dropped
	StateHasher after;
	loaded->crc(after);
	CHECK(after.getCRC() == before.getCRC());

	// and the loaded pool behaves like the saved one: the suppressed category still refuses a list of its own (RW 0x00804D27's test, lane SPELL-2)
	CHECK(loaded->categoryCount(0) == pool->categoryCount(0));
}
