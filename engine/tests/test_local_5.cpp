// OpenBFME unit tests: LOCAL-5 capture flash and audio tick (issue #110, TOP20 #12).
//
// RotWK addresses:
//   SpecialAbilityUpdate::continuePreparation RW 0x852A3E .. 0x852CA1
//   DoCaptureFX gate: RW 0x852B14 (SpecialAbilityUpdateModuleData + 0xAF)
//   Accumulator math: RW 0x852B33 .. 0x852B9C
//   Flash condition: RW 0x852B9E, 0x852BA6 (old_bit != 0 && new_bit == 0)
//   Caster indicator color: RW 0x852BB1 (RW 0x68B6F5 Object::getIndicatorColor)
//   Saturation scaling: RW 0x852BC4 .. 0x852BD5 (RW 0x6704A2, SelectionFlashSaturationFactor 0.6f)
//   Target drawable flash: RW 0x852BDD (RW 0x678FA7 Drawable::flashAsSelected)
//   Linked flag query: RW 0x852BE6 .. 0x852C3B (ThePartitionManager radius 150.0f, KindOf LINKED_TO_FLAG = 50)
//   Flag drawable flash: RW 0x852C35 (RW 0x678FA7 Drawable::flashAsSelected)
//   Audio tick: RW 0x852C40 .. 0x852C69 (TheAudio slot 0x138 MiscAudio + 0x20 DefectorTimerTickSound on target ID)

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/KindOfTokens.h"
#include "GameClient/ClientEvents.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"

#include <cmath>
#include <vector>

namespace
{
struct Local5World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;

	explicit Local5World(const char *objects)
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}

	~Local5World()
	{
		TheSpecialPowerStore = savedPowers;
	}
};

const char kCaptureWorld[] =
	"SpecialPower SpecialPowerCaptureBuilding\n"
	"  Enum = SPECIAL_INFANTRY_CAPTURE_BUILDING\n"
	"  ReloadTime = 0\n"
	"End\n"
	"SpecialPower SpecialPowerCaptureDisabled\n"
	"  Enum = SPECIAL_INFANTRY_CAPTURE_BUILDING\n"
	"  ReloadTime = 0\n"
	"End\n"
	"Object CapturerUnit\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = SpecialPowerModule PowerTag\n"
	"    SpecialPowerTemplate = SpecialPowerCaptureBuilding\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate AbilityTag\n"
	"    SpecialPowerTemplate = SpecialPowerCaptureBuilding\n"
	"    PreparationTime = 40000\n"
	"    DoCaptureFX = Yes\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"End\n"
	"Object CapturerNoFX\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = SpecialPowerModule PowerTag\n"
	"    SpecialPowerTemplate = SpecialPowerCaptureDisabled\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate AbilityTag\n"
	"    SpecialPowerTemplate = SpecialPowerCaptureDisabled\n"
	"    PreparationTime = 40000\n"
	"    DoCaptureFX = No\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"End\n"
	"Object TargetStructure\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object FlagStructure\n"
	"  KindOf = STRUCTURE LINKED_TO_FLAG IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n";

constexpr unsigned kStatusUsingAbility = 0x18; // USING_ABILITY

SpecialAbilityUpdate *abilityOf(Object *o)
{
	auto *m = dynamic_cast<SpecialAbilityUpdate *>(o->findModule("SpecialAbilityUpdate"));
	REQUIRE(m != nullptr);
	return m;
}
} // namespace

TEST_CASE("LOCAL-5 capture flash timing: DoCaptureFX accelerates countdown flashes at exact RotWK frames (RW 0x852B14 .. 0x852C88)")
{
	Local5World f(kCaptureWorld);
	ClientEventRecorder events(*f.logic);
	f.logic->setClientHooks(&events);
	struct Detach { GameLogic &logic; ~Detach() { logic.setClientHooks(nullptr); } } detach{ *f.logic };

	// Alice (team 0 / side A) captures Bob's structure (team 1 / side B)
	Object *caster = f.unit("CapturerUnit", 'A', 300, 300);
	Object *target = f.unit("TargetStructure", 'B', 310, 300);
	Object *flag = f.unit("FlagStructure", 'B', 315, 305); // within 150 radius of caster

	Player *alice = f.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	alice->setPlayerColor(0xFF224466u); // set distinct indicator color

	SpecialAbilityUpdate *u = abilityOf(caster);
	u->initiateIntentToDoSpecialPower(u->abilityData()->m_specialPowerTemplate, target, nullptr, 0, 0);

	// Advance 1 frame to enter preparation
	f.frames(1);
	REQUIRE(caster->testStatus(kStatusUsingAbility));
	REQUIRE(u->isActive());

	const std::vector<int> kExpectedFrames = { 49, 69, 85, 98, 110, 120, 130, 139, 147, 155, 162, 170, 177, 183, 190, 196 };
	std::vector<int> actualFrames;

	// Saturated indicator color expected
	const std::uint32_t packed = caster->getIndicatorColor();
	constexpr float kInv255 = 1.0f / 255.0f;
	const RGBColor indicator = {
		((packed >> 16) & 0xFF) * kInv255,
		((packed >> 8) & 0xFF) * kInv255,
		(packed & 0xFF) * kInv255
	};
	const float sat = f.logic->settings().selectionFlashSaturationFactor;
	const float halfSat = sat * 0.5f;
	const RGBColor expectedColor = { indicator.red * sat - halfSat, indicator.green * sat - halfSat, indicator.blue * sat - halfSat };

	events.take(); // discard initiation events

	// Frame loop for 200 preparation frames (frames 1 to 200 of preparation)
	for (int prepFrame = 1; prepFrame <= 200; ++prepFrame)
	{
		f.frames(1);
		auto frameEvents = events.take();
		bool flashedTarget = false;
		bool flashedFlag = false;

		for (const auto &ev : frameEvents)
		{
			if (ev.kind == ClientEvent::FLASH_AS_SELECTED)
			{
				if (ev.object == target->getID())
				{
					flashedTarget = true;
					REQUIRE(ev.hasFlashColor);
					// tolerance: floating-point color saturation math from packed integer RGB
					CHECK(std::abs(ev.flashColor.red - expectedColor.red) < 0.001f);
					// tolerance: floating-point color saturation math from packed integer RGB
					CHECK(std::abs(ev.flashColor.green - expectedColor.green) < 0.001f);
					// tolerance: floating-point color saturation math from packed integer RGB
					CHECK(std::abs(ev.flashColor.blue - expectedColor.blue) < 0.001f);
				}
				else if (ev.object == flag->getID())
				{
					flashedFlag = true;
					REQUIRE(ev.hasFlashColor);
					// tolerance: floating-point color saturation math from packed integer RGB
					CHECK(std::abs(ev.flashColor.red - expectedColor.red) < 0.001f);
					// tolerance: floating-point color saturation math from packed integer RGB
					CHECK(std::abs(ev.flashColor.green - expectedColor.green) < 0.001f);
					// tolerance: floating-point color saturation math from packed integer RGB
					CHECK(std::abs(ev.flashColor.blue - expectedColor.blue) < 0.001f);
				}
			}
		}

		if (flashedTarget)
		{
			actualFrames.push_back(prepFrame);
			CHECK(flashedFlag); // linked flag should flash alongside target
		}
	}

	for (size_t i = 0; i < actualFrames.size(); ++i)
	{
		MESSAGE("actualFrames[", i, "] = ", actualFrames[i]);
	}
	CHECK(actualFrames == kExpectedFrames);
}

TEST_CASE("LOCAL-5 capture flash disabled: DoCaptureFX = No produces zero flash events (RW 0x852B14)")
{
	Local5World f(kCaptureWorld);
	ClientEventRecorder events(*f.logic);
	f.logic->setClientHooks(&events);
	struct Detach { GameLogic &logic; ~Detach() { logic.setClientHooks(nullptr); } } detach{ *f.logic };

	Object *caster = f.unit("CapturerNoFX", 'A', 300, 300);
	Object *target = f.unit("TargetStructure", 'B', 310, 300);
	Object *flag = f.unit("FlagStructure", 'B', 315, 305);

	SpecialAbilityUpdate *u = abilityOf(caster);
	u->initiateIntentToDoSpecialPower(u->abilityData()->m_specialPowerTemplate, target, nullptr, 0, 0);

	f.frames(1);
	events.take();

	int totalFlashEvents = 0;
	for (int prepFrame = 1; prepFrame <= 200; ++prepFrame)
	{
		f.frames(1);
		for (const auto &ev : events.take())
		{
			if (ev.kind == ClientEvent::FLASH_AS_SELECTED)
			{
				++totalFlashEvents;
			}
		}
	}

	CHECK(totalFlashEvents == 0);
}

TEST_CASE("LOCAL-5 capture aborts when target is allied (RW 0x852A7E)")
{
	Local5World f(kCaptureWorld);
	Object *caster = f.unit("CapturerUnit", 'A', 300, 300);
	Object *alliedTarget = f.unit("TargetStructure", 'A', 310, 300); // same team

	SpecialAbilityUpdate *u = abilityOf(caster);
	u->initiateIntentToDoSpecialPower(u->abilityData()->m_specialPowerTemplate, alliedTarget, nullptr, 0, 0);

	f.frames(1);
	// In continuePreparation, target relationship is ALLIES, so it returns false and does not stay in preparation
	CHECK_FALSE(caster->testStatus(kStatusUsingAbility));
}
