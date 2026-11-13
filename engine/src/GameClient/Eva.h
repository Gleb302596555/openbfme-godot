// OpenBFME. GPL-3.0.
//
// Eva: the announcer (ZH GameClient/Eva.cpp, here RotWK's name based event system). Game code REPORTS events (a building was lost, a hero died, the
// player cannot afford something) and Eva decides what the local player hears, when and from where.
//
// The rules are the ones EA documents in Data\INI\Eva.ini and Default\Eva.ini (the comments next to every field; the binary's own queue code is not
// decoded: acceptance stop S-247, the INI text is the specification used):
//   * Priority: if several events want to play, the highest priority plays first (ties: the one reported first).
//   * TimeBetweenEventsMS: after an event plays, the SAME event is ignored for this long.
//   * ExpirationTimeMS: an event that cannot play because Eva is busy with another sound is held this long, then thrown away.
//   * QuietTimeMS: "the event must not be reported to Eva for this long before it can play" (a condition that keeps being reported postpones it).
//   * MillisecondsToWaitBeforePlaying: the event waits this long after it was reported ("wait until really ready").
//   * OtherEvaEventsToBlock: while this event plays, the listed events are dropped and counted as just played (INFERENCE of the exact moment).
//   * SideSound: the sound the player's SIDE hears (Side compared without case; no entry for the side: nothing plays and the event is consumed).
//   * AlwaysPlayFromHomeBase: the sound comes from the home base, otherwise from the event's position (a 2D sound when there is none).
//   * CountAsJumpToLocation: when the event's sound starts, its jump position (or else its position) joins the list the space bar jumps through. RotWK
//     does it at play time, not at the report: RW 0x5DD7FF .. 0x5DD826 (the audio handle >= 5, the record's + 0x2D = CountAsJumpToLocation, field
//     table RW 0xBF2270; the position RW 0x5DC455: the jump position + 0x14 when set (+ 0x21), else the position + 0x8 (+ 0x20)) calls
//     RW 0x5DD1F3. The list lives in the radar here (Radar::noteEvaPosition); the HUD hands it what takePlayedJumpPositions() returns.
//   * Eva never overlaps itself: one Eva sound at a time, played through the audio manager as an ordinary event of the local player.
// Time is the caller's client clock in milliseconds (never the logic clock: Eva is presentation and must not affect simulation state).

#pragma once

#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"

#include <string>
#include <vector>

class Eva
{
public:
	Eva(AudioIniState &ini, AudioManager &audio);

	// The local player's side name ("Elves", "Mordor", ...) and home base position (valid when the player has a base).
	void setLocalSide(const std::string &side) { m_side = side; }
	void setHomeBase(const Coord3D &position, bool valid)
	{
		m_home = position;
		m_homeValid = valid;
	}
	void setLocalPlayerIndex(int index) { m_localPlayer = index; }
	void setEnabled(bool enabled);
	bool isEnabled() const { return m_enabled; }

	// The event `index` (EvaEventStore) / `name` happened. false: the event is unknown, Eva is disabled or the event was dropped on the spot (an identical
	// event inside its TimeBetweenEventsMS).
	bool reportEvent(int index, const Coord3D *position, double nowMs, const Coord3D *jumpPosition = nullptr);
	bool reportEventByName(const std::string &name, const Coord3D *position, double nowMs, const Coord3D *jumpPosition = nullptr);
	void update(double nowMs);
	void reset();

	// "Player_/Eva event played recently": was `index` played within `withinMs`?
	bool playedRecently(int index, double withinMs, double nowMs) const;
	// where `index` last played (false: never played, or it had no position)
	bool lastPlayedPosition(int index, Coord3D *position) const;
	int findEvent(const std::string &name) const { return m_ini.eva.findIndex(name, false); }
	// RW 0x5DCC88 (BFME2 decomp Eva::isEventBlockedByTimeout, Eva.cpp:522): an unknown event counts as blocked; RW's status timer is not decoded (S-247):
	// INFERENCE, blocked = a report now would be dropped as identical (it played, or another event blocked it, within its TimeBetweenEventsMS)
	bool isEventBlockedByTimeout(int index, double nowMs) const;
	// RW 0x5DCCEB (Eva::isEventAboutToPlay, Eva.cpp:534): false for an unknown event; INFERENCE (S-247): the event waits in the queue
	bool isEventAboutToPlay(int index) const;
	// the jump positions of the CountAsJumpToLocation events whose sound started since the last call (RW 0x5DD826 -> RW 0x5DD1F3), oldest first
	std::vector<Coord3D> takePlayedJumpPositions();
	const MiscEvaData &miscEvaData() const { return m_ini.eva.miscEvaData(); }
	float enemySightedMaxVoicePositionScanRange() const { return m_ini.eva.miscEvaData().enemySightedMaxVoicePositionScanRange; }

	struct Counters
	{
		unsigned reported = 0, played = 0, droppedIdentical = 0, droppedExpired = 0, droppedBlocked = 0, noSideSound = 0, droppedQuiet = 0;
	};
	const Counters &counters() const { return m_counters; }
	size_t pendingCount() const { return m_pending.size(); }

private:
	struct Pending
	{
		int index;
		double reportedMs;
		Coord3D position;
		bool hasPosition;
		Coord3D jump;  ///< + 0x14 (RW 0x5DC455)
		bool hasJump;  ///< + 0x21
	};
	void blockOthers(const EvaEventRecord &record, double nowMs);

	AudioIniState &m_ini;
	AudioManager &m_audio;
	std::string m_side;
	Coord3D m_home;
	bool m_homeValid = false;
	int m_localPlayer = -1;
	bool m_enabled = true;
	std::vector<Pending> m_pending;
	std::vector<double> m_lastPlayed;   ///< per event index; negative = never
	std::vector<double> m_lastReported; ///< per event index; negative = never
	std::vector<std::pair<bool, Coord3D>> m_lastPlayedPos; ///< per event index: the position it last played at
	AudioHandle m_speech = 0;
	std::vector<Coord3D> m_playedJumps;
	Counters m_counters;
};
