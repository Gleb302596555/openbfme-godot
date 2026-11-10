// OpenBFME unit tests: LOCAL-24 DetachableRiderUpdate and DetachableRiderBody (issue #179; editions: both).
//
// Target facts and RotWK 2.01 / BFME2 1.06 addresses:
//   - DetachableRiderUpdate:
//       * Registration RW 0x659FBD (create RW 0x64F603, createData RW 0x64F63B), data ctor RW 0x8B2B75 / DONOR BFME2 0x4AEAB7,
//         buildFieldParse RW 0x8B2FC3 / DONOR BFME2 0x4AEF05, table RW 0xC6C2E8 / DONOR BFME2 0xB46668,
//         parseDeathEntry RW 0x8B2C42 / DONOR BFME2 0x4AEB84.
//       * Runtime ctor RW 0x8B2941 / DONOR BFME2 0x4AE883, getDisabledTypesToProcess RW 0x8B2A30 / DONOR BFME2 0x4AE972 (0x18),
//         killRider RW 0x8B2A46 / DONOR BFME2 0x4AE988, restoreRider RW 0x8B29B8 / DONOR BFME2 0x4AE8FA,
//         update RW 0x8B2D59 / DONOR BFME2 0x4AEC9B, xfer RW 0x8B285F / DONOR BFME2 0x4AE7A2 (version 1, 1).
//   - DetachableRiderBody:
//       * Registration RW 0x65B25E (create RW 0x6514A3, createData RW 0x6514DE), data ctor RW 0x8C6035 / DONOR BFME2 0x4C1E8B,
//         buildFieldParse RW 0x8C5D2B / DONOR BFME2 0x4C1B80, table RW 0xC72A0C / DONOR BFME2 0xB4CEF4 + UpgradeMuxData RW 0xC76AD8.
//       * Runtime ctor RW 0x8C5E2C / DONOR BFME2 0x4C1C82, attemptDamage RW 0x8C5EF3 / DONOR BFME2 0x4C1D49,
//         xfer RW 0x8C5EB5 / DONOR BFME2 0x4C1D0B (version 1, 2).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/INIException.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DetachableRiderUpdate.h"
#include "GameLogic/Module/RunOffMapBehavior.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"

namespace
{
struct SubObjectHookRecorder : ObjectClientHooks
{
	struct Call
	{
		ObjectID id = INVALID_ID;
		std::string name;
		bool visible = false;
		bool permanent = false;
	};
	std::vector<Call> calls;

	void objectCreated(Object &obj) override { obj.friend_bindToClient(this); }
	void objectDestroyed(Object &obj) override { obj.friend_bindToClient(nullptr); }
	void showSubObject(Object &obj, const std::string &name, bool visible, bool permanent) override
	{
		calls.push_back({ obj.getID(), name, visible, permanent });
	}
};

struct RiderTestRig
{
	combattest::CombatWorld w;
	ObjectCreationListStore oclStore;
	ObjectCreationListStore *savedOclStore = TheObjectCreationListStore;

	explicit RiderTestRig(const char *extraObjectIni = nullptr)
		: oclStore(w.w.keys)
	{
		TheObjectCreationListStore = &oclStore;
		if (!w.w.fx.env.blocks.contains("ObjectCreationList"))
		{
			w.w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		}
		DetachableRiderUpdate::registerClass(w.w.modules);
		DetachableRiderBody::registerClass(w.w.modules);
		RunOffMapBehavior::registerClass(w.w.modules);

		const std::string baseErr = w.w.load(
			"Weapon PrimaryLance\n"
			"  AttackRange = 20.0\n"
			"  DelayBetweenShots = 1000\n"
			"  DamageNugget\n"
			"    Damage = 30.0\n"
			"    Radius = 0.0\n"
			"    DamageType = PIERCE\n"
			"    DeathType = NORMAL\n"
			"  End\n"
			"End\n"
			"Weapon SecondaryTrample\n"
			"  AttackRange = 10.0\n"
			"  DelayBetweenShots = 1000\n"
			"  DamageNugget\n"
			"    Damage = 15.0\n"
			"    Radius = 0.0\n"
			"    DamageType = CRUSH\n"
			"    DeathType = CRUSHED\n"
			"  End\n"
			"End\n"
			"Upgrade Upgrade_EnableDetach\n"
			"  Type = OBJECT\n"
			"End\n"
			"Object DeadRiderCorpse\n"
			"  KindOf = INFANTRY\n"
			"  Body = ActiveBody ModuleTag_Body\n"
			"    MaxHealth = 50.0\n"
			"    InitialHealth = 50.0\n"
			"  End\n"
			"End\n"
			"ObjectCreationList OCL_SpawnDeadRider\n"
			"  CreateObject\n"
			"    ObjectNames = DeadRiderCorpse\n"
			"    Count = 1\n"
			"  End\n"
			"End\n",
			INI_LOAD_OVERWRITE,
			"rider_base.ini");
		REQUIRE_MESSAGE(baseErr.empty(), baseErr);
		if (extraObjectIni && *extraObjectIni)
		{
			const std::string extraErr = w.w.load(extraObjectIni, INI_LOAD_OVERWRITE, "rider_extra.ini");
			REQUIRE_MESSAGE(extraErr.empty(), extraErr);
		}
	}

	~RiderTestRig()
	{
		TheObjectCreationListStore = savedOclStore;
	}
};

void dealDamage(Object *victim, float amount, bool kill = false)
{
	DamageInfo info;
	info.m_input.m_damageType = DAMAGE_SLASH;
	info.m_input.m_deathType = DEATH_NORMAL;
	info.m_input.m_amount = amount;
	info.m_input.m_kill = kill;
	victim->attemptDamage(info);
}
} // namespace

TEST_CASE("LOCAL-24 DetachableRiderBody + DetachableRiderUpdate: lethal hit clamps damage to HealthPercentageWhenRiderDies, plays DeathEntry AnimState while paralyzed, and on wake hides RiderSubObjects, spawns RiderOCL, locks RiderlessWeaponSlot, sets RIDERLESS, and triggers RunOffMapBehavior (RW 0x8C5EF3, RW 0x8B2A46, RW 0x8B2D59)")
{
	RiderTestRig rig(
		"Object TestMountedKnight\n"
		"  KindOf = SELECTABLE CAVALRY CAN_ATTACK\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY PrimaryLance\n"
		"    Weapon = SECONDARY SecondaryTrample\n"
		"  End\n"
		"  Body = DetachableRiderBody ModuleTag_Body\n"
		"    MaxHealth = 250.0\n"
		"    MaxHealthDamaged = 125.0\n"
		"    RecoveryTime = 5000\n"
		"    HealthPercentageWhenRiderDies = 50%\n"
		"    StartsActive = Yes\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    RiderSubObjects = RUROHRM SHIELD\n"
		"    RiderlessWeaponSlot = SECONDARY\n"
		"    RiderlessHordeFlees = Yes\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:600 RiderOCL:OCL_SpawnDeadRider\n"
		"  End\n"
		"  Behavior = RunOffMapBehavior ModuleTag_RunOff\n"
		"    RequiresSpecificTrigger = Yes\n"
		"    DieOnMap = Yes\n"
		"  End\n"
		"End\n");

	SubObjectHookRecorder hooks;
	rig.w.logic->setClientHooks(&hooks);

	Object *knight = rig.w.unit("TestMountedKnight", 'A', 100.0f, 100.0f);
	REQUIRE(knight != nullptr);
	knight->friend_bindToClient(&hooks);

	DetachableRiderBody *body = dynamic_cast<DetachableRiderBody *>(knight->getBodyModule());
	DetachableRiderUpdate *riderUpd = dynamic_cast<DetachableRiderUpdate *>(knight->findModule("DetachableRiderUpdate"));
	REQUIRE(body != nullptr);
	REQUIRE(riderUpd != nullptr);
	CHECK(body->isAlreadyUpgraded());
	CHECK_FALSE(riderUpd->isRiderDead());

	const int condDeath2 = CombatNames::modelCondition("DEATH_2");
	const int condRiderless = CombatNames::modelCondition("RIDERLESS");
	const int statusRiderless = CombatNames::status("RIDERLESS");
	const int statusFleeOffMap = CombatNames::status("FLEE_OFF_MAP");

	// Non-lethal hit (50 damage on 250 HP -> 200 HP) does not detach the rider
	dealDamage(knight, 50.0f);
	CHECK(body->getHealth() == doctest::Approx(200.0f)); // tolerance: exact binary32 subtraction 250 - 50
	CHECK_FALSE(riderUpd->isRiderDead());
	CHECK_FALSE(knight->isDisabledByType(DISABLED_PARALYZED));

	// Lethal hit (300 damage on 200 HP):
	// RW 0x8C5FB0: floor = 250 * 0.5 = 125; clamped amount = 200 - 125 = 75 -> health becomes 125.0
	// RW 0x8B2A46: killRider() sets DEATH_2, DISABLED_PARALYZED, m_isRiderDead = true, wakes in 3 frames (600 ms at 5 fps)
	dealDamage(knight, 300.0f);
	CHECK_FALSE(knight->isEffectivelyDead());
	CHECK(body->getHealth() == doctest::Approx(125.0f)); // tolerance: exact binary32 clamp to 50% of 250
	CHECK(riderUpd->isRiderDead());
	CHECK(knight->testModelCondition(condDeath2));
	CHECK(knight->isDisabledByType(DISABLED_PARALYZED));
	CHECK_FALSE(knight->testStatus(statusRiderless));

	// After 2 frames (still in 3-frame DeathEntry AnimTime = 600ms): still paralyzed and not yet RIDERLESS
	rig.w.frames(2);
	CHECK(knight->isDisabledByType(DISABLED_PARALYZED));
	CHECK_FALSE(knight->testStatus(statusRiderless));

	// On the 3rd frame, DetachableRiderUpdate::update() runs despite DISABLED_PARALYZED (getDisabledTypesToProcess RW 0x8B2A30 = 0x18):
	// clears DISABLED_PARALYZED, hides RUROHRM and SHIELD, spawns DeadRiderCorpse via OCL_SpawnDeadRider, clears DEATH_2,
	// permanently locks SECONDARY weapon, sets OBJECT_STATUS_RIDERLESS and MODELCONDITION_RIDERLESS, and triggers RunOffMapBehavior!
	rig.w.frames(1);
	CHECK_FALSE(knight->isDisabledByType(DISABLED_PARALYZED));
	CHECK_FALSE(knight->testModelCondition(condDeath2));
	CHECK(knight->testStatus(statusRiderless));
	CHECK(knight->testModelCondition(condRiderless));
	CHECK(knight->testStatus(statusFleeOffMap));

	REQUIRE(knight->getWeapons() != nullptr);
	CHECK(knight->getWeapons()->curSlot() == SECONDARY_WEAPON);
	CHECK(knight->getWeapons()->isCurWeaponLocked());

	REQUIRE(hooks.calls.size() == 2u);
	CHECK(hooks.calls[0].name == "RUROHRM");
	CHECK_FALSE(hooks.calls[0].visible);
	CHECK_FALSE(hooks.calls[0].permanent);
	CHECK(hooks.calls[1].name == "SHIELD");
	CHECK_FALSE(hooks.calls[1].visible);
	CHECK_FALSE(hooks.calls[1].permanent);

	// Verify OCL_SpawnDeadRider created DeadRiderCorpse at the knight's position
	Object *spawnedCorpse = nullptr;
	for (Object *it = rig.w.logic->getFirstObject(); it; it = it->getNextObject())
	{
		if (it->getTemplate()->getName() == "DeadRiderCorpse")
		{
			spawnedCorpse = it;
			break;
		}
	}
	REQUIRE(spawnedCorpse != nullptr);

	// Test restoreRider() (RW 0x8B29B8 -> update RW 0x8B2F4E):
	// re-shows RUROHRM and SHIELD, releases LOCKED_PERMANENTLY weapon lock, clears RIDERLESS status and model condition
	hooks.calls.clear();
	riderUpd->restoreRider();
	CHECK_FALSE(riderUpd->isRiderDead());
	rig.w.frames(1);
	CHECK_FALSE(knight->testStatus(statusRiderless));
	CHECK_FALSE(knight->testModelCondition(condRiderless));
	CHECK_FALSE(knight->getWeapons()->isCurWeaponLocked());
	REQUIRE(hooks.calls.size() == 2u);
	CHECK(hooks.calls[0].name == "RUROHRM");
	CHECK(hooks.calls[0].visible);
	CHECK(hooks.calls[1].name == "SHIELD");
	CHECK(hooks.calls[1].visible);

	// Verify parseDeathEntry error messages (RW 0x8B2C42: "AnimState expected", "AnimDuration expected", "RiderOCL expected")
	CHECK(rig.w.w.load(
		"Object BadDeathEntry1\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    DeathEntry = WrongKey:DEATH_2 AnimTime:1000 RiderOCL:None\n"
		"  End\n"
		"End\n", INI_LOAD_OVERWRITE, "bad1.ini").find("AnimState expected") != std::string::npos);
	CHECK(rig.w.w.load(
		"Object BadDeathEntry2\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    DeathEntry = AnimState:DEATH_2 WrongKey:1000 RiderOCL:None\n"
		"  End\n"
		"End\n", INI_LOAD_OVERWRITE, "bad2.ini").find("AnimDuration expected") != std::string::npos);
	CHECK(rig.w.w.load(
		"Object BadDeathEntry3\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:1000 WrongKey:None\n"
		"  End\n"
		"End\n", INI_LOAD_OVERWRITE, "bad3.ini").find("RiderOCL expected") != std::string::npos);
}

TEST_CASE("LOCAL-24 DetachableRiderBody gates: StartsActive=No waits for TriggeredBy upgrade; FREEFALL and RiderlessDeathChance=100% skip detach; second lethal hit while RIDERLESS kills mount (RW 0x8C5E2C, RW 0x8C5EF3)")
{
	RiderTestRig rig(
		"Object KnightStartsInactive\n"
		"  KindOf = SELECTABLE CAVALRY\n"
		"  Body = DetachableRiderBody ModuleTag_Body\n"
		"    MaxHealth = 200.0\n"
		"    HealthPercentageWhenRiderDies = 50%\n"
		"    StartsActive = No\n"
		"    TriggeredBy = Upgrade_EnableDetach\n"
		"  End\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:200 RiderOCL:None\n"
		"  End\n"
		"End\n"
		"Object KnightAlwaysDiesWithRider\n"
		"  KindOf = SELECTABLE CAVALRY\n"
		"  Body = DetachableRiderBody ModuleTag_Body\n"
		"    MaxHealth = 200.0\n"
		"    HealthPercentageWhenRiderDies = 50%\n"
		"    StartsActive = Yes\n"
		"    RiderlessDeathChance = 100%\n"
		"  End\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:200 RiderOCL:None\n"
		"  End\n"
		"End\n");

	// 1. StartsActive = No without upgrade: lethal hit kills the unit outright without detaching rider
	Object *inactive = rig.w.unit("KnightStartsInactive", 'A', 50.0f, 50.0f);
	REQUIRE(inactive != nullptr);
	dealDamage(inactive, 250.0f);
	CHECK(inactive->isEffectivelyDead());

	// 2. StartsActive = No WITH TriggeredBy upgrade granted: lethal hit clamps to 50% (100 HP) and detaches rider
	Object *upgraded = rig.w.unit("KnightStartsInactive", 'A', 100.0f, 50.0f);
	REQUIRE(upgraded != nullptr);
	const UpgradeTemplate *upg = rig.w.upgradeCenter.findUpgrade("Upgrade_EnableDetach");
	REQUIRE(upg != nullptr);
	upgraded->giveUpgrade(upg);
	DetachableRiderBody *upgBody = dynamic_cast<DetachableRiderBody *>(upgraded->getBodyModule());
	REQUIRE(upgBody != nullptr);
	CHECK(upgBody->isAlreadyUpgraded());

	// m_kill = true also triggers rider detach and clears m_kill (RW 0x8C5FA8, 0x8C5FD9)
	dealDamage(upgraded, 10.0f, true);
	CHECK_FALSE(upgraded->isEffectivelyDead());
	CHECK(upgBody->getHealth() == doctest::Approx(100.0f)); // tolerance: exact binary32 50% of 200
	CHECK(upgraded->isDisabledByType(DISABLED_PARALYZED));

	// Second lethal hit after waking as RIDERLESS kills the mount (RW 0x8C5F60, 0x8C5F7C)
	rig.w.frames(2);
	CHECK(upgraded->testStatus(CombatNames::status("RIDERLESS")));
	dealDamage(upgraded, 250.0f);
	CHECK(upgraded->isEffectivelyDead());

	// 3. MODELCONDITION_FREEFALL (bit 72, RW 0x8C5F4F) forces keepRider = true -> unit dies with rider
	Object *falling = rig.w.unit("KnightStartsInactive", 'A', 150.0f, 50.0f);
	REQUIRE(falling != nullptr);
	falling->giveUpgrade(upg);
	falling->setModelConditionState(CombatNames::modelCondition("FREEFALL"), true);
	dealDamage(falling, 250.0f);
	CHECK(falling->isEffectivelyDead());

	// 4. RiderlessDeathChance = 100% (roll < 1.0f, RW 0x8C5F34) forces keepRider = true -> unit dies with rider
	Object *chance100 = rig.w.unit("KnightAlwaysDiesWithRider", 'A', 200.0f, 50.0f);
	REQUIRE(chance100 != nullptr);
	dealDamage(chance100, 250.0f);
	CHECK(chance100->isEffectivelyDead());
}

TEST_CASE("LOCAL-24 DetachableRiderUpdate horde behaviors: RemoveRiderlessFromHorde removes member from HordeContain and idles AI; RiderlessHordeFlees triggers RunOffMapBehavior only once every member is RIDERLESS (RW 0x8B2E2E, RW 0x8B2EA0)")
{
	RiderTestRig rig(
		"Object RiderMemberRemove\n"
		"  KindOf = SELECTABLE CAVALRY\n"
		"  Body = DetachableRiderBody ModuleTag_Body\n"
		"    MaxHealth = 100.0\n"
		"    HealthPercentageWhenRiderDies = 50%\n"
		"    StartsActive = Yes\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    RemoveRiderlessFromHorde = Yes\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:0 RiderOCL:None\n"
		"  End\n"
		"End\n"
		"Object RiderHordeRemove\n"
		"  KindOf = SELECTABLE HORDE\n"
		"  Body = ImmortalBody ModuleTag_Body\n"
		"    MaxHealth = 1000.0\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = HordeContain ModuleTag_Horde\n"
		"    Slots = 2\n"
		"    InitialPayload = RiderMemberRemove 2\n"
		"    RankInfo = RankNumber:1 UnitType:RiderMemberRemove Position:X:0 Y:0 Position:X:0 Y:20\n"
		"  End\n"
		"End\n"
		"Object RiderMemberFlee\n"
		"  KindOf = SELECTABLE CAVALRY\n"
		"  Body = DetachableRiderBody ModuleTag_Body\n"
		"    MaxHealth = 100.0\n"
		"    HealthPercentageWhenRiderDies = 50%\n"
		"    StartsActive = Yes\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    RiderlessHordeFlees = Yes\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:0 RiderOCL:None\n"
		"  End\n"
		"  Behavior = RunOffMapBehavior ModuleTag_RunOff\n"
		"    RequiresSpecificTrigger = Yes\n"
		"    DieOnMap = Yes\n"
		"  End\n"
		"End\n"
		"Object RiderHordeFlee\n"
		"  KindOf = SELECTABLE HORDE\n"
		"  Body = ImmortalBody ModuleTag_Body\n"
		"    MaxHealth = 1000.0\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = HordeContain ModuleTag_Horde\n"
		"    Slots = 2\n"
		"    InitialPayload = RiderMemberFlee 2\n"
		"    RankInfo = RankNumber:1 UnitType:RiderMemberFlee Position:X:0 Y:0 Position:X:0 Y:20\n"
		"  End\n"
		"End\n");

	// 1. RemoveRiderlessFromHorde = Yes (RW 0x8B2E2E .. 0x8B2E9B):
	Object *hordeRem = rig.w.unit("RiderHordeRemove", 'A', 100.0f, 100.0f);
	REQUIRE(hordeRem != nullptr);
	const std::vector<Object *> remMembers = rig.w.membersOf(hordeRem);
	REQUIRE(remMembers.size() == 2u);
	Object *m0 = remMembers[0];
	CHECK(m0->getContainedBy() == hordeRem);

	dealDamage(m0, 150.0f);
	rig.w.frames(1);
	CHECK(m0->testStatus(CombatNames::status("RIDERLESS")));
	CHECK(m0->getContainedBy() == nullptr);
	CHECK(hordeRem->getContain()->getContainCount() == 1u);

	// 2. RiderlessHordeFlees = Yes (RW 0x8B2EA0 .. 0x8B2F3F):
	// First member losing its rider does NOT trigger RunOffMapBehavior while the second member still has a rider
	Object *hordeFlee = rig.w.unit("RiderHordeFlee", 'A', 200.0f, 100.0f);
	REQUIRE(hordeFlee != nullptr);
	const std::vector<Object *> fleeMembers = rig.w.membersOf(hordeFlee);
	REQUIRE(fleeMembers.size() == 2u);
	const int statusFleeOffMap = CombatNames::status("FLEE_OFF_MAP");

	dealDamage(fleeMembers[0], 150.0f);
	rig.w.frames(1);
	CHECK(fleeMembers[0]->testStatus(CombatNames::status("RIDERLESS")));
	CHECK_FALSE(fleeMembers[0]->testStatus(statusFleeOffMap));
	CHECK_FALSE(fleeMembers[1]->testStatus(statusFleeOffMap));

	// Once the second member also loses its rider, riderlessCount == getContainCount() -> all members flee!
	dealDamage(fleeMembers[1], 150.0f);
	rig.w.frames(1);
	CHECK(fleeMembers[1]->testStatus(CombatNames::status("RIDERLESS")));
	CHECK(fleeMembers[0]->testStatus(statusFleeOffMap));
	CHECK(fleeMembers[1]->testStatus(statusFleeOffMap));
}

TEST_CASE("LOCAL-24 xfer and crc: DetachableRiderBody (1, 2) and DetachableRiderUpdate (1, 1) roundtrip mid-animation state (RW 0x8C5EB5, RW 0x8B285F)")
{
	RiderTestRig rig(
		"Object XferMountedKnight\n"
		"  KindOf = SELECTABLE CAVALRY\n"
		"  Body = DetachableRiderBody ModuleTag_Body\n"
		"    MaxHealth = 250.0\n"
		"    HealthPercentageWhenRiderDies = 50%\n"
		"    StartsActive = Yes\n"
		"  End\n"
		"  Behavior = DetachableRiderUpdate ModuleTag_Rider\n"
		"    DeathEntry = AnimState:DEATH_1 AnimTime:1000 RiderOCL:None\n"
		"    DeathEntry = AnimState:DEATH_2 AnimTime:1000 RiderOCL:None\n"
		"  End\n"
		"End\n");

	Object *src = rig.w.unit("XferMountedKnight", 'A', 100.0f, 100.0f);
	Object *dst = rig.w.unit("XferMountedKnight", 'A', 200.0f, 100.0f);
	REQUIRE(src != nullptr);
	REQUIRE(dst != nullptr);

	DetachableRiderBody *srcBody = dynamic_cast<DetachableRiderBody *>(src->getBodyModule());
	DetachableRiderUpdate *srcUpd = dynamic_cast<DetachableRiderUpdate *>(src->findModule("DetachableRiderUpdate"));
	DetachableRiderBody *dstBody = dynamic_cast<DetachableRiderBody *>(dst->getBodyModule());
	DetachableRiderUpdate *dstUpd = dynamic_cast<DetachableRiderUpdate *>(dst->findModule("DetachableRiderUpdate"));
	REQUIRE(srcBody != nullptr);
	REQUIRE(srcUpd != nullptr);
	REQUIRE(dstBody != nullptr);
	REQUIRE(dstUpd != nullptr);

	StateHasher hBefore, hAfter;
	srcBody->crc(hBefore);
	srcUpd->crc(hBefore);

	// Trigger rider death on src
	dealDamage(src, 300.0f);
	CHECK(srcUpd->isRiderDead());

	srcBody->crc(hAfter);
	srcUpd->crc(hAfter);
	CHECK(hBefore.value() != hAfter.value());

	// Save srcBody + srcUpd and load into dstBody + dstUpd
	XferSave save;
	srcBody->xfer(save);
	srcUpd->xfer(save);

	XferLoad load(save.data());
	dstBody->xfer(load);
	dstUpd->xfer(load);

	CHECK(dstUpd->isRiderDead() == srcUpd->isRiderDead());
	CHECK(dstUpd->whichDeathEntry() == srcUpd->whichDeathEntry());
	CHECK(dstBody->getHealth() == doctest::Approx(srcBody->getHealth())); // tolerance: exact float roundtrip

	StateHasher hDst;
	dstBody->crc(hDst);
	dstUpd->crc(hDst);
	CHECK(hDst.value() == hAfter.value());
}

TEST_CASE("LOCAL-24 ENNORATH scenario DetachableRiderUpdate: CINE_Gothmog and CINE_RohanRohirrim_Cheer from cinematicobjects.ini detach their rider at 50% health and trigger RunOffMapBehavior (RW 0x8C5EF3, RW 0x8B2D59)")
{
	ennorathscenario::World world;
	if (!world.load("Ennorath scenario DetachableRiderUpdate"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionMordor", "FactionMen");

	// data/ini/object/modcinematic/cinematicobjects.ini:
	// CINE_Gothmog: DetachableRiderBody (MaxHealth = 250, MaxHealthDamaged = 125, RecoveryTime = 5000, HealthPercentageWhenRiderDies = 50%, StartsActive = Yes)
	//               DetachableRiderUpdate (RiderSubObjects = RUROHRM SHIELD, RiderlessWeaponSlot = SECONDARY, RiderlessHordeFlees = Yes,
	//                                      DeathEntry = AnimState:DEATH_2 AnimTime:3000 RiderOCL:OCL_RohirrimSpawnDeadRider)
	//               RunOffMapBehavior (RequiresSpecificTrigger = Yes, RunOffMapWaypointName = Rohan1, DieOnMap = No)
	Object *gothmog = world.place(arena, "CINE_Gothmog", 0, 400.0f, 400.0f);
	Object *rohirrim = world.place(arena, "CINE_RohanRohirrim_Cheer", 1, 500.0f, 400.0f);
	REQUIRE(gothmog != nullptr);
	REQUIRE(rohirrim != nullptr);

	DetachableRiderBody *gBody = dynamic_cast<DetachableRiderBody *>(gothmog->getBodyModule());
	DetachableRiderUpdate *gUpd = dynamic_cast<DetachableRiderUpdate *>(gothmog->findModule("DetachableRiderUpdate"));
	DetachableRiderBody *rBody = dynamic_cast<DetachableRiderBody *>(rohirrim->getBodyModule());
	DetachableRiderUpdate *rUpd = dynamic_cast<DetachableRiderUpdate *>(rohirrim->findModule("DetachableRiderUpdate"));
	REQUIRE(gBody != nullptr);
	REQUIRE(gUpd != nullptr);
	REQUIRE(rBody != nullptr);
	REQUIRE(rUpd != nullptr);

	CHECK(gBody->getMaxHealth() == doctest::Approx(250.0f)); // tolerance: INI float literal 250
	CHECK(gBody->riderBodyData()->m_healthPercentageWhenRiderDies == doctest::Approx(0.5f)); // tolerance: 50%
	CHECK(gBody->startsActive());
	CHECK(gBody->isAlreadyUpgraded());
	CHECK_FALSE(rBody->startsActive());
	CHECK_FALSE(rBody->isAlreadyUpgraded());

	REQUIRE(gUpd->riderData()->m_riderSubObjects.size() == 2u);
	CHECK(gUpd->riderData()->m_riderSubObjects[0] == "RUROHRM");
	CHECK(gUpd->riderData()->m_riderSubObjects[1] == "SHIELD");
	CHECK(gUpd->riderData()->m_riderlessWeaponSlot == SECONDARY_WEAPON);
	CHECK(gUpd->riderData()->m_riderlessHordeFlees);
	REQUIRE(gUpd->riderData()->m_deathEntries.size() == 1u);
	CHECK(gUpd->riderData()->m_deathEntries[0].m_animTime == 15u); // 3000 ms at 5 logic fps = 15 frames
	CHECK(gUpd->riderData()->m_deathEntries[0].m_ocl == "OCL_RohirrimSpawnDeadRider");

	// Deal lethal damage to CINE_Gothmog: DetachableRiderBody clamps input m_amount to 125 (50% of 250), then ActiveBody::attemptDamage
	// multiplies by RohanHeavyCavalryArmor SLASH = 110% (125 * 1.1 = 137.5 dealt -> 112.5 remaining), sets DEATH_2 and DISABLED_PARALYZED for 15 frames
	dealDamage(gothmog, 500.0f);

	CHECK_FALSE(gothmog->isEffectivelyDead());
	CHECK(gBody->getHealth() == doctest::Approx(112.5f)); // tolerance: 250 - (125 * 1.1 SLASH armor)
	CHECK(gUpd->isRiderDead());
	CHECK(gothmog->isDisabledByType(DISABLED_PARALYZED));
	CHECK(gothmog->testModelCondition(CombatNames::modelCondition("DEATH_2")));

	// Advance 15 frames (3000 ms): wakes, clears DISABLED_PARALYZED and DEATH_2, sets RIDERLESS status + model condition, triggers RunOffMapBehavior
	for (int i = 0; i < 15; ++i)
	{
		arena.logic.runLogicFrame();
	}
	CHECK_FALSE(gothmog->isDisabledByType(DISABLED_PARALYZED));
	CHECK_FALSE(gothmog->testModelCondition(CombatNames::modelCondition("DEATH_2")));
	CHECK(gothmog->testStatus(CombatNames::status("RIDERLESS")));
	CHECK(gothmog->testModelCondition(CombatNames::modelCondition("RIDERLESS")));
	CHECK(gothmog->testStatus(CombatNames::status("FLEE_OFF_MAP")));
}
