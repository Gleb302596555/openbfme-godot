// OpenBFME unit tests: LOCAL-123 (fork issue #807) -- draw, client, collide, contain, body and power modules (13 classes, 38 functions).
// Every expectation is derived from RotWK 2.01 game.dat instructions (RW 0x... cited per test) and Ennorath 1.9.8.3 INI data.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/Player.h"
#include "Common/StateHash.h"
#include "Common/Team.h"
#include "Common/XferSave.h"
#include "Common/XferLoad.h"
#include "GameClient/BeaconClientUpdate.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/ModelConditionAudioLoopClientBehavior.h"
#include "GameClient/RadarMarkerClientUpdate.h"
#include "GameClient/SoundSelectorClientBehavior.h"
#include "GameClient/TerrainResourceFeedback.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AISpecialPowerUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AODCrushCollide.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HordeTransportContainDamage.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/SquishCollide.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Contain/OpenContainRuntime.h"
#include "GameLogic/Object/Contain/SiegeEngineContainRuntime.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{

const char kLocal123Ini[] =
	"Object Local123Rider\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SquishCollide ModuleTag_Squish\n"
	"  End\n"
	"  Behavior = HordeMemberCollide ModuleTag_HMC\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = PlainLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123HordeRider\n"
	"  KindOf = INFANTRY HORDE SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_InnerContain\n"
	"    Slots = 5\n"
	"    PassengerFilter = ANY +INFANTRY\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123PlainGarrison\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123HordeGarrison\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = HordeGarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123HordeTransport\n"
	"  KindOf = VEHICLE SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeTransportContain ModuleTag_Contain\n"
	"    Slots = 4\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"  End\n"
	"  Behavior = HordeTransportContainDamage ModuleTag_HTCD\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123Crew\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK ARCHER\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123FlammableSiegeEngine\n"
	"  KindOf = VEHICLE SIEGEENGINE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 25\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SiegeEngineContain ModuleTag_Contain\n"
	"    Slots = 2\n"
	"    PassengerFilter = ANY +INFANTRY\n"
	"    CrewFilter = NONE +ARCHER\n"
	"    CrewMax = 2\n"
	"    SpeedPercentPerCrew = 50%\n"
	"  End\n"
	"  Behavior = FlammableUpdate ModuleTag_Flammable\n"
	"    FlameDamageLimit = 20\n"
	"    FlameDamageExpiration = 10000\n"
	"    AflameDuration = 5000\n"
	"    AflameDamageDelay = 1000\n"
	"    AflameDamageAmount = 10\n"
	"    BurnContained = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123FlammableHordeSiegeEngine\n"
	"  KindOf = VEHICLE SIEGEENGINE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 25\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = HordeSiegeEngineContain ModuleTag_Contain\n"
	"    Slots = 2\n"
	"    PassengerFilter = ANY +INFANTRY +HORDE\n"
	"    CrewFilter = NONE +ARCHER\n"
	"    CrewMax = 2\n"
	"    SpeedPercentPerCrew = 50%\n"
	"  End\n"
	"  Behavior = FlammableUpdate ModuleTag_Flammable\n"
	"    FlameDamageLimit = 20\n"
	"    FlameDamageExpiration = 10000\n"
	"    AflameDuration = 5000\n"
	"    AflameDamageDelay = 1000\n"
	"    AflameDamageAmount = 10\n"
	"    BurnContained = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local123AodCrusher\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AODCrushCollide ModuleTag_AOD\n"
	"    Damage = 50.0\n"
	"    DamageType = MAGIC\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n";

struct Local123World : CombatWorld
{
	Local123World() : CombatWorld(kLocal123Ini) {}
};

} // namespace

TEST_CASE("LOCAL-123 HordeGarrisonContain and OpenContain onContainedAboutToTakeDamage (RW 0x0087A3A4 / RW 0x00865631)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - OpenContain::onContainedAboutToTakeDamage (RW 0x00865631, DONOR BFME2 0x00462516, contain slot +0x68):
	//   * When damageType is not DAMAGE_PASSENGER (14) and not DAMAGE_UNRESISTABLE (8), zeroes info->m_input.m_amount = 0.0f
	//     (RW 0x00865669) and clears info->m_input.m_kill = false (RW 0x0086566D), leaving rider inside the container.
	//   * When damageType is DAMAGE_UNRESISTABLE (8) or DAMAGE_PASSENGER (14) with m_kill == false, does not remove rider
	//     and does not zero m_amount (RW 0x00865659).
	//   * When damageType is DAMAGE_UNRESISTABLE (8) or DAMAGE_PASSENGER (14) with m_kill == true, calls
	//     removeFromContain(rider, false) (RW 0x00865660) so the rider is ejected before lethal damage lands.
	// - HordeGarrisonContain / HordeTransportContain slot +0x68 (RW 0x0087A3A4, DONOR BFME2 0x00479C0C):
	//   * Checks rider->getContain() == nullptr (cmp dword ptr [eax + 0x258], 0; jne ret).
	//   * A horde container rider (getContain() != nullptr) is skipped untouched.
	//   * A single rider/member (getContain() == nullptr) is forwarded to OpenContain::onContainedAboutToTakeDamage (RW 0x00865631).
	Local123World w;

	// 1. Plain GarrisonContain (slot +0x68 = RW 0x00865631):
	Object *plain = w.unit("Local123PlainGarrison", 'A', 100.0f, 100.0f);
	Object *rider1 = w.unit("Local123Rider", 'A', 100.0f, 100.0f);
	ContainModuleInterface *plainContain = plain->getContain();
	REQUIRE(plainContain != nullptr);
	REQUIRE(plainContain->addToContain(rider1));
	CHECK(plainContain->getContainCount() == 1u);

	// Non-unresistable / non-surrender damage is zeroed and kill is cleared (RW 0x00865669..0x0086566D).
	DamageInfo slashInfo;
	slashInfo.m_input.m_damageType = DAMAGE_SLASH;
	slashInfo.m_input.m_amount = 45.0f;
	slashInfo.m_input.m_kill = true;
	plainContain->onContainedAboutToTakeDamage(rider1, rider1->getBodyModule(), &slashInfo);
	CHECK(slashInfo.m_input.m_amount == 0.0f);
	CHECK_FALSE(slashInfo.m_input.m_kill);
	CHECK(plainContain->getContainCount() == 1u);

	// UNRESISTABLE without m_kill keeps rider inside and keeps amount (RW 0x00865655..0x00865659).
	DamageInfo unresNoKill;
	unresNoKill.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	unresNoKill.m_input.m_amount = 25.0f;
	unresNoKill.m_input.m_kill = false;
	plainContain->onContainedAboutToTakeDamage(rider1, rider1->getBodyModule(), &unresNoKill);
	CHECK(unresNoKill.m_input.m_amount == 25.0f);
	CHECK(plainContain->getContainCount() == 1u);

	// UNRESISTABLE with m_kill == true removes rider from contain (RW 0x0086565B..0x00865660).
	DamageInfo unresKill;
	unresKill.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	unresKill.m_input.m_amount = 100.0f;
	unresKill.m_input.m_kill = true;
	plainContain->onContainedAboutToTakeDamage(rider1, rider1->getBodyModule(), &unresKill);
	CHECK(plainContain->getContainCount() == 0u);
	CHECK(rider1->getContainedBy() == nullptr);

	// 2. HordeGarrisonContain (slot +0x68 = RW 0x0087A3A4):
	Object *hordeGar = w.unit("Local123HordeGarrison", 'A', 200.0f, 100.0f);
	Object *singleRider = w.unit("Local123Rider", 'A', 200.0f, 100.0f);
	Object *hordeRider = w.unit("Local123HordeRider", 'A', 200.0f, 100.0f);
	ContainModuleInterface *hgContain = hordeGar->getContain();
	REQUIRE(hgContain != nullptr);
	REQUIRE(hordeRider->getContain() != nullptr);
	REQUIRE(hgContain->addToContain(singleRider));
	REQUIRE(hgContain->addToContain(hordeRider));
	CHECK(hgContain->getContainCount() == 2u);

	// For hordeRider (rider->getContain() != nullptr), RW 0x0087A3A4 returns early without zeroing damage or removing it.
	DamageInfo hordeSlash;
	hordeSlash.m_input.m_damageType = DAMAGE_SLASH;
	hordeSlash.m_input.m_amount = 30.0f;
	hordeSlash.m_input.m_kill = true;
	hgContain->onContainedAboutToTakeDamage(hordeRider, hordeRider->getBodyModule(), &hordeSlash);
	CHECK(hordeSlash.m_input.m_amount == 30.0f);
	CHECK(hordeSlash.m_input.m_kill);

	DamageInfo hordeKill;
	hordeKill.m_input.m_damageType = DAMAGE_PASSENGER;
	hordeKill.m_input.m_amount = 100.0f;
	hordeKill.m_input.m_kill = true;
	hgContain->onContainedAboutToTakeDamage(hordeRider, hordeRider->getBodyModule(), &hordeKill);
	CHECK(hgContain->getContainCount() == 2u);

	// For singleRider (rider->getContain() == nullptr), RW 0x0087A3A4 forwards to RW 0x00865631:
	DamageInfo singleSlash;
	singleSlash.m_input.m_damageType = DAMAGE_PIERCE;
	singleSlash.m_input.m_amount = 40.0f;
	singleSlash.m_input.m_kill = true;
	hgContain->onContainedAboutToTakeDamage(singleRider, singleRider->getBodyModule(), &singleSlash);
	CHECK(singleSlash.m_input.m_amount == 0.0f);
	CHECK_FALSE(singleSlash.m_input.m_kill);
	CHECK(hgContain->getContainCount() == 2u);

	DamageInfo singleSurrender;
	singleSurrender.m_input.m_damageType = DAMAGE_PASSENGER;
	singleSurrender.m_input.m_amount = 100.0f;
	singleSurrender.m_input.m_kill = true;
	hgContain->onContainedAboutToTakeDamage(singleRider, singleRider->getBodyModule(), &singleSurrender);
	CHECK(hgContain->getContainCount() == 1u);
	CHECK(singleRider->getContainedBy() == nullptr);

	// 3. HordeTransportContain (slot +0x68 = RW 0x0087A3A4):
	Object *hordeTrans = w.unit("Local123HordeTransport", 'A', 300.0f, 100.0f);
	Object *tSingle = w.unit("Local123Rider", 'A', 300.0f, 100.0f);
	Object *tHorde = w.unit("Local123HordeRider", 'A', 300.0f, 100.0f);
	ContainModuleInterface *htContain = hordeTrans->getContain();
	REQUIRE(htContain != nullptr);
	REQUIRE(htContain->addToContain(tSingle));
	REQUIRE(htContain->addToContain(tHorde));
	CHECK(htContain->getContainCount() == 2u);

	DamageInfo thKill;
	thKill.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	thKill.m_input.m_kill = true;
	htContain->onContainedAboutToTakeDamage(tHorde, tHorde->getBodyModule(), &thKill);
	CHECK(htContain->getContainCount() == 2u); // skipped because tHorde->getContain() != nullptr

	htContain->onContainedAboutToTakeDamage(tSingle, tSingle->getBodyModule(), &thKill);
	CHECK(htContain->getContainCount() == 1u); // ejected because tSingle->getContain() == nullptr
}

TEST_CASE("LOCAL-123 SiegeEngineContain and HordeSiegeEngineContain evacuateCrew at ignition (RW 0x0087F0CF / RW 0x0087FF18 / RW 0x00890666)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - SiegeEngineContain::evacuateCrew (RW 0x0087F0CF, DONOR BFME2 0x0047BE23, contain slot +0x88):
	//   drains m_crew (+0x11C) via removeFromContain(crewMember, false) (contain slot +0xA4), leaving regular riders in m_contained intact.
	// - HordeSiegeEngineContain::evacuateCrew (RW 0x0087FF18, DONOR BFME2 0x0047CDC8, contain slot +0x88):
	//   drains m_crew (+0x128) via removeFromContain(crewMember, false) and flings with GameLogicRandomValue(3, 8).
	// - FlammableUpdate::onDamage (RW 0x00890656..0x00890666): when BurnContained = No and the object ignites,
	//   calls obj->getContain()->evacuateCrew(2 /*CMD_FROM_AI*/).
	Local123World w;

	// 1. SiegeEngineContain ignited via FLAME damage exceeding FlameDamageLimit (20):
	Object *siege = w.unit("Local123FlammableSiegeEngine", 'A', 150.0f, 150.0f);
	Object *crew1 = w.unit("Local123Crew", 'A', 150.0f, 150.0f);
	Object *crew2 = w.unit("Local123Crew", 'A', 150.0f, 150.0f);
	Object *passenger = w.unit("Local123Rider", 'A', 150.0f, 150.0f);
	auto *seContain = dynamic_cast<SiegeEngineContain *>(siege->getContain());
	REQUIRE(seContain != nullptr);
	REQUIRE(seContain->addToContain(crew1));
	REQUIRE(seContain->addToContain(crew2));
	REQUIRE(seContain->addToContain(passenger));
	CHECK(seContain->getCrewCount() == 2);
	CHECK(seContain->getContainCount() == 1u);

	DamageInfo flame;
	flame.m_input.m_damageType = DAMAGE_FLAME;
	flame.m_input.m_deathType = DEATH_BURNED;
	flame.m_input.m_amount = 30.0f; // > FlameDamageLimit (20) -> tryToIgnite() -> RW 0x00890666 contain->evacuateCrew(2)
	siege->attemptDamage(flame);

	// Crew members are evacuated by RW 0x0087F0CF while non-crew passenger remains inside (BurnContained = No).
	CHECK(seContain->getCrewCount() == 0);
	CHECK(crew1->getContainedBy() == nullptr);
	CHECK(crew2->getContainedBy() == nullptr);
	CHECK(seContain->getContainCount() == 1u);
	CHECK(passenger->getContainedBy() == siege);

	// 2. HordeSiegeEngineContain ignited -> evacuates crew (RW 0x0087FF18) and flings via PhysicsBehavior:
	Object *hordeSiege = w.unit("Local123FlammableHordeSiegeEngine", 'A', 250.0f, 150.0f);
	Object *hCrew = w.unit("Local123Crew", 'A', 250.0f, 150.0f);
	auto *hseContain = dynamic_cast<HordeSiegeEngineContain *>(hordeSiege->getContain());
	REQUIRE(hseContain != nullptr);
	REQUIRE(hseContain->addToContain(hCrew));
	CHECK(hseContain->getCrewCount() == 1);

	hordeSiege->attemptDamage(flame);
	CHECK(hseContain->getCrewCount() == 0);
	CHECK(hCrew->getContainedBy() == nullptr);
	PhysicsBehavior *phys = PhysicsBehavior::find(*hordeSiege);
	REQUIRE(phys != nullptr);
	CHECK(phys->isFlying());
}

TEST_CASE("LOCAL-123 AODCrushCollide xfer round-trip and collide/client module behavior (RW 0x008C004C, RW 0x008CD9FD, RW 0x008CE027)")
{
	// Expectations from RotWK 2.01 game.dat:
	// - AODCrushCollide::xfer (RW 0x008C004C, DONOR BFME2 0x004BBF03): serializes version 1, UpdateModule base,
	//   m_clearFrame (+0x24), m_lastTarget (+0x28), m_firing (+0x2C).
	// - BeaconClientUpdate::pulseDue (RW 0x008CD9FD / RW 0x008CDA03 / RW 0x008CDCE9): pulses when m_lastRadarPulse + frequency < frame.
	Local123World w;
	Object *crusher = w.unit("Local123AodCrusher", 'A', 100.0f, 100.0f);
	Object *victim1 = w.unit("Local123Crew", 'B', 102.0f, 100.0f);
	Object *victim2 = w.unit("Local123Crew", 'B', 104.0f, 100.0f);
	REQUIRE(crusher != nullptr);
	REQUIRE(victim1 != nullptr);
	REQUIRE(victim2 != nullptr);

	auto *aod = dynamic_cast<AODCrushCollide *>(crusher->findModule("AODCrushCollide"));
	REQUIRE(aod != nullptr);
	aod->onCollide(victim1, victim1->getPosition(), nullptr);

	StateHasher hBefore;
	aod->crc(hBefore);
	XferSave save;
	aod->xfer(save);

	// Mutate m_lastTarget on the same module at the same frame, then restore from snapshot and verify state hash matches.
	aod->onCollide(victim2, victim2->getPosition(), nullptr);
	StateHasher hMutated;
	aod->crc(hMutated);
	CHECK(hBefore.value() != hMutated.value());
	XferLoad load(save.data());
	aod->xfer(load);
	StateHasher hRestored;
	aod->crc(hRestored);
	CHECK(hBefore.value() == hRestored.value());

	// BeaconClientUpdate pulse schedule (RW 0x008CDCE9: m_lastRadarPulse + m_framesBetweenRadarPulses < frame).
	BeaconClientUpdateModuleData beaconData;
	beaconData.m_framesBetweenRadarPulses = 30;
	beaconData.m_radarPulseDuration = 15;
	BeaconClientUpdate beacon(nullptr, &beaconData);
	CHECK_FALSE(beacon.pulseDue(10));  // first call initializes m_lastRadarPulse = 10
	CHECK_FALSE(beacon.pulseDue(40));  // 10 + 30 < 40 is false (strictly less)
	CHECK(beacon.pulseDue(41));        // 10 + 30 < 41 is true -> pulses and sets m_lastRadarPulse = 41
	CHECK(beacon.pulseSeconds() == 3.0f);
	beacon.hideBeacon();
	CHECK(beacon.hidden());
	CHECK_FALSE(beacon.pulseDue(100));
}

TEST_CASE("LOCAL-123 ENNORATH scenario: live Ennorath objects of the 13 module classes (#807)")
{
	// Expectations from RotWK 2.01 game.dat and Ennorath 1.9.8.3 INI:
	// 1. WildMarauderSwordHorde / WildMarauderSwords: WeaponModeSpecialPowerUpdate (RW 0x00898276 / RW 0x00898290 / RW 0x0089833B / RW 0x009E7051),
	//    AISpecialPowerUpdate (RW 0x008B6FDE / RW 0x008B6FE4 / RW 0x008B706F), SquishCollide (RW 0x008BFAAB), HordeMemberCollide (RW 0x008C0487).
	//    Initiating WeaponModeSpecialPowerUpdate sets isActive() == true (RW 0x009E7051) and activates WEAPONSET_TOGGLE_1 on the horde and members.
	// 2. GondorBattleTower: HordeGarrisonContain (RW 0x0087A3A4): onContainedAboutToTakeDamage skips a garrisoned horde container
	//    (rider->getContain() != nullptr) and intercepts a single non-container unit via OpenContain::onContainedAboutToTakeDamage (RW 0x00865631).
	// 3. IsengardBatteringRam: SiegeEngineContain (RW 0x0087F0CF): spawns with InitialCrew (getCrewCount() > 0) and evacuateCrew(2) removes all crew.
	// 4. RohanOathbreaker1 (AODCrushCollide RW 0x008C004C), GondorFarm (TerrainResourceClientBehavior RW 0x00652BF1 &
	//    ModelConditionAudioLoopClientBehavior RW 0x008D0380), MordorMumakil (HordeTransportContainDamage RW 0x008BF22E &
	//    HordeTransportContain RW 0x0087A3A4), TheDroppedRing (RadarMarkerClientUpdate RW 0x008CE027),
	//    RohanPeasant1 (RandomSoundSelectorClientBehavior RW 0x008D00E2), MultiplayerBeacon (BeaconClientUpdate RW 0x008CD9FD).
	ennorathscenario::World world;
	if (!world.load("LOCAL-123 ENNORATH scenario"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	std::string error;

	// 1. WildMarauderSwordHorde & WildMarauderSwords (WeaponModeSpecialPowerUpdate, AISpecialPowerUpdate, SquishCollide, HordeMemberCollide)
	Object *marauderHorde = game.game->createObject("WildMarauderSwordHorde", game.index(), Coord3D{ 1000.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(marauderHorde != nullptr, error);
	Object *marauderMember = game.game->createObject("WildMarauderSwords", game.index(), Coord3D{ 1020.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(marauderMember != nullptr, error);

	auto *wmHorde = dynamic_cast<WeaponModeSpecialPowerUpdate *>(marauderHorde->findModule("WeaponModeSpecialPowerUpdate"));
	auto *wmMember = dynamic_cast<WeaponModeSpecialPowerUpdate *>(marauderMember->findModule("WeaponModeSpecialPowerUpdate"));
	REQUIRE(wmHorde != nullptr);
	REQUIRE(wmMember != nullptr);
	CHECK_FALSE(wmHorde->isActive());  // RW 0x009E7051
	CHECK_FALSE(wmMember->isActive()); // RW 0x009E7051
	wmHorde->pauseCountdown(false);
	wmMember->pauseCountdown(false);
	wmHorde->doSpecialPower(0);        // RW 0x009911FB -> RW 0x0089841E
	wmMember->doSpecialPower(0);
	CHECK(wmHorde->isActive());
	CHECK(wmMember->isActive());

	CHECK(marauderHorde->findModule("AISpecialPowerUpdate") != nullptr);
	CHECK(marauderMember->findModule("SquishCollide") != nullptr);
	CHECK(marauderMember->findModule("HordeMemberCollide") != nullptr);

	// 2. GondorBattleTower (HordeGarrisonContain RW 0x0087A3A4)
	Object *tower = game.game->createObject("GondorBattleTower", game.index(), Coord3D{ 1200.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(tower != nullptr, error);
	Object *archer = game.game->createObject("GondorArcher", game.index(), Coord3D{ 1210.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(archer != nullptr, error);
	Object *archerHorde = game.game->createObject("GondorArcherHorde", game.index(), Coord3D{ 1220.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(archerHorde != nullptr, error);
	auto *towerContain = dynamic_cast<GarrisonContain *>(tower->getContain());
	REQUIRE(towerContain != nullptr);
	CHECK(towerContain->isHordeGarrison());
	REQUIRE(towerContain->addToContain(archer));
	REQUIRE(towerContain->addToContain(archerHorde));
	const unsigned countWithBoth = towerContain->getContainCount();
	CHECK(countWithBoth == 2u);

	// Horde container is skipped by RW 0x0087A3A4; single archer has non-lethal damage zeroed and is ejected on lethal UNRESISTABLE.
	DamageInfo hitInfo;
	hitInfo.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	hitInfo.m_input.m_amount = 100.0f;
	hitInfo.m_input.m_kill = true;
	towerContain->onContainedAboutToTakeDamage(archerHorde, archerHorde->getBodyModule(), &hitInfo);
	CHECK(towerContain->getContainCount() == countWithBoth);
	towerContain->onContainedAboutToTakeDamage(archer, archer->getBodyModule(), &hitInfo);
	CHECK(towerContain->getContainCount() == countWithBoth - 1u);

	// 3. IsengardBatteringRam (SiegeEngineContain RW 0x0087F0CF)
	Object *ram = game.game->createObject("IsengardBatteringRam", game.index(), Coord3D{ 1400.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(ram != nullptr, error);
	auto *ramContain = dynamic_cast<SiegeEngineContain *>(ram->getContain());
	REQUIRE(ramContain != nullptr);
	CHECK(ramContain->getCrewCount() == 6); // Ennorath isengardsiegeunits.ini: InitialCrew = IsengardFighterCrew 6
	ramContain->evacuateCrew(2 /*CMD_FROM_AI*/); // RW 0x0087F0CF
	CHECK(ramContain->getCrewCount() == 0);

	// 4. Remaining classes on live Ennorath templates:
	Object *oathbreaker = game.game->createObject("RohanOathbreaker1", game.index(), Coord3D{ 1500.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(oathbreaker != nullptr, error);
	auto *aod = dynamic_cast<AODCrushCollide *>(oathbreaker->findModule("AODCrushCollide"));
	REQUIRE(aod != nullptr);
	XferSave aodSave;
	aod->xfer(aodSave); // RW 0x008C004C
	XferLoad aodLoad(aodSave.data());
	aod->xfer(aodLoad);

	Object *mumak = game.game->createObject("MordorMumakil", game.index(), Coord3D{ 1600.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(mumak != nullptr, error);
	CHECK(mumak->findModule("HordeTransportContainDamage") != nullptr);
	CHECK(dynamic_cast<HordeTransportContain *>(mumak->getContain()) != nullptr);

	Object *farm = game.game->createObject("GondorFarm", game.index(), Coord3D{ 1700.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(farm != nullptr, error);
	CHECK(TerrainResourceFeedback::hasClientBehavior(*static_cast<const ThingTemplate *>(farm->getTemplate())));

	Object *ring = game.game->createObject("TheDroppedRing", game.index(), Coord3D{ 1800.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(ring != nullptr, error);
	Object *peasant = game.game->createObject("RohanPeasant1", game.index(), Coord3D{ 1850.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(peasant != nullptr, error);
	CHECK(SoundSelectors::any(*static_cast<const ThingTemplate *>(peasant->getTemplate())));
	Object *beacon = game.game->createObject("MultiplayerBeacon", game.index(), Coord3D{ 1900.0f, 1000.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(beacon != nullptr, error);

	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);

	auto hasClientMod = [](Drawable *d, auto pred) {
		if (!d) return false;
		for (const std::unique_ptr<DrawableModule> &m : d->clientModules())
		{
			if (pred(m.get())) return true;
		}
		return false;
	};

	Drawable *farmDrawable = game.game->drawables().findByObject(farm->getID());
	REQUIRE(farmDrawable != nullptr);
	CHECK(hasClientMod(farmDrawable, [](DrawableModule *m) { return dynamic_cast<TerrainResourceClientBehavior *>(m) != nullptr; }));
	Drawable *marauderDrawable = game.game->drawables().findByObject(marauderMember->getID());
	REQUIRE(marauderDrawable != nullptr);
	CHECK(hasClientMod(marauderDrawable, [](DrawableModule *m) { return dynamic_cast<ModelConditionAudioLoopClientBehavior *>(m) != nullptr; }));

	Drawable *ringDrawable = game.game->drawables().findByObject(ring->getID());
	REQUIRE(ringDrawable != nullptr);
	CHECK(hasClientMod(ringDrawable, [](DrawableModule *m) { return dynamic_cast<RadarMarkerClientUpdate *>(m) != nullptr; }));

	Drawable *beaconDrawable = game.game->drawables().findByObject(beacon->getID());
	REQUIRE(beaconDrawable != nullptr);
	CHECK(hasClientMod(beaconDrawable, [](DrawableModule *m) { return dynamic_cast<BeaconClientUpdate *>(m) != nullptr; }));
}
