// OpenBFME. GPL-3.0.
// See GameClient/Drawable.h for the sources and the stops.

#include "GameClient/Drawable.h"

#include "Common/Audio/AudioRequests.h"

#include "Common/AsciiString.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/MapObjectDrawables.h"
#include "GameClient/ModelConditionAudioLoopClientBehavior.h"
#include "GameClient/RenderInterpolation.h"
#include "GameClient/StealthLook.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawVariants.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawBones.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DQuadrupedDraw.h"
#include "Common/ModelState.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object/ObjectGeometry.h"

#include <cmath>
#include <cstring>
#include <variant>

DrawEntry::DrawEntry() = default;
DrawEntry::~DrawEntry() = default;
DrawEntry::DrawEntry(DrawEntry &&) noexcept = default;
DrawEntry &DrawEntry::operator=(DrawEntry &&) noexcept = default;

Drawable::Drawable(DrawServices &services, const ThingTemplate *tt, DrawableID id, ObjectID objectId, const std::vector<ClientEvent::ClientModule> &clientModules,
	std::vector<std::string> &problems, int variation)
	: Thing(tt)
	, m_services(services)
	, m_id(id)
	, m_objectId(objectId)
	, m_variation(variation)
{
	m_services.host.setChunkName(tt->getName()); // the Lua chunk name of the BeginScript runs: the object template's name
	if (const FieldValue *v = tt->findField("Scale"))
	{
		if (const float *f = std::get_if<float>(v))
		{
			m_scale = *f; // the instance scale starts as the template's Scale (RW 0x679FD7, default 1.0, RW 0x74008C)
		}
	}
	m_buildingEntries = true;
	for (const ThingTemplate::Nugget &n : tt->drawModules().nuggets())
	{
		addEntry(services, n, problems);
	}
	m_buildingEntries = false;
	// lane BUILD-4: a module request a draw module's first state script made before every module existed applies now, in call order
	for (const std::pair<std::string, bool> &r : m_pendingModuleRequests)
	{
		showModule(r.first, r.second, false);
	}
	m_pendingModuleRequests.clear();
	// the ClientUpdate and ClientBehavior modules (after the draw modules, ZH Drawable.cpp); then onObjectCreated on all of them
	for (const ClientEvent::ClientModule &cm : clientModules)
	{
		std::unique_ptr<Module> mod = ModuleFactory::newResolvedModule(this, cm.resolved, cm.name, cm.data);
		DrawableModule *dm = dynamic_cast<DrawableModule *>(mod.get());
		if (!dm)
		{
			problems.push_back(cm.name + ": the client module class did not make a DrawableModule");
			continue;
		}
		mod.release();
		m_clientModules.push_back(std::unique_ptr<DrawableModule>(dm));
		if (auto *loop = dynamic_cast<ModelConditionAudioLoopClientBehavior *>(dm))
		{
			m_audioLoops.push_back(loop); // RW 0x67449C: the list + 0x158 the flush walks
		}
	}
	for (std::unique_ptr<DrawableModule> &m : m_clientModules)
	{
		m->onObjectCreated();
	}
}

bool Drawable::isKindOfName(const char *name) const
{
	const int bit = ObjectTemplateInfoBuilder::kindOfIndex(name);
	return bit >= 0 && MaskTest(m_kindOf, (unsigned)bit);
}

bool Drawable::isKindOfIndex(int bit) const
{
	return bit >= 0 && MaskTest(m_kindOf, (unsigned)bit);
}

void Drawable::reactToTransformChange(const Coord3D *oldPos, float oldAngle)
{
	if (m_reactionMuted)
	{
		return;
	}
	for (std::unique_ptr<DrawableModule> &m : m_clientModules)
	{
		m->reactToTransformChange(oldPos, oldAngle); // lane AUDIO-4
	}
	for (DrawEntry &e : m_entries)
	{
		if (e.lightDraw)
		{
			e.lightDraw->reactToTransformChange(oldPos, oldAngle);
		}
	}
}

void Drawable::friend_boundToObject()
{
	for (std::unique_ptr<DrawableModule> &m : m_clientModules)
	{
		m->onDrawableBoundToObject();
	}
}

void Drawable::createSelectionFeedback()
{
	// RW 0x672ED1 / BFME2 0x672BAB: walks the client modules calling slot 17 (+0x44)
	for (std::unique_ptr<DrawableModule> &m : m_clientModules)
	{
		m->createSelectionFeedback();
	}
}

void Drawable::removeSelectionFeedback()
{
	// RW 0x672F0D / BFME2 0x672BE7: walks the client modules calling slot 18 (+0x48)
	for (std::unique_ptr<DrawableModule> &m : m_clientModules)
	{
		m->removeSelectionFeedback();
	}
}

void Drawable::addEntry(DrawServices &services, const ThingTemplate::Nugget &nugget, std::vector<std::string> &problems)
{
	DrawEntry e;
	e.className = nugget.name;
	e.tag = nugget.tag;
	e.data = nugget.data.get();
	// lane STEALTHLOOK-1 (RW 0x679AE8): +0x3AB, a draw module's ModuleData fails its +0x78 query (the model draws' AffectedByStealth)
	m_cannotHideByStealth = m_cannotHideByStealth || !StealthLook::moduleDataAffectedByStealth(e.data);
	const W3DDrawClassInfo *ci = W3DDrawModules::find(nugget.name);
	if (!ci)
	{
		problems.push_back(nugget.name + ": not a registered draw class");
		e.kind = W3D_DRAWKIND_NOT_DRAWN;
		m_entries.push_back(std::move(e));
		return;
	}
	e.kind = ci->kind;
	if (e.kind == W3D_DRAWKIND_MODEL)
	{
		const W3DModelDrawModuleData *data = dynamic_cast<const W3DModelDrawModuleData *>(nugget.data.get());
		if (!data)
		{
			problems.push_back(nugget.name + ": module data is not typed");
		}
		else
		{
			try
			{
				W3DScriptedModelDraw::Options o;
				o.scale = m_scale;
				o.instanceScale = [this]() { return m_scale; }; // PORT-126: live scale after Drawable::setInstanceScale
				o.variation = m_variation; // RandomTexture (RW 0x4C238A)
				o.buildBones = false; // turrets, barrels and particle bones are not drawn (S-095 / S-097)
				o.cameraFadeAvailable = static_cast<bool>(services.cameraPosition); // PORT-49: the device camera source is installed before the load
				// PORT-106 (issue #266): the MultiPlayerOnly gate (RW 0x4C72E1..0x4C730C). The inputs: isInMultiplayerGame (RW 0x441B7C)
				// from the logic through DrawServices, and GlobalData +0xD45 — always 0 in RotWK 2.01 (ctor RW 0x6435AF; the only other
				// writer RW 0x919FB6 restores a snapshot, so the byte never turns on: full .text scan). Without the logic source (a test
				// rig, the map-object runtime) the gate is not applied — RW always has TheGameLogic (RW 0xDE412C).
				o.multiPlayerOnlySkipped = services.inMultiplayerGame && data->multiPlayerOnlySkipsBody(services.inMultiplayerGame(), false);
				// AUDIO-2: Lua CurDrawablePlaySound (RW 0x73529F) plays for this drawable through the installed audio manager
				const DrawableID drawableId = m_id;
				o.playSound = [drawableId](const std::string &name) { AudioApi::playSoundForDrawable(name, (std::uint32_t)drawableId); };
				// lane FX-2: the client effect player, looked up when the draw calls (it may be installed after this drawable was made)
				DrawServices *svc = &services;
				const DrawableID id = m_id;
				o.enteringStateFX = [svc, id](const std::string &fx) { return svc->fx && svc->fx->enteringStateFX(id, fx); };
				o.particleSysBonesPlayed = [svc]() { return svc->fx != nullptr; };
				// lane SMOOTH-3: Distance animations sync to the object's speed (RW 0x4B67D4), taken from the presented snapshot (Drawable lives in a unique_ptr)
				o.objectSpeed = [this]() { return m_moveSpeed; };
				// lane FX-3 (QA-1 U9): the draw scripts' target questions answer from the object's target record (RW 0x73667D / 0x734A75); a drawable
				// without an object answers false / 0 (retail: nil)
				o.targetKindOf = [this](const std::string &kind) { return m_scriptTarget.isTargetKindOf(kind); };
				o.targetBearing = [this]() { return m_scriptTarget.hasObject ? m_scriptTarget.bearing() : 0.0f; };
				// lane ANIM-1: UseWeaponTiming (RW 0x4BEE24 .. 0x4BEEA2) divides by the object's weapon cycle as the flush carried it; no current weapon: -1,
				// the speed factor is left alone (RW 0x4BEE3F)
				o.weaponTimingFrames = [this]() { return m_hasWeaponTiming ? m_weaponTimingFrames : -1; };
				// lane BUILD-4: CurDrawableShowSubObject / HideSubObject with a draw module's tag (RW 0x734EF4) and CurDrawableShowModule / HideModule reach
				// Drawable::showModule (RW 0x6789B4); the template answers which tags exist (the drawable makes every draw module of its template, S-114)
				o.hasModule = [this](const std::string &tag) { return ClientEventRecorder::templateHasDrawModuleTag(*m_template, tag); };
				o.setModuleVisible = [this](const std::string &tag, bool visible) { scriptModuleVisible(tag, visible); };
				if (const W3DHordeModelDrawModuleData *hd = dynamic_cast<const W3DHordeModelDrawModuleData *>(nugget.data.get()))
				{
					e.draw.reset(new W3DHordeModelDraw(*hd, 2, services.drawAssets, services.random, &services.host, o)); // LOD HIGH (S-114)
				}
				else if (const W3DQuadrupedDrawModuleData *qd = dynamic_cast<const W3DQuadrupedDrawModuleData *>(nugget.data.get()))
				{
					e.draw.reset(new W3DQuadrupedDraw(*qd, services.drawAssets, services.random, &services.host, o));
				}
				else
				{
					e.draw.reset(new W3DScriptedModelDraw(*data, services.drawAssets, services.random, &services.host, o));
				}
				e.draw->setBaseHouseColor(Render_House_Color_Params(m_houseColor, m_customKind, m_customColors));
				e.noRotate = data->m_noRotate;
				e.attachToBone = data->m_attachToDrawableBone;
				e.hasTrackMarks = !data->m_trackFile.empty(); // lane PORT-69: RW 0x4C4E69 / 0x47A7DE
				m_hasTrackMarks = m_hasTrackMarks || e.hasTrackMarks;
				e.animated = e.draw->needsAdvance(); // W3DScriptedModelDraw::needsAdvance: a blend runs whatever track 0's mode (S-090)
				// the draw runtime's errors and stops are collected by DrawableManager (DrawEntry::draw->errors() / stops()), after the script hooks ran
			}
			catch (const std::exception &ex)
			{
				problems.push_back(nugget.name + ": draw runtime failed: " + ex.what());
				e.draw.reset();
			}
			if (e.draw)
			{
				if (const auto *supplyData = dynamic_cast<const W3DSupplyDrawModuleData *>(data))
				{
					e.supply.reset(new W3DSupplyDraw(*m_template, *supplyData, services.drawAssets));
				}
			}
		}
	}
	else
	{
		// tree / prop / floor / nothing / effect: the model the module shows for the empty flag set
		MapDrawModule m;
		m.className = nugget.name;
		m.tag = nugget.tag;
		m.kind = e.kind;
		m.data = nugget.data.get();
		std::vector<std::string> errors;
		MapObjectCreation::resolveDrawModel(m, ModelConditionFlags(), m_template->getName(), errors);
		for (const std::string &err : errors)
		{
			problems.push_back(err);
		}
		e.staticModel = m.model;
		e.notDrawnReason = m.notDrawnReason;
		// RENDER-2: a floor draw hidden by its HideIfModelConditions at creation (resolveDrawModel with no flags) is still the floor the structure shows
		// once the flags change: keep its model and its conditions, and let setModelConditionFlags decide (the foundation floor while being built)
		if (const W3DFloorDrawModuleData *fl = dynamic_cast<const W3DFloorDrawModuleData *>(nugget.data.get()))
		{
			e.hideIf = fl->m_hideIfModelConditions;
		}
		// lane LOCAL-28: W3DLightDraw (RW 0x004CE750 / DONOR BFME2 0x004CEF80)
		if (ci->kind == W3D_DRAWKIND_NOT_DRAWN && (nugget.name == "W3DLightDraw" || dynamic_cast<const W3DLightDrawModuleData *>(nugget.data.get()) != nullptr))
		{
			const auto *ld = dynamic_cast<const W3DLightDrawModuleData *>(nugget.data.get());
			if (ld)
			{
				e.lightDraw = std::make_unique<W3DLightDraw>(this, ld);
				e.attachToBone = ld->m_attachToBone;
				e.animated = true;
			}
		}
	}
	m_entries.push_back(std::move(e));
}

bool Drawable::hasAnimatedEntry() const
{
	for (const DrawEntry &e : m_entries)
	{
		if (e.animated && (e.draw || e.lightDraw))
		{
			return true;
		}
	}
	return false;
}

void Drawable::setModelConditionFlags(const ModelConditionFlags &flags)
{
	m_flags = flags;
	++m_changeCount;
	// RENDER-2: a W3DFloorDraw is not drawn while one of its HideIfModelConditions sets is all in the flags (the rule MapObjectCreation::resolveDrawModel applies
	// at creation; e.g. the barracks' floor during AWAITING_CONSTRUCTION / PARTIALLY_CONSTRUCTED)
	for (DrawEntry &e : m_entries)
	{
		bool hide = false;
		for (const ModelConditionFlags &h : e.hideIf)
		{
			hide = hide || (h.any() && flags.testForAll(h));
		}
		e.conditionHidden = hide;
	}
	m_services.host.setChunkName(m_template->getName());
	for (DrawEntry &e : m_entries)
	{
		if (e.draw)
		{
			e.draw->setModelConditionFlags(flags);
			e.animated = e.draw->needsAdvance();
			if (e.supply)
			{
				e.supply->refresh(*e.draw, flags, m_scale); // model-state changes clear plain sub-object ops; reapply the live supply status
			}
		}
	}
	for (ModelConditionAudioLoopClientBehavior *loop : m_audioLoops)
	{
		loop->modelConditionFlagsChanged(flags); // RW 0x67449C -> 0x8D04CA
	}
}

ModelConditionFlags Drawable::dependencySharedModelFlags() const
{
	// the union of the draws' sets, a word at a time (lane BIGBATTLE-1: the bit loop ran for every contained drawable every render frame;
	// MODELCONDITION_COUNT is the type's width and its unused high bits are zero, so the words give the same set)
	ModelConditionFlags out;
	for (const DrawEntry &e : m_entries)
	{
		if (e.draw)
		{
			out.set(e.draw->dependencySharedModelFlags());
		}
	}
	return out;
}

bool Drawable::applyDependencyFlags(const ModelConditionFlags &shared, const ModelConditionFlags &containerFlags)
{
	// the shared bits take the container's values, the others keep this drawable's (a word at a time, as dependencySharedModelFlags)
	ModelConditionFlags next = m_flags;
	std::uint32_t *n = next.words();
	const std::uint32_t *s = shared.words(), *c = containerFlags.words();
	for (int w = 0; w < ModelConditionFlags::NUM_WORDS; ++w)
	{
		n[w] = (n[w] & ~s[w]) | (c[w] & s[w]);
	}
	const bool changed = next != m_flags;
	if (changed)
	{
		setModelConditionFlags(next);
	}
	return changed;
}

void Drawable::scriptModuleVisible(const std::string &name, bool visible)
{
	if (m_buildingEntries)
	{
		m_pendingModuleRequests.emplace_back(name, visible);
		return;
	}
	showModule(name, visible, false);
}

bool Drawable::showModule(const std::string &name, bool visible, bool permanent)
{
	(void)permanent; // RW 0x736412 passes permanent = 1; nothing here changes a drawable's modules afterwards, so permanent and temporary hides coincide
	for (DrawEntry &e : m_entries)
	{
		if (e.tag == name)
		{
			e.moduleHidden = !visible;
			if (e.lightDraw)
			{
				e.lightDraw->setEnabled(visible);
			}
			++m_changeCount;
			return true;
		}
	}
	return false;
}

void Drawable::setUpgradeTextures(const std::vector<std::pair<std::string, std::string>> &swaps)
{
	for (DrawEntry &e : m_entries)
	{
		if (e.draw)
		{
			e.draw->setUpgradeTextureSwaps(swaps);
		}
	}
}

void Drawable::setHouseColor(std::uint32_t argb)
{
	m_houseColor = argb;
	const auto params = Render_House_Color_Params(argb, m_customKind, m_customColors);
	for (DrawEntry &e : m_entries)
	{
		if (e.draw)
		{
			e.draw->setBaseHouseColor(params);
		}
	}
}

void Drawable::recolorRenderObject(const std::string &excludeSubobject, const HouseColorParams &params)
{
	for (DrawEntry &e : m_entries)
	{
		if (e.draw)
		{
			e.draw->recolorRenderObject(excludeSubobject, params);
		}
	}
}

void Drawable::showSubObject(const std::string &name, bool visible, bool permanent)
{
	m_services.host.setChunkName(m_template->getName());
	for (DrawEntry &e : m_entries)
	{
		// a draw module whose state shows no model has no sub object to hide: the same filter as MapObjectRuntime (only modules that show a model take
		// the request; RW 0x672823 asks every module, a module without a render object does nothing). Lane RENDER-3: a PERMANENT request reaches every
		// model draw: RW 0x4C3B25 records it by name without looking at the render object, and the next model shows it
		if (!e.draw || (!permanent && e.draw->frame().model == nullptr))
		{
			continue;
		}
		if (permanent)
		{
			visible ? e.draw->showSubObjectPermanently(name) : e.draw->hideSubObjectPermanently(name);
		}
		else
		{
			visible ? e.draw->showSubObject(name) : e.draw->hideSubObject(name);
		}
	}
	++m_changeCount;
}

void Drawable::setSupplyStatus(int currentBoxes, int startingBoxes)
{
	bool changed = false;
	for (DrawEntry &e : m_entries)
	{
		if (e.supply && e.draw)
		{
			changed = e.supply->setSupplyStatus(*e.draw, m_flags, m_scale, currentBoxes, startingBoxes) || changed;
		}
	}
	if (changed)
	{
		++m_changeCount;
	}
}

void Drawable::setModelCondition(int bit, bool on)
{
	ModelConditionFlags flags = m_flags;
	if (on)
	{
		flags.set(bit);
	}
	else
	{
		flags.clearBit(bit);
	}
	setModelConditionFlags(flags);
}

void Drawable::storeModelCondition(int bit, bool on)
{
	const bool was = m_flags.test(bit);
	if (was == on)
	{
		return; // RW 0x679553: the same flags again change nothing
	}
	if (on)
	{
		m_flags.set(bit);
	}
	else
	{
		m_flags.clearBit(bit);
	}
	m_flagsDirty = true; // RW 0x67961F
}

void Drawable::storeModelConditions(const ModelConditionFlags &flags)
{
	if (flags == m_flags)
	{
		return; // RW 0x679553: the same flags again change nothing
	}
	m_flags = flags;
	m_flagsDirty = true; // RW 0x67961F
}

// Lane PORT-21 (stop S-1463), RW 0x674727 Drawable::setInaudible / RW 0x67896F Drawable::setAudible. RW 0x67472A takes the flag's address (+ 0x44A)
// and returns when it already holds the value asked for (RW 0x674733 `je` / RW 0x67897B `jne`), so the modules hear one change per transition. The
// modules of the drawable's client behaviour list (+ 0x154) are asked in list order through their + 0x30 interface table: slot 1 (offset 4) for
// inaudible (RW 0x674761 `call [edx + 4]`), slot 0 for audible (RW 0x6789AB `call [edx]`); a module without the interface is skipped (RW 0x674752 /
// RW 0x6789A5).
void Drawable::setAudible(bool audible)
{
	if (m_audible == audible)
	{
		return;
	}
	m_audible = audible;
	for (const std::unique_ptr<DrawableModule> &m : m_clientModules)
	{
		if (audible)
		{
			m->becameAudible();
		}
		else
		{
			m->becameInaudible();
		}
	}
}

void Drawable::flushModelConditions(bool hasWeaponTiming, int weaponTimingFrames, bool immediate)
{
	m_hasWeaponTiming = hasWeaponTiming;
	m_weaponTimingFrames = weaponTimingFrames;
	if (!m_flagsDirty && !immediate)
	{
		return; // RW 0x6744A3: a drawable that is not dirty is not flushed without the immediate flag
	}
	m_flagsDirty = false;
	setModelConditionFlags(m_flags);
}

bool Drawable::advanceIsLocal() const
{
	for (const DrawEntry &e : m_entries)
	{
		if (e.draw && e.animated && !e.draw->advanceIsLocal())
		{
			return false;
		}
	}
	return true;
}

// PORT-49, TARGET RW 0x4B52C8 (called by 0x4BF586 from the draw path): the camera distance fade is not an animation. It runs for
// every model draw, including a static prop whose only state is the constructor's empty idle, and writes the draw's own opacity /
// hidden state. The position is the drawable's synced drawn position (RW 0x676711); the port advances before syncTransforms, so it
// is the pose the previous render frame synced (client presentation only, no simulation state).
void Drawable::advanceCameraFade()
{
	if (!m_services.cameraPosition)
	{
		return;
	}
	Coord3D camera;
	if (!m_services.cameraPosition(camera))
	{
		return;
	}
	const int effectsLOD = m_services.effectsLOD ? m_services.effectsLOD() : -1;
	const Coord3D &pos = *getPosition();
	for (DrawEntry &e : m_entries)
	{
		if (e.draw)
		{
			e.draw->updateCameraFade(camera, pos, effectsLOD);
		}
	}
}

void Drawable::advanceAnimationLocal(double elapsedMs)
{
	advanceCameraFade(); // PORT-49: before the tracks, as RW 0x4BF586 precedes the track step
	for (DrawEntry &e : m_entries)
	{
		if (e.draw && e.animated)
		{
			e.draw->advance(elapsedMs);
		}
		if (e.isTruck || !e.truckChecked || e.truckData != e.data)
		{
			advanceTires(e, elapsedMs);
		}
		if (e.lightDraw)
		{
			e.lightDraw->stepFlicker((float)(elapsedMs * 0.001));
		}
	}
}

void Drawable::advanceAnimation(double elapsedMs)
{
	m_services.host.setChunkName(m_template->getName());
	advanceCameraFade(); // PORT-49: before the tracks, as RW 0x4BF586 precedes the track step
	for (DrawEntry &e : m_entries)
	{
		if (e.draw && e.animated)
		{
			e.draw->advance(elapsedMs);
		}
		if (e.isTruck || !e.truckChecked || e.truckData != e.data) // lane PERF-HD: the call only for a truck (or an entry not looked at yet)
		{
			advanceTires(e, elapsedMs);
		}
		if (e.lightDraw)
		{
			e.lightDraw->stepFlicker((float)(elapsedMs * 0.001));
		}
	}
}

// lane COMBAT-4 (FB-0002), RW 0x4CBFFB (W3DTruckDraw's draw update, run once per client frame: 6 per logic frame, RW 0xDE4324 + 0x38): with a front tire bone
// (+ 0x320) or a mid-left one (+ 0x328), speed = the object's locomotor speed (RW 0x68B34C, per logic frame), negated with PowerslideRotationAddition while the
// locomotor moves backwards (locomotor + 0x44 bit 7); step = TireRotationMultiplier (+ 0x1E8) / 6; front += step * speed; rear += step * speed (powerslide:
// (PowerslideRotationAddition + speed) * step); each tire bone is controlled (render object slots 0xD8 / 0xE4) with Rotate_Y of its angle. The port advances by the
// client frame's share of a logic frame (elapsed / 200 ms) instead of a fixed sixth. INFERENCE / NOT PORTED (stop S-1792): backwards is read from BACKING_UP (the
// locomotor flag is not in the snapshot); the drawable's wheel record (+ 0x13C + 0x3C: the suspension heights and the steering angle of the front tires, a powerslide)
// is not kept, so the tires only spin; the speed gate RW 0x46E918 is not read; the cab / trailer bones and the dust / dirt / powerslide effects are not run.
void Drawable::advanceTires(DrawEntry &e, double elapsedMs)
{
	if (e.truckData != e.data || !e.truckChecked)
	{
		e.truckData = e.data;
		e.truckChecked = true;
		e.isTruck = e.draw && dynamic_cast<const W3DTruckDrawModuleData *>(e.data) != nullptr;
	}
	if (!e.isTruck)
	{
		return;
	}
	const W3DTruckDrawModuleData *truck = static_cast<const W3DTruckDrawModuleData *>(e.data);
	if (e.frontTireBones.empty() && e.rearTireBones.empty())
	{
		for (const std::string *b : { &truck->m_leftFrontTireBone, &truck->m_rightFrontTireBone, &truck->m_leftFrontTireBone2, &truck->m_rightFrontTireBone2,
				 &truck->m_midLeftFrontTireBone, &truck->m_midRightFrontTireBone, &truck->m_midLeftMidTireBone, &truck->m_midRightMidTireBone,
				 &truck->m_midLeftMidTireBone2, &truck->m_midRightMidTireBone2 })
		{
			if (!b->empty())
			{
				e.frontTireBones.push_back(*b);
			}
		}
		for (const std::string *b : { &truck->m_leftRearTireBone, &truck->m_rightRearTireBone, &truck->m_leftRearTireBone2, &truck->m_rightRearTireBone2,
				 &truck->m_midLeftRearTireBone, &truck->m_midRightRearTireBone })
		{
			if (!b->empty())
			{
				e.rearTireBones.push_back(*b);
			}
		}
	}
	if (e.frontTireBones.empty() && e.rearTireBones.empty())
	{
		return;
	}
	static const int kBackingUp = ModelCondition::indexOf("BACKING_UP");
	float speed = m_moveSpeed;
	float powerslide = truck->m_powerslideRotationAddition;
	if (kBackingUp >= 0 && m_flags.test(kBackingUp))
	{
		speed = -speed;
		powerslide = -powerslide;
	}
	(void)powerslide; // the powerslide (+ 0x2EA) comes from the wheel record, which is not kept
	const float share = SimMath::fstpDword(SimMath::divD(elapsedMs, 200.0));
	const float step = SimMath::mulf32(SimMath::mulf32(truck->m_tireRotationMultiplier, speed), share);
	if (step != 0.0f)
	{
		const float twoPi = 6.28318548f;
		auto wrap = [twoPi](float a) {
			while (a >= twoPi)
			{
				a = SimMath::subf32(a, twoPi);
			}
			while (0.0f > a)
			{
				a = SimMath::addf32(a, twoPi);
			}
			return a;
		};
		e.tireFront = wrap(SimMath::addf32(e.tireFront, step));
		e.tireRear = wrap(SimMath::addf32(e.tireRear, step));
		++e.tireChanges;
	}
}

void Drawable::syncFromSnapshot(const ObjectSnapshot &rec, UnsignedInt snapshotFrame, double alpha, bool interpolate)
{
	// the sequential form (lane PERF-3 keeps it as the reference of prepareSync / commitSync, DrawableManager::setParallelSync(false)). Lane SMOOTH-1: the
	// pose of the completed frame's record (RW 0x6765B9: Catmull-Rom translation, slerped rotation, the stale snap); without interpolation the record's
	// current transform (the stepped 5 Hz look of the comparisons)
	RenderInterpolation::Pose p;
	if (interpolate)
	{
		p = RenderInterpolation::smoothPose(rec, snapshotFrame, alpha); // lane SMOOTH-2: retail's pose without its 5 Hz pulse (RenderInterpolation.h)
	}
	else
	{
		p.position = rec.position;
		p.angle = rec.angle;
		std::memcpy(p.basis, rec.basis, sizeof(p.basis));
		p.zRotation = RenderInterpolation::isZRotation(rec.basis);
	}
	applyReleaseBlend(p); // lane PORT-105: RW 0xB27C80(snapshot, current, weight), the drawn transform after a release
	if (p.zRotation)
	{
		setOrientation(p.angle);
		setPosition(&p.position);
	}
	else
	{
		setTransform(&p.position, p.basis);
	}
	m_zBasisCached = false; // the next prepareSync recomputes its trigonometry
	updateConstruction(rec, snapshotFrame, alpha); // RENDER-2 (DrawableConstruction.cpp)
	updatePhantomLook(rec);                        // lane FBSIM-1
	m_moveSpeed = rec.moveSpeed;                   // lane SMOOTH-3: the walk / run cycle's speed source (RW 0x4B67D4)
	m_scriptTarget = rec.scriptTarget;             // lane FX-3
	applyPhysicsXform(rec, p);                     // LOCAL-25: presentation physics
}

void Drawable::prepareSync(const ObjectSnapshot &rec, UnsignedInt snapshotFrame, double alpha, bool interpolate, SyncPrep &out)
{
	// lane SMOOTH-1: the pose of the completed frame's record (RW 0x6765B9: Catmull-Rom translation, slerped rotation, the stale snap); without
	// interpolation the record's current transform (the stepped 5 Hz look of the comparisons)
	out.rec = &rec;
	RenderInterpolation::Pose &p = out.pose;
	if (interpolate)
	{
		p = RenderInterpolation::smoothPose(rec, snapshotFrame, alpha); // lane SMOOTH-2: retail's pose without its 5 Hz pulse (RenderInterpolation.h)
	}
	else
	{
		p.position = rec.position;
		p.angle = rec.angle;
		std::memcpy(p.basis, rec.basis, sizeof(p.basis));
		p.zRotation = RenderInterpolation::isZRotation(rec.basis);
	}
	applyReleaseBlend(p); // lane PORT-105: RW 0xB27C80(snapshot, current, weight), the drawn transform after a release
	out.sameBasis = false;
	if (p.zRotation)
	{
		// lane PERF-3: the same angle on the basis made from it is the state setOrientation would leave (m_angle = angle, the basis of its cos / sin); the
		// transform reaction it would add is the one setPosition makes next (the footstep manager's toDirty is idempotent, RW 0x83F159)
		const float current = getOrientation();
		out.sameBasis = m_zBasisCached && std::memcmp(&p.angle, &m_zCachedAngle, sizeof(float)) == 0 && std::memcmp(&current, &m_zCachedAngle, sizeof(float)) == 0 &&
			std::memcmp(getBasis(), m_zCachedBasis, sizeof(m_zCachedBasis)) == 0;
		if (!out.sameBasis)
		{
			out.c = SimMath::cosf32(p.angle); // Thing::setOrientation's values
			out.s = SimMath::sinf32(p.angle);
		}
	}
	// the transform does not enter these (the construction look reads the record and the flags): done here, before commitSync sets it
	updateConstruction(rec, snapshotFrame, alpha); // RENDER-2 (DrawableConstruction.cpp)
	updatePhantomLook(rec);                        // lane FBSIM-1
	m_moveSpeed = rec.moveSpeed;                   // lane SMOOTH-3: the walk / run cycle's speed source (RW 0x4B67D4)
	m_scriptTarget = rec.scriptTarget;             // lane FX-3
	// LOCAL-25: no presentation physics here: this runs on the job pool, and the step draws the shared client random (the ordered pass of
	// DrawableManager makes it)
}

void Drawable::updatePhantomLook(const ObjectSnapshot &rec)
{
	// lane FBSIM-1: a dozer's phantom structure is drawn at 0.4 opacity (RW 0x88D3E5: Drawable + 0xB0 = RW 0xBE0DE4) until the placement sets 1.0 (RW 0x88D683, RW 0xBD1908)
	if (rec.phantomStructure != m_phantomLook)
	{
		m_phantomLook = rec.phantomStructure;
		setDrawableOpacity(m_phantomLook ? 0.4f : 1.0f);
	}
}

void Drawable::syncStealthLook(const ObjectSnapshot &rec)
{
	// lane STEALTHLOOK-1: a member of a HORDE imitates (RW 0x673114's parent branch), the setter never runs for it; every other object takes the
	// setter when the snapshot's look changed (RW 0x6760F9; the phase is drawn only then, retail draws it inside the setter on a pulse look)
	m_stealthParentId = rec.stealthParent;
	if (m_stealthParentId != INVALID_ID)
	{
		return;
	}
	if (rec.stealthLook == m_stealth.look)
	{
		return;
	}
	const float phaseDraw = (rec.stealthLook == StealthLook::LOOK_VISIBLE_FRIENDLY || rec.stealthLook == StealthLook::LOOK_VISIBLE_FRIENDLY_DETECTED)
		? m_services.random.real(0.0f, StealthLook::kPi)
		: 0.0f;
	if (StealthLook::setLook(m_stealth, rec.stealthLook, rec.stealthOpacityMin, rec.stealthOpacityMax, rec.stealthCycleSeconds, phaseDraw, m_cannotHideByStealth))
	{
		++m_changeCount;
	}
}

void Drawable::updateStealthLook(const Drawable *parent)
{
	// RW 0x673114, once per client frame (updateDrawable RW 0x675996 via GameClient::update RW 0x64849E)
	if (m_stealthParentId != INVALID_ID)
	{
		if (!parent)
		{
			return; // the parent object has no drawable: the call ends early
		}
		if (StealthLook::imitate(m_stealth, parent->stealth()))
		{
			++m_changeCount;
		}
		return;
	}
	StealthLook::update(m_stealth);
}

void Drawable::commitSync(const SyncPrep &prep)
{
	const RenderInterpolation::Pose &p = prep.pose;
	if (p.zRotation)
	{
		if (!prep.sameBasis)
		{
			setOrientationTrig(p.angle, prep.c, prep.s);
			m_zCachedAngle = p.angle;
			std::memcpy(m_zCachedBasis, getBasis(), sizeof(m_zCachedBasis));
			m_zBasisCached = true;
		}
		setPosition(&p.position);
	}
	else
	{
		setTransform(&p.position, p.basis);
	}
}

Drawable::PresentedLook Drawable::presentedLook(const LogicSnapshot *presented, bool shroudDrawn) const
{
	PresentedLook look;
	if (!presented)
	{
		return look;
	}
	bool synced = false;
	const ObjectSnapshot *rec = syncedRecord(*presented, synced); // lane PERF-3: the record syncTransforms found in this snapshot
	if (!synced)
	{
		rec = presented->find(m_objectId);
	}
	if (shroudDrawn)
	{
		// VIS-1: an object the local player cannot see is not drawn (RW 0xB4E890 SHROUDED); with RW 0x64849E's grace when the record is this frame's sync
		look.hidden = synced ? m_shroudLook.hidden : (rec && rec->shroudedForLocal);
		look.fogLook = synced && m_shroudLook.fullyObscured;
	}
	// lane STEALTHLOOK-1 (RW 0x81AA03 -> RW 0x6760F9, GameClient/StealthLook): the drawable's own hidden-by-stealth flag (+0x43E; a member's is the
	// imitated parent's, RW 0x673114) decides what an enemy sees - a look-5 drawable whose modules are not all affected stays drawn
	if (m_stealth.hidden || (rec && rec->drawableHidden))
	{
		look.hidden = true;
	}
	if (m_clientHidden)
	{
		look.hidden = true; // lane CHAT-1: BeaconClientUpdate::hideBeacon on an enemy's screen
	}
	look.fogLook = look.fogLook && !look.hidden;
	return look;
}

// ---- LOCAL-3: TintEnvelope & Drawable tint envelope (RW 0x671A73 .. 0x675F8A / BFME2 0x671753 .. 0x675C5F, both editions identical) ----

void TintEnvelope::setAttackFrames(UnsignedInt frames)
{
	// RW 0x671C25 / BFME2 0x671905: frames = max(frames, 1u); recip = 1.0f / (float)frames (fild, fdivr 1.0f at RW 0xBD1908);
	// m_attackRate = (m_peakColor - m_currentColor) * recip
	if (frames < 1u)
	{
		frames = 1u;
	}
	const double recip = SimMath::pc24DivW(1.0, SimMath::fildU32(frames));
	m_attackRate.red = SimMath::pc24MulD((double)SimMath::sseSub(m_peakColor.red, m_currentColor.red), recip);
	m_attackRate.green = SimMath::pc24MulD((double)SimMath::sseSub(m_peakColor.green, m_currentColor.green), recip);
	m_attackRate.blue = SimMath::pc24MulD((double)SimMath::sseSub(m_peakColor.blue, m_currentColor.blue), recip);
}

void TintEnvelope::setDecayFrames(UnsignedInt frames)
{
	// RW 0x671CBA / BFME2 0x67199A: frames = max(frames, 1u); recip = -1.0f / (float)frames (fild, fdivr -1.0f at RW 0xBD19DC);
	// m_decayRate = m_peakColor * recip
	if (frames < 1u)
	{
		frames = 1u;
	}
	const double recip = SimMath::pc24DivW(-1.0, SimMath::fildU32(frames));
	m_decayRate.red = SimMath::pc24MulD((double)m_peakColor.red, recip);
	m_decayRate.green = SimMath::pc24MulD((double)m_peakColor.green, recip);
	m_decayRate.blue = SimMath::pc24MulD((double)m_peakColor.blue, recip);
}

void TintEnvelope::play(const RGBColor *peak, UnsignedInt attackFrames, UnsignedInt decayFrames, UnsignedInt sustainAtPeak)
{
	// RW 0x674818 / BFME2 0x6744F2
	if (!peak)
	{
		return;
	}
	m_peakColor = *peak;
	setAttackFrames(attackFrames);
	setDecayFrames(decayFrames);
	m_envState = ENV_STATE_ATTACK;
	m_sustainCounter = sustainAtPeak;
	m_affect = true;
	const float dr = SimMath::sseSub(m_currentColor.red, m_peakColor.red);
	const float dg = SimMath::sseSub(m_currentColor.green, m_peakColor.green);
	const float db = SimMath::sseSub(m_currentColor.blue, m_peakColor.blue);
	const float diffLen = (float)SimMath::length3d(db, dg, dr);
	if (0.001f >= diffLen) // RW 0x6748AA: comiss xmm0 [0.001f at RW 0xC10D24], xmm1; jna
	{
		m_envState = ENV_STATE_SUSTAIN;
	}
}

void TintEnvelope::update()
{
	// RW 0x6748B7 / BFME2 0x674591
	switch (m_envState)
	{
	case ENV_STATE_REST:
		// RW 0x674AC7
		m_currentColor.red = 0.0f;
		m_currentColor.green = 0.0f;
		m_currentColor.blue = 0.0f;
		m_affect = false;
		break;
	case ENV_STATE_ATTACK:
	{
		// RW 0x6749E3
		const float dr = SimMath::sseSub(m_currentColor.red, m_peakColor.red);
		const float dg = SimMath::sseSub(m_currentColor.green, m_peakColor.green);
		const float db = SimMath::sseSub(m_currentColor.blue, m_peakColor.blue);
		const float deltaLen = (float)SimMath::length3d(db, dg, dr);
		const float attackLen = (float)SimMath::length3d(m_attackRate.blue, m_attackRate.green, m_attackRate.red);
		if (attackLen > deltaLen || 0.001f >= deltaLen)
		{
			m_envState = (m_sustainCounter != 0u) ? ENV_STATE_SUSTAIN : ENV_STATE_DECAY;
		}
		else
		{
			m_currentColor.red = SimMath::sseAdd(m_currentColor.red, m_attackRate.red);
			m_currentColor.green = SimMath::sseAdd(m_currentColor.green, m_attackRate.green);
			m_currentColor.blue = SimMath::sseAdd(m_currentColor.blue, m_attackRate.blue);
			m_affect = true;
		}
		break;
	}
	case ENV_STATE_DECAY:
	{
		// RW 0x6748EF
		const float curLen = (float)SimMath::length3d(m_currentColor.blue, m_currentColor.green, m_currentColor.red);
		const float decayLen = (float)SimMath::length3d(m_decayRate.blue, m_decayRate.green, m_decayRate.red);
		if (decayLen > curLen || 0.001f >= curLen)
		{
			m_envState = ENV_STATE_REST;
			m_affect = false;
		}
		else
		{
			m_currentColor.red = SimMath::sseAdd(m_currentColor.red, m_decayRate.red);
			m_currentColor.green = SimMath::sseAdd(m_currentColor.green, m_decayRate.green);
			m_currentColor.blue = SimMath::sseAdd(m_currentColor.blue, m_decayRate.blue);
			m_affect = true;
		}
		break;
	}
	case ENV_STATE_SUSTAIN:
		// RW 0x6748DC
		if (m_sustainCounter != 0u)
		{
			--m_sustainCounter;
		}
		else
		{
			m_envState = ENV_STATE_DECAY;
		}
		break;
	}
}

const RGBColor *TintEnvelope::getColor(UnsignedInt logicFrame) const
{
	// RW 0x671B92 / BFME2 0x671872: when m_frequency != 0 and m_amplitude != 0, scales m_currentColor by
	// sin((double)logicFrame * m_frequency) * m_amplitude * 0.5 + 0.5
	if (m_frequency != 0.0f && m_amplitude != 0.0f)
	{
		m_modulatedColor = m_currentColor;
		const double arg = SimMath::pc24MulD(SimMath::fildU32(logicFrame), (double)m_frequency);
		const double s = SimMath::sind(arg);
		const double factor = SimMath::pc24AddW(SimMath::pc24MulW(SimMath::pc24MulW(s, (double)m_amplitude), 0.5), 0.5);
		m_modulatedColor.red = SimMath::pc24MulD((double)m_modulatedColor.red, factor);
		m_modulatedColor.green = SimMath::pc24MulD((double)m_modulatedColor.green, factor);
		m_modulatedColor.blue = SimMath::pc24MulD((double)m_modulatedColor.blue, factor);
		return &m_modulatedColor;
	}
	return &m_currentColor;
}

void Drawable::setCustomTintEnvelope(const RGBColor &color, UnsignedInt attackFrames, UnsignedInt decayFrames, UnsignedInt sustainFrames, float frequency, float amplitude)
{
	// RW 0x671A73 / BFME2 0x671753: stores the raw integer frame counts into +0x78..+0x80 and the pulse parameters into +0x84..+0x88;
	// all three retail callers (TintDrawableFXNugget::doFXObj RW 0x5E0127, W3DTankDraw::doDrawModule RW 0x4D1241, Object::update RW 0x6ED185)
	// also set bit 0x20 of m_tintStatus (+0x118).
	m_customTintColor = color;
	m_customTintAttackFrames = attackFrames;
	m_customTintDecayFrames = decayFrames;
	m_customTintSustainFrames = sustainFrames;
	m_customTintAmplitude = amplitude;
	m_customTintFrequency = frequency;
	m_tintStatus |= TINT_STATUS_CUSTOM;
}

void Drawable::colorFlash(const RGBColor *color, UnsignedInt decayFrames, UnsignedInt attackFrames, UnsignedInt sustainAtPeak)
{
	// RW 0x675749 / BFME2 0x67541E: argument order in game.dat is (color, decayFrames, attackFrames, sustainAtPeak), forwarding
	// (color, attackFrames, decayFrames, sustainAtPeak) to m_colorTintEnvelope.play; null color flashes white (1, 1, 1)
	if (color)
	{
		m_colorTintEnvelope.play(color, attackFrames, decayFrames, sustainAtPeak);
	}
	else
	{
		// RW 0x675789 .. 0x6757AA: no colour ignores the frame arguments: white with attack 1, decay 4, sustain 1
		const RGBColor white{ 1.0f, 1.0f, 1.0f };
		m_colorTintEnvelope.play(&white, 1u, 4u, 1u);
	}
	m_colorTintEnvelope.setEffective(true);
}

void Drawable::colorTint(const RGBColor *color)
{
	// RW 0x6757BB / BFME2 0x675490
	if (color)
	{
		colorFlash(color, 0u, 0u, 1u);
	}
	else
	{
		m_colorTintEnvelope.setEffective(false);
	}
}

void Drawable::flashAsSelected(const RGBColor *color)
{
	// RW 0x678FA7 / BFME2 0x678C4F: plays on m_selectionFlashEnvelope (+0x64) with attack=0, decay=4, sustain=1
	if (color)
	{
		m_selectionFlashEnvelope.play(color, 0u, 4u, 1u);
	}
	else
	{
		// RW 0x67908E .. 0x67910B: with SelectionFlashHouseColor, flash uses indicator / house color; else white (0xBD1908).
		// Multiplied by SelectionFlashSaturationFactor (RW 0x6790CB, GlobalData +0xB54), shifted by -sat * 0.5f (RW 0x6790E3 / 0xBD869C).
		RGBColor baseColor{ 1.0f, 1.0f, 1.0f };
		if (m_services.selectionFlashHouseColor && m_indicatorColor != 0u)
		{
			// RW 0x67908E / 0x679097: indicator color [drawable + 0x454] / Object::getIndicatorColor (RW 0x68B6F5)
			// RW 0x4049FB (RGBColor::setFromInt on indicatorColor): unpack 0xRRGGBB / 0xAARRGGBB
			baseColor.red = (float)((m_indicatorColor >> 16) & 0xFF) / 255.0f;
			baseColor.green = (float)((m_indicatorColor >> 8) & 0xFF) / 255.0f;
			baseColor.blue = (float)(m_indicatorColor & 0xFF) / 255.0f;
		}
		const float sat = m_services.selectionFlashSaturationFactor;
		const RGBColor flashColor{
			baseColor.red * sat - sat * 0.5f,
			baseColor.green * sat - sat * 0.5f,
			baseColor.blue * sat - sat * 0.5f
		};
		m_selectionFlashEnvelope.play(&flashColor, 0u, 4u, 1u);
	}
}

const RGBColor *Drawable::getTintColor(UnsignedInt logicFrame) const
{
	// RW 0x672F9A / BFME2 0x672C74
	return m_colorTintEnvelope.isEffective() ? m_colorTintEnvelope.getColor(logicFrame) : nullptr;
}

const RGBColor *Drawable::getSelectionColor(UnsignedInt logicFrame) const
{
	// RW 0x672FAF / BFME2 0x672C89
	return m_selectionFlashEnvelope.isEffective() ? m_selectionFlashEnvelope.getColor(logicFrame) : nullptr;
}

bool Drawable::evaluateRenderTint(UnsignedInt logicFrame, RGBColor &out) const
{
	// RW 0x46F4E7 .. 0x46F5A0 / BFME2 0x46F1F1 .. 0x46F2AA (RTS3DScene::Customized_Render / renderOneObject):
	// sums getTintColor (+0x68) and getSelectionColor (+0x64) into the render tint passed to the shader
	out = RGBColor{ 0.0f, 0.0f, 0.0f };
	const RGBColor *tint = getTintColor(logicFrame);
	const RGBColor *sel = getSelectionColor(logicFrame);
	if (!tint && !sel)
	{
		return false;
	}
	if (tint)
	{
		out = *tint;
	}
	if (sel)
	{
		out.red = SimMath::sseAdd(out.red, sel->red);
		out.green = SimMath::sseAdd(out.green, sel->green);
		out.blue = SimMath::sseAdd(out.blue, sel->blue);
	}
	return true;
}

void Drawable::updateTint()
{
	// RW 0x675D8B .. 0x675F8A / BFME2 0x675A60 .. 0x675C5F (inside Drawable::updateDrawable RW 0x675996):
	// 1. Step both envelopes once per client frame.
	m_colorTintEnvelope.update();
	m_selectionFlashEnvelope.update();

	// 2. Priority chain over m_tintStatus (+0x118) vs m_prevTintStatus (+0x11C).
	static const RGBColor kDazedTint{ -0.5f, -0.5f, -0.5f };  // RW 0xDA06E8
	static const RGBColor kFrenzyTint{ 0.5f, -0.5f, -0.5f };  // RW 0xDA0700
	if ((m_tintStatus & TINT_STATUS_DISABLED) != 0u)
	{
		// RW 0x675DAD: disabled / dazed dark tint
		if ((m_prevTintStatus & TINT_STATUS_DISABLED) != 0u)
		{
			m_colorTintEnvelope.setSustainCounter(2u);
		}
		else
		{
			m_colorTintEnvelope.setPulse(0.0f, 0.0f);
			m_colorTintEnvelope.play(&kDazedTint, 30u, 30u, 2u);
		}
	}
	else if ((m_tintStatus & TINT_STATUS_POISONED) != 0u)
	{
		// RW 0x675DD0: poisoned tint (0.5, 1, 0)
		if ((m_prevTintStatus & TINT_STATUS_POISONED) != 0u)
		{
			m_colorTintEnvelope.setSustainCounter(2u);
		}
		else
		{
			const RGBColor poisonTint{ 0.5f, 1.0f, 0.0f };
			m_colorTintEnvelope.setPulse(0.0f, 0.0f);
			m_colorTintEnvelope.play(&poisonTint, 10u, 10u, 2u);
		}
	}
	else if ((m_tintStatus & TINT_STATUS_IRRADIATED) != 0u)
	{
		// RW 0x675DF3: irradiated tint (0, 1, 0)
		if ((m_prevTintStatus & TINT_STATUS_IRRADIATED) != 0u)
		{
			m_colorTintEnvelope.setSustainCounter(2u);
		}
		else
		{
			const RGBColor radTint{ 0.0f, 1.0f, 0.0f };
			m_colorTintEnvelope.setPulse(0.0f, 0.0f);
			m_colorTintEnvelope.play(&radTint, 30u, 30u, 2u);
		}
	}
	else if ((m_tintStatus & TINT_STATUS_GAINING_SUBDUAL_DAMAGE) != 0u)
	{
		// RW 0x675E1E: frenzy pulsing red tint (amp=0.5, freq=0.7, attack=3, decay=3, sustain=300)
		if ((m_prevTintStatus & TINT_STATUS_GAINING_SUBDUAL_DAMAGE) != 0u)
		{
			if (m_colorTintEnvelope.isResting())
			{
				m_tintStatus &= ~TINT_STATUS_GAINING_SUBDUAL_DAMAGE;
			}
		}
		else
		{
			m_colorTintEnvelope.setPulse(0.5f, 0.7f);
			m_colorTintEnvelope.play(&kFrenzyTint, 3u, 3u, 300u);
		}
	}
	else if ((m_tintStatus & TINT_STATUS_FRENZY) != 0u)
	{
		// RW 0x675E5E: locked red tint (attack=3, decay=3, sustain=9999)
		if ((m_prevTintStatus & TINT_STATUS_FRENZY) != 0u)
		{
			if (m_colorTintEnvelope.isResting())
			{
				m_tintStatus &= ~TINT_STATUS_FRENZY;
			}
		}
		else
		{
			m_colorTintEnvelope.setPulse(0.0f, 0.0f);
			m_colorTintEnvelope.play(&kFrenzyTint, 3u, 3u, 9999u);
		}
	}
	else if ((m_tintStatus & TINT_STATUS_CUSTOM) != 0u)
	{
		// RW 0x675E9A: custom tint envelope (+0x6C..+0x88)
		if ((m_prevTintStatus & TINT_STATUS_CUSTOM) != 0u)
		{
			if (m_colorTintEnvelope.isResting())
			{
				m_tintStatus &= ~TINT_STATUS_CUSTOM;
			}
		}
		else
		{
			m_colorTintEnvelope.setPulse(m_customTintAmplitude, m_customTintFrequency);
			m_colorTintEnvelope.play(&m_customTintColor, m_customTintAttackFrames, m_customTintDecayFrames, m_customTintSustainFrames);
		}
	}
	else
	{
		// RW 0x675EDD..0x675F84: clear sustain on transition out of a sustained status bit
		if ((m_prevTintStatus & TINT_STATUS_DISABLED) != 0u && (m_tintStatus & TINT_STATUS_DISABLED) == 0u)
		{
			m_colorTintEnvelope.setSustainCounter(0u);
		}
		if ((m_prevTintStatus & TINT_STATUS_POISONED) != 0u && (m_tintStatus & TINT_STATUS_POISONED) == 0u)
		{
			m_colorTintEnvelope.setSustainCounter(0u);
		}
		if ((m_prevTintStatus & TINT_STATUS_IRRADIATED) != 0u && (m_tintStatus & TINT_STATUS_IRRADIATED) == 0u)
		{
			m_colorTintEnvelope.setSustainCounter(0u);
		}
		if ((m_prevTintStatus & TINT_STATUS_GAINING_SUBDUAL_DAMAGE) != 0u && (m_tintStatus & TINT_STATUS_GAINING_SUBDUAL_DAMAGE) == 0u)
		{
			m_colorTintEnvelope.setSustainCounter(0u);
		}
		if ((m_prevTintStatus & TINT_STATUS_FRENZY) != 0u && (m_tintStatus & TINT_STATUS_FRENZY) == 0u)
		{
			m_colorTintEnvelope.setSustainCounter(0u);
		}
		if ((m_prevTintStatus & TINT_STATUS_CUSTOM) != 0u && (m_tintStatus & TINT_STATUS_CUSTOM) == 0u)
		{
			m_colorTintEnvelope.setSustainCounter(0u);
		}
	}
	m_prevTintStatus = m_tintStatus;
}

// RW 0x672B5B: bone world transform query across draw module models (DONOR BFME2 0x00272835)
// Retail iterates m_modules, calls slotA4() on each module, and if non-null calls slot14(boneName, &matrix)
// (RW 0x004B2F19), which queries the current animated pose of m_renderObj via Get_Bone_Index and Get_Bone_Transform.
bool Drawable::boneTransform(const std::string &boneName, Matrix3D &outTransform) const
{
	if (boneName.empty() || AsciiStringUtil::compareNoCase(boneName, "None") == 0)
	{
		return false;
	}
	std::string lower = boneName;
	AsciiStringUtil::toLower(lower);

	for (const DrawEntry &e : m_entries)
	{
		if (!e.draw)
		{
			continue;
		}
		const W3DDrawFrame f = e.draw->frame();
		if (!f.model || !f.model->Tree)
		{
			continue;
		}
		int boneIdx = f.model->Tree->Get_Bone_Index(lower);
		if (boneIdx <= 0)
		{
			const int sub = W3DFindSubObjectByName(*f.model, lower);
			boneIdx = sub >= 0 ? f.model->SubObjects[(size_t)sub].BoneIndex : -1;
		}
		if (boneIdx > 0)
		{
			HTreePose pose;
			if (f.blending && f.motion1 != nullptr)
			{
				f.model->Tree->Blend_Pose(Matrix3D(), f.motion0, f.frame0, f.motion1, f.frame1, f.blendPercentage, pose);
			}
			else if (f.motion0 != nullptr)
			{
				f.model->Tree->Anim_Pose(Matrix3D(), f.motion0, f.frame0, pose);
			}
			else
			{
				f.model->Tree->Base_Pose(Matrix3D(), pose);
			}
			if ((size_t)boneIdx < pose.Transform.size())
			{
				const Matrix3D &m = pose.Transform[(size_t)boneIdx];

				// Build object world matrix: basis, scale, position (including presentation physics and construction offset)
				float basis[9];
				Coord3D pos;
				getPresentationPose(basis, pos);
				if (e.constructionOffsetZ != 0.0f)
				{
					const float k = e.constructionOffsetZ * m_scale;
					pos.x += basis[2] * k;
					pos.y += basis[5] * k;
					pos.z += basis[8] * k;
				}

				Matrix3D objMtx;
				for (int r = 0; r < 3; ++r)
				{
					for (int c = 0; c < 3; ++c)
					{
						objMtx.Row[r][c] = basis[r * 3 + c] * m_scale;
					}
				}
				objMtx.Row[0][3] = pos.x;
				objMtx.Row[1][3] = pos.y;
				objMtx.Row[2][3] = pos.z;

				Matrix3D::Multiply(objMtx, m, &outTransform);
				return true;
			}
		}
	}
	return false;
}

bool Drawable::boneWorldPosition(const std::string &boneName, Coord3D &outPos) const
{
	Matrix3D mtx;
	if (boneTransform(boneName, mtx))
	{
		outPos = Coord3D{ mtx.Row[0][3], mtx.Row[1][3], mtx.Row[2][3] };
		return true;
	}
	return false;
}

// RW 0xAD1920: maximum geometry height above position for the drawable's template
float Drawable::geometryHeight() const
{
	if (m_geometryHeight < 0.0f)
	{
		const ThingTemplate *tt = getTemplate();
		m_geometryHeight = tt ? ObjectGeometry::maxHeightAbovePosition(ObjectGeometry::shapesOf(*tt)) : 0.0f;
	}
	return m_geometryHeight;
}

// lane LOCAL-29 (issue #218): computes world transform of entry e (RW 0x004B6A6F..0x004B6B26 / slot +0x104); returns true if attached
bool Drawable::entryTransform(const DrawEntry &e, Matrix3D &outMatrix) const
{
	if (!e.attachToBone.empty() && boneTransform(e.attachToBone, outMatrix))
	{
		// RW 0x004B6B26: slot +0x104 virtual callback to adjust attached transform
		if (e.draw)
		{
			e.draw->adjustAttachedTransform(outMatrix);
		}
		return true;
	}

	// Default unattached transform: presentation pose (or basis), scale, construction offset Z, noRotate
	float basis[9];
	Coord3D pos;
	getPresentationPose(basis, pos);
	if (e.noRotate)
	{
		static const float kIdentityBasis[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		std::memcpy(basis, kIdentityBasis, sizeof(basis));
	}
	if (e.constructionOffsetZ != 0.0f)
	{
		const float k = e.constructionOffsetZ * getInstanceScale();
		pos.x += basis[2] * k;
		pos.y += basis[5] * k;
		pos.z += basis[8] * k;
	}
	const float scale = getInstanceScale();
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			outMatrix.Row[r][c] = basis[r * 3 + c] * scale;
		}
	}
	outMatrix.Row[0][3] = pos.x;
	outMatrix.Row[1][3] = pos.y;
	outMatrix.Row[2][3] = pos.z;
	return false;
}

// ------------------------------------------------------------------------------------------------
// LOCAL-25 (issue #187): Client presentation physics (pitch, roll, suspension, bobbing)
// Retail RotWK 2.01:
//   - calcPitchRoll: RW 0x670570 (DONOR BFME2 0x00270276, Drawable_calcPitchRoll.cpp)
//   - calcPhysicsXform: RW 0x67BDC0 (DONOR BFME2 0x0027BA95, DrawableCalcPhysicsXform.cpp)
//   - applyPhysicsXform: RW 0x67BE90 (DONOR BFME2 0x0027BB65, DrawableApplyPhysicsXform.cpp)
//   - calcPhysicsXformWheels: RW 0x677026 (DONOR BFME2 0x00276CFB, DrawablePhysicsXformWheels.cpp)
//   - calcPhysicsXformTreads: RW 0x670B11 (DONOR BFME2 0x00270817, DrawablePhysicsXformTreads.cpp)
//   - calcPhysicsXformHugeFourLegs: RW 0x67B7AA (DONOR BFME2 0x0027B47F, DrawablePhysicsXformHugeFourLegs.cpp)
//   - calcPhysicsXformShip: RW 0x6732DC (DONOR BFME2 0x00272FB6, DrawablePhysicsXformShip.cpp)
//   - calcPhysicsXformGiantBird: RW 0x670E09 (DONOR BFME2 0x00270B0F)
// Stop S-7222: the entry points, the dispatcher, the step gate and the wheels field loads are checked in RotWK game.dat; the arithmetic inside
// the five bodies follows the BFME2 donor (INFERENCE that RotWK keeps it)
// ------------------------------------------------------------------------------------------------

namespace
{
// RenderInterpolation::Pose leaves pose.basis as identity when pose.zRotation is true (only pose.angle is set);
// mirror commitSync / syncFromSnapshot so physics and presentation poses keep the actual yaw.
void resolvePoseBasis(const RenderInterpolation::Pose &pose, float outBasis[9])
{
	if (pose.zRotation)
	{
		const float c = SimMath::cosf32(pose.angle);
		const float s = SimMath::sinf32(pose.angle);
		outBasis[0] = c;    outBasis[1] = -s;   outBasis[2] = 0.0f;
		outBasis[3] = s;    outBasis[4] = c;    outBasis[5] = 0.0f;
		outBasis[6] = 0.0f; outBasis[7] = 0.0f; outBasis[8] = 1.0f;
	}
	else
	{
		std::memcpy(outBasis, pose.basis, sizeof(float) * 9);
	}
}
} // namespace

bool Drawable::calcPitchRoll(const Coord3D &pos, const Coord3D &dir, const LocomotorTemplate *loco, float &pitch, float &roll)
{
	Coord3D perp;
	perp.x = -dir.y;
	perp.y = dir.x;
	perp.z = 0.0f;

	// RW 0x670570: if LocomotorTemplate flag +0xED (m_useTerrainSmoothing) is set, sample terrain under 3 corners
	if (loco && loco->m_useTerrainSmoothing && m_services.groundHeight)
	{
		float length = 15.0f;
		float width = 15.0f;
		if (m_template)
		{
			const std::vector<ObjectGeometry::Shape> &shapes = ObjectGeometry::cachedShapesOf(*m_template);
			if (!shapes.empty())
			{
				length = shapes[0].majorRadius;
				width = shapes[0].minorRadius;
			}
		}

		Coord3D halfLength;
		halfLength.x = dir.x * 0.5f * length;
		halfLength.y = (length * dir.y) * 0.5f;
		halfLength.z = 0.0f;

		Coord3D vec;
		vec.x = perp.x * 0.5f * width;
		vec.y = perp.y * 0.5f * width;
		vec.z = 0.0f;

		Coord3D corners[3];
		corners[0].x = pos.x - halfLength.x + vec.x;
		corners[0].y = pos.y - halfLength.y + vec.y;
		corners[0].z = pos.z;

		corners[1].x = pos.x - halfLength.x - vec.x;
		corners[1].y = pos.y - halfLength.y - vec.y;
		corners[1].z = pos.z;

		corners[2].x = halfLength.x + pos.x + vec.x;
		corners[2].y = halfLength.y + pos.y + vec.y;
		corners[2].z = pos.z;

		bool tooSteep = false;
		float heights[3];
		for (int i = 0; i < 3; ++i)
		{
			heights[i] = m_services.groundHeight(corners[i].x, corners[i].y, nullptr);
			if (std::abs(heights[i] - pos.z) > (length + width) * 0.5f)
			{
				tooSteep = true;
			}
		}

		if (!tooSteep)
		{
			Coord3D widthEdge;
			widthEdge.x = corners[1].x - corners[0].x;
			widthEdge.y = corners[1].y - corners[0].y;
			widthEdge.z = heights[1] - heights[0];

			Coord3D lengthEdge;
			lengthEdge.x = corners[2].x - corners[0].x;
			lengthEdge.y = corners[2].y - corners[0].y;
			lengthEdge.z = heights[2] - heights[0];

			vec.x = widthEdge.y * lengthEdge.z - widthEdge.z * lengthEdge.y;
			vec.y = widthEdge.z * lengthEdge.x - widthEdge.x * lengthEdge.z;
			vec.z = widthEdge.x * lengthEdge.y - widthEdge.y * lengthEdge.x;

			float len = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
			if (len > 0.0f)
			{
				vec.x /= len;
				vec.y /= len;
				vec.z /= len;
			}

			float s = vec.x * dir.x + vec.y * dir.y + vec.z * dir.z;
			s = std::max(-1.0f, std::min(1.0f, s));
			pitch = std::asin(s);

			s = vec.x * perp.x + vec.y * perp.y + vec.z * perp.z;
			s = std::max(-1.0f, std::min(1.0f, s));
			roll = std::asin(s);
			return true;
		}
	}

	// Fallback (or when !m_useTerrainSmoothing): terrain normal at position
	Coord3D normal{ 0.0f, 0.0f, 1.0f };
	if (m_services.groundHeight)
	{
		m_services.groundHeight(pos.x, pos.y, &normal);
	}
	float s = normal.x * dir.x + normal.y * dir.y;
	s = std::max(-1.0f, std::min(1.0f, s));
	pitch = std::asin(s);

	s = normal.x * perp.x + normal.y * perp.y;
	s = std::max(-1.0f, std::min(1.0f, s));
	roll = std::asin(s);
	return true;
}

bool Drawable::calcPhysicsXform(const ObjectSnapshot &rec, const RenderInterpolation::Pose &pose, PhysicsXformInfo &info)
{
	const LocomotorTemplate *loco = rec.locomotorTemplate;
	if (!loco)
	{
		return false;
	}

	if (!m_locoInfo)
	{
		m_locoInfo = std::make_unique<DrawableLocoInfo>();
	}

	// Dispatcher by LocomotorAppearance (RW 0x67BDC0 / donor 0x0027BA95)
	switch (loco->m_appearance)
	{
	case LOCO_WHEELS_FOUR: // 1
	case LOCO_TREADS:      // 8
		calcPhysicsXformWheels(loco, rec, pose, info);
		return true;

	case LOCO_HOVER: // 2
	case LOCO_WINGS: // 3
		calcPhysicsXformTreads(loco, rec, pose, info);
		return true;

	case LOCO_LEGS_FOUR_HUGE: // 4
		calcPhysicsXformHugeFourLegs(loco, rec, pose, info);
		return true;

	case LOCO_GIANT_BIRD: // 5
		calcPhysicsXformGiantBird(loco, rec, pose, info);
		return true;

	case LOCO_SHIP: // 9
		calcPhysicsXformShip(loco, rec, pose, info);
		return true;

	default:
		return false;
	}
}

void Drawable::calcPhysicsXformWheels(const LocomotorTemplate *loco, const ObjectSnapshot &rec, const RenderInterpolation::Pose &pose, PhysicsXformInfo &info)
{
	const float ACCEL_PITCH_LIMIT = loco->m_accelerationPitchLimit;
	const float BOUNCE_ANGLE_KICK = loco->m_bounceAmount;
	const float PITCH_STIFFNESS = loco->m_pitchStiffness;
	const float ROLL_STIFFNESS = loco->m_rollStiffness;
	const float PITCH_DAMPING = loco->m_pitchDamping;
	const float ROLL_DAMPING = loco->m_rollDamping;
	const float UNIFORM_AXIAL_DAMPING = loco->m_uniformAxialDamping;
	const float MAX_SUSPENSION_EXTENSION = loco->m_maximumWheelExtension;
	const bool DO_WHEELS = loco->m_hasSuspension;

	float length = 15.0f;
	float width = 15.0f;
	if (m_template)
	{
		const std::vector<ObjectGeometry::Shape> &shapes = ObjectGeometry::cachedShapesOf(*m_template);
		if (!shapes.empty())
		{
			length = shapes[0].majorRadius;
			width = shapes[0].minorRadius;
		}
	}

	const Coord3D &pos = pose.position;
	float basis[9];
	resolvePoseBasis(pose, basis);
	Coord3D dir{ basis[0], basis[3], 0.0f };
	float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
	if (dirLen > 0.0f)
	{
		dir.x /= dirLen;
		dir.y /= dirLen;
	}
	else
	{
		dir = Coord3D{ 1.0f, 0.0f, 0.0f };
	}

	float groundPitch = 0.0f;
	float groundRoll = 0.0f;
	calcPitchRoll(pos, dir, loco, groundPitch, groundRoll);

	float hheight = m_services.groundHeight ? m_services.groundHeight(pos.x, pos.y, nullptr) : 0.0f;
	bool airborne = rec.aboveTerrain;

	if (airborne)
	{
		if (DO_WHEELS)
		{
			m_locoInfo->m_framesAirborne = 0;
			m_locoInfo->m_framesAirborneCounter++;
			if (pos.z - hheight > -MAX_SUSPENSION_EXTENSION)
			{
				m_locoInfo->m_rearLeftHeightOffset += (MAX_SUSPENSION_EXTENSION - m_locoInfo->m_rearLeftHeightOffset) * 0.5f;
				m_locoInfo->m_rearRightHeightOffset += (MAX_SUSPENSION_EXTENSION - m_locoInfo->m_rearRightHeightOffset) * 0.5f;
			}
			else
			{
				m_locoInfo->m_rearLeftHeightOffset += (0.0f - m_locoInfo->m_rearLeftHeightOffset) * 0.5f;
				m_locoInfo->m_rearRightHeightOffset += (0.0f - m_locoInfo->m_rearRightHeightOffset) * 0.5f;
			}
		}

		float pitchHeight = length * std::sin(m_locoInfo->m_accelerationPitch + m_locoInfo->m_pitch - groundPitch);
		float rollHeight = width * std::sin(m_locoInfo->m_accelerationRoll + m_locoInfo->m_roll - groundRoll);
		info.m_totalZ = (std::abs(pitchHeight) + std::abs(rollHeight)) * 0.25f;
		return;
	}

	float curSpeed = rec.moveSpeed;
	float maxSpeed = loco->m_maxSpeed > 0.0f ? loco->m_maxSpeed : 1.0f;
	if (curSpeed > maxSpeed * 0.1f && BOUNCE_ANGLE_KICK > 0.0f)
	{
		float factor = curSpeed / maxSpeed;
		if (std::abs(m_locoInfo->m_pitchRate) < factor * BOUNCE_ANGLE_KICK * 0.25f && std::abs(m_locoInfo->m_rollRate) < factor * BOUNCE_ANGLE_KICK * 0.125f)
		{
			int r = m_services.random.value(0, 3);
			if (r == 0)
			{
				m_locoInfo->m_pitchRate -= BOUNCE_ANGLE_KICK * factor;
				m_locoInfo->m_rollRate -= BOUNCE_ANGLE_KICK * factor * 0.5f;
			}
			else if (r == 1)
			{
				m_locoInfo->m_pitchRate += BOUNCE_ANGLE_KICK * factor;
				m_locoInfo->m_rollRate -= BOUNCE_ANGLE_KICK * factor * 0.5f;
			}
			else if (r == 2)
			{
				m_locoInfo->m_pitchRate -= BOUNCE_ANGLE_KICK * factor;
				m_locoInfo->m_rollRate += BOUNCE_ANGLE_KICK * factor * 0.5f;
			}
			else
			{
				m_locoInfo->m_pitchRate += BOUNCE_ANGLE_KICK * factor;
				m_locoInfo->m_rollRate += BOUNCE_ANGLE_KICK * factor * 0.5f;
			}
		}
	}

	m_locoInfo->m_pitchRate += (-PITCH_STIFFNESS * (m_locoInfo->m_pitch - groundPitch)) + (-PITCH_DAMPING * m_locoInfo->m_pitchRate);
	if (m_locoInfo->m_pitchRate > 0.0f)
	{
		m_locoInfo->m_pitchRate *= 0.5f;
	}
	m_locoInfo->m_rollRate += (-ROLL_STIFFNESS * (m_locoInfo->m_roll - groundRoll)) + (-ROLL_DAMPING * m_locoInfo->m_rollRate);

	m_locoInfo->m_pitch += m_locoInfo->m_pitchRate * UNIFORM_AXIAL_DAMPING;
	m_locoInfo->m_roll += m_locoInfo->m_rollRate * UNIFORM_AXIAL_DAMPING;

	m_locoInfo->m_accelerationPitchRate += (-PITCH_STIFFNESS * m_locoInfo->m_accelerationPitch) + (-PITCH_DAMPING * m_locoInfo->m_accelerationPitchRate);
	m_locoInfo->m_accelerationPitch += m_locoInfo->m_accelerationPitchRate;

	m_locoInfo->m_accelerationRollRate += (-ROLL_STIFFNESS * m_locoInfo->m_accelerationRoll) + (-ROLL_DAMPING * m_locoInfo->m_accelerationRollRate);
	m_locoInfo->m_accelerationRoll += m_locoInfo->m_accelerationRollRate;

	info.m_totalPitch = m_locoInfo->m_accelerationPitch + m_locoInfo->m_pitch;
	info.m_totalRoll = m_locoInfo->m_accelerationRoll + m_locoInfo->m_roll;

	if (m_locoInfo->m_accelerationPitch > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationPitch < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = -ACCEL_PITCH_LIMIT;

	if (m_locoInfo->m_accelerationRoll > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationRoll < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = -ACCEL_PITCH_LIMIT;

	info.m_totalYaw = 0.0f;
	info.m_totalZ = 0.0f;

	float pitchHeight = length * std::sin(info.m_totalPitch - groundPitch);
	float rollHeight = width * std::sin(info.m_totalRoll - groundRoll);

	if (DO_WHEELS)
	{
		m_locoInfo->m_framesAirborne = m_locoInfo->m_framesAirborneCounter;
		m_locoInfo->m_framesAirborneCounter = 0;
		float newFrontLeft = 0.0f, newFrontRight = 0.0f, newRearLeft = 0.0f, newRearRight = 0.0f;

		const float SPRING_FACTOR = 0.9f;
		if (pitchHeight < 0.0f)
		{
			newFrontLeft = SPRING_FACTOR * (pitchHeight * (1.0f / 3.0f + 1.0f / 2.0f));
			newFrontRight = SPRING_FACTOR * (pitchHeight * (1.0f / 3.0f + 1.0f / 2.0f));
			newRearLeft = pitchHeight * (-1.0f / 2.0f + 1.0f / 4.0f);
			newRearRight = pitchHeight * (-1.0f / 2.0f + 1.0f / 4.0f);
		}
		else
		{
			newFrontLeft = (pitchHeight * (-1.0f / 4.0f + 1.0f / 2.0f));
			newFrontRight = (pitchHeight * (-1.0f / 4.0f + 1.0f / 2.0f));
			newRearLeft = SPRING_FACTOR * (pitchHeight * (-1.0f / 2.0f - 1.0f / 3.0f));
			newRearRight = SPRING_FACTOR * (pitchHeight * (-1.0f / 2.0f - 1.0f / 3.0f));
		}
		if (rollHeight > 0.0f)
		{
			newFrontRight += -SPRING_FACTOR * (rollHeight * (1.0f / 3.0f + 1.0f / 2.0f));
			newRearRight += -SPRING_FACTOR * (rollHeight * (1.0f / 3.0f + 1.0f / 2.0f));
			newRearLeft += rollHeight * 0.5f - rollHeight * 0.25f;
			newFrontLeft += rollHeight * 0.5f - rollHeight * 0.25f;
		}
		else
		{
			newFrontRight += -rollHeight * (1.0f / 2.0f - 1.0f / 4.0f);
			newRearRight += -rollHeight * (1.0f / 2.0f - 1.0f / 4.0f);
			newRearLeft += SPRING_FACTOR * (rollHeight * (1.0f / 3.0f + 1.0f / 2.0f));
			newFrontLeft += SPRING_FACTOR * (rollHeight * (1.0f / 3.0f + 1.0f / 2.0f));
		}

		if (newFrontLeft < m_locoInfo->m_frontLeftHeightOffset)
			m_locoInfo->m_frontLeftHeightOffset += (newFrontLeft - m_locoInfo->m_frontLeftHeightOffset) * 0.5f;
		else
			m_locoInfo->m_frontLeftHeightOffset = newFrontLeft;

		if (newFrontRight < m_locoInfo->m_frontRightHeightOffset)
			m_locoInfo->m_frontRightHeightOffset += (newFrontRight - m_locoInfo->m_frontRightHeightOffset) * 0.5f;
		else
			m_locoInfo->m_frontRightHeightOffset = newFrontRight;

		if (newRearLeft < m_locoInfo->m_rearLeftHeightOffset)
			m_locoInfo->m_rearLeftHeightOffset += (newRearLeft - m_locoInfo->m_rearLeftHeightOffset) * 0.5f;
		else
			m_locoInfo->m_rearLeftHeightOffset = newRearLeft;

		if (newRearRight < m_locoInfo->m_rearRightHeightOffset)
			m_locoInfo->m_rearRightHeightOffset += (newRearRight - m_locoInfo->m_rearRightHeightOffset) * 0.5f;
		else
			m_locoInfo->m_rearRightHeightOffset = newRearRight;

		if (m_locoInfo->m_frontLeftHeightOffset < MAX_SUSPENSION_EXTENSION)
			m_locoInfo->m_frontLeftHeightOffset = MAX_SUSPENSION_EXTENSION;
		if (m_locoInfo->m_frontRightHeightOffset < MAX_SUSPENSION_EXTENSION)
			m_locoInfo->m_frontRightHeightOffset = MAX_SUSPENSION_EXTENSION;
		if (m_locoInfo->m_rearLeftHeightOffset < MAX_SUSPENSION_EXTENSION)
			m_locoInfo->m_rearLeftHeightOffset = MAX_SUSPENSION_EXTENSION;
		if (m_locoInfo->m_rearRightHeightOffset < MAX_SUSPENSION_EXTENSION)
			m_locoInfo->m_rearRightHeightOffset = MAX_SUSPENSION_EXTENSION;
	}

	float divisor = 4.0f;
	float pitchDiff = std::abs(info.m_totalPitch - groundPitch);
	constexpr float kPi = 3.14159265358979323846f;
	if (pitchDiff > kPi / 8.0f)
	{
		divisor = ((4.0f * kPi / 8.0f) + (1.0f * (pitchDiff - kPi / 8.0f))) / pitchDiff;
	}
	info.m_totalZ += std::abs(pitchHeight) / divisor;
	info.m_totalZ += std::abs(rollHeight) / divisor;
}

void Drawable::calcPhysicsXformTreads(const LocomotorTemplate *loco, const ObjectSnapshot &rec, const RenderInterpolation::Pose &pose, PhysicsXformInfo &info)
{
	const float ACCEL_PITCH_LIMIT = loco->m_accelerationPitchLimit;
	const float PITCH_STIFFNESS = loco->m_pitchStiffness;
	const float ROLL_STIFFNESS = loco->m_rollStiffness;
	const float PITCH_DAMPING = loco->m_pitchDamping;
	const float ROLL_DAMPING = loco->m_rollDamping;
	const float UNIFORM_AXIAL_DAMPING = loco->m_uniformAxialDamping;

	m_locoInfo->m_pitchRate += ((-PITCH_STIFFNESS * m_locoInfo->m_pitch) + (-PITCH_DAMPING * m_locoInfo->m_pitchRate));
	m_locoInfo->m_rollRate += ((-ROLL_STIFFNESS * m_locoInfo->m_roll) + (-ROLL_DAMPING * m_locoInfo->m_rollRate));

	if (!rec.aboveTerrain)
	{
		m_locoInfo->m_pitch += m_locoInfo->m_pitchRate * UNIFORM_AXIAL_DAMPING;
		m_locoInfo->m_roll += m_locoInfo->m_rollRate * UNIFORM_AXIAL_DAMPING;

		m_locoInfo->m_accelerationPitchRate += ((-PITCH_STIFFNESS * m_locoInfo->m_accelerationPitch) + (-PITCH_DAMPING * m_locoInfo->m_accelerationPitchRate));
		m_locoInfo->m_accelerationPitch += m_locoInfo->m_accelerationPitchRate;

		m_locoInfo->m_accelerationRollRate += ((-ROLL_STIFFNESS * m_locoInfo->m_accelerationRoll) + (-ROLL_DAMPING * m_locoInfo->m_accelerationRollRate));
		m_locoInfo->m_accelerationRoll += m_locoInfo->m_accelerationRollRate;
	}
	else
	{
		m_locoInfo->m_accelerationPitch *= 0.5f;
		if (std::abs(m_locoInfo->m_accelerationPitch) < 0.0001f)
			m_locoInfo->m_accelerationPitch = 0.0f;
		m_locoInfo->m_accelerationRoll *= 0.5f;
		if (std::abs(m_locoInfo->m_accelerationRoll) < 0.0001f)
			m_locoInfo->m_accelerationRoll = 0.0f;
	}

	info.m_totalPitch = m_locoInfo->m_accelerationPitch + m_locoInfo->m_pitch;
	info.m_totalRoll = m_locoInfo->m_accelerationRoll + m_locoInfo->m_roll;

	if (m_locoInfo->m_accelerationPitch > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationPitch < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = -ACCEL_PITCH_LIMIT;

	if (m_locoInfo->m_accelerationRoll > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationRoll < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = -ACCEL_PITCH_LIMIT;

	info.m_totalYaw = 0.0f;
	info.m_totalZ = 0.0f;
}

void Drawable::calcPhysicsXformHugeFourLegs(const LocomotorTemplate *loco, const ObjectSnapshot &rec, const RenderInterpolation::Pose &pose, PhysicsXformInfo &info)
{
	(void)rec;
	const float ACCEL_PITCH_LIMIT = loco->m_accelerationPitchLimit;
	const float PITCH_STIFFNESS = loco->m_pitchStiffness;
	const float ROLL_STIFFNESS = loco->m_rollStiffness;
	const float PITCH_DAMPING = loco->m_pitchDamping;
	const float ROLL_DAMPING = loco->m_rollDamping;
	const float UNIFORM_AXIAL_DAMPING = loco->m_uniformAxialDamping;

	const Coord3D &pos = pose.position;

	// PORT-126 (issue #310): RW 0x67B894..0x67B8E4 walks draw modules in template order, calls
	// ObjectDrawInterface::getFootLocations(points, false) (slot 47 / +0xBC; RW 0x4C9A39 on W3DQuadrupedDraw),
	// and falls back to object geometry corners only when none supplies >= 4 feet.
	std::vector<Coord3D> points;
	bool gotFootLocations = false;
	for (const DrawEntry &e : m_entries)
	{
		if (!e.draw)
		{
			continue;
		}
		Matrix3D renderMtx;
		if (!e.attachToBone.empty() && boneTransform(e.attachToBone, renderMtx))
		{
			e.draw->adjustAttachedTransform(renderMtx);
		}
		else
		{
			float basis[9];
			resolvePoseBasis(pose, basis);
			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 3; ++c)
				{
					renderMtx.Row[r][c] = basis[r * 3 + c];
				}
			}
			renderMtx.Row[0][3] = pos.x;
			renderMtx.Row[1][3] = pos.y;
			renderMtx.Row[2][3] = pos.z;
			if (m_hasPhysicsXform)
			{
				renderMtx.Translate(Vector3(0.0f, 0.0f, m_lastPhysicsInfo.m_totalZ));
				renderMtx.Rotate_Y(m_lastPhysicsInfo.m_totalPitch);
				renderMtx.Rotate_X(-m_lastPhysicsInfo.m_totalRoll);
				renderMtx.Rotate_Z(m_lastPhysicsInfo.m_totalYaw);
			}
			if (e.noRotate)
			{
				renderMtx.Row[0][0] = 1.0f; renderMtx.Row[0][1] = 0.0f; renderMtx.Row[0][2] = 0.0f;
				renderMtx.Row[1][0] = 0.0f; renderMtx.Row[1][1] = 1.0f; renderMtx.Row[1][2] = 0.0f;
				renderMtx.Row[2][0] = 0.0f; renderMtx.Row[2][1] = 0.0f; renderMtx.Row[2][2] = 1.0f;
			}
			if (e.constructionOffsetZ != 0.0f)
			{
				const float k = e.constructionOffsetZ * m_scale;
				renderMtx.Row[0][3] += renderMtx.Row[0][2] * k;
				renderMtx.Row[1][3] += renderMtx.Row[1][2] * k;
				renderMtx.Row[2][3] += renderMtx.Row[2][2] * k;
			}
			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 3; ++c)
				{
					renderMtx.Row[r][c] *= m_scale;
				}
			}
		}

		std::vector<Coord3D> candidate;
		if (e.draw->getFootLocations(candidate, false, renderMtx) && candidate.size() >= 4)
		{
			points = std::move(candidate);
			gotFootLocations = true;
			break;
		}
	}

	if (!gotFootLocations)
	{
		float length = 15.0f;
		float width = 15.0f;
		if (m_template)
		{
			const std::vector<ObjectGeometry::Shape> &shapes = ObjectGeometry::cachedShapesOf(*m_template);
			if (!shapes.empty())
			{
				length = shapes[0].majorRadius;
				width = shapes[0].minorRadius;
			}
		}

		float direction = pose.angle;
		float s = std::sin(direction);
		float c = std::cos(direction);

		points.resize(4);
		points[0] = Coord3D{ -length * c - width * s + pos.x, -width * c + -length * s + pos.y, pos.z };
		points[1] = Coord3D{  length * c - width * s + pos.x, -width * c +  length * s + pos.y, pos.z };
		points[2] = Coord3D{ -length * c + width * s + pos.x,  width * c + -length * s + pos.y, pos.z };
		points[3] = Coord3D{  length * c + width * s + pos.x,  width * c +  length * s + pos.y, pos.z };
	}

	float heights[4];
	for (int i = 0; i < 4; ++i)
	{
		if (i == 0 || points[i].x != points[i - 1].x || points[i].y != points[i - 1].y)
		{
			heights[i] = m_services.groundHeight ? m_services.groundHeight(points[i].x, points[i].y, nullptr) : pos.z;
		}
		else
		{
			heights[i] = heights[i - 1];
		}
	}

	float front = (heights[0] + heights[1]) * 0.5f;
	float back = (heights[2] + heights[3]) * 0.5f;
	float frontX = (points[0].x + points[1].x) * 0.5f;
	float frontY = (points[0].y + points[1].y) * 0.5f;
	float backX = (points[2].x + points[3].x) * 0.5f;
	float backY = (points[2].y + points[3].y) * 0.5f;
	float dx = frontX - backX;
	float dy = frontY - backY;
	float dist = std::sqrt(dx * dx + dy * dy);
	if (!gotFootLocations && dist < 1.0f) dist = 1.0f;
	float pitch = std::atan2(back - front, dist);

	m_locoInfo->m_pitchRate += (-PITCH_STIFFNESS * (m_locoInfo->m_pitch - pitch)) + (-PITCH_DAMPING * m_locoInfo->m_pitchRate);
	if (m_locoInfo->m_pitchRate > 0.0f)
		m_locoInfo->m_pitchRate *= 0.5f;

	m_locoInfo->m_pitch += UNIFORM_AXIAL_DAMPING * m_locoInfo->m_pitchRate;
	m_locoInfo->m_roll += UNIFORM_AXIAL_DAMPING * m_locoInfo->m_rollRate;

	m_locoInfo->m_accelerationPitchRate += (-PITCH_STIFFNESS * m_locoInfo->m_accelerationPitch) + (-PITCH_DAMPING * m_locoInfo->m_accelerationPitchRate);
	m_locoInfo->m_accelerationPitch += m_locoInfo->m_accelerationPitchRate;

	m_locoInfo->m_accelerationRollRate += (-ROLL_STIFFNESS * m_locoInfo->m_accelerationRoll) + (-ROLL_DAMPING * m_locoInfo->m_accelerationRollRate);
	m_locoInfo->m_accelerationRoll += m_locoInfo->m_accelerationRollRate;

	info.m_totalPitch = m_locoInfo->m_accelerationPitch + m_locoInfo->m_pitch;
	info.m_totalRoll = m_locoInfo->m_accelerationRoll + m_locoInfo->m_roll;

	if (m_locoInfo->m_accelerationPitch > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationPitch < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = -ACCEL_PITCH_LIMIT;

	if (m_locoInfo->m_accelerationRoll > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationRoll < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = -ACCEL_PITCH_LIMIT;

	float oldZ = m_locoInfo->m_overlapZ;
	float wanted = (front + back) * 0.5f - pos.z;
	m_locoInfo->m_overlapZ += m_locoInfo->m_overlapZVel;

	if ((wanted >= oldZ && wanted <= m_locoInfo->m_overlapZ) || (wanted <= oldZ && wanted >= m_locoInfo->m_overlapZ))
	{
		m_locoInfo->m_overlapZ = wanted;
		m_locoInfo->m_overlapZVel = 0.0f;
	}
	else if ((wanted > m_locoInfo->m_overlapZ && m_locoInfo->m_overlapZVel < 0.0f) || (wanted < m_locoInfo->m_overlapZ && m_locoInfo->m_overlapZVel > 0.0f))
	{
		m_locoInfo->m_overlapZ -= m_locoInfo->m_overlapZVel;
		m_locoInfo->m_overlapZVel = 0.0f;
	}
	else if (wanted > m_locoInfo->m_overlapZ)
	{
		m_locoInfo->m_overlapZVel += 1.5f;
	}
	else
	{
		m_locoInfo->m_overlapZVel -= 0.5f;
	}

	info.m_totalZ = m_locoInfo->m_overlapZ;
	info.m_totalYaw = 0.0f;
}

void Drawable::calcPhysicsXformGiantBird(const LocomotorTemplate *loco, const ObjectSnapshot &rec, const RenderInterpolation::Pose &pose, PhysicsXformInfo &info)
{
	// RW 0x670E09 / donor 0x00270B0F: roll ease towards banked angle, clamped to +-0.8
	m_locoInfo->m_giantBirdRoll *= 0.8f;
	info.m_totalRoll = m_locoInfo->m_giantBirdRoll;
	info.m_totalPitch = 0.0f;
	info.m_totalYaw = 0.0f;
	info.m_totalZ = 0.0f;
}

void Drawable::calcPhysicsXformShip(const LocomotorTemplate *loco, const ObjectSnapshot &rec, const RenderInterpolation::Pose &pose, PhysicsXformInfo &info)
{
	const float ACCEL_PITCH_LIMIT = loco->m_accelerationPitchLimit;
	const float PITCH_STIFFNESS = loco->m_pitchStiffness;
	const float ROLL_STIFFNESS = loco->m_rollStiffness;
	const float PITCH_DAMPING = loco->m_pitchDamping;
	const float ROLL_DAMPING = loco->m_rollDamping;
	const float FORWARD_VEL_COEF = loco->m_forwardVelocityPitchFactor;
	const float FORWARD_ACCEL_COEF = loco->m_forwardAccelerationPitchFactor;
	const float LATERAL_ACCEL_COEF = loco->m_lateralAccelerationRollFactor;
	const float UNIFORM_AXIAL_DAMPING = loco->m_uniformAxialDamping;
	const float PITCH_KICK = loco->m_bounceAmount;

	float length = 20.0f;
	float width = 10.0f;
	float height = 15.0f;
	if (m_template)
	{
		const std::vector<ObjectGeometry::Shape> &shapes = ObjectGeometry::cachedShapesOf(*m_template);
		if (!shapes.empty())
		{
			length = shapes[0].majorRadius;
			width = shapes[0].minorRadius;
			height = shapes[0].height;
		}
	}

	float shipBasis[9];
	resolvePoseBasis(pose, shipBasis);
	Coord3D dir{ shipBasis[0], shipBasis[3], 0.0f };
	float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
	if (dirLen > 0.0f)
	{
		dir.x /= dirLen;
		dir.y /= dirLen;
	}
	else
	{
		dir = Coord3D{ 1.0f, 0.0f, 0.0f };
	}

	Coord3D vel;
	vel.x = pose.position.x - rec.previousPos.x;
	vel.y = pose.position.y - rec.previousPos.y;
	vel.z = 0.0f;

	m_locoInfo->springStep(m_locoInfo->m_pitchRate, m_locoInfo->m_pitch, PITCH_STIFFNESS, PITCH_DAMPING);
	m_locoInfo->springStep(m_locoInfo->m_rollRate, m_locoInfo->m_roll, ROLL_STIFFNESS, ROLL_DAMPING);
	m_locoInfo->m_pitch += UNIFORM_AXIAL_DAMPING * m_locoInfo->m_pitchRate;
	m_locoInfo->m_roll += UNIFORM_AXIAL_DAMPING * m_locoInfo->m_rollRate;

	m_locoInfo->springStep(m_locoInfo->m_accelerationPitchRate, m_locoInfo->m_accelerationPitch, PITCH_STIFFNESS, PITCH_DAMPING);
	m_locoInfo->m_accelerationPitch += m_locoInfo->m_accelerationPitchRate;
	m_locoInfo->springStep(m_locoInfo->m_accelerationRollRate, m_locoInfo->m_accelerationRoll, ROLL_STIFFNESS, ROLL_DAMPING);
	m_locoInfo->m_accelerationRoll += m_locoInfo->m_accelerationRollRate;

	info.m_totalPitch = m_locoInfo->m_pitch + m_locoInfo->m_accelerationPitch;
	info.m_totalRoll = m_locoInfo->m_roll + m_locoInfo->m_accelerationRoll;

	if (!rec.aboveTerrain)
	{
		float forwardVel = vel.x * dir.x + vel.y * dir.y;
		m_locoInfo->m_pitchRate += -(FORWARD_VEL_COEF * forwardVel);
		m_locoInfo->m_accelerationPitchRate += -(FORWARD_ACCEL_COEF * forwardVel * 0.1f);
	}

	if (m_locoInfo->m_accelerationPitch > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationPitch < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationPitch = -ACCEL_PITCH_LIMIT;

	if (m_locoInfo->m_accelerationRoll > ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = ACCEL_PITCH_LIMIT;
	else if (m_locoInfo->m_accelerationRoll < -ACCEL_PITCH_LIMIT)
		m_locoInfo->m_accelerationRoll = -ACCEL_PITCH_LIMIT;

	// PORT-132 (issue #334): the bobbing amplitudes and rates are the locomotor template's four correction fields, not constants. A ship rights
	// its roll and pitch with them: the mod's 13 ship locomotors set them (LargeShipLocomotor, VeryLargeShipLocomotor, TransportShipLocomotor,
	// SmallShipLocomotor ...: 0.073 / 0.021 / 0.163 / 0.016 and others), and a template that leaves them 0 neither bobs nor is corrected.
	// The reads are in this ship body (RW 0x6732DC; the parse rows RW 0xBF48E8 / 0xBF48F8 / 0xBF4908 / 0xBF4918, parser RW 0x42ED00):
	//   RW 0x67386C movss xmm0,[eax+0x120] (AeleronCorrectionRate)  -> RW 0x673874 addss xmm0,[ecx+0x34] -> RW 0x673879 store: the roll
	//   accumulator DrawableLocoInfo +0x34 (m_yawModulator) steps by the rate, then RW 0x67388C call 0xA3CF90 (sin) ->
	//   RW 0x673891 fmul dword ptr [ebp+8] (the AeleronCorrectionDegree read at RW 0x673847 mov ecx,[eax+0x11C], kept at RW 0x673855) ->
	//   RW 0x673894 fadd dword ptr [esi+4] -> RW 0x673897: PhysicsXformInfo::m_totalRoll takes the amplitude times the sine.
	//   RW 0x67384D movss xmm0,[eax+0x118] (ElevatorCorrectionRate) -> RW 0x6738A5 addss xmm0,[ebp+0xc] -> RW 0x6738AA store into
	//   DrawableLocoInfo +0x38 (m_pitchModulator), then RW 0x6738BB call 0xA3CF84 (cos) -> RW 0x6738C0 fmul dword ptr [ebp-0x40] (the
	//   ElevatorCorrectionDegree read at RW 0x673858 mov ecx,[eax+0x114], kept at RW 0x67385E) -> RW 0x6738C5 fadd dword ptr [esi] ->
	//   RW 0x6738CD: PhysicsXformInfo::m_totalPitch takes the amplitude times the cosine.
	// The two accumulators sit at +0x34 / +0x38 of the 0x58-byte DrawableLocoInfo, whose order this struct keeps; the donor agrees
	// (DONOR BFME2 0x00272FB6, DrawablePhysicsXformShip.cpp:298-305: the same four offsets, sin for the roll and cos for the pitch).
	m_locoInfo->m_yawModulator += loco->m_aeleronCorrectionRate;
	info.m_totalRoll += std::sin(m_locoInfo->m_yawModulator) * loco->m_aeleronCorrectionDegree;
	m_locoInfo->m_pitchModulator += loco->m_elevatorCorrectionRate;
	info.m_totalPitch += std::cos(m_locoInfo->m_pitchModulator) * loco->m_elevatorCorrectionDegree;

	float pitchHeight = std::sin(info.m_totalPitch) * length;
	float rollHeight = std::sin(info.m_totalRoll) * width;
	const float Z_SCALE = 0.7f;
	info.m_totalZ = (std::abs(pitchHeight) + std::abs(rollHeight)) * Z_SCALE - 0.5f * height;
	if (info.m_totalZ < 0.0f)
		info.m_totalZ = 0.0f;
	info.m_totalYaw = 0.0f;
}

void Drawable::applyPhysicsXform(const ObjectSnapshot &rec, const RenderInterpolation::Pose &poseIn)
{
	// RW 0x67BE90 applies the physics to the drawable's own transform (its matrix after the interpolation). A pose that turns about Z carries only its
	// angle (RenderInterpolation: `basis` stays the identity), and the drawable has already taken it (setOrientation / commitSync run first): the
	// basis is the drawable's. Owner 2026-11-10: with the identity here every moving unit with presentation physics was drawn facing +x and turned
	// only when it stopped (the stale pose is not interpolated, its record's basis is full)
	RenderInterpolation::Pose pose = poseIn;
	if (pose.zRotation)
	{
		std::memcpy(pose.basis, getBasis(), sizeof(pose.basis));
		pose.angle = getOrientation();
	}
	if (!rec.locomotorTemplate)
	{
		m_hasPhysicsXform = false;
		m_physicsStepped = false;
		return;
	}
	// RW 0x67BDF6 .. 0x67BE17: the step is made once per client frame (`call [TheGameClient + 0x7C]`, compared with the stamp at the locomotor info
	// + 0xAC, RW 0x67BE04 `cmp` / `je`; stamped at RW 0x67BE17). Retail draws one picture per client frame, so it has nothing between two steps;
	// INFERENCE: the port, drawing more often, keeps the last step's angles and height on the pictures between two client frames
	const bool step = !m_services.clientFrame || !m_physicsStepped || *m_services.clientFrame != m_physicsClientFrame;
	if (step)
	{
		PhysicsXformInfo info;
		m_hasPhysicsXform = calcPhysicsXform(rec, pose, info);
		if (m_hasPhysicsXform)
		{
			m_lastPhysicsInfo = info;
		}
		m_physicsStepped = true;
		m_physicsClientFrame = m_services.clientFrame ? *m_services.clientFrame : 0;
	}
	if (m_hasPhysicsXform)
	{
		const PhysicsXformInfo &info = m_lastPhysicsInfo;

		float basis[9];
		resolvePoseBasis(pose, basis);
		Matrix3D mtx;
		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 3; ++c)
			{
				mtx.Row[r][c] = basis[r * 3 + c];
			}
		}
		mtx.Row[0][3] = pose.position.x;
		mtx.Row[1][3] = pose.position.y;
		mtx.Row[2][3] = pose.position.z;

		// Retail RW 0x67BE90 (donor BFME2 0x0027BB65):
		// Translate Z, Rotate Y (pitch), Rotate X (-roll), Rotate Z (yaw)
		mtx.Translate(Vector3(0.0f, 0.0f, info.m_totalZ));
		mtx.Rotate_Y(info.m_totalPitch);
		mtx.Rotate_X(-info.m_totalRoll);
		mtx.Rotate_Z(info.m_totalYaw);

		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 3; ++c)
			{
				m_presentationBasis[r * 3 + c] = mtx.Row[r][c];
			}
		}
		m_presentationPosition.x = mtx.Row[0][3];
		m_presentationPosition.y = mtx.Row[1][3];
		m_presentationPosition.z = mtx.Row[2][3];
	}
}

// lane PORT-105: RW 0x4B516C (ObjectDrawInterface slot 0x50, reached from Drawable RW 0x67476B) keeps the transform the module is drawn at
// (module +0x290), the weight (+0x2C0) and the client frame of the last step (+0x2C4); the weight starts at the argument
void Drawable::releaseFromBone(float snappyness)
{
	getPresentationPose(m_releaseBasis, m_releasePosition);
	m_releaseAngle = getOrientation();
	m_releaseWeight = snappyness;
	m_releaseClientFrame = m_services.clientFrame ? *m_services.clientFrame : 0;
	m_releaseActive = true;
	// RW 0x67476B after the module loop drops the drawable's pending model condition sets (memset +0x2A4 and +0x2F0, 0x4C bytes each). This port keeps
	// no pending sets: a changed bit is the drawable's flags until the flush applies them (lane ANIM-1), so there is nothing to drop.
}

// lane PORT-105: RW 0x4B6B21 .. 0x4B6BB2. While the weight is below 1.0 the drawn transform is RW 0xB27C80(snapshot, current, weight): the
// translation lerped, the rotation slerped. The weight grows by 1/30 per client frame (RW 0xBDFC6C, the frame stamp +0x2C4 gates the step) and is
// clamped to 1.0 (RW 0xBD1908); from 1.0 on the blend is off and the module draws the object's own transform (RW 0x4B6B34 .. 0x4B6B3B).
void Drawable::applyReleaseBlend(RenderInterpolation::Pose &p)
{
	if (!m_releaseActive)
	{
		return;
	}
	if (!(m_releaseWeight < 1.0f))
	{
		m_releaseActive = false; // RW 0x4B6BAB .. 0x4B6BB2: the finish branch (+0x2C4 = -1, the weight = 1.0)
		return;
	}
	// without a client clock every sync is one client frame (the same reading lane LOCAL-25 takes at RW 0x67BDF6 .. 0x67BE17)
	const unsigned frame = m_services.clientFrame ? *m_services.clientFrame : m_releaseClientFrame + 1u;
	if (!m_services.clientFrame || frame != m_releaseClientFrame)
	{
		m_releaseWeight += 1.0f / 30.0f; // RW 0xBDFC6C (0.033333335)
		if (m_releaseWeight > 1.0f)
		{
			m_releaseWeight = 1.0f; // RW 0x4B6B86 (RW 0xBD1908)
		}
		m_releaseClientFrame = frame;
	}
	p = RenderInterpolation::interpolate(true, m_releasePosition, m_releaseAngle, m_releaseBasis, p.position, p.angle, p.basis, m_releaseWeight);
}

void Drawable::getPresentationPose(float outBasis[9], Coord3D &outPos) const
{
	if (m_hasPhysicsXform)
	{
		std::memcpy(outBasis, m_presentationBasis, sizeof(m_presentationBasis));
		outPos = m_presentationPosition;
	}
	else
	{
		std::memcpy(outBasis, getBasis(), sizeof(float) * 9);
		outPos = *getPosition();
	}
}

Coord3D Drawable::getPresentationPosition() const
{
	if (m_hasPhysicsXform)
	{
		return m_presentationPosition;
	}
	return *getPosition();
}
