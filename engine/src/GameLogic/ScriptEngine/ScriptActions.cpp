// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
// See GameLogic/ScriptEngine/ScriptActions.h.

#include "GameLogic/ScriptEngine/ScriptActions.h"
#include "GameLogic/SelfDestruct.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Science.h"
#include "Common/Team.h"
#include "Common/Thing/ThingFactory.h"
#include "GameClient/MapChunks.h"
#include "GameLogic/AI/AICommandSink.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ScriptEngine/ScriptConditions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/ScriptEngine/ScriptTemplates.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/System/ShroudManager.h"

#include <cstdint>
#include <cstring>
#include <set>

namespace
{

const ScriptParameter &param(const ScriptActionRec &a, size_t i)
{
	static const ScriptParameter kEmpty;
	return i < a.params.size() ? a.params[i] : kEmpty;
}

// RW adds 32-bit registers (add): two's complement wrap-around, as PlayerScience's wrapAdd. The sum is computed on uint32_t and its bits moved
// to int32_t (memcpy), so an overflow is the retail result and never C++ signed overflow (#342 review).
std::int32_t wrapAdd(std::int32_t a, std::int32_t b)
{
	const std::uint32_t u = (std::uint32_t)a + (std::uint32_t)b;
	std::int32_t r;
	std::memcpy(&r, &u, sizeof r);
	return r;
}

int ordinal(const char *name)
{
	return ScriptTemplates::findAction(name);
}

// Parameter::ParameterType values (the templates' parameter types)
constexpr int PARAM_WAYPOINT = 7;
constexpr int PARAM_CAMERA_POSITION = 51; ///< MOVE_CAMERA_TO's first parameter (a waypoint or named camera name in the corpus)

// The actions recorded as client requests (S-1182): the camera, the screen, the UI, audio, movies and input. Retail runs them on TheTacticalView,
// TheInGameUI, TheAudio ... from inside the logic frame; here the logic records them and the client applies them in frame order.
const std::set<int> &clientActions()
{
	static const std::set<int> s = [] {
		static const char *const names[] = {
			"MOVE_CAMERA_TO", "MOVE_CAMERA_ALONG_WAYPOINT_PATH", "MOVE_CAMERA_ALONG_SPLINE_PATH", "MOVE_CAMERA_LOCATOR_ALONG_SPLINE_PATH",
			"ROTATE_CAMERA", "RESET_CAMERA", "ZOOM_CAMERA", "PITCH_CAMERA", "ROLL_CAMERA", "ROTATE_CAMERA_LOCKED", "CAMERA_FOLLOW_NAMED",
			"CAMERA_STOP_FOLLOW", "CAMERA_LOOK_TOWARD_OBJECT", "CAMERA_LOOK_TOWARD_WAYPOINT", "CAMERA_MOD_SET_FINAL_ZOOM", "CAMERA_MOD_SET_FINAL_PITCH",
			"CAMERA_MOD_FREEZE_TIME", "CAMERA_MOD_FREEZE_ANGLE", "CAMERA_MOD_FINAL_LOOK_TOWARD", "CAMERA_MOD_LOOK_TOWARD", "CAMERA_MOD_SET_ROLLING_AVERAGE",
			"CAMERA_MOD_FINAL_SPEED_MULTIPLIER", "CAMERA_MOD_FINAL_TIME_MULTIPLIER", "CAMERA_LETTERBOX_BEGIN", "CAMERA_LETTERBOX_END", "CAMERA_BW_MODE_BEGIN",
			"CAMERA_BW_MODE_END", "CAMERA_MOTION_BLUR", "CAMERA_MOTION_BLUR_JUMP", "CAMERA_MOTION_BLUR_FOLLOW", "CAMERA_MOTION_BLUR_END_FOLLOW",
			"CAMERA_FADE_ADD", "CAMERA_FADE_SUBTRACT", "CAMERA_FADE_SATURATE", "CAMERA_FADE_MULTIPLY", "CAMERA_SET_DEFAULT", "CAMERA_MOVE_HOME",
			"CAMERA_TETHER_NAMED", "CAMERA_STOP_TETHER_NAMED", "CAMERA_ENABLE_SLAVE_MODE", "CAMERA_DISABLE_SLAVE_MODE", "CAMERA_ADD_SHAKER_AT",
			"CAMERA_SET_AUDIBLE_DISTANCE", "SETUP_CAMERA", "SHOW_MILITARY_CAPTION", "DISPLAY_TEXT", "DISPLAY_CINEMATIC_TEXT", "DISPLAY_COUNTER",
			"HIDE_COUNTER", "DISPLAY_COUNTDOWN_TIMER", "HIDE_COUNTDOWN_TIMER", "DISPLAY_NOTIFICATION_BOX", "DISPLAY_NOTIFICATION_BOX_WITH_OBJECT_TYPE_IMAGE_OVERRIDE",
			"SHOW_MISSION_OBJECTIVE", "HIDE_MISSION_OBJECTIVE", "MARK_MISSION_OBJECTIVE_COMPLETED", "MARK_MISSION_OBJECTIVE_NOT_COMPLETED",
			"PLAY_SOUND_EFFECT", "PLAY_SOUND_EFFECT_AT", "PLAY_SOUND_EFFECT_AT_TEAM", "SOUND_PLAY_NAMED", "SPEECH_PLAY", "MUSIC_SET_TRACK",
			"MUSIC_SET_VOLUME", "SOUND_SET_VOLUME", "SPEECH_SET_VOLUME", // lane SCRIPT-3: RW 0x7BCAAE (TheAudio's script volume, client)
			"REFRESH_RADAR", // lane BFME2CAMP1: RW 0x7BCC53: TheRadar (RW 0xDE4AB8) vslot 0x10(TheTerrainLogic), the client's radar redraw
			"MUSIC_PLAY_TRACK_FINITE_TIMES", "MOVIE_PLAY_FULLSCREEN", "MOVIE_PLAY_RADAR", "PLAY_MOVIE_IN_GAME", "DISABLE_INPUT", "ENABLE_INPUT",
			"HIDE_UI", "SHOW_UI", "DESELECT", "SELECT_OBJECT", "EVA_SET_ENABLED_DISABLED", "AUDIO_MAKE_SOUND_IMMUNE_TO_FADE", "AUDIO_FADE_VOLUME",
			"AUDIO_RESTORE_VOLUME_ALL_TYPE", "AUDIO_OVERRIDE_VOLUME_TYPE", "AUDIO_RESTORE_VOLUME_TYPE", "AUDIO_SET_VOLUME_TYPE", "CAMEO_FLASH",
			"NAMED_FLASH", "TEAM_FLASH", "ENABLE_HOUSE_COLOR", "DRAW_SKYBOX_BEGIN", "DRAW_SKYBOX_END", "SUSPEND_BACKGROUND_SOUNDS",
			"RESUME_BACKGROUND_SOUNDS", "SET_VISUAL_SPEED_MULTIPLIER", "SHOW_WEATHER", "SCREEN_SHAKE", "SELECT_BUILDER_BUTTON_FLASH", "HERO_SELECT_BUTTON_FLASH",
			"CLOSE_OBJECTIVES_SCREEN", "PLAYER_SELECT_SKILLSET", "MAP_SHROUD_AT_WAYPOINT",
			// lane SCRIPT-2: the victory screen (RW 0x7BF05C: TheShell's victory window, no logic), the objectives button flash (RW 0x7BDF4E), the command
			// bar edits (RW 0x7C6793 ...: TheControlBar). Lane BACKLOG3: the radar event actions are ported below (radarEventAction)
			"VICTORY_SCREEN", "FLASH_OBJECTIVES_BUTTON", "FLASH_SPELL_STORE_BUTTON", "FLASH_PLANNING_MODE_BUTTON", // lane CAMPUI-1: RW 0x6D455F's other two slots "COMMANDBAR_REMOVE_BUTTON_OBJECTTYPE", "COMMANDBAR_ADD_BUTTON_OBJECTTYPE_SLOT",
			"TOGGLE_AVI_CAPTURE", "ENABLE_OBJECT_SOUND", "DISABLE_OBJECT_SOUND",
			// lane SCRIPT-4: RW 0x7BC604 doSetBorderShroud (BFME2 ScriptActions.cpp, tier A): TheDisplay vslot 0x138 with GlobalData ClearAlpha (+ 0xBE8) / ShroudAlpha
			// (+ 0xBEA): the shroud border's fill (client, not saved)
			"DISABLE_BORDER_SHROUD", "ENABLE_BORDER_SHROUD",
			// lane CAMANIM-1: case 0x226 (RW 0x7CB8F4) -> RW 0x7BBB9D: TheCameraAnimationList's animation of that name (RW 0x7320BA) to TheTacticalView's
			// slot 0x68 (RW 0x4864F9); client state only (GameClient/CameraAnimation.h)
			"MOVE_CAMERA_BY_ANIMATION",
			// lane MODCOMPAT-AI: RW 0x7CAFA5 case 0x14E: GlobalData + 0x950 = the real parameter (movss), the camera's far clip multiplier that
			// setCameraTransform reads (RW 0x48B7B1: far = + 0x950 * 1800); nothing else in the logic (client, not saved)
			"SET_CAMERA_CLIP_DEPTH_MULTIPLIER",
			// lane PORT-439: RW 0x7CAFA5 case 0x185 (RW 0x7CB6FB) -> RW 0x7BB627(focal, seconds * 1000, easeIn * 1000, easeOut * 1000) ->
			// TheTacticalView vslot 0xF0 = View::setFocalLength RW 0x48885E: the view's own camera animation (client, not saved)
			"FOCAL_LENGTH_CAMERA",
			// LOCAL-94 (issue #720): case 331 (RW 0x7CE53A) -> RW 0x7BD6E9 (DONOR BFME2 0x003BCC08, ScriptActions::doResizeViewGuardband) ->
			// TheTacticalView vslot 0x1BC = W3DView::setGuardBandBias (RW 0x48B5FC: View + 0x78 / + 0x7C); client tactical view state only
			"RESIZE_VIEW_GUARDBAND",
			// Local 161 (fork issue 928): citations for client actions already in this list (S-1182, RotWK 2.01 game.dat):
			// RW 0x7BB3AD (DISABLE_OBJECT_SOUND), RW 0x7BB3D9 (AUDIO_FADE_VOLUME), RW 0x7BB419 (AUDIO_MAKE_SOUND_IMMUNE_TO_FADE),
			// RW 0x7BB5D6 (ZOOM_CAMERA), RW 0x7BB678 (PITCH_CAMERA), RW 0x7BB6C9 (ROLL_CAMERA), RW 0x7BB771 (SETUP_CAMERA),
			// RW 0x7BB926 (ROTATE_CAMERA), RW 0x7BB9F1 (ROTATE_CAMERA_LOCKED), RW 0x7BBA44 (CAMERA_LOOK_TOWARD_OBJECT),
			// RW 0x7BBABB (CAMERA_LOOK_TOWARD_WAYPOINT), RW 0x7BBB23 (MOVE_CAMERA_LOCATOR_ALONG_SPLINE_PATH), RW 0x7BBFB2 (CAMEO_FLASH),
			// RW 0x7BC07A (MOVIE_PLAY_RADAR), RW 0x7BC2B1 (CAMERA_MOTION_BLUR_JUMP), RW 0x7BC34B (CAMERA_MOTION_BLUR),
			// RW 0x7BC8F7 (CAMERA_BW_MODE_END), RW 0x7BC9A7 (SHOW_MILITARY_CAPTION), RW 0x7BCC65 (CAMERA_TETHER_NAMED),
			// RW 0x7BCCEF (CAMERA_SET_DEFAULT), RW 0x7BCF3B (SCREEN_SHAKE), RW 0x7BD005 (AUDIO_OVERRIDE_VOLUME_TYPE),
			// RW 0x7BD030 (AUDIO_RESTORE_VOLUME_TYPE), RW 0x7BD53E (EVA_SET_ENABLED_DISABLED), RW 0x7BD5BB (CAMERA_ENABLE_SLAVE_MODE),
			// RW 0x7BD5C9 (CAMERA_DISABLE_SLAVE_MODE), RW 0x7BD5D7 (CAMERA_ADD_SHAKER_AT), RW 0x7BD618 (PLAYER_SELECT_SKILLSET),
			// RW 0x7BD783 (TOGGLE_AVI_CAPTURE), RW 0x7BDC7F (SELECT_OBJECT), RW 0x7BDD97 (SELECT_BUILDER_BUTTON_FLASH),
			// RW 0x7BDDB0 (ENABLE_HOUSE_COLOR), RW 0x7BDF39 (FLASH_SPELL_STORE_BUTTON), RW 0x7BDF63 (FLASH_PLANNING_MODE_BUTTON),
			// RW 0x7BE0EA (PLAY_SOUND_EFFECT), RW 0x7BE183 (PLAY_SOUND_EFFECT_AT), RW 0x7BF36F (PLAY_SOUND_EFFECT_AT_TEAM).
			// Local 161 (fork issue 928): client-only audio / camera / lighting / UI / drawable actions recorded as client requests (S-1182):
			"COMMANDBAR_ADD_BUTTON_OBJECTTYPE_SLOT", // RW 0x7BD65B (case 312: ControlBar / GameLogic::setControlBarOverride)
			"AUDIO_MAKE_SOUND_SUBJECT_TO_FADE", // RW 0x7BB434 (case 442: TheAudio vslot 0xFC)
			"AUDIO_MAKE_ALL_SOUNDS_SUBJECT_TO_FADE", // RW 0x7BB44F (case 443: TheAudio vslot 0x100)
			"DIM_WORLD_LIGHTS", // RW 0x7BB464 (case 437: TheTerrainVisual vslot 0xCC)
			"RESTORE_WORLD_LIGHTS", // RW 0x7BB497 (case 438: TheTerrainVisual vslot 0xD0)
			"OVERSIZE_TERRAIN", // RW 0x7BB726 (case 123: TheTacticalView vslot 0x5C)
			"MOVE_CAMERA_TO_SELECTION", // RW 0x7BB860 (case 35: TheInGameUI / TheTacticalView)
			"ROTATE_CAMERA_TO_ANGLE", // RW 0x7BB979 (case 529: TheTacticalView vslot 0x100 / 0xCC)
			"NAMED_CUSTOM_COLOR", // RW 0x7BBFF2 (case 252: Object::setCustomIndicatorColor 0x68B6CB)
			"NAMED_FLASH_WHITE", // RW 0x7BC013 (case 246: Drawable +0x168/+0x16C flash)
			"AUDIO_SET_REVERB_ROOM_TYPE", // RW 0x7BC088 (case 448: TheAudio vslot 0x164)
			"AUDIO_REMOVE_REVERB", // RW 0x7BC09D (case 449: TheAudio vslot 0x16C)
			"AUDIO_REMOVE_ALL_REVERB_SUPPRESSION_POLYGONS", // RW 0x7BC0B0 (case 527: TheAudio vslot 0x17C)
			"SOUND_AMBIENT_RESUME", // RW 0x7BC714 (case 101: TheAudio vslot 0x40/0x44)
			"MUSIC_SCRIPT_POP_MUSIC", // RW 0x7BC734 (case 458: TheAudio vslot 0x88)
			"MUSIC_TURN_OFF_MUSIC_SCRIPTING", // RW 0x7BC766 (case 459: TheAudio vslot 0x90)
			"MUSIC_RETURN_TO_MUSIC_SCRIPTING", // RW 0x7BC785 (case 460: TheAudio vslot 0x94)
			"NAMED_SET_CAMERA_FADING", // RW 0x7BC7B5 (case 530: Drawable camera fading)
			"CAMERA_RING_MODE_END", // RW 0x7BC944 (case 350: TheTacticalView vslot 0xB4/0xBC)
			"DISABLE_SPECIAL_POWER_DISPLAY", // RW 0x7BCA42 (case 159: TheDisplay vslot 0x94)
			"ENABLE_SPECIAL_POWER_DISPLAY", // RW 0x7BCA53 (case 160: TheDisplay vslot 0x94)
			"NAMED_HIDE_SPECIAL_POWER_DISPLAY", // RW 0x7BCA64 (case 161: TheDisplay vslot 0x9C)
			"NAMED_SHOW_SPECIAL_POWER_DISPLAY", // RW 0x7BCA89 (case 162: TheDisplay vslot 0xA0)
			"DISABLE_COUNTDOWN_TIMER_DISPLAY", // RW 0x7BCBBA (case 166: TheDisplay 0x69B0B1)
			"ENABLE_COUNTDOWN_TIMER_DISPLAY", // RW 0x7BCBC8 (case 165: TheDisplay 0x69B0B1)
			"SOUND_ENABLE_TYPE", // RW 0x7BCFB2 (case 222: TheAudio vslot 0xB4)
			"SOUND_DISABLE_TYPE", // RW 0x7BCFC9 (case 221: TheAudio vslot 0xB8)
			"SOUND_ENABLE_ALL", // RW 0x7BCFE0 (case 223: TheAudio vslot 0xBC)
			"SOUND_REMOVE_TYPE", // RW 0x7BCFF1 (case 325: TheAudio vslot 0x68)
			"NAMED_SET_EMOTICON", // RW 0x7BD477 (case 288: Drawable::setEmoticon 0x6739E0)
			"CAMERA_BLOOM_EFFECT_END", // RW 0x7BD762 (case 342: GlobalData bloom flag)
			"LOCK_CAMERA", // RW 0x7BDAB7 (case 513: TheInGameUI camera lock)
			"LOCK_CAMERA_ZOOM", // RW 0x7BDACA (case 574: TheTacticalView vslot 0x24C)
			"LOCK_CAMERA_ROTATION", // RW 0x7BDAD8 (case 575: TheTacticalView vslot 0x250)
			"LOCK_CAMERA_SCROLL", // RW 0x7BDAE6 (case 576: TheTacticalView vslot 0x254)
			// Local 180 (fork issue 961): RW 0x7BB316 (ScriptActions::reset letterbox clear), RW 0x7BB32E (CAMERA_LETTERBOX_BEGIN),
			// RW 0x7BC8B7 (CAMERA_LETTERBOX_END), RW 0x7BD047 (AUDIO_RESTORE_VOLUME_ALL_TYPE).
			"TERRAIN_RENDER_DISABLE", // RW 0x7BCE53 (case 372: TheTerrainVisual +0x1914 client render toggle)
			"LOCK_CAMERA_RESET", // RW 0x7BDAF4 (case 577: TheTacticalView vslot 0x258)
			"LOCK_CAMERA_ANGLE_AND_HEIGHT", // RW 0x7BDB02 (case 528: TheInGameUI angle/height lock)
			"ENABLE_SPELL_STORE", // RW 0x7BDF0F (case 580: AptSpellStore 0x822D75)
			"ENABLE_PLANNING_MODE", // RW 0x7BDF2E (case 584: TheAptUI 0x6D47F0)
			// LOCAL-162 (issue #929): already-present client actions cited with their RotWK 2.01 entry addresses:
			//   DISPLAY_TEXT (RW 0x7C081B), DISPLAY_CINEMATIC_TEXT (RW 0x7C083E), MOVIE_PLAY_FULLSCREEN (RW 0x7C0A7F),
			//   CAMERA_MOD_LOOK_TOWARD (RW 0x7C57F3), CAMERA_MOD_FINAL_LOOK_TOWARD (RW 0x7C5859), RESET_CAMERA (RW 0x7C58BF),
			//   DISPLAY_COUNTER (RW 0x7C5E9F), DISPLAY_COUNTDOWN_TIMER (RW 0x7C5F37), HIDE_COUNTDOWN_TIMER / HIDE_COUNTER (RW 0x7C5FCF).
			// Additional client-only audio / drawable / UI actions (no simulation state):
			"TEAM_FLASH_WHITE",                             // RW 0x7C09E3 (case 247: Drawable flash white on team members)
			"AUDIO_SET_REVERB_SUPPRESSION_POLYGON",         // RW 0x7C0AE0 (case 525: TheAudio vslot 0x174)
			"AUDIO_REMOVE_REVERB_SUPPRESSION_POLYGON",      // RW 0x7C0B24 (case 526: TheAudio vslot 0x178)
			"MUSIC_SCRIPT_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY", // RW 0x7C0EB8 (case 478: TheAudio music script push)
			"TEAM_SET_EMOTICON",                            // RW 0x7C299D (case 287: Drawable emoticon on team members)
			"UNIT_ENABLE_HOUSE_COLOR",                      // RW 0x7C4156 (case 516: Drawable house color toggle on unit)
			"TEAM_ENABLE_HOUSE_COLOR",                      // RW 0x7C4194 (case 517: Drawable house color toggle on team)
			"OBJECT_FORCE_SELECT",                          // RW 0x7C6552 (case 209: TheInGameUI selection / audio / view center)
		};
		std::set<int> out;
		for (const char *n : names)
		{
			const int o = ordinal(n);
			if (o >= 0)
			{
				out.insert(o);
			}
		}
		return out;
	}();
	return s;
}

bool positionOf(ScriptEngine &engine, const std::string &name, Coord3D &out, bool cameraFirst = false)
{
	// lane CAMPUI-1: MOVE_CAMERA_TO's parameter (type 51) is a named camera or a waypoint; RW 0x7BB540 (BFME2 0x7BAAE4, decomp ScriptActions.cpp)
	// looks up both and the named camera's position wins
	if (cameraFirst)
	{
		if (const NamedCamera *c = engine.findNamedCamera(name))
		{
			out = c->position;
			return true;
		}
	}
	if (const TerrainLogic *terrain = engine.logic().terrain())
	{
		if (const Waypoint *w = terrain->findWaypointByName(name))
		{
			out = w->location;
			return true;
		}
	}
	if (const NamedCamera *c = engine.findNamedCamera(name))
	{
		out = c->position;
		return true;
	}
	return false;
}

// Lane BACKLOG3 (S-2452): the scripts' radar events. TARGET FACTS (RotWK game.dat, caveat S-001; BFME2 decomp ScriptActions.cpp /
// ScriptActions_doObjectRadarCreateEvent.cpp, tier A for these addresses):
//   RADAR_CREATE_EVENT (RW 0x7BC221): TheRadar->createEvent(the Coord3D parameter, the type parameter, 4.0f (RW 0xBD88C0)), then TheEva's event position (RW 0x5DD1F3);
//   OBJECT_CREATE_RADAR_EVENT (RW 0x7BC250): the same at the named unit's position (Object + 0x38); nothing without the unit;
//   TEAM_CREATE_RADAR_EVENT (RW 0x7C0B61): the team by name (getTeamNamed), only when it has a unit (RW 0x7A07D8: a member that is not dead, not Object + 0x94
//   bit 0, not a STRUCTURE (KindOf bit 7), not a PROJECTILE (bit 25) and not IGNORE_FOR_VICTORY (bit 148)), at the members' average position (RW 0x7A0ECD).
// The radar is the client's: the logic records the event (GameLogic::noteRadarEvent), Radar::noteLogicEvents applies it.
void radarEventAction(ScriptEngine &engine, const ScriptActionRec &a, const std::string &name)
{
	GameLogic &logic = engine.logic();
	const int type = param(a, 1).intValue;
	Coord3D pos{ 0.0f, 0.0f, 0.0f };
	if (name == "RADAR_CREATE_EVENT")
	{
		const ScriptParameter &p = param(a, 0);
		pos = Coord3D{ p.x, p.y, p.z };
	}
	else if (name == "OBJECT_CREATE_RADAR_EVENT")
	{
		const Object *o = engine.getUnitNamed(param(a, 0).stringValue);
		if (!o)
		{
			return;
		}
		pos = *o->getPosition();
	}
	else
	{
		const Team *team = ScriptConditions::team(engine, param(a, 0).stringValue);
		if (!team)
		{
			return;
		}
		// the binary's bit numbers (RW 0x7A07D8 reads the template's KindOf words at + 0x108 and + 0x11A)
		constexpr unsigned kStructure = 7, kProjectile = 25, kIgnoreForVictory = 148;
		bool hasUnit = false;
		float x = 0.0f, y = 0.0f, z = 0.0f;
		int n = 0;
		for (const Object *o = team->getFirstMember(); o; o = o->friend_teamNext())
		{
			hasUnit = hasUnit || (!o->isEffectivelyDead() && !o->isDestroyed() && !o->isKindOf(kStructure) && !o->isKindOf(kProjectile) && !o->isKindOf(kIgnoreForVictory));
			// RW 0x7A0ECD: every member, SSE sums in member order, times 1 / n
			x = SimMath::addf32(x, o->getPosition()->x);
			y = SimMath::addf32(y, o->getPosition()->y);
			z = SimMath::addf32(z, o->getPosition()->z);
			++n;
		}
		if (!hasUnit)
		{
			return;
		}
		const float f = SimMath::divf32(1.0f, (float)n);
		pos = Coord3D{ SimMath::mulf32(x, f), SimMath::mulf32(y, f), SimMath::mulf32(z, f) };
	}
	// a map's Coord3D parameter is outside data: a position that is not a number is refused by name (retail would hand it to the radar and to the view)
	const auto finite = [](float v) {
		std::uint32_t bits;
		std::memcpy(&bits, &v, sizeof bits);
		return (bits & 0x7F800000u) != 0x7F800000u; // neither an infinity nor a NaN (no float arithmetic: the exponent's bits)
	};
	if (!finite(pos.x) || !finite(pos.y) || !finite(pos.z))
	{
		logic.reportError("script action " + name + ": a radar event position that is not a finite number is refused");
		return;
	}
	logic.noteRadarEvent(type, pos, GameLogic::RadarEventNotice::SCRIPT, -1);
}

// RW 0x7C595A (ZH ScriptActions::doCreateObject): CREATE_OBJECT (RW 0x7CBCE0: no name, RW 0xDC62B8) and UNIT_SPAWN_NAMED_LOCATION_ORIENTATION
// (RW 0x7CE45B: the name parameter): a living object of that name refuses (the warning RW 0xC362FC); the team by name, made when missing
// (RW 0x759FDA with 1, RW 0x7C597E); the template (RW 0x6D1305; none: nothing); the new object (RW 0x6D165E) named before it enters the
// cache (RW 0x60A1D5); setOrientation(angle) (RW 0x70C31E), setPosition (RW 0x70C201) and the pathfinder (RW 0x6902F5) through the host
void createOnTeamAtPosition(ScriptEngine &engine, const std::string &name, const std::string &templateName, const std::string &teamName,
	const Coord3D &pos, float angle)
{
	GameLogic &logic = engine.logic();
	Object *existing = name.empty() ? nullptr : engine.getUnitNamed(name);
	if (existing && !existing->isEffectivelyDead())
	{
		engine.note("WARNING: Object with name " + name + " already exists.  Failed Create.");
		return;
	}
	Team *team = ScriptConditions::team(engine, teamName, true);
	if (!team)
	{
		engine.note("***WARNING: Team not found:*** " + teamName);
		return;
	}
	const ThingTemplate *tt = logic.things().findTemplate(templateName);
	if (!tt)
	{
		engine.note("***WARNING: object template not found:*** " + templateName);
		return;
	}
	if (!engine.host())
	{
		engine.note("[S-1180] no object creation host: " + templateName);
		return;
	}
	Object *obj = engine.host()->createObject(*tt, *team, pos, angle);
	if (obj)
	{
		obj->friend_onBuildComplete(); // RW 0x7C595A: onBuildComplete (RW 0x68D252) after the position (a horde makes its members there)
	}
	if (obj && !name.empty())
	{
		obj->setName(name);
	}
}

// RW 0x7C5B3B
void createOnTeamAtWaypoint(ScriptEngine &engine, const std::string &name, const std::string &templateName, const std::string &teamName,
	const std::string &waypointName)
{
	GameLogic &logic = engine.logic();
	Object *existing = engine.getUnitNamed(name);
	if (existing && !existing->isEffectivelyDead())
	{
		engine.note("WARNING: Object with name " + name + " already exists.  Failed Create.");
		return;
	}
	Team *team = ScriptConditions::team(engine, teamName, true); // lane CAMP-1H: RW 0x7C5BC1 passes 1 (a missing team is made, an inactive one activated)
	if (!team)
	{
		engine.note("***WARNING: Team not found:*** " + teamName);
		return;
	}
	Coord3D spawnPos{ 0.0f, 0.0f, 0.0f };
	if (const Waypoint *wp = logic.terrain() ? logic.terrain()->findWaypointByName(waypointName) : nullptr)
	{
		spawnPos = wp->location;
	}
	else
	{
		// RW 0x7C5C36 .. 0x7C5CD5: when waypointName is not a waypoint, resolve it as a ThingTemplate (RW 0x6D1305) and find the closest
		// equivalent object (filter RW 0xC3603C -> RW 0x66114D -> RW 0x73D5C2) to Team::getApproximatePosition (RW 0x7A0ECD) within 1e6 (FROM_CENTER_2D, RW 0xA39090).
		Object *atObj = nullptr;
		if (const ThingTemplate *atTmpl = logic.things().findTemplate(waypointName))
		{
			Coord3D centre{ 0.0f, 0.0f, 0.0f };
			int n = 0;
			for (Object *m : ScriptConditions::members(*team))
			{
				centre.x = SimMath::sseAdd(centre.x, m->getPosition()->x);
				centre.y = SimMath::sseAdd(centre.y, m->getPosition()->y);
				centre.z = SimMath::sseAdd(centre.z, m->getPosition()->z);
				++n;
			}
			if (n > 0)
			{
				const float inv = SimMath::sseDiv(1.0f, SimMath::sseFromInt32(n));
				centre.x = SimMath::sseMul(inv, centre.x);
				centre.y = SimMath::sseMul(inv, centre.y);
				centre.z = SimMath::sseMul(inv, centre.z);
			}
			auto typed = [atTmpl](Object &o) { return ScriptConditions::equivalentTemplates(o.getTemplate(), atTmpl); };
			PartitionFilterFn<decltype(typed)> fTyped(typed);
			atObj = logic.partition().getClosestObject(centre, 1000000.0f, FROM_CENTER_2D, { &fTyped });
		}
		if (!atObj)
		{
			engine.note("***WARNING: Waypoint/Object type not found:*** " + waypointName);
			return;
		}
		spawnPos = *atObj->getPosition();
	}
	const ThingTemplate *tt = logic.things().findTemplate(templateName);
	if (!tt)
	{
		engine.note("***WARNING: object template not found:*** " + templateName);
		return;
	}
	if (!engine.host())
	{
		engine.note("[S-1180] no object creation host: " + templateName);
		return;
	}
	// the name goes on before the object enters the cache (RW 0x7C5D30: + 0x88, then RW 0x60A1D5); the host makes the full object (ZH
	// ScriptActions::doCreateObject: newObject, position, pathfinder registration)
	Object *obj = engine.host()->createObject(*tt, *team, spawnPos, 0.0f);
	if (obj)
	{
		obj->friend_onBuildComplete(); // RW 0x7C5B3B: onBuildComplete (RW 0x68D252) after the position
	}
	if (obj && !name.empty())
	{
		obj->setName(name); // the cache entry (Object::setName -> ScriptEngine::objectNamed)
	}
}

} // namespace

bool ScriptActions::isClientAction(int o)
{
	return clientActions().count(o) != 0;
}

void ScriptActions::clientRequest(ScriptEngine &engine, const ScriptActionRec &a)
{
	ScriptClientRequest r;
	r.action = ScriptTemplates::action(a.resolved)->name;
	r.params = a.params;
	if (r.action == "FOCAL_LENGTH_CAMERA")
	{
		// lane PORT-439: RW 0x7CB6FB reads param 0's real (the focal length in millimetres) and hands it to RW 0x7BB627's view call
		r.focalMm = a.params.empty() ? 0.0f : a.params[0].realValue;
	}
	if (r.action == "HERO_SELECT_BUTTON_FLASH")
	{
		if (a.params.size() != 2)
		{
			engine.logic().reportError("HERO_SELECT_BUTTON_FLASH requires a template name and integer seconds");
			return;
		}
		// RW 0x7BDD7A: signed 32-bit IMUL by the 30 Hz client rate, not the 5 Hz logic rate.
		const std::uint32_t bits = 30u * (std::uint32_t)a.params[1].intValue;
		const int frames = (int)(bits <= 0x7FFFFFFFu ? (std::int64_t)bits : (std::int64_t)bits - 0x100000000LL);
		if (ObjectClientHooks *hooks = engine.logic().clientHooks())
		{
			hooks->flashHeroButton(a.params[0].stringValue, frames);
		}
	}
	for (const ScriptParameter &p : a.params)
	{
		if ((p.type == PARAM_WAYPOINT || p.type == PARAM_CAMERA_POSITION) && !r.hasPosition)
		{
			r.hasPosition = positionOf(engine, p.stringValue, r.position, p.type == PARAM_CAMERA_POSITION);
		}
		if (p.type == 14 && r.objectId == 0) // a named unit: the client follows / sounds at it
		{
			if (const Object *o = engine.getUnitNamed(p.stringValue))
			{
				r.objectId = o->getID();
			}
		}
	}
	engine.addClientRequest(std::move(r));
}

// lane CAMPUI-1: the objective actions on TheMissionObjectiveTracker (MissionObjectiveTracker.h): SHOW / HIDE RW 0x7BDE35 / 0x7BDE57 -> setShown RW 0x835FAE,
// MARK_..._COMPLETED / _NOT_COMPLETED RW 0x7BDE79 / 0x7BDE9B -> setCompleted RW 0x836097 (the parameter minus one); ENABLE / DISABLE_OBJECTIVES_SCREEN
// RW 0x7BDF1C -> RW 0x8E89BD. The sound of a false -> true change (the local player's PlayerTemplate ObjectiveAddedSound + 0x1A8 / ObjectiveCompletedSound
// + 0x1AC, the latter only after logic frame 3) goes to the client as MISSION_OBJECTIVE_SOUND(event name). Every one is also a client request as before
// (the presentation: the Palantir's flash, the screen)
static bool missionObjectiveAction(ScriptEngine &engine, const ScriptActionRec &a, const std::string &name)
{
	GameLogic &logic = engine.logic();
	MissionObjectiveTracker &tracker = logic.missionObjectives();
	if (name == "ENABLE_OBJECTIVES_SCREEN" || name == "DISABLE_OBJECTIVES_SCREEN")
	{
		tracker.setScreenEnabled(name == "ENABLE_OBJECTIVES_SCREEN");
		return true;
	}
	const bool show = name == "SHOW_MISSION_OBJECTIVE", hide = name == "HIDE_MISSION_OBJECTIVE";
	const bool done = name == "MARK_MISSION_OBJECTIVE_COMPLETED", undone = name == "MARK_MISSION_OBJECTIVE_NOT_COMPLETED";
	if (!show && !hide && !done && !undone)
	{
		return false;
	}
	if (!tracker.hasList() || a.params.empty())
	{
		return true; // RW 0x7BDE35 ..: no tracker list, nothing
	}
	const int index = a.params[0].intValue - 1;
	const bool changed = (show || hide) ? tracker.setShown(index, show) : tracker.setCompleted(index, done);
	if (!changed || (done && logic.getFrame() <= 3))
	{
		return true;
	}
	const Player *local = logic.players().getLocalPlayer();
	const PlayerTemplate *pt = local ? local->getPlayerTemplate() : nullptr;
	const std::string &sound = pt ? (show ? pt->m_objectiveAddedSound : pt->m_objectiveCompletedSound) : std::string();
	if (!sound.empty())
	{
		ScriptClientRequest r;
		r.action = "MISSION_OBJECTIVE_SOUND";
		ScriptParameter p;
		p.type = 12; // a sound name (PLAY_SOUND_EFFECT's parameter type)
		p.stringValue = sound;
		r.params.push_back(p);
		engine.addClientRequest(std::move(r));
	}
	return true;
}

// RW 0x7CAFA5
void ScriptActions::execute(ScriptEngine &engine, const ScriptActionRec &a)
{
	if (!a.enabled)
	{
		return; // RW 0x7CAFB7: + 0x41 == 0
	}
	const ScriptTemplate *t = ScriptTemplates::action(a.resolved);
	if (!t || !t->retailCase)
	{
		return; // retail's default: nothing
	}
	if (missionObjectiveAction(engine, a, t->name) && !isClientAction(a.resolved))
	{
		return; // lane CAMPUI-1: ENABLE / DISABLE_OBJECTIVES_SCREEN are logic only (their flag is saved, CHUNK_ObjectivesMenu)
	}
	if (isClientAction(a.resolved))
	{
		clientRequest(engine, a);
		return;
	}
	GameLogic &logic = engine.logic();
	const std::string &name = t->name;
	if (name == "VICTORY" || name == "QUICKVICTORY")
	{
		engine.addEndRequest(true, name); // RW 0x7C45E0 / 0x7BE0A2 (the client's end screen)
		// lane BFME2CAMP1: RW 0x7C45F8 / 0x7BE0C6: TheLinearCampaignManager RW 0x5EC0BA -> a current campaign's onCurrentMissionWon RW 0x5EBAEE (every
		// call: a second VICTORY records the survivors again)
		if (CampaignCarryover::State *carry = logic.campaignCarryover())
		{
			CampaignCarryover::onCurrentMissionWon(logic, logic.players(), *carry);
		}
		return;
	}
	if (name == "DEFEAT")
	{
		engine.addEndRequest(false, name); // RW 0x7BF15E
		return;
	}
	if (name == "DEBUG_MESSAGE_BOX" || name == "DEBUG_STRING")
	{
		// LOCAL-94 (issue #720): case 0 (RW 0x7CB007) / case 104 (RW 0x7CB017) -> RW 0x604D85 (DONOR BFME2 0x00205263, ScriptEngine::AppendDebugMessage):
		// tests [0xDE87B8] (ScriptDebugMessagesDisabled) and [0xDE3B98] (TheScriptDebugWindowDLL, always null in retail 2.01), returning immediately.
		return;
	}
	if (name == "OPTIONS_SET_OCCLUSION_MODE")
	{
		// lane OCCLUDE-1: case 298 (RW 0x7CE207): TheGameLogic + 0x99 = (param 0's int (+ 8) != 0) through RW 0x7BD549; the renderer's occluded-object
		// colours read it (GameLogic::getShowBehindBuildingMarkers)
		logic.setShowBehindBuildingMarkers(param(a, 0).intValue != 0);
		return;
	}
	if (name == "SET_HERO_EXPERIENCE_SHARING")
	{
		// PORT-301: case 538 (RW 0x7CDF99): TheGameLogic + 0x9F = (param 0's int (+ 8) != 0) through RW 0x7BD3D1; ShareExperienceBehavior::share reads it
		// (RW 0x883428) and skips a HERO candidate while it is clear
		logic.setHeroExperienceSharing(param(a, 0).intValue != 0);
		return;
	}
	if (name == "OPTIONS_SET_DRAWICON_UI_MODE")
	{
		// LOCAL-94 (issue #720): case 300 (RW 0x7CE222) -> RW 0x7BD55C (DONOR BFME2 0x003BCA7B): TheGameLogic + 0x9A = (param 0's int != 0),
		// read by Drawable::drawIconUI (RW 0x679134).
		logic.setShowDrawableIconUI(param(a, 0).intValue != 0);
		return;
	}
	if (name == "OPTIONS_SET_PARTICLE_CAP_MODE")
	{
		// LOCAL-94 (issue #720): case 301 (RW 0x7CE2B6) -> RW 0x7BD56F (DONOR BFME2 0x003BCA8E): TheGameLogic + 0x9B = (param 0's int != 0),
		// read by GameLogic::getParticleCapMode (RW 0x441B52 at RW 0x4438ED).
		logic.setParticleCapMode(param(a, 0).intValue != 0);
		return;
	}
	if (name == "PLAYER_REPAIR_NAMED_STRUCTURE")
	{
		// LOCAL-94 (issue #720): case 257 (RW 0x7CC332) -> RW 0x7C0402 (DONOR BFME2 0x003BF96F, ScriptActions::doPlayerRepairNamedStructure):
		// resolves the named unit (RW 0x75A243) and the first player of the mask (RW 0x758F7C / 0x6A85EE), then calls Player::repairStructure
		// (RW 0x6AA671 -> AIPlayer::repairStructure RW 0x8F78EB).
		Object *obj = engine.getUnitNamed(param(a, 1).stringValue);
		Player *player = obj ? ScriptConditions::firstPlayer(engine, param(a, 0).stringValue) : nullptr;
		if (player)
		{
			player->repairStructure(logic, obj->getID());
		}
		return;
	}
	if (name == "RADAR_CREATE_EVENT" || name == "OBJECT_CREATE_RADAR_EVENT" || name == "TEAM_CREATE_RADAR_EVENT")
	{
		radarEventAction(engine, a, name);
		return;
	}
	if (name == "PLAYER_ADD_SKILLPOINTS") // case 276: RW 0x7CDF75 -> RW 0x7BD063(param 0 player, param 1 int) -> RW 0x782AA4(float, false)
	{
		const float pts = SimMath::sseFromInt32(param(a, 1).intValue); // cvtsi2ss (RW 0x7BD090)
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			p->science().addSkillPoints(pts, false);
		}
		return;
	}
	if (name == "PLAYER_ADD_RANKLEVEL") // case 277: RW 0x7CDFB4 -> RW 0x7BD0AF(param 0 player, param 1 int) -> vslot 1 (RW 0x6AA9C2 / RW 0x782865), RW 0x71C311
	{
		const int delta = param(a, 1).intValue;
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			p->science().setRankLevel(wrapAdd(p->science().getRankLevel(), delta)); // RW add: wraps
			p->science().setScalarBaseRank(p->science().getRankLevel()); // RW 0x71C311 (+0x18 = +0x14)
		}
		return;
	}
	if (name == "PLAYER_SET_RANKLEVEL") // case 278: RW 0x7CDFD8 -> RW 0x7BD107(param 0 player, param 1 int) -> vslot 1 (RW 0x6AA9C2 / RW 0x782865), RW 0x71C311
	{
		const int level = param(a, 1).intValue;
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			p->science().setRankLevel(level);
			p->science().setScalarBaseRank(p->science().getRankLevel()); // RW 0x71C311 (+0x18 = +0x14)
		}
		return;
	}
	if (name == "PLAYER_SET_RANKLEVELLIMIT") // PORT-43: case 279, RW 0x7CE020 -> RW 0x7BD1A8 -> RW 0x7BB04E(param 0 int)
	{
		logic.setRankLevelLimit(param(a, 0).intValue);
		return;
	}
	if (name == "PLAYER_GRANT_SCIENCE" || name == "PLAYER_PURCHASE_SCIENCE") // RW 0x7CE037 / 0x7CE05C -> RW 0x7BD1BA / 0x7BD210(param 0 player, param 1 science)
	{
		// TheScienceStore by name (RW 0x5FEF8F; an unknown name does nothing), then every player of the parameter: grantScience (RW 0x6AE3A7) or
		// attemptToPurchaseScience (RW 0x6AE36F)
		const ScienceType st = TheScienceStore ? TheScienceStore->getScienceFromInternalName(param(a, 1).stringValue) : SCIENCE_INVALID;
		if (st == SCIENCE_INVALID)
		{
			return;
		}
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			if (name == "PLAYER_GRANT_SCIENCE")
			{
				p->science().grantScience(st);
			}
			else
			{
				p->science().attemptToPurchaseScience(st);
			}
		}
		return;
	}
	if (name == "OBJECTLIST_ADDOBJECTTYPE" || name == "OBJECTLIST_REMOVEOBJECTTYPE") // RW 0x7CE11A / 0x7CE11E -> RW 0x759D77(param 0, param 1, add)
	{
		engine.objectListAdd(param(a, 0).stringValue, param(a, 1).stringValue, name == "OBJECTLIST_ADDOBJECTTYPE");
		return;
	}
	if (name == "CREATE_NAMED_ON_TEAM_AT_WAYPOINT" || name == "CREATE_NAMED_ON_TEAM_AT_OBJECTTYPE") // RW 0x7CBD6F -> RW 0x7C5B3B
	{
		createOnTeamAtWaypoint(engine, param(a, 0).stringValue, param(a, 1).stringValue, param(a, 2).stringValue, param(a, 3).stringValue);
		return;
	}
	if (name == "CREATE_OBJECT") // RW 0x7CBCE0 -> RW 0x7C595A("", param 0 type, param 1 team, param 2 position, param 3 angle)
	{
		const ScriptParameter &at = param(a, 2);
		createOnTeamAtPosition(engine, std::string(), param(a, 0).stringValue, param(a, 1).stringValue, Coord3D{ at.x, at.y, at.z }, param(a, 3).realValue);
		return;
	}
	if (name == "UNIT_SPAWN_NAMED_LOCATION_ORIENTATION") // RW 0x7CE45B -> RW 0x7C595A(param 0 name, param 1 type, param 2 team, param 3, param 4)
	{
		const ScriptParameter &at = param(a, 3);
		createOnTeamAtPosition(engine, param(a, 0).stringValue, param(a, 1).stringValue, param(a, 2).stringValue, Coord3D{ at.x, at.y, at.z }, param(a, 4).realValue);
		return;
	}
	if (name == "CREATE_UNNAMED_ON_TEAM_AT_WAYPOINT")
	{
		createOnTeamAtWaypoint(engine, std::string(), param(a, 0).stringValue, param(a, 1).stringValue, param(a, 2).stringValue);
		return;
	}
	if (name == "PLAYER_SET_MONEY") // RW 0x7BCAFB (player, amount)
	{
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			Money *m = p->getMoney();
			m->withdraw(m->countMoney(), nullptr, true);
			m->deposit((std::uint32_t)param(a, 1).intValue, nullptr, true);
		}
		return;
	}
	if (name == "PLAYER_GIVE_MONEY") // RW 0x7BCB5A (player, amount)
	{
		const std::int32_t amount = param(a, 1).intValue;
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			if (amount < 0)
			{
				p->getMoney()->withdraw((std::uint32_t)(-amount), nullptr, true);
			}
			else
			{
				p->getMoney()->deposit((std::uint32_t)amount, nullptr, true);
			}
		}
		return;
	}
	if (name == "PLAYER_RELATES_PLAYER") // RW 0x7CC779: RW 0x7BC1C6(param 0, param 2's int, param 1): the first player of parameter 0 relates to parameter 1's
	{
		Player *p1 = ScriptConditions::firstPlayer(engine, param(a, 0).stringValue);
		Player *p2 = ScriptConditions::firstPlayer(engine, param(a, 1).stringValue);
		const int r = param(a, 2).intValue; // RELATION: 0 enemy, 1 neutral, 2 friend (ZH Parameter::RELATION)
		if (p1 && p2 && r >= ENEMIES && r <= ALLIES)
		{
			p1->setPlayerRelationship(p2, (Relationship)r);
		}
		return;
	}
	if (name == "UNIT_SET_TEAM") // RW 0x7BF794 (the AI's team hook, vslot 0x54, is not ported)
	{
		Object *o = engine.getUnitNamed(param(a, 0).stringValue);
		Team *tm = ScriptConditions::team(engine, param(a, 1).stringValue, true); // lane CAMP-1H: RW 0x7BF7BF passes 1 (the team made / activated)
		if (o && tm)
		{
			o->setTeam(tm);
		}
		return;
	}
	if (name == "PLAYER_TRANSFER_OWNERSHIP_PLAYER")
	{
		// lane BFME2CAMP1: RW 0x7BC0C3 (from, to): each parameter's player by its exact mask (RW 0x758F7C -> RW 0x6A85B6); both: the second takes the
		// first's assets, transferAssetsFromThat(this = to, that = from, inherited 0) (RW 0x6AF598, GameLogic/SelfDestruct.h)
		Player *to = ScriptConditions::playerOfMask(engine, param(a, 1).stringValue);
		Player *from = ScriptConditions::playerOfMask(engine, param(a, 0).stringValue);
		if (to && from)
		{
			SelfDestruct::transferAssetsFromThat(logic, *to, *from, false);
		}
		return;
	}
	if (name == "TEAM_TRANSFER_TO_PLAYER") // RW 0x7C9B79
	{
		Team *tm = ScriptConditions::team(engine, param(a, 0).stringValue);
		Player *p = ScriptConditions::firstPlayer(engine, param(a, 1).stringValue);
		if (tm && p && p->getDefaultTeam())
		{
			// INFERENCE (S-1180): retail gives the TEAM to the player (RW 0x7A00DA); the port moves its members to the player's default team, which
			// runs the same owner change per object; the members then go idle (RW 0x5E821A)
			engine.note("[S-1180] TEAM_TRANSFER_TO_PLAYER moves the members to the player's default team");
			for (Object *o : ScriptConditions::members(*tm))
			{
				o->setTeam(p->getDefaultTeam());
				AICommand idle;
				idle.type = AICMD_IDLE;
				idle.source = CMD_FROM_SCRIPT;
				idle.object = o->getID();
				idle.frame = logic.getFrame();
				logic.aiCommands().issue(*o, idle);
			}
		}
		return;
	}
	if (name == "NAMED_DELETE") // RW 0x7BBEF7: the object is removed without dying
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			logic.destroyObject(o);
		}
		return;
	}
	if (name == "NAMED_KILL") // RW 0x7BBF53
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			if (!o->isEffectivelyDead())
			{
				o->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL); // RW 0x7BBF6C: kill(8, 0)
			}
		}
		return;
	}
	if (name == "TEAM_KILL" || name == "TEAM_DELETE" || name == "TEAM_DELETE_LIVING") // RW 0x7C07ED / RW 0x7C0556
	{
		// LOCAL-162: RW 0x7CAFA5 case 73 (TEAM_DELETE, push 0) / case 330 (TEAM_DELETE_LIVING, push 1) -> RW 0x7C0556 -> RW 0x7A3F77 (Team::deleteTeam):
		// RW 0x7A4040..0x7A4056 skips isEffectivelyDead() (+0x458 bit 0) when ignoreDead != 0, then destroys each member.
		const bool ignoreDead = (name == "TEAM_DELETE_LIVING");
		if (Team *tm = ScriptConditions::team(engine, param(a, 0).stringValue))
		{
			for (Object *o : ScriptConditions::members(*tm))
			{
				if (name == "TEAM_DELETE" || ignoreDead)
				{
					if (!ignoreDead || !o->isEffectivelyDead())
					{
						logic.destroyObject(o);
					}
				}
				else if (!o->isEffectivelyDead())
				{
					o->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL); // RW 0x7BCEDA (the TEAM_KILL member callback): kill(8, 0)
				}
			}
		}
		return;
	}
	if (name == "DELETE_ALL_UNMANNED")
	{
		// LOCAL-162: RW 0x7CAFA5 case 332 -> RW 0x7C2A40: iterate all objects in TheGameLogic; destroy any with DISABLED_UNMANNED (Object +0x1C8 bit 0x20, type 5)
		std::vector<Object *> doomed;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (o->isDisabledByType(5u))
			{
				doomed.push_back(o);
			}
		}
		for (Object *o : doomed)
		{
			logic.destroyObject(o);
		}
		return;
	}
	if (name == "MUSIC_RESET_MUSIC_SCRIPTING_SYSTEM")
	{
		// LOCAL-162: RW 0x7CAFA5 case 461 -> RW 0x7C0FC5 (DONOR BFME2 ScriptActions::doResetMusicScripting): pauses audio (client) and sets
		// the script flag "/___MusicScript_Init" to true.
		engine.flag("/___MusicScript_Init") = true;
		ScriptActions::clientRequest(engine, a);
		return;
	}
	if (name == "MOVE_NAMED_UNIT_TO" || name == "MOVE_TEAM_TO") // RW 0x7C84CC / RW 0x7BF4A3: aiMoveToPosition from the script
	{
		Coord3D pos;
		if (!positionOf(engine, param(a, 1).stringValue, pos))
		{
			engine.note("***WARNING: Waypoint not found:*** " + param(a, 1).stringValue);
			return;
		}
		std::vector<Object *> objs;
		if (name == "MOVE_NAMED_UNIT_TO")
		{
			if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
			{
				objs.push_back(o);
			}
		}
		else if (Team *tm = ScriptConditions::team(engine, param(a, 0).stringValue))
		{
			objs = ScriptConditions::members(*tm);
		}
		for (Object *o : objs)
		{
			if (o->isEffectivelyDead())
			{
				continue;
			}
			AICommand move;
			move.type = AICMD_MOVE_TO_POSITION;
			move.source = CMD_FROM_SCRIPT;
			move.object = o->getID();
			move.path.push_back(pos);
			move.frame = logic.getFrame();
			logic.aiCommands().issue(*o, move);
		}
		return;
	}
	if (name == "NAMED_FACE_WAYPOINT") // RW 0x7CA509: the AI turns to face the waypoint (RW 0x7C821E)
	{
		Object *o = engine.getUnitNamed(param(a, 0).stringValue);
		Coord3D pos;
		if (o && positionOf(engine, param(a, 1).stringValue, pos))
		{
			// INFERENCE (S-1180): the face command is not an AI command here; the object turns at once
			engine.note("[S-1180] NAMED_FACE_WAYPOINT turns the object at once (retail: the AI's face-position command)");
			const Coord3D *p = o->getPosition();
			const double angle = SimMath::atan2d(SimMath::sseSub(pos.y, p->y), SimMath::sseSub(pos.x, p->x));
			o->setOrientation(SimMath::fstpDword(angle)); // the stored float
		}
		return;
	}
	// RW 0x7BBD79 (PLAYER_DISABLE_BASE_CONSTRUCTION), RW 0x7BBE38 (PLAYER_ENABLE_BASE_CONSTRUCTION),
	// RW 0x7BBDFA (PLAYER_DISABLE_UNIT_CONSTRUCTION), RW 0x7BBEB9 (PLAYER_ENABLE_UNIT_CONSTRUCTION)
	if (name == "PLAYER_DISABLE_BASE_CONSTRUCTION" || name == "PLAYER_ENABLE_BASE_CONSTRUCTION" || name == "PLAYER_DISABLE_UNIT_CONSTRUCTION" ||
		name == "PLAYER_ENABLE_UNIT_CONSTRUCTION")
	{
		const bool enable = name.find("ENABLE") == 7;
		const bool base = name.find("BASE") != std::string::npos;
		for (Player *p : ScriptConditions::players(engine, param(a, 0).stringValue))
		{
			if (base)
			{
				p->setCanBuildBase(enable);
			}
			else
			{
				p->setCanBuildUnits(enable);
			}
		}
		return;
	}
	// MAP_REVEAL_PERMANENTLY_IN_TRIGGER (case 554): RW 0x7BD4F3 -> createNamedMapReveal (RW 0x75A344) + doNamedMapReveal (RW 0x7592E1)
	if (name == "MAP_REVEAL_AT_WAYPOINT" || name == "MAP_REVEAL_PERMANENTLY_AT_WAYPOINT" ||
		name == "MAP_UNDO_REVEAL_PERMANENTLY_AT_WAYPOINT" || name == "MAP_REVEAL_IN_TRIGGER" ||
		name == "MAP_REVEAL_PERMANENTLY_IN_TRIGGER" || name == "MAP_UNDO_REVEAL_PERMANENTLY_IN_TRIGGER")
	{
		engine.executeMapReveal(a);
		return;
	}
	// Local 161 (fork issue 928): MAP_REVEAL_ALL (case 105, RW 0x7BC3FF), MAP_REVEAL_ALL_PERM / MAP_REVEAL_ALL_UNDO_PERM
	// (cases 233/234, RW 0x7BC479), MAP_SHROUD_ALL (case 149, RW 0x7BC50C): when the player mask is 0, RotWK iterates
	// every PlayerList entry with playerType == PLAYER_HUMAN (Player + 0x5C == 0) instead of doing nothing.
	if (name == "MAP_REVEAL_ALL" || name == "MAP_REVEAL_ALL_PERM" || name == "MAP_REVEAL_ALL_UNDO_PERM" || name == "MAP_SHROUD_ALL")
	{
		ShroudManager *shroud = logic.shroud();
		if (!shroud)
		{
			engine.note("[S-1180] " + name + ": the game has no shroud manager");
			return;
		}
		auto applyShroud = [&](int i) {
			if (name == "MAP_REVEAL_ALL") shroud->revealMapForPlayer(i);
			else if (name == "MAP_REVEAL_ALL_PERM") shroud->revealMapForPlayerPermanently(i);
			else if (name == "MAP_REVEAL_ALL_UNDO_PERM") shroud->undoRevealMapForPlayerPermanently(i);
			else shroud->shroudMapForPlayer(i);
		};
		std::vector<Player *> targets = ScriptConditions::players(engine, param(a, 0).stringValue);
		if (targets.empty())
		{
			PlayerList &plist = logic.players();
			for (int i = 0; i < plist.getPlayerCount(); ++i)
			{
				if (Player *p = plist.getNthPlayer(i); p && p->getPlayerType() == PLAYER_HUMAN)
				{
					applyShroud(i);
				}
			}
		}
		else
		{
			for (Player *p : targets)
			{
				applyShroud(p->getPlayerIndex());
			}
		}
		return;
	}
	if (name == "WAREHOUSE_SET_VALUE") // action 216, RW 0x7C191A (dispatched at RW 0x7CDC71)
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue)) // RW 0x75A243
		{
			if (auto *wh = dynamic_cast<SupplyWarehouseDockUpdate *>(o->findModule("SupplyWarehouseDockUpdate"))) // RW 0x68BDA5
			{
				wh->setCashValue(param(a, 1).intValue); // RW 0x8AB64E
			}
		}
		return;
	}
	if (name == "NAMED_SET_STRICT_CONTROL_ENABLED") // Local 161: case 432, RW 0x7BC814 -> Object::setScriptStatus(0x20, on) (RW 0x69317D)
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			o->setScriptStatus(0x20u, param(a, 1).intValue != 0);
		}
		return;
	}
	if (name == "NAMED_SET_HELD") // Local 161: case 229, RW 0x7BC9D6 -> Object::setDisabledUntil(DISABLED_HELD=3, held ? 0x3FFFFFFF : 0) (RW 0x6907F1)
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			if (param(a, 1).intValue != 0)
			{
				o->setDisabled(DISABLED_HELD, 0x3FFFFFFFu);
			}
			else
			{
				o->clearDisabled(DISABLED_HELD);
			}
		}
		return;
	}
	if (name == "UNIT_SET_EXPERIENCE_POINTS") // Local 161: case 360, RW 0x7BD41D -> ExperienceTracker::setExperienceAndLevel((float)pts, true) (RW 0x79D8EF)
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			if (ExperienceTracker *xp = o->getExperienceTracker())
			{
				xp->setExperienceAndLevel(static_cast<float>(param(a, 1).intValue), true);
			}
		}
		return;
	}
	if (name == "UNIT_GAIN_LEVEL") // Local 161: case 523, RW 0x7BDE10 -> ExperienceTracker::gainLevel(feedback) (RW 0x79D7AD)
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			if (ExperienceTracker *xp = o->getExperienceTracker())
			{
				xp->gainLevel(param(a, 1).intValue != 0);
			}
		}
		return;
	}
	if (name == "NAMED_SET_SPECIAL_WEAPONSET") // Local 161: case 431, RW 0x7BDB15 -> Object::setWeaponSetFlag(0x17, on) / clearWeaponSetFlag (RW 0x691059 / RW 0x691106)
	{
		if (Object *o = engine.getUnitNamed(param(a, 0).stringValue))
		{
			o->setWeaponSetFlag(0x17, param(a, 1).intValue > 0);
		}
		return;
	}
	if (name == "DAMAGE_MEMBERS_OF_TEAM") // Local 161: case 12, RW 0x7BF46E -> Team::damageTeamMembers(amount) (RW 0x7A1080)
	{
		if (Team *t = ScriptConditions::team(engine, param(a, 0).stringValue))
		{
			const float amount = param(a, 1).realValue;
			std::vector<Object *> members;
			for (Object *o = t->getFirstMember(); o; o = o->friend_teamNext())
			{
				members.push_back(o);
			}
			for (Object *o : members)
			{
				if (!o || o->isDestroyed() || o->isEffectivelyDead())
				{
					continue;
				}
				if (amount < 0.0f)
				{
					o->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL); // RW 0x7A10BA -> RW 0x698EC3
				}
				else
				{
					DamageInfo d;
					d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
					d.m_input.m_deathType = DEATH_NORMAL;
					d.m_input.m_amount = amount;
					o->attemptDamage(d); // RW 0x7A10E8 -> RW 0x698E7D
				}
			}
		}
		return;
	}
	if (executeUnitAction(engine, a, name) || executeOrderAction(engine, a, name) || executeCampaignAction(engine, a, name) || executeSiegeAction(engine, a, name) ||
		executeThreatFinderAction(engine, a, name)) // PORT 17 (issue #73): case 556, RW 0x7C68AA
	{
		return;
	}
	engine.noteUnportedAction(name);
}
