// OpenBFME. GPL-3.0.
// RotWK 2.01 W3DSailModelDraw runtime (RW 0x4CFF82 ctor, vtable 0xBE3C40, +0x104 = RW 0x4D00CD).
// The scripted-model base renders the model; only its assembled matrix gets the sail's local Z turn.
// The fixed direction, clamp and per-call damping are read from RW 0x4D00CD..0x4D033B
// (DONOR BFME2 0x000D08C3, W3DSailModelDrawCtor.cpp). BlowingThresholdDegrees is parsed but
// no function of the 68-slot sail vtable reads it (docs/ennorath/port63-sail-rw.md).
// The RW client xfer (0x4D0009) saves the angle and valid flag; the port does not yet
// snapshot *client draw runtimes* (S-7232), so this visual-only state is not simulation state.
#pragma once

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawVariants.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"

class Matrix3D;

class W3DSailModelDraw final : public W3DScriptedModelDraw
{
public:
	W3DSailModelDraw(const W3DSailModelDrawModuleData &data, W3DDrawAssets &assets, W3DDrawRandom &random,
		W3DDrawScriptHost *scripts = nullptr, const Options &options = Options());
	void adjustAttachedTransform(Matrix3D &matrix) override;

private:
	const W3DSailModelDrawModuleData &m_sailData;
	float m_lastAngle = 0.0f; // RW +0x2E8
	bool m_hasLastAngle = false; // RW +0x2EC: first matrix takes the target without damping
};
