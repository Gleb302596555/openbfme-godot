// OpenBFME. GPL-3.0.
//
// LiveFX (lane FX-2): the client effect player of a live game. It connects the logic's effect calls (GameLogic/FXEvents.h) and the drawables' state effects
// to FX-1's stack (GameClient/FXPlayback: FXList dispatcher, particle simulator) and is the FXPlaybackWorld that stack runs in: the map's terrain, the live
// objects and drawables (attachments, bones), the audio entry points. Client-only: it reads the logic, never writes it, and draws only the CLIENT random
// stream (FXPlayback's).
//
// What it plays, each with its source:
//   * logic events (installed as the GameLogic's FXEventSink, called at the moment of the retail call):
//       OBJECT_FX -> FXList::doFXObj(fx, primary, secondary) (RW 0x4B1B5A), POSITION_FX -> FXList::doFXPos(fx, pos, matrix or null) (RW 0x494615),
//       WEAPON_FIRE_FX -> the client half of RW 0x6CC915: no drawable on the shooter, no FX; then Drawable::handleWeaponFireFX (RW 0x671402 -> every model
//       draw's slot 0x58; DONOR ZH W3DModelDraw::handleWeaponFireFX: the slot's barrel list of the current model state, the barrel's fire FX bone, doFXPos at
//       the bone's world transform with the weapon speed and victim position), and when no draw module took it, doFXObj(fx, shooter, victim).
//   * EnteringStateFX of the model draws (RW 0x4BF222: doFXObj on the drawable's object), through DrawableFXHost.
//   * ParticleSysBone systems of the current model condition state and animation state of every model draw (DONOR ZH W3DModelDraw::
//     recalcBonesForClientParticleSystems / stopClientParticleSystems: on a state change the old systems are destroyed, each entry's system is created at the
//     bone's model-space position (drawable scale applied) turned by the bone's Z rotation and attached to the drawable). Construction dust, fires and smoke
//     of damaged buildings and projectile trails are this path.
//   * sounds of Sound nuggets through AudioApi::playSoundAtPosition.
//   * TransitionDamageFX's attached particle systems (lane FX-4: ATTACH / DESTROY_PARTICLE_SYSTEM events, kept per (object, slot) as the module keeps them).
//   * the FXEvent entries of the current ANIMATION state of every model draw (RW 0x4BCE68, called from W3DModelDraw::doDrawModule RW 0x4C72BE): per entry
//     {FrameStep +0, Frame +4, FrameStop +8, FireWhenSkipped +0xC, Bone +0x10, FXList +0x14} against the animation's integer frame (+0x114) and previous
//     frame (+0x118, direction +0x124); a firing entry plays through RW 0x4BABBA: no bone -> doFXObj(fx, the drawable's object, null), a bone ->
//     doFXPos(fx, bone position, bone matrix, 0, null). INFERENCE (S-682): the entries are evaluated once per change of the integer frame (RW evaluates
//     every draw at its 30 Hz cap; this renderer runs faster), the FireWhenSkipped single-frame test is RW 0x4B337B (mode and direction); NOT PORTED (S-683): the FXEvent entries of MODEL CONDITION states and the FXEvents whose FXList is routed to the timed particle
//     list (FXList byte +8, the second loop of RW 0x4BCE68).
//
// INFERENCE / not ported (stop S-682, reported by stops()):
//   * bone positions come from the model's bind pose (ZH asks the live render object's current pose; FollowBone systems are not moved with the animation);
//   * ParticleSysBone FXTrigger / Persist / PersistID / HouseColor are ignored; OnlyIfOnWater systems are not created (no water query), OnlyIfOnLand ones are;
//   * the shooter's stealth (RW 0x694C0D with a NULL viewer, RW 0x6CCB91; lane STEALTH-1) is read when the effect plays, not at the shot;
//   * the muzzle flash and recoil of handleWeaponFireFX are not drawn;
//   * isWater is false everywhere; the shroud is clear (VIS-1 owns it).
// Data\INI\ParticleSystem.ini (329 ZH-format `ParticleSystem` blocks) is not loaded, as in retail (lane FX-3, retires stop S-684). TARGET FACTS (RotWK
// game.dat, S-001 caveat): GameEngine::init never names TheParticleSystemManager (the string is not in the image; the legend entry is never requested,
// spec ini-and-object-model 3.5); no INI block registration names `ParticleSystem` (the only particle block is `FXParticleSystem` -> RW 0x5FC7DB, record
// RW 0xD9E440); the ZH ParticleSystem fields VelocityType / VolumeType / SpinRate exist nowhere in the image; ParticleSysBone (RW 0x73AECB) and the FXList
// ParticleSystem nugget resolve names in TheFXParticleSystemManager alone. 317 of the file's 329 names are also FXParticleSystem templates (those play);
// the 12 defined only there (InfantryDustTrails, the RainOfFire* set, ...) play nothing in retail and are listed in Stats::unresolvedParticleSystems.

#pragma once

#include "GameClient/BuffDraw.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/ParticleBoneFollow.h"
#include "GameEngineDevice/W3DDevice/GameClient/TerrainScorches.h"
#include "GameLogic/FXEvents.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

class LiveGame;
class Object;
struct FireSetting;

class LiveFX : public FXEventSink, public FXPlaybackWorld, public DrawableFXHost
{
public:
	// installs itself as the logic's sink, the drawables' FX host and the playback's world; the destructor uninstalls. Both must outlive it.
	LiveFX(LiveGame &game, FXPlayback &playback, int effectsLod = 4);
	~LiveFX() override;
	LiveFX(const LiveFX &) = delete;
	LiveFX &operator=(const LiveFX &) = delete;

	// FXEventSink. SMOOTH-1 (merge with FX-2): called on the simulation owner (the logic worker) at the moment of the retail call; it only CAPTURES what the
	// effect will read of its objects (position, transform, conditions, owner, relationship, kinds, radius, drawable, the list's object filters) and queues
	// it; flushPending plays the queue on the render side at a worker-idle point, in call order
	void setEffectsLod(int lod);
	int effectsLod() const { return m_effectsLod; }
	void ensureSystems();

	void onFXEvent(const FXEvent &event) override;
	// plays the queued logic effects and entering-state effects, in order (the render side, while the logic is idle)
	void flushPending();
	size_t pendingEvents() const { return m_pending.size() + m_pendingStates.size(); }
	// lane SAVE-2: drops the queued effects unplayed (a loaded save's catch-up: the frames before the saved one are not shown); returns how many
	size_t discardPending()
	{
		const size_t n = pendingEvents();
		m_pending.clear();
		m_pendingStates.clear();
		return n;
	}
	// DrawableFXHost
	bool enteringStateFX(DrawableID id, const std::string &fx) override;

	// once per client frame, after the drawables' poses were synced: the ParticleSysBone systems follow every model draw's current states; the systems of
	// drawables that are gone are destroyed
	void updateAttachedSystems();

	// FXPlaybackWorld
	std::uint32_t logicFrame() const override;
	float groundHeight(float x, float y) override;
	bool isWater(float, float) override { return false; }
	// lane VISUAL-2: the shroud as the presented snapshot shows it to the local player (the view the drawn shroud is made of)
	int shroudStatusAt(const Coord3D &pos) override;
	bool objectShroudedForLocalPlayer(const FXObject &obj) override;
	int mapWeather() const override { return m_mapWeather; } // lane WEATHER-1
	const std::set<std::pair<int, int>> *burningCells() override;
	int maxCellsBurning() const override;
	FXParticleSystem::ParticleAttachInfo attachedDrawable(std::uint32_t drawableId, const std::string &boneName) override;
	FXParticleSystem::ParticleAttachInfo attachedObject(std::uint32_t objectId, const std::string &boneName, int localPlayerIndex) override;
	bool boneWorldMatrix(const FXObject &obj, const std::string &boneName, Matrix3D &out) override;
	std::vector<FXBoneTransform> boneWorldTransforms(const FXObject &obj, const std::string &boneName, int startIndex, int maxBones) override;
	bool playSound(const std::string &eventName, const Coord3D *pos, int playerIndex) override;
	// lane SPELL-4: the BuffNugget's buffs (GameClient/BuffDraw.h): the template's W3DBuffDraw model on the object's drawable
	bool turnOnBuff(const FXObject &obj, int type, const std::string &thingTemplate, int lifeFrames) override;
	bool turnOffBuff(const FXObject &obj, int type) override;
	// LOCAL-3: TintDrawableFXNugget::doFXObj (RW 0x5E0108 / BFME2 0x5DFE1E -> Drawable::setCustomTintEnvelope RW 0x671A73 + m_tintStatus |= 0x20)
	bool tintDrawable(const FXObject &obj, const RGBColor &color, unsigned attackFrames, unsigned decayFrames, unsigned sustainFrames, float frequency,
		float amplitude) override;
	// lane FX-4: the ViewShake calls the effects made (W3DView::shake RW 0x486D86 with the client stream's angle), for the view: the HUD takes them before its
	// camera's client frame (TacticalCamera::shake). Bounded: without a view taking them, the oldest are dropped
	struct ViewShake
	{
		Coord3D epicenter;
		int type = 0;
		float angle = 0.0f;
	};
	bool viewShake(const Coord3D &epicenter, int type, float angle) override;
	std::vector<ViewShake> takeViewShakes()
	{
		std::vector<ViewShake> out;
		out.swap(m_viewShakes);
		return out;
	}
	// the DynamicDecal nuggets' decals (RW 0x5E05AB, S-5205): kept with the client frame of the call, for the view; the ones past their lifetime are pruned
	// at the next call or prune. Bounded: the oldest are dropped past 128
	struct LiveDecal
	{
		FXDecalDesc desc;
		std::uint32_t born = 0; ///< FXPlayback::clientFrame() at the call
	};
	bool createDecal(const FXDecalDesc &decal) override;
	// TOP20 "Fire, scorches": the terrain's scorch marks, one store for the three sources RotWK has (TerrainScorches.h):
	//   * the TerrainScorch nugget (W3DGameClient::addScorch RW 0x44C047 -> RW 0x46765B), this override;
	//   * the map's Scorch objects, added when the live game is made (W3DTerrainVisual::load RW 0x491E36 .. 0x491EC3: every map object with the scorch
	//     flag, + 0x44 bit 3 = a scorchType INT, RW 0x70F5A9: its location, the REAL "objectRadius" (NameKey RW 0xDA2E84, string RW 0xC1F840) and the
	//     INT "scorchType" (RW 0xDA2E9C, RW 0xC1F874)), in map order; a Scorch object without a REAL objectRadius is an error (stats().scorchErrors);
	//   * the burning terrain, once per client frame (clientFrameUpdate -> updateFireScorches, RW 0x49829C).
	bool addScorch(const Coord3D &pos, float radius, int type) override;
	void clientFrameUpdate() override;
	// RW 0x49829C (the fire manager's update, W3DFire vtable RW 0xBDE280 slot 10): with the Fire block's EnableScorches (the manager's copy + 0x2C of the
	// active record + 0x20, RW 0x70C5F8) and the effects LOD (TheGameLODManager + 0x1778) above 1, once the client frame passes the stored one (+ 0x64,
	// zero after the manager's reset RW 0x497F5A): the stored frame becomes the frame plus (int) ScorchFrequency (+ 0x30, GameClientRandomVariable
	// RW 0x6D343B, _ftol2 RW 0xA3CFA4); then for every burning cell of TheFireLogicSystem (RW 0xDE46A8 + 0x84, its world x / y at node + 0x18 / + 0x1C, in
	// set order) a client draw Real(0, 1) (RW 0x498352, line 0xA2) below ScorchIntensity (+ 0x48) adds a scorch, with a terrain (RW 0xDC78EC), at
	// (x + Real(-10, 10), y + Real(-10, 10), ground(x, y) + 1) - the y offset is drawn first (line 0xA7, RW 0x4983BC), then the x offset (line 0xA6,
	// RW 0x4983E5) - of ScorchSize (+ 0x3C, drawn after the offsets, RW 0x49840C) and type 7, or 8 when GlobalData + 0x138 (the map weather) is 1
	// (RW 0x4983F6). INFERENCE: the frame update runs after the client frame moved on, once per frame (the subsystem's caller was not traced).
	void updateFireScorches();
	// the body of RW 0x49829C past the effect systems, over explicit inputs (tests): returns the scorches added
	static int fireScorchUpdate(const FireSetting &fire, int effectsLod, std::uint32_t clientFrame, std::uint32_t &nextScorchFrame,
		const std::set<std::pair<int, int>> &burningCells, W3DDrawRandom &rng, int mapWeather, const std::function<float(float, float)> &groundHeight,
		TerrainScorchBuffer &scorches);
	TerrainScorchBuffer &scorches() { return m_scorches; }
	const TerrainScorchBuffer &scorches() const { return m_scorches; }
	void pruneDecals();
	const std::vector<LiveDecal> &decals() const { return m_decals; }
	std::uint32_t clientFrame() const;
	ClientBuffs &buffs() { return m_buffs; }
	const ClientBuffs &buffs() const { return m_buffs; }
	// the buff templates refused (BuffLogic::addBuff RW 0x707FFF: "No draw module specified for Buff!", "Buff <name> requires a W3DBuffDrawModule.")
	const std::set<std::string> &buffErrors() const { return m_buffErrors; }

	struct Stats
	{
		std::map<std::string, unsigned long long> played;   ///< FXLists played, per site (logic sites, "EnteringStateFX")
		std::map<std::string, unsigned long long> skipped;  ///< calls not played, per reason ("no drawable", "suspended", ...)
		std::set<std::string> missingFXLists;               ///< names the FXList store does not hold (retail rejects them at parse time)
		std::set<std::string> unresolvedParticleSystems;    ///< ParticleSysBone systems the FXParticleSystem store does not hold (RW 0x73AECB: NULL, not played)
		unsigned long long fireFXAtBone = 0;                ///< fire FX played at a barrel's fire FX bone
		unsigned long long fireFXOnObject = 0;              ///< fire FX played through doFXObj (no draw module took it)
		unsigned long long attachedCreated = 0, attachedDestroyed = 0, attachedLive = 0;
		// lane PORT-50 (RW 0x4B6D5F): the follow's per-frame calls - transforms applied (RW 0x5F2F55 + 0x5F32A7), records whose
		// system or bone was gone, and the by-index branch's starts / stops (RW 0x5F2EE6 / 0x5F2EDE)
		unsigned long long bonesFollowed = 0, bonesMissed = 0, bonesStarted = 0, bonesStopped = 0;
		unsigned long long boneMisses = 0;                  ///< ParticleSysBone bones the model lacks (RW 0x4C6514: the system plays at the drawable's origin)
		std::set<std::string> missingBones;                 ///< "MODEL:bone" of the bone misses of every path (at most 200): retail data, played as retail does
		unsigned long long sounds = 0;
		unsigned long long frameEventsFired = 0;           ///< animation state FXEvent entries fired (RW 0x4BCE68)
		unsigned long long uncapturedFilters = 0;          ///< SMOOTH-1: an object filter asked at play time that the capture at the call did not evaluate (fails)
		unsigned long long transitionSystemsCreated = 0, transitionSystemsDestroyed = 0; ///< lane FX-4: TransitionDamageFX's attached systems
		unsigned long long viewShakes = 0, viewShakesDropped = 0; ///< lane FX-4: ViewShake calls queued for the view / dropped (no view took them)
		unsigned long long decals = 0, decalsDropped = 0;         ///< DynamicDecal calls kept for the view / dropped past the bound
		unsigned long long scorchesFX = 0, scorchesMap = 0, scorchesFire = 0; ///< addScorch calls per source (TerrainScorchBuffer)
		std::vector<std::string> scorchErrors;                    ///< Scorch map objects that could not be added (no REAL objectRadius)
	};
	const Stats &stats() const { return m_stats; }
	// RW 0x4B337B, the single-frame test of a FireWhenSkipped FXEvent (mode: W3D_ANIM_MODE_*, direction +1 / -1)
	static bool framePassed(int prev, int cur, int frame, int mode, int direction);
	// the stop lines of the live effects (S-680 .. S-686: the logic side's FXEventLog::stops() and this player's), for the report
	static std::vector<std::string> stops();

private:
	struct Attached
	{
		std::vector<const void *> states;                     ///< per draw entry: model state, animation state (compared, never ordered)
		std::vector<FXParticleSystem::ParticleSystemID> systems;
		// lane PORT-50: retail's tracker list (the draw module's +0x58, node +0xC -> the record RW 0x4B6CDD copies), one node
		// per created system, walked once per client frame by ParticleBoneFollow::followFrame (RW 0x4B6D5F)
		std::vector<ParticleBoneFollow::Record> records;
	};
	Object *object(ObjectID id) const;
	struct Captured; // the captured view of an event's object (LiveFX.cpp)
	struct Pending
	{
		FXEvent event;
		std::shared_ptr<Captured> primary, secondary;
	};
	void play(const Pending &p);
	void playTransitionSystem(const FXEvent &e); // lane FX-4
	bool playEnteringStateFX(DrawableID id, const std::string &fx);
	std::vector<Pending> m_pending;                                     ///< filled on the simulation owner, drained at idle points
	std::vector<std::pair<DrawableID, std::string>> m_pendingStates;    ///< EnteringStateFX of drawables (render side), played at idle points
	bool modelBone(const RenderObjPrototype *proto, const std::string &bone, float scale, Matrix3D &out, bool subObjects = true);
	bool handleWeaponFireFX(const Drawable &shooter, const FXEvent &e, const FXList *fx);
	void destroySystems(Attached &a);
	void createSystems(Drawable &d, Attached &a);
	// lane PORT-50: RW 0x4C7795's gated call (RW 0x4B6D5F with 0) for one drawable: every ParticleSysBone system it created
	// follows the animated bone of the pose it was created for
	void followAnimatedBones(Drawable &d, Attached &a);
	struct FrameEventState
	{
		const void *animState = nullptr; ///< the animation state the frames belong to
		int lastFrame = -1;              ///< the integer frame the events were last evaluated for
	};
	using FrameEventMap = std::map<std::pair<DrawableID, size_t>, FrameEventState>;
	// lane PERF-3: `pos` walks m_frameEvents alongside updateAttachedSystems' walk over the drawables in id order (both are in key order): each entry's
	// lookup is a step of the walk instead of a search of the map
	void frameEvents(Drawable &d, size_t entryIndex, const DrawEntry &e, FrameEventMap::iterator &pos);
	void fireFrameEvent(Drawable &d, const FXEventInfo &ev, const RenderObjPrototype *model);

	FXParticleSystem::ParticleSystemID m_terrainFireSystem = FXParticleSystem::INVALID_PARTICLE_SYSTEM_ID;
	FXParticleSystem::ParticleSystemID m_terrainSmokeSystem = FXParticleSystem::INVALID_PARTICLE_SYSTEM_ID;

	LiveGame &m_game;
	int m_effectsLod = 4;
	int m_mapWeather = 0; ///< lane WEATHER-1: the map's WorldInfo weather (GameLogicSettings::snowy), read at construction (the logic is idle then)
	FXPlayback &m_playback;
	ClientBuffs m_buffs;                 // lane SPELL-4
	std::vector<ViewShake> m_viewShakes; // lane FX-4
	std::vector<LiveDecal> m_decals;     // S-5205
	TerrainScorchBuffer m_scorches;      // BaseHeightMap's scorch list (RW 0x46765B)
	std::uint32_t m_nextScorchFrame = 0; // the fire manager's + 0x64 (RW 0x498310)
	std::set<std::string> m_buffErrors;  // lane SPELL-4
	struct BuffTemplate
	{
		std::string model;
		bool preDraw = false;
	};
	std::map<std::string, BuffTemplate> m_buffModels; // lane SPELL-4: template -> W3DBuffDraw (ModelName, PreDraw) (looked up only)
	Stats m_stats;
	std::map<DrawableID, Attached> m_attached;
	std::map<std::pair<ObjectID, std::uint32_t>, FXParticleSystem::ParticleSystemID> m_transitionSystems; // lane FX-4: (object, slot) -> system; looked up only
	FrameEventMap m_frameEvents; // looked up only (and pruned), never ordered by pointer
	W3DDrawFrame m_eventFrame;   // lane PERF-3: frameEvents' frame of the entry (its storage reused)
	std::vector<const void *> m_states; // lane PERF-3: updateAttachedSystems' states of the drawable (its storage reused)
	std::vector<ParticleBoneFollow::Pose> m_bonePoses; // lane PORT-50: followAnimatedBones' poses of the drawable (reused)
	ParticleBoneFollow::Counters m_boneCounters;       // lane PORT-50: one frame's follow counters (reused)
	std::map<std::pair<const RenderObjPrototype *, std::string>, std::pair<bool, Matrix3D>> m_boneCache; // looked up only, never iterated
};
