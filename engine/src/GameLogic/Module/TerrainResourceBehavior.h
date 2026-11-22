// OpenBFME. GPL-3.0.
//
// TerrainResourceBehavior (RotWK ModuleFactory name "TerrainResourceBehavior", create proc RW 0x64BF55, data create proc RW 0x64BF8D, module class vtable RW
// 0xC5FC94, interface mask 0xB = UPDATE | DIE | CREATE; BFME-new, ZH has nothing like it), lane ECON-1: the module of every resource building (farm, mill,
// mine, ...) and of the "dead spot" every other building carries (MaxIncome 0, IncomeInterval 999999, HighPriority Yes). It claims ground from the
// TerrainResourceManager (GameLogic/System/TerrainResourceManager.h) and, every IncomeInterval, pays its owner MaxIncome scaled by the share of its
// ground it holds.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; B2 Open-BFME-2 TerrainResourceBehavior*.cpp agree on the layout and the data defaults):
//   * data (size 0x24, ctor RW 0x88525D, field table RW 0xC5FD78): Radius (parseReal, +8, default 0), MaxIncome (parseInt, +0xC, 0), IncomeInterval
//     (parseDurationUnsignedInt, +0x10, default 0xFFFFFFFF), HighPriority (+0x14, false), Visible (+0x15, true), UpgradeMustBePresent (ObjectFilter, +0x18, unset),
//     Upgrade (parseUpgradeTemplate, +0x1C, none), UpgradeBonusPercent (parsePercentToReal, +0x20, default 1.0 = RW 0xBD1908).
//   * module (size 0x30, ctor RW 0x8852C9): the UpdateModule base, a Die interface at +0x20 (vtable RW 0xC5FBC8), a Create interface at +0x24 (RW 0xC5FBB8);
//     +0x28 claimed (false), +0x29 needToRunOnBuildComplete (true), +0x2C the claimed share of its ground (float, 0.0); the constructor wakes it
//     (setWakeFrame(object, UPDATE_SLEEP_NONE), RW 0x850C32).
//   * onCreate does nothing. onBuildComplete (RW 0x8853A0): setWakeFrame(NONE); needToRunOnBuildComplete = false; when not claimed yet: claim and set
//     claimed; then TerrainResourceManager::onBuildComplete(object) (the finished building turns its shared ground into owned ground).
//   * onDie (RW 0x885380 / 0x885362): TerrainResourceManager::unclaim(object, data.Radius).
//   * update (RW 0x8854D3, always called with the object's disabled mask handled by the scheduler):
//       if not claimed: claim(object, Radius, Visible, HighPriority), claimed = true, and when needToRunOnBuildComplete (still under construction) return
//         UPDATE_SLEEP_FOREVER (RW returns 0x3FFFFFFF: the building sleeps until onBuildComplete wakes it);
//       owner = the object's controlling player; none, or a player without a PlayerTemplate: return IncomeInterval;
//       bonus = 1.0; when Upgrade is set and the owner has completed it and the owner has an object that satisfies UpgradeMustBePresent (Player::hasObjectMatching
//         with completedOnly = true): bonus = UpgradeBonusPercent;
//       modifier = 1.0 * the product of the object's active PRODUCTION attribute modifiers (type 0xD; RW 0x68C82D: out = 1.0 first, then the product);
//       resourceFactor = 1.0, or when the owner's PlayerTemplate has a ResourceModifierObjectFilter (valid) that allows this object: n = the number of the owner's
//         live objects that the filter allows (RW 0x6ABABD with the callback RW 0x885230: an object under construction (status bit 2) is skipped; the filter asked
//         with no second player); resourceFactor = values[n] * 0.01f when n < values.size(), else values.back() * 0.01f - (n - values.size()) * 0.02f clamped at 0
//         (all SSE: cvtsi2ss, mulss, subss);
//       amount = ceil(MaxIncome * share * modifier * bonus) with the x87 chain fild; fmul share; fmul modifier; fmul bonus (each at 24 bits), the MSVCR71 ceil of the
//         double, stored as float, fistp;   when amount > 0: amount = ceil(amount * resourceFactor) the same way (x87 fild, fmul, ceil, fstp, fistp), at least 1;
//         owner.money.deposit(amount, owner's score keeper, sound) (RW 0x7B18B8) and the floating "+amount" text over the building is a client event (EconomyEvents);
//         whether or not anything was paid, the object's experience tracker (when it has one and it is trainable) gets (float)amount experience points (RW 0x79D833:
//         a hook, stop S-256);
//       returns IncomeInterval (the next payment in that many frames).
//   * the share is written by the TerrainResourceManager (RW 0x5F2D53: module + 0x2C). A module that was never given one pays 0.
// Differences: the first matching module of the class is the one the manager finds (Object::findModule), exactly like RW's findModule by class name key: an
// object with two TerrainResourceBehavior modules gives the second one no share (it pays nothing).
//
// The rest of the class's own functions (Port 297, fork issue 689; every body read from its first instruction in game.dat 2.01):
//   * module vtable RW 0xC5FC94 (13 slots): slot 0 RW 0x885346 the scalar deleting destructor (RW 0x8851B7, then operator delete: not behaviour);
//     slot 1 loadPostProcess RW 0x88546B: the UpdateModule base (RW 0x4D0EAF, a plain ret) and, when the module has not claimed yet (+0x28 == 0),
//     setWakeFrame(object, UPDATE_SLEEP_NONE) (RW 0x850C32 with 1). The port has no module loadPostProcess (S-4190) and needs none here: the wake frame is
//     in the save (UpdateModule::xferUpdateModule), and an unclaimed module is one the constructor woke for the next frame and whose update has not run
//     yet, so the saved frame is the frame RW sets on the load (the logic frame + 1);
//     slot 2 getModuleName RW 0x8851DE (the literal "TerrainResourceBehavior", RW 0xC0BA9C) and slot 4 getModuleNameKey RW 0x8851EB (that literal's name
//     key, cached in a function static): Module::getModuleClassName / getModuleNameKey, set by the ModuleFactory;
//     slot 3 xfer RW 0x8853F9: UpdateModule::xfer (RW 0x850C60), version 2, claimed (+0x28, version 2 and later), needToRunOnBuildComplete (+0x29), the
//     share (+0x2C); a version 1 save gets claimed = !needToRunOnBuildComplete. The port's save is its own layout (S-4190) with the same three fields in
//     the same order (xfer below);
//     slot 8 onDelete RW 0x885362, slot 9 RW 0x8851E4 (a `ret 8` shared by many classes: nothing).
//   * the create interface (+0x24, vtable RW 0xC5FBB8): slot 0 onCreate and slot 2 RW 0x63F3BF (nothing), slot 1 onBuildComplete RW 0x8853A0, slot 3
//     RW 0x8851E7 shouldDoOnBuildComplete: the byte at +0x29 (needsBuildComplete() here). No retail caller asks it of this class: the object's
//     onBuildComplete loop RW 0x68D252 calls slot 1 of every create interface unconditionally (RW 0x68D263 getCreate, RW 0x68D26E slot 1), and the other
//     getCreate sites (RW 0x68B22B slot 2, RW 0x6D16FC slot 0, RW 0x86A18C slot 1) do not read slot 3.
//   * the behaviour interface (+0x0C, vtable RW 0xC5FBD8; the update interface is at +0x10, vtable RW 0xC5FBCC): slot 3 RW 0x8A18ED getCreate (the
//     interface's this - 0xC + 0x24 = the create interface, null for null; the object asks it at RW 0x6D16FA) and slot 7 RW 0x8A18E0 getDie (+0x20):
//     getCreate() / getDie() below.
//   * the data vtable RW 0xC5FCC8: slot 0 RW 0x885487 the data's scalar deleting destructor (RW 0x8854A3 frees the UpgradeMustBePresent filter at +0x18:
//     not behaviour); slot 14 RW 0x4B612F is the ModuleData base's (an empty AsciiString, RW 0xDC62B8; SupplyWarehouseDockUpdate's data has the same
//     slot): not this class's behaviour.

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpdateModule.h"

#include <memory>
#include <string>

struct ObjectFilter;

class TerrainResourceBehaviorModuleData : public ModuleData
{
public:
	float m_radius = 0.0f;                                            // +8
	int m_maxIncome = 0;                                              // +0xC
	std::uint32_t m_incomeInterval = 0xFFFFFFFFu;                      // +0x10 frames (RW default -1)
	bool m_highPriority = false;                                      // +0x14
	bool m_visible = true;                                            // +0x15
	std::shared_ptr<const ObjectFilter> m_upgradeMustBePresent;       // +0x18 (null = the handle -1)
	std::string m_upgrade;                                            // +0x1C ("" = none)
	float m_upgradeBonusPercent = 1.0f;                               // +0x20

	static void buildFieldParse(MultiIniFieldParse &p);
};

class TerrainResourceBehavior : public UpdateModule, public CreateModuleInterface, public DieModuleInterface
{
public:
	OBFME_MODULE_XFER(TerrainResourceBehavior) // lane SAVE-4b
	TerrainResourceBehavior(Thing *thing, const TerrainResourceBehaviorModuleData *data);

	CreateModuleInterface *getCreate() override { return this; } // RW 0x8A18ED (behaviour interface slot 3)
	DieModuleInterface *getDie() override { return this; }       // RW 0x8A18E0 (behaviour interface slot 7)
	UpdateSleepTime update() override;
	// CreateModuleInterface
	void onCreate() override {}
	void onBuildComplete() override;
	// DieModuleInterface
	void onDie(const DieModuleInterface::Event &event) override;
	// lane FBSIM-1: vtable RW 0xC5FC94 slot 8 (onDelete) RW 0x885362: the claim goes with the object too (a dozer's phantom removed before it is placed)
	void onDelete() override;

	// RW 0x5F2D53: the manager reports the share of the ground this module holds
	void setClaimShare(float share) { m_share = share; }
	float claimShare() const { return m_share; }
	bool claimed() const { return m_claimed; }
	bool needsBuildComplete() const { return m_needToRunOnBuildComplete; } // RW 0x8851E7: create interface slot 3, the byte at +0x29
	const TerrainResourceBehaviorModuleData *data() const { return m_data; }

	void crc(StateHasher &hasher) const override;

private:
	const TerrainResourceBehaviorModuleData *m_data;
	bool m_claimed = false;                 // +0x28
	bool m_needToRunOnBuildComplete = true; // +0x29
	float m_share = 0.0f;                   // +0x2C
};
