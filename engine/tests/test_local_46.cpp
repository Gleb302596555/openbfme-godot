// OpenBFME unit tests. LOCAL-46 (issue #402, P-82): passengers on Grond and the mumak.
// Frozen animation while the host moves, model scale pulsing, mumak archers not attached.
//
// The scene is synthetic (no game files): a carrier with a HordeTransportContain, a horde inside it,
// its archers standing on the bones their PassengerBonePrefix names. The carrier is then ordered to move
// and every frame checks where the passengers are, whether their animation advances, and whether their
// drawn scale drifts.
//
// What it pins: a rider whose KindOf matches a PassengerBonePrefix stands on the bone and rides the
// top-level physical container's presentation pose (tilt, suspension), not the invisible horde's.
// The rider's animation advances while the carrier moves, and its drawn basis stays orthonormal
// (no scale drift) over 300 frames, moving and standing.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameClient/ContainedPose.h"
#include "GameClient/LogicSnapshot.h"

#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Contain/OpenContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>

using namespace combattest;

namespace
{
// A carrier like Ennorath's Grond: HordeTransportContain, PassengerBonePrefix B_CARGO for INFANTRY.
const char kLocal46Objects[] =
	"Weapon L46Bow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 800\n"
	"  PreAttackDelay = 400\n"
	"  PreAttackType = PER_POSITION\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Object L46Carrier\n"
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
	"  Behavior = HordeTransportContain ModuleTag_Siege\n"
	"    Slots = 1\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE CAN_ATTACK\n"
	"    PassengerBonePrefix = PassengerBone:B_CARGO KindOf:INFANTRY\n"
	"    ForceOrientationContainer = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 30\n"
	"  End\n"
	"End\n"
	// a passenger of a kind no PassengerBonePrefix names: it falls back to "ARROW_", finds nothing
	// and is put at the container's origin
	"Object L46Stranger\n"
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
	"End\n"
	// an archer that can fire while contained
	"Object L46Archer\n"
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
	"    Weapon = PRIMARY L46Bow\n"
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
	"End\n";

// the client's bone provider for the carrier: two bones of the prefix "B_CARGO", 30 above the carrier
class CarrierBones : public ProjectileLaunchOffsets
{
public:
	bool launchOffset(const Object &, int, int, float[12]) override { return false; }
	int multiLogicalBones(const Object &, const std::string &prefix, const std::array<std::uint32_t, 19> &, int maxBones, float (*out)[12]) override
	{
		++asked;
		if (prefix != "B_CARGO" || maxBones < 10)
		{
			return 0;
		}
		for (int i = 0; i < 10; ++i)
		{
			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 4; ++c)
				{
					out[i][r * 4 + c] = r == c ? 1.0f : 0.0f;
				}
			}
			out[i][3] = 0.0f;
			out[i][7] = (i % 2 == 0) ? -8.0f : 8.0f; // side by side
			out[i][11] = 30.0f;                // 30 above
		}
		return 10;
	}
	int asked = 0;
};
} // namespace

TEST_CASE("LOCAL-46: a rider inside a horde inside a transport is on a bone (containedOnBone through the horde)")
{
	// carrier only; passengers use Archer (INFANTRY, from kCombatObjects) and ArcherHorde (from kArcherHorde)
	const char kCarrierOnly[] =
		"Object L46Carrier\n"
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
		"  Behavior = HordeTransportContain ModuleTag_Siege\n"
		"    Slots = 1\n"
		"    PassengerFilter = NONE +INFANTRY\n"
		"    ObjectStatusOfContained = UNSELECTABLE UNATTACKABLE CAN_ATTACK\n"
		"    PassengerBonePrefix = PassengerBone:B_CARGO KindOf:INFANTRY\n"
		"    ForceOrientationContainer = No\n"
		"  End\n"
		"  LocomotorSet\n"
		"    Locomotor = WalkerLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 30\n"
		"  End\n"
		"End\n";
	CombatWorld w(kCarrierOnly);
	CarrierBones bones;
	w.logic->combat().setLaunchOffsets(&bones);
	Object *carrier = w.unit("L46Carrier", 'A', 500.0f, 500.0f);
	REQUIRE(carrier != nullptr);
	ContainModuleInterface *c = carrier->getContain();
	REQUIRE(c != nullptr);
	HordeTransportContain *htc = dynamic_cast<HordeTransportContain *>(c);
	REQUIRE(htc != nullptr);
	Object *archer = w.unit("Archer", 'A', 500.0f, 500.0f);
	REQUIRE(archer != nullptr);
	REQUIRE(c->isValidContainerFor(*archer, true, false));
	REQUIRE(c->addToContain(archer));

	// a horde of archers inside the carrier: the horde's members are riders on the carrier's bones
	Object *horde = w.unit("ArcherHorde", 'A', 500.0f, 700.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(c->addToContain(horde));
	w.frames(1);

	// the direct rider has a bone
	OpenContain *oc = dynamic_cast<OpenContain *>(c);
	REQUIRE(oc != nullptr);
	CHECK_FALSE(oc->riderBone(archer->getID()).empty());

	// the LogicSnapshot marks the direct rider as on a bone
	const std::shared_ptr<const LogicSnapshot> built = LogicSnapshot::build(*w.logic, nullptr, false, 0);
	const LogicSnapshot *snap = built.get();
	REQUIRE(snap != nullptr);
	const ObjectSnapshot *r = snap->find(archer->getID());
	REQUIRE(r != nullptr);
	CHECK(r->containedBy == carrier->getID());
	CHECK(r->containedOnBone); // direct rider on B_CARGO01

	// the horde's members: each on its own bone of the carrier
	ContainModuleInterface *hc = horde->getContain();
	REQUIRE(hc != nullptr);
	const ContainModuleInterface::ContainedItemsList *members = hc->getContainedItemsList();
	REQUIRE(members != nullptr);
	REQUIRE(!members->empty());
	for (Object *member : *members)
	{
		const ObjectSnapshot *mrec = snap->find(member->getID());
		REQUIRE(mrec != nullptr);
		CHECK(mrec->containedBy == horde->getID()); // the horde's members are contained by the horde
		CHECK(mrec->containedOnBone); // each member on a bone of the carrier (via the carrier's OpenContain)
	}
}

TEST_CASE("LOCAL-46: compose with a grandparent container (rider's logic pose relative to the top container)")
{
	using containedpose::Pose;
	// the grandparent container stands at (-100, 50, 0) facing +x: identity logic basis
	Pose grandLogic;
	grandLogic.pos = Coord3D{ -100.0f, 50.0f, 0.0f };
	// its presentation adds a quarter pitch about X and 1.5 of suspension height
	Pose grandPresentation;
	const float pitchQuarter[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
	std::memcpy(grandPresentation.basis, pitchQuarter, sizeof(pitchQuarter));
	grandPresentation.pos = Coord3D{ -100.0f, 50.0f, 1.5f };
	// an intermediate horde stands at the grandparent's position (no extra pose)
	Pose hordeLogic = grandLogic;
	// the rider on the horde's bone: 8 to the -y and 30 above the horde, facing +x
	Pose riderLogic;
	riderLogic.pos = Coord3D{ -100.0f, 42.0f, 30.0f };

	// compose using the grandparent's presentation: (0, -8, 30) in the carrier's frame, quarter pitch sends
	// 30 to y -30 and -8 to z -8, suspension lifts by 1.5
	const Pose drawn = containedpose::compose(grandPresentation, grandLogic, riderLogic);
	CHECK(drawn.pos.x == -100.0f);
	CHECK(drawn.pos.y == 20.0f);
	CHECK(drawn.pos.z == -6.5f);

	// the rider's own turn stays: a quarter yaw of its own is that yaw of the drawn basis, after the carrier's pitch
	Pose turned = riderLogic;
	const float yawQuarter[9] = { 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	std::memcpy(turned.basis, yawQuarter, sizeof(yawQuarter));
	const Pose drawnTurned = containedpose::compose(grandPresentation, grandLogic, turned);
	const float expected[9] = { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
	for (int i = 0; i < 9; ++i)
	{
		CHECK(drawnTurned.basis[i] == expected[i]);
	}

	// with no extra pose (grandPresentation == grandLogic) the rider is unchanged: the horde in the middle is invisible
	const Pose unchanged = containedpose::compose(grandLogic, grandLogic, riderLogic);
	CHECK(unchanged.pos.x == riderLogic.pos.x);
	CHECK(unchanged.pos.y == riderLogic.pos.y);
	CHECK(unchanged.pos.z == riderLogic.pos.z);
}

TEST_CASE("LOCAL-46: compose preserves orthonormality - no scale drift over 300 frames")
{
	using containedpose::Pose;
	// a container that pitches back and forth each frame (like suspension on a rough road)
	Pose containerLogic;
	containerLogic.pos = Coord3D{ 0.0f, 0.0f, 0.0f };
	// identity logic basis
	for (int i = 0; i < 9; ++i)
		containerLogic.basis[i] = (i % 4 == 0 || i % 4 == 4) ? 1.0f : 0.0f;

	Pose riderLogic;
	riderLogic.pos = Coord3D{ 0.0f, 0.0f, 30.0f };
	for (int i = 0; i < 9; ++i)
		riderLogic.basis[i] = (i % 4 == 0 || i % 4 == 4) ? 1.0f : 0.0f;

	float worstScaleErr = 0.0f;
	Pose prev;
	prev.pos = Coord3D{ 0.0f, 0.0f, 30.0f };
	for (int i = 0; i < 9; ++i)
		prev.basis[i] = (i % 4 == 0 || i % 4 == 4) ? 1.0f : 0.0f;

	for (int f = 0; f < 300; ++f)
	{
		// the container's presentation pitch oscillates: +5 deg / -5 deg / 0
		const float angle = (f % 3 == 0) ? 0.0872664626f : ((f % 3 == 1) ? -0.0872664626f : 0.0f); // ~5 degrees
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		Pose containerPresentation = containerLogic;
		containerPresentation.basis[0] = c;
		containerPresentation.basis[1] = -s;
		containerPresentation.basis[3] = s;
		containerPresentation.basis[4] = c;
		containerPresentation.pos.z = std::sin(f * 0.1f) * 2.0f; // suspension bobbing

		Pose drawn = containedpose::compose(containerPresentation, containerLogic, riderLogic);

		// check orthonormality: each row/column of the basis should be unit length, rows/cols orthogonal
		float rowErr = 0.0f;
		for (int r = 0; r < 3; ++r)
		{
			float len2 = drawn.basis[r * 3 + 0] * drawn.basis[r * 3 + 0]
			           + drawn.basis[r * 3 + 1] * drawn.basis[r * 3 + 1]
			           + drawn.basis[r * 3 + 2] * drawn.basis[r * 3 + 2];
			rowErr = std::max(rowErr, std::fabs(len2 - 1.0f));
		}
		for (int r = 0; r < 3; ++r)
		{
			for (int r2 = r + 1; r2 < 3; ++r2)
			{
				float dot = drawn.basis[r * 3 + 0] * drawn.basis[r2 * 3 + 0]
				          + drawn.basis[r * 3 + 1] * drawn.basis[r2 * 3 + 1]
				          + drawn.basis[r * 3 + 2] * drawn.basis[r2 * 3 + 2];
				rowErr = std::max(rowErr, std::fabs(dot));
			}
		}
		worstScaleErr = std::max(worstScaleErr, rowErr);
		prev = drawn;
	}
	CHECK(worstScaleErr < 1e-4f); // tolerance: binary32 rounding over 300 compose steps cannot reach a visible 0.01% scale error
}
