// OpenBFME. GPL-3.0.
// LOCAL-171 (fork issue 952):
//   - Binary-map audit of 449 uncited RotWK 2.01 objects / modules functions (RW 0x441A32 .. 0x692B7B).
//   - Differs port: SubObjectsUpgradeModuleData ctor (RW 0x65747E, 0x6574CC movss xmm0, [0xbd869c] = 0.5f)
//     initializes FadeTimeInSeconds (+0x168) to 0.5f and WaitBeforeFadeInSeconds (+0x16C) to 0.0f.

#include "doctest.h"
#include "LogicTestUtil.h"

#include "Common/Module.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Module/UpgradeModuleClasses.h"

namespace
{
const SubObjectsUpgradeModuleData *findSubObjectsData(logictest::LogicWorld &lw, const char *templateName)
{
	const ThingTemplate *tt = lw.w.things.findTemplate(templateName);
	if (!tt)
		return nullptr;
	for (const ThingTemplate::Nugget &n : tt->moduleList(MODULETYPE_BEHAVIOR).nuggets())
	{
		if (const auto *d = dynamic_cast<const SubObjectsUpgradeModuleData *>(n.data.get()))
			return d;
	}
	return nullptr;
}
} // namespace

TEST_CASE("LOCAL-171 SubObjectsUpgradeModuleData: default FadeTimeInSeconds is 0.5f (RW 0x65747E)")
{
	// Expectations from RW 0x65747E (SubObjectsUpgradeModuleData::SubObjectsUpgradeModuleData):
	//   0x6574CC movss xmm0, [0xbd869c] (= 0.5f) -> [esi+0x168] m_fadeTimeInSeconds
	//   0x6574DE xorps xmm0, xmm0 (= 0.0f)       -> [esi+0x16c] m_waitBeforeFadeInSeconds
	//   0x6574E1..0x6574F3                       -> [esi+0x170..0x173] bool flags = false
	SubObjectsUpgradeModuleData direct;
	CHECK(direct.m_fadeTimeInSeconds == 0.5f);
	CHECK(direct.m_waitBeforeFadeInSeconds == 0.0f);
	CHECK(!direct.m_recolorHouse);
	CHECK(!direct.m_skipFadeOnCreate);
	CHECK(!direct.m_hideSubObjectsOnRemove);
	CHECK(!direct.m_unHideSubObjectsOnRemove);

	// Parsing a synthetic Object with SubObjectsUpgrade omitting FadeTimeInSeconds keeps the 0.5f default.
	logictest::LogicWorld lw;
	UpgradeModuleClasses::registerAll(lw.w.modules);
	const std::string err = lw.w.load(
		"Object Local171SubObjDefault\n"
		"  Side = Men\n"
		"  Behavior = SubObjectsUpgrade ModuleTag_SO\n"
		"    ShowSubObjects = Forged_Blade\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE, "test_local_171_default.ini");
	REQUIRE(err.empty());
	const SubObjectsUpgradeModuleData *data = findSubObjectsData(lw, "Local171SubObjDefault");
	REQUIRE(data != nullptr);
	CHECK(data->m_fadeTimeInSeconds == 0.5f);
	CHECK(data->m_waitBeforeFadeInSeconds == 0.0f);
	REQUIRE(data->m_showSubObjects.size() == 1);
	CHECK(data->m_showSubObjects[0] == "Forged_Blade");
}

TEST_CASE("LOCAL-171 SubObjectsUpgradeModuleData: explicit FadeTimeInSeconds overrides default (RW 0x65747E)")
{
	// Expectations from RW 0x65747E + field table RW 0xC6E928 (FadeTimeInSeconds +0x168, WaitBeforeFadeInSeconds +0x16C).
	logictest::LogicWorld lw;
	UpgradeModuleClasses::registerAll(lw.w.modules);
	const std::string err = lw.w.load(
		"Object Local171SubObjZero\n"
		"  Side = Men\n"
		"  Behavior = SubObjectsUpgrade ModuleTag_SOZero\n"
		"    FadeTimeInSeconds = 0.0\n"
		"    SkipFadeOnCreate = Yes\n"
		"  End\n"
		"End\n"
		"Object Local171SubObjCustom\n"
		"  Side = Men\n"
		"  Behavior = SubObjectsUpgrade ModuleTag_SOCustom\n"
		"    FadeTimeInSeconds = 1.25\n"
		"    WaitBeforeFadeInSeconds = 0.25\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE, "test_local_171_explicit.ini");
	REQUIRE(err.empty());

	const SubObjectsUpgradeModuleData *zeroFade = findSubObjectsData(lw, "Local171SubObjZero");
	REQUIRE(zeroFade != nullptr);
	CHECK(zeroFade->m_fadeTimeInSeconds == 0.0f);
	CHECK(zeroFade->m_skipFadeOnCreate);

	const SubObjectsUpgradeModuleData *customFade = findSubObjectsData(lw, "Local171SubObjCustom");
	REQUIRE(customFade != nullptr);
	CHECK(customFade->m_fadeTimeInSeconds == 1.25f);
	CHECK(customFade->m_waitBeforeFadeInSeconds == 0.25f);
}

TEST_CASE("LOCAL-171 ModuleFactory: cited RotWK modules are registered by init (RW 0x6579C9, RW 0x65644C)")
{
	// Expectations from RW 0x6579C9 (ModuleFactory::init) and RW 0x65644C (ModuleFactory::findModuleTemplate).
	logictest::LogicWorld lw;
	const char *const citedModules[] = {
		"SubObjectsUpgrade",
		"RunOffMapBehavior",
		"CommandSetUpgrade",
		"VeterancyCrateCollide",
		"DeployStyleAIUpdate",
		"OpenContain",
		"AttributeModifierPoolUpdate",
		"SlavedUpdate",
		"StructureToppleUpdate",
		"SpawnBehavior",
	};
	for (const char *name : citedModules)
	{
		CAPTURE(name);
		CHECK(lw.w.modules.findModuleTemplate(name, MODULETYPE_BEHAVIOR) != nullptr);
	}
}
