// OpenBFME unit tests, PORT-252 (issue #600): RotWK 2.01 parity for SpecialAbilityUpdate and its subclasses
// (HeroModeSpecialAbilityUpdate, ModelConditionSpecialAbilityUpdate, TeleportSpecialAbilityUpdate,
// FlingPassengerSpecialAbilityUpdate). Synthetic INI only, no retail assets. Each check cites its RotWK 2.01 address. GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/GameState.h"
#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "GameClient/ClientEvents.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/FlingPassengerSpecialAbilityUpdate.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <string>
#include <vector>

namespace
{

const char kPort252INI[] = R"ini(
SpecialPower P252CapturePower
  Enum = SPECIAL_INFANTRY_CAPTURE_BUILDING
  ReloadTime = 0
  ObjectFilter = ALL
End

SpecialPower P252ParalyzePower
  Enum = SPECIAL_KNIFE_ATTACK
  ReloadTime = 0
End

SpecialPower P252GrabChunkPower
  Enum = SPECIAL_GRAB_CHUNK
  ReloadTime = 0
End

SpecialPower P252GrabPassengerPower
  Enum = SPECIAL_GRAB_PASSENGER
  ReloadTime = 0
End

SpecialPower P252TerrorPower
  Enum = SPECIAL_HERO_MODE
  ReloadTime = 0
End

SpecialPower P252TeleportPower
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 0
End

Object P252Building
  KindOf = STRUCTURE IMMOBILE SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 15
  GeometryHeight = 30
  Body = ActiveBody TagBody
    MaxHealth = 500
  End
End

Object P252Capturer
  KindOf = INFANTRY SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252CapturePower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate TagSAU
    SpecialPowerTemplate = P252CapturePower
    StartAbilityRange = 100
    AbilityAbortRange = 200
    UnpackTime = 600
    PreparationTime = 1000
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
End

Object P252ParalyzeHero
  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252ParalyzePower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate TagSAU
    SpecialPowerTemplate = P252ParalyzePower
    StartAbilityRange = 100
    AbilityAbortRange = 200
    UnpackTime = 400
    PreparationTime = 200
    PackTime = 200
    ParalyzeDurationWhenCompleted = 1000
    ParalyzeDurationWhenAborted = 600
    IgnoreFacingCheck = Yes
  End
End

Object P252ChunkVendor
  KindOf = STRUCTURE IMMOBILE CHUNK_VENDOR
  Geometry = CYLINDER
  GeometryMajorRadius = 10
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 500
  End
End

Object P252PlainTarget
  KindOf = INFANTRY SELECTABLE
  TransportSlotCount = 1
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = EmotionTrackerUpdate TagEmo
  End
End

Object P252GrabAndKillTarget
  KindOf = INFANTRY SELECTABLE GRAB_AND_KILL
  TransportSlotCount = 1
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface TagAI
  End
End

Object P252ShrubberyTarget
  KindOf = SHRUBBERY IMMOBILE
  TransportSlotCount = 1
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 200
  End
End

Object P252ChunkGrabber
  KindOf = MONSTER SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 12
  GeometryHeight = 30
  Body = ActiveBody TagBody
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = TransportContain TagContain
    ContainMax = 1
    Slots = 1
    AllowAlliesInside = Yes
    AllowEnemiesInside = Yes
    AllowNeutralInside = Yes
    PassengerFilter = ALL
    TypeOneForWeaponState = INFANTRY
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252GrabChunkPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate TagSAU
    SpecialPowerTemplate = P252GrabChunkPower
    StartAbilityRange = 100
    AbilityAbortRange = 200
    UnpackTime = 600
    PreparationTime = 400
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
End

Object P252PassengerGrabber
  KindOf = MONSTER SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 12
  GeometryHeight = 30
  Body = ActiveBody TagBody
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = TransportContain TagContain
    ContainMax = 1
    Slots = 1
    AllowAlliesInside = Yes
    AllowEnemiesInside = Yes
    AllowNeutralInside = Yes
    PassengerFilter = ALL
    TypeOneForWeaponState = INFANTRY
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252GrabPassengerPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate TagSAU
    SpecialPowerTemplate = P252GrabPassengerPower
    StartAbilityRange = 100
    AbilityAbortRange = 200
    UnpackTime = 0
    PreparationTime = 200
    PersistentPrepTime = 600
    PersistentCount = 2
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
End

Object P252WeaponStatePreparer
  KindOf = MONSTER SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 12
  GeometryHeight = 30
  Body = ActiveBody TagBody
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = TransportContain TagContain
    ContainMax = 1
    Slots = 1
    AllowAlliesInside = Yes
    AllowEnemiesInside = Yes
    AllowNeutralInside = Yes
    PassengerFilter = ALL
    TypeOneForWeaponState = INFANTRY
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252ParalyzePower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate TagSAU
    SpecialPowerTemplate = P252ParalyzePower
    StartAbilityRange = 100
    AbilityAbortRange = 200
    UnpackTime = 0
    PreparationTime = 600
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
End

Object P252TerrorCaster
  KindOf = INFANTRY SELECTABLE HERO
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 300
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252TerrorPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = ModelConditionSpecialAbilityUpdate TagMC
    SpecialPowerTemplate = P252TerrorPower
    UnpackTime = 0
    PreparationTime = 0
    PackTime = 0
    GenerateTerror = Yes
    EmotionPulseRadius = 200
    IgnoreFacingCheck = Yes
  End
End

Object P252TeleportHero
  KindOf = INFANTRY SELECTABLE HERO
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 300
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252TeleportPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = TeleportSpecialAbilityUpdate TagTP
    SpecialPowerTemplate = P252TeleportPower
    StartAbilityRange = 500
    AbilityAbortRange = 1000
    UnpackTime = 600
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
End

Object P252HeroModeHero
  KindOf = INFANTRY SELECTABLE HERO
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody TagBody
    MaxHealth = 300
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252TerrorPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = HeroModeSpecialAbilityUpdate TagHM
    SpecialPowerTemplate = P252TerrorPower
    UnpackTime = 600
    PreparationTime = 0
    PackTime = 0
    HeroEffectDuration = 2000
    IgnoreFacingCheck = Yes
  End
End

Object P252FlingHero
  KindOf = MONSTER SELECTABLE HERO
  Geometry = CYLINDER
  GeometryMajorRadius = 12
  GeometryHeight = 30
  Body = ActiveBody TagBody
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface TagAI
  End
  Behavior = TransportContain TagContain
    ContainMax = 1
    Slots = 1
    AllowAlliesInside = Yes
    AllowEnemiesInside = Yes
    AllowNeutralInside = Yes
    PassengerFilter = ALL
  End
  Behavior = SpecialPowerModule TagSP
    SpecialPowerTemplate = P252TeleportPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = FlingPassengerSpecialAbilityUpdate TagFling
    SpecialPowerTemplate = P252TeleportPower
    StartAbilityRange = 500
    AbilityAbortRange = 1000
    UnpackTime = 600
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
End
)ini";

struct Port252World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	Port252World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		HeroAbilityModules::registerAll(w.modules);
		FlingPassengerSpecialAbilityUpdate::registerClass(w.modules);
		EmotionModules::registerAll(w.modules);
		const std::string err = w.load(kPort252INI, INI_LOAD_OVERWRITE, "port252.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		logic->partition().setRegion(0.0f, 0.0f, 2000.0f, 2000.0f);
		combat().setAutoAcquireEnabled(false);
	}
	~Port252World()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
	}
};

template <class T>
T *findMod(Object *o)
{
	if (!o)
	{
		return nullptr;
	}
	for (const auto &m : o->modules())
	{
		if (T *p = dynamic_cast<T *>(m.get()))
		{
			return p;
		}
	}
	return nullptr;
}

class EffectExpiryProbeAbility : public SpecialAbilityUpdate
{
public:
	EffectExpiryProbeAbility(Thing *thing, const SpecialAbilityUpdateModuleData *data)
		: SpecialAbilityUpdate(thing, data)
	{
	}
	void setEffectExpiryForTest(unsigned f) { m_effectExpiry = f; }
	void invokeBaseOnEffectExpired() { SpecialAbilityUpdate::onEffectExpired(); }
	void setTargetForTest(ObjectID id) { m_targetID = id; }
	void invokeRestartPersistent() { restartPersistent(); }
};

} // namespace

TEST_CASE("PORT-252: ~SpecialAbilityUpdate (RW 0x00854D9A / 0x0085536C) releases capture and clears power updateDisabled on object destruction without post-DESTROYED MODEL_CONDITION")
{
	Port252World f;
	ClientEventRecorder recorder(*f.logic);
	f.logic->setClientHooks(&recorder);

	Object *cap = f.unit("P252Capturer", 'A', 300.0f, 300.0f);
	Object *bld = f.unit("P252Building", 'B', 320.0f, 300.0f);
	REQUIRE(cap);
	REQUIRE(bld);
	const ObjectID capId = cap->getID();

	SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(cap);
	SpecialPowerModule *spm = findMod<SpecialPowerModule>(cap);
	REQUIRE(sau);
	REQUIRE(spm);
	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252CapturePower");
	REQUIRE(spt);

	sau->initiateIntentToDoSpecialPower(spt, bld, nullptr, 0, 0);
	f.frames(4); // UnpackTime = 600 (3 frames): frame 1 starts unpack, frame 4 completes unpack -> claims capturerID & START_CAPTURE
	REQUIRE(sau->isActive());
	REQUIRE(bld->getCapturerID() == capId);
	const int kMcStartCapture = CombatNames::modelCondition("START_CAPTURE");
	const int kMcCancelCapture = CombatNames::modelCondition("CANCEL_CAPTURE");
	REQUIRE(kMcStartCapture == 110);
	REQUIRE(kMcCancelCapture == 111);
	REQUIRE(bld->testModelCondition(kMcStartCapture));

	spm->setUpdateDisabled(true);
	REQUIRE(spm->isUpdateDisabled());

	(void)recorder.take(false); // drain creation / unpack events before destruction
	f.logic->destroyObject(cap);
	f.frames(1);
	f.logic->setClientHooks(nullptr);

	// ~SpecialAbilityUpdate() ran finishAbility(true, true): target capture released cleanly
	CHECK(bld->getCapturerID() == INVALID_ID);
	CHECK_FALSE(bld->testModelCondition(kMcStartCapture));
	CHECK(bld->testModelCondition(kMcCancelCapture));

	// Object::~Object() detached m_client after objectDestroyed(): no MODEL_CONDITION for capId after DESTROYED
	bool sawDestroyed = false;
	bool sawModelCondAfterDestroyed = false;
	for (const ClientEvent &ev : recorder.take(false))
	{
		if (ev.object != capId)
		{
			continue;
		}
		if (ev.kind == ClientEvent::DESTROYED)
		{
			sawDestroyed = true;
		}
		else if (sawDestroyed && ev.kind == ClientEvent::MODEL_CONDITION)
		{
			sawModelCondAfterDestroyed = true;
		}
	}
	CHECK(sawDestroyed);
	CHECK_FALSE(sawModelCondAfterDestroyed);

	// Subclass deleting dtor chaining (RW 0x008958A1, 0x0089453F, 0x00896484, 0x0089873E -> ~SpecialAbilityUpdate 0x00854D9A)
	const Coord3D dst{ 500.0f, 300.0f, 0.0f };
	auto destroyWhileActive = [&](const char *tmpl, const char *power, const Coord3D *loc) {
		Object *o = f.unit(tmpl, 'A', 400.0f, 300.0f);
		REQUIRE(o);
		const ObjectID oid = o->getID();
		SpecialAbilityUpdate *u = findMod<SpecialAbilityUpdate>(o);
		REQUIRE(u);
		u->initiateIntentToDoSpecialPower(f.powers.findSpecialPowerTemplate(power), nullptr, loc, 0, 0);
		f.frames(1);
		f.logic->destroyObject(o);
		f.frames(1);
		CHECK(f.byId(oid) == nullptr);
	};
	destroyWhileActive("P252TeleportHero", "P252TeleportPower", &dst);
	destroyWhileActive("P252HeroModeHero", "P252TerrorPower", nullptr);
	destroyWhileActive("P252TerrorCaster", "P252TerrorPower", nullptr);
	destroyWhileActive("P252FlingHero", "P252TeleportPower", &dst);
}

TEST_CASE("PORT-252: finishAbility routes ParalyzeDurationWhenCompleted and ParalyzeDurationWhenAborted to aiBusyFor (RW 0x0085338E -> 0x00852E2A) and clears m_updateDisabled (RW 0x00896751)")
{
	Port252World f;
	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252ParalyzePower");
	REQUIRE(spt);

	SUBCASE("completed ability invokes aiBusyFor(ParalyzeDurationWhenCompleted) and clears m_updateDisabled")
	{
		Object *hero = f.unit("P252ParalyzeHero", 'A', 300.0f, 300.0f);
		Object *tgt = f.unit("P252PlainTarget", 'B', 320.0f, 300.0f);
		REQUIRE(hero);
		REQUIRE(tgt);
		SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(hero);
		SpecialPowerModule *spm = findMod<SpecialPowerModule>(hero);
		AIUpdateInterface *ai = hero->getAIUpdateInterface();
		REQUIRE(sau);
		REQUIRE(spm);
		REQUIRE(ai);

		spm->setUpdateDisabled(true);
		sau->initiateIntentToDoSpecialPower(spt, tgt, nullptr, 0, 0);
		int frames = f.runUntil([&]() { return !sau->isActive(); }, 30);
		CHECK(frames == 5);
		CHECK_FALSE(sau->isActive());
		CHECK(sau->unported() == 0);
		CHECK_FALSE(spm->isUpdateDisabled());
		CHECK(ai->stateMachine().isBusyState());
		CHECK(ai->stateMachine().temporaryStateId() == (unsigned)AI_BUSY);

		// ParalyzeDurationWhenCompleted = 1000 ms = 5 frames; temporary busy expires after 5 frames
		f.frames(6);
		CHECK_FALSE(ai->stateMachine().isBusyState());
	}

	SUBCASE("aborted ability invokes aiBusyFor(ParalyzeDurationWhenAborted) and clears m_updateDisabled")
	{
		Object *hero = f.unit("P252ParalyzeHero", 'A', 300.0f, 300.0f);
		Object *tgt = f.unit("P252PlainTarget", 'B', 320.0f, 300.0f);
		REQUIRE(hero);
		REQUIRE(tgt);
		SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(hero);
		SpecialPowerModule *spm = findMod<SpecialPowerModule>(hero);
		AIUpdateInterface *ai = hero->getAIUpdateInterface();
		REQUIRE(sau);
		REQUIRE(spm);
		REQUIRE(ai);

		spm->setUpdateDisabled(true);
		sau->initiateIntentToDoSpecialPower(spt, tgt, nullptr, 0, 0);
		f.frames(1);
		REQUIRE(sau->isActive());

		// Issue a non-AI command to abort the ability mid-unpack
		ai->aiIdle(CMD_FROM_PLAYER);
		f.frames(1);
		CHECK_FALSE(sau->isActive());
		CHECK(sau->unported() == 0);
		CHECK_FALSE(spm->isUpdateDisabled());
		CHECK(ai->stateMachine().isBusyState());
		CHECK(ai->stateMachine().temporaryStateId() == (unsigned)AI_BUSY);

		// ParalyzeDurationWhenAborted = 600 ms = 3 frames
		f.frames(4);
		CHECK_FALSE(ai->stateMachine().isBusyState());
	}
}

TEST_CASE("PORT-252: handlePackingProcessing (RW 0x00851F39..0x00852027) finishes SPECIAL_INFANTRY_CAPTURE_BUILDING early if target is missing or claimed by another capturer")
{
	Port252World f;
	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252CapturePower");
	REQUIRE(spt);
	const int kMcCapturing = CombatNames::modelCondition("CAPTURING");
	REQUIRE(kMcCapturing == 112);

	SUBCASE("second capturer aborts via finishAfterPacking when building already has another capturerID")
	{
		Object *c1 = f.unit("P252Capturer", 'A', 290.0f, 300.0f);
		Object *c2 = f.unit("P252Capturer", 'A', 310.0f, 300.0f);
		Object *bld = f.unit("P252Building", 'B', 300.0f, 320.0f);
		REQUIRE(c1);
		REQUIRE(c2);
		REQUIRE(bld);

		SpecialAbilityUpdate *sau1 = findMod<SpecialAbilityUpdate>(c1);
		SpecialAbilityUpdate *sau2 = findMod<SpecialAbilityUpdate>(c2);
		REQUIRE(sau1);
		REQUIRE(sau2);

		sau1->initiateIntentToDoSpecialPower(spt, bld, nullptr, 0, 0);
		f.frames(4);
		REQUIRE(sau1->isActive());
		REQUIRE(bld->getCapturerID() == c1->getID());

		sau2->initiateIntentToDoSpecialPower(spt, bld, nullptr, 0, 0);
		f.frames(4);
		CHECK_FALSE(sau2->isActive());
		CHECK(bld->getCapturerID() == c1->getID());
		CHECK_FALSE(c2->testModelCondition(kMcCapturing));
	}

	SUBCASE("capturer aborts cleanly when target is destroyed before handlePackingProcessing runs")
	{
		Object *c1 = f.unit("P252Capturer", 'A', 290.0f, 300.0f);
		Object *bld = f.unit("P252Building", 'B', 300.0f, 320.0f);
		REQUIRE(c1);
		REQUIRE(bld);
		SpecialAbilityUpdate *sau1 = findMod<SpecialAbilityUpdate>(c1);
		REQUIRE(sau1);

		sau1->initiateIntentToDoSpecialPower(spt, bld, nullptr, 0, 0);
		f.frames(1); // enter unpacking
		REQUIRE(sau1->isActive());
		f.logic->destroyObject(bld);
		f.frames(4); // handlePackingProcessing sees missing target -> finishAfterPacking()
		CHECK_FALSE(sau1->isActive());
	}
}

TEST_CASE("PORT-252: update (RW 0x00854F05..0x00854F1E) preserves active ability on non-AI command when OBJECT_STATUS_SUMMONING_REPLACEMENT (0x66) is set")
{
	Port252World f;
	Object *hero = f.unit("P252ParalyzeHero", 'A', 300.0f, 300.0f);
	Object *tgt = f.unit("P252PlainTarget", 'B', 320.0f, 300.0f);
	REQUIRE(hero);
	REQUIRE(tgt);
	SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(hero);
	AIUpdateInterface *ai = hero->getAIUpdateInterface();
	REQUIRE(sau);
	REQUIRE(ai);

	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252ParalyzePower");
	REQUIRE(spt);
	sau->initiateIntentToDoSpecialPower(spt, tgt, nullptr, 0, 0);
	f.frames(1);
	REQUIRE(sau->isActive());

	// Set OBJECT_STATUS_SUMMONING_REPLACEMENT (0x66): player command does NOT abort the ability
	constexpr unsigned kStatusSummoningReplacement = 0x66;
	hero->setStatus(kStatusSummoningReplacement, true);
	ai->aiIdle(CMD_FROM_PLAYER);
	f.frames(1);
	CHECK(sau->isActive());

	// Clearing 0x66 restores normal non-AI abort behavior
	hero->setStatus(kStatusSummoningReplacement, false);
	ai->aiIdle(CMD_FROM_PLAYER);
	f.frames(1);
	CHECK_FALSE(sau->isActive());
}

TEST_CASE("PORT-252: startUnpacking (RW 0x0085384C..0x00853882) sets model condition 176 for SPECIAL_GRAB_CHUNK on CHUNK_VENDOR targets only")
{
	Port252World f;
	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252GrabChunkPower");
	REQUIRE(spt);

	SUBCASE("CHUNK_VENDOR target sets model condition 176 with zero unported count")
	{
		Object *grabber = f.unit("P252ChunkGrabber", 'A', 300.0f, 300.0f);
		Object *vendor = f.unit("P252ChunkVendor", 'B', 320.0f, 300.0f);
		REQUIRE(grabber);
		REQUIRE(vendor);
		SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(grabber);
		REQUIRE(sau);

		sau->initiateIntentToDoSpecialPower(spt, vendor, nullptr, 0, 0);
		f.frames(1); // within range -> startUnpacking runs
		CHECK(sau->isActive());
		CHECK(sau->unported() == 0);
		CHECK(grabber->testModelCondition(176));
	}

	SUBCASE("non-CHUNK_VENDOR target leaves model condition 176 clear with zero unported count")
	{
		Object *grabber = f.unit("P252ChunkGrabber", 'A', 300.0f, 300.0f);
		Object *plain = f.unit("P252PlainTarget", 'B', 320.0f, 300.0f);
		REQUIRE(grabber);
		REQUIRE(plain);
		SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(grabber);
		REQUIRE(sau);

		sau->initiateIntentToDoSpecialPower(spt, plain, nullptr, 0, 0);
		f.frames(1);
		CHECK(sau->isActive());
		CHECK(sau->unported() == 0);
		CHECK_FALSE(grabber->testModelCondition(176));
	}
}

TEST_CASE("PORT-252: startPreparation (RW 0x00853C1A..0x00853C42) calls ContainModuleInterface::setWeaponStateOfRider (RW 0x0086A43E)")
{
	Port252World f;
	Object *caster = f.unit("P252WeaponStatePreparer", 'A', 300.0f, 300.0f);
	Object *inf = f.unit("P252PlainTarget", 'B', 320.0f, 300.0f);
	REQUIRE(caster);
	REQUIRE(inf);
	const int kMcWeaponStateOne = CombatNames::modelCondition("WEAPONSTATE_ONE");
	REQUIRE(kMcWeaponStateOne == 18);
	REQUIRE_FALSE(caster->testModelCondition(kMcWeaponStateOne));

	SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(caster);
	REQUIRE(sau);
	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252ParalyzePower");
	REQUIRE(spt);

	sau->initiateIntentToDoSpecialPower(spt, inf, nullptr, 0, 0);
	f.frames(1); // UnpackTime = 0 -> startPreparation runs with PreparationTime = 600 (3 frames)
	CHECK(sau->isActive());
	CHECK(caster->testModelCondition(kMcWeaponStateOne));
}

TEST_CASE("PORT-252: restartPersistent (RW 0x008524EA..0x00852512) clears m_prepFrames for SPECIAL_GRAB_PASSENGER unless target is SHRUBBERY or GRAB_AND_KILL")
{
	Port252World f;
	const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252GrabPassengerPower");
	REQUIRE(spt);

	SUBCASE("end-to-end SHRUBBERY target reloads PersistentPrepTime and fires trigger 2 (CLUB); missing target clears m_prepFrames to 0")
	{
		Object *grabber = f.unit("P252PassengerGrabber", 'A', 300.0f, 300.0f);
		Object *tree = f.unit("P252ShrubberyTarget", 'B', 320.0f, 300.0f);
		REQUIRE(grabber);
		REQUIRE(tree);
		SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(grabber);
		REQUIRE(sau);

		sau->initiateIntentToDoSpecialPower(spt, tree, nullptr, 0, 0);
		f.frames(2); // frame 1: startPreparation (m_prepFrames=1); frame 2: trigger 1 grabs tree -> restartPersistent sets m_prepFrames = 3
		CHECK(sau->triggerCount() == 1);
		CHECK(sau->prepFrames() == 3);
		CHECK(tree->getContainedBy() == grabber);

		f.frames(4); // after 3 prep frames, trigger 2 runs and sets CLUB on tree
		CHECK(sau->triggerCount() == 2);
		const int kClub = CombatNames::modelCondition("CLUB");
		REQUIRE(kClub == 91);
		CHECK(tree->testModelCondition(kClub));
	}

	SUBCASE("direct restartPersistent gate (RW 0x008524EA..0x00852512): SHRUBBERY and GRAB_AND_KILL reload PersistentPrepTime; plain infantry and null target zero m_prepFrames")
	{
		Object *grabber = f.unit("P252PassengerGrabber", 'A', 300.0f, 300.0f);
		Object *plain = f.unit("P252PlainTarget", 'B', 320.0f, 300.0f);
		Object *gak = f.unit("P252GrabAndKillTarget", 'B', 340.0f, 300.0f);
		Object *tree = f.unit("P252ShrubberyTarget", 'B', 360.0f, 300.0f);
		REQUIRE(grabber);
		REQUIRE(plain);
		REQUIRE(gak);
		REQUIRE(tree);
		SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(grabber);
		REQUIRE(sau);

		EffectExpiryProbeAbility probe(grabber, sau->abilityData());
		probe.setTargetForTest(tree->getID());
		probe.invokeRestartPersistent();
		CHECK(probe.prepFrames() == 3);

		probe.setTargetForTest(gak->getID());
		probe.invokeRestartPersistent();
		CHECK(probe.prepFrames() == 3);

		probe.setTargetForTest(plain->getID());
		probe.invokeRestartPersistent();
		CHECK(probe.prepFrames() == 0);

		probe.setTargetForTest(INVALID_ID);
		probe.invokeRestartPersistent();
		CHECK(probe.prepFrames() == 0);
	}
}

TEST_CASE("PORT-252: ModelConditionSpecialAbilityUpdate emotionPulse (RW 0x0089463A -> 0x00C0F374 / 0x00660E93) skips victims whose isOffMap differs from caster")
{
	Port252World f;

	WorldHeightMap hm;
	hm.m_width = 200;
	hm.m_height = 200;
	hm.m_data.assign(200u * 200u, 0);
	hm.m_boundaries = { ICoord2D{ 100, 100 }, ICoord2D{ 200, 200 } }; // active boundary: (0,0) .. (1000,1000)
	MapChunks chunks;
	TerrainLogic boundaryTerrain;
	boundaryTerrain.init(hm, chunks);
	f.logic->setTerrain(&boundaryTerrain);

	SUBCASE("on-map caster terrifies on-map enemy and skips off-map enemy")
	{
		Object *caster = f.unit("P252TerrorCaster", 'A', 950.0f, 500.0f);
		Object *onMapEnemy = f.unit("P252PlainTarget", 'B', 900.0f, 500.0f);
		Object *offMapEnemy = f.unit("P252PlainTarget", 'B', 1020.0f, 500.0f);
		REQUIRE(caster);
		REQUIRE(onMapEnemy);
		REQUIRE(offMapEnemy);

		REQUIRE_FALSE(caster->isOffMap());
		REQUIRE_FALSE(onMapEnemy->isOffMap());
		REQUIRE(offMapEnemy->isOffMap());

		ModelConditionSpecialAbilityUpdate *mc = findMod<ModelConditionSpecialAbilityUpdate>(caster);
		REQUIRE(mc);
		const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252TerrorPower");
		REQUIRE(spt);

		mc->initiateIntentToDoSpecialPower(spt, nullptr, nullptr, 0, 0);
		f.frames(1); // triggerAbilityEffect runs on frame 1 (requestEmotion delay = 1)

		EmotionTrackerUpdate *onEmo = EmotionTrackerUpdate::of(*onMapEnemy);
		EmotionTrackerUpdate *offEmo = EmotionTrackerUpdate::of(*offMapEnemy);
		REQUIRE(onEmo);
		REQUIRE(offEmo);
		CHECK(mc->emotionRequests() == 1);
		CHECK(onEmo->requested(EMOTION_TERROR));
		CHECK_FALSE(offEmo->requested(EMOTION_TERROR));
	}

	SUBCASE("off-map caster terrifies off-map enemy and skips on-map enemy")
	{
		Object *caster = f.unit("P252TerrorCaster", 'A', 1050.0f, 500.0f);
		Object *onMapEnemy = f.unit("P252PlainTarget", 'B', 950.0f, 500.0f);
		Object *offMapEnemy = f.unit("P252PlainTarget", 'B', 1020.0f, 500.0f);
		REQUIRE(caster);
		REQUIRE(onMapEnemy);
		REQUIRE(offMapEnemy);

		REQUIRE(caster->isOffMap());
		REQUIRE_FALSE(onMapEnemy->isOffMap());
		REQUIRE(offMapEnemy->isOffMap());

		ModelConditionSpecialAbilityUpdate *mc = findMod<ModelConditionSpecialAbilityUpdate>(caster);
		REQUIRE(mc);
		const SpecialPowerTemplate *spt = f.powers.findSpecialPowerTemplate("P252TerrorPower");
		REQUIRE(spt);

		mc->initiateIntentToDoSpecialPower(spt, nullptr, nullptr, 0, 0);
		f.frames(1);

		EmotionTrackerUpdate *onEmo = EmotionTrackerUpdate::of(*onMapEnemy);
		EmotionTrackerUpdate *offEmo = EmotionTrackerUpdate::of(*offMapEnemy);
		REQUIRE(onEmo);
		REQUIRE(offEmo);
		CHECK(mc->emotionRequests() == 1);
		CHECK_FALSE(onEmo->requested(EMOTION_TERROR));
		CHECK(offEmo->requested(EMOTION_TERROR));
	}

	f.logic->reset();
	f.logic->setTerrain(nullptr);
}

TEST_CASE("PORT-252: SpecialAbilityUpdate::onEffectExpired (RW 0x00851E9A) clears m_effectExpiry")
{
	Port252World f;
	Object *hero = f.unit("P252ParalyzeHero", 'A', 300.0f, 300.0f);
	REQUIRE(hero);
	SpecialAbilityUpdate *sau = findMod<SpecialAbilityUpdate>(hero);
	REQUIRE(sau);

	EffectExpiryProbeAbility probe(hero, sau->abilityData());
	probe.setEffectExpiryForTest(42);
	CHECK(probe.effectExpiry() == 42);
	probe.invokeBaseOnEffectExpired();
	CHECK(probe.effectExpiry() == 0);
}
