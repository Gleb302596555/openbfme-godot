// OpenBFME unit tests. GPL-3.0.
// PORT-255 (issue #603): RotWK's SlavedUpdate suspension interface. Synthetic INI only; no installed game data.
// RW 0xC68820 slots 6 / 7 point to 0x49D0E9 / 0x49D0F3: set/read the existing module byte +0x44. The update calls slot 7 at
// RW 0x8A5D38 and a true result branches at 0x8A5D42 to sleep forever at 0x8A5D44. The test also verifies its existing snapshot / CRC state.

#include "doctest.h"
#include "MoveTestUtil.h"

#include "Common/StateHash.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Module/SlaveWatcherBehavior.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace movetest;

namespace
{
const char kPort255Objects[] =
	"Object P255Watcher\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Behavior = SlaveWatcherBehavior ModuleTag_Watcher\n"
	"  End\n"
	"End\n"
	"Object P255Slave\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Die\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slaved\n"
	"    DieOnMastersDeath = Yes\n"
	"    UseSlaverAsControlForEvaObjectSightedEvents = Yes\n"
	"  End\n"
	"End\n";

struct Port255World : MoveWorld
{
	Port255World() : MoveWorld(20, 20, kPort255Objects) {}
};

SlavedUpdate *slavedOf(Object &obj)
{
	auto *module = dynamic_cast<SlavedUpdate *>(obj.findModule("SlavedUpdate"));
	REQUIRE_MESSAGE(module, "the synthetic slave has a SlavedUpdate");
	return module;
}
} // namespace

TEST_CASE("PORT-255 SlavedUpdate: RotWK suspension vslots pause the update and round-trip through the existing snapshot")
{
	Port255World world;
	Object *master = world.spawn("Block", 100, 100);
	Object *slave = world.spawn("P255Slave", 100, 100);
	REQUIRE(master);
	REQUIRE(slave);
	const ObjectID masterID = master->getID();
	const ObjectID slaveID = slave->getID();
	SlavedUpdate *module = slavedOf(*slave);
	SlavedUpdateInterface *interface = SlavedUpdateInterface::of(*slave);
	REQUIRE(interface);
	CHECK_FALSE(interface->isSelfTasking()); // RotWK vtable slot 4 -> RW 0x9188EB: xor al, al; ret
	CHECK_FALSE(interface->isUpdateSuspended());
	CHECK(interface->useSlaverAsControlForEvaObjectSightedEvents()); // vtable slot 5 -> RW 0x8A4FE4 reads module data +0x6D
	CHECK(interface->dieOnMastersDeath()); // vtable slot 8 -> RW 0x8A4FEB reads module data +0x55

	interface->onEnslave(master);
	StateHasher awakeHash;
	module->crc(awakeHash);
	std::vector<std::uint8_t> awakeSnapshot;
	std::string error;
	REQUIRE_MESSAGE(world.logic->saveSnapshot(awakeSnapshot, &error), error);
	const std::uint64_t fullAwakeHash = world.logic->computeStateHash();

	// RW 0x49D0E9 sets interface-this +0x24, i.e. SlavedUpdate +0x44; 0x49D0F3 reads it.
	interface->suspendUpdate();
	CHECK(interface->isUpdateSuspended());
	StateHasher suspendedHash;
	module->crc(suspendedHash);
	CHECK(suspendedHash.value() != awakeHash.value());
	std::vector<std::uint8_t> suspendedSnapshot;
	REQUIRE_MESSAGE(world.logic->saveSnapshot(suspendedSnapshot, &error), error);
	const std::uint64_t fullSuspendedHash = world.logic->computeStateHash();
	CHECK(fullSuspendedHash != fullAwakeHash);

	// The existing SlavedUpdate snapshot v1 already xfers +0x44. Loading the two snapshots restores both values without a schema bump.
	REQUIRE_MESSAGE(world.logic->loadSnapshot(awakeSnapshot, &error), error);
	CHECK(world.logic->computeStateHash() == fullAwakeHash);
	slave = world.logic->findObjectByID(slaveID);
	REQUIRE(slave);
	module = slavedOf(*slave);
	interface = SlavedUpdateInterface::of(*slave);
	REQUIRE(interface);
	CHECK(module->getSlaverID() == masterID);
	CHECK_FALSE(interface->isUpdateSuspended());

	REQUIRE_MESSAGE(world.logic->loadSnapshot(suspendedSnapshot, &error), error);
	CHECK(world.logic->computeStateHash() == fullSuspendedHash);
	slave = world.logic->findObjectByID(slaveID);
	REQUIRE(slave);
	module = slavedOf(*slave);
	interface = SlavedUpdateInterface::of(*slave);
	REQUIRE(interface);
	CHECK(interface->isUpdateSuspended());
	CHECK(module->update() == UPDATE_SLEEP_FOREVER); // RW 0x8A5D38 / 0x8A5D42 / 0x8A5D44: slot 7 true returns forever
}

TEST_CASE("PORT-255 SlaveWatcherBehavior neighbor: a watched slave's death releases the watcher once (RW 0x887AF5 / 0x887A35)")
{
	Port255World world;
	Object *watcherObject = world.spawn("P255Watcher", 100, 100);
	Object *slave = world.spawn("P255Slave", 100, 100);
	REQUIRE(watcherObject);
	REQUIRE(slave);
	auto *watcher = dynamic_cast<SlaveWatcherBehavior *>(watcherObject->findModule("SlaveWatcherBehavior"));
	REQUIRE(watcher);

	watcher->setSlave(slave->getID()); // RW 0x887AF5: remember +0x20 and wake for the next frame
	CHECK(watcher->getSlaveID() == slave->getID());
	CHECK(watcher->update() == UPDATE_SLEEP_NONE); // RW 0x887A35: a live slave keeps the watch active
	CHECK(watcher->releases() == 0u);

	slave->kill(DAMAGE_UNRESISTABLE, 0);
	REQUIRE(slave->isEffectivelyDead()); // ActiveBody + DestroyDie are synthetic fixture data
	CHECK(watcher->update() == UPDATE_SLEEP_FOREVER); // death branch restores configured upgrades, clears the id, then sleeps
	CHECK(watcher->getSlaveID() == INVALID_ID);
	CHECK(watcher->releases() == 1u);
	CHECK(watcher->update() == UPDATE_SLEEP_NONE); // RW returns 1 when no slave id is set; it does not release twice
	CHECK(watcher->releases() == 1u);
}
