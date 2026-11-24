// OpenBFME. GPL-3.0. PORT-234: do not attach these cues to the Salvage or Veterancy reward paths.
#include "GameLogic/Module/RewardCrates.h"

#include "Common/Audio/AudioRequests.h"
#include "Common/NumericState.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/Thing/ThingFactory.h"
#include "GameLogic/Economy.h"
#include "GameLogic/FindPositionAround.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"
#include "GameLogic/System/ShroudManager.h"

#include <cstddef>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

void MoneyCrateData::buildFieldParse(MultiIniFieldParse &p)
{
	CrateCollideModuleData::buildFieldParse(p);
	static const FieldParse rows[] = {{"MoneyProvided", INI::parseUnsignedInt, nullptr, (int)offsetof(MoneyCrateData, moneyProvided)}, {nullptr, nullptr, nullptr, 0}};
	p.add(rows);
}

void UnitCrateData::buildFieldParse(MultiIniFieldParse &p)
{
	CrateCollideModuleData::buildFieldParse(p);
	static const FieldParse rows[] = {
		{"UnitCount", INI::parseUnsignedInt, nullptr, (int)offsetof(UnitCrateData, unitCount)},
		{"UnitName", INI::parseAsciiString, nullptr, (int)offsetof(UnitCrateData, unitType)}, {nullptr, nullptr, nullptr, 0}};
	p.add(rows);
}

// RW 0x8C0A02 (0x650ECE -> 0x8C0958, 0xC71450+30). DONOR BFME2 0x004BC8B9 (HealCrateCollide.cpp).
bool HealCrateCollide::executeCrateBehavior(Object &collector)
{
	Player *owner = collector.getControllingPlayer();
	if (!owner) return false;
	// RW 0x6ABA9C -> 0x7A18A7 -> 0x7A05FD: owned prototypes, newest team/member first. Heal completely
	// uses a self-sourced HEALING of the immediate at 0xC11DB4 (RW 0x69067B), not a setHealth on just the picker.
	for (const auto &prototype : collector.logic().players().teams().prototypes())
	{
		if (prototype->getControllingPlayer() != owner) continue;
		const auto &teams = prototype->teams();
		for (auto it = teams.rbegin(); it != teams.rend(); ++it)
		{
			for (Object *o = (*it)->getLastMember(); o; o = o->friend_teamPrev())
			{
				o->attemptHealing(999999.0f, nullptr);
			}
		}
	}
	AudioApi::playMiscAudioAtPosition("CrateHeal", *collector.getPosition());
	return true;
}

// RW 0x8C0B32 (0x650F21 -> 0x8C0A88, 0xC71488+30). DONOR BFME2 0x004BC9E9 (MoneyCrateCollide.cpp).
bool MoneyCrateCollide::executeCrateBehavior(Object &collector)
{
	unsigned amount = m_data->moneyProvided;
	Economy &economy = collector.logic().economy();
	if (economy.isMultiplayerGame())
	{
		const float mult = economy.multiPlayMoneyMult(economy.livePlayableCount(false));
		amount = NumericState::ftol2Low32(NumericState::pc24MulW(NumericState::fildU32(amount), mult));
	}
	if (Player *p = collector.getControllingPlayer())
	{
		p->getMoney()->deposit((unsigned)economy.applyHandicapMoney(*p, (int)amount), &p->getScoreKeeper(), true);
	}
	AudioApi::playMiscAudioForObject("CrateMoney", collector.getID()); // RW 0x8C0BC1 even on zero deposit
	return true;
}

// RW 0x8C0C6F (0x650F59 -> 0x8C0C26, 0xC714C0+30). DONOR BFME2 0x004BCB26 (ShroudCrateCollide.cpp).
bool ShroudCrateCollide::executeCrateBehavior(Object &collector)
{
	const Player *p = collector.getControllingPlayer();
	ShroudManager *shroud = collector.logic().shroud();
	if (!p || !shroud) return false;
	shroud->revealMapForPlayer(p->getPlayerIndex()); // RW 0xB4D930 -> 0xB4F570, not a permanent reveal
	AudioApi::playMiscAudioForObject("CrateShroud", collector.getID());
	return true;
}

// RW 0x8C0E06 (0x650FAC -> 0x8C0D5C, 0xC714FC+30). DONOR BFME2 0x004BCCBD (UnitCrateCollide.cpp).
bool UnitCrateCollide::executeCrateBehavior(Object &collector)
{
	GameLogic &logic = collector.logic();
	const ThingTemplate *unit = logic.things().findTemplate(m_data->unitType);
	const Player *p = collector.getControllingPlayer();
	if (!unit || !p) return false;
	FindPositionOptions options;
	options.maxRadius = 20.0f; // RW 0x8C0ED1: min=0, default random start, maxZDelta=1e10, no flags or objects
	for (unsigned i = 0; i < m_data->unitCount; ++i)
	{
		if (Object *made = logic.newObject(unit, p->getDefaultTeam(), {}))
		{
			Coord3D pos = *collector.getPosition();
			FindPosition::findPositionAround(logic, *collector.getPosition(), options, pos);
			made->setOrientation(collector.getOrientation());
			made->setPosition(&pos);
		}
	}
	AudioApi::playMiscAudioForObject("CrateFreeUnit", collector.getID()); // RW 0x8C0F18 once, never on the new unit
	return true;
}

void RewardCrate::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1); // RW 0x8C1442
	xferCrateCollide(x);
}
