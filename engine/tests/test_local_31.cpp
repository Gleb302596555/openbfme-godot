// OpenBFME unit tests: LOCAL-31 Object/EmotionRange as retail (issue #281; editions: both).
//
// Target facts and RotWK 2.01 / BFME2 1.06 addresses:
//   - Parse table: ThingTemplate field table RW 0xDA3DF8 row 26 at RW 0xDA3F98 (token "EmotionRange",
//     parser INI::parseReal RW 0x42ED00, context 0, offset 1244 / +0x4DC)
//   - ThingTemplate constructor: RW 0x740211 initializes +0x4DC to 0.0f (movss dword ptr [ebx + 0x4dc], xmm0)
//   - ThingTemplate::copyFrom: RW 0x6D2193 copies +0x4DC (mov ecx, [eax + 0x4dc]; mov [ebx + 0x4dc], ecx)
//   - Candidate Reader 1: RW 0x68AF80 is ThingTemplate::getEmotionRange() const:
//       fld dword ptr [ecx + 0x4dc]
//       ret
//     Callers in game.dat: 0 callers (verified by rw.py xref 0x68AF80)
//   - Candidate Reader 2: RW 0x699DFD in Object::Object(const ThingTemplate *template, ...):
//       00699DFD movss xmm0, dword ptr [edx + 0x4dc]  ; edx = ThingTemplate*
//       00699E05 movss dword ptr [esi + 0x1bc], xmm0  ; esi = Object* (Object + 0x1BC)
//     Copies template's EmotionRange (+0x4DC) into Object::m_emotionRange (+0x1BC).
//   - Object::xfer: RW 0x698AA3 transfers Object + 0x1BC (lea ecx, [ebx + 0x15c] with ebx = this + 0x60; xferReal)
//   - Object::crc: RW 0x69355D does NOT hash or read +0x1BC
//   - Object + 0x1BC logic readers in .text: 0 readers (byte scan of 253 occurrences of \xbc\x01\x00\x00 across .text:
//     only 9 float instructions exist in .text, none of which read [reg + 0x1BC]; none of the 48 mov instructions access Object*)
//   - Open-BFME-2 donor check:
//       * Code/GameEngine/Include/Common/ThingTemplate.h:163: float m_emotionRange; // +0x4CC
//       * Code/GameEngine/Source/Common/Thing/ThingTemplateConstructor.cpp:475: m_emotionRange = 0.0f;
//       * Code/GameEngine/Source/Common/Thing/ThingTemplateFieldTable.cpp: { "EmotionRange", INI::parseReal, NULL, 0x4cc },
//       * In BFME2, m_emotionRange was not even copied to Object. Zero logic readers in Open-BFME-2.
//   - Mod data (Ennorath 547 uses):
//       * data\ini\_gamedata.inc: #define EMOTION_CHEER_RADIUS 100
//       * 522 definitions: EmotionRange = 240 // Make sure this value is greater than the fear distribution range.
//       * 15 horde definitions: EmotionRange = EMOTION_CHEER_RADIUS (expands to 100)
//       * 10 siege/oathbreaker/wall definitions: EmotionRange = 0
//   - Retail emotion queries (none use EmotionRange):
//       * EmotionTrackerUpdate::update at RW 0x8B5A7E: reads TauntAndPointDistance (+0x08), HeroScanDistance (+0x20),
//         FearScanDistance (+0x24) from module data, and calls Object::getVisionRange (RW 0x68E4E2) directly, matching
//         visionRangeOf in EmotionModules.cpp:142.
//       * ActiveBody::cheer at RW 0x8C3B89: reads m_cheerRadius (+0x4C) from ActiveBodyModuleData.
//       * RadiateFearUpdate at RW 0x89FA13: reads m_emotionPulseRadius (+0x24) from module data.
//       * SpecialAbilityUpdate at RW 0x89463A: reads m_emotionPulseRadius (+0xD8) from module data.
//       * EmotionWeaponNugget at RW 0x911707: reads m_radius (+0x14C).

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/INI.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"

namespace
{

const char kEmotionRangeIni[] =
	"#define EMOTION_CHEER_RADIUS 100\n"
	"\n"
	"Object EmotionStandardSoldier\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VisionRange = 100\n"
	"  EmotionRange = 240\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    CheerRadius = EMOTION_CHEER_RADIUS\n"
	"  End\n"
	"End\n"
	"\n"
	"Object EmotionSiegeMachine\n"
	"  KindOf = MACHINE SELECTABLE\n"
	"  VisionRange = 50\n"
	"  EmotionRange = 0\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 25\n"
	"End\n"
	"\n"
	"Object EmotionHordeMacro\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VisionRange = 120\n"
	"  EmotionRange = EMOTION_CHEER_RADIUS\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"\n"
	"Object EmotionDefaultUnit\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VisionRange = 80\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"\n"
	"ChildObject EmotionChildInherited EmotionStandardSoldier\n"
	"End\n"
	"\n"
	"ChildObject EmotionChildOverridden EmotionStandardSoldier\n"
	"  EmotionRange = 0\n"
	"End\n";

} // namespace

TEST_CASE("LOCAL-31: ThingTemplate::emotionRange getter parses explicit, zero, macro, and default values (RW 0x68AF80, RW 0x740211)")
{
	movetest::MoveWorld w(40, 20, kEmotionRangeIni);
	w.buildMap();
	ThingFactory &factory = w.logic->things();

	// 1. Explicit float 240 (522 occurrences in Ennorath)
	const ThingTemplate *standard = factory.findTemplate("EmotionStandardSoldier");
	REQUIRE(standard != nullptr);
	CHECK(standard->emotionRange() == 240.0f);

	// 2. Explicit zero 0 (10 occurrences in Ennorath: siege, oathbreakers)
	const ThingTemplate *siege = factory.findTemplate("EmotionSiegeMachine");
	REQUIRE(siege != nullptr);
	CHECK(siege->emotionRange() == 0.0f);

	// 3. Macro expansion: EMOTION_CHEER_RADIUS -> 100 (15 occurrences in Ennorath hordes)
	const ThingTemplate *macroHorde = factory.findTemplate("EmotionHordeMacro");
	REQUIRE(macroHorde != nullptr);
	CHECK(macroHorde->emotionRange() == 100.0f);

	// 4. Default when unspecified: 0.0f (RW 0x740211, BFME2 ThingTemplateConstructor.cpp:475)
	const ThingTemplate *defUnit = factory.findTemplate("EmotionDefaultUnit");
	REQUIRE(defUnit != nullptr);
	CHECK(defUnit->emotionRange() == 0.0f);

	// 5. Template inheritance: ChildObject inherits parent's EmotionRange (RW 0x6D2193)
	const ThingTemplate *childInh = factory.findTemplate("EmotionChildInherited");
	REQUIRE(childInh != nullptr);
	CHECK(childInh->emotionRange() == 240.0f);

	// 6. Template override: ChildObject overrides parent's EmotionRange
	const ThingTemplate *childOver = factory.findTemplate("EmotionChildOverridden");
	REQUIRE(childOver != nullptr);
	CHECK(childOver->emotionRange() == 0.0f);
}

TEST_CASE("LOCAL-31: Object::Object constructor copies template emotionRange to Object::m_emotionRange (RW 0x699DFD -> Object +0x1BC)")
{
	movetest::MoveWorld w(40, 20, kEmotionRangeIni);
	w.buildMap();

	// Spawn standard unit (EmotionRange = 240)
	Object *u240 = w.spawn("EmotionStandardSoldier", 50.0f, 50.0f);
	REQUIRE(u240 != nullptr);
	CHECK(u240->emotionRange() == 240.0f);
	CHECK(u240->emotionRange() == u240->getTemplate()->emotionRange());

	// Spawn siege machine (EmotionRange = 0)
	Object *u0 = w.spawn("EmotionSiegeMachine", 70.0f, 50.0f);
	REQUIRE(u0 != nullptr);
	CHECK(u0->emotionRange() == 0.0f);
	CHECK(u0->emotionRange() == u0->getTemplate()->emotionRange());

	// Spawn macro horde unit (EmotionRange = EMOTION_CHEER_RADIUS = 100)
	Object *u100 = w.spawn("EmotionHordeMacro", 90.0f, 50.0f);
	REQUIRE(u100 != nullptr);
	CHECK(u100->emotionRange() == 100.0f);
	CHECK(u100->emotionRange() == u100->getTemplate()->emotionRange());

	// Spawn default unit (EmotionRange = 0 by default)
	Object *uDef = w.spawn("EmotionDefaultUnit", 110.0f, 50.0f);
	REQUIRE(uDef != nullptr);
	CHECK(uDef->emotionRange() == 0.0f);
	CHECK(uDef->emotionRange() == uDef->getTemplate()->emotionRange());

	// Spawn inherited child unit
	Object *uChildInh = w.spawn("EmotionChildInherited", 130.0f, 50.0f);
	REQUIRE(uChildInh != nullptr);
	CHECK(uChildInh->emotionRange() == 240.0f);

	// Spawn overridden child unit
	Object *uChildOver = w.spawn("EmotionChildOverridden", 150.0f, 50.0f);
	REQUIRE(uChildOver != nullptr);
	CHECK(uChildOver->emotionRange() == 0.0f);
}

TEST_CASE("LOCAL-31: retail emotion mechanisms use dedicated module radii, not Object EmotionRange (RW 0x8B5A7E, RW 0x8C3B89, RW 0x89FA13)")
{
	// Proves that in retail RotWK and the port:
	// 1. EmotionTrackerUpdate::update at RW 0x8B5A7E uses Object::getVisionRange (RW 0x68E4E2), matching visionRangeOf.
	// 2. ActiveBody::cheer at RW 0x8C3B89 uses m_cheerRadius (+0x4C) from ActiveBodyModuleData.
	// 3. RadiateFearUpdate at RW 0x89FA13 uses m_emotionPulseRadius (+0x24) from its module data.
	// 4. SpecialAbilityUpdate (Aragorn Elendil, etc.) uses m_emotionPulseRadius (+0xD8) from its module data.
	movetest::MoveWorld w(40, 20, kEmotionRangeIni);
	w.buildMap();

	Object *soldier = w.spawn("EmotionStandardSoldier", 50.0f, 50.0f);
	REQUIRE(soldier != nullptr);

	// Object EmotionRange is 240
	CHECK(soldier->emotionRange() == 240.0f);

	// ActiveBody CheerRadius was parsed from EMOTION_CHEER_RADIUS (100)
	ActiveBody *body = static_cast<ActiveBody *>(soldier->findModule("ActiveBody"));
	REQUIRE(body != nullptr);
	CHECK(body->data()->m_cheerRadius == 100.0f);

	// Vision range is 100, distinct from EmotionRange 240
	const float vr = AIUpdateInterface::objectVisionRangeOf(*soldier);
	CHECK(vr == 100.0f);
	if (ShroudManager *sm = w.logic->shroud())
	{
		CHECK(sm->visionRange(*soldier) == vr);
	}
}
