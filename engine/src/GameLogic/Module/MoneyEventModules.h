// OpenBFME. GPL-3.0.
//
// The money-moving modules that act on deaths and hits (lane ECON-1): RefundDie (RW create proc 0x64C502, data 0x6536A3, die interface mask 2), PillageModule (RW create
// 0x64C492, data 0x6535ED, interface mask 0x2000: "damage dealt"), and SalvageCrateCollide (RW create 0x651052, data 0x655980, collide interface mask 0x10).
// What calls them (the death pipeline, the damage pipeline, the collision system) belongs to other lanes (stop S-257): RefundDie answers Object::friend_onDie,
// PillageModule::onDamageDealt is the function the damage pipeline calls for each hit its object lands, SalvageCrateCollide answers the collide interface (lane MODGAP-1,
// P-10: AIWorld::processCollisions' static contacts call it; the notes on the class below).
//
// TARGET FACTS (RotWK game.dat, caveat S-001):
//   * DieMuxData (the start of every die module's table, RW 0xC76BD8 extra 8; defaults RW 0x8D2970): DeathTypes (RW 0x73A68A, default ALL), ExemptStatus (+4) and
//     RequiredStatus (+0x14) object status masks (default empty), DamageAmountRequired (+0x24, default -1.0), MinKillerAngle (+0x28, default 1.0) and MaxKillerAngle (+0x2C,
//     default -1.0), both in radians (RW 0x42EE15). isDieApplicable (RW 0x8D29A9, args the object and the damage info): false unless bit (deathType - 1) of DeathTypes is set (the
//     shift count is the x86's, modulo 32); false unless the object's status has every RequiredStatus bit and none of ExemptStatus; with DamageAmountRequired >= 0 false when
//     it is larger than the damage amount; when MaxKillerAngle > MinKillerAngle the killer's bearing must fall inside the window (not ported: reported, S-257).
//   * RefundDie (module size 0x1C, data 0x44): table RW 0xC06998: UpgradeRequired (+0x38, none), RefundPercent (+0x3C, parsePercentToReal, 0.0), BuildingRequired (+0x40,
//     ObjectFilter, unset). onDie (RW 0x888519): the die mux test; the object is neither under construction (status 2) nor sold (status 19); its owner exists, has completed
//     UpgradeRequired (when set) and owns an object satisfying BuildingRequired (when valid, completedOnly = false); refund = ceil(object + 0x33C * RefundPercent) (x87 at
//     24 bits, the MSVCR71 ceil, stored as float, fistp); a non-zero refund is deposited to the owner (keeper, sound) and shown as floating text.
//   * PillageModule (module size 0x1C): table RW 0xC068B8: PillageAmount (+8, unsigned), NumDamageEventsPerPillage (+0xC, unsigned), PillageFilter (+0x10, ObjectFilter).
//     The damage-dealt handler (RW 0x88826E, argument the victim): when the victim satisfies PillageFilter (asked with no second player) the counter (module + 0x18) grows; when it
//     reaches NumDamageEventsPerPillage it is cleared and min(PillageAmount, the victim owner's cash) is withdrawn from the victim's owner (keeper) and deposited to this
//     object's owner (keeper); a withdrawal of 0 stops there.

#pragma once

#include "Common/INI.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/DieModule.h"

#include <memory>
#include <string>

struct ObjectFilter;
class Object;

struct DieMuxData
{
	std::uint32_t m_deathTypes = 0xFFFFFFFFu;
	ObjectStatusMaskType m_exemptStatus{};
	ObjectStatusMaskType m_requiredStatus{};
	float m_damageAmountRequired = -1.0f;
	float m_minKillerAngle = 1.0f;
	float m_maxKillerAngle = -1.0f;
	// RW 0x8D29A9 (see above); false + `*angleNotPorted` set when the angle window would have been consulted
	bool isDieApplicable(const Object &obj, const DieModuleInterface::Event &event, bool *angleNotPorted = nullptr) const;
};

class RefundDieModuleData : public ModuleData
{
public:
	DieMuxData m_dieMux;                                       // RW 0xC76BD8
	std::string m_upgradeRequired;                             // +0x38
	float m_refundPercent = 0.0f;                              // +0x3C
	std::shared_ptr<const ObjectFilter> m_buildingRequired;    // +0x40
	static void buildFieldParse(MultiIniFieldParse &p);
};

class RefundDie : public BehaviorModule, public DieModuleInterface
{
public:
	OBFME_MODULE_XFER(RefundDie) // lane SAVE-4b: BehaviorModule's (no state)
	RefundDie(Thing *thing, const RefundDieModuleData *data);
	DieModuleInterface *getDie() override { return this; }
	void onDie(const DieModuleInterface::Event &event) override;
	// the refund this die would pay now (0 when a condition fails): RW 0x8885BF .. 0x8885E4, exposed for tests
	int refundAmount(const DieModuleInterface::Event &event) const;
	const RefundDieModuleData *data() const { return m_data; }

private:
	const RefundDieModuleData *m_data;
};

class PillageModuleData : public ModuleData
{
public:
	std::uint32_t m_pillageAmount = 0;                         // +8
	std::uint32_t m_numDamageEventsPerPillage = 0;             // +0xC
	std::shared_ptr<const ObjectFilter> m_pillageFilter;       // +0x10 (null = unset)
	static void buildFieldParse(MultiIniFieldParse &p);
};

class PillageModule : public BehaviorModule
{
public:
	OBFME_MODULE_XFER(PillageModule) // lane SAVE-4b
	PillageModule(Thing *thing, const PillageModuleData *data);
	// RW 0x88826E: the damage pipeline calls this for every hit this object's weapons land on `victim`
	void onDamageDealt(Object &victim);
	unsigned counter() const { return m_counter; }
	const PillageModuleData *data() const { return m_data; }
	void crc(StateHasher &hasher) const override;

private:
	const PillageModuleData *m_data;
	unsigned m_counter = 0; // RW module + 0x18
};

// the data of the crate collide (the runtime is SalvageCrateCollide below)
class SalvageCrateCollideModuleData : public ModuleData
{
public:
	KindOfMaskType m_requiredKindOf{};   // +8
	KindOfMaskType m_forbiddenKindOf{};  // +0x24
	bool m_forbidOwnerPlayer = false;    // +0x40
	bool m_buildingPickup = false;       // +0x41
	bool m_humanOnly = false;            // +0x42
	std::string m_pickupScience;         // +0x44
	std::string m_executeFX;             // +0x48
	std::string m_executeAnimation;      // +0x4C
	float m_executeAnimationTime = 0.0f; // +0x50
	float m_executeAnimationZRise = 0.0f; // +0x54
	bool m_executeAnimationFades = false; // +0x58
	float m_porterChance = 0.0f;         // +0x5C
	float m_bannerChance = 0.0f;         // +0x60
	float m_levelUpChance = 0.0f;        // +0x64
	float m_levelUpRadius = 0.0f;        // +0x68
	float m_resourceChance = 0.0f;       // +0x6C
	int m_minResource = 0;               // +0x70
	int m_maxResource = 0;               // +0x74
	std::string m_upgrade;               // +0x78
	bool m_allowAIPickup = false;        // +0x7C
	static void buildFieldParse(MultiIniFieldParse &p);
};

// SalvageCrateCollide (lane MODGAP-1, P-10, stop S-260): the treasure chests of the maps, of the tribute spells and of the creeps' lairs.
// TARGET FACTS (RotWK game.dat, caveat S-001; BFME2 decomp SalvageCrateCollide.cpp for the names, whose bodies are summaries only):
//   * CrateCollide::onCollide RW 0x8C06B6 (self = the crate, other = the toucher): isValidToExecute (vslot 0x34) then executeCrateBehavior (vslot 0x30); when that
//     answers yes: ExecuteFX (data + 0x48) plays on `other` (FXList::doFXObj RW 0x4B1B5A), the module's executed byte (module + 0x14) is set, the crate is destroyed
//     (GameLogic::destroyObject RW 0x62BBAB), and an ExecuteAnimation name (data + 0x4C) is shown at the crate by the client (RW 0x6A1875, not ported: reported);
//   * isValidToExecute, the base RW 0x8C080B: false for no `other`, an executed crate, an `other` of the neutral player (RW 0x68B760), an `other` without an AI unless
//     BuildingPickup and `other` is a STRUCTURE, a CREEP whose player is not a playable side (RW 0x6AAC66), an `other` that fails RequiredKindOf / ForbiddenKindOf
//     (isKindOfMulti RW 0x70C4FE), a dead `other` (+0x458 bit 0), a crate above the terrain (RW 0x4B12E8) unless BuildingPickup and `other` is a STRUCTURE, ForbidOwnerPlayer
//     with the crate and `other` of one player, HumanOnly with a computer `other` player (player + 0x5C), a PickupScience the player of `other` has not (RW 0x6AC207),
//     and a crate whose flag byte + 0x458 bit 1 (the undetected defector, never set: S-141) is set. SalvageCrateCollide adds (RW 0x8C145D): `other` has an AI
//     (object + 0x260) and is not IMMOBILE (template KindOf bit 2);
//   * the outcome RW 0x8C136C draws GameLogicRandomValueReal(0, 1): below LevelUpChance (+0x64) is 2 (a rank), else 4 with an Upgrade name (+0x78) and 3 without. The
//     branch of RW 0x8C168D (the outcomes 0 and 1: units of `other`'s player within LevelUpRadius each get a level, money when none) is not reachable in 2.01: the outcome
//     never is 0 or 1 (INFERENCE from the function; it is not ported);
//   * executeCrateBehavior RW 0x8C158B: false for a computer player's `other` unless AllowAIPickup (+0x7C); outcome 2: the experience tracker of `other` gets one level
//     (gainExpForLevel(1, true, false), RW 0x79DA0A) and the client text GUI:GainRank at the crate (RW 0x8C13BB; not ported: reported); outcome 4: the player upgrade named
//     by Upgrade (a PLAYER type, RW 0x66F5E5 / 0x6AEE22(upgrade, COMPLETE, 0)) is given to the player of `other`; otherwise doMoney. Always true afterwards;
//   * doMoney RW 0x8C148B: amount = MinResource, or GameLogicRandomValue(Min, Max) when they differ; nothing for an amount <= 0; in a multiplayer game
//     amount = ftol(amount * MultiPlayMoneyMult[livePlayable - 1]) (RW 0x625456 / 0x6A8630 / 0x642002, as the bounty); the handicap (RW 0x6AA858); the deposit to
//     the player of `other` (score keeper, RW 0x7B18B8) and the floating text GUI:AddCash 10 above the crate in that player's colour (RW 0x8C13BB);
//   * xfer RW 0x8C0760: version 2, the base collide module, and from version 2 the executed byte.
// PorterChance / BannerChance / ResourceChance (+0x5C / +0x60 / +0x6C) are read by none of the functions above (INFERENCE: unused in 2.01).
// INFERENCE (stop S-581): the touches come from AIWorld::processCollisions' static contacts (planar circles of the crate and of each mobile AI unit, units in id
// order), not from RW's partition / physics contacts (RW 0x62E93B).
class SalvageCrateCollide : public BehaviorModule, public CollideModuleInterface
{
public:
	OBFME_MODULE_XFER(SalvageCrateCollide)
	SalvageCrateCollide(Thing *thing, const SalvageCrateCollideModuleData *data);
	CollideModuleInterface *getCollide() override { return this; }
	void onCollide(Object *other, const Coord3D *loc, const Coord3D *normal) override; // RW 0x8C06B6
	bool wouldLikeToCollideWith(const Object &other) const override { return isValidToExecute(&other); } // RW 0x8C0664 (the crate's isValidToExecute)
	bool touchesMobileUnits() const override { return true; }
	void crc(StateHasher &hasher) const override;

	bool isValidToExecute(const Object *other) const; // RW 0x8C145D
	bool executed() const { return m_executed; }
	const SalvageCrateCollideModuleData *data() const { return m_data; }
	// the stop line S-260 (what is not ported)
	static const char *stopLine();

private:
	bool crateIsValid(const Object &other) const; // the base, RW 0x8C080B
	int chooseOutcome() const;                    // RW 0x8C136C
	bool executeCrateBehavior(Object &other);     // RW 0x8C158B
	void doMoney(Object &other);                  // RW 0x8C148B

	const SalvageCrateCollideModuleData *m_data;
	bool m_executed = false; // RW module + 0x14
};
