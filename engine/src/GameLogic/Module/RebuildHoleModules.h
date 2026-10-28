// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0 (GameLogic/Object/Die/RebuildHoleExposeDie.cpp,
// GameLogic/Object/Behavior/RebuildHoleBehavior.cpp).
//
// The rebuild holes (lane BFME2CAMP1, stop S-1427 closed): a structure with RebuildHoleExposeDie leaves a hole (the creep lairs' SpiderLairHole,
// WargLairHole ...) whose RebuildHoleBehavior rebuilds the structure after WorkerRespawnDelay.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; BFME2 decomp RebuildHoleExposeDieOnDie.cpp / RebuildHoleBehavior*.cpp, same shape):
//   * RebuildHoleExposeDie (create RW 0x64C7B9; data table RW 0xC616B8 after the DieMux: HoleName + 0x38 parseAsciiString RW 0x42EE5E, HoleMaxHealth
//     + 0x3C parseReal RW 0x42ED00, FadeInTimeSeconds + 0x40 parseReal, TransferAttackers + 0x44 parseBool RW 0x42E558; xfer RW 0x8899A4: version 1, the
//     die module's). onDie RW 0x889AAF: the die mux applies; the object's player is not the neutral player (ThePlayerList + 0x18), is active (RW 0x6AAC52:
//     neither observer nor defeated) and the object is not UNDER_CONSTRUCTION (status 2, Object + 0x94 bit 2); then the HoleName object is made on the
//     object's team (RW 0x6D165E with an empty status mask), placed at its position and angle (RW 0x70C201 / 0x70C31E), given its geometry (RW 0x699349)
//     and its script name (RW 0x759467); the object leaves the pathfind map and the hole enters it (RW 0x6E85FB / 0x6E85E9); the hole's max health is
//     HoleMaxHealth (body slot 0x5C, change type 0); its rebuild interface (RW 0x886448) takes the object's spawn interface's active flag (spawn slot
//     11 -> rebuild slot 4) when it has one, then startRebuildProcess(the object's template, its id) (rebuild slot 0); with TransferAttackers every
//     AI of the game transfers its attack from the object to the hole (RW 0x66826E); FadeInTimeSeconds > 0 fades the hole's drawable in over
//     _ftol2(30 * seconds) client frames (RW 0x670AA2).
//   * RebuildHoleBehavior (constructor RW 0x8864EE, vtable RW 0xC60294; data table RW 0xC60230: WorkerObjectName + 0x10 parseAsciiString,
//     WorkerRespawnDelay + 0x08 parseDurationReal RW 0x73A403, HoleHealthRegen%PerSecond + 0x0C parsePercentToReal RW 0x42EEFA). Fields: + 0x28 the
//     worker's id, + 0x2C the rebuilt structure's, + 0x30 the dead structure's, + 0x34 the frames until the next worker / rebuild, + 0x38 the worker
//     template, + 0x3C the template to rebuild, + 0x40 the dead structure's paid cost (Object + 0x33C), + 0x44 its spawner's active flag (1 at start).
//     xfer RW 0x886669 (version 3): the update module's, the three ids, + 0x34, the two templates by name, + 0x44, and from version 3 + 0x40.
//   * startRebuildProcess RW 0x886831 (interface RW 0xC60270 slot 0): + 0x40 = the spawner's paid cost (when it still exists), + 0x3C = the template,
//     + 0x30 = the spawner's id, then newWorkerRespawnProcess(the worker, when it exists).
//   * newWorkerRespawnProcess RW 0x8865F7: the worker given is destroyed (RW 0x62BBAB); + 0x28 = 0; + 0x34 = _ftol2(WorkerRespawnDelay); the hole is
//     unmasked (RW 0x68BEB1(0): Object + 0x459 bit 1), loses UNSELECTABLE and UNATTACKABLE (statuses 3 and 60, RW 0x68D440(mask, 0)), becomes
//     selectable (RW 0x68BE3C(1): Object + 0x454) and enters the pathfind map (RW 0x6E85E9).
//   * update RW 0x8868C2 (the update interface RW 0xC60288 slot 0): a worker id that finds no object restarts the process (with no worker); a rebuilt
//     id that finds none restarts it with the worker and is cleared. Without a worker and with + 0x34 above 0, + 0x34 counts down; at 0:
//       - no WorkerObjectName (RW 0x401E64): the hole leaves the pathfind map and TheBuildAssistant's slot 0x38 (buildObjectNow) makes the structure
//         at the hole's position and angle for the hole's player; none -> sleep for ever (0x3FFFFFFF). A structure with a GettingBuiltBehavior (RW
//         0x68C3E6) starts at 1% health (body slot 0x58: setInitialHealthPercent(1.0)), is force-completed (slot 0x28) and starts building itself (slot
//         0x10, forced); its model conditions 0x44 / 0x45 are set (RW 0x5E3BA5) and its drawable updated (client);
//       - a WorkerObjectName: the template is found (RW 0x6D1305) and the worker made on the hole's team at the hole's position; + 0x28 = its id; with an
//         AI it builds the structure (AI slot 0x1F8, the dozer's construct; none -> sleep for ever) or resumes the one there (RW 0x771526); the worker
//         becomes UNSELECTABLE.
//     Then, with a structure: every AI transfers its attack from the hole to it, + 0x2C = its id, its producer is the hole (RW 0x68B6A1), its paid cost
//     + 0x40; with the spawner's flag clear its spawn interface stops spawning (spawn slot 9); the hole is masked (RW 0x68BEB1(1)), becomes UNSELECTABLE
//     and UNATTACKABLE and not selectable. Every frame the hole heals while its health is below its max: DAMAGE_HEALING (7), DEATH_NONE (1), source the
//     hole, amount = HoleHealthRegen%PerSecond / 5 * max health (x87), body slot 4 (attemptHealing). The structure is done when it is
//     BUILD_BEING_CANCELED (status 86) or neither UNDER_CONSTRUCTION nor (with a GettingBuiltBehavior) dead: the hole's script name goes to it (RW
//     0x759467), the worker is destroyed and the hole killed (RW 0x698EC3(8, 0x16): UNRESISTABLE damage, death type 22). The update answers 1.
// INFERENCE / NOT PORTED (S-1427): the geometry copy RW 0x699349 (the port's geometry is the template's: S-1423); the mask bit (Object + 0x459 bit 1,
// RW 0x68BEB1, also written by RW 0x86321B / 0x8A77D6) is not modelled: its readers (RW 0x6C7F11, 0x6C9147, 0x81E60B, 0x83BCB1) and the mask's client
// partition call RW 0x625759 are not ported; the old holder's colour reset in RW 0x759467 and the drawable updates are the client's.

#pragma once

#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/Module/UpdateModule.h"

#include <string>

class ModuleFactory;
class MultiIniFieldParse;
class ThingTemplate;

class RebuildHoleExposeDieModuleData : public ModuleData
{
public:
	DieMuxData m_dieMux;                 ///< RW 0xC76BD8
	std::string m_holeName;              ///< + 0x38
	float m_holeMaxHealth = 0.0f;        ///< + 0x3C
	float m_fadeInTimeSeconds = 0.0f;    ///< + 0x40
	bool m_transferAttackers = true;     ///< + 0x44 (RW 0x8899D0: 1)
	static void buildFieldParse(MultiIniFieldParse &p);
};

class RebuildHoleExposeDie : public BehaviorModule, public DieModuleInterface
{
public:
	OBFME_MODULE_XFER(RebuildHoleExposeDie)
	RebuildHoleExposeDie(Thing *thing, const RebuildHoleExposeDieModuleData *data) : BehaviorModule(thing, data), m_data(data) {}
	DieModuleInterface *getDie() override { return this; }
	void onDie(const DieModuleInterface::Event &event) override; // RW 0x889AAF
	void crc(StateHasher &) const override {}

private:
	const RebuildHoleExposeDieModuleData *m_data;
};

class RebuildHoleBehaviorModuleData : public ModuleData
{
public:
	float m_workerRespawnDelay = 0.0f;   ///< + 0x08 (frames)
	float m_holeHealthRegen = 0.1f;      ///< + 0x0C (RW 0x48323E: the shared 0.1f)
	std::string m_workerObjectName;      ///< + 0x10
	static void buildFieldParse(MultiIniFieldParse &p);
};

class RebuildHoleBehavior : public UpdateModule
{
public:
	OBFME_MODULE_XFER(RebuildHoleBehavior)
	RebuildHoleBehavior(Thing *thing, const RebuildHoleBehaviorModuleData *data);
	UpdateSleepTime update() override; // RW 0x8868C2
	void crc(StateHasher &hasher) const override;

	// the rebuild interface (RW 0xC60270): slot 0 startRebuildProcess RW 0x886831, slot 4 the spawner's active flag
	void startRebuildProcess(const ThingTemplate *rebuild, ObjectID spawnerId);
	void setSpawnerActive(bool active) { m_spawnerActive = active; }
	// RW 0x886448: the hole's RebuildHoleBehavior, or null
	static RebuildHoleBehavior *of(Object &obj);

	ObjectID workerId() const { return m_workerId; }
	ObjectID reconstructingId() const { return m_reconstructingId; }
	ObjectID spawnerId() const { return m_spawnerId; }
	unsigned workerWaitCounter() const { return m_workerWaitCounter; }
	const ThingTemplate *rebuildTemplate() const { return m_rebuildTemplate; }

private:
	void newWorkerRespawnProcess(Object *existingWorker); // RW 0x8865F7
	const RebuildHoleBehaviorModuleData *m_data;
	ObjectID m_workerId = INVALID_ID;                  ///< + 0x28
	ObjectID m_reconstructingId = INVALID_ID;          ///< + 0x2C
	ObjectID m_spawnerId = INVALID_ID;                 ///< + 0x30
	unsigned m_workerWaitCounter = 0;                  ///< + 0x34
	const ThingTemplate *m_workerTemplate = nullptr;   ///< + 0x38
	const ThingTemplate *m_rebuildTemplate = nullptr;  ///< + 0x3C
	float m_spawnerCostPaid = 0.0f;                    ///< + 0x40
	bool m_spawnerActive = true;                       ///< + 0x44
};

namespace RebuildHoleModules
{
void registerAll(ModuleFactory &modules);
}
