// OpenBFME. GPL-3.0.
// Shared HitTracker and EffectProbe rules for the retail FACT-B/C unit sweeps, Ennorath button matrix sweep, and LOCAL-78 synthetic tests.
// Both RotWK 2.01 (RW) and BFME2 1.06 rules apply here unless a cited rule differs.

#pragma once

#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/AI/AIHarvest.h"
#include "GameLogic/Armor.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/DeployStyleAIUpdate.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/ExtraDieModules.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Module/HordeDispatchSpecialPower.h"
#include "GameLogic/Module/ReplaceObjectUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/Module/StealthAbilityModules.h"
#include "GameLogic/Module/WeaponChangeSpecialPower.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/System/EmotionSystem.h"
#include "GameLogic/System/InvisibilityManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponNugget.h"
#include "GameLogic/WeaponState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace sweep_probe
{

inline std::string str(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return buf;
}

inline float dist2d(const Coord3D &a, const Coord3D &b)
{
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// the damage a weapon's hit can do to `victim`: per DamageNugget (its own, or its projectile's warhead's) Damage x the victim's armour coefficient for
// the nugget's DamageType (the INI's numbers: Weapon.ini, Armor.ini); the sum of them as one more candidate (a hit of every nugget at once)
inline std::vector<float> expectedHits(const WeaponTemplate &w, const Object &victim, const Player *attackerPlayer, std::string &text, const Object *attacker = nullptr)
{
	std::vector<float> out;
	const ActiveBody *body = dynamic_cast<const ActiveBody *>(victim.getBodyModule());
	const ArmorTemplate *armor = body ? body->currentArmor() : nullptr;
	float sum = 0.0f;
	std::vector<const WeaponTemplate *> weapons{ &w };
	// lane FACT-C: with the attacker known, a nugget its upgrades switch off (RequiredUpgradeNames / ForbiddenUpgradeNames: the spider riders'
	// forged-blade swords, the fire arrows' warhead) is not one of its hits
	auto off = [&](const WeaponNugget &n) {
		bool o = false;
		for (const std::string &u : n.m_requiredUpgradeNames)
		{
			o = o || (attacker && !attacker->hasUpgrade(u));
		}
		for (const std::string &u : n.m_forbiddenUpgradeNames)
		{
			o = o || (attacker && attacker->hasUpgrade(u));
		}
		return o;
	};
	for (const std::shared_ptr<WeaponNugget> &n : w.m_nuggets)
	{
		if (off(*n))
		{
			continue;
		}
		if (const ProjectileNugget *p = dynamic_cast<const ProjectileNugget *>(n.get()))
		{
			if (const WeaponTemplate *warhead = TheWeaponStore ? TheWeaponStore->findWeaponTemplate(p->m_warheadTemplateName) : nullptr)
			{
				weapons.push_back(warhead);
			}
		}
	}
	for (const WeaponTemplate *wt : weapons)
	{
		for (const std::shared_ptr<WeaponNugget> &n : wt->m_nuggets)
		{
			if (const DamageNugget *d = dynamic_cast<const DamageNugget *>(n.get()))
			{
				if (off(*d))
				{
					continue;
				}
				if (n->kind() == NUGGET_FIRE_LOGIC)
				{
					continue; // lane FLY-1: a FireLogicNugget feeds fires, it hits no victim (RW 0x8F8014 answers false: Drogoth's WingBlast's INCREASE_FUEL_ON_EXISTING_FIRE)
				}
				const float coef = armor ? armor->getCoefficient(d->m_damageType) : 1.0f;
				// the nugget's DamageScalar entries whose filter takes the victim (Weapon.ini `DamageScalar = 80% ANY +HERO`)
				float scalar = 1.0f;
				for (const DamageScalarEntry &e : d->m_damageScalar)
				{
					if (ObjectFilterMatch::allows(victim.logic(), e.filter, victim, attackerPlayer))
					{
						scalar *= e.scalar;
					}
				}
				const float hit = d->m_damage * coef * scalar;
				out.push_back(hit);
				sum += hit;
				text += str(" %g x %g%s (type %d)", (double)d->m_damage, (double)coef, scalar != 1.0f ? str(" x %g", (double)scalar).c_str() : "", d->m_damageType);
			}
		}
	}
	if (out.size() > 1)
	{
		out.push_back(sum);
	}
	return out;
}

// The hits and state transitions the caster (the unit and its horde's members) lands, frame by frame
struct HitTracker
{
	struct Hit
	{
		UnsignedInt frame;
		ObjectID victim;
		float amount;
		float left; ///< the victim's health after it
	};
	std::set<ObjectID> casters;
	std::map<ObjectID, float> health;
	std::vector<Hit> hits;
	std::map<ObjectID, UnsignedInt> terrified; ///< the first frame an object other than the casters had a TERROR request or emotion
	/// per emotion type, the first tracked frame each non-caster object carried the request or active emotion
	std::map<int, std::map<ObjectID, UnsignedInt>> emotionFirstSeen;
	/// per model condition bit, the first tracked frame each object had that bit set
	std::map<int, std::map<ObjectID, UnsignedInt>> modelConditionFirstSeen;
	/// per disabled bit, the first tracked frame each object carried it
	std::map<unsigned, std::map<ObjectID, UnsignedInt>> disabledFirstSeen;
	/// the first tracked frame each object entered AI_HARVEST
	std::map<ObjectID, UnsignedInt> harvestFirstSeen;
	struct Made
	{
		UnsignedInt frame;
		const Player *owner;
		const ThingTemplate *tmpl;
	};
	std::map<ObjectID, Made> made; ///< the objects that appeared after the first frame tracked (not projectiles), with their owner then
	/// per attribute modifier list, the first tracked frame each object carried it (a buffed unit can die in the scene's fight before the closing probe)
	std::map<std::string, std::map<ObjectID, UnsignedInt>> modifierFirstSeen;
	ObjectID highest = INVALID_ID;

	void track(GameLogic &logic)
	{
		const bool first = highest == INVALID_ID;
		ObjectID top = first ? 0 : highest;
		const UnsignedInt now = logic.getFrame();
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (const AttributeModifierPool *pool = static_cast<const AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate")))
			{
				for (const std::string &n : pool->listNames())
				{
					modifierFirstSeen[n].emplace(o->getID(), now);
				}
			}
			const Object::ModelConditionBits &mc = o->getModelConditionBits();
			for (size_t w = 0; w < mc.size(); ++w)
			{
				std::uint32_t word = mc[w];
				while (word != 0)
				{
					int b = 0;
					while (((word >> b) & 1u) == 0)
					{
						++b;
					}
					const int bit = (int)(w * 32 + (size_t)b);
					modelConditionFirstSeen[bit].emplace(o->getID(), now);
					word &= ~(1u << b);
				}
			}
			for (unsigned d = 0; d < DISABLED_TYPE_COUNT; ++d)
			{
				if (o->isDisabledByType(d))
				{
					disabledFirstSeen[d].emplace(o->getID(), now);
				}
			}
			if (const AIUpdateInterface *ai = o->getAIUpdateInterface(); ai && ai->aiStateType() == AI_HARVEST)
			{
				harvestFirstSeen.emplace(o->getID(), now);
			}
			if (!first && o->getID() > highest && !made.count(o->getID()) && !o->isKindOfName("PROJECTILE"))
			{
				made[o->getID()] = { now, o->getControllingPlayer(), o->getTemplate() };
			}
			top = std::max(top, o->getID());
		}
		highest = top;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (!casters.count(o->getID()))
			{
				if (const EmotionTrackerUpdate *tr = EmotionTrackerUpdate::of(*o))
				{
					for (int et = 0; et < EMOTION_TYPE_COUNT; ++et)
					{
						if (tr->requested(et) || (tr->current() && tr->current()->type() == et) || (tr->forcedType() == et && tr->forcedFrames() > 0))
						{
							emotionFirstSeen[et].emplace(o->getID(), now);
							if (et == EMOTION_TERROR && !terrified.count(o->getID()))
							{
								terrified[o->getID()] = now;
							}
						}
					}
				}
			}
			const ActiveBody *body = dynamic_cast<const ActiveBody *>(o->getBodyModule());
			if (!body || casters.count(o->getID()))
			{
				continue;
			}
			const float h = body->getHealth();
			auto it = health.find(o->getID());
			if (it != health.end() && h < it->second && body->lastDamageFrame() == now && casters.count(body->lastDamager()))
			{
				hits.push_back({ now, o->getID(), it->second - h, h });
			}
			health[o->getID()] = h;
		}
	}

	// the most victims the casters hit in one frame of [from, to)
	int maxVictims(UnsignedInt from, UnsignedInt to) const
	{
		std::map<UnsignedInt, int> n;
		int m = 0;
		for (const Hit &x : hits)
		{
			m = x.frame >= from && x.frame < to ? std::max(m, ++n[x.frame]) : m;
		}
		return m;
	}

	float maxHit(UnsignedInt from, UnsignedInt to) const
	{
		float m = 0.0f;
		for (const Hit &x : hits)
		{
			m = x.frame >= from && x.frame < to ? std::max(m, x.amount) : m;
		}
		return m;
	}
};

// What a power did, read before and after a cast, as the world shows it (not a module's own counter): per module class of the power (on the unit and
// its members, or the spell book) the result it must have.
struct EffectProbe
{
	enum Kind
	{
		OWNER,
		LEVEL,
		DAMAGE,
		CURSE,
		MODIFIER,
		SPAWN,
		HEAL,
		WEAPON_MODE,
		WEAPON_FLAGS,
		STATUS,
		GRAB,
		TERROR,
		MOUNT,
		TELEPORT,        ///< lane FACT-C: TeleportSpecialAbilityUpdate -> the caster stands elsewhere
		BOUNTY,          ///< lane FACT-C: ScavengerSpecialPower -> the player's bounty percent changed (RW 0x6AA847)
		EMOTION,         ///< EmotionWeaponNugget / ModelConditionSpecialAbilityUpdate emotion pulse
		LUA_EVENT,       ///< LuaEventNugget -> script engine dispatch / target emotion
		META_IMPACT,     ///< MetaImpactNugget -> combat metaImpactHits increased
		STEAL_MONEY,     ///< StealMoneyNugget -> caster player's cash increased
		PARALYZE,        ///< ParalyzeNugget -> object disabled with USER_PARALYZED / USER_FROZEN
		MODEL_CONDITION, ///< SpecialModelConditionNugget / ModelConditionSpecialAbilityUpdate / SpecialDisguise / ToggleDeploy / Invisibility
		REPLACE,         ///< ReplaceObjectUpdate -> replacement template made or non-holder enemy destroyed
		HARVEST,         ///< SPECIAL_HARVEST -> caster entered AI_HARVEST / harvest model condition
		TIMER_REFRESH,   ///< SpecialPowerTimerRefreshSpecialPower -> allied non-holder readySum decreased
		FIRE_LOGIC,      ///< FireLogicNugget -> extinguishes / feeds fires (unported S-191 / S-1997 -> NO-EFFECT)
	};

	struct Expect
	{
		enum Scope
		{
			SCOPE_ANY = 0,
			SCOPE_HOLDERS,
			SCOPE_PLAYER,
		};
		Kind kind;
		std::string what;                            ///< the module class and its INI value, for the report
		std::string modifier;                        ///< MODIFIER: single modifier list name (or LUA_EVENT: event name)
		const WeaponTemplate *weapon = nullptr;      ///< DAMAGE (primary template)
		float amount = 0.0f;                         ///< LEVEL: LevelGrantSpecialPower's Experience
		std::set<std::string> templates;             ///< SPAWN / REPLACE: target templates
		std::set<std::string> modifiers;             ///< MODIFIER: any of these list names satisfies (multi-nugget weapons)
		std::vector<const WeaponTemplate *> weapons; ///< DAMAGE: all damage-bearing templates (incl. HordeAttackNugget member slot / DamageField)
		size_t hordeMembers = 1;                     ///< DAMAGE: member count scaling when HordeAttackNugget fires all members at once
		std::set<int> emotions;                      ///< EMOTION: acceptable EmotionType ids (empty = any)
		std::vector<int> modelConditions;            ///< MODEL_CONDITION: any of these bit indices satisfies
		Scope scope = SCOPE_ANY;
	};

	// the CreateObject templates of an ObjectCreationList (its ObjectNames)
	static void oclTemplates(const std::string &name, std::set<std::string> &out)
	{
		const ObjectCreationList *ocl = TheObjectCreationListStore && !name.empty() ? TheObjectCreationListStore->findObjectCreationList(name) : nullptr;
		for (size_t i = 0; ocl && i < ocl->nuggets().size(); ++i)
		{
			if (ocl->nuggets()[i].kind == OCLNugget::CREATE_OBJECT)
			{
				out.insert(ocl->nuggets()[i].objectNames.begin(), ocl->nuggets()[i].objectNames.end());
			}
		}
	}

	static Expect spawnOf(const std::string &what, const std::set<std::string> &templates, Kind kind = SPAWN)
	{
		Expect e{ kind, what };
		e.templates = templates;
		return e;
	}

	static Expect modifierOf(const std::string &what, const std::string &modName)
	{
		Expect e{ MODIFIER, what, modName };
		if (!modName.empty())
		{
			e.modifiers.insert(modName);
		}
		return e;
	}

	struct Seen
	{
		const Player *owner = nullptr;
		float health = 0.0f;
		int rank = 0;
		float experience = 0.0f;
		std::set<std::string> modifiers;
		unsigned long long readySum = 0;
		ObjectStatusMaskType status{};
		WeaponConditionFlags weaponFlags{};
		Object::ModelConditionBits modelConditions{};
		DisabledMaskType disabled = 0;
		ObjectID containedBy = INVALID_ID;
		bool terror = false;
		std::array<bool, EMOTION_TYPE_COUNT> emotions{};
		bool harvesting = false;
		const ThingTemplate *tmpl = nullptr;
		Coord3D pos{}; ///< lane FACT-C
	};

	UnsignedInt frame = 0;
	ObjectID highest = 0;
	const Player *player = nullptr;
	std::set<ObjectID> holders;
	std::map<ObjectID, Seen> seen;
	std::vector<Expect> expects;
	std::vector<std::string> unasserted;
	std::vector<std::string> skippedNotes;
	std::set<const BehaviorModule *> visited;

	static bool nuggetUpgradesOk(const WeaponNugget &n, const Object *caster)
	{
		if (n.m_requiredUpgradeNames.empty() && n.m_forbiddenUpgradeNames.empty())
		{
			return true;
		}
		if (!caster)
		{
			return n.m_requiredUpgradeNames.empty();
		}
		const Player *pl = caster->getControllingPlayer();
		auto has = [&](const std::string &up) { return caster->hasUpgrade(up) || (pl && pl->hasUpgradeComplete(up)); };
		for (const std::string &up : n.m_forbiddenUpgradeNames)
		{
			if (has(up))
			{
				return false;
			}
		}
		for (const std::string &up : n.m_requiredUpgradeNames)
		{
			if (!has(up))
			{
				return false;
			}
		}
		return true;
	}

	static bool nuggetFilterOk(const WeaponNugget &n, const Object &caster, const Object *target)
	{
		if (!target || !ObjectFilterMatch::isValid(&n.m_specialObjectFilter))
		{
			return true;
		}
		return ObjectFilterMatch::allows(caster.logic(), n.m_specialObjectFilter, *target, caster.getControllingPlayer());
	}
	bool sawBaseSpecialAbilityUpdate = false;
	unsigned weaponModeActive = 0;
	float bounty = 0.0f; ///< lane FACT-C: the caster's player's bounty percent
	float casterDamageMult = 1.0f;
	unsigned long long metaImpactHits = 0;
	std::uint32_t playerMoney = 0;
	size_t luaEvents = 0;

	static void inspectWeaponSpecial(const std::string &cls, const std::string &weaponName, const WeaponTemplate *w, Object &holder, EffectProbe &p, const Object *target = nullptr)
	{
		std::vector<const WeaponTemplate *> ws;
		std::set<const WeaponTemplate *> seenWs;
		if (w)
		{
			ws.push_back(w);
			seenWs.insert(w);
		}
		bool damage = false, grab = false, ocl = false, hordeAttack = false, metaImpact = false, stealMoney = false, paralyze = false;
		bool fireLogic = false;
		std::string hordeProblem;
		size_t hordeMembers = 1;
		std::vector<const WeaponTemplate *> damageWeapons;
		std::set<std::string> made;
		std::set<std::string> attrMods;
		std::set<int> emotionTypes;
		std::vector<std::string> luaEventNames;
		std::vector<int> specialModelConds;

		auto enqueueWeapon = [&](const std::string &name) {
			if (const WeaponTemplate *nw = (!name.empty() && TheWeaponStore) ? TheWeaponStore->findWeaponTemplate(name) : nullptr)
			{
				if (seenWs.insert(nw).second)
				{
					ws.push_back(nw);
				}
			}
		};

		for (size_t i = 0; i < ws.size(); ++i)
		{
			const WeaponTemplate *cur = ws[i];
			bool curHasDamage = false;
			for (const std::shared_ptr<WeaponNugget> &n : cur->m_nuggets)
			{
				if (!nuggetUpgradesOk(*n, &holder))
				{
					continue;
				}
				if (!nuggetFilterOk(*n, holder, target))
				{
					p.skippedNotes.push_back(std::string(n->keyword()) + " skipped (target fails SpecialObjectFilter)");
					continue;
				}
				const WeaponNuggetKind nk = n->kind();
				if (nk == NUGGET_DAMAGE || nk == NUGGET_DOT)
				{
					damage = true;
					curHasDamage = true;
				}
				else if (nk == NUGGET_FIRE_LOGIC)
				{
					fireLogic = true;
				}
				else if (nk == NUGGET_GRAB)
				{
					grab = true;
				}
				else if (const WeaponOCLNugget *on = dynamic_cast<const WeaponOCLNugget *>(n.get()))
				{
					ocl = true;
					oclTemplates(on->m_weaponOCLName, made);
				}
				else if (const SpawnAndFadeNugget *sfn = dynamic_cast<const SpawnAndFadeNugget *>(n.get()))
				{
					if (!sfn->m_spawnedObjectName.empty())
					{
						ocl = true;
						made.insert(sfn->m_spawnedObjectName);
					}
				}
				else if (const AttributeModifierNugget *an = dynamic_cast<const AttributeModifierNugget *>(n.get()))
				{
					if (!an->m_attributeModifier.empty())
					{
						attrMods.insert(an->m_attributeModifier);
					}
				}
				else if (const EmotionWeaponNugget *en = dynamic_cast<const EmotionWeaponNugget *>(n.get()))
				{
					if (en->m_emotionType >= 0)
					{
						emotionTypes.insert(en->m_emotionType);
					}
				}
				else if (const LuaEventNugget *ln = dynamic_cast<const LuaEventNugget *>(n.get()))
				{
					if (!ln->m_luaEvent.empty())
					{
						luaEventNames.push_back(ln->m_luaEvent);
					}
				}
				else if (nk == NUGGET_META_IMPACT)
				{
					metaImpact = true;
				}
				else if (nk == NUGGET_STEAL_MONEY)
				{
					stealMoney = true;
				}
				else if (nk == NUGGET_PARALYZE)
				{
					paralyze = true;
				}
				else if (const SpecialModelConditionNugget *smn = dynamic_cast<const SpecialModelConditionNugget *>(n.get()))
				{
					for (const std::string &mcName : smn->m_modelConditionNames)
					{
						if (const int bit = CombatNames::modelCondition(mcName.c_str()); bit >= 0)
						{
							specialModelConds.push_back(bit);
						}
					}
				}
				else if (const DamageFieldNugget *dfn = dynamic_cast<const DamageFieldNugget *>(n.get()))
				{
					enqueueWeapon(dfn->m_weaponTemplateName);
				}
				else if (const HordeAttackNugget *hn = dynamic_cast<const HordeAttackNugget *>(n.get()))
				{
					hordeAttack = true;
					ContainModuleInterface *hc = holder.getContain();
					if (!hc || !hc->getHordeContainInterface())
					{
						hordeProblem = "caster has no HordeContain";
					}
					else
					{
						const auto *items = hc->getContainedItemsList();
						hordeMembers = items ? std::max<size_t>(1, items->size()) : 1;
						const Weapon *mw = nullptr;
						if (items)
						{
							for (const Object *mem : *items)
							{
								if (!mem || mem->isEffectivelyDead() || !mem->getWeapons())
								{
									continue;
								}
								mw = hn->m_lockWeaponSlot != 5 ? mem->getWeapons()->weaponInSlot(hn->m_lockWeaponSlot) : mem->getWeapons()->currentWeapon();
								if (mw && mw->getTemplate())
								{
									break;
								}
							}
						}
						if (mw && mw->getTemplate())
						{
							enqueueWeapon(mw->getTemplate()->getName());
						}
						else
						{
							hordeProblem = str("HordeContain has no weapon in slot %d", hn->m_lockWeaponSlot);
						}
					}
				}
				else if (const ProjectileNugget *pn = dynamic_cast<const ProjectileNugget *>(n.get()))
				{
					enqueueWeapon(pn->m_warheadTemplateName);
					if (!pn->m_projectileTemplateName.empty())
					{
						if (const ThingTemplate *proj = holder.logic().things().findTemplate(pn->m_projectileTemplateName))
						{
							for (const ThingTemplate::Nugget &pnug : proj->behaviorModules().nuggets())
							{
								if (const CreateObjectDieModuleData *cod = dynamic_cast<const CreateObjectDieModuleData *>(pnug.data.get()))
								{
									if (!cod->m_creationList.empty())
									{
										ocl = true;
										oclTemplates(cod->m_creationList, made);
									}
								}
							}
						}
					}
				}
			}
			if (curHasDamage)
			{
				damageWeapons.push_back(cur);
			}
		}

		const size_t baseCount = p.expects.size();
		if (damage)
		{
			Expect de{ DAMAGE, cls + " SpecialWeapon " + weaponName + (hordeAttack ? " (HordeAttackNugget)" : "") };
			de.weapon = w ? w : damageWeapons.front();
			de.weapons = damageWeapons;
			if (w && std::find(de.weapons.begin(), de.weapons.end(), w) == de.weapons.end())
			{
				de.weapons.push_back(w);
			}
			de.hordeMembers = hordeAttack ? hordeMembers : 1;
			p.expects.push_back(std::move(de));
		}
		else if (hordeAttack && (!hordeProblem.empty() || !damage))
		{
			// Single units with HordeAttackNugget, empty horde slots, or dummy member weapons have no damage delivery; judge as NO-EFFECT rather than NO-RULE
			const std::string &why = !hordeProblem.empty() ? hordeProblem : std::string("member weapon has no damage nugget");
			p.expects.push_back({ DAMAGE, cls + " SpecialWeapon " + weaponName + " (HordeAttackNugget: " + why + ")" });
		}
		if (grab)
		{
			p.expects.push_back({ GRAB, cls + " SpecialWeapon " + weaponName + " (GrabNugget)" });
		}
		if (ocl)
		{
			p.expects.push_back(spawnOf(cls + " SpecialWeapon " + weaponName + " (WeaponOCLNugget)", made));
		}
		if (!attrMods.empty())
		{
			std::string names;
			for (const std::string &nm : attrMods)
			{
				names += (names.empty() ? "" : ", ") + nm;
			}
			Expect me{ MODIFIER, cls + " SpecialWeapon " + weaponName + " (AttributeModifierNugget " + names + ")" };
			me.modifier = *attrMods.begin();
			me.modifiers = std::move(attrMods);
			p.expects.push_back(std::move(me));
		}
		if (!emotionTypes.empty())
		{
			Expect ee{ EMOTION, cls + " SpecialWeapon " + weaponName + " (EmotionWeaponNugget)" };
			ee.emotions = std::move(emotionTypes);
			p.expects.push_back(std::move(ee));
		}
		for (const std::string &le : luaEventNames)
		{
			Expect leExpect{ LUA_EVENT, cls + " SpecialWeapon " + weaponName + " (LuaEventNugget " + le + ")", le };
			if (le == "BeTerrified")
			{
				leExpect.emotions.insert(EMOTION_TERROR);
			}
			else if (le == "BeUncontrollablyAfraid")
			{
				leExpect.emotions.insert(EMOTION_UNCONTROLLABLE_FEAR);
			}
			p.expects.push_back(std::move(leExpect));
		}
		if (metaImpact)
		{
			p.expects.push_back({ META_IMPACT, cls + " SpecialWeapon " + weaponName + " (MetaImpactNugget)" });
		}
		if (stealMoney)
		{
			p.expects.push_back({ STEAL_MONEY, cls + " SpecialWeapon " + weaponName + " (StealMoneyNugget)" });
		}
		if (paralyze)
		{
			p.expects.push_back({ PARALYZE, cls + " SpecialWeapon " + weaponName + " (ParalyzeNugget)" });
		}
		if (!specialModelConds.empty())
		{
			Expect mce{ MODEL_CONDITION, cls + " SpecialWeapon " + weaponName + " (SpecialModelConditionNugget)" };
			mce.modelConditions = std::move(specialModelConds);
			mce.scope = Expect::SCOPE_ANY;
			p.expects.push_back(std::move(mce));
		}
		if (p.expects.size() == baseCount)
		{
			if (fireLogic)
			{
				p.expects.push_back({ FIRE_LOGIC, cls + " SpecialWeapon " + weaponName });
			}
			else if (p.skippedNotes.empty())
			{
				p.unasserted.push_back(cls + " SpecialWeapon " + weaponName + " (no supported weapon nugget)");
			}
		}
	}

	// lane TAINTAREA-1 (main, merged into Local 78's probe 2026-11-16): Taint / Elven Wood paint TheTaintManager's grid under their area object,
	// whose AttributeModifierAuraUpdate (RequiredConditions TAINT / ELVEN_WOOD) gives its BonusName to the caster's allies standing on the painted
	// ground: that list is the power's effect
	static void areaAura(Object &holder, const std::string &cls, const std::string &areaName, EffectProbe &p)
	{
		const ThingTemplate *area = holder.logic().things().findTemplate(areaName);
		std::string bonus;
		for (const ThingTemplate::Nugget &n : area ? area->behaviorModules().nuggets() : std::vector<ThingTemplate::Nugget>{})
		{
			const AttributeModifierAuraUpdateModuleData *ad = dynamic_cast<const AttributeModifierAuraUpdateModuleData *>(n.data.get());
			bonus = ad && bonus.empty() ? ad->m_bonusName : bonus;
		}
		if (bonus.empty())
		{
			p.unasserted.push_back(cls);
			return;
		}
		p.expects.push_back({ MODIFIER, cls + " area " + areaName + " aura " + bonus, bonus });
	}

	static void expectFrom(BehaviorModule *m, Object &holder, const SpecialPowerTemplate *t, EffectProbe &p, int depth, const Object *target = nullptr)
	{
		const std::string cls = m->getModuleClassName();
		if (const WeaponModeSpecialPowerUpdate *wm = dynamic_cast<const WeaponModeSpecialPowerUpdate *>(m))
		{
			p.weaponModeActive += wm->active() ? 1 : 0;
			p.expects.push_back({ WEAPON_MODE, cls });
			return;
		}
		if (const SpecialAbilityUpdate *a = dynamic_cast<const SpecialAbilityUpdate *>(m))
		{
			const SpecialAbilityUpdateModuleData *d = a->abilityData();
			if (!d->m_triggerAttributeModifier.empty())
			{
				p.expects.push_back(modifierOf(cls + " TriggerAttributeModifier " + d->m_triggerAttributeModifier, d->m_triggerAttributeModifier));
			}
			if (dynamic_cast<const DominateEnemySpecialPower *>(m))
			{
				p.expects.push_back({ OWNER, cls });
			}
			else if (const LevelGrantSpecialPowerModuleData *ld = dynamic_cast<const LevelGrantSpecialPowerModuleData *>(d))
			{
				p.expects.push_back({ LEVEL, cls + str(" Experience %d", ld->m_experience), "", nullptr, (float)ld->m_experience });
			}
			else if (dynamic_cast<const CurseSpecialPower *>(m))
			{
				p.expects.push_back({ CURSE, cls });
			}
			else if (const WeaponFireSpecialAbilityUpdateModuleData *wd = dynamic_cast<const WeaponFireSpecialAbilityUpdateModuleData *>(d))
			{
				const WeaponTemplate *w = TheWeaponStore ? TheWeaponStore->findWeaponTemplate(wd->m_specialWeapon) : nullptr;
				inspectWeaponSpecial(cls, wd->m_specialWeapon, w, holder, p, target);
			}
			else if (const ArrowStormUpdateModuleData *ad = dynamic_cast<const ArrowStormUpdateModuleData *>(d))
			{
				const WeaponTemplate *w = TheWeaponStore ? TheWeaponStore->findWeaponTemplate(ad->m_weaponTemplate) : nullptr;
				Expect de{ DAMAGE, cls + " WeaponTemplate " + ad->m_weaponTemplate, "", w };
				if (w)
				{
					de.weapons.push_back(w);
				}
				p.expects.push_back(std::move(de));
			}
			else if (const HeroModeSpecialAbilityUpdateModuleData *hd = dynamic_cast<const HeroModeSpecialAbilityUpdateModuleData *>(d))
			{
				if (!hd->m_heroAttributeModifier.empty())
				{
					p.expects.push_back(modifierOf(cls + " HeroAttributeModifier " + hd->m_heroAttributeModifier, hd->m_heroAttributeModifier));
				}
			}
			else if (dynamic_cast<const ToggleHiddenSpecialAbilityUpdate *>(m))
			{
				p.expects.push_back({ STATUS, cls });
			}
			else if (dynamic_cast<const TeleportSpecialAbilityUpdate *>(m))
			{
				p.expects.push_back({ TELEPORT, cls });
			}
			else if (const ToggleMountedSpecialAbilityUpdateModuleData *td = dynamic_cast<const ToggleMountedSpecialAbilityUpdateModuleData *>(d))
			{
				// a MountedTemplate replaces the unit (seen by the caller, nothing else counts); without one the unit itself mounts: its status or weapon set
				p.expects.push_back({ MOUNT, cls + (td->m_mountedTemplate.empty() ? "" : " MountedTemplate " + td->m_mountedTemplate), td->m_mountedTemplate });
			}
			else if (const SummonReplacementSpecialAbilityUpdateModuleData *sd = dynamic_cast<const SummonReplacementSpecialAbilityUpdateModuleData *>(d))
			{
				// lane SUMMON-1: the Thrall Master's summons, ToggleMounted's replacement over its own data (RW 0x8B228C)
				p.expects.push_back({ MOUNT, cls + (sd->m_mountedTemplate.empty() ? "" : " MountedTemplate " + sd->m_mountedTemplate), sd->m_mountedTemplate });
			}
			else if (const ReplaceObjectUpdateModuleData *rd = dynamic_cast<const ReplaceObjectUpdateModuleData *>(d))
			{
				std::set<std::string> reps;
				for (const ReplaceObjectEntry &entry : rd->entries)
				{
					reps.insert(entry.replacements.begin(), entry.replacements.end());
				}
				p.expects.push_back(spawnOf(cls + " (ReplaceObjectUpdate)", reps, REPLACE));
			}
			else if (dynamic_cast<const ToggleDeploySpecialAbilityUpdate *>(m))
			{
				Expect ex{ MODEL_CONDITION, cls + " (DEPLOYED / UNPACKING / PACKING)" };
				ex.modelConditions = { CombatNames::modelCondition("DEPLOYED"), CombatNames::modelCondition("UNPACKING"), CombatNames::modelCondition("PACKING") };
				ex.scope = Expect::SCOPE_HOLDERS;
				p.expects.push_back(std::move(ex));
			}
			else if (dynamic_cast<const SpecialDisguiseUpdate *>(m))
			{
				Expect ex{ MODEL_CONDITION, cls + " (DISGUISED)" };
				ex.modelConditions = { CombatNames::modelCondition("DISGUISED") };
				ex.scope = Expect::SCOPE_HOLDERS;
				p.expects.push_back(std::move(ex));
			}
			else if (const ModelConditionSpecialAbilityUpdateModuleData *mcd = dynamic_cast<const ModelConditionSpecialAbilityUpdateModuleData *>(d))
			{
				const char *mcName = mcd->m_whichSpecialPower == 0 ? "INVULNERABLE"
									 : mcd->m_whichSpecialPower == 1 ? "SPECIAL_POWER_1"
									 : mcd->m_whichSpecialPower == 2 ? "SPECIAL_POWER_2"
									 : mcd->m_whichSpecialPower == 3 ? "SPECIAL_POWER_3"
																	 : nullptr;
				if (mcName)
				{
					Expect ex{ MODEL_CONDITION, cls + " (" + mcName + ")" };
					ex.modelConditions = { CombatNames::modelCondition(mcName) };
					ex.scope = Expect::SCOPE_HOLDERS;
					p.expects.push_back(std::move(ex));
				}
				if (mcd->m_generateTerror)
				{
					Expect ee{ EMOTION, cls + " (GenerateTerror)" };
					ee.emotions.insert(EMOTION_TERROR);
					p.expects.push_back(std::move(ee));
				}
				if (mcd->m_generateUncontrollableFear)
				{
					Expect ee{ EMOTION, cls + " (GenerateUncontrollableFear)" };
					ee.emotions.insert(EMOTION_UNCONTROLLABLE_FEAR);
					p.expects.push_back(std::move(ee));
				}
			}
			else if (const ActivateModuleSpecialPowerModuleData *amd = dynamic_cast<const ActivateModuleSpecialPowerModuleData *>(d))
			{
				for (const ActivateModuleSpecialPowerModuleData::Entry &e : amd->m_triggers)
				{
					const NameKeyType key = holder.logic().things().nameKeys().findKey(e.tag);
					BehaviorModule *activated = key == NAMEKEY_INVALID ? nullptr : holder.findModuleByTag(key);
					if (activated && activated != m && depth < 3 && p.visited.insert(activated).second)
					{
						expectFrom(activated, holder, nullptr, p, depth + 1, target);
						if (activated->getModuleClassName() == "SpecialPowerModule")
						{
							if (SpecialPowerModuleInterface *actSp = activated->getSpecialPower())
							{
								if (const SpecialPowerTemplate *actTpl = actSp->getSpecialPowerTemplate(); actTpl && actTpl->getName() != "SpecialAbilityActivateeDummy")
								{
									for (const std::unique_ptr<BehaviorModule> &om : holder.modules())
									{
										if (om.get() == m || om.get() == activated)
										{
											continue;
										}
										const SpecialAbilityUpdate *oa = dynamic_cast<const SpecialAbilityUpdate *>(om.get());
										const WeaponModeSpecialPowerUpdate *owm = dynamic_cast<const WeaponModeSpecialPowerUpdate *>(om.get());
										SpecialPowerModuleInterface *osp = om->getSpecialPower();
										const bool match = oa ? oa->abilityData()->m_specialPowerTemplateName == actTpl->getName()
															  : owm ? owm->getSpecialPowerTemplate() == actTpl
																	: osp && osp->getSpecialPowerTemplate() == actTpl;
										if (match && p.visited.insert(om.get()).second)
										{
											expectFrom(om.get(), holder, actTpl, p, depth + 1, target);
										}
									}
								}
							}
						}
					}
				}
			}
			else if (cls == "SpecialAbilityUpdate")
			{
				const int type = d->m_specialPowerTemplate ? d->m_specialPowerTemplate->getFinalOverride()->m_type : 0;
				bool asserted = false;
				if (type == 0x1D || type == 0x1A)
				{
					p.expects.push_back({ OWNER, cls + " (a capture)" });
					asserted = true;
				}
				if (type == 0x89)
				{
					p.expects.push_back({ TERROR, cls + " (SPECIAL_SCREECH: BeScary -> RadiateTerrorEx -> BeTerrified)" });
					asserted = true;
				}
				if (type == 0x80 || type == 0x8F)
				{
					// RW 0x854841 (SPECIAL_ATHELAS / SPECIAL_ELVEN_GRACE) triggers AutoHealBehavior::triggerByButton (RW 0x855431)
					p.expects.push_back({ HEAL, cls + (type == 0x80 ? " (SPECIAL_ATHELAS)" : " (SPECIAL_ELVEN_GRACE)") });
					asserted = true;
				}
				if (!d->m_specialObject.empty())
				{
					p.expects.push_back(spawnOf(cls + " SpecialObject " + d->m_specialObject, { d->m_specialObject }));
					asserted = true;
				}
				if (!asserted && d->m_triggerAttributeModifier.empty())
				{
					p.sawBaseSpecialAbilityUpdate = true;
				}
			}
			else
			{
				p.unasserted.push_back(cls);
			}
			return;
		}
		SpecialPowerModuleInterface *sp = m->getSpecialPower();
		if (!sp || (t && sp->getSpecialPowerTemplate() != t))
		{
			return;
		}
		const SpecialPowerModule *spm = dynamic_cast<const SpecialPowerModule *>(sp);
		if (dynamic_cast<const WeaponChangeSpecialPowerModule *>(m))
		{
			p.expects.push_back({ WEAPON_FLAGS, cls });
		}
		else if (const TaintSpecialPower *tp = dynamic_cast<const TaintSpecialPower *>(m))
		{
			areaAura(holder, cls, tp->data()->m_taintObject, p); // lane TAINTAREA-1
		}
		else if (const ElvenWoodSpecialPower *ep = dynamic_cast<const ElvenWoodSpecialPower *>(m))
		{
			areaAura(holder, cls, ep->data()->m_elvenGroveObject, p); // lane TAINTAREA-1
		}
		else if (const OCLSpecialPower *op = dynamic_cast<const OCLSpecialPower *>(m))
		{
			std::set<std::string> made;
			oclTemplates(op->data()->m_ocl, made);
			for (const auto &u : op->data()->m_upgradeOCL)
			{
				oclTemplates(u.second, made);
			}
			p.expects.push_back(spawnOf(cls + " OCL " + op->data()->m_ocl, made));
		}
		else if (dynamic_cast<const PlayerHealSpecialPower *>(m))
		{
			p.expects.push_back({ HEAL, cls });
		}
		else if (dynamic_cast<const ScavengerSpecialPower *>(m))
		{
			p.expects.push_back({ BOUNTY, cls });
		}
		else if (const InvisibilitySpecialPower *ip = dynamic_cast<const InvisibilitySpecialPower *>(m))
		{
			const auto *id = static_cast<const InvisibilitySpecialPowerModuleData *>(ip->spData());
			Expect ex{ MODEL_CONDITION, cls + " (InvisibilitySpecialPower)" };
			if (id)
			{
				if (id->m_nugget.invisibilityType == InvisibilityNugget::STEALTH)
				{
					ex.modelConditions.push_back(CombatNames::modelCondition("INVISIBLE_STEALTH"));
				}
				else if (id->m_nugget.invisibilityType == InvisibilityNugget::CAMOUFLAGE)
				{
					ex.modelConditions.push_back(CombatNames::modelCondition("INVISIBLE_CAMOUFLAGE"));
				}
				ex.scope = id->m_broadcastRadius > 0.0f ? Expect::SCOPE_PLAYER : Expect::SCOPE_HOLDERS;
			}
			p.expects.push_back(std::move(ex));
		}
		else if (dynamic_cast<const SpecialPowerTimerRefreshSpecialPower *>(m))
		{
			p.expects.push_back({ TIMER_REFRESH, cls });
		}
		else if (dynamic_cast<const HordeDispatchSpecialPower *>(m))
		{
			// lane FACT-C: the horde passes the power on to its members (S-2001): their modules carry the results
		}
		else if (cls == "SpecialPowerModule")
		{
			const SpecialPowerTemplate *st = sp->getSpecialPowerTemplate();
			const int type = st ? st->getFinalOverride()->m_type : 0;
			if (type == 0x8D)
			{
				p.expects.push_back({ HARVEST, cls + " (SPECIAL_HARVEST)" });
			}
		}
		else if (!(spm && !spm->spData()->m_attributeModifier.empty()))
		{
			p.unasserted.push_back(cls);
		}
		if (spm && !spm->spData()->m_attributeModifier.empty())
		{
			p.expects.push_back(modifierOf(cls + " AttributeModifier " + spm->spData()->m_attributeModifier, spm->spData()->m_attributeModifier));
		}
	}

	static EffectProbe take(GameLogic &logic, Object *u, const SpecialPowerTemplate *t, size_t engineEvents = 0, const Object *target = nullptr)
	{
		EffectProbe p;
		p.frame = logic.getFrame();
		p.player = u ? u->getControllingPlayer() : nullptr;
		p.bounty = p.player ? p.player->getBountyPercent() : 0.0f;
		p.metaImpactHits = logic.combat().counters().metaImpactHits;
		p.playerMoney = p.player ? p.player->getMoney()->countMoney() : 0;
		p.luaEvents = engineEvents;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			p.highest = std::max(p.highest, o->getID());
			Seen s;
			s.owner = o->getControllingPlayer();
			s.health = o->getBodyModule() ? o->getBodyModule()->getHealth() : 0.0f;
			s.rank = o->getExperienceTracker() ? o->getExperienceTracker()->getRank() : 0;
			s.experience = o->getExperienceTracker() ? o->getExperienceTracker()->getExperience() : 0.0f;
			if (const AttributeModifierPool *pool = static_cast<const AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate")))
			{
				for (const std::string &n : pool->listNames())
				{
					s.modifiers.insert(n);
				}
			}
			for (const std::unique_ptr<BehaviorModule> &m : o->modules())
			{
				if (SpecialPowerModuleInterface *sp = m->getSpecialPower())
				{
					s.readySum += sp->getReadyFrame();
				}
			}
			s.tmpl = o->getTemplate();
			s.pos = *o->getPosition();
			s.status = o->getStatusBits();
			s.weaponFlags = o->getWeaponSetFlags();
			s.modelConditions = o->getModelConditionBits();
			s.disabled = o->getDisabledMask();
			s.containedBy = o->getContainedBy() ? o->getContainedBy()->getID() : INVALID_ID;
			const EmotionTrackerUpdate *tr = EmotionTrackerUpdate::of(*o);
			if (tr)
			{
				for (int et = 0; et < EMOTION_TYPE_COUNT; ++et)
				{
					s.emotions[(size_t)et] = tr->requested(et) || (tr->current() && tr->current()->type() == et) || (tr->forcedType() == et && tr->forcedFrames() > 0);
				}
			}
			s.terror = s.emotions[EMOTION_TERROR];
			if (const AIUpdateInterface *ai = o->getAIUpdateInterface())
			{
				s.harvesting = ai->aiStateType() == AI_HARVEST;
			}
			p.seen[o->getID()] = s;
		}
		if (!u || !t)
		{
			return p;
		}
		std::vector<Object *> hs{ u };
		if (u->getContain())
		{
			for (Object *m : *u->getContain()->getContainedItemsList())
			{
				hs.push_back(m);
			}
		}
		for (Object *h : hs)
		{
			p.holders.insert(h->getID());
			float dmgMult = 1.0f;
			if (h->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, false, dmgMult))
			{
				p.casterDamageMult = std::max(p.casterDamageMult, dmgMult);
			}
		}
		// the classes per holder: a horde's members carry the same modules, one expectation each class is enough
		std::set<std::string> done;
		for (Object *h : hs)
		{
			EffectProbe one;
			for (const std::unique_ptr<BehaviorModule> &m : h->modules())
			{
				const SpecialAbilityUpdate *a = dynamic_cast<const SpecialAbilityUpdate *>(m.get());
				const WeaponModeSpecialPowerUpdate *wm = dynamic_cast<const WeaponModeSpecialPowerUpdate *>(m.get());
				SpecialPowerModuleInterface *sp = m->getSpecialPower();
				const bool mine = a ? a->abilityData()->m_specialPowerTemplateName == t->getName()
									: wm ? wm->getSpecialPowerTemplate() == t : sp && sp->getSpecialPowerTemplate() == t;
				if (mine && one.visited.insert(m.get()).second)
				{
					expectFrom(m.get(), *h, t, one, 0, target);
				}
			}
			if (one.sawBaseSpecialAbilityUpdate && one.expects.empty() && one.unasserted.empty())
			{
				one.unasserted.push_back("SpecialAbilityUpdate");
			}
			p.weaponModeActive += one.weaponModeActive;
			for (const Expect &e : one.expects)
			{
				if (done.insert(e.what).second)
				{
					p.expects.push_back(e);
				}
			}
			for (const std::string &c : one.unasserted)
			{
				if (done.insert("unasserted " + c).second)
				{
					p.unasserted.push_back(c);
				}
			}
			for (const std::string &sn : one.skippedNotes)
			{
				if (done.insert("skipped " + sn).second)
				{
					p.skippedNotes.push_back(sn);
				}
			}
		}
		return p;
	}

	// what the cast did ("" when nothing); `missing` gets each expected result that did not show. `hits`: the caster's hits (DAMAGE), tracked from
	// before the cast (its frames before this probe's frame are the baseline)
	std::string compare(const EffectProbe &after, std::string &missing, const HitTracker *hits = nullptr) const
	{
		std::vector<std::string> out;
		auto was = [&](ObjectID id) -> const Seen * {
			auto it = seen.find(id);
			return it == seen.end() ? nullptr : &it->second;
		};
		auto bitSet = [](const Object::ModelConditionBits &bits, int bit) {
			return bit >= 0 && bit < 19 * 32 && ((bits[(size_t)bit >> 5] >> (bit & 31)) & 1u) != 0;
		};
		for (const Expect &e : expects)
		{
			std::string got;
			switch (e.kind)
			{
			case OWNER:
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					if (b && !holders.count(kv.first) && b->owner != player && kv.second.owner == player && player)
					{
						got = str("object %u's owner became the caster's player", (unsigned)kv.first);
						break;
					}
				}
				break;
			case LEVEL:
			{
				float gained = 0.0f;
				int objects = 0;
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					if (b && kv.second.owner == player && !holders.count(kv.first))
					{
						if (kv.second.rank > b->rank)
						{
							got = str("object %u: rank %d -> %d", (unsigned)kv.first, b->rank, kv.second.rank);
							break;
						}
						if (kv.second.experience > b->experience)
						{
							gained += kv.second.experience - b->experience;
							++objects;
						}
					}
				}
				if (got.empty() && e.amount > 0.0f && gained >= e.amount * 0.9f)
				{
					got = str("%.0f experience on %d own objects", (double)gained, objects);
				}
				break;
			}
			case DAMAGE:
				if (!e.weapon && e.weapons.empty())
				{
					break;
				}
				if (hits)
				{
					const int before = hits->maxVictims(0, frame);
					UnsignedInt first = 0;
					for (const HitTracker::Hit &x : hits->hits)
					{
						first = x.frame >= frame && (first == 0 || x.frame < first) ? x.frame : first;
					}
					if (before > 0)
					{
						break; // reported below: the caster fought before the cast
					}
					const float dmgBonus = std::max(1.0f, std::max(casterDamageMult, after.casterDamageMult));
					const float upperMult = (e.hordeMembers > 1 ? 2.5f * (float)e.hordeMembers : 2.5f) * dmgBonus;
					for (const HitTracker::Hit &x : hits->hits)
					{
						auto it = hitNominal.find(x.victim);
						const float nominal = it == hitNominal.end() ? 0.0f : it->second, sum = it == hitNominal.end() ? 0.0f : hitSum.at(x.victim);
						if (x.frame == first && nominal > 0.0f && x.amount >= nominal * 0.9f && x.amount <= sum * upperMult)
						{
							got = str("its first hit after the cast (frame %u) %.0f, the weapon's %.0f (all nuggets %.0f) against the victim's armour; no hit before the cast",
									  first - frame, (double)x.amount, (double)nominal, (double)sum);
							break;
						}
					}
				}
				break;
			case CURSE:
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					if (b && !holders.count(kv.first) && kv.second.owner != player && kv.second.readySum > b->readySum)
					{
						got = str("object %u's power recharge restarted", (unsigned)kv.first);
						break;
					}
				}
				break;
			case TIMER_REFRESH:
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					if (b && !holders.count(kv.first) && kv.second.owner == player && kv.second.readySum < b->readySum)
					{
						got = str("allied object %u's power timer refreshed (%llu -> %llu)", (unsigned)kv.first, b->readySum, kv.second.readySum);
						break;
					}
				}
				break;
			case FIRE_LOGIC:
				break;
			case MODIFIER:
			{
				auto checkMod = [&](const std::string &modName) {
					for (const auto &kv : after.seen)
					{
						const Seen *b = was(kv.first);
						if (kv.second.modifiers.count(modName) && (!b || !b->modifiers.count(modName)))
						{
							got = str("%s on object %u", modName.c_str(), (unsigned)kv.first);
							return true;
						}
					}
					if (hits && hits->modifierFirstSeen.count(modName))
					{
						for (const auto &kv : hits->modifierFirstSeen.at(modName))
						{
							const Seen *b = was(kv.first);
							if (kv.second >= frame && (!b || !b->modifiers.count(modName)))
							{
								got = str("%s on object %u (frame %u, gone by the closing probe)", modName.c_str(), (unsigned)kv.first, (unsigned)kv.second);
								return true;
							}
						}
					}
					return false;
				};
				if (!e.modifiers.empty())
				{
					for (const std::string &modName : e.modifiers)
					{
						if (checkMod(modName))
						{
							break;
						}
					}
				}
				else if (!e.modifier.empty())
				{
					checkMod(e.modifier);
				}
				break;
			}
			case SPAWN:
			case REPLACE:
			{
				auto expected = [&](const ThingTemplate *t) { return t && e.templates.count(t->getName()) > 0; };
				for (const auto &kv : after.seen)
				{
					if (kv.first > highest && expected(kv.second.tmpl))
					{
						got = str("%s %u made", kv.second.tmpl->getName().c_str(), (unsigned)kv.first);
						break;
					}
				}
				for (auto it = hits ? hits->made.begin() : std::map<ObjectID, HitTracker::Made>::const_iterator{}; hits && it != hits->made.end() && got.empty(); ++it)
				{
					if (it->first > highest && expected(it->second.tmpl))
					{
						got = str("%s %u made (frame %u)", it->second.tmpl->getName().c_str(), (unsigned)it->first, it->second.frame);
					}
				}
				// RW 0x8B6D2A: ReplaceObjectUpdate kills and destroys the matched enemy even when Racoon_Replacement fails template lookup
				if (got.empty() && e.kind == REPLACE)
				{
					for (const auto &kv : seen)
					{
						if (!holders.count(kv.first) && kv.second.owner != player && !after.seen.count(kv.first))
						{
							got = str("enemy object %u replaced / removed", (unsigned)kv.first);
							break;
						}
					}
				}
				break;
			}
			case HEAL:
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					if (b && kv.second.owner == player && kv.second.health > b->health + 1.0f)
					{
						got = str("object %u healed %.0f", (unsigned)kv.first, (double)(kv.second.health - b->health));
						break;
					}
				}
				break;
			case GRAB:
				for (const auto &kv : after.seen)
				{
					if (!holders.count(kv.first) && kv.second.owner != player && holders.count(kv.second.containedBy))
					{
						got = str("object %u held by the caster", (unsigned)kv.first);
						break;
					}
				}
				break;
			case TERROR:
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					const bool now = kv.second.terror || (hits && hits->terrified.count(kv.first) && hits->terrified.at(kv.first) >= frame);
					if (kv.second.owner != player && now && !(b && b->terror))
					{
						got = str("object %u terrified (EMOTION_TERROR)", (unsigned)kv.first);
						break;
					}
				}
				break;
			case EMOTION:
			{
				auto matchType = [&](int et) {
					if (et < 0 || et >= EMOTION_TYPE_COUNT)
					{
						return false;
					}
					for (const auto &kv : after.seen)
					{
						const Seen *b = was(kv.first);
						const bool wasSet = b && b->emotions[(size_t)et];
						const bool tracked = hits && hits->emotionFirstSeen.count(et) && hits->emotionFirstSeen.at(et).count(kv.first)
											 && hits->emotionFirstSeen.at(et).at(kv.first) >= frame;
						if (!holders.count(kv.first) && (kv.second.emotions[(size_t)et] || tracked) && !wasSet)
						{
							got = str("object %u received emotion %d", (unsigned)kv.first, et);
							return true;
						}
					}
					if (hits && hits->emotionFirstSeen.count(et))
					{
						for (const auto &kv : hits->emotionFirstSeen.at(et))
						{
							const Seen *b = was(kv.first);
							if (!holders.count(kv.first) && kv.second >= frame && !(b && b->emotions[(size_t)et]))
							{
								got = str("object %u received emotion %d (frame %u)", (unsigned)kv.first, et, (unsigned)kv.second);
								return true;
							}
						}
					}
					return false;
				};
				if (!e.emotions.empty())
				{
					for (int et : e.emotions)
					{
						if (matchType(et))
						{
							break;
						}
					}
				}
				else
				{
					for (int et = 0; et < EMOTION_TYPE_COUNT; ++et)
					{
						if (matchType(et))
						{
							break;
						}
					}
				}
				break;
			}
			case LUA_EVENT:
			{
				for (int et : e.emotions)
				{
					for (const auto &kv : after.seen)
					{
						const Seen *b = was(kv.first);
						const bool wasSet = b && et >= 0 && et < EMOTION_TYPE_COUNT && b->emotions[(size_t)et];
						const bool tracked = hits && hits->emotionFirstSeen.count(et) && hits->emotionFirstSeen.at(et).count(kv.first)
											 && hits->emotionFirstSeen.at(et).at(kv.first) >= frame;
						if (!holders.count(kv.first) && et >= 0 && et < EMOTION_TYPE_COUNT && (kv.second.emotions[(size_t)et] || tracked) && !wasSet)
						{
							got = str("%s triggered emotion %d on object %u", e.modifier.c_str(), et, (unsigned)kv.first);
							break;
						}
					}
					if (!got.empty())
					{
						break;
					}
				}
				if (got.empty() && after.luaEvents > luaEvents)
				{
					got = str("Lua event %s dispatched (%zu -> %zu)", e.modifier.c_str(), luaEvents, after.luaEvents);
				}
				break;
			}
			case META_IMPACT:
				if (after.metaImpactHits > metaImpactHits)
				{
					got = str("metaImpactHits %llu -> %llu", metaImpactHits, after.metaImpactHits);
				}
				break;
			case STEAL_MONEY:
				if (after.playerMoney > playerMoney)
				{
					got = str("player money %u -> %u", playerMoney, after.playerMoney);
				}
				break;
			case PARALYZE:
			{
				const DisabledMaskType mask = (1u << DISABLED_USER_PARALYZED) | (1u << DISABLED_USER_FROZEN);
				for (const auto &kv : after.seen)
				{
					const Seen *b = was(kv.first);
					const DisabledMaskType wasDis = b ? b->disabled : 0;
					if (!holders.count(kv.first) && ((kv.second.disabled & ~wasDis) & mask) != 0)
					{
						got = str("object %u disabled (0x%x)", (unsigned)kv.first, kv.second.disabled);
						break;
					}
				}
				if (got.empty() && hits)
				{
					for (unsigned d : { (unsigned)DISABLED_USER_PARALYZED, (unsigned)DISABLED_USER_FROZEN })
					{
						if (hits->disabledFirstSeen.count(d))
						{
							for (const auto &kv : hits->disabledFirstSeen.at(d))
							{
								const Seen *b = was(kv.first);
								if (!holders.count(kv.first) && kv.second >= frame && !(b && (b->disabled & (1u << d)) != 0))
								{
									got = str("object %u disabled bit %u (frame %u)", (unsigned)kv.first, d, (unsigned)kv.second);
									break;
								}
							}
						}
						if (!got.empty())
						{
							break;
						}
					}
				}
				break;
			}
			case MODEL_CONDITION:
			{
				auto allowedObj = [&](ObjectID id, const Seen *s) {
					if (e.scope == Expect::SCOPE_HOLDERS)
					{
						return holders.count(id) > 0;
					}
					if (e.scope == Expect::SCOPE_PLAYER)
					{
						return s && s->owner == player;
					}
					return true;
				};
				for (int bit : e.modelConditions)
				{
					if (bit < 0)
					{
						continue;
					}
					for (const auto &kv : after.seen)
					{
						if (!allowedObj(kv.first, &kv.second))
						{
							continue;
						}
						const Seen *b = was(kv.first);
						const bool had = b && bitSet(b->modelConditions, bit);
						if (!had && bitSet(kv.second.modelConditions, bit))
						{
							got = str("object %u set model condition %d", (unsigned)kv.first, bit);
							break;
						}
					}
					if (got.empty() && hits && hits->modelConditionFirstSeen.count(bit))
					{
						for (const auto &kv : hits->modelConditionFirstSeen.at(bit))
						{
							auto ait = after.seen.find(kv.first);
							const Seen *s = ait != after.seen.end() ? &ait->second : was(kv.first);
							if (!allowedObj(kv.first, s))
							{
								continue;
							}
							const Seen *b = was(kv.first);
							if (kv.second >= frame && !(b && bitSet(b->modelConditions, bit)))
							{
								got = str("object %u set model condition %d (frame %u)", (unsigned)kv.first, bit, (unsigned)kv.second);
								break;
							}
						}
					}
					if (!got.empty())
					{
						break;
					}
				}
				break;
			}
			case HARVEST:
			{
				const int prepBit = CombatNames::modelCondition("HARVEST_PREPARATION");
				const int actBit = CombatNames::modelCondition("HARVEST_ACTION");
				for (ObjectID id : holders)
				{
					const Seen *b = was(id);
					auto it = after.seen.find(id);
					if (it != after.seen.end())
					{
						if (it->second.harvesting && !(b && b->harvesting))
						{
							got = str("caster %u entered AI_HARVEST", (unsigned)id);
							break;
						}
						if ((bitSet(it->second.modelConditions, prepBit) && !(b && bitSet(b->modelConditions, prepBit)))
							|| (bitSet(it->second.modelConditions, actBit) && !(b && bitSet(b->modelConditions, actBit))))
						{
							got = str("caster %u showed harvest model condition", (unsigned)id);
							break;
						}
					}
					if (hits && hits->harvestFirstSeen.count(id) && hits->harvestFirstSeen.at(id) >= frame && !(b && b->harvesting))
					{
						got = str("caster %u entered AI_HARVEST (frame %u)", (unsigned)id, (unsigned)hits->harvestFirstSeen.at(id));
						break;
					}
				}
				break;
			}
			case TELEPORT:
				for (ObjectID id : holders)
				{
					const Seen *b = was(id);
					auto it = after.seen.find(id);
					if (b && it != after.seen.end() && dist2d(b->pos, it->second.pos) > 100.0f)
					{
						got = str("the caster moved %.0f in an instant", (double)dist2d(b->pos, it->second.pos));
						break;
					}
				}
				break;
			case BOUNTY:
				if (after.bounty != bounty)
				{
					got = str("the player's bounty went from %g to %g", (double)bounty, (double)after.bounty);
				}
				break;
			case WEAPON_MODE:
				if (after.weaponModeActive > weaponModeActive)
				{
					got = str("%u weapon modes on", after.weaponModeActive - weaponModeActive);
				}
				break;
			case WEAPON_FLAGS:
			case STATUS:
			case MOUNT:
				for (ObjectID id : holders)
				{
					if (e.kind == MOUNT && !e.modifier.empty())
					{
						break; // the replacement by the MountedTemplate (the caller's check) is the only result
					}
					const Seen *b = was(id);
					auto it = after.seen.find(id);
					const bool status = it != after.seen.end() && b && it->second.status != b->status, flags = it != after.seen.end() && b && it->second.weaponFlags != b->weaponFlags;
					if (e.kind == STATUS ? status : e.kind == WEAPON_FLAGS ? flags : status || flags)
					{
						got = e.kind == WEAPON_FLAGS || !status ? "the weapon set flags changed" : "the caster's status changed";
						break;
					}
				}
				break;
			}
			if (got.empty())
			{
				std::string detail;
				if (e.kind == DAMAGE && hits && !e.weapons.empty())
				{
					float best = 0.0f, nominal = 0.0f;
					for (const HitTracker::Hit &x : hits->hits)
					{
						if (x.frame >= frame && x.amount > best)
						{
							best = x.amount;
							auto it = hitNominal.find(x.victim);
							nominal = it == hitNominal.end() ? 0.0f : it->second;
						}
					}
					detail = str(" (the caster's biggest hit after the cast %.0f, the weapon's against that victim %.0f; %d victims hit in a frame before the cast)", (double)best,
								 (double)nominal, hits->maxVictims(0, frame));
				}
				if (e.kind == SPAWN || e.kind == REPLACE)
				{
					std::string names;
					for (const std::string &n : e.templates)
					{
						names += (names.empty() ? "" : ", ") + n;
					}
					detail = names.empty() ? " (its OCL names no CreateObject template)" : " (none of " + names + " made)";
				}
				if (e.kind == FIRE_LOGIC)
				{
					detail = " (FireLogicNugget)";
				}
				if (e.kind == WEAPON_FLAGS || e.kind == STATUS)
				{
					for (ObjectID id : holders)
					{
						const Seen *b = was(id);
						auto it = after.seen.find(id);
						detail += it == after.seen.end() ? str(" (holder %u gone)", (unsigned)id)
														 : b ? str(" (holder %u flags %08x -> %08x)", (unsigned)id, b->weaponFlags[0], it->second.weaponFlags[0]) : "";
					}
				}
				missing += e.what + ": no result" + detail + "; ";
			}
			else
			{
				out.push_back(e.what + ": " + got);
			}
		}
		for (const std::string &c : unasserted)
		{
			missing += "no effect of " + c + " is asserted; ";
		}
		if (expects.empty() && unasserted.empty())
		{
			if (!skippedNotes.empty())
			{
				for (const std::string &sn : skippedNotes)
				{
					missing += sn + "; ";
				}
			}
			else
			{
				missing += "no effect of it is asserted (module SpecialPowerModule); ";
			}
		}
		std::string all;
		for (const std::string &x : out)
		{
			all += (all.empty() ? "" : ", ") + x;
		}
		for (const std::string &sn : skippedNotes)
		{
			all += (all.empty() ? "" : ", ") + sn;
		}
		return all;
	}

	// DAMAGE: the special weapon's biggest nugget against each object present before the cast (filled by nominals())
	std::map<ObjectID, float> hitNominal;
	std::map<ObjectID, float> hitSum;
	void nominals(GameLogic &logic)
	{
		for (const Expect &e : expects)
		{
			if (e.kind != DAMAGE)
			{
				continue;
			}
			std::vector<const WeaponTemplate *> wts = e.weapons;
			if (e.weapon && std::find(wts.begin(), wts.end(), e.weapon) == wts.end())
			{
				wts.push_back(e.weapon);
			}
			for (const WeaponTemplate *wt : wts)
			{
				if (!wt)
				{
					continue;
				}
				for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
				{
					if (o->getControllingPlayer() == player || !o->getBodyModule())
					{
						continue;
					}
					std::string text;
					const std::vector<float> h = expectedHits(*wt, *o, player, text);
					if (h.empty())
					{
						continue;
					}
					// the biggest single nugget (not the sum expectedHits appends): a frame's drop holds at least it when the weapon's hit lands
					float m = 0.0f;
					for (size_t k = 0; k < (h.size() > 1 ? h.size() - 1 : h.size()); ++k)
					{
						m = std::max(m, h[k]);
					}
					if (m > 0.0f)
					{
						hitNominal[o->getID()] = std::max(hitNominal[o->getID()], m);
						hitSum[o->getID()] = std::max(hitSum[o->getID()], h.back()); // every nugget at once (expectedHits' last entry with several)
					}
				}
			}
		}
	}
};

} // namespace sweep_probe
