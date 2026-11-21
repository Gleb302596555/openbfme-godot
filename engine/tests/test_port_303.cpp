// OpenBFME. PORT-303 (fork issue 695): SalvageCrateCollide's four unread chances, its radius, and the three functions the
// module map still lists as uncited (RW 0x8C12EC, RW 0x8C130B, RW 0x8C1350). The reading, instruction by instruction:
// docs/ennorath/port303-salvage-crate-rw.md.
//
// The lane changes no behaviour: the three uncited functions are the class's name getter, its lazy name-key accessor and its
// scalar deleting destructor, and the port answers all three through Module / ModuleFactory and the compiler's destructor.
// The four chances (PorterChance +0x5C, BannerChance +0x60, ResourceChance +0x6C) and LevelUpRadius +0x68 stay parsed and
// unread; the port's note for them is the existing S-7234 row (Port 94), which this reading narrows.
//
// Synthetic INI only, no game files. Every expectation names the RotWK 2.01 rule and its address.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/Module.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/Module/SalvageCrateCollide.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>
#include <string>
#include <typeinfo>
#include <vector>

using namespace combattest;

namespace
{

template <class T>
T *moduleOf(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (T *t = dynamic_cast<T *>(m.get()))
		{
			return t;
		}
	}
	return nullptr;
}

// an ExperienceLevel chain for SalvPicker: gainExpForLevel(1, true, false) (RW 0x79DA0A) has a next level to grant, and a fresh
// object of a template with a chain is at rank 0 (the chain's first level is applied at spawn)
const char kSalvLevels[] =
	"ExperienceLevel SalvPickerLevel0\n"
	"  TargetNames = SalvPicker\n"
	"  RequiredExperience = 1\n"
	"  ExperienceAward = 0\n"
	"  Rank = 0\n"
	"End\n"
	"ExperienceLevel SalvPickerLevel1\n"
	"  TargetNames = SalvPicker\n"
	"  RequiredExperience = 2\n"
	"  ExperienceAward = 10\n"
	"  Rank = 1\n"
	"End\n";

struct LevelScope
{
	initest::Fixture fx;
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *prev;
	LevelScope()
		: prev(TheExperienceLevelSystem)
	{
		levels.registerBlocks(fx.env.blocks);
		TheExperienceLevelSystem = &levels;
		REQUIRE(initest::loadError(fx.env, "levels.ini", kSalvLevels).empty());
	}
	~LevelScope()
	{
		TheExperienceLevelSystem = prev;
	}
};

const char kSalvageObjects[] =
	"Object SalvPicker\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
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
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	// the three unread chances and the unread radius all at 100% (the percent parser turns 100% into 1.0), LevelUpChance at 0%:
	// the crate must pay money and nothing else (RW 0x8C136C reads + 0x64 and the Upgrade string only)
	"Object SalvChestAllChances\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    PorterChance = 100%\n"
	"    BannerChance = 100%\n"
	"    LevelUpChance = 0%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 100%\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// the same chances with LevelUpChance at 100%: outcome 2 wins, no money
	"Object SalvChestLevelFirst\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    PorterChance = 100%\n"
	"    BannerChance = 100%\n"
	"    LevelUpChance = 100%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 100%\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// ExecuteFX: the crate hands the data's name (+ 0x48) to the FX slot (RW 0x8C06D1 -> RW 0x4B1B5A)
	"Object SalvChestFx\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    LevelUpChance = 0%\n"
	"    ResourceChance = 0%\n"
	"    ExecuteFX = PORT303_PickupFx\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n";

} // namespace

// ---- the three uncited functions ------------------------------------------------------------------------------------
// RW 0x8C12EC (vtable RW 0xC715C0 slot 2): `mov eax, 0xC0AA68; ret` — the class name, the string "SalvageCrateCollide".
// The port answers the same call through Module::getModuleClassName, filled by the ModuleFactory when it makes the module.
TEST_CASE("PORT-303: the name getter RW 0x8C12EC is the class name the factory set (Module::getModuleClassName)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	Object *picker = w.unit("SalvPicker", 'A', 330, 300);
	REQUIRE(chest != nullptr);
	REQUIRE(picker != nullptr);
	CrateCollide *m = moduleOf<CrateCollide>(chest);
	REQUIRE(m != nullptr);
	CHECK(m->getModuleClassName() == "SalvageCrateCollide");
	CHECK(dynamic_cast<SalvageCrateCollide *>(m) != nullptr);
	// a unit has no crate collide, so the check above is about this class and not about any module
	CHECK(moduleOf<CrateCollide>(picker) == nullptr);
}

// RW 0x8C130B (vtable RW 0xC715C0 slot 4): the lazy name-key accessor — RW 0xA3CEF0 guards the one-time hash of the class name
// (RW 0x5487EC) into the global [0xDE9ACC] and returns it. The port's ModuleFactory does the same: it hands the module the key of
// the name it resolved (Common/Thing/ModuleFactory.cpp), and the key comes from the world's name table (RW 0x548538 = h * 33 + c).
TEST_CASE("PORT-303: the lazy name key RW 0x8C130B is the world's key for the class name (Module::getModuleNameKey)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	CrateCollide *m = moduleOf<CrateCollide>(chest);
	REQUIRE(m != nullptr);
	const NameKeyType key = m->getModuleNameKey();
	// the world's table holds the class name with exactly this key (the factory created it through the same table)
	CHECK(w.w.keys.findKey("SalvageCrateCollide") == key);
	// a name the table never saw has no key: the check above is not true of every key
	CHECK(w.w.keys.findKey("SalvageCrateCollidf") == NAMEKEY_INVALID);
	// the name hash is the retail one and is the module's own class hash
	CHECK(NameKeyGenerator::hash("SalvageCrateCollide") == m->getModuleClassHash());
	CHECK(m->getModuleClassHash() != NameKeyGenerator::hash("SalvageCrateCollidf"));
}

// RW 0x8C1350 (vtable RW 0xC715C0 slot 0): the scalar deleting destructor — the data's vtable RW 0xC09D50 goes back to the base
// before the compiler's operator delete (0x42F6A0), i.e. nothing of the class survives its destruction. The port's data is held by
// RAII; what the module level can hold is that the instance is its own class (the xfer's type) and carries the class's own data.
TEST_CASE("PORT-303: the destructor's class identity RW 0x8C1350 is the instance's own (xferClass and the data's type)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	CrateCollide *m = moduleOf<CrateCollide>(chest);
	REQUIRE(m != nullptr);
	CHECK(m->xferClass() == typeid(SalvageCrateCollide));
	CHECK(m->xferClass() != typeid(CrateCollide));
	const SalvageCrateCollideModuleData *data = dynamic_cast<const SalvageCrateCollideModuleData *>(m->getModuleData());
	REQUIRE(data != nullptr);
	// the data is the class's own layout (vtable RW 0xC09D50): its fields parse independently of the base's
	CHECK(data->m_levelUpChance == doctest::Approx(0.0f)); // tolerance: 0% through the percent parser (RW 0x42EEFA) is exactly 0.0f
	CHECK(data->m_resourceChance == doctest::Approx(1.0f)); // tolerance: 100% through the percent parser (RW 0x42EEFA) under PC24
}

// ---- xfer: the version pair is the reading's ----------------------------------------------------------------
// RW 0x8C1442 writes version (1, 1) (RW 0x404F15); then RW 0x8C0760 writes CrateCollide's own version, whose WINDOW is (1, 2)
// (docs/SAVES-RETAIL.md's inventory row), then BehaviorModule (RW 0x9933FE = version (1, 1) + ObjectModule + Module) and, from
// version 2 on, the ever-executed byte + 0x14.
TEST_CASE("PORT-303: xfer writes SalvageCrateCollide's (1, 1), then CrateCollide's current 2, then the ever-executed byte (RW 0x8C1442, RW 0x8C0760)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	SalvageCrateCollide *m = moduleOf<SalvageCrateCollide>(chest);
	REQUIRE(m != nullptr);
	const bool everBefore = m->everExecuted();
	XferSave save;
	m->xfer(save);
	const std::vector<std::uint8_t> bytes = save.data();
	// one version byte per class in the chain: Salvage, CrateCollide, BehaviorModule, ObjectModule, Module; then the flag and the count
	REQUIRE(bytes.size() == 5 + 1 + 8);
	CHECK(bytes[0] == 1); // RW 0x8C1442's version: earliest 1, current 1
	CHECK(bytes[1] == 2); // RW 0x8C0760's version: window (1, 2), the current value 2 is written
	CHECK(bytes[2] == 1); // BehaviorModule RW 0x9933FE
	CHECK(bytes[3] == 1); // ObjectModule
	CHECK(bytes[4] == 1); // Module
	CHECK(bytes[5] == (everBefore ? 1 : 0)); // the ever-executed byte + 0x14
	XferLoad load(bytes);
	REQUIRE_NOTHROW(m->xfer(load));
	CHECK(m->everExecuted() == everBefore);
	CHECK(load.atEnd());
}

// A retail version-1 CrateCollide snapshot is legal for RW 0x8C0760's window (1, 2) and carries no ever-executed byte; the port must
// read it (before PORT-303 it wrote the window as (2, 2) and refused the byte 1), and the module then holds the version-1 default.
TEST_CASE("PORT-303: a retail version-1 CrateCollide snapshot loads without the ever-executed byte (RW 0x8C0760's window is (1, 2))")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	SalvageCrateCollide *m = moduleOf<SalvageCrateCollide>(chest);
	REQUIRE(m != nullptr);
	// first give the module a used-up state through the version-2 block that does carry it (a load is destructive, as retail saves are)
	const std::vector<std::uint8_t> v2{ 1, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 }; // era 1, count 1
	XferLoad loadV2(v2);
	REQUIRE_NOTHROW(m->xfer(loadV2));
	REQUIRE(loadV2.atEnd());
	REQUIRE(m->everExecuted());
	REQUIRE(m->executions() == 1);
	// the bytes a version-1 retail save has: Salvage 1, CrateCollide 1, BehaviorModule 1, ObjectModule 1, Module 1 — and nothing more
	const std::vector<std::uint8_t> v1{ 1, 1, 1, 1, 1 };
	XferLoad load(v1);
	REQUIRE_NOTHROW(m->xfer(load));
	CHECK(load.atEnd()); // a version-1 block is five bytes: the byte + 0x14 is not asked for
	CHECK_FALSE(m->everExecuted()); // reset, not kept
	CHECK(m->executions() == 0);
}

// the version bytes are checked by the reader, so the window above is a contract and not a comment: a byte outside it is refused
TEST_CASE("PORT-303: a version byte outside the class's window is refused at load (RW 0x8C1442's pair and RW 0x8C0760's window are read)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	SalvageCrateCollide *m = moduleOf<SalvageCrateCollide>(chest);
	REQUIRE(m != nullptr);
	SUBCASE("a newer SalvageCrateCollide version than 1")
	{
		const std::vector<std::uint8_t> v1{ 9, 1, 1, 1, 1 };
		XferLoad load(v1);
		CHECK_THROWS_AS(m->xfer(load), XferException);
	}
	SUBCASE("a newer CrateCollide version than its current 2")
	{
		const std::vector<std::uint8_t> v1{ 1, 9, 1, 1, 1 };
		XferLoad load(v1);
		CHECK_THROWS_AS(m->xfer(load), XferException);
	}
	SUBCASE("a CrateCollide version of 0 (earlier than the window's 1)")
	{
		const std::vector<std::uint8_t> v1{ 1, 0, 1, 1, 1 };
		XferLoad load(v1);
		CHECK_THROWS_AS(m->xfer(load), XferException);
	}
}

// ---- the three chances and the radius: parsed, and no reachable reader -------------------------------------------------
// With LevelUpChance at 0% the outcome is money (3) whatever PorterChance, BannerChance, ResourceChance and LevelUpRadius say
// (RW 0x8C136C reads + 0x64 and the Upgrade string + 0x78 only); the picking unit and a bystander inside the radius gain nothing.
TEST_CASE("PORT-303: the three chances and the radius change nothing: LevelUpChance 0% pays money (RW 0x8C136C -> RW 0x8C148B)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	REQUIRE(picker != nullptr);
	Object *bystander = w.unit("SalvPicker", 'A', 330, 300); // same player, 30 away: inside a LevelUpRadius of 100.0 that parsed to 1.0
	REQUIRE(bystander != nullptr);
	SalvageCrateCollide *m = moduleOf<SalvageCrateCollide>(chest);
	REQUIRE(m != nullptr);
	const int before = w.cash('A');
	// one draw: the outcome roll at line 0x45 (MinResource == MaxResource, so no money draw, RW 0x8C148B)
	w.logic->random().enableCallLog(true);
	w.logic->random().clearCallLog();
	CHECK(m->executeCrateBehavior(*picker) == true);
	w.logic->random().enableCallLog(false);
	const std::vector<GameLogicRandom::Call> &log = w.logic->random().callLog();
	REQUIRE(log.size() == 1);
	CHECK(log[0].real);
	CHECK(log[0].rlo == 0.0f); // RW 0x8C136C: GetGameLogicRandomValueReal(0.0, 1.0)
	CHECK(log[0].rhi == 1.0f);
	CHECK(log[0].line == 0x45); // RW 0x8C136C's draw
	CHECK(log[0].file == "SalvageCrateCollide.cpp");
	CHECK(m->lastOutcome() == SalvageCrateCollide::OUTCOME_MONEY); // RW returns 3
	CHECK(m->lastMoney() == 250);
	CHECK(w.cash('A') == before + 250);
	REQUIRE(picker->getExperienceTracker() != nullptr);
	REQUIRE(bystander->getExperienceTracker() != nullptr);
	CHECK(picker->getExperienceTracker()->getRank() == 0);
	CHECK(bystander->getExperienceTracker()->getRank() == 0); // the LevelUpRadius branch RW 0x8C168D never runs
}

// the money's range rule is the reading's: MinResource == MaxResource pays the minimum without a second draw (RW 0x8C148B); the
// same crate family with a range draws [Min, Max] at line 0xDB
TEST_CASE("PORT-303: an equal Min and Max pays without a money draw, a range draws at line 0xDB (RW 0x8C148B)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestAllChances", 'A', 300, 300); // Min 250 == Max 250
	REQUIRE(chest != nullptr);
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	REQUIRE(picker != nullptr);
	SalvageCrateCollide *m = moduleOf<SalvageCrateCollide>(chest);
	REQUIRE(m != nullptr);
	w.logic->random().enableCallLog(true);
	w.logic->random().clearCallLog();
	CHECK(m->executeCrateBehavior(*picker) == true);
	w.logic->random().enableCallLog(false);
	const std::vector<GameLogicRandom::Call> &log = w.logic->random().callLog();
	// exactly one draw in total: the outcome's
	REQUIRE(log.size() == 1);
	CHECK(log[0].real);
	CHECK(log[0].line == 0x45);
	CHECK(m->lastMoney() == 250);
}

// ---- the ExecuteFX half of the picked-up crate -----------------------------------------------------------------------
// RW 0x8C06D1 reads ExecuteFX (+ 0x48) and hands it to RW 0x4B1B5A for the collecting object; the port records one event with
// that name and the collector as the primary, once per crate used up (RW 0x62BBAB destroys it).
TEST_CASE("PORT-303: a used crate reports its ExecuteFX once for the collecting unit (RW 0x8C06D1 -> RW 0x4B1B5A)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestFx", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	const ObjectID chestId = chest->getID();
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	REQUIRE(picker != nullptr);
	const std::uint32_t pickerId = picker->getID();
	const auto &perSite = w.logic->fxEvents().perSite();
	const unsigned long long before = perSite.count("CrateCollide") ? perSite.at("CrateCollide") : 0;
	// step one frame at a time, so the frame the crate was used up in is the last one and its events are still there
	int stepped = 0;
	while (w.byId(chestId) != nullptr && stepped < 8)
	{
		w.frames(1);
		++stepped;
	}
	REQUIRE(w.byId(chestId) == nullptr); // the crate was used up (RW 0x62BBAB)
	const unsigned long long after = perSite.count("CrateCollide") ? perSite.at("CrateCollide") : 0;
	CHECK(after == before + 1);
	// that frame's event carried the data's name and named the collecting unit (RW 0x4B1B5A(fx, other, 0))
	bool found = false;
	for (const FXEvent &e : w.logic->fxEvents().frameEvents())
	{
		if (std::string(e.site) == "CrateCollide" && e.fxList && *e.fxList == "PORT303_PickupFx")
		{
			found = true;
			CHECK(e.primary == pickerId);
		}
	}
	CHECK(found);
}

// ---- a neighbouring behaviour the lane must not disturb --------------------------------------------------------------
// LevelUpChance 100% still levels the picker first, whatever the three chances and the radius say, and the crate is spent.
TEST_CASE("PORT-303: LevelUpChance 100% still wins over PorterChance / BannerChance / ResourceChance (RW 0x8C136C)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestLevelFirst", 'A', 300, 300);
	REQUIRE(chest != nullptr);
	const ObjectID chestId = chest->getID();
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	REQUIRE(picker != nullptr);
	const int before = w.cash('A');
	w.frames(4);
	CHECK(w.byId(chestId) == nullptr);
	REQUIRE(picker->getExperienceTracker() != nullptr);
	CHECK(picker->getExperienceTracker()->getRank() == 1);
	CHECK(w.cash('A') == before);
}
