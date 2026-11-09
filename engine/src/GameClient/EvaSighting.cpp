// OpenBFME. GPL-3.0. See EvaSighting.h.

#include "GameClient/EvaSighting.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LogicSnapshot.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/EvaDamageNotice.h"
#include "GameLogic/Object/EvaSightingNotice.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"

namespace
{
int kindBit(const char *name)
{
	return ObjectTemplateInfoBuilder::kindOfIndex(name);
}
} // namespace

void EvaSighting::update(DrawableManager &drawables, const LogicSnapshot &snapshot, GameLogic &logic, Stats &stats)
{
	static const int kIgnoredInGui = kindBit("IGNORED_IN_GUI");
	static const int kStructure = kindBit("STRUCTURE");

	const Player *localPlayer = logic.players().getLocalPlayer();
	if (!localPlayer)
	{
		return;
	}

	const float voiceScanRange = AudioApi::enemySightedMaxVoicePositionScanRange();

	for (size_t i = 1; i < drawables.slotCount(); ++i)
	{
		Drawable *d = drawables.find((DrawableID)i);
		if (!d || !d->takeShroudRevealed())
		{
			continue;
		}
		++stats.sightings;

		// RW 0x678CC0: TheGameLogic->m_logicFrame > 5 (initial objects do not announce)
		if (snapshot.frame <= 5)
		{
			continue;
		}

		Object *obj = logic.findObjectByID(d->getObjectID());
		if (!obj)
		{
			continue;
		}
		const ThingTemplate *tt = d->getTemplate();
		if (!tt)
		{
			continue;
		}

		// RW 0x678CCA: Object::isVisibleTo(localPlayer, true)
		const bool isEnemy = EvaDamage::isVisibleTo(logic, *obj, localPlayer, true);

		if (isEnemy)
		{
			// ---- Branch 1: Sighted as enemy ----
			// 1. EvaEnemyObjectSightedEvent / EvaEnemyObjectSightedAfterRespawnEvent (RW 0x678CDC..0x678DE4)
			const bool effectivelyDead = obj->isEffectivelyDead();
			const bool ignoredInGui = kIgnoredInGui >= 0 && obj->isKindOf((unsigned)kIgnoredInGui);

			if (!effectivelyDead && !ignoredInGui && !d->evaEnemySighted())
			{
				const std::string enemySightedEvent = EvaDamage::templateEvent(*tt, "EvaEnemyObjectSightedEvent");
				if (!enemySightedEvent.empty())
				{
					ObjectID slaverId = INVALID_ID;
					const bool isSlave = EvaSighting::isSlaveControlledBySlaver(*obj, slaverId);
					bool proceed = false;

					if (isSlave)
					{
						// RW 0x678D24: slave with useSlaverAsControl skips owner walk
						proceed = true;
					}
					else
					{
						// RW 0x6ABABD / RW 0x672D90: owner walk
						const Player *owner = obj->getControllingPlayer();
						const bool hasAnother = owner && EvaSighting::ownerHasAnotherSightedObject(
							logic, *obj, *owner, *localPlayer, enemySightedEvent,
							[&drawables](ObjectID id) {
								const Drawable *otherD = drawables.findByObject(id);
								return !otherD || otherD->shroudLook().fullyObscured;
							});
						if (hasAnother)
						{
							++stats.ownerSilenced;
						}
						else
						{
							proceed = true;
						}
					}

					if (proceed)
					{
						// RW 0x672DFB: find closest local object for voice position
						Coord3D voicePos;
						if (EvaSighting::findClosestLocalObject(logic, *obj, *localPlayer, voiceScanRange, voicePos))
						{
							const std::string respawnEvent = EvaDamage::templateEvent(*tt, "EvaEnemyObjectSightedAfterRespawnEvent");
							const bool isRespawn = d->evaEnemySightedAfterRespawn() && !respawnEvent.empty();
							const std::string &chosenEvent = isRespawn ? respawnEvent : enemySightedEvent;

							const Coord3D jumpPos = *d->getPosition();
							const bool reported = AudioApi::reportEva(chosenEvent, &voicePos, &jumpPos);
							if (reported || AudioApi::currentEva() == nullptr)
							{
								if (isRespawn)
								{
									++stats.respawnSightedReported;
								}
								else
								{
									++stats.enemySightedReported;
								}
								stats.lastEvent = chosenEvent;
							}

							// RW 0x678DB5 / 0x6709A2: if slave, mark drawable, master slaver, and sibling slaves
							if (isSlave)
							{
								d->setEvaEnemySighted(true);
								if (slaverId != INVALID_ID)
								{
									if (Drawable *slaverD = drawables.findByObject(slaverId))
									{
										slaverD->setEvaEnemySighted(true);
									}
									if (Object *slaverObj = logic.findObjectByID(slaverId))
									{
										EvaSighting::forEachSpawnedSlave(*slaverObj, [&drawables](ObjectID siblingId) {
											if (Drawable *sd = drawables.findByObject(siblingId))
											{
												sd->setEvaEnemySighted(true);
											}
										});
									}
								}
							}
						}
						else
						{
							++stats.voiceSuppressedNoFriendly;
						}
					}
				}
			}

			// 2. CastleMemberBehavior / EvaEnemyCastleSightedEvent (RW 0x678DE7..0x678EC5)
			if (kStructure >= 0 && obj->isKindOf((unsigned)kStructure) && !d->evaCastleMemberSighted())
			{
				ObjectID castleId = INVALID_ID;
				std::string castleEvent;
				if (EvaSighting::getCastleInfo(*obj, castleId, castleEvent))
				{
					Drawable *castleD = drawables.findByObject(castleId);
					if (castleD && !castleD->evaCastleMemberSighted())
					{
						Coord3D voicePos;
						if (EvaSighting::findClosestLocalObject(logic, *obj, *localPlayer, voiceScanRange, voicePos))
						{
							const Coord3D jumpPos = *d->getPosition();
							const bool reported = AudioApi::reportEva(castleEvent, &voicePos, &jumpPos);
							if (reported || AudioApi::currentEva() == nullptr)
							{
								++stats.castleSightedReported;
								stats.lastEvent = castleEvent;
							}
						}
						else
						{
							++stats.voiceSuppressedNoFriendly;
						}
					}

					// RW 0x678EB6 / 0x6709EA: mark bit 0x40 on castle drawable and all member drawables
					if (Object *castleObj = logic.findObjectByID(castleId))
					{
						EvaSighting::forEachCastleMember(*castleObj, [&drawables](ObjectID mid) {
							if (Drawable *md = drawables.findByObject(mid))
							{
								md->setEvaCastleMemberSighted(true);
							}
						});
					}
				}
			}

			// 3. EvaOnFirstSightingEventEnemy (RW 0x678EC8..0x678F45)
			if (!d->evaFirstSighted() && !effectivelyDead)
			{
				d->markEvaFirstSighted();
				const std::string firstSightingEvent = EvaDamage::templateEvent(*tt, "EvaOnFirstSightingEventEnemy");
				if (!firstSightingEvent.empty())
				{
					Coord3D voicePos;
					if (EvaSighting::findClosestLocalObject(logic, *obj, *localPlayer, voiceScanRange, voicePos))
					{
						const Coord3D jumpPos = *d->getPosition();
						const bool reported = AudioApi::reportEva(firstSightingEvent, &voicePos, &jumpPos);
						if (reported || AudioApi::currentEva() == nullptr)
						{
							++stats.firstSightingEnemyReported;
							stats.lastEvent = firstSightingEvent;
						}
					}
					else
					{
						++stats.voiceSuppressedNoFriendly;
					}
				}
			}
		}
		else
		{
			// ---- Branch 2: Sighted as non-enemy (RW 0x678EEC..0x678F45) ----
			// RW 0x67055A: not clientHidden
			if (!d->clientHidden())
			{
				if (!d->evaFirstSighted() && !obj->isEffectivelyDead())
				{
					d->markEvaFirstSighted();
					const std::string firstSightingEvent = EvaDamage::templateEvent(*tt, "EvaOnFirstSightingEventNonEnemy");
					if (!firstSightingEvent.empty())
					{
						Coord3D voicePos;
						if (EvaSighting::findClosestLocalObject(logic, *obj, *localPlayer, voiceScanRange, voicePos))
						{
							const Coord3D jumpPos = *d->getPosition();
							const bool reported = AudioApi::reportEva(firstSightingEvent, &voicePos, &jumpPos);
							if (reported || AudioApi::currentEva() == nullptr)
							{
								++stats.firstSightingNonEnemyReported;
								stats.lastEvent = firstSightingEvent;
							}
						}
						else
						{
							++stats.voiceSuppressedNoFriendly;
						}
					}
				}
			}
		}
	}
}
