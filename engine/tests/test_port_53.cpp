// OpenBFME. GPL-3.0. PORT-53 synthetic PreAttackFX call-site tests; no game files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"

#include <string>
#include <vector>

namespace
{
struct Recorder : FXEventSink
{
	std::vector<FXEvent> events;
	void onFXEvent(const FXEvent &e) override { if (std::string(e.site) == "PreAttackFX") events.push_back(e); }
};

struct Port53World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *saved = TheSpecialPowerStore;
	Port53World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		combat().setAutoAcquireEnabled(false);
		const std::string ini = R"ini(
SpecialPower Port53Power
 Enum = SPECIAL_WORD_OF_POWER
 ReloadTime = 5000
End
Weapon Port53Weapon
 AttackRange = 180
 WeaponSpeed = 100
 PreAttackDelay = 1000
 PreAttackType = PER_SHOT
 PreAttackFX = FX_Port53
End
Weapon Port53NoFX
 AttackRange = 180
 PreAttackDelay = 1000
 PreAttackType = PER_SHOT
End
Weapon Port53NoDelay
 AttackRange = 180
 PreAttackFX = FX_Port53
End
)ini";
		std::string data = ini;
		for (const char *weapon : {"Port53Weapon", "Port53NoFX", "Port53NoDelay"})
		{
			for (const char *enabled : {"Yes", "No", "Missing"})
			{
				const bool missing = std::string(enabled) == "Missing";
				data += std::string("Object ") + weapon + enabled + R"ini(
 KindOf = HERO INFANTRY SELECTABLE CAN_ATTACK
 Geometry = CYLINDER
 GeometryMajorRadius = 8
 GeometryHeight = 20
 WeaponSet
  Conditions = None
  Weapon = PRIMARY )ini" + weapon + R"ini(
 End
 Body = ActiveBody BodyTag
  MaxHealth = 200
 End
 Behavior = AIUpdateInterface AiTag
 End
 Behavior = SpecialPowerModule PowerTag
  SpecialPowerTemplate = Port53Power
  UpdateModuleStartsAttack = Yes
 End
 Behavior = WeaponFireSpecialAbilityUpdate AbilityTag
  SpecialPowerTemplate = Port53Power
  SpecialWeapon = )ini" + (missing ? "Port53UnknownWeapon" : weapon) + R"ini(
  WhichSpecialWeapon = 1
  StartAbilityRange = 160
  UnpackTime = 200
  PackTime = 200
  IgnoreFacingCheck = Yes
  PlayWeaponPreFireFX = )ini" + (missing ? "Yes" : enabled) + R"ini(
 End
End
)ini";
			}
		}
		const std::string err = w.load(data);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port53World() { TheSpecialPowerStore = saved; }
};

void checkEvent(const FXEvent &e, const Object &owner, const WeaponTemplate &weapon)
{
	CHECK(e.kind == FXEvent::POSITION_FX);
	REQUIRE(e.fxList != nullptr);
	CHECK(e.fxList == &weapon.m_preAttackFXs[0]); // RW reads +0xB8, the REGULAR entry
	CHECK(*e.fxList == "FX_Port53");
	CHECK(e.primary == owner.getID());
	CHECK(e.drawablePosition);
	CHECK(e.drawableMatrix);
	CHECK(e.hasSecondaryPosition);
	CHECK(e.weaponSpeed == weapon.m_weaponSpeed);
	CHECK(e.weaponSpeed == 20.0f);
}
}

TEST_CASE("PORT-53 normal pre-fire emits one drawable positional event only for a positive delay and FX")
{
	Port53World f;
	Recorder rec;
	f.logic->fxEvents().setSink(&rec);
	Object *owner = f.unit("Port53WeaponYes", 'A', 300, 300);
	Object *target = f.unit("Dummy", 'B', 312, 300);
	f.frames(2);
	REQUIRE(owner->getWeapons());
	Weapon *weapon = owner->getWeapons()->weaponInSlot(PRIMARY_WEAPON);
	REQUIRE(weapon);
	const WeaponTemplate &tmpl = *weapon->getTemplate();
	for (const auto &fx : tmpl.m_preAttackFXs) CHECK(fx == "FX_Port53"); // RW 0x6C9CDC fills all four
	owner->getWeapons()->preFireCurrentWeapon(target, nullptr);
	REQUIRE(rec.events.size() == 1);
	checkEvent(rec.events[0], *owner, tmpl);
	for (const char *name : {"Port53NoFXYes", "Port53NoDelayYes"})
	{
		Object *other = f.unit(name, 'A', 330, 300);
		REQUIRE(other->getWeapons());
		other->getWeapons()->preFireCurrentWeapon(target, nullptr);
		CHECK(rec.events.size() == 1);
	}
}

TEST_CASE("PORT-53 special unpack requires the flag and weapon FX and uses the caster drawable")
{
	Port53World f;
	Recorder rec;
	f.logic->fxEvents().setSink(&rec);
	Object *target = f.unit("Dummy", 'B', 320, 300);
	const SpecialPowerTemplate *power = f.powers.findSpecialPowerTemplate("Port53Power");
	REQUIRE(power);
	for (const char *name : {"Port53WeaponYes", "Port53WeaponNo", "Port53NoFXYes", "Port53WeaponMissing"})
	{
		Object *caster = f.unit(name, 'A', 300, 300);
		auto *ability = dynamic_cast<WeaponFireSpecialAbilityUpdate *>(caster->findModule("WeaponFireSpecialAbilityUpdate"));
		REQUIRE(ability);
		if (std::string(name) == "Port53WeaponMissing") CHECK(ability->ownedWeapon() == nullptr);
		else REQUIRE(ability->ownedWeapon());
		const size_t before = rec.events.size();
		ability->initiateIntentToDoSpecialPower(power, target, nullptr, 0, 0);
		f.frames(3);
		if (std::string(name) == "Port53WeaponYes")
		{
			REQUIRE(rec.events.size() == before + 1);
			checkEvent(rec.events.back(), *caster, *ability->ownedWeapon()->getTemplate());
		}
		else CHECK(rec.events.size() == before);
	}
}

TEST_CASE("PORT-53 neighbouring POSITION_FX defaults remain unchanged")
{
	Port53World f;
	Object *owner = f.unit("Port53WeaponYes", 'A', 300, 300);
	const std::string name = "FX_Other";
	const FXEvent e = FXEventLog::objectEvent(FXEvent::POSITION_FX, "other", f.logic->getFrame(), name, *owner);
	CHECK_FALSE(e.drawablePosition);
	CHECK_FALSE(e.drawableMatrix);
	CHECK_FALSE(e.hasSecondaryPosition);
	CHECK(e.weaponSpeed == 0.0f);
}
