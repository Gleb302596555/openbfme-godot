// OpenBFME unit tests. PORT-146 (issue #363): the archers on a moving siege carrier (Grond, P-82).
//
// The scene is synthetic (no game files): a siege carrier with a SiegeEngineContain, its passengers standing on the bones their
// PassengerBonePrefix names (the client's bone provider is a stub, as in test_port_22.cpp), an archer of the owner on one bone and an enemy in the
// archer's reach. The carrier is then ordered to move and every frame prints where the archers are and whether they fired.
//
// This file starts as a reproduction: the printed lines are the observation, the checks come once the divergence is known.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Contain/OpenContainRuntime.h"
#include "GameLogic/Object/Contain/SiegeEngineContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

using namespace combattest;

namespace
{
// MordorGrond's shape as the mod has it (object/evilfaction/units/mordor/grond.ini): Slots = 6, PassengerFilter = NONE +MordorArcherForGrond,
// ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE CAN_ATTACK, PassengerBonePrefix = PassengerBone:B_ARCHER KindOf:NOT_AUTOACQUIRABLE. The archer is a
// carrier's passenger whose AI keeps looking for enemies while it is inside (CanAttackWhileContained, the AIUpdate field RW field table 0xC0F530 + 0x25).
const char kPort146Objects[] =
	"Object P146Carrier\n"
	"  KindOf = SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 200\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = SiegeEngineContain ModuleTag_Siege\n"
	"    Slots = 2\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE CAN_ATTACK\n"
	"    PassengerBonePrefix = PassengerBone:B_ARCHER KindOf:INFANTRY\n"
	"    ForceOrientationContainer = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 30\n"
	"  End\n"
	"End\n"
	"Object P146Still\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"End\n"
	"Object P146Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 250\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = Yes\n"
	"    CanAttackWhileContained = Yes\n"
	"    MoodAttackCheckRate = 400\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

// the client's bone provider for the carrier: two bones of the prefix "B_ARCHER", 30 above the carrier
class CarrierBones : public ProjectileLaunchOffsets
{
public:
	bool launchOffset(const Object &, int, int, float[12]) override { return false; }
	int multiLogicalBones(const Object &, const std::string &prefix, const std::array<std::uint32_t, 19> &, int maxBones, float (*out)[12]) override
	{
		++asked;
		if (prefix != "B_ARCHER" || maxBones < 2)
		{
			return 0;
		}
		for (int i = 0; i < 2; ++i)
		{
			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 4; ++c)
				{
					out[i][r * 4 + c] = r == c ? 1.0f : 0.0f;
				}
			}
			out[i][3] = 0.0f;
			out[i][7] = i == 0 ? -8.0f : 8.0f; // the carrier's row: the archers stand side by side
			out[i][11] = 30.0f;                // and 30 above it
		}
		return 2;
	}
	int asked = 0;
};

void printConditions(const char *who, const Object &o)
{
	std::string s;
	for (const char *n : { "MOVING", "TRANSPORT_MOVING", "PASSENGER", "ATTACKING", "FIRING_A", "NO_ATTACK", "PASSENGER_VARIATION_1" })
	{
		if (o.testModelCondition(CombatNames::modelCondition(n)))
		{
			s += std::string(" ") + n;
		}
	}
	for (const char *n : { "NO_ATTACK", "UNSELECTABLE", "UNATTACKABLE", "CAN_ATTACK" })
	{
		if (o.testStatus((unsigned)CombatNames::status(n)))
		{
			s += std::string(" +") + n;
		}
	}
	std::printf("  %s at %.2f %.2f %.2f [%s]\n", who, o.getPosition()->x, o.getPosition()->y, o.getPosition()->z, s.c_str());
}
} // namespace

TEST_CASE("PORT-146 reproduction: an archer carried by a moving siege engine")
{
	CombatWorld w(kPort146Objects);
	CarrierBones bones;
	w.logic->combat().setLaunchOffsets(&bones);
	Object *carrier = w.unit("P146Carrier", 'A', 500.0f, 500.0f);
	REQUIRE(carrier != nullptr);
	ContainModuleInterface *c = carrier->getContain();
	REQUIRE(c != nullptr);
	OpenContain *oc = dynamic_cast<OpenContain *>(c);
	REQUIRE(oc != nullptr);
	Object *archer = w.unit("P146Archer", 'A', 500.0f, 500.0f);
	REQUIRE(archer != nullptr);
	REQUIRE(c->isValidContainerFor(*archer, true, false));
	REQUIRE(c->addToContain(archer));
	CHECK(archer->getContainedBy() == carrier);
	Object *still = w.unit("P146Still", 'A', 500.0f, 500.0f);
	REQUIRE(still != nullptr);
	REQUIRE(c->addToContain(still));
	Object *enemy = w.unit("Dummy", 'B', 650.0f, 500.0f);
	REQUIRE(enemy != nullptr);
	w.frames(1);
	std::printf("info: the archer's bone is \"%s\"\n", oc->riderBone(archer->getID()).c_str());
	printConditions("archer (idle carrier)", *archer);
	printConditions("carrier (idle)", *carrier);
	AIUpdateInterface *aai = archer->getAIUpdateInterface();
	REQUIRE(aai != nullptr);
	std::printf("info: the archer's AI idle %d able to attack %d\n", (int)aai->isIdle(), (int)archer->isAbleToAttack());

	// the archer fires from the standing carrier
	const auto before = w.logic->combat().counters().projectilesLaunched;
	w.frames(30);
	std::printf("info: shots in 30 frames standing: %llu (victim %s)\n", (unsigned long long)(w.logic->combat().counters().projectilesLaunched - before),
	            aai->currentVictim() ? aai->currentVictim()->getTemplate()->getName().c_str() : "none");

	// the carrier rolls and the archer's z is followed frame by frame
	carrier->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 800.0f, 500.0f, 0.0f }, CMD_FROM_PLAYER);
	const auto movingBefore = w.logic->combat().counters().projectilesLaunched;
	const Coord3D &cp = *carrier->getPosition();
	std::printf("info: the carrier at %.2f %.2f %.2f\n", cp.x, cp.y, cp.z);
	float minDz = 1e9f, maxDz = -1e9f;
	for (int f = 0; f < 20; ++f)
	{
		w.frames(1);
		const Coord3D &ap = *archer->getPosition(), &np = *carrier->getPosition(), &sp = *still->getPosition();
		const float dz = ap.z - np.z;
		minDz = dz < minDz ? dz : minDz;
		maxDz = dz > maxDz ? dz : maxDz;
		std::printf("  f%-3d carrier %.1f %.1f %.1f | ai archer %.1f %.1f %.1f (dz %.2f) | no-ai %.1f %.1f %.1f (dz %.2f) idle %d\n", f, np.x, np.y, np.z, ap.x,
		            ap.y, ap.z, dz, sp.x, sp.y, sp.z, sp.z - np.z, (int)aai->isIdle());
	}
	std::printf("info: dz range %.3f .. %.3f\n", minDz, maxDz);
	std::printf("info: shots in 20 frames moving: %llu (victim %s)\n", (unsigned long long)(w.logic->combat().counters().projectilesLaunched - movingBefore),
	            aai->currentVictim() ? aai->currentVictim()->getTemplate()->getName().c_str() : "none");
	printConditions("archer (moving carrier)", *archer);
	printConditions("carrier", *carrier);
	std::printf("info: bone queries %d\n", bones.asked);
}
