// OpenBFME. GPL-3.0.
//
// Module data of W3DTreeDraw, W3DPropDraw and W3DFloorDraw (lane MAPOBJ-1): the three static draw modules the RotWK map objects
// use for trees, shrubs, props and building floors ("bibs"). ZH has W3DTreeDraw only; BFME adds PropDraw and FloorDraw. Only the
// DATA and the static draw (which model, which texture) are ported here; the dynamic tree behaviour is a registered stop.
//
// Sources, in PLAN rule 1 priority order. Facts are tagged TARGET (RotWK game.dat, caveat S-001; "RW 0x..." addresses), DONOR
// (Open-BFME-2 matched files, BFME2 1.06, and the Zero Hour source) or INFERENCE.
//
//   TARGET RW 0xBE2E80  the W3DTreeDraw FieldParse table (24 rows), parse functions RW 0x42EE5E (string), 0x73A429 (duration in
//                       logic frames), 0x42ED00 (real), 0x73A302 (FXList by name), 0x42E558 (bool), 0x42EEFA (percent), 0x42ED1C
//                       (positive non-zero real), 0x42ECB2 (unsigned int); registry entry W3DTreeDraw: createData RW 0x464402,
//                       constructor RW 0x4CE3A8 (object size 0x64), table add RW 0xBE2E80, vtable RW 0xBE2C88.
//   TARGET RW 0x4CE3A8  the constructor defaults: MoveOutwardTime / MoveInwardTime 1 frame, MoveOutwardDistanceFactor 1.0,
//                       DarkeningFactor 0, InitialVelocityPercent 0.2 (RW 0xBDAD78), InitialAccelPercent 0.01 (RW 0xBE5600),
//                       BounceVelocityPercent 0.3 (RW 0xBE29D4), MinimumToppleSpeed 0.5 (RW 0xBD869C), KillWhenFinishedToppling
//                       true, DoTopple false, SinkTime and MorphTime 10 * [RW 0xD9F608] frames (that global holds 5 in the image,
//                       the logic frame rate), SinkDistance 20.0 (RW 0xBDBC6C), TaintedTree false, FadeRate 5, FadeTarget 0x69,
//                       FadeDistance 40.0 (RW 0xBDD28C).
//   TARGET RW 0xBE3340  the W3DPropDraw table (2 rows: ModelName, DistanceFog); constructor RW 0x4CE704 (ModelName empty,
//                       DistanceFog true), createData RW 0x464514, object size 0x10.
//   TARGET RW 0xBE3548  the W3DFloorDraw table (6 rows) that follows the prop table (registry tables [0xBE3340, 0xBE3548]);
//                       constructor RW 0x4CEE74 (a W3DPropDraw data, then the weather map at +0x10, the three flags at +0x1C..0x1E
//                       and FloorFadeRateOnObjectDeath 0.0 at +0x20, the hide-condition vector at +0x24), createData RW 0x46459D.
//   TARGET RW 0x4CF066  WeatherTexture = WEATHER NAME: parseIndexList over the list at RW 0xDA3A84 (NORMAL, SNOWY) then a string;
//                       RW 0x4CEF06  HideIfModelConditions = <model condition flags>: one more ModelConditionFlags is APPENDED
//                       to the vector at +0x24 per line (the retail INI repeats the line, once per condition).
//   DONOR  Open-BFME-2 GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.cpp,
//          W3DPropDraw.cpp, W3DFloorDrawCtor.cpp (member layout and names), ZH W3DTreeDraw.cpp.
//
//   TARGET RW 0x467B4D  the terrain visual's floor forwarder (BaseHeightMapRenderObjClass; the receiver is at RW 0xDC78EC + 0x3860):
//                       with no floor list it returns 0 (RET 0x14), else it tail-jumps to RW 0x4E592B with the five arguments the
//                       module hands over - the object (RW 0x4CEAB7: Drawable + 0x100), the module data's ModelName (+8), the weather
//                       texture (RW 0x4CEA1C), ForceToBack (+0x1D) and StaticModelLODMode (+0x1C). RW 0x4E592B: no record when the floor
//                       list holds 0x1C2 (450) records (RW 0x4E5953), when the model does not load (RW 0x4E5960, the W3D asset manager),
//                       when the list's enabled byte (+0x20) is clear (RW 0x4E5968) or when an existing record for that object and name
//                       is not reusable (RW 0x4E5886 / RW 0x4E3D33); otherwise RW 0x4E4000 refreshes that record or RW 0x4E59BA makes one.
//                       RW 0x4E580B (reached from RW 0x46AF77): the record's drawable (+0x2C) is cleared and its slot freed (+0x9C = 1).
//   TARGET RW 0x467AD4  the terrain visual's prop forwarder: with no prop list (RW 0xDC78EC + 0x3858) it does nothing (RET 0x20), else
//                       RW 0x4EE5B8 with 8 arguments. RW 0x4EE5B8: no record without the global switch (RW 0xDE4364 + 0x1C, RW 0x4EE5C3),
//                       at 0xFA0 (4000) records (RW 0x4EE5D0) or with the list's enabled byte (+0x2EE09) clear (RW 0x4EE5E0); a record
//                       keyed by the object (RW 0x4065D4) is reused, else RW 0x4EE1E3 makes one.
//
// What is NOT retail knowledge (stops, docs/STOPS.md):
//   S-111  the dynamic tree behaviour (push-aside, topple, sink, morph, fade) and the FXList / tree-template names are stored but
//          not run; the FXList registry is not ported, so a name retail would reject is accepted here.
//   S-7904 the W3D asset layer the three modules above end in: the render-object loader RW 0x536E49 / RW 0x536310 (name, scale and
//          an option block -> a render object) and the animation lookup RW 0x54C8B6 (a name -> an animation handle). The port keeps
//          names and hands them to Godot; the sub-model attachment those loaders feed is S-7701 (RW 0x4B7384).

#pragma once

#include "Common/INI.h"
#include "Common/Module.h"
#include "Common/ModelState.h"

#include <string>
#include <utility>
#include <vector>

// ZH W3DTreeDrawModuleData with the BFME2 / RotWK additions (names kept from the BFME2 donor, INI names in the comments).
class W3DTreeDrawModuleData : public ModuleData
{
public:
	W3DTreeDrawModuleData();
	~W3DTreeDrawModuleData() override = default;
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_modelName;       ///< ModelName
	std::string m_textureName;     ///< TextureName
	unsigned m_framesToMoveOutward = 1; ///< MoveOutwardTime (ms in the INI, logic frames here)
	unsigned m_framesToMoveInward = 1;  ///< MoveInwardTime
	float m_maxOutwardMovement = 1.0f;  ///< MoveOutwardDistanceFactor
	float m_darkening = 0.0f;           ///< DarkeningFactor
	std::string m_toppleFX;        ///< ToppleFX (FXList name; S-111)
	std::string m_bounceFX;        ///< BounceFX
	std::string m_stumpName;       ///< StumpName
	float m_initialVelocityPercent = 0.2f;   ///< InitialVelocityPercent
	float m_initialAccelPercent = 0.01f;     ///< InitialAccelPercent
	float m_bounceVelocityPercent = 0.3f;    ///< BounceVelocityPercent
	float m_minimumToppleSpeed = 0.5f;       ///< MinimumToppleSpeed
	bool m_killWhenToppled = true;           ///< KillWhenFinishedToppling
	bool m_doTopple = false;                 ///< DoTopple
	unsigned m_sinkFrames = 50;              ///< SinkTime
	float m_sinkDistance = 20.0f;            ///< SinkDistance
	std::string m_morphTree;                 ///< MorphTree
	unsigned m_morphTime = 50;               ///< MorphTime
	std::string m_morphFX;                   ///< MorphFX
	bool m_taintedTree = false;              ///< TaintedTree
	unsigned m_fadeRate = 5;                 ///< FadeRate
	unsigned m_fadeTarget = 0x69;            ///< FadeTarget
	float m_fadeDistance = 40.0f;            ///< FadeDistance
};

class W3DPropDrawModuleData : public ModuleData
{
public:
	W3DPropDrawModuleData() = default;
	~W3DPropDrawModuleData() override = default; // RW 0x004CE715
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_modelName;  ///< ModelName
	bool m_distanceFog = true; ///< DistanceFog (constructor default 1, RW 0x4CE710)
};

class W3DFloorDrawModuleData : public W3DPropDrawModuleData
{
public:
	W3DFloorDrawModuleData() = default;
	static void buildFieldParse(MultiIniFieldParse &p);

	std::vector<std::pair<int, std::string>> m_weatherTextures; ///< WeatherTexture = WEATHER (0 NORMAL, 1 SNOWY) NAME, in file order
	std::vector<ModelConditionFlags> m_hideIfModelConditions;   ///< HideIfModelConditions, one set per line
	bool m_staticModelLODMode = false;  ///< StaticModelLODMode
	bool m_forceToBack = false;         ///< ForceToBack
	bool m_startHidden = false;         ///< StartHidden
	float m_floorFadeRateOnObjectDeath = 0.0f; ///< FloorFadeRateOnObjectDeath
};

namespace W3DTreeDrawTables
{
const FieldParse *tree();      ///< RW 0xBE2E80, 24 rows
const FieldParse *prop();      ///< RW 0xBE3340, 2 rows
const FieldParse *floor();     ///< RW 0xBE3548, 6 rows
const char *const *weatherNames(); ///< RW 0xDA3A84: NORMAL, SNOWY (NULL terminated)
} // namespace W3DTreeDrawTables

class Xfer;

// What one static draw hands the terrain visual's record list, and whether that record is there now. The port draws a map-placed
// prop / floor as a static model of its own (GameClient/Drawable.h DrawEntry, GameClient/FloorFade.h record); this is the module
// state that decides when such a record is made and with which fields, as the module on RW 0xBE3378 / RW 0xBE34xx does.
struct W3DPropDrawRecord
{
	bool added = false;              ///< a record for this module exists in the prop list (RW 0x467AD4 -> RW 0x4EE5B8)
	std::string modelName;           ///< the module data's ModelName (+8), the first argument RW 0x4CE7F9 pushes
	bool distanceFog = true;         ///< the module data's DistanceFog (+0xC), RW 0x4CE7F2
	float x = 0.0f, y = 0.0f;        ///< the position the record was made at (RW 0x4CE7B6 / RW 0x4CE7CE read it from the drawable)
};

struct W3DFloorDrawRecord
{
	// RW 0x004CE9B6: the floor's destructor clears its record (RW 0x4CE9D6..0x4CE9FE: the data's ModelName and the drawable go to
	// RW 0x46AF77 -> RW 0x4E580B) and then the prop base's destructor runs; the port's record lives in this object, so it goes with it.
	bool added = false;              ///< a record for this module exists in the floor list (RW 0x467B4D -> RW 0x4E592B)
	std::string modelName;           ///< the module data's ModelName (+8): RW 0x4CEAB1 pushes it
	std::string weatherTexture;      ///< RW 0x4CEA1C for the map's weather ("" when the module has no entry for it: the model keeps its own)
	bool forceToBack = false;        ///< the module data's ForceToBack (+0x1D), RW 0x4CEAA9
	bool staticModelLODMode = false; ///< the module data's StaticModelLODMode (+0x1C), RW 0x4CEAA0
};

// Runtime state of W3DPropDraw (RW 0x004CE776 ctor, RW 0x004CE6AE GetSnapshotName, RW 0x004CE6BF getModuleNameKey,
// RW 0x004CE75C getModelName, RW 0x004CE793 reactToTransformChange, RW 0x004B1B3F xfer, RW 0x004CE84C ModuleData deleting dtor).
class W3DPropDrawState
{
public:
	explicit W3DPropDrawState(const W3DPropDrawModuleData *data = nullptr) : m_data(data) {}

	// RW 0x004CE75C: returns m_data->m_modelName
	std::string getModelName() const { return m_data ? m_data->m_modelName : std::string(); }
	bool distanceFog() const { return m_data ? m_data->m_distanceFog : true; }

	// RW 0x004CE793: one-shot prop registration when +0x0C is clear and the object is not at the origin (RW 0x4CE7B6 / RW 0x4CE7CE:
	// a position of exactly (0, 0) never gets a record, so the prop is never drawn). The record is made at that first position and is
	// NOT moved afterwards: nothing in RW 0x4CE793 or RW 0x4EE5B8 re-reads the drawable's transform.
	bool reactToTransformChange(float posX, float posY);
	bool propCreated() const { return m_propCreated; }
	const W3DPropDrawRecord &record() const { return m_record; }

	// RW 0x004B1B3F (DONOR BFME2 0x000B221A, W3DPropDrawXfer.cpp): version(1, 1)
	void xfer(Xfer &x);

protected:
	const W3DPropDrawModuleData *m_data = nullptr;
	bool m_propCreated = false; ///< +0x0C
	W3DPropDrawRecord m_record;
};

// Runtime state of W3DFloorDraw (RW 0x004CE91B ctor, RW 0x004CE95F GetSnapshotName, RW 0x004CE971 getModuleNameKey,
// RW 0x004CE868 xfer, RW 0x004CEA6F reactToTransformChange, RW 0x004CEB80 replaceModelConditionState,
// RW 0x004CEBCA doDrawModule, RW 0x004CEC85 scalar deleting dtor, RW 0x004CECA1 setSubObjectHidden,
// RW 0x004CECB5 setHidden, RW 0x004CF013 ModuleData scalar deleting dtor).
class W3DFloorDrawState : public W3DPropDrawState
{
public:
	explicit W3DFloorDrawState(const W3DFloorDrawModuleData *data = nullptr);

	// RW 0x004CEA1C: resolves texture name for weather index (0 NORMAL, 1 SNOWY) from m_weatherTextures (+0x10); the calls come from
	// RW 0x4CEA46 RW 0xDE4364 + 0x138, the weather the map is in, and no matching entry leaves the result empty
	std::string resolveWeatherTexture(int weather) const;

	// RW 0x004CEA6F: the first transform change adds the floor record, but only while +0x14 is clear. RW 0x4CE950..0x4CE955 seeds
	// +0x14 from StartHidden (data +0x1E), so a StartHidden floor is NOT added here at all - it enters the list only through the
	// refresh below. `weather` is the map's weather (RW 0x4CEA1C reads it from RW 0xDE4364 itself). True when a record was handed over.
	bool reactToTransformChange(int weather = 0);
	bool registered() const { return m_registered; }                  ///< +0x14
	const W3DFloorDrawRecord &record() const { return m_floorRecord; } ///< the terrain visual's record for this floor
	bool recordAdded() const { return m_floorRecord.added; }
	const std::string &activeWeatherTexture() const { return m_floorRecord.weatherTexture; }

	// RW 0x004CEB80: tests m_hideIfModelConditions (+0x24) through RW 0x4CE8CA ((flags & mask) == mask over 0x13 dwords) into +0x17
	// and, only when +0x17 changed (RW 0x4CEBB3), refreshes the record (RW 0x4CEADF). An empty vector returns before any change.
	bool replaceModelConditionState(const ModelConditionFlags &flags);

	// RW 0x004CECA1: sets +0x15 and refreshes the record when the value changed (RW 0x4CECA8 / RW 0x4CECAD)
	void setSubObjectHidden(bool hidden);
	// RW 0x004CECB5: sets +0x16 and refreshes the record when the value changed (RW 0x4CECBC / RW 0x4CECC1)
	void setHidden(bool hidden);

	bool hiddenBySubObject() const { return m_hiddenBySubObject; }
	bool hiddenByModule() const { return m_hiddenByModule; }
	bool hiddenByModelCondition() const { return m_hiddenByModelCondition; }
	// RW 0x004CEADF: the record is drawn iff all three hide flags (+0x15, +0x16, +0x17) are false
	bool isFloorVisible() const { return !m_hiddenBySubObject && !m_hiddenByModule && !m_hiddenByModelCondition; }

	// RW 0x004CEADF: drops the record (RW 0x46AF77 -> RW 0x4E580B clears the drawable) and hands it over again when the floor is
	// visible (RW 0x467B4D, re-resolving the weather texture through RW 0x4CEA1C). Returns true when a record exists afterwards.
	bool refreshRecord();

	// RW 0x004CE868 (DONOR BFME2 0x000CF074): version(1..3), calls W3DPropDraw::xfer, v>=2 xfers +0x16, v>=3 xfers +0x17 and +0x15
	void xfer(Xfer &x);

private:
	const W3DFloorDrawModuleData *m_floorData = nullptr;
	bool m_registered = false;             ///< +0x14: seeded from StartHidden, then set by the first transform change
	bool m_hiddenBySubObject = false;      ///< +0x15
	bool m_hiddenByModule = false;         ///< +0x16
	bool m_hiddenByModelCondition = false; ///< +0x17
	int m_weather = 0;                     ///< the map weather the last hand-over resolved the texture for
	W3DFloorDrawRecord m_floorRecord;
};
