// OpenBFME. GPL-3.0.
// See GameLogic/Combat/ProjectileLauncher.h for the target facts.

#include "GameLogic/Combat/ProjectileLauncher.h"

#include "Common/Player.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponNugget.h"

void ObjectProjectileLauncher::launch(GameLogic &logic, const ProjectileShot &shot)
{
	Object *source = shot.source != INVALID_ID ? logic.findObjectByID(shot.source) : nullptr;
	if (!source || !shot.nugget)
	{
		return;
	}
	const std::string &name = shot.nugget->m_projectileTemplateName;
	// TARGET RW 0x90FACB (object target) / 0x90FDAA (position target):
	// ProjectileSelf selects the firing object BEFORE testing ProjectileTemplateName.
	// Leap weapons deliberately omit that name; they wake the hero's own projectile module.
	Object *projectile = nullptr;
	// RW 0x90FA81 .. 0x90FAC9 (lane SPELL-2): ProjectileFilterInContainer (the fired weapon's template + 0x120; RW 0x762977 the empty test) on a source
	// that has a contain ([object + 0x258], RW 0x90FA90) -> the source's launch transform (Weapon RW 0x6CAB85, RW 0x90FAAA) and then contain slot 0x13C
	// with the filter (RW 0x90FAC3, OpenContain RW 0x866367): the projectile is the first matching CONTained object, taken out of the container (contain
	// slot 0xA4 at RW 0x8663AC) -- the cave troll's held goblin. No match: null, and the launch has no object to fly (RW 0x90FB3D reads the template of a
	// null projectile only after the branch, so the shot ends there in retail's own dereference; the port stops before it).
	if (shot.weapon && shot.weapon->m_projectileFilterInContainer.flag && source->getContain())
	{
		projectile = source->getContain()->takeContainedMatching(shot.weapon->m_projectileFilterInContainer); // RW 0x90FAC3
		if (!projectile)
		{
			return;
		}
	}
	else if (shot.weapon && shot.weapon->m_projectileSelf)
	{
		projectile = source; // RW 0x90FACB / 0x90FACE: ProjectileSelf selects the firing object, the projectile template is not read
	}
	else
	{
		if (name.empty() || name == "None" || name == "NONE")
		{
			return; // RW 0x90FAF7: no projectile template
		}
		const ThingTemplate *tt = logic.things().findTemplate(name);
		if (!tt)
		{
			logic.reportError("weapon " + (shot.weapon ? shot.weapon->getName() : std::string("?")) + ": the ProjectileNugget's projectile '" + name + "' is not an object template");
			return;
		}
		Player *owner = source->getControllingPlayer();
		Team *team = owner ? owner->getDefaultTeam() : nullptr;
		projectile = logic.newObject(tt, team, ObjectStatusMaskType{});
		if (!projectile)
		{
			logic.reportError("weapon " + (shot.weapon ? shot.weapon->getName() : std::string("?")) + ": the projectile '" + name + "' could not be created");
			return;
		}
	}
	ProjectileUpdateInterface *iface = nullptr;
	for (const std::unique_ptr<BehaviorModule> &m : projectile->modules())
	{
		iface = dynamic_cast<ProjectileUpdateInterface *>(m.get());
		if (iface)
		{
			break;
		}
	}
	if (!iface)
	{
		logic.destroyObject(projectile); // RW 0x90FB6C
		logic.reportError("projectile '" + name + "' has no projectile update module (BezierProjectileBehavior)");
		return;
	}
	projectile->setProducer(source);
	int slot = shot.slot;
	const int override = shot.nugget->m_weaponLaunchBoneSlotOverride;
	if (override >= 0 && override < 6)
	{
		slot = override;
	}
	const Object *victim = shot.victim != INVALID_ID ? logic.findObjectByID(shot.victim) : nullptr;
	if (victim)
	{
		iface->projectileLaunchAtObjectOrPosition(victim, nullptr, source, slot, shot.barrel, shot.weapon, shot.warhead);
	}
	else
	{
		Coord3D pos = shot.targetPosition;
		iface->projectileLaunchAtObjectOrPosition(nullptr, &pos, source, slot, shot.barrel, shot.weapon, shot.warhead);
	}
	// ---- the firing weapon's projectile stream (ProjectileStreamName, RW 0x90FB7A .. 0x90FC5A) ----
	// Read off the object-target deliver, which is the one RW 0x6CD1FD arms (RW 0x6CF012 is privateFireWeapon's step 6):
	//   RW 0x90FB7A  mov edi, [RW 0xDE412C] / RW 0x90FB80 push [esi + 0x5C] / RW 0x90FB85 call RW 0x449681   the stream object of the SHOOTING weapon
	//                (esi is the weapon the warhead block was given, RW 0x90F9F5); missing -> the record ends (RW 0x90FB8E jz 0x90FC5F, the outer return)
	//   RW 0x90FB94  push [esi + 8] / RW 0x90FB99 call RW 0x449681   the owner; missing -> the record ends (RW 0x90FBA2)
	//   RW 0x90FBE8  lea eax, [esi + 0x38] / RW 0x90FBEE call RW 0x74094C   setPosition of the STREAM object (Thing::setPosition, RW 0x70C201) to the
	//                OWNER's geometry centre: the stream follows the shooter, not the target
	//   RW 0x90FBDD  call RW 0x68BDA5 with the key `[RW 0xDE9FCC]` (the string `ProjectileStreamUpdate` at RW 0xBE40E8, cached once): the stream
	//                object's update interface; null -> the record ends (RW 0x90FBE6)
	//   RW 0x90FBF3  the victim argument (RW 0x90FBF8: null keeps ecx 0), then [eax + 0x74] is its id
	//   RW 0x90FC5A  call RW 0x7408A8(streamUpdate, owner id, victim | null, victim id | 0, projectile id, &owner geometry centre)
	//                ([0x90FC4A] = add eax, 0x38 on the owner) -> ProjectileStreamUpdate::addProjectile
	// Both positions are the OWNER's geometry centre (+ 0x38), not the target's: the segment the stream draws runs from the shooter to the projectile.
	// The position-target deliver (RW 0x90FC70) has no such block: a shot with no victim is never recorded in a stream, and neither is a shot whose
	// nugget is not there to reach this code.
	if (!victim)
	{
		return; // RW 0x90FC70, the position-target deliver, has no stream block: only an object target is recorded
	}
	const ObjectID streamID = (ObjectID)shot.streamID; // RW 0x90FB80: the firing weapon's + 0x5C
	if (streamID == INVALID_ID)
	{
		return; // the weapon holds no stream object (no ProjectileStreamName, or RW 0x6CD1FD never made one)
	}
	Object *stream = logic.findObjectByID(streamID); // RW 0x90FB85, RW 0x90FB8E `je` on the pointer alone
	if (!stream)
	{
		return;
	}
	const Coord3D &ownerCentre = *source->getPosition(); // RW 0x90FBEE / 0x90FC4A: esi + 0x38
	stream->setPosition(&ownerCentre);                   // RW 0x74094C
	auto *streamUpdate = dynamic_cast<ProjectileStreamUpdate *>(stream->findModule("ProjectileStreamUpdate")); // RW 0x68BDA5
	if (!streamUpdate)
	{
		return; // RW 0x90FBE6
	}
	streamUpdate->addProjectile(source->getID(), projectile->getID(), victim->getID(), &ownerCentre); // RW 0x7408A8
}
