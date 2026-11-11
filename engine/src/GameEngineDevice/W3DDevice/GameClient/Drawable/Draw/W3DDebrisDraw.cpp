// OpenBFME. GPL-3.0. See GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.h (issue #285, LOCAL-35).
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.h"

#include "Common/AsciiString.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/SnapshotXfer.h"

#include <memory>
#include <stdexcept>

namespace
{
const FieldParse kEmptyFields[] = { { nullptr, nullptr, nullptr, 0 } };
} // namespace

void W3DDebrisDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kEmptyFields); // RW table 0x0: no fields
}

W3DDebrisDraw::W3DDebrisDraw(Thing *thing, const W3DDebrisDrawModuleData *data)
	: DrawableModule(thing, data)
	, m_data(data)
{
}

void W3DDebrisDraw::setModelName(const std::string &name, std::int32_t color, int /*shadowType*/)
{
	// RW 0x4B15E1 side (donor setModelName 0x000B1EEB): the model is loaded and added to the scene only when no model is set
	// yet and the name is not empty. The RenderObj / scene add / shadow are the renderer's (stop S-7101).
	if (!m_modelName.empty() || name.empty())
	{
		return;
	}
	m_modelName = name;
	m_modelColor = color;
}

void W3DDebrisDraw::setAnimNames(const std::string &initial, const std::string &flying, const std::string &final)
{
	// RW donor setAnimNames 0x000B207F: "STOP" means the final animation is the flying one frozen on its last frame.
	std::string finalName = final;
	if (AsciiStringUtil::compareNoCase(final, "STOP") == 0)
	{
		m_finalStop = true;
		finalName = flying;
	}
	else
	{
		m_finalStop = false;
	}
	m_animInitial = initial;
	m_animFlying = flying;
	m_animFinal = finalName;
	m_state = INITIAL;
	m_frames = 0;
}

W3DDebrisDrawStep W3DDebrisDraw::advance(bool isAboveTerrain, bool animationComplete)
{
	// RW 0x4B15E1 doDrawModule (donor 0x000B1A37). No model -> nothing is drawn and the machine does not advance.
	W3DDebrisDrawStep step;
	if (m_modelName.empty())
	{
		return step;
	}
	step.hasModel = true;

	const int oldState = m_state;
	const bool firstFrame = m_frames == 0;
	if (m_state != FINAL && !isAboveTerrain && m_frames > MIN_FINAL_FRAMES)
	{
		m_state = FINAL; // the piece landed: go straight to the final animation
	}
	else if (m_state < FINAL && animationComplete)
	{
		++m_state;
	}

	step.anim = m_state;
	step.animChanged = firstFrame || oldState != m_state;
	step.mode = (m_state == FLYING) ? W3DDebrisDrawStep::MODE_LOOP : W3DDebrisDrawStep::MODE_ONCE; // RW TheAnimModes
	if (m_state == FINAL)
	{
		step.finalFX = oldState != FINAL; // the final FX fires once, when FINAL is entered
		if (m_finalStop)
		{
			step.mode = W3DDebrisDrawStep::MODE_MANUAL;
		}
	}

	++m_frames;
	return step;
}

void W3DDebrisDraw::xfer(Xfer &x)
{
	// RW donor W3DDebrisDraw.cpp xfer: model name / colour, the three anim names, then the state, frames and final-stop flag.
	SnapshotXfer::version(x, 1, 1);
	x.xferAsciiString(&m_modelName);
	x.xferInt(&m_modelColor);
	x.xferAsciiString(&m_animInitial);
	x.xferAsciiString(&m_animFlying);
	x.xferAsciiString(&m_animFinal);
	x.xferInt(&m_state);
	x.xferInt(&m_frames);
	x.xferBool(&m_finalStop);
}

void W3DDebrisDraw::crc(StateHasher &hasher) const
{
	hasher.addString(m_modelName);
	hasher.addU32((std::uint32_t)m_modelColor);
	hasher.addString(m_animInitial);
	hasher.addString(m_animFlying);
	hasher.addString(m_animFinal);
	hasher.addI32(m_state);
	hasher.addI32(m_frames);
	hasher.addBool(m_finalStop);
}

void W3DDebrisDraw::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<W3DDebrisDrawModuleData>("W3DDebrisDraw", MODULETYPE_DRAW);
	modules.bindModuleProc("W3DDebrisDraw", MODULETYPE_DRAW,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const auto *typed = dynamic_cast<const W3DDebrisDrawModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("W3DDebrisDraw: the module data is not typed");
			}
			return std::make_unique<W3DDebrisDraw>(thing, typed);
		});
}
