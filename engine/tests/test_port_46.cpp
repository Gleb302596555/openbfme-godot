// PORT-46 (issue #150): ToggleMountedSpecialAbilityUpdate / OpacityTarget — the drawable fade while mounting and dismounting.
// Synthetic INI, no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL. Expected values come from RW 0x8B1142 (the PC24 curve),
// RW 0x8B1219 / 0x8B125F (the resets) and Ennorath 1.9.8.2's OpacityTarget = .3 on its mounted heroes.
// GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// The synthetic world's special-power store: SpecialPower blocks parse into it, and the two module families register.
struct PowerWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	explicit PowerWorld(const char *objects)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~PowerWorld()
	{
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}
};

struct DetachClientHooks
{
	GameLogic &logic;
	~DetachClientHooks() { logic.setClientHooks(nullptr); }
};

SpecialAbilityUpdate *abilityOf(Object &o, const char *power)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		SpecialAbilityUpdate *u = dynamic_cast<SpecialAbilityUpdate *>(m.get());
		if (u && u->abilityData()->m_specialPowerTemplate && u->abilityData()->m_specialPowerTemplate->getName() == power)
		{
			return u;
		}
	}
	return nullptr;
}

std::vector<float> takeOpacities(ClientEventRecorder &events, ObjectID id, PowerWorld &f, int frames)
{
	std::vector<float> out;
	for (int i = 0; i < frames; ++i)
	{
		f.frames(1);
		for (const ClientEvent &e : events.take())
		{
			if (e.kind == ClientEvent::SET_OPACITY && e.object == id)
			{
				out.push_back(e.opacity);
			}
		}
	}
	return out;
}

std::string join(const std::vector<float> &v)
{
	std::ostringstream s;
	for (float x : v)
	{
		s << x << ' ';
	}
	return s.str();
}

const char kMountToggle[] =
	"SpecialPower SpecialAbilityTestMount\n  Enum = SPECIAL_TOGGLE_MOUNTED\n  ReloadTime = 1000\nEnd\n"
	"Object MountRider\n"
	" KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule MountPowerTag\n"
	"  SpecialPowerTemplate = SpecialAbilityTestMount\n  UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = ToggleMountedSpecialAbilityUpdate MountTag\n"
	"  SpecialPowerTemplate = SpecialAbilityTestMount\n"
	"  UnpackTime = 1000\n  PreparationTime = 1\n  PersistentPrepTime = 250\n  PackTime = 1000\n"
	"  OpacityTarget = .3\n  IgnoreFacingCheck = Yes\n End\n"
	"End\n";

const char kSummonReplacement[] =
	"SpecialPower SpecialAbilityTestSummonToggle\n  Enum = SPECIAL_TOGGLE_MOUNTED\n  ReloadTime = 1000\nEnd\n"
	"Object TestSummonToggle\n"
	" KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule SummonPowerTag\n"
	"  SpecialPowerTemplate = SpecialAbilityTestSummonToggle\n  UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = SummonReplacementSpecialAbilityUpdate SummonTag\n"
	"  SpecialPowerTemplate = SpecialAbilityTestSummonToggle\n"
	"  UnpackTime = 1000\n  PreparationTime = 1\n  PersistentPrepTime = 250\n  PackTime = 1000\n"
	"  OpacityTarget = .25\n  IgnoreFacingCheck = Yes\n End\n"
	"End\n";

const char kSpecialDisguise[] =
	"SpecialPower SpecialAbilityTestDisguise46\n  Enum = SPECIAL_DISGUISE\n  ReloadTime = 1000\nEnd\n"
	"Object DisguiseRider46\n"
	" KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule DisguisePowerTag\n"
	"  SpecialPowerTemplate = SpecialAbilityTestDisguise46\n  UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = SpecialDisguiseUpdate DisguiseTag\n"
	"  SpecialPowerTemplate = SpecialAbilityTestDisguise46\n"
	"  UnpackTime = 1000\n  PreparationTime = 1\n  PersistentPrepTime = 250\n  PackTime = 1000\n"
	"  OpacityTarget = .3\n  IgnoreFacingCheck = Yes\n End\n"
	"End\n";
} // namespace

// RW 0x8B1142: while the base RW 0x851E9F has frames left, UNPACKING writes 1.0 -> .3 and PACKING .3 -> 1.0,
// q = 1 - framesLeft / total under PC24. UnpackTime / PackTime 1000 ms = 5 logic frames, so the four busy
// unpack frames are .86, .72, .58, .44, the four busy pack frames .44, .58, .72, .86; the last unpack frame
// writes nothing (RW 0x8B1155). The last pack frame writes 1.0 three times: finishAbility (RW 0x8B1219,
// called by the base finishAfterPacking RW 0x8549E7), finishAfterPacking (RW 0x8B125F) and the fade with
// framesLeft 0. The exact event count is the "one action — one reaction" check (PROCESS.md 15.2).
TEST_CASE("PORT-46 ToggleMountedSpecialAbilityUpdate fades the drawable to .3 and back (RW 0x8B1142)")
{
	PowerWorld f(kMountToggle);
	ClientEventRecorder events(*f.logic);
	f.logic->setClientHooks(&events);
	DetachClientHooks detach{ *f.logic };
	Object *rider = f.unit("MountRider", 'A', 300.0f, 300.0f);
	REQUIRE(rider);
	auto *tm = dynamic_cast<ToggleMountedSpecialAbilityUpdate *>(abilityOf(*rider, "SpecialAbilityTestMount"));
	REQUIRE(tm);
	events.take(); // the object's CREATED / MODEL_FLUSH events
	tm->initiateIntentToDoSpecialPower(tm->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
	const std::vector<float> got = takeOpacities(events, rider->getID(), f, 30);
	INFO("SET_OPACITY: ", join(got));
	// .3 is Ennorath's mounted-hero value; the sequence is RW 0x8B1164 .. 0x8B11F0, not the new code's result.
	const std::vector<float> expected = { 0.86f, 0.72f, 0.58f, 0.44f, 0.44f, 0.58f, 0.72f, 0.86f, 1.0f, 1.0f, 1.0f };
	REQUIRE(got.size() == expected.size());
	for (size_t i = 0; i < expected.size(); ++i)
	{
		CHECK(got[i] == expected[i]);
	}
	CHECK(tm->toggles() == 1); // the mount toggle still runs (RW 0x8B1690)
}

// RW 0xC05B48 (table 0xC05AF8) parses SummonReplacementSpecialAbilityUpdate/OpacityTarget to data + 0xE0, but
// vtable 0xC6C028 slot 0x50 is the base RW 0x851E9F: no reader of + 0xE0 exists in RotWK 2.01 (data ctor
// RW 0x8B21DA only zeroes it at RW 0x8B2235). No fade and no SET_OPACITY event may appear. Neighbouring behaviour.
TEST_CASE("PORT-46 SummonReplacementSpecialAbilityUpdate's OpacityTarget is never read (RW 0x851E9F)")
{
	PowerWorld f(kSummonReplacement);
	ClientEventRecorder events(*f.logic);
	f.logic->setClientHooks(&events);
	DetachClientHooks detach{ *f.logic };
	Object *o = f.unit("TestSummonToggle", 'A', 300.0f, 300.0f);
	REQUIRE(o);
	auto *sr = dynamic_cast<SummonReplacementSpecialAbilityUpdate *>(abilityOf(*o, "SpecialAbilityTestSummonToggle"));
	REQUIRE(sr);
	events.take();
	sr->initiateIntentToDoSpecialPower(sr->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
	const std::vector<float> got = takeOpacities(events, o->getID(), f, 30);
	INFO("SET_OPACITY: ", join(got));
	CHECK(got.empty());
	CHECK(sr->summons() == 1); // the toggle itself runs (RW 0x8B2584)
}

// RW 0x8B4455 is the twin of 0x8B1142 with data + 0xD4; the port already computes it into module + 0x8C.
// PORT-46 only shares the helper, the SpecialDisguiseUpdate numbers must not shift. Neighbouring behaviour.
TEST_CASE("PORT-46 SpecialDisguiseUpdate's own fade is unchanged (RW 0x8B4455)")
{
	PowerWorld f(kSpecialDisguise);
	Object *o = f.unit("DisguiseRider46", 'A', 300.0f, 300.0f);
	REQUIRE(o);
	auto *sd = dynamic_cast<SpecialDisguiseUpdate *>(abilityOf(*o, "SpecialAbilityTestDisguise46"));
	REQUIRE(sd);
	sd->initiateIntentToDoSpecialPower(sd->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
	std::vector<float> fade;
	for (int i = 0; i < 30 && sd->isActive(); ++i)
	{
		f.frames(1);
		fade.push_back(sd->opacity());
	}
	INFO("opacity: ", join(fade));
	CHECK(std::find(fade.begin(), fade.end(), 0.86f) != fade.end()); // left 4 of the unpack
	CHECK(std::find(fade.begin(), fade.end(), 0.44f) != fade.end()); // left 4 of the pack
	CHECK(sd->opacity() == 1.0f); // RW 0x8B4521: back to 1.0 at the end
	CHECK(sd->disguises() == 1);
}
