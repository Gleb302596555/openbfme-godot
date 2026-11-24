// OpenBFME. GPL-3.0. The four reward crate overrides of RW 0x8C06B6. Shared validity and consumption are CrateCollide's.
#pragma once

#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/Module/SalvageCrateCollide.h"

struct MoneyCrateData : CrateCollideModuleData
{
	unsigned moneyProvided = 0; // +5C, RW 0x65607D
	static void buildFieldParse(MultiIniFieldParse &p);
};
struct UnitCrateData : CrateCollideModuleData
{
	unsigned unitCount = 0; // +5C
	std::string unitType;   // +60
	static void buildFieldParse(MultiIniFieldParse &p);
};

// The four primary vtables have the same +0C (RW 0x8C1442, version 1 then the base) and +34 (RW 0x8C080B).
class RewardCrate : public CrateCollide
{
public:
	using CrateCollide::CrateCollide;
	void xfer(Xfer &x) override;
};
class HealCrateCollide final : public RewardCrate
{
public:
	using RewardCrate::RewardCrate;
	const std::type_info &xferClass() const override { return typeid(HealCrateCollide); }
	bool executeCrateBehavior(Object &collector) override;
};
class ShroudCrateCollide final : public RewardCrate
{
public:
	using RewardCrate::RewardCrate;
	const std::type_info &xferClass() const override { return typeid(ShroudCrateCollide); }
	bool executeCrateBehavior(Object &collector) override;
};
class MoneyCrateCollide final : public RewardCrate
{
public:
	MoneyCrateCollide(Thing *thing, const MoneyCrateData *data) : RewardCrate(thing, data), m_data(data) {}
	const std::type_info &xferClass() const override { return typeid(MoneyCrateCollide); }
	bool executeCrateBehavior(Object &collector) override;
private:
	const MoneyCrateData *m_data;
};
class UnitCrateCollide final : public RewardCrate
{
public:
	UnitCrateCollide(Thing *thing, const UnitCrateData *data) : RewardCrate(thing, data), m_data(data) {}
	const std::type_info &xferClass() const override { return typeid(UnitCrateCollide); }
	bool executeCrateBehavior(Object &collector) override;
private:
	const UnitCrateData *m_data;
};
