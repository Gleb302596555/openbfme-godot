// OpenBFME unit tests. PORT-146 (issue #363): the archers on a moving siege carrier (Grond, P-82).
//
// The scene is synthetic (no game files): a siege carrier with a SiegeEngineContain, its passengers standing on the bones their PassengerBonePrefix
// names (the client's bone provider is a stub, as in port 22's tests), an archer of the owner on one bone and an enemy in the archer's reach. The carrier
// is then ordered to move and every frame prints where the passengers are and whether the archer fired.
//
// What it pins (2026-11-12): a rider whose KindOf matches a PassengerBonePrefix stands on `<prefix>01` / `<prefix>02` and stays exactly 30.00 above the
// carrier for every frame of the move; it carries TRANSPORT_MOVING while the carrier rolls; it is a rider and not the crew (crewCount 0); and with
// CanAttackWhileContained (the AIUpdate field at RW 0xC0F530 + 0x25) it fires from the moving carrier. A passenger whose KindOf matches no
// PassengerBonePrefix falls back to the "ARROW_" prefix and, finding no bone, is placed at the container's own origin (the 30 below the archer) and
// carries NO_ATTACK - that is the one shape in which the owner's "jumps and does not shoot" can come out of this path at all, so it is pinned too.
//
// The issue itself stays open: on the shape the report describes the path below behaves, so the divergence is in what this scene cannot see - the real
// model's bone list per model state, the mod's MordorArcherForGrond fields, or the client, which hands the container's DependencySharedModelFlags
// (Grond: MOVING TURN_LEFT TURN_RIGHT ... ) to every contained drawable (DrawableManager::syncTransforms, stop S-1791 INFERENCE "its riders and
// crew": a rider's draw module then picks its MOVING AnimationState while the carrier rolls).

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameClient/ContainedPose.h"
#include "GameClient/LogicSnapshot.h"

#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Contain/OpenContainRuntime.h"
#include "GameLogic/Object/Contain/SiegeEngineContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>

using namespace combattest;

namespace
{
// MordorGrond's shape as the mod has it (object/evilfaction/units/mordor/grond.ini): Slots = 6, PassengerFilter = NONE +MordorArcherForGrond,
// ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE CAN_ATTACK, PassengerBonePrefix = PassengerBone:B_ARCHER KindOf:NOT_AUTOACQUIRABLE,
// a CrewFilter of trolls, and the archer's own weapon is the turreted "MordorSummonedBarricadeArcherBowGrondMoving" (RotatingTurret = Yes, the bow of
// port 28's in-game check: the archer turns on its bone to shoot, its AI's Turret holds ControlledWeaponSlots PRIMARY).
const char kPort146Objects[] =
	"Weapon P146GrondBow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 800\n"
	"  PreAttackDelay = 400\n"
	"  PreAttackType = PER_POSITION\n"
	"  RotatingTurret = Yes\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
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
	"    PassengerBonePrefix = PassengerBone:B_ARCHER KindOf:NOT_AUTOACQUIRABLE\n"
	"    CrewFilter = NONE +MONSTER\n"
	"    CrewMax = 6\n"
	"    ObjectStatusOfCrew = UNSELECTABLE UNATTACKABLE\n"
	"    SpeedPercentPerCrew = 50%\n"
	"    CrewAllowedToFire = No\n"
	"    ForceOrientationContainer = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 30\n"
	"  End\n"
	"End\n"
	// a passenger of a kind no PassengerBonePrefix names: it falls back to "ARROW_", finds nothing and is put at the container's origin
	"Object P146Stranger\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  TransportSlotCount = 1\n"
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
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE NOT_AUTOACQUIRABLE\n"
	"  TransportSlotCount = 1\n"
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
	"    Weapon = PRIMARY P146GrondBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = Yes\n"
	"    CanAttackWhileContained = Yes\n"
	"    MoodAttackCheckRate = 400\n"
	"    Turret\n"
	"      TurretTurnRate = 360\n"
	"      ControlledWeaponSlots = PRIMARY\n"
	"    End\n"
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
} // namespace

TEST_CASE("PORT-146: a siege carrier's passengers stand on their bones and the archer fires while it moves")
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
	SiegeEngineContain *siege = dynamic_cast<SiegeEngineContain *>(c);
	REQUIRE(siege != nullptr);
	Object *archer = w.unit("P146Archer", 'A', 500.0f, 500.0f);
	Object *stranger = w.unit("P146Stranger", 'A', 500.0f, 500.0f);
	REQUIRE(archer != nullptr);
	REQUIRE(stranger != nullptr);
	REQUIRE(c->isValidContainerFor(*archer, true, false));
	REQUIRE(c->addToContain(archer));
	REQUIRE(c->addToContain(stranger));
	CHECK(archer->getContainedBy() == carrier);
	Object *enemy = w.unit("Dummy", 'B', 650.0f, 500.0f); // in the archer's reach (its bow is 200)
	REQUIRE(enemy != nullptr);
	w.frames(1);

	// the archer is a rider of the kind its prefix names, the stranger matched no prefix
	CHECK(oc->riderBone(archer->getID()) == "B_ARCHER01");
	CHECK(siege->crewCount() == 0);
	CHECK(archer->getPosition()->z == carrier->getPosition()->z + 30.0f); // the bone's z, added to the carrier's, exactly
	CHECK(stranger->getPosition()->z == carrier->getPosition()->z); // no bone: the container's own origin
	CHECK(oc->riderBone(stranger->getID()).empty()); // "ARROW_" found no bone
	CHECK_FALSE(stranger->isAbleToAttack());

	// it fires from the standing carrier
	AIUpdateInterface *aai = archer->getAIUpdateInterface();
	REQUIRE(aai != nullptr);
	CHECK(archer->isAbleToAttack());
	const auto standing = w.logic->combat().counters().projectilesLaunched;
	w.frames(30);
	std::printf("info: shots in 30 frames standing: %llu (victim %s)\n", (unsigned long long)(w.logic->combat().counters().projectilesLaunched - standing),
	            aai->currentVictim() ? aai->currentVictim()->getTemplate()->getName().c_str() : "none");
	// the bow: PreAttackDelay 400, ClipSize 2 of DelayBetweenShots 800, then ClipReloadTime 2000 - five fires in the 30 frames of one bow's clip run
	CHECK(w.logic->combat().counters().projectilesLaunched == standing + 5);

	// the carrier rolls: the archer keeps its bone offset frame by frame and goes on firing
	REQUIRE(carrier->getAIUpdateInterface() != nullptr);
	carrier->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 800.0f, 500.0f, 0.0f }, CMD_FROM_PLAYER);
	static const int movingCond = CombatNames::modelCondition("MOVING");
	static const int transportMovingCond = CombatNames::modelCondition("TRANSPORT_MOVING");
	const auto rolling = w.logic->combat().counters().projectilesLaunched;
	float lowest = 1e9f, highest = -1e9f;
	for (int f = 0; f < 20; ++f)
	{
		w.frames(1);
		const float dz = archer->getPosition()->z - carrier->getPosition()->z;
		lowest = dz < lowest ? dz : lowest;
		highest = dz > highest ? dz : highest;
		std::printf("  f%-3d carrier %.1f %.1f %.1f | archer %.1f %.1f %.1f (dz %.2f) idle %d | carrier MOVING %d, archer TRANSPORT_MOVING %d\n", f,
		            carrier->getPosition()->x, carrier->getPosition()->y, carrier->getPosition()->z, archer->getPosition()->x, archer->getPosition()->y,
		            archer->getPosition()->z, dz, (int)aai->isIdle(), (int)carrier->testModelCondition(movingCond),
		            (int)archer->testModelCondition(transportMovingCond));
	}
	std::printf("info: dz range %.3f .. %.3f, shots in 20 frames moving: %llu\n", lowest, highest,
	            (unsigned long long)(w.logic->combat().counters().projectilesLaunched - rolling));
	CHECK(lowest == 30.0f); // the archer does not sink into the carrier nor rise off it, frame by frame
	CHECK(highest == 30.0f);
	CHECK(carrier->testModelCondition(movingCond));
	CHECK(archer->testModelCondition(transportMovingCond));
	CHECK(w.logic->combat().counters().projectilesLaunched == rolling + 3); // the bow goes on firing while the carrier rolls
}

TEST_CASE("PORT-146: an object on a bone is drawn with the container's presentation pose, tilt and suspension height alike")
{
	using containedpose::Pose;
	// the carrier stands at (-100, 50, 0) facing +x: identity basis
	Pose containerLogic;
	containerLogic.pos = Coord3D{ -100.0f, 50.0f, 0.0f };
	// what the client's presentation physics adds to it (Drawable::applyPhysicsXform, RW 0x67BE90): a quarter turn of pitch about X - exact, cos 0 and
	// sin 1 - and 1.5 of suspension height (PhysicsXformInfo::m_totalZ)
	Pose containerPresentation;
	const float pitchQuarter[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
	std::memcpy(containerPresentation.basis, pitchQuarter, sizeof(pitchQuarter));
	containerPresentation.pos = Coord3D{ -100.0f, 50.0f, 1.5f };
	// the archer on its bone: 8 to the carrier's -y and 30 above it, facing +x
	Pose riderLogic;
	riderLogic.pos = Coord3D{ -100.0f, 42.0f, 30.0f };

	// a carrier with no extra pose draws its riders exactly where the logic put them: the lane changes nothing there
	const Pose unchanged = containedpose::compose(containerLogic, containerLogic, riderLogic);
	CHECK(unchanged.pos.x == -100.0f);
	CHECK(unchanged.pos.y == 42.0f);
	CHECK(unchanged.pos.z == 30.0f);

	// the extra pose carries the rider: its place in the carrier's frame is (0, -8, 30); a quarter pitch about X sends 30 to y -30 and -8 to z -8, and
	// the suspension lifts it by 1.5. Without the composition the rider would stay at (-100, 42, 30) - the owner's floating and passing through.
	const Pose drawn = containedpose::compose(containerPresentation, containerLogic, riderLogic);
	CHECK(drawn.pos.x == -100.0f);
	CHECK(drawn.pos.y == 20.0f);
	CHECK(drawn.pos.z == -6.5f);

	// the rider's own turn stays in the carrier's frame: a quarter yaw of its own is that yaw of the drawn basis, after the carrier's pitch
	Pose turned = riderLogic;
	const float yawQuarter[9] = { 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	std::memcpy(turned.basis, yawQuarter, sizeof(yawQuarter));
	const Pose drawnTurned = containedpose::compose(containerPresentation, containerLogic, turned);
	const float expected[9] = { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
	for (int i = 0; i < 9; ++i)
	{
		CHECK(drawnTurned.basis[i] == expected[i]);
	}
}

TEST_CASE("PORT-146: only an object its contain puts on a bone is marked for the container's pose (a horde's member is not)")
{
	CombatWorld w(kPort146Objects);
	CarrierBones bones; // the carrier's bone provider: B_ARCHER01 / B_ARCHER02
	w.logic->combat().setLaunchOffsets(&bones);
	Object *carrier = w.unit("P146Carrier", 'A', 500.0f, 500.0f);
	REQUIRE(carrier != nullptr);
	ContainModuleInterface *c = carrier->getContain();
	REQUIRE(c != nullptr);
	Object *archer = w.unit("P146Archer", 'A', 500.0f, 500.0f);
	REQUIRE(archer != nullptr);
	REQUIRE(c->addToContain(archer));
	Object *stranger = w.unit("P146Stranger", 'A', 500.0f, 500.0f); // a kind no PassengerBonePrefix names: it gets no bone
	REQUIRE(stranger != nullptr);
	REQUIRE(c->addToContain(stranger));
	Object *horde = w.unit("ArcherHorde", 'A', 500.0f, 700.0f); // HordeContain with InitialPayload = Archer 6
	REQUIRE(horde != nullptr);
	ContainModuleInterface *hc = horde->getContain();
	REQUIRE(hc != nullptr);
	const ContainModuleInterface::ContainedItemsList *members = hc->getContainedItemsList();
	REQUIRE(members != nullptr);
	REQUIRE(!members->empty());
	Object *member = members->front();
	w.frames(1);

	const std::shared_ptr<const LogicSnapshot> built = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	const LogicSnapshot *snap = built.get();
	REQUIRE(snap != nullptr);
	const ObjectSnapshot *r = snap->find(archer->getID());
	REQUIRE(r != nullptr);
	CHECK(r->containedBy == carrier->getID());
	CHECK(r->containedOnBone); // B_ARCHER01, so the drawable rides the carrier's presentation pose
	const ObjectSnapshot *m = snap->find(member->getID());
	REQUIRE(m != nullptr);
	CHECK(m->containedBy == horde->getID());
	CHECK_FALSE(m->containedOnBone); // a horde's member keeps its own pose
	const ObjectSnapshot *srec = snap->find(stranger->getID());
	REQUIRE(srec != nullptr);
	CHECK(srec->containedBy == carrier->getID());
	CHECK_FALSE(srec->containedOnBone); // the contain found no bone for it: it keeps its own pose too
}
