// OpenBFME. GPL-3.0.
// PORT-16 prerequisite: the state shared by Player AI's attacked-supply queries.
// RW 0x8F5ABD (RotWK 2.01). This is not the skirmish manager's AI player.
#pragma once

#include "GameLogic/ObjectTypes.h"

#include <cstdint>

class Player;
class StateHasher;
class TeamFactory;
class Xfer;

class PlayerSupplyAttack
{
public:
	// RW 0x8F5ABD: updates the timer and the selected object ID even on failure.
	bool check(const Player &player, const TeamFactory &teams, std::uint32_t frame);
	// RW 0x8F744B: the other AI caller forces a fresh check before calling.
	void clearTimer() { m_nextFrame = 0; }
	std::uint32_t nextFrame() const { return m_nextFrame; }
	ObjectID objectID() const { return m_objectID; }
	void crc(StateHasher &hash) const;
	void xfer(Xfer &x);

private:
	std::uint32_t m_nextFrame = 0; // Player AI +0x70
	ObjectID m_objectID = INVALID_ID; // Player AI +0x74
};
