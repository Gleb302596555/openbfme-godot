// OpenBFME. GPL-3.0. GiveUpgradeUpdate, both editions.
// Target search RW 0x89FEBD / BFME2 Rva 0x49C73A; update RW 0x8A00C2 /
// BFME2 Rva 0x49C93F; delivery RW 0x8A01B2 / BFME2 Rva 0x49CA2F.
#pragma once
#include "GameLogic/Module/SpecialAbilityModules.h"
class UpgradeTemplate;

class GiveUpgradeUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	std::string m_giveFX, m_spawnFX;
	float m_fadeSpeed = 0.025f;
	bool m_deliver = false;
	static void buildFieldParse(MultiIniFieldParse &p);
};

class GiveUpgradeUpdate : public SpecialAbilityUpdate
{
public:
	OBFME_MODULE_XFER(GiveUpgradeUpdate)
	GiveUpgradeUpdate(Thing *thing, const GiveUpgradeUpdateModuleData *data);
	void onObjectCreated() override;
	UpdateSleepTime update() override;
	void finishAbility(bool ownerDying, bool aborted) override;
	void crc(StateHasher &h) const override;
	static void registerClass(ModuleFactory &factory);
	static const char *stopLine();
protected:
	void triggerAbilityEffect() override;
private:
	const UpgradeTemplate *upgrade() const;
	void findDeliveryTarget();
	void releaseTarget();
	const GiveUpgradeUpdateModuleData *m_giveData;
	bool m_fading = false, m_searching = false;
	float m_opacity = 1.0f;
};
