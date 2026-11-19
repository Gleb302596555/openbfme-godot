// OpenBFME. GPL-3.0.
//
// PORT-267 (fork issue 626): the floor record's fade on death, W3DFloorDraw FloorFadeRateOnObjectDeath. RotWK keeps every drawn floor in one
// shared W3DFloorBuffer: one record per (drawable, model name) holding the drawable (record +0x2C), the record's own opacity (+0x84), its fade
// rate (+0x88) and a visibility state (+0x94 / +0x98). The port draws a floor as a per-drawable static model entry (Drawable::DrawEntry, kind
// W3D_DRAWKIND_FLOOR) instead of that buffer; this record is that entry's part of the buffer's record, so the fade needs no second path.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; every read below was taken with tools/mod/rw.py dis):
//   * RW 0x4CEBCA (W3DFloorDraw's doDrawModule, instance vtable RW 0xBE3378 slot +0x2C): with the module data (RW 0x4CEBCE) and the drawable
//     (RW 0x4CEBD6) and a non-zero FloorFadeRateOnObjectDeath (data +0x20, RW 0x4CEBDA) and the drawable's object effectively dead
//     (RW 0x4CEBEC..0x4CEBFD: Object + 0x458 bit 0) it passes the model name and the rate to RW 0x46AFCA.
//   * RW 0x46AFCA: the global's floor buffer (+0x3860) when it exists, then RW 0x4E5A5B: with a drawable and the buffer enabled (+0x20) it walks
//     its records, matches the drawable's name (RW 0x5DC493 then RW 0x4E57E7) and stores the rate at the record's +0x88 (RW 0x4E5A98).
//   * RW 0x4E3EC5 (one record's update, run by the walk RW 0x4E547D): an empty drawable (+0x2C) or a state other than 1 (+0x98) returns without
//     a change (RW 0x4E3ED3..0x4E3EE0); a non-zero rate (+0x88) is added to the opacity (+0x84) and clamped to 0 .. 1 (RW 0x4E3F00..0x4E3F25,
//     the upper bound RW 0xBD1908 = 1.0f); with a zero rate the opacity is the drawable's own (RW 0x4E3F2D..0x4E3FD4) and the record's matrix is
//     resynced (that part is the drawing transform, not ported here).
//   * RW 0x4E547D (the walk): each record's visibility result (RW 0x4E42DE: bit 12 of Drawable + 0x264) drives the state machine at
//     RW 0x4E54BE..0x4E54FD: state 0 becomes 1 when visible, 1 becomes 2 when hidden, 2 becomes 1 when visible again.
//   * RW 0x4E504B (the buffer's xfer): the record saves version and name, the drawable id, its matrix, three flag bytes, the opacity
//     (RW 0x4E50C3) and the rate (RW 0x4E50D1) as floats and two four-byte integers (+0x94 at RW 0x4E50E1, then +0x98).
//
// DONOR FACTS (Open-BFME-2, BFME2 1.06): W3DFloorBuffer.cpp, m_opacity84 (+0x84) / m_88 (+0x88) / m_94 / m_98, DoXfer -> the per-record xfer.
//
// INFERENCE / OpenBFME: the record is created at opacity 1.0 (the drawable's opacity until the first update writes it); the module update
// (RW 0x4CEBCA) and the buffer's walk (RW 0x4E547D) both run once per client frame and the port runs them in that order.

#pragma once


#include <cstdint>

namespace FloorFade
{
class Record
{
public:
	enum State
	{
		STATE_NEW = 0,     ///< +0x98 = 0: created, not drawn yet
		STATE_VISIBLE = 1, ///< +0x98 = 1: drawn; the only state the update steps
		STATE_HIDDEN = 2   ///< +0x98 = 2: not drawn; the opacity is frozen
	};

	// RW 0x4E5A5B (RW 0x4E5A98 stores into the record's +0x88): the rate the floor module hands over. The buffer finds its record by the
	// drawable and the model name; the port's entry is that record, so the name match is the entry's own model.
	void setRate(float rate) { m_rate = rate; }

	// RW 0x4E547D's state machine (RW 0x4E54BE..0x4E54FD) over the visibility RW 0x4E42DE returns.
	void setVisible(bool visible)
	{
		const int now = visible ? 1 : 0;
		if (m_state == STATE_NEW && now == 1)
		{
			m_state = STATE_VISIBLE; // RW 0x4E54CB..0x4E54D6
		}
		else if (m_state == STATE_VISIBLE && now == 0)
		{
			m_state = STATE_HIDDEN; // RW 0x4E54DD..0x4E54EB
		}
		else if (m_state == STATE_HIDDEN && now == 1)
		{
			m_state = STATE_VISIBLE; // RW 0x4E54F2..0x4E54FD
		}
	}

	// RW 0x4E3EC5: one client frame of the record; true when the opacity changed.
	bool step(float drawableOpacity)
	{
		if (m_state != STATE_VISIBLE)
		{
			return false; // RW 0x4E3ED3..0x4E3EE0
		}
		float next;
		if (m_rate != 0.0f) // RW 0x4E3EF3..0x4E3EFE (a zero rate takes the other branch)
		{
			next = m_opacity + m_rate; // RW 0x4E3F00..0x4E3F08
			if (next < 0.0f)
			{
				next = 0.0f; // RW 0x4E3F0C..0x4E3F11
			}
			if (next > 1.0f)
			{
				next = 1.0f; // RW 0x4E3F14..0x4E3F25
			}
		}
		else
		{
			next = drawableOpacity; // RW 0x4E3F2D..0x4E3FD4: the drawable's own opacity
		}
		if (m_opacity == next)
		{
			return false; // RW 0x4E3FE5..0x4E3FF2 stores only a changed value
		}
		m_opacity = next;
		return true;
	}

	// RW 0x4E504B: the record's opacity (+0x84, RW 0x4E50C3), its rate (+0x88, RW 0x4E50D1) and its state (+0x94 / +0x98, four raw bytes).
	// The transfer is a template so that this header does not pull Common/Xfer.h (which declares class Snapshot) into every user of Drawable.h.
	template <typename XferType> void xfer(XferType &x)
	{
		x.xferReal(&m_opacity);
		x.xferReal(&m_rate);
		std::int32_t state = m_state;
		x.xferInt(&state);
		m_state = state;
	}

	float opacity() const { return m_opacity; } ///< record +0x84
	float rate() const { return m_rate; }       ///< record +0x88
	int state() const { return m_state; }       ///< record +0x98

private:
	float m_opacity = 1.0f;
	float m_rate = 0.0f;
	int m_state = STATE_NEW;
};
} // namespace FloorFade
