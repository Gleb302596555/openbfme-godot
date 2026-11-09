// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
//
// Drawable (ZH Include/GameClient/Drawable.h, Source/GameClient/Drawable.cpp:349-505, 4162-4186), lane LOGIC-1: the client half of a live
// Object. It owns the draw modules of the object's template (built from the merged draw runtime: W3DScriptedModelDraw / W3DHordeModelDraw
// for model draws, static model names for tree / prop / floor draws), the object's model condition flags, its instance scale, its
// house colour and a RENDER transform that follows the object.
//
// TARGET FACTS (RotWK game.dat, caveat S-001): the Drawable constructor RW 0x679FD7 makes the draw modules in template list order, each
// skipped only when the GameLOD module gate is on and its minimum LOD is above the static LOD (S-114: every module is created here); the
// instance scale starts as the template's Scale; the Object binds the drawable (RW 0x628882 sendObjectCreated -> Object::friend_bindToDrawable,
// which calls onDrawableBoundToObject on every module). DONOR (ZH Object::friend_bindToDrawable, Object.cpp:2975-3010): with GameData
// ForceModelsToFollowTimeOfDay / ForceModelsToFollowWeather a new drawable gets NIGHT / SNOW when the map's time of day / weather is.
//
// RENDER INTERPOLATION (stops S-151, S-810 .. S-813, lane SMOOTH-1). Retail records the object's transform in logic phase 2 (Object::recordTransform,
// RW 0x6260E1), gathers the drawable's interpolation inputs in phase 1 of the next frame (RW 0x674B1F, the "drawableCallback" row) and draws RW 0x6765B9:
// a Catmull-Rom position through the previous, recorded, current and pending positions and a slerped rotation, by the client alpha. This port gathers the
// same inputs into the immutable snapshot of a completed frame (GameClient/LogicSnapshot) and draws RenderInterpolation::retailPose from it
// (syncFromSnapshot). The drawable knows its object by id only and never reads or writes logic state; it consumes no simulation randomness (the draw
// modules use their own client random, W3DClientRandom).
// What the draw modules do not do yet: bones (turrets, barrels, particle bones are not built: S-095 / S-097), the Lua BeginScript bodies run
// on LUA-1's real Lua (W3DLuaDrawScriptHost; what its api cannot answer is S-126), the draw variants' own motion (S-112), effect draws (S-113).

#pragma once

#include "Common/Module.h"
#include "Common/ModelState.h"
#include "Common/Thing/Thing.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableShroud.h"
#include "GameClient/LogicSnapshot.h"
#include "GameClient/RenderInterpolation.h"
#include "GameClient/StealthLook.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLuaDrawScriptHost.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <memory>
#include <string>
#include <vector>

class Object;

typedef std::uint32_t DrawableID;

// The services every draw module of a game shares (one per DrawableManager).
class ModuleFactory;
class AnimationSoundModuleManager;
class ModelConditionAudioLoopClientBehavior;

// LOCAL-3: TintEnvelope (ZH Include/GameClient/TintEnvelope.h, RW 0x671B20 .. 0x674AD7 / BFME2 0x671800 .. 0x6747B1, both editions identical).
// Steps a drawable's additive tint or selection flash through attack, sustain and decay at the client frame rate, with optional sine modulation by the
// logic frame (RW 0x671B92).
class TintEnvelope
{
public:
	enum EnvelopeState : std::int8_t
	{
		ENV_STATE_REST = 0,
		ENV_STATE_ATTACK = 1,
		ENV_STATE_DECAY = 2,
		ENV_STATE_SUSTAIN = 3,
	};

	TintEnvelope() = default; // RW 0x671B20 / BFME2 0x671800

	void play(const RGBColor *peak, UnsignedInt attackFrames = 0, UnsignedInt decayFrames = 0, UnsignedInt sustainAtPeak = 0); // RW 0x674818 / BFME2 0x6744F2
	void update();                                                                                                             // RW 0x6748B7 / BFME2 0x674591
	bool isEffective() const { return m_affect; }                                                                              // RW +0x39
	void setEffective(bool affect) { m_affect = affect; }
	bool isResting() const { return m_envState == ENV_STATE_REST; } // RW +0x38 == 0
	const RGBColor *getColor(UnsignedInt logicFrame = 0) const;     // RW 0x671B92 / BFME2 0x671872
	void setAttackFrames(UnsignedInt frames);                       // RW 0x671C25 / BFME2 0x671905
	void setDecayFrames(UnsignedInt frames);                        // RW 0x671CBA / BFME2 0x67199A
	void setPulse(float amplitude, float frequency)                 // RW 0x671CA1 / BFME2 0x671981
	{
		m_amplitude = amplitude;
		m_frequency = frequency;
	}
	void setSustainCounter(UnsignedInt count) { m_sustainCounter = count; } // RW +0x34
	EnvelopeState state() const { return m_envState; }
	UnsignedInt sustainCounter() const { return m_sustainCounter; }

private:
	RGBColor m_attackRate{ 0.0f, 0.0f, 0.0f };           ///< +0x04..+0x0C
	RGBColor m_decayRate{ 0.0f, 0.0f, 0.0f };            ///< +0x10..+0x18
	RGBColor m_peakColor{ 0.0f, 0.0f, 0.0f };            ///< +0x1C..+0x24
	RGBColor m_currentColor{ 0.0f, 0.0f, 0.0f };         ///< +0x28..+0x30
	UnsignedInt m_sustainCounter = 0;                    ///< +0x34
	EnvelopeState m_envState = ENV_STATE_REST;           ///< +0x38
	bool m_affect = false;                               ///< +0x39
	float m_amplitude = 0.0f;                            ///< +0x3C
	float m_frequency = 0.0f;                            ///< +0x40
	mutable RGBColor m_modulatedColor{ 0.0f, 0.0f, 0.0f }; ///< +0x44..+0x4C
};

// lane FX-2: the client effect player the draw modules call (GameClient/LiveFX). Read at call time, so drawables made before the player was installed use it too.
class DrawableFXHost
{
public:
	virtual ~DrawableFXHost() = default;
	// RW 0x4BF222: a model draw of drawable `id` entered an animation state with EnteringStateFX `fx`; true when it played
	virtual bool enteringStateFX(DrawableID id, const std::string &fx) = 0;
};

struct DrawServices
{
	DrawServices(WW3DAssetManager &a, ModuleFactory &m, const W3DLuaDrawScriptHost::Options &hostOptions)
		: assets(a)
		, modules(m)
		, drawAssets(a)
		, random(RandomAlgorithm::ZH_CarryChain)
		, host(hostOptions)
	{
		random.seed(1); // a fixed client seed (the retail initial client seed is S-093)
	}
	WW3DAssetManager &assets;
	ModuleFactory &modules;  ///< makes the ClientUpdate / ClientBehavior modules (every class registered; unported ones are explicit)
	WW3DDrawAssets drawAssets;
	W3DClientRandom random;
	W3DLuaDrawScriptHost host; ///< the real Lua of the drawable state (lane LUA-1): BeginScript bodies of every draw module
	DrawableFXHost *fx = nullptr; ///< lane FX-2 (null: EnteringStateFX and ParticleSysBone stay S-095)
	AnimationSoundModuleManager *animationSounds = nullptr; ///< lane AUDIO-4: TheAnimationSoundModuleManager of the game's drawables
	bool selectionFlashHouseColor = true; ///< lane LOCAL-3: GameData SelectionFlashHouseColor (RW 0xC005E0, GlobalData +0xB58)
	float selectionFlashSaturationFactor = 0.6f; ///< lane LOCAL-3: GameData SelectionFlashSaturationFactor (RW 0xC005D0, GlobalData +0xB54)
};

// One draw module of a drawable.
struct DrawEntry
{
	std::string className;
	std::string tag;
	W3DDrawKind kind = W3D_DRAWKIND_NOT_DRAWN;
	const ModuleData *data = nullptr;
	std::string staticModel;          ///< tree / prop / floor kinds: the model the module shows ("" when none)
	std::string notDrawnReason;       ///< why a non-model entry draws nothing
	std::unique_ptr<W3DScriptedModelDraw> draw; ///< MODEL kind
	bool animated = false;            ///< advance() would change the draw (W3DScriptedModelDraw::needsAdvance, set at creation and on every flag change, never lowered by an advance: the pose after a blend's promotion is still pushed): advance() each render frame
	bool missingModel = false;        ///< the model is not in the mounted archives (a retail data defect): not drawn
	std::string lastModel;            ///< the model last handed to the device layer (the device layer keeps its own handle)
	bool moduleHidden = false;        ///< a CurDrawableHideModule / ObjectHideModule request of a script hid this module (Drawable::showModule, RW 0x6789B4): not drawn
	float constructionOffsetZ = 0.0f; ///< RENDER-2: the model's move along its own Z while being built (RW 0x4B686D, ADJUST_HEIGHT_BY_CONSTRUCTION_PERCENT)
	bool noRotate = false;            ///< the model draw's NoRotate (INFERENCE: drawn at the object's position without its orientation, S-5204)
	bool conditionHidden = false;     ///< RENDER-2: a W3DFloorDraw whose HideIfModelConditions match the drawable's flags (the foundation floor while being built)
	std::vector<ModelConditionFlags> hideIf; ///< RENDER-2: that floor draw's HideIfModelConditions (one set per line)
	// lane COMBAT-4: a W3DTruckDraw's tire spin (RW 0x4CBFFB): the front and rear wheel angles (W3DTruckDraw + 0x310 / + 0x314; the mid tires take the front's / the
	// rear's: + 0x318 / + 0x31C) and the bones they turn (the module data's tire bone names; the front set, then the rear set)
	float tireFront = 0.0f, tireRear = 0.0f;
	std::vector<std::string> frontTireBones, rearTireBones;
	unsigned tireChanges = 0; ///< bumped when an angle changed (the device layer re-poses the instance)
	// advanceTires: dynamic_cast once per entry. A rebuilt entry is a new DrawEntry, so this starts unset; a replaced data pointer misses the cache.
	const ModuleData *truckData = nullptr;
	bool truckChecked = false;
	bool isTruck = false;
};

class Drawable : public Thing
{
public:
	// Builds the draw modules of `tt` (a final override). Problems are appended to `problems` ("class: message"), never dropped.
	// SMOOTH-1: the drawable knows its object by id only (it lives on the render side; the object belongs to the simulation's thread)
	// `clientModules`: the template's ClientUpdate / ClientBehavior modules as the logic owner resolved them (ClientEvent::clientModules; review r2:
	// the drawable never interns into the logic's name keys)
	// `variation`: the model variation (drawable + 0x364, RW 0x679FEA): an object's creation draw, -1 for the others
	Drawable(DrawServices &services, const ThingTemplate *tt, DrawableID id, ObjectID objectId, const std::vector<ClientEvent::ClientModule> &clientModules,
		std::vector<std::string> &problems, int variation = -1);

	Drawable *asDrawable() override { return this; }
	DrawableID getID() const { return m_id; }
	ObjectID getObjectID() const { return m_objectId; }
	// SMOOTH-1: the object's KindOf bits as the creation event carried them (Object::isKindOfName for the render side)
	void setKindOf(const KindOfMaskType &kindOf) { m_kindOf = kindOf; }
	bool isKindOfName(const char *name) const;
	// lane SELWALL-1: the same by the bit's index (ObjectTemplateInfoBuilder::kindOfIndex; -1: false), for a per-frame test without the name lookup
	bool isKindOfIndex(int bit) const;
	// lane FX-3: the object's target record as the latest event or snapshot carried it (what the draw scripts ask: DrawableScriptTarget.h)
	void setScriptTarget(const DrawableScriptTarget &t) { m_scriptTarget = t; }
	const DrawableScriptTarget &scriptTarget() const { return m_scriptTarget; }

	// ZH Drawable::setModelConditionState / clearModelConditionState and the whole-set form this layer needs
	const ModelConditionFlags &getModelConditionFlags() const { return m_flags; }
	// lane COMBAT-4, RW 0x4BF2D8 (W3DModelDraw::replaceModelConditionState): the union of the model draws' DependencySharedModelFlags (+ 0xBC of the module data)
	ModelConditionFlags dependencySharedModelFlags() const;
	// the dependent's half of RW 0x4BF2D8: the container's flags within `shared` are set, the shared ones it lacks cleared (RW 0x67651E clearAndSet on the dependent
	// drawable); the draw modules take the result at once when it changed. True when it changed
	bool applyDependencyFlags(const ModelConditionFlags &shared, const ModelConditionFlags &containerFlags);
	void setModelConditionFlags(const ModelConditionFlags &flags);
	void setModelCondition(int bit, bool on);
	// lane ANIM-1, RW 0x679512 / 0x67449C (see ClientEvents.h): a changed bit is stored and marks the drawable dirty; flushModelConditions hands the
	// stored flags to the draw modules (setModelConditionFlags) when dirty, with the object's weapon cycle for UseWeaponTiming as the flush event carried it
	void storeModelCondition(int bit, bool on);
	void storeModelConditions(const ModelConditionFlags &flags); ///< lane SAVE-4e: the whole set (RW 0x679512's argument), dirty when it differs
	void flushModelConditions(bool hasWeaponTiming, int weaponTimingFrames);
	bool modelConditionsDirty() const { return m_flagsDirty; }
	// the weapon cycle the last flush carried (-1: the object had no current weapon), what UseWeaponTiming animations divide by
	int weaponTimingFrames() const { return m_hasWeaponTiming ? m_weaponTimingFrames : -1; }
	// incremented whenever the model condition flags are set: the device layer re-reads the draw frames of a drawable whose count changed
	unsigned getChangeCount() const { return m_changeCount; }

	float getInstanceScale() const { return m_scale; }
	void setInstanceScale(float scale) { m_scale = scale; }
	// 0xAARRGGBB of the owner's colour for model recolour (RW 0x6712A6: [drawable + 0x3AA] flag, 0 when showsHouseColor is false)
	std::uint32_t getHouseColor() const { return m_houseColor; }
	void setHouseColor(std::uint32_t argb) { m_houseColor = argb; }
	// 0xAARRGGBB of the owner's indicator colour (RW 0x674510: [drawable + 0x454] set always; read by flashAsSelected RW 0x679097)
	std::uint32_t indicatorColor() const { return m_indicatorColor; }
	void setIndicatorColor(std::uint32_t argb) { m_indicatorColor = argb; }
	// lane CAH-2 (RW 0x6727B0): the house colour set the draw modules recolour with (kind 0 none, 1 .. 3 colours, HouseColor.h); the revision moves on
	// every set so the device layer re-applies it
	void setCustomColors(int kind, const std::uint32_t colors[3])
	{
		m_customKind = kind;
		for (int i = 0; i < 3; ++i)
		{
			m_customColors[i] = colors[i];
		}
		++m_customRevision;
	}
	int customColorKind() const { return m_customKind; }
	const std::uint32_t *customColors() const { return m_customColors; }
	std::uint32_t customColorRevision() const { return m_customRevision; }
	bool drawsInMirror() const { return m_drawsInMirror; }
	void setDrawsInMirror(bool v) { m_drawsInMirror = v; }

	std::vector<DrawEntry> &entries() { return m_entries; }
	const std::vector<DrawEntry> &entries() const { return m_entries; }
	// The ClientUpdate and ClientBehavior modules of the template, in list order (ZH Drawable.cpp:349-505: ClientUpdate modules follow the draw
	// modules; RotWK's ClientBehavior list is the fourth module list). A class that is not ported is an UnportedDrawableModule (stop S-140).
	const std::vector<std::unique_ptr<DrawableModule>> &clientModules() const { return m_clientModules; }
	// ZH Object::friend_bindToDrawable -> Drawable: calls onDrawableBoundToObject on the client modules
	void friend_boundToObject();
	// Drawable::createSelectionFeedback (RW 0x672ED1 / BFME2 0x672BAB) and removeSelectionFeedback (RW 0x672F0D / BFME2 0x672BE7): walk the client modules
	// calling slot 17 / slot 18 (TerrainResourceClientBehavior, RW 0x8D0A58 / 0x8D0A78, BFME2 0x8CC5AA / 0x8CC5CA)
	void createSelectionFeedback();
	void removeSelectionFeedback();
	// lane AUDIO-4: the client modules hear every transform change (Thing::setPosition / setOrientation / setTransform)
	void reactToTransformChange(const Coord3D *oldPos, float oldAngle) override;
	DrawServices &drawServices() { return m_services; }
	DrawServices &drawServices() const { return m_services; }
	bool hasAnimatedEntry() const;
	// steps every model draw's animations by `elapsedMs` of render time
	void advanceAnimation(double elapsedMs);
	// lane PERF-HD: every animated draw's advance is advanceIsLocal (and the tires are this drawable's): advanceAnimation touches only this drawable
	bool advanceIsLocal() const;
	void advanceAnimationLocal(double elapsedMs); ///< advanceAnimation without the host's chunk name (nothing it runs reports through the host)
	void advanceTires(DrawEntry &e, double elapsedMs); ///< lane COMBAT-4: W3DTruckDraw's tire spin (RW 0x4CBFFB)

	// RW 0x6789B4 Drawable::showModule: the draw module whose tag is `name` (exact, case sensitive) takes the request (true: a module exists; the
	// module-first rule: no fall through to a sub object of the same name) and its entry is hidden / shown (DrawEntry::moduleHidden).
	// RW 0x672823 Drawable::showSubObject: every model draw module hides / shows the sub object `name` (permanent: the script's permanent form).
	// Both bump the change count so the device layer re-reads the draws.
	bool showModule(const std::string &name, bool visible, bool permanent);
	// lane BUILD-4: a draw module's state script asked for a module's visibility (RW 0x734EF4 -> 0x6789B4); while the constructor still makes the modules the
	// request waits until all exist
	void scriptModuleVisible(const std::string &name, bool visible);
	void showSubObject(const std::string &name, bool visible, bool permanent);
	// RW 0x8B911E: every model draw module takes `swaps` as its upgrade textures (draw module vslot 0x84)
	void setUpgradeTextures(const std::vector<std::pair<std::string, std::string>> &swaps);

	// The render transform from the object's record in a completed logic frame's snapshot (SMOOTH-1: RenderInterpolation::retailPose, RW 0x6765B9;
	// `interpolate` false: the record's current transform, the stepped look). Then the construction look (updateConstruction).
	void syncFromSnapshot(const ObjectSnapshot &s, UnsignedInt snapshotFrame, double alpha, bool interpolate);
	// lane PERF-3: syncFromSnapshot in two parts, for DrawableManager::syncTransforms to run the first on the client job pool. prepareSync reads only this
	// drawable and the record and writes only this drawable's own state (the construction look, the move speed, the script target) and `out` (the pose,
	// with the cos / sin of a rotation about Z); commitSync then sets the transform, whose reaction (the footstep manager's lists) needs the drawables'
	// order. prepareSync then commitSync leave the state syncFromSnapshot leaves.
	struct SyncPrep
	{
		const ObjectSnapshot *rec = nullptr;
		RenderInterpolation::Pose pose;
		bool sameBasis = false; ///< a rotation about Z already set on this basis: no new orientation
		float c = 0.0f, s = 0.0f;
		bool committed = false; ///< lane PERF-HD: commitSync ran on the job pool with the reaction held back (DrawableManager::syncTransforms)
	};
	// lane PERF-HD: while muted, a transform change does not reach the client modules (syncTransforms makes the reaction afterwards, in slot order)
	void setReactionMuted(bool muted) { m_reactionMuted = muted; }
	void prepareSync(const ObjectSnapshot &rec, UnsignedInt snapshotFrame, double alpha, bool interpolate, SyncPrep &out);
	void commitSync(const SyncPrep &prep);
	// lane PERF-3: the record DrawableManager::syncTransforms found for this drawable in `snapshot` (null: none), so the render side asks the same snapshot
	// again without a search. syncedRecord answers only for the snapshot (and frame) of the last sync; otherwise `found` is false
	void setSyncedRecord(const LogicSnapshot *snapshot, const ObjectSnapshot *rec)
	{
		m_syncSnapshot = snapshot;
		m_syncFrame = snapshot ? snapshot->frame : 0;
		m_syncRec = rec;
		bool wasObscured = m_shroudLook.fullyObscured;
		// lane VISUAL-2: RW 0x64849E's per-frame shroud look (GameClient/DrawableShroud.h) from the presented record
		const ShroudView *view = snapshot ? snapshot->shroud.get() : nullptr;
		m_shroudLook.update(rec ? rec->objectShroud : 0, rec && rec->effectivelyDead, m_syncFrame,
			view ? DrawableShroudLook::viewerKeyOf(view->localPlayer, view->visionPlayers) : 0);
		if (wasObscured && !m_shroudLook.fullyObscured)
		{
			m_shroudRevealed = true;
		}
	}
	// lane VISUAL-2: the shroud look of the last sync: fullyObscured is RW Drawable +0x440 (attached particle systems do not emit), hidden: not drawn
	const DrawableShroudLook &shroudLook() const { return m_shroudLook; }
	UnsignedInt syncFrame() const { return m_syncFrame; }
	// lane EVADAMAGE-1: RW Drawable +0x388 (0 at creation), the logic frame until which a damage Eva event here is "near the first" (EvaDamage.h); RW 0x671DFD
	// raises it, never lowers it
	UnsignedInt evaDamageFarFrame() const { return m_evaDamageFarFrame; }
	void raiseEvaDamageFarFrame(UnsignedInt frame) { m_evaDamageFarFrame = frame > m_evaDamageFarFrame ? frame : m_evaDamageFarFrame; }
	// lane VISUAL-2 r2: whether the device layer draws this drawable for the presented snapshot `presented` (null: none yet), in one place for the models,
	// the attached particle systems (RW 0x5FA2CE reads the drawable's visible byte +0x441), the buffs (TBuff's visible = +0x441, RW 0x4D12FA) and the
	// streaks. `hidden`: not drawn (the grace-adjusted SHROUDED status while `shroudDrawn`, an enemy's invisible object, a drawable a container hid);
	// `fogLook`: drawn fully obscured (the fogged light, RW 0x46F48E). INFERENCE (S-4971): +0x441 is the render's visible byte; the port's renderer
	// draws exactly what is not `hidden` (no view frustum test)
	struct PresentedLook
	{
		bool hidden = false;
		bool fogLook = false;
	};
	PresentedLook presentedLook(const LogicSnapshot *presented, bool shroudDrawn) const;
	// lane CHAT-1: this client's own hiding (ZH Drawable::setDrawableHidden from BeaconClientUpdate::hideBeacon, RW 0x6718FB + 0x43D): an enemy's beacon
	void setClientHidden(bool hidden) { m_clientHidden = hidden; }
	bool clientHidden() const { return m_clientHidden; }
	void markEvaFirstSighted() { m_evaFirstSighted = true; }
	bool evaFirstSighted() const { return m_evaFirstSighted; }
	void setEvaEnemySighted(bool on) { m_evaEnemySighted = on; }
	bool evaEnemySighted() const { return m_evaEnemySighted; }
	void setEvaEnemySightedAfterRespawn(bool on) { m_evaEnemySightedAfterRespawn = on; }
	bool evaEnemySightedAfterRespawn() const { return m_evaEnemySightedAfterRespawn; }
	void setEvaCastleMemberSighted(bool on) { m_evaCastleMemberSighted = on; }
	bool evaCastleMemberSighted() const { return m_evaCastleMemberSighted; }
	bool takeShroudRevealed()
	{
		bool r = m_shroudRevealed;
		m_shroudRevealed = false;
		return r;
	}
	// RW 0x67561E Drawable::notifyDead (called on Object death at RW 0x699077):
	// if m_evaEnemySighted (+0x445) is set, clears it to 0 and sets m_evaEnemySightedAfterRespawn = 1 (+0x446)
	void notifyDead()
	{
		if (m_evaEnemySighted)
		{
			m_evaEnemySighted = false;
			m_evaEnemySightedAfterRespawn = true;
		}
	}
	// TOP20 #24, RW 0x672D28 setShadowsEnabled: status bit 2 (+ 0x114) set when on, cleared when off; every draw module's vslot 0x30 takes the same answer (the
	// port's renderer reads shadowsEnabled() for each module's instance)
	void setShadowsEnabled(bool on) { m_shadowsEnabled = on; }
	bool shadowsEnabled() const { return m_shadowsEnabled; }
	// lane PORT-21 (stop S-1463): the audible flag at Drawable + 0x44A (1 at construction). RW 0x674727 Drawable::setInaudible clears it and asks every
	// client behaviour module's second interface for slot 1 (RW 0x674756: module + 0x30 table, `call [edx + 4]`), RW 0x67896F Drawable::setAudible sets
	// it again and asks slot 0 (RW 0x6789A0 `call [edx]`). Both return at once when the flag already has that value (RW 0x674733 / 0x67897B), so a
	// repeated request tells no module anything twice.
	void setAudible(bool audible);
	bool audible() const { return m_audible; }
	const ObjectSnapshot *syncedRecord(const LogicSnapshot &snapshot, bool &found) const
	{
		found = m_syncSnapshot == &snapshot && m_syncFrame == snapshot.frame;
		return found ? m_syncRec : nullptr;
	}
	// RENDER-2, once per client frame: RW 0x675996 Drawable::updateDrawable -> RW 0x6730E1: while the flags hold ACTIVELY_BEING_CONSTRUCTED every model draw's
	// RW 0x4B51B5 (the build-up animation frame from the object's construction percent), and every model draw's RW 0x4B686D height adjustment. `alpha` is
	// the client sub-frame fraction; the cached percent is refreshed on the first client frame of a new logic frame (RW 0x63252F). The rate is
	// 1 / the build frames of the object's GettingBuiltBehavior (RW 0x68BD71 recomputes calcTimeToBuild with no producer: INFERENCE, the same number unless
	// a producer changes it). Bumps the change count when a frame or an offset changed.
	void updateConstruction(const ObjectSnapshot &s, UnsignedInt snapshotFrame, double alpha);
	// RENDER-2: where entry `e`'s model is drawn: the render position moved along the drawable's own Z axis by the entry's construction offset (times the instance
	// scale, as the offset is in model units)
	Coord3D entryPosition(const DrawEntry &e) const;

	// ---- lane PROJ-2: the drawable's fade (GameClient/DrawableFade.cpp; RW Drawable +0x128 mode, +0x12C counter, +0x130 target, +0x134 the fade-in after
	// the invisible frames, +0x37C the client frame of the last step, +0xB0 opacity, +0x43D hidden) ----
	struct Fade
	{
		int mode = 0;           ///< 0 none, 1 fading in, 2 fading out, 3 / 4 / 5 delayed (5: hidden, then fade in: RW 0x67309D)
		double counter = 0.0;   ///< client frames (30 per second, retail's client frame rate) since the fade started, at most `target`
		double target = 0.0;    ///< client frames
		double fadeIn = 0.0;    ///< mode 5: the fade-in that follows, client frames
		float opacity = 1.0f;
		bool hidden = false;
		UnsignedInt seenFire = 0; ///< the projectile launch (ObjectSnapshot::projectileFireFrame) the fade was started for
	};
	// RW 0x85EE00 .. 0x85EF31: a projectile launched (or bounced) in the snapshot's frame starts its drawable's fade: InvisibleFrames / FadeInTime when the
	// local player sees the launcher and the end of the flight, a fade over the flight when only one of them is seen, hidden when neither is
	void startProjectileFade(const ObjectSnapshot &rec, const LogicSnapshot &snapshot);
	// RW 0x675AB7 .. 0x675BBF (Drawable::updateDrawable): one step of the fade by `elapsedMs` of render time
	void updateFade(double elapsedMs);
	const Fade &fade() const { return m_fade; }
	// RW 0x670AA2 Drawable::fadeIn(frames): 0 shows at once; else the opacity rises from 0 over `frames` client frames (lane BUILD-4: public, the
	// simulation's FADE_IN request for a wall span's tile)
	void fadeIn(double frames);
	void fadeOut(double frames); ///< RW 0x670A50 (lane PORTERFOG-1: public, the simulation's FADE_OUT request for a porter at work)
	// what the device layer draws: 0 while hidden, else the fade's opacity (1 without a fade)
	float drawOpacity() const { return m_fade.hidden ? 0.0f : m_fade.opacity; }
	// lane BUILD-5: ZH Drawable::setDrawableOpacity, the opacity at Drawable + 0xB0 (the field the fade steps): the placement ghost's 0.45 (RW 0x69C714)
	void setDrawableOpacity(float opacity) { m_fade.opacity = opacity; }
	// lane FBSIM-1: the phantom structure's opacity (ObjectSnapshot::phantomStructure)
	void updatePhantomLook(const ObjectSnapshot &rec);

	// ---- lane STEALTHLOOK-1: the drawable's stealth look (GameClient/StealthLook.h; RW 0x6760F9 setter and the per-client-frame step RW 0x673114) ----
	// the snapshot's look drives the setter (retail's callers hand it (look, min, max, cycle) unconditionally); the object's HORDE parent
	// (ObjectSnapshot::stealthParent, RW 0x693A1A) makes the drawable imitate instead. `mgr` resolves the parent drawable
	void syncStealthLook(const ObjectSnapshot &rec);
	// RW 0x673114, once per client frame: imitate the stealth parent or step the pulse / blend. `parent` is the drawable of stealthParent (null: none / early out)
	void updateStealthLook(const Drawable *parent);
	ObjectID stealthParentId() const { return m_stealthParentId; }
	const StealthLook::State &stealth() const { return m_stealth; }
	float stealthOpacity() const { return m_stealth.opacity; }
	bool stealthHidden() const { return m_stealth.hidden; }
	// RW +0x3AB: a draw module's data is not affected by stealth (the drawable cannot hide for look 5)
	bool cannotHideByStealth() const { return m_cannotHideByStealth; }

	// ---- LOCAL-3: the drawable's tint and selection flash envelopes (RW Drawable +0x64 m_selectionFlashEnvelope, +0x68 m_colorTintEnvelope,
	// +0x6C..+0x88 custom tint parameters, +0x118 m_tintStatus, +0x11C m_prevTintStatus; both editions identical) ----
	enum TintStatus : UnsignedInt
	{
		TINT_STATUS_DISABLED = 0x01u,               ///< RW 0x675DAD: disabled / dazed dark tint (-0.5, -0.5, -0.5) at RW 0xDA06E8
		TINT_STATUS_IRRADIATED = 0x02u,             ///< RW 0x675DF3: irradiated green tint (0, 1, 0)
		TINT_STATUS_POISONED = 0x04u,               ///< RW 0x675DD0: poisoned green-red tint (0.5, 1, 0)
		TINT_STATUS_GAINING_SUBDUAL_DAMAGE = 0x08u, ///< RW 0x675E1E: frenzy pulsing red tint (0.5, -0.5, -0.5) at RW 0xDA0700
		TINT_STATUS_FRENZY = 0x10u,                 ///< RW 0x675E5E: locked red tint (0.5, -0.5, -0.5) at RW 0xDA0700
		TINT_STATUS_CUSTOM = 0x20u,                 ///< RW 0x675E9A: custom tint envelope (+0x6C..+0x88)
	};
	void setTintStatus(UnsignedInt statusBits) { m_tintStatus |= statusBits; }   // RW +0x118
	void clearTintStatus(UnsignedInt statusBits) { m_tintStatus &= ~statusBits; }
	bool testTintStatus(UnsignedInt statusBits) const { return (m_tintStatus & statusBits) != 0u; }
	UnsignedInt tintStatus() const { return m_tintStatus; }
	// RW 0x671A73 / BFME2 0x671753 (Drawable::setCustomTintEnvelope) + caller `or [drawable+0x118], 0x20` (RW 0x5E0127 / 0x4D1241 / 0x6ED185)
	void setCustomTintEnvelope(const RGBColor &color, UnsignedInt attackFrames, UnsignedInt decayFrames, UnsignedInt sustainFrames, float frequency, float amplitude);
	// RW 0x675749 / BFME2 0x67541E (Drawable::colorFlash): argument order in game.dat is (color, decayFrames, attackFrames, sustainAtPeak)
	void colorFlash(const RGBColor *color, UnsignedInt decayFrames = 0, UnsignedInt attackFrames = 0, UnsignedInt sustainAtPeak = 0);
	// RW 0x6757BB / BFME2 0x675490 (Drawable::colorTint)
	void colorTint(const RGBColor *color);
	// RW 0x678FA7 / BFME2 0x678C4F (Drawable::flashAsSelected)
	void flashAsSelected(const RGBColor *color = nullptr);
	// RW 0x672F9A / BFME2 0x672C74 (Drawable::getTintColor) & RW 0x672FAF / BFME2 0x672C89 (Drawable::getSelectionColor)
	const RGBColor *getTintColor(UnsignedInt logicFrame = 0) const;
	const RGBColor *getSelectionColor(UnsignedInt logicFrame = 0) const;
	// RW 0x46F4E7 .. 0x46F5A0 / BFME2 0x46F1F1 .. 0x46F2AA (RTS3DScene::Customized_Render): sums the active tint and selection flash for the shader
	bool evaluateRenderTint(UnsignedInt logicFrame, RGBColor &out) const;
	// RW 0x675D8B .. 0x675F8A / BFME2 0x675A60 .. 0x675C5F (Drawable::updateDrawable): steps the envelopes and reconciles m_tintStatus / m_prevTintStatus
	void updateTint();
	const TintEnvelope &colorTintEnvelope() const { return m_colorTintEnvelope; }
	const TintEnvelope &selectionFlashEnvelope() const { return m_selectionFlashEnvelope; }

private:
	void addEntry(DrawServices &services, const ThingTemplate::Nugget &nugget, std::vector<std::string> &problems);

	DrawServices &m_services;
	DrawableID m_id;
	ObjectID m_objectId;
	KindOfMaskType m_kindOf{};
	ModelConditionFlags m_flags;
	float m_scale = 1.0f;
	int m_variation = -1; ///< + 0x364
public:
	int variation() const { return m_variation; } ///< + 0x364 (RW 0x679FEA)
private:
	std::uint32_t m_houseColor = 0;
	std::uint32_t m_indicatorColor = 0; ///< RW +0x454 (Drawable::setIndicatorColor RW 0x674510: indicator color)
	int m_customKind = 0;                              // lane CAH-2
	std::uint32_t m_customColors[3] = { 0u, 0u, 0u };
	std::uint32_t m_customRevision = 0;
	unsigned m_changeCount = 0;
	bool m_drawsInMirror = false;
	unsigned m_constructionLogicFrame = ~0u; ///< RENDER-2: the logic frame the cached construction percents were refreshed in
	float m_geometryHeight = -1.0f;          ///< RENDER-2: RW 0xAD1920 of the template's shapes (computed on first use)
	std::vector<DrawEntry> m_entries;
	std::vector<std::unique_ptr<DrawableModule>> m_clientModules;
	std::vector<ModelConditionAudioLoopClientBehavior *> m_audioLoops; ///< those of m_clientModules the flush tells (RW 0x67449C)
	bool m_evaFirstSighted = false; // RW Drawable +0x114 bit 0x80; sighting consumer S-709 / Local 13
	bool m_evaEnemySighted = false; // RW Drawable +0x445
	bool m_evaEnemySightedAfterRespawn = false; // RW Drawable +0x446
	bool m_evaCastleMemberSighted = false; // RW Drawable +0x114 bit 0x40
	bool m_shroudRevealed = false; // transitioned from fullyObscured == true to false
	bool m_shadowsEnabled = true;   // RW Drawable +0x114 bit 2 (TOP20 #24)
	bool m_audible = true;          ///< lane PORT-21: Drawable + 0x44A, 1 until the object becomes INAUDIBLE (RW 0x674727 / 0x67896F)
	bool m_clientHidden = false; ///< lane CHAT-1
	bool m_phantomLook = false;  ///< lane FBSIM-1: drawn as a dozer's phantom structure (0.4 opacity)
	Fade m_fade; ///< lane PROJ-2
	StealthLook::State m_stealth;            ///< lane STEALTHLOOK-1: RW Drawable +0xB4 .. +0xD4, +0x164, +0x358, +0x43E
	bool m_cannotHideByStealth = false;      ///< lane STEALTHLOOK-1: RW +0x3AB, set at construction (GameClient/StealthLook.cpp)
	ObjectID m_stealthParentId = INVALID_ID; ///< lane STEALTHLOOK-1: the imitated parent's object (ObjectSnapshot::stealthParent)
	bool m_buildingEntries = false;                                     ///< lane BUILD-4: the constructor is making the draw modules
	std::vector<std::pair<std::string, bool>> m_pendingModuleRequests; ///< lane BUILD-4: script module requests made meanwhile
	float m_moveSpeed = 0.0f; ///< lane SMOOTH-3: the presented record's moveSpeed (the Distance animations' speed sync)
	DrawableScriptTarget m_scriptTarget; ///< lane FX-3
	// lane PERF-3: the angle whose rotation basis syncFromSnapshot last set and that basis; a render frame that presents the same angle (bit for bit) on that
	// basis skips setOrientation's trigonometry (its result would be these same values)
	const LogicSnapshot *m_syncSnapshot = nullptr; ///< lane PERF-3: setSyncedRecord
	UnsignedInt m_syncFrame = 0;
	const ObjectSnapshot *m_syncRec = nullptr;
	DrawableShroudLook m_shroudLook; ///< lane VISUAL-2
	UnsignedInt m_evaDamageFarFrame = 0; ///< lane EVADAMAGE-1: RW + 0x388
	bool m_zBasisCached = false;
	bool m_reactionMuted = false; ///< lane PERF-HD: see setReactionMuted
	float m_zCachedAngle = 0.0f;
	float m_zCachedBasis[9] = {};
	bool m_flagsDirty = false;           ///< lane ANIM-1: RW Drawable + 0x443
	bool m_hasWeaponTiming = false;      ///< lane ANIM-1: the last flush's weapon cycle (RW 0x4BEE31 reads the object's when the draw applies the flags)
	int m_weaponTimingFrames = 0;
	// LOCAL-3: RW Drawable +0x64 m_selectionFlashEnvelope, +0x68 m_colorTintEnvelope, +0x6C..+0x88 custom tint parameters, +0x118..+0x11C tint status
	TintEnvelope m_selectionFlashEnvelope;
	TintEnvelope m_colorTintEnvelope;
	RGBColor m_customTintColor{ -1.0f, -1.0f, -1.0f }; ///< RW 0x67A01E..0x67A028 (+0x6C..+0x74)
	UnsignedInt m_customTintAttackFrames = 0;          ///< +0x78
	UnsignedInt m_customTintDecayFrames = 0;           ///< +0x7C
	UnsignedInt m_customTintSustainFrames = 0;         ///< +0x80
	float m_customTintAmplitude = 0.0f;                ///< +0x84
	float m_customTintFrequency = 0.0f;                ///< +0x88
	UnsignedInt m_tintStatus = 0;                      ///< +0x118
	UnsignedInt m_prevTintStatus = 0;                  ///< +0x11C
	void setFadeHidden(bool hidden); ///< RW 0x6718FB (bumps the change count when it changes)
};
