// OpenBFME unit tests. GPL-3.0.
// LOCAL-32 (issue #282): Object / InstanceScaleFuzziness and Object / ShowHealthInSelectionDecal.
//
// Binary verification against RotWK 2.01 game.dat (MD5 daed3668006cd90f01c34e5a7da1901f):
//
// 1. InstanceScaleFuzziness (offset +0x4F4 = 1268):
//   * Row RW 0xDA4678 of the object table (RW 0xDA3DB8 .. 0xDA49E8): name RW 0xC25654 "InstanceScaleFuzziness",
//     parser RW 0x42ED00 (INI::parseReal), userData 0, store offset +0x4F4.
//   * ThingTemplate constructor (prologue RW 0x73ED19): +0x4F4 initialized to 0.0f at RW 0x740190 (movss [ebx+0x4F4], xmm0).
//   * ThingTemplate copy function (prologue RW 0x6D1D80): moved at RW 0x6D21DB (mov ecx, [eax+0x4F4]) -> RW 0x6D21E1 (mov [ebx+0x4F4], ecx).
//   * Leads checked: RW 0x46E2FC is in the fog lighting function (caller RW 0x471A27, lane VISUAL-2) operating on a 3-float vector
//     at +0x4F0 of an internal render/lighting struct, not ThingTemplate; RW 0x893203 is a byte flag compare (cmp byte [esi+0x4F4], 0)
//     in an unrelated class.
//   * Real instance scale: in Drawable constructor RW 0x679FD7 (movss xmm0, [edi+0x4F0]) -> RW 0x679FE2 (movss [esi+0x200], xmm0),
//     Scale (+0x4F0, default 1.0f at RW 0x74008C) alone sets the instance scale. Nothing randomises it in RotWK 2.01 (stop S-7192).
//   * BFME2 donor (open-bfme-2): m_instanceScaleFuzziness at +0x4E4 (ThingTemplate.h:169). All 89 occurrences of 0x4E4 in .text
//     verified: only constructor 0x73E7CA, copy 0x6D155C and parse table 0xADC138 touch it; zero logic or render readers.
//
// 2. ShowHealthInSelectionDecal (offset +0x641 = 1601):
//   * Row RW 0xDA4848 of the object table: name RW 0xC2542C "ShowHealthInSelectionDecal", parser RW 0x42E558 (INI::parseBool),
//     userData 0, store offset +0x641.
//   * ThingTemplate constructor: +0x641 initialized to false (0) at RW 0x73FFBD (mov byte ptr [ebx+0x641], 0).
//   * ThingTemplate copy function: moved at RW 0x6D26D6 (mov cl, byte ptr [eax+0x641]) -> RW 0x6D26DC (mov byte ptr [ebx+0x641], cl).
//   * Full .text scan: all 8 occurrences of displacement 0x641 (bytes 41 06 00 00) verified: constructor RW 0x73FFBD, copy RW 0x6D26D6,
//     and 6 branch-target/disp8 coincidences. ZERO logic or render readers exist in the entire RotWK binary.
//   * BFME2 donor: m_showHealthInSelectionDecal at +0x631 (ThingTemplate.h:274). All 10 occurrences of 0x631 in .text verified:
//     constructor 0x73E5F3, copy 0x6D1A57, parse table 0xADC218, and 7 relative branches. ZERO readers.
//   * Selection decal creation path: W3DScriptedModelDraw vslot 0x6C (RW 0x4B2B2A) -> RadiusDecalTemplate::createSelectionRadiusDecal
//     (RW 0x732BAD). Parameters: Coord3D position, int count/selected, int indicatorColor, RadiusDecal *out, float scale.
//     Neither health nor the ShowHealthInSelectionDecal flag is passed or queried.
//   * Health in retail RotWK is drawn exclusively via 2D overhead health bars (DrawableIconUI, RW 0x676346), never in ground selection decals.
//   * Stop S-7286.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameClient/SelectionDecals.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

namespace
{
struct Local32World : CombatWorld
{
	explicit Local32World(const std::string &objects)
		: CombatWorld("")
	{
		const std::string err = w.load(objects, INI_LOAD_OVERWRITE, "local32.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

std::string propSnippet(const std::string &name, const std::string &extraFields)
{
	return "Object " + name + "\n"
		"  KindOf = IMMOBILE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryHeight = 20\n" +
		extraFields +
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n";
}

std::string unitSnippet(const std::string &name, const std::string &extraFields)
{
	return "Object " + name + "\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 10\n"
		"  GeometryHeight = 20\n" +
		extraFields +
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"    InitialHealth = 1000\n"
		"  End\n"
		"End\n";
}
} // namespace

TEST_CASE("LOCAL-32 InstanceScaleFuzziness: template stores parsed value (RW 0xDA4678, +0x4F4), instance scale is strictly Scale (RW 0x679FD7), stop S-7192")
{
	// Ennorath uses InstanceScaleFuzziness on 89 definitions: 59 x 0.3, 25 x 0.1, 5 x 0.4
	const std::string ini = propSnippet("L32Tree30", "  Scale = 0.75\n  InstanceScaleFuzziness = 0.3\n") +
		propSnippet("L32Tree40", "  Scale = 2.0\n  InstanceScaleFuzziness = 0.4\n") +
		propSnippet("L32Tree10", "  InstanceScaleFuzziness = 0.1\n") +
		propSnippet("L32TreeZero", "  InstanceScaleFuzziness = 0.0\n") +
		propSnippet("L32TreeDefault", "");

	Local32World w(ini);

	// 1. Template stores the exact parsed float value via instanceScaleFuzziness() and findField()
	const ThingTemplate *tt30 = w.w.get("L32Tree30");
	REQUIRE(tt30 != nullptr);
	CHECK(tt30->instanceScaleFuzziness() == 0.3f);
	const FieldValue *fv30 = tt30->findField("InstanceScaleFuzziness");
	REQUIRE(fv30 != nullptr);
	REQUIRE(std::holds_alternative<float>(*fv30));
	CHECK(std::get<float>(*fv30) == 0.3f);

	const ThingTemplate *tt40 = w.w.get("L32Tree40");
	REQUIRE(tt40 != nullptr);
	CHECK(tt40->instanceScaleFuzziness() == 0.4f);

	const ThingTemplate *tt10 = w.w.get("L32Tree10");
	REQUIRE(tt10 != nullptr);
	CHECK(tt10->instanceScaleFuzziness() == 0.1f);

	const ThingTemplate *ttZero = w.w.get("L32TreeZero");
	REQUIRE(ttZero != nullptr);
	CHECK(ttZero->instanceScaleFuzziness() == 0.0f);

	// Default when omitted is 0.0f (RW 0x740190)
	const ThingTemplate *ttDef = w.w.get("L32TreeDefault");
	REQUIRE(ttDef != nullptr);
	CHECK(ttDef->instanceScaleFuzziness() == 0.0f);
	CHECK(ttDef->findField("InstanceScaleFuzziness") == nullptr);

	// 2. Ten instances of L32Tree30 in the world have bit-for-bit identical scale (0.75f).
	// In RotWK 2.01, InstanceScaleFuzziness is NOT read by logic or renderer (stop S-7192); Scale alone sets the scale.
	float x = 100.0f;
	for (int i = 0; i < 10; ++i)
	{
		Object *o = w.unit("L32Tree30", 'A', x, 400.0f);
		x += 25.0f;
		REQUIRE(o != nullptr);
		CHECK(o->getInstanceScale() == 0.75f);
	}

	// L32Tree40: scale is exactly 2.0f
	Object *big = w.unit("L32Tree40", 'A', 100.0f, 450.0f);
	REQUIRE(big != nullptr);
	CHECK(big->getInstanceScale() == 2.0f);

	// L32Tree10: no Scale specified -> constructor default 1.0f (RW 0x74008C)
	Object *one = w.unit("L32Tree10", 'A', 100.0f, 500.0f);
	REQUIRE(one != nullptr);
	CHECK(one->getInstanceScale() == 1.0f);

	// 3. ChildObject inheritance (copy function RW 0x6D21DB):
	const std::string childIni =
		"ChildObject L32Child30 L32Tree30\n  InstanceScaleFuzziness = 0.4\nEnd\n"
		"ChildObject L32ChildInherit L32Tree30\nEnd\n"
		"ChildObject L32ChildScale L32Tree30\n  Scale = 0.5\nEnd\n";
	Local32World wChild(ini + childIni);

	const ThingTemplate *child30 = wChild.w.get("L32Child30");
	REQUIRE(child30 != nullptr);
	CHECK(child30->instanceScaleFuzziness() == 0.4f);

	const ThingTemplate *childInherit = wChild.w.get("L32ChildInherit");
	REQUIRE(childInherit != nullptr);
	CHECK(childInherit->instanceScaleFuzziness() == 0.3f);

	Object *childObj = wChild.unit("L32ChildScale", 'A', 100.0f, 550.0f);
	REQUIRE(childObj != nullptr);
	CHECK(childObj->getInstanceScale() == 0.5f);
	CHECK(childObj->getTemplate()->instanceScaleFuzziness() == 0.3f);
}

TEST_CASE("LOCAL-32 ShowHealthInSelectionDecal: template stores parsed boolean (RW 0xDA4848, +0x641), default false (RW 0x73FFBD), ChildObject copy (RW 0x6D26D6), stop S-7286")
{
	// Ennorath uses ShowHealthInSelectionDecal on 84 definitions (82 x Yes, 2 x No)
	const std::string ini = unitSnippet("L32HealthYes", "  ShowHealthInSelectionDecal = Yes\n") +
		unitSnippet("L32HealthNo", "  ShowHealthInSelectionDecal = No\n") +
		unitSnippet("L32HealthDef", "");

	Local32World w(ini);

	// 1. Template parses and stores boolean value via showHealthInSelectionDecal() and findField()
	const ThingTemplate *ttYes = w.w.get("L32HealthYes");
	REQUIRE(ttYes != nullptr);
	CHECK(ttYes->showHealthInSelectionDecal() == true);
	const FieldValue *fvYes = ttYes->findField("ShowHealthInSelectionDecal");
	REQUIRE(fvYes != nullptr);
	REQUIRE(std::holds_alternative<bool>(*fvYes));
	CHECK(std::get<bool>(*fvYes) == true);

	const ThingTemplate *ttNo = w.w.get("L32HealthNo");
	REQUIRE(ttNo != nullptr);
	CHECK(ttNo->showHealthInSelectionDecal() == false);
	const FieldValue *fvNo = ttNo->findField("ShowHealthInSelectionDecal");
	REQUIRE(fvNo != nullptr);
	REQUIRE(std::holds_alternative<bool>(*fvNo));
	CHECK(std::get<bool>(*fvNo) == false);

	// Default when omitted is false (RW 0x73FFBD)
	const ThingTemplate *ttDef = w.w.get("L32HealthDef");
	REQUIRE(ttDef != nullptr);
	CHECK(ttDef->showHealthInSelectionDecal() == false);
	CHECK(ttDef->findField("ShowHealthInSelectionDecal") == nullptr);

	// 2. ChildObject inheritance (copy function RW 0x6D26D6):
	const std::string childIni =
		"ChildObject L32ChildHealthYes L32HealthNo\n  ShowHealthInSelectionDecal = Yes\nEnd\n"
		"ChildObject L32ChildHealthNo L32HealthYes\n  ShowHealthInSelectionDecal = No\nEnd\n"
		"ChildObject L32ChildHealthInherit L32HealthYes\nEnd\n";
	Local32World wChild(ini + childIni);

	const ThingTemplate *childYes = wChild.w.get("L32ChildHealthYes");
	REQUIRE(childYes != nullptr);
	CHECK(childYes->showHealthInSelectionDecal() == true);

	const ThingTemplate *childNo = wChild.w.get("L32ChildHealthNo");
	REQUIRE(childNo != nullptr);
	CHECK(childNo->showHealthInSelectionDecal() == false);

	const ThingTemplate *childInherit = wChild.w.get("L32ChildHealthInherit");
	REQUIRE(childInherit != nullptr);
	CHECK(childInherit->showHealthInSelectionDecal() == true);
}

TEST_CASE("LOCAL-32 ShowHealthInSelectionDecal: selection decal does not change with health or flag (RW 0x4B2B2A -> RW 0x732BAD, stop S-7286)")
{
	const std::string ini = unitSnippet("L32DecalUnitYes", "  ShowHealthInSelectionDecal = Yes\n") +
		unitSnippet("L32DecalUnitNo", "  ShowHealthInSelectionDecal = No\n");
	Local32World w(ini);

	Object *unitYes = w.unit("L32DecalUnitYes", 'A', 200.0f, 300.0f);
	REQUIRE(unitYes != nullptr);
	REQUIRE(unitYes->getBodyModule() != nullptr);
	CHECK(unitYes->getBodyModule()->getHealth() == 1000.0f);
	CHECK(unitYes->getBodyModule()->getMaxHealth() == 1000.0f);

	Object *unitNo = w.unit("L32DecalUnitNo", 'A', 250.0f, 300.0f);
	REQUIRE(unitNo != nullptr);
	REQUIRE(unitNo->getBodyModule() != nullptr);

	SelectionDecalSettings settings;
	settings.showSelectedUnitMarker = true;
	settings.useSimpleMergeDecals = true;

	const Player *playerA = w.logic->players().findPlayerWithName("Alice");
	REQUIRE(playerA != nullptr);

	// Full health (1000/1000)
	std::vector<SelectionDecal> decalsFull = BuildSelectionDecals(*w.logic, nullptr, playerA, { unitYes->getID() }, settings, 0);

	// Damaged to 50% health (500/1000)
	DamageInfo dmg50;
	dmg50.m_input.m_amount = 500.0f;
	dmg50.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	unitYes->attemptDamage(dmg50);
	CHECK(unitYes->getBodyModule()->getHealth() == 500.0f);
	std::vector<SelectionDecal> decalsHalf = BuildSelectionDecals(*w.logic, nullptr, playerA, { unitYes->getID() }, settings, 0);

	// Damaged to 10% health (100/1000)
	DamageInfo dmg40;
	dmg40.m_input.m_amount = 400.0f;
	dmg40.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	unitYes->attemptDamage(dmg40);
	CHECK(unitYes->getBodyModule()->getHealth() == 100.0f);
	std::vector<SelectionDecal> decalsTen = BuildSelectionDecals(*w.logic, nullptr, playerA, { unitYes->getID() }, settings, 0);

	// Unit with ShowHealthInSelectionDecal = No, damaged to 50%
	DamageInfo dmgNo50;
	dmgNo50.m_input.m_amount = 500.0f;
	dmgNo50.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	unitNo->attemptDamage(dmgNo50);
	CHECK(unitNo->getBodyModule()->getHealth() == 500.0f);
	std::vector<SelectionDecal> decalsNo = BuildSelectionDecals(*w.logic, nullptr, playerA, { unitNo->getID() }, settings, 0);

	// In RotWK 2.01, selection decals on ground are NOT affected by unit health or ShowHealthInSelectionDecal (stop S-7286):
	// Decal count and properties remain identical.
	CHECK(decalsFull.size() == decalsHalf.size());
	CHECK(decalsFull.size() == decalsTen.size());
	CHECK(decalsFull.size() == decalsNo.size());

	if (!decalsFull.empty() && !decalsHalf.empty())
	{
		CHECK(decalsFull[0].size == decalsHalf[0].size);
		CHECK(decalsFull[0].color == decalsHalf[0].color);
		CHECK(decalsFull[0].style == decalsHalf[0].style);
		CHECK(decalsFull[0].texture == decalsHalf[0].texture);
		CHECK(decalsFull[0].texture2 == decalsHalf[0].texture2);
	}
}
