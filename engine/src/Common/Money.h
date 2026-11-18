// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
//
// Money (ZH Include/Common/Money.h, Source/Common/RTS/Money.cpp): a player's cash. init() clears it, deposit adds, withdraw takes
// at most what is there and returns what it took.
//
// TARGET (RotWK game.dat, caveat S-001; lane ECON-1 read the whole class): the object is {+0 unused, +4 amount (u32), +8 player index}.
//   * withdraw (RW 0x7B17EF, ret 0xC; args amount, ScoreKeeper*, playSound): the amount is capped at the cash; 0 returns 0 before anything else;
//     with playSound the withdraw sound event plays; cash -= amount; the LOCAL player's statistics (RW 0xDE8A98 + 4) count it when this is the
//     local player's money; then ScoreKeeper::addMoneySpent(amount) when the keeper is not null. Returns the amount taken.
//   * deposit (RW 0x7B18B8, ret 0xC): amount 0 does nothing at all; sound; cash += amount (u32 add, wraps like ZH); local statistics + 8; the keeper's
//     addMoneyEarned(amount) when not null.
//   * set-amount (RW 0x7B18B8's twin used by Player::init with the keeper null) is `deposit` on a cleared Money: the template's StartMoney.
// Local player statistics are not here. Sound requests have no bearing on the returned transfer or the score.
//
// The ScoreKeeper pointer follows the retail signature: money that is earned or spent through a player's score passes that player's keeper (the
// economy modules pass Player::getScoreKeeper()); a caller with null leaves the score alone (a refund that is not income, the start money).

#pragma once

#include "Common/ScoreKeeper.h"
#include "Common/StateHash.h"

#include <cstdint>

class Money
{
public:
	Money() = default;
	explicit Money(std::uint32_t amount)
		: m_money(amount)
	{
	}

	void init() { m_money = 0; }
	std::uint32_t countMoney() const { return m_money; }

	// returns the amount actually withdrawn (never more than the player has)
	std::uint32_t withdraw(std::uint32_t amountToWithdraw, bool playSound = true) { return withdraw(amountToWithdraw, nullptr, playSound); }
	std::uint32_t withdraw(std::uint32_t amountToWithdraw, ScoreKeeper *score, bool playSound = true);
	void deposit(std::uint32_t amountToDeposit, bool playSound = true) { deposit(amountToDeposit, nullptr, playSound); }
	void deposit(std::uint32_t amountToDeposit, ScoreKeeper *score, bool playSound = true);
	void setPlayerIndex(int index) { m_playerIndex = index; }
	int getPlayerIndex() const { return m_playerIndex; }

	void crc(StateHasher &h) const { h.addU32(m_money); }
	void xfer(class Xfer &x); // lane SAVE-4a (Player.cpp)

private:
	std::uint32_t m_money = 0;
	int m_playerIndex = 0;
};
