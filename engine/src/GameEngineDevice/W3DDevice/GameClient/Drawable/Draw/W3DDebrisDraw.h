// OpenBFME. GPL-3.0.
//
// W3DDebrisDraw (issue #285, LOCAL-35; RotWK only: this binary's class, caveat S-001).
// The flying debris pieces of a destroyed object (GenericDebris in system.ini). The module data has no fields; the model
// name / colour and the three animation names are set through the DebrisDrawInterface (setModelName / setAnimNames) by the
// object that makes the debris (CreateDebris / OCL, not in this issue). doDrawModule advances the INITIAL -> FLYING -> FINAL
// animation state machine and picks the animation to play.
//
// TARGET FACTS (RotWK game.dat, read through tools/mod/rw.py dis):
// - name() RW 0xBDBBC8 ("W3DDebrisDraw"), factory RW 0x464B15; the draw class's own code RW 0x4B15E1 (doDrawModule),
//   0x4B1601 (reactToTransformChange / setModelName side). No fields in the module data (table RW 0x0).
// DONOR BFME2 1.06 (workspace/reference/open-bfme-2, W3DDebrisDraw*.cpp): ctor 0x000B1C41, doDrawModule 0x000B1A37,
//   reactToTransformChange 0x000B1A14, setModelName 0x000B1EEB, setAnimNames 0x000B207F, xfer (see below). The RW state
//   machine matches the donor.
//
// The state machine (donor doDrawModule, confirmed shape in RW 0x4B15E1): while not FINAL, a landed object (not above
// terrain) with m_frames > 3 goes FINAL; otherwise, while below FINAL, a completed animation advances the state. The
// animation changes only when the new state's animation differs from the playing one or the state changed. The final state
// plays its FX once and stops on the last frame when the final name was "STOP" (m_finalStop). The RenderObj itself and the
// debris object's destruction after the final animation are the renderer's / the drawable lifecycle's, not this class.

#pragma once

#include "Common/Module.h"

#include <cstdint>
#include <string>

class ModuleFactory;
class MultiIniFieldParse;

class W3DDebrisDrawModuleData : public ModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p); // RW table 0x0: no fields
};

// What one doDrawModule step decided (the renderer consumes this; there is no RenderObj here).
struct W3DDebrisDrawStep
{
	enum Anim
	{
		ANIM_INITIAL = 0,
		ANIM_FLYING = 1,
		ANIM_FINAL = 2,
		ANIM_COUNT = 3
	};
	enum Mode
	{
		MODE_MANUAL = 0,
		MODE_LOOP = 1,
		MODE_ONCE = 2
	};
	int anim = ANIM_INITIAL;   ///< which animation plays this frame
	bool animChanged = false;  ///< the animation must be (re)started
	int mode = MODE_ONCE;      ///< ANIM_INITIAL/ANIM_FINAL play once, ANIM_FLYING loops (RW TheAnimModes)
	bool finalFX = false;      ///< the FINAL state was entered this step (the FX list fires once)
	bool hasModel = false;     ///< a model name is set (else doDrawModule draws nothing)
};

class W3DDebrisDraw : public DrawableModule
{
public:
	OBFME_MODULE_XFER(W3DDebrisDraw)

	W3DDebrisDraw(Thing *thing, const W3DDebrisDrawModuleData *data);

	// DebrisDrawInterface: the debris maker (CreateDebris) sets the model and the three animations.
	void setModelName(const std::string &name, std::int32_t color, int shadowType);
	void setAnimNames(const std::string &initial, const std::string &flying, const std::string &final);

	// RW 0x4B15E1 doDrawModule: advance the state machine. `animationComplete` is the RenderObj's Is_Animation_Complete /
	// HLod answer; `isAboveTerrain` is the object's height test. The step is empty when no model name is set.
	W3DDebrisDrawStep advance(bool isAboveTerrain, bool animationComplete);

	int state() const { return m_state; }
	int frames() const { return m_frames; }
	bool finalStop() const { return m_finalStop; }
	const std::string &modelName() const { return m_modelName; }
	std::int32_t modelColor() const { return m_modelColor; }

	void crc(StateHasher &hasher) const override;
	static void registerClass(ModuleFactory &modules);

private:
	enum
	{
		INITIAL = 0,
		FLYING = 1,
		FINAL = 2,
		STATECOUNT = 3,
		MIN_FINAL_FRAMES = 3
	};
	const W3DDebrisDrawModuleData *m_data = nullptr;
	std::string m_modelName;    ///< setModelName
	std::int32_t m_modelColor = 0;
	std::string m_animInitial;  ///< setAnimNames
	std::string m_animFlying;
	std::string m_animFinal;
	int m_state = INITIAL;      ///< 0 INITIAL, 1 FLYING, 2 FINAL
	int m_frames = 0;
	bool m_finalStop = false;   ///< the final name was "STOP": freeze on the last frame
};
