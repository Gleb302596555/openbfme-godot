// OpenBFME. GPL-3.0.
//
// OneRingPenaltyUpdate (lane MODGAP-1, MODULE_GAPS.md group 9; retail 2.01 has it only commented out, Ennorath's Frodo uses it): wearing the One Ring too long calls the Eye of
// Sauron, which roams around the wearer; when it finishes the ring comes off and cannot be used for a while. Ported from the RotWK binary (caveat S-001); the BFME2 decompile
// Object/Update/OneRingPenaltyUpdate.cpp and OneRingPenaltyModuleDataView.h name the fields.
//
// TARGET FACTS (RotWK game.dat):
//   create RW 0x64E0DC, 0x34 bytes, constructor RW 0x89D21F (zeroes, wake next frame RW 0x850C32(1)); table RW 0xC66B18: SpecialObjectName + 0x08, RingTimeBeforeSpawning + 0x0C,
//   TimeSpentRoamingAround + 0x10, TimeRingPowerSuppressed + 0x14, StartingDistanceFromMe + 0x18, TimeFrozenFromPenalty + 0x1C, DiscoveredSound + 0x20 (RW 0x73B217). Module:
//   + 0x24 the eye's id, + 0x28 the frame the ring was seen on, + 0x2C the eye's angle, + 0x30 the frame of the penalty. Mask 3: update and die.
//   * update RW 0x89D647 (answers 5 frames, RW 0xD9F608): worn = (invisible RW 0x68FC2F or STEALTHED) and the StealthUpdate's ring is on (+ 0x31). A penalty older than
//     TimeRingPowerSuppressed (frame > penalty + time, unsigned) ends (RW 0x89D3E4: + 0x30 = 0, CAN_STEALTH on). Else no ring frame: worn keeps the frame. Else not worn: the
//     frame clears and the eye (if any) dies. Else with an eye it moves (RW 0x89D4D2), without one a frame past ring + RingTimeBeforeSpawning spawns it (RW 0x89D2AC);
//   * spawn RW 0x89D2AC: the template by name (none: nothing), made on the neutral player's default team (the object's team without one), empty mask (RW 0x6D165E); the angle
//     draws GameLogicRandomValueReal(-PI, PI) (line 0xDA) and is kept; the position is the object's + (cos, sin) * StartingDistanceFromMe at the terrain's height;
//   * move RW 0x89D4D2: with start = ring + RingTimeBeforeSpawning and end = start + TimeSpentRoamingAround: a frame >= end puts the eye on the object and pays the penalty.
//     Else an eye whose AI is idle (slot 0x1B8) adds a draw of (-PI / 2, PI / 2) to the angle (line 0xB8) and walks (aiMoveToPosition, CMD_FROM_AI) to the object's position
//     + (cos, sin) * radius on the terrain; radius = StartingDistance, or at frame != start: distance + (frame - start) / (end - start) * (distance * 0.5 - distance);
//   * penalty RW 0x89D41B: the ring frame clears, + 0x30 = frame, a worn ring toggles off (RW 0x776B50), CAN_STEALTH off (RW 0x62684D(0x12, 0)), the emotion request (5, none, 1)
//     (RW 0x68F37F), DiscoveredSound on the object, then the eye is killed (RW 0x89D3F5: UNRESISTABLE, death 0);
//   * die RW 0x89D4C7 kills the eye the same way.
// TimeFrozenFromPenalty (+ 0x1C) is read by none of these functions (INFERENCE: the emotion's own time rules; stop S-5604). The snapshot is OpenBFME native (S-4190).

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpdateModule.h"

#include <string>

class ModuleFactory;
class StateHasher;

class OneRingPenaltyUpdateModuleData : public ModuleData
{
public:
	std::string m_specialObjectName;     ///< + 0x08
	unsigned m_ringTimeBeforeSpawning = 0; ///< + 0x0C (frames)
	unsigned m_timeSpentRoamingAround = 0; ///< + 0x10
	unsigned m_timeRingPowerSuppressed = 0; ///< + 0x14
	float m_startingDistanceFromMe = 0.0f; ///< + 0x18
	unsigned m_timeFrozenFromPenalty = 0;  ///< + 0x1C (read by nothing in RW's module)
	std::string m_discoveredSound;       ///< + 0x20
	static void buildFieldParse(MultiIniFieldParse &p);
};

class OneRingPenaltyUpdate : public UpdateModule, public DieModuleInterface
{
public:
	OBFME_MODULE_XFER(OneRingPenaltyUpdate)
	OneRingPenaltyUpdate(Thing *thing, const OneRingPenaltyUpdateModuleData *data); // RW 0x89D21F
	DieModuleInterface *getDie() override { return this; }
	UpdateSleepTime update() override;                                  // RW 0x89D647
	void onDie(const DieModuleInterface::Event &event) override;        // RW 0x89D4C7
	void crc(StateHasher &hasher) const override;

	ObjectID eye() const { return m_eye; }
	UnsignedInt ringFrame() const { return m_ringFrame; }
	UnsignedInt penaltyFrame() const { return m_penaltyFrame; }
	static const char *stopLine();
	static void registerClass(ModuleFactory &modules);

private:
	void endPenalty();   // RW 0x89D3E4
	void killEye();      // RW 0x89D3F5
	void payPenalty();   // RW 0x89D41B
	void spawnEye();     // RW 0x89D2AC
	void moveEye();      // RW 0x89D4D2

	const OneRingPenaltyUpdateModuleData *m_data;
	ObjectID m_eye = INVALID_ID;     ///< + 0x24
	UnsignedInt m_ringFrame = 0;     ///< + 0x28
	float m_angle = 0.0f;            ///< + 0x2C
	UnsignedInt m_penaltyFrame = 0;  ///< + 0x30
};
