// OpenBFME. GPL-3.0.
//
// LOCAL-130 (fork issue 829): tests for RW 0x006760CA / RW 0x00677898 (construction model-condition override of
// damaged ambient sounds in LiveGameAudio::ambientSoundFor) and shared helper parity (RW 0x0068CC38 BitFlags::clear,
// RW 0x0081A76C BitFlags::operator==).
// Sources of expectations: RotWK 2.01 game.dat RW 0x006760CA (bits 0x43 AWAITING_CONSTRUCTION, 0x45 ACTIVELY_BEING_CONSTRUCTED),
// RW 0x00677898 (non-RUBBLE + construction flags -> SoundAmbient 0x25; else BodyDamageType 0..3), RW 0x00678578 (fallback).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "Common/BitFlags.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Object/Object.h"

TEST_CASE("LOCAL-130: RW 0x006760CA / 0x00677898 construction flags force pristine SoundAmbient except on RUBBLE")
{
	// Source: RotWK 2.01 RW 0x006760CA (bits 0x43, 0x45) and RW 0x00677898 (sel != 3 && 0x6760CA -> 0x676B5A).
	combattest::CombatWorld w;
	const std::string err = w.w.load(
		"Object Local130Tower\n"
		"  KindOf = STRUCTURE\n"
		"  SoundAmbient = TowerPristineLoop\n"
		"  SoundAmbientDamaged = TowerDamagedLoop\n"
		"  SoundAmbientReallyDamaged = TowerReallyDamagedLoop\n"
		"  SoundAmbientRubble = TowerRubbleLoop\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100.0\n"
		"    InitialHealth = 100.0\n"
		"    MaxHealthDamaged = 60.0\n"
		"    MaxHealthReallyDamaged = 25.0\n"
		"  End\n"
		"End\n"
		"Object Local130Fallback\n"
		"  KindOf = STRUCTURE\n"
		"  SoundAmbient = FallbackPristine\n"
		"  SoundAmbientDamaged = NoSound\n"
		"  SoundAmbientRubble = NoSound\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100.0\n"
		"    InitialHealth = 100.0\n"
		"    MaxHealthDamaged = 60.0\n"
		"    MaxHealthReallyDamaged = 25.0\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE,
		"local130_ambient.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *tower = w.unit("Local130Tower", 'A', 200.0f, 300.0f);
	REQUIRE(tower != nullptr);
	REQUIRE(tower->getBodyModule() != nullptr);

	// Pristine: SoundAmbient (RW 0x00676B5A)
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerPristineLoop");

	// Damage to BODY_DAMAGED (50 HP <= 60)
	DamageInfo dmg;
	dmg.m_input.m_amount = 50.0f;
	dmg.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	tower->attemptDamage(dmg);
	REQUIRE(tower->getBodyModule()->getDamageState() == BODY_DAMAGED);
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerDamagedLoop");

	// AWAITING_CONSTRUCTION (0x43) forces SoundAmbient (RW 0x006760CA -> RW 0x00677898)
	tower->setModelConditionState(0x43, true);
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerPristineLoop");
	tower->setModelConditionState(0x43, false);
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerDamagedLoop");

	// Damage to BODY_REALLYDAMAGED (20 HP <= 25) and test ACTIVELY_BEING_CONSTRUCTED (0x45)
	dmg.m_input.m_amount = 30.0f;
	tower->attemptDamage(dmg);
	REQUIRE(tower->getBodyModule()->getDamageState() == BODY_REALLYDAMAGED);
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerReallyDamagedLoop");
	tower->setModelConditionState(0x45, true);
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerPristineLoop");

	// Damage to BODY_RUBBLE (0 HP): RW 0x6778A0 skips 0x6760CA when sel == 3, so RUBBLE keeps SoundAmbientRubble
	dmg.m_input.m_amount = 30.0f;
	tower->attemptDamage(dmg);
	REQUIRE(tower->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(LiveGameAudio::ambientSoundFor(*tower) == "TowerRubbleLoop");

	// Neighbouring behaviour: NoSound (RW 0x006750FA) falls back to SoundAmbient on DAMAGED (RW 0x00678578) but stays empty on RUBBLE
	Object *fb = w.unit("Local130Fallback", 'A', 220.0f, 300.0f);
	REQUIRE(fb != nullptr);
	dmg.m_input.m_amount = 50.0f;
	fb->attemptDamage(dmg);
	REQUIRE(fb->getBodyModule()->getDamageState() == BODY_DAMAGED);
	CHECK(LiveGameAudio::ambientSoundFor(*fb) == "FallbackPristine");
	dmg.m_input.m_amount = 60.0f;
	fb->attemptDamage(dmg);
	REQUIRE(fb->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(LiveGameAudio::ambientSoundFor(*fb).empty());
}

TEST_CASE("LOCAL-130: shared BitFlags::clear (RW 0x0068CC38) and operator== (RW 0x0081A76C)")
{
	// Source: RotWK 2.01 RW 0x0068CC38 (4-dword AND-NOT) and RW 0x0081A76C (16-byte memcmp == 0).
	BitFlags<128> a;
	BitFlags<128> b;
	a.set(3, true);
	a.set(67, true);
	a.set(125, true);
	b.set(67, true);
	CHECK(!(a == b));
	a.clear(b);
	CHECK(a.test(3));
	CHECK(!a.test(67));
	CHECK(a.test(125));
	BitFlags<128> expected;
	expected.set(3, true);
	expected.set(125, true);
	CHECK(a == expected);
}
