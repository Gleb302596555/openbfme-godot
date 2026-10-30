// OpenBFME. GPL-3.0.
//
// RebuildHoleExposeDie and RebuildHoleBehavior (lane MODGAP-1, group 2, stops S-1427 / S-5603): the ruin a creep lair or a Wild structure leaves when it dies, and the
// countdown that puts the structure back. Ported from the RotWK binary (caveat S-001); the BFME2 decompilation (Open-BFME-2 Code/GameEngine/Source/GameLogic/Object/
// Behavior/RebuildHole*.cpp, summaries) names the pieces.
//
// TARGET FACTS (RotWK game.dat, each read from the disassembly):
//   RebuildHoleExposeDie (die; table RW 0xC616B8 after the DieMux table, parse RW 0x889A00; constructor RW 0x889A19 over the DieMux base RW 0x85FEEB; data: HoleName + 0x38
//   (RW 0x42EE5E), HoleMaxHealth + 0x3C (real), FadeInTimeSeconds + 0x40 (real), TransferAttackers + 0x44 (bool, default 1 RW 0x88654A is the module's, the data's is not read));
//     * onDie RW 0x889AAF: nothing unless the DieMux admits the death (RW 0x85FED5), the dead object's player is not the neutral player (PlayerList + 0x18), is active
//       (RW 0x6AAC52: not an observer, not defeated) and the object was not UNDER_CONSTRUCTION (object + 0x94 bit 2, i.e. status 2). Else the hole template (HoleName,
//       RW 0x6D1305) is made for the dead object's team (RW 0x6D165E) at its position and angle, with its GeometryInfo (RW 0x699349) and script name (RW 0x759467
//       moves the name), out of and back into the partition (RW 0x6E85FB / 0x6E85E9), its body's maximum health set to HoleMaxHealth (body slot 0x5C(health, 0));
//       the hole's rebuild interface (RW 0x886448 finds the RebuildHoleBehavior by its name key) gets the dead object's template and id (RW 0x886831); the dead
//       object's spawn interface (RW 0x68C3A3, slot 0x2C isActive) tells the hole whether the spawner was active (interface slot 4); TransferAttackers: every object
//       whose AI attacks the dead one (object + 0x8C list) is told to attack the hole (AI RW 0x66826E); FadeInTimeSeconds above 0x0 fades the hole's drawable in
//       (RW 0x70E013 / 0x670AA2, the client);
//   RebuildHoleBehavior (update; table RW 0xC60230, constructor RW 0x8864E0: the rebuild interface at + 0x24, + 0x28 .. + 0x3C zero, + 0x40 the price 0, + 0x44 the
//   spawner flag 1; data: WorkerRespawnDelay + 0x08 (RW 0x73A403, frames as a real), HoleHealthRegen%PerSecond + 0x0C (RW 0x42EEFA, a percent), WorkerObjectName + 0x10):
//     * the data setter RW 0x886831 keeps the template (+ 0x3C), the dead object's id (+ 0x30) and its paid price (+ 0x40 = object + 0x33C), then the reset RW 0x8865F7:
//       the worker (if any) goes, the countdown (+ 0x34) = WorkerRespawnDelay truncated, the hole is made selectable and put back in the partition;
//     * update RW 0x8868C2: a rebuilt object id (+ 0x2C) that no longer exists resets the countdown; with no worker the countdown (above 0) falls one per call and, at 0:
//       WITHOUT WorkerObjectName the structure is made complete at once by BuildAssistant slot 0x38 (buildObjectNow) at the hole's position and angle for the hole's
//       owner, kept as the rebuilt object, given the paid price (+ 0x33C), its producer set to the hole (RW 0x68B6A1), its attackers taken from the hole, and the
//       spawner stopped when the dead one's was; no structure made (the site is blocked): the module sleeps for ever; WITH a name a worker is made and builds it;
//     * the tail RW 0x886B77: below its maximum health the hole is healed by HoleHealthRegen%PerSecond / 5 of the maximum per call (HEALING, DEATH_NONE, the hole as the
//       source); a rebuilt structure that is complete (not UNDER_CONSTRUCTION, or SUPPLY_SOURCE) and not effectively dead takes the hole's script name back and the hole is
//       killed (damage type 8, death type 0x16 FADED: its CreateObjectDie, DeathTypes ALL -FADED, makes no treasure);
//   the hole is destroyed by damage first: its own CreateObjectDie makes the treasure (OCL_SpawnLairTreasure*) and its SlowDeathBehavior sinks it.
// NOT PORTED (stop S-5603): the worker branch (WorkerObjectName: the worker object builds the structure with DozerAIUpdate); TransferAttackers; the dead object's
// GeometryInfo on the hole; the spawn interface's flag (the rebuilt spawner is always active); the fade-in of the hole (client).

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/Module/UpdateModule.h"

#include <string>

class ModuleFactory;
class StateHasher;
class ThingTemplate;

class RebuildHoleExposeDieModuleData : public ModuleData
{
public:
	DieMuxData m_dieMux;               // RW 0xC76BD8
	std::string m_holeName;            // + 0x38
	float m_holeMaxHealth = 0.0f;      // + 0x3C
	float m_fadeInTimeSeconds = 0.0f;  // + 0x40
	bool m_transferAttackers = true;   // + 0x44
	static void buildFieldParse(MultiIniFieldParse &p); // RW 0x889A00
};

class RebuildHoleExposeDie : public BehaviorModule, public DieModuleInterface
{
public:
	OBFME_MODULE_XFER(RebuildHoleExposeDie)
	RebuildHoleExposeDie(Thing *thing, const RebuildHoleExposeDieModuleData *data); // RW 0x889A19
	DieModuleInterface *getDie() override { return this; }
	void onDie(const Event &event) override; // RW 0x889AAF
	static const char *stopLine();
	static void registerClass(ModuleFactory &modules);

private:
	const RebuildHoleExposeDieModuleData *m_data;
};

class RebuildHoleBehaviorModuleData : public ModuleData
{
public:
	float m_workerRespawnDelay = 0.0f;  // + 0x08 frames
	float m_regenPercentPerSecond = 0.1f; // + 0x0C (INFERENCE: the data constructor was not read; holes.ini's comment names 0.1 as the default)
	std::string m_workerName;           // + 0x10
	static void buildFieldParse(MultiIniFieldParse &p); // RW 0xC60230
};

class RebuildHoleBehavior : public UpdateModule
{
public:
	OBFME_MODULE_XFER(RebuildHoleBehavior)
	RebuildHoleBehavior(Thing *thing, const RebuildHoleBehaviorModuleData *data); // RW 0x8864E0
	UpdateSleepTime update() override;                                            // RW 0x8868C2
	void crc(StateHasher &h) const override;

	// RW 0x886831: what the hole rebuilds (the dead object's template, id and paid price), then the reset RW 0x8865F7
	void setRebuildData(const ThingTemplate *what, ObjectID spawnerId, float pricePaid);
	unsigned countdown() const { return m_countdown; }
	ObjectID rebuiltId() const { return m_rebuiltId; }
	const ThingTemplate *rebuildTemplate() const { return m_template; }
	static RebuildHoleBehavior *of(const Object &obj);
	static void registerClass(ModuleFactory &modules);

private:
	void reset(); // RW 0x8865F7
	const RebuildHoleBehaviorModuleData *m_data;
	const ThingTemplate *m_template = nullptr;  // + 0x3C
	ObjectID m_spawnerId = INVALID_ID;          // + 0x30
	ObjectID m_rebuiltId = INVALID_ID;          // + 0x2C
	float m_pricePaid = 0.0f;                   // + 0x40
	unsigned m_countdown = 0;                   // + 0x34
};
