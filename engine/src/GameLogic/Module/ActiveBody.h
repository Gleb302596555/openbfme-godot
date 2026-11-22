// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
//
// ActiveBody (ZH Include/GameLogic/Module/ActiveBody.h, Source/GameLogic/Object/Body/ActiveBody.cpp), lane LOGIC-1: the health state of an
// object. Damage handling (attemptDamage, healing, the damage states' side effects) belongs to the combat lane; this port has the data, the
// constructor and the damage state calculation.
//
// TARGET FACTS (RotWK game.dat, caveat S-001):
//   * ActiveBodyModuleData: constructor RW 0x8C373F, field table RW 0xC71D68 (21 rows, below). Defaults: MaxHealth 0, InitialHealth -1.0
//     (RW 0xBD19DC), MaxHealthDamaged 0, MaxHealthReallyDamaged 0, DodgePercent 0, the three transition / recovery times 0,
//     UseDefaultDamageSettings true, GrabDamage 200.0 and CheerRadius 200.0 (RW 0xBE4170), flags false. The createData proc is RW 0x651186,
//     the create proc RW 0x65114B (an object of 0x100 bytes built by RW 0x8C3841).
//   * ActiveBody::ActiveBody (RW 0x8C3841): the initial health (+0x1C) and the max health (+0x20) are copied from the data; the damaged
//     fraction (+0x24) is MaxHealthDamaged * (1.0 / maxHealth) and the really damaged fraction (+0x28) MaxHealthReallyDamaged * (1.0 /
//     maxHealth) (SSE: the reciprocal first, then the products); the current health (+0x18) is the max health when InitialHealth == -1.0,
//     else the initial health, and the previous health (+0x2C) the same; with UseDefaultDamageSettings a fraction of 0 takes the
//     GlobalData value (RW 0xDE4364 + 0xB4 / + 0xB8 = UnitDamagedThreshold / UnitReallyDamagedThreshold, gamedata.ini); then the damage
//     state is set from the health (RW 0x8C2A5D).
//   * the damage state (RW 0x8C1B79, a function of the current health): 0 health = RUBBLE (3); reallyDamagedFraction * maxHealth >= health =
//     REALLYDAMAGED (2); damagedFraction * maxHealth >= health = DAMAGED (1); else PRISTINE (0).
//   * which GlobalData fields +0xB4 / +0xB8 are is INFERENCE (the INI names UnitDamagedThreshold / UnitReallyDamagedThreshold; the offsets
//     are not tied to the names): stop S-149.
// RecoveryTime is read by attemptDamage exactly where RW 0x8C425D does and handed to the empty RW 0x9F3A3C (see scheduleRecovery below): retail keeps no
// state from it, and neither does the port.
// Not ported: the transition times' consumers, DodgePercent, the attribute modifiers, the
// burning death behaviour, CheerRadius: the fields are parsed and kept (the FX names unresolved: the store is the client's, stop S-149);
// PORT-78 reads the Grab fields on the existing vendor grab path (RW 0x85415C).
// The body's last-healing timestamp (PORT-293): module + 0xC0, the body interface's slot 0x48 (RW 0x5DC48C `mov eax, [ecx + 0xB0]; ret`). The object's
// constructor ORs it with -1 (RW 0x8C3897) and attemptHealing stamps the logic frame on every hit it applies (RW 0x8C30A9 .. 0x8C30BA, before the
// health-up test of RW 0x8C30B1). Its one reader is AutoHealBehavior's NonStackable rule (RW 0x855777 .. 0x8557A2: the stamp plus the module's HealingDelay
// against the current frame, unsigned). A body class without the writer answers the shared `or eax, -1` stub (InactiveBody RW 0x48DACD).
// The body's veterancy notify (RW 0x8C261D, module vtable 0xC71B78 slot 0x30) is not ported: stop S-7521 -- its only caller is the experience lane's
// Object::onVeterancyLevelChanged (RW 0x6957B9, the single `+ 0x25C` site that calls slot 0x30, RW 0x6957FD), and the ratio it stores through body slot 0x5C
// needs the four GameData rows + 0xAD4 .. 0xAE0 (HealthBonus_Regular / _Veteran / _Elite / _Heroic) the port does not parse.
// DamageCreationList (PORT-45): the parser RW 0x8C3F1F keeps the three tokens as a 12-byte record {OCL, FXTrigger index, death-side index};
// doDamageFX creates the record whose FXTrigger matched the hit's and whose death side is 0 (RW 0x8C2F75 .. 0x8C2FB8). The other reader,
// the structure's damage-side branch (RW 0x8C2EBD .. 0x8C2EEF, death sides 1-4 for FRONT / RIGHT / BACK / LEFT destroyed), has no ported
// equivalent because the damage split over a structure's sides is not ported: stop S-149.

#pragma once

#include "Common/INI.h"
#include "Common/INIDataTypes.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BehaviorModule.h"

#include <string>
#include <vector>

class ArmorTemplate;

class ActiveBodyModuleData : public ModuleData
{
public:
	ActiveBodyModuleData() = default;

	// RW 0xC71D68, in the binary's order
	float m_maxHealth = 0.0f;
	float m_initialHealth = -1.0f;
	float m_maxHealthDamaged = 0.0f;
	float m_maxHealthReallyDamaged = 0.0f;
	unsigned m_recoveryTime = 0;
	float m_dodgePercent = 0.0f;
	unsigned m_enteringDamagedTransitionTime = 0;
	unsigned m_enteringReallyDamagedTransitionTime = 0;
	std::string m_grabObject;
	std::string m_grabFX;
	float m_grabDamage = 200.0f;
	Coord2D m_grabOffset;
	bool m_useDefaultDamageSettings = true;
	// PORT-45: RW parser 0x8C3F1F appends one 12-byte record to the vector at module data +0x58: the ObjectCreationList
	// (RW 0x73A368 lookup, NULL for NONE / an unknown name), the damage index (scanIndexList over RW 0xDB2AF4: 0 NONE,
	// 1 CATAPULT_ROCK, 2 TREBUCHET_ROCK) and the death-side index (scanIndexList over RW 0xDB2B04: 0 NONE, 1 FRONT_DESTROYED,
	// 2 RIGHT_DESTROYED, 3 BACK_DESTROYED, 4 LEFT_DESTROYED). The second token is an FXTrigger (the Weapon field's list,
	// RW 0xDA14FC), not a DamageType.
	struct DamageCreation
	{
		std::string m_oclName;               ///< record +0x00 as the name; NULL (NONE / unknown) is an empty name
		unsigned m_fxTrigger = 0;            ///< record +0x04, compared with DamageInfo input's FXTrigger
		unsigned m_deathSide = 0;            ///< record +0x08, the damage stage the side branch (RW 0x8C2EC8) looks for
	};
	std::vector<DamageCreation> m_damageCreationList; ///< one entry per `DamageCreationList` line, in file order
	std::string m_healingBuffFx;
	bool m_healingBuffFxSet = false; ///< lane OWNERFIX-1: a HealingBuffFx row was read (else the GameData default, RW 0x8C373F)
	std::string m_damagedAttributeModifier;
	std::string m_reallyDamagedAttributeModifier;
	float m_cheerRadius = 200.0f;
	bool m_removeUpgradesOnDeath = false;
	bool m_burningDeathBehavior = false;
	std::string m_burningDeathFX;

	static void buildFieldParse(MultiIniFieldParse &p);
};

// RW primary vtable 0xC71B78: scalar deleting dtor RW 0x008C3B6D (DONOR BFME2 0x004BF9D1), loadPostProcess RW 0x008C35AF (DONOR BFME2 0x004BF40F; S-7408 / S-342),
// getModuleName RW 0x008C3A2D (DONOR BFME2 0x004BF88D), getModuleNameKey RW 0x008C39E8 (DONOR BFME2 0x004BE1F0), isArmorOverridden RW 0x008C3A48 (DONOR BFME2 0x004BF8AC),
// clearLastAttacker RW 0x008C3A89 (DONOR BFME2 0x004BF8ED). Body vtable 0xC720F0: getFrontCrushed RW 0x00932519 / getBackCrushed RW 0x008C3A53 (only caller CrushDie, S-1242);
// setIndestructible RW 0x8C3326 -> BridgeBehavior::getBridgeBehaviorInterfaceFromObject RW 0x008595BA (S-161 / S-1282); onVeterancyLevelChanged promotion sounds
// RW 0x008C22F2 / 0x008C230B / 0x008C2324 (S-7521); per-side damage sort helper RW 0x00970ED8 (S-149).
class ActiveBody : public BehaviorModule, public BodyModuleInterface
{
public:
	ActiveBody(Thing *thing, const ActiveBodyModuleData *data);
	~ActiveBody() override = default; // RW 0x008C3AED (~ActiveBody), RW 0x008C3B6D (scalar deleting dtor, DONOR BFME2 0x004BF9D1)

	BodyModuleInterface *getBody() override { return this; }
	// PORT-293: the interface's getters, each its own RW function (table RW 0xC720F0; the interface `this` is module + 0x10, as RW 0x8C1C51's
	// `lea ecx, [esi - 0x10]` shows). 41 sites call the health getter, 58 the max-health one, 27 the damage state
	float getHealth() const override { return m_currentHealth; }        ///< RW 0x5F2D41 (slot 0x10, `fld [ecx + 8]` = module + 0x18)
	float getMaxHealth() const override { return m_maxHealth; }         ///< RW 0x6AA274 (slot 0x1C, `fld [ecx + 0x10]` = module + 0x20)
	float getInitialHealth() const override { return m_initialHealth; }
	float getPreviousHealth() const { return m_previousHealth; }        ///< RW 0x49449C (slot 0x20, `fld [ecx + 0x1C]` = module + 0x2C)
	// lane HERO-1 (Sol review): the reference the revive scales (RW 0x8C47DE reads body interface + 0x1C = module + 0x2C; RW 0x8C1CD5 setMaxHealth stores newMax
	// there). Distinct from the previous health internalChangeHealth keeps (RW 0x8C31A5 writes interface + 0x0C); it starts at the actual initial health
	float getReviveReference() const { return m_reviveReference; }
	BodyDamageType getDamageState() const override { return calcDamageState(); }
	void applyInitialDamageState() override { updateDamageState(true); }
	// Body interface RW 0x8C1D53: apply the stored damage state even when health did not change (e.g. construction completion).
	void evaluateVisualCondition();
	void resetDamageState(int state = 0) override; // RW 0x008C1BF9 (DONOR BFME2 0x004BDAA9)
	void resetHealthToDamageState() override; // RW 0x008C1C75 (DONOR BFME2 0x004BDB25)
	void setInitialHealth(int initialPercent) override;
	// lane SUMMON-1: RW 0x8C47DE (vt[0x58]) with its second argument 0 (the ObjectCreationList's health)
	void setInitialHealthPercent(float percent) override;
	const ActiveBodyModuleData *data() const { return m_data; }

	// ---- lane COMBAT-1: the damage pipeline (RW 0x8C3FA3; scratch/weapon1/damage.md 2.7) ----
	void attemptDamage(DamageInfo &info) override;
	void setIndestructible(bool on) override { m_indestructible = on; } // lane SCRIPT-2 (RW 0x8C3326)
	bool isIndestructible() const override { return m_indestructible; } // RW 0x008C3A74 (body slot 35 +0x8C, DONOR BFME2 0x004BF8D8)
	void attemptHealing(DamageInfo &info) override;
	// PORT-2 (RecoveryTime): RW 0x8C425D .. 0x8C4276, inside attemptDamage between the health change (branch 16) and the record (branch 18). The data's
	// +0x24 (field row RW 0xC71DA8, parser RW 0x73A429) is read, a zero value skips the call (RW 0x8C4265), and `current frame (TheGameLogic + 0x40) +
	// RecoveryTime` (RW 0x8C4267 .. 0x8C4270) goes to `call 0x9F3A3C` (RW 0x8C4276), the image's shared EMPTY function (`ret 4`; STOPS.md S-113 cites the
	// same address as `W3DBuffDraw::doDrawModule`'s empty slot). Nothing stores the deadline and nothing reads it: no state, no hash, no save. Every body
	// class that reaches the shared function (DelayedDeathBody, RespawnBody, HighlanderBody, ImmortalBody, StructureBody, HiveStructureBody,
	// PorcupineFormationBodyModule) therefore computes the same deadline and drops it, as retail does
	void scheduleRecovery();
	// the test observer of scheduleRecovery: calls whose RecoveryTime was non-zero, and the last deadline computed (never hashed, never saved)
	static unsigned long long &recoverySchedules()
	{
		static unsigned long long n = 0;
		return n;
	}
	static UnsignedInt &lastRecoveryDeadline()
	{
		static UnsignedInt frame = 0;
		return frame;
	}
	// TOP20 #21 (ActiveBody.cpp): the burning death's cap test (RW 0x8C4169 .. 0x8C41DD) and the capped hit's tail (RW 0x8C476C .. 0x8C47BD)
	bool burningDeathCaps(const Object &obj, int damageType, const Object *container, const class ContainModuleInterface *hordeHelper) const;
	void startBurningDeath(class ContainModuleInterface *hordeHelper);
	float estimateDamage(const DamageInfoInput &input) const override;
	float getDodgePercent() const override { return m_data->m_dodgePercent; } // RW 0x008C1DAD (body slot 36 +0x90, DONOR BFME2 0x004BDC41)
	std::string getGrabObjectName() const override { return m_data->m_grabObject; } // RW 0x8C281E (lane BACKLOG-1)
	const std::string *getGrabFXName() const override { return &m_data->m_grabFX; } // RW 0x8C1DB4
	float getGrabDamage() const override { return m_data->m_grabDamage; } // RW 0x8C1DBB
	Coord2D getGrabOffset() const override { return m_data->m_grabOffset; } // RW 0x8C1DC2
	// lane XP-1: RW 0x8C1CD5
	void setMaxHealth(float newMax, int changeType) override;
	// RW 0x8C31A5 internalChangeHealth (Immortal overrides it, RW 0x8C4AB4)
	virtual void internalChangeHealth(float delta);
	// lane BUILD-2: RW body vslot 0xAC (RW 0x5015C6): stores the current health, nothing else (GettingBuiltBehavior::startConstruction puts the health back with it)
	void friend_setHealthRaw(float health) { m_currentHealth = health; }
	// the armour of the object's current armor set: the ArmorTemplate named by the best ArmorTemplateSet (RW 0x5D893C looks the name up per hit); null when the template has
	// no ArmorSet; an armour name the store lacks is reported once (a logic error, PLAN rule 10) and counts as no armour
	const ArmorTemplate *currentArmor() const;
	// RW 0x008C3A48 (primary slot 14 +0x38, DONOR BFME2 0x004BF8AC -> RW 0x005D891E ArmorStore::isOverridden)
	bool isArmorOverridden() const;
	// ZH BodyModuleInterface::setDamageStateIgnoringBodyScalar: the body scalar (RW this + 4, multiplied in by body slot 0x6C): 1.0 by default
	float bodyDamageScalar() const { return m_damageScalar; }
	void setBodyDamageScalar(float s) { m_damageScalar = s; }
	void applyDamageScalar(float scalar) override; // RW 0x8C329F (vt[0x6C])
	// the last frame the body took a damaging hit and from whom (RW + 0xAC, the last damage record); 0xFFFFFFFF / INVALID_ID when never
	UnsignedInt lastDamageFrame() const override { return m_lastDamageFrame; }
	ObjectID lastDamager() const { return m_lastDamager; }
	// PORT-293: body interface vslot 0x48 (RW 0x5DC48C: `mov eax, [ecx + 0xB0]; ret` = module + 0xC0) -- the frame of the last healing hit, -1 while the
	// body was never healed (the object's constructor ORs the dword with -1, RW 0x8C3897). attemptHealing stamps the current logic frame on every hit it
	// applies (RW 0x8C30A9 .. 0x8C30BA); AutoHealBehavior's NonStackable rule reads it (RW 0x855797)
	UnsignedInt getLastHealingFrame() const override { return m_lastHealingFrame; }
	// lane PLAY-4: body interface vslot 0x4C (RW 0x8C3A33): the last damage record's source once per record (+ 0xB6 set by the first call, cleared by the
	// next damaging hit, RW 0x8C42FA); INVALID_ID when already taken (ZH getClearableLastAttacker + clearLastAttacker in one call). The guard machine's
	// "attacked me" condition (RW 0x949994) asks it
	ObjectID takeLastAttacker()
	{
		if (m_lastAttackerTaken)
		{
			return INVALID_ID;
		}
		m_lastAttackerTaken = true;
		return m_lastDamager;
	}
	// RW 0x008C3A89 (primary slot 16 +0x40, DONOR BFME2 0x004BF8ED): sets m_lastAttackerTaken (+0xC6) to true
	void clearLastAttacker() { m_lastAttackerTaken = true; }
	// lane CAMP-1: the last damage record's source player mask (RW body vslot 0x40 record + 0xC, DamageInfo::m_sourcePlayerMask; TEAM_ATTACKED_BY_PLAYER RW 0x7E7361)
	std::uint32_t lastDamagerPlayerMask() const { return m_lastDamagerPlayerMask; }
	// lane FX-4: body interface slot 0x40 (RW 0x715914: the last damage record, ActiveBody + 0x40, never null) and its damage type (+ 0x10; the record's
	// constructor RW 0x66341C: 0x16 = UNDEFINED)
	bool lastDamageRecordType(int &type) const override
	{
		type = m_lastDamageType;
		return true;
	}
	bool lastDamageRecordNoEffect(bool &noEffect) const override
	{
		noEffect = m_lastDamageNoEffect;
		return true;
	}
	bool lastDamageNoEffect() const { return m_lastDamageNoEffect; }
	// lane COMBAT-2: the damage state the body last applied (RW + 0x30, written by setCorrectDamageState RW 0x8C2A5D)
	BodyDamageType appliedDamageState() const { return m_curDamageState; }
	void crc(StateHasher &hasher) const override;
	OBFME_MODULE_XFER(ActiveBody) // lane SAVE-4b (OpenBFME native, S-4190)
	// TOP20 #20: RW 0x8C1E18, primary vslot 8 (see ActiveBody.cpp)
	void onDelete() override;
	// TOP20 #20: RW 0x8C3B89, primary vslot 0x58: the enemies within CheerRadius are asked to cheer (see ActiveBody.cpp)
	void cheer();
	static bool isCheerExempt(const Object &obj);
	// PORT-66 (issue #180): RW 0x8C1E4E, the body interface's slot 0x2C (table RW 0xC71ED0). The auto particle systems of a damaged or burning object: the
	// seven GameData entries (RW 0xC00150 .. 0xC00290 -> GlobalData +0x9F0 .. +0xA40) name a bone prefix and a template each; an AFLAME object (status 0xA)
	// swaps the templates the way RW 0x8C1FDD .. 0x8C202F does and doubles the cap. The bones are the template's (RW 0x68C650, model space); the client
	// makes and deletes the systems (FXEvent BODY_PARTICLE_SYSTEMS), as FX-4's TransitionDamageFX systems are the client's
	void updateBodyParticleSystems() override;

protected:
	// the DamageInfo attemptDamage is processing (RW passes it down to internalChangeHealth, RW 0x8C553F / 0x8C5828 read and change it); null outside a hit
	DamageInfo *currentDamageInfo() const { return m_currentInfo; }
	// RW 0x8C2A5D setCorrectDamageState (called by internalChangeHealth RW 0x8C31A5 through vslot 0x54): recompute the damage state from the health; when it changed apply what the
	// state means to the object: the model conditions DAMAGED / REALLYDAMAGED / RUBBLE and, for a STRUCTURE entering or leaving RUBBLE, the rubble effects (see ActiveBody.cpp)
	// `initial`: the first application after the object was built (no position yet: the pathfinder footprint is registered later by whoever places the object, and sees the state)
	void updateDamageState(bool initial = false);
	// lane FX-4: the damage modules hear of a damage state change (RW 0x8C4395 / 0x8C3139)
	void notifyDamageStateChange(const DamageInfo *info, BodyDamageType oldState, BodyDamageType newState);
	float m_currentHealth = 0.0f;  // RW +0x18
	float m_previousHealth = 0.0f; // the previous health of internalChangeHealth (damage accounting)
	float m_reviveReference = 0.0f; // lane HERO-1: RW module + 0x2C (see getReviveReference)

private:
	class DamageHost;
	BodyDamageType calcDamageState() const;
	void doDamageFX(const DamageInfo &info); // RW 0x8C2F02 (lane FX-2)

	const ActiveBodyModuleData *m_data;
	float m_initialHealth = 0.0f;  // RW +0x1C
	float m_maxHealth = 0.0f;      // RW +0x20
	float m_damagedFraction = 0.0f;        // RW +0x24
	float m_reallyDamagedFraction = 0.0f;  // RW +0x28
	float m_damageScalar = 1.0f;   // RW this + 4 (the body's damage scalar)
	bool m_indestructible = false; // RW + 0xC7 (lane SCRIPT-2: the map's objectIndestructible, UNIT_AFFECT_OBJECT_PANEL_FLAGS)
	BodyDamageType m_curDamageState = BODY_PRISTINE; // RW + 0x30
	DamageInfo *m_currentInfo = nullptr;
	UnsignedInt m_lastDamageFrame = 0xFFFFFFFFu; // RW + 0xAC (RW 0x8C3841: -1 = never; BUILD-2)
	UnsignedInt m_lastHealingFrame = 0xFFFFFFFFu; // PORT-293: RW + 0xB0 (RW 0x8C3897: -1 = never; attemptHealing's stamp RW 0x8C30BA)
	ObjectID m_lastDamager = INVALID_ID;
	bool m_lastAttackerTaken = false; // RW + 0xB6 of the body interface (lane PLAY-4)
	std::uint32_t m_lastDamagerPlayerMask = 0; // lane CAMP-1: the record's + 0xC
	int m_lastDamageType = DAMAGE_UNDEFINED;   // lane FX-4: the record's + 0x10
	bool m_lastDamageNoEffect = false;         // RW 0x744203: DamageInfo + 0x78
	mutable bool m_armorReported = false;
	// lane FX-2: the damage FX throttle of RW 0x8C2F02 (+0x3C the last DamageFX type played, +0x38 the frame until which that type stays throttled). Only the
	// client output depends on them; not hashed (the event queue boundary of FXEvents.h)
	int m_lastDamageFXType = 0;           // RW + 0x3C
	UnsignedInt m_damageFXThrottleUntil = 0; // RW + 0x38
};

// RW vtable 0xC71FE0: getModuleName RW 0x008C4A34, getModuleNameKey RW 0x008C4A53, dtor RW 0x008C4A98;
// health never drops below 1 (internalChangeHealth delta = max(delta, 1 - health), RW 0x8C4AB4)
class ImmortalBody : public ActiveBody
{
public:
	using ActiveBody::ActiveBody;
	~ImmortalBody() override = default; // RW 0x008C4A98
	void internalChangeHealth(float delta) override;
	OBFME_MODULE_XFER(ImmortalBody) // lane SAVE-4b: ActiveBody's (no state of its own)
};

// RW vtable 0xC71ED0: getModuleName RW 0x008C4947, getModuleNameKey RW 0x008C4966, dtor RW 0x008C49AB;
// attemptDamage RW 0x8C49C7: a hit never takes more than health - 1 unless UNRESISTABLE
class HighlanderBody : public ActiveBody
{
public:
	using ActiveBody::ActiveBody;
	~HighlanderBody() override = default; // RW 0x008C49AB
	void attemptDamage(DamageInfo &info) override;
	OBFME_MODULE_XFER(HighlanderBody) // lane SAVE-4b: ActiveBody's (no state of its own)
};
