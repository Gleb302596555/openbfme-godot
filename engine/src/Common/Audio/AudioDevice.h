// OpenBFME. GPL-3.0.
//
// IAudioDevice: the boundary between the audio manager core (which decides WHAT plays, how loud and when it stops) and a backend that
// makes sound (the Godot device) or only simulates it (SimulatedAudioDevice, the tests and the headless checks).
//
// It replaces the Miles calls of ZH MilesAudioManager.cpp (AIL_open_stream / AIL_start_sample / AIL_set_3D_position ...): the core owns the
// decisions, the device owns the channel and the sample data. The core computes the final volume of every voice (RW getEffectiveVolume
// 0x4591D4, including the linear distance attenuation) and the left / right gains Miles turns it into (MilesMix.h, lane AUDIO-5), so a
// backend only has to play a stream at two channel gains and a pitch.

#pragma once

#include "Common/INIDataTypes.h"

#include <string>

enum class VoiceKind
{
	Sample2D,
	Sample3D,
	Stream
};

struct VoiceParams
{
	float volume = 1.0f;    ///< final linear volume 0..1 (sliders, distance and shifts applied): what RotWK hands Miles
	float gainLeft = 1.0f;  ///< the linear channel gains Miles mixes the voice at (MilesMix: its volume law, its pan / 3D provider)
	float gainRight = 1.0f;
	float pitch = 1.0f;  ///< playback rate multiplier
	// RW 0x5DD9EE: RotWK hands the voice ReverbEffectLevel, DryLevel and ZoomedInOffscreenOcclusionPercent read from the
	// AudioEventInfo (game.dat RW reader chain: table rows 0xBEFF40/0xBEFF50/0xBEFF60, parser 0x42EEFA, storage +0xA4/+0xA8/+0xAC,
	// pushed into call 0x5DD9EE). voiceParamsFor fills these from the event info; the device layer renders them.
	float reverbSend = 0.0f;  ///< RotWK ReverbEffectLevel 0..1: how much of the voice is sent to reverb (0.0 = none)
	float dryLevel = 1.0f;    ///< RotWK DryLevel 0..1: the dry (direct) level of the voice (1.0 = full dry)
	float occlusion = 0.0f;   ///< RotWK ZoomedInOffscreenOcclusionPercent 0..1 applied when zoomed in and offscreen (0.0 = none)
};

struct VoiceStart
{
	VoiceKind kind = VoiceKind::Sample2D;
	std::string file;      ///< the virtual path inside the archives
	std::string eventName; ///< for the device's own diagnostics
	VoiceParams params;
	bool loop = false;       ///< the device loops the file by itself (a music track, an ambient stream)
	float startFraction = 0.0f; ///< 0..1: where in the file to start (RANDOMSTART)
	float fadeInMs = 0.0f;   ///< FADE_ON_START
};

class IAudioDevice
{
public:
	virtual ~IAudioDevice() {}
	// A voice id > 0, or 0 with *error set (missing file, undecodable data): the caller reports it, nothing is substituted.
	virtual int startVoice(const VoiceStart &start, std::string *error) = 0;
	virtual void updateVoice(int voice, const VoiceParams &params) = 0;
	virtual void stopVoice(int voice) = 0;
	// false once the voice has finished (or was stopped)
	virtual bool isVoicePlaying(int voice) = 0;
	// Playing time of a file at pitch 1.0; -1 + *error when unavailable
	virtual double fileLengthMs(const std::string &file, std::string *error) = 0;
	virtual void setListener(const Coord3D &position, const Coord3D &forward) = 0;
};
