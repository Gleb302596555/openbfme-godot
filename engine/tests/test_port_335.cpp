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

#include "Common/AsciiString.h"
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

std::set<std::string> lowerSet(const Strs &v)
{
	std::set<std::string> out;
	for (const std::string &s : v)
	{
		out.insert(AsciiStringUtil::lowered(s));
	}
	return out;
}

// one line per loaded file, so a mismatch names the files instead of doctest's {?}
std::string joined(const Strs &v)
{
	std::string out;
	for (const std::string &s : v)
	{
		if (!out.empty())
		{
			out += " | ";
		}
		out += s;
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
	INFO("loaded: " << joined(ini.loadedFiles()));
	CHECK(lowerSet(ini.loadedFiles()) == std::set<std::string>{ "data\\ini\\default\\object.ini", "data\\ini\\object\\units.ini" });
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
	"  Behavior = BloodthirstyUpdate BloodTag\n   SacrificeFilter = ALL\n  End\n"
	"End\n";

const char *const kPoolObjects =
	"ModifierList Pool335Leadership\n  Category = LEADERSHIP\n  Duration = 600000\n  IgnoreIfAnticategoryActive = Yes\n  Modifier = ARMOR 150%\nEnd\n"
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
	// slot 37 (offset 0x94) of each module's + 0xC interface, taking the first non-null answer. Of the 166 candidate tables RotWK's write pattern
	// `mov dword ptr [reg + 0xC], imm32` gives, slot 37 holds RW 0x008A18E0 (module + 0x20) in exactly one - BloodthirstyUpdate's RW 0xC55688, written
	// by its constructor 0x00850CB0 and its destructor 0x00850DA9 - and the null stub RW 0x00851E97 in 65, so the walk can only answer with a
	// sacrifice interface.
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

	// the same module, emptied first: its base registration state (UpdateModule::crc's m_indexInLogic and m_phaseInLogic, which no snapshot carries)
	// stays the one the saved crc was taken with, so an equal crc afterwards says the snapshot carried every field the pool itself owns
	pool->clearEntries(); // RW 0x006998D0: the entry vector goes in place, without the per-entry removal of RW 0x8052FB
	pool->disableCategories(0x7FFFu, 0u);
	REQUIRE(pool->entryCount() == 0);
	REQUIRE(pool->categoryDisabledUntil(1) == 0u);

	XferLoad load(save.data());
	pool->xfer(load);
	CHECK(load.atEnd());

	// the deque retail saves at this + 0x20 (entry size 0x10; RW 0x00805F39 reads the count as (this + 0x24 - this + 0x20) >> 4)
	CHECK(pool->listNames() == Strs{ "Pool335Leadership", "Pool335Weapon" });
	CHECK(pool->expireOf("Pool335Leadership") == leadershipExpire);
	CHECK(pool->expireOf("Pool335Weapon") == weaponExpire);
	// the 15 category frames, this + 0x30 .. this + 0x68: the four the mask named, and the eleven it did not
	for (int c = 0; c < 4; ++c)
	{
		CHECK(pool->categoryDisabledUntil(c) == until);
	}
	for (int c = 4; c < 15; ++c)
	{
		CHECK(pool->categoryDisabledUntil(c) == 0u);
	}
	StateHasher after;
	pool->crc(after);
	CHECK(after.value() == before.value());

	// a second, untouched pool of the same template takes the same snapshot: the store is the same INI, so the entries' indices resolve, and the
	// restored category frames answer the suppressed test of RW 0x804D27 (lane SPELL-2) - LEADERSHIP is category 1, WEAPON category 4
	World335 g(kPoolObjects);
	Object *o2 = g.unit("Pool335", 'A', 100, 100);
	REQUIRE(o2);
	AttributeModifierPool *loaded = poolOf(o2);
	REQUIRE(loaded->entryCount() == 0);
	XferLoad second(save.data());
	loaded->xfer(second);
	CHECK(second.atEnd());
	CHECK(loaded->listNames() == Strs{ "Pool335Leadership", "Pool335Weapon" });
	CHECK(loaded->expireOf("Pool335Leadership") == leadershipExpire);
	CHECK(loaded->expireOf("Pool335Weapon") == weaponExpire);
	for (int c = 0; c < 4; ++c)
	{
		CHECK(loaded->categoryDisabledUntil(c) == until);
	}
	for (int c = 4; c < 15; ++c)
	{
		CHECK(loaded->categoryDisabledUntil(c) == 0u);
	}
	// the counters at this + 0x6C: retail derives them at load (RW 0x008059D0's walk), the snapshot carries them, and both pools agree
	CHECK(loaded->categoryCount(1) == pool->categoryCount(1));
	CHECK(loaded->categoryCount(1) == 1);
	CHECK(loaded->categoryCount(4) == 1);
	// and the loaded pool behaves like the saved one: the LEADERSHIP list, whose IgnoreIfAnticategoryActive is set, is refused while its category
	// frame is in the future; the WEAPON list, whose category the mask did not name, is not
	CHECK_FALSE(loaded->add("Pool335Leadership", -1));
	CHECK(loaded->categoryCount(1) == 1);
	CHECK(loaded->add("Pool335Weapon", -1));
}
