// OpenBFME. GPL-3.0.
// See GameClient/EnnorathOptions.h (fork issue 811, lane MODS-3).

#include "GameClient/EnnorathOptions.h"

#include "Common/ModEngineFeatures.h"
#include "GameClient/GUI/GameTextSource.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/MessageStream/MetaEvent.h"
#include "GameClient/OptionPreferences.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{

std::u16string utf8ToU16(const std::string &s)
{
	std::u16string out;
	out.reserve(s.size());
	for (std::size_t i = 0; i < s.size();)
	{
		const unsigned char c = static_cast<unsigned char>(s[i]);
		if (c < 0x80)
		{
			out.push_back(static_cast<char16_t>(c));
			++i;
		}
		else if ((c & 0xE0) == 0xC0 && i + 1 < s.size())
		{
			const unsigned int cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
			out.push_back(static_cast<char16_t>(cp));
			i += 2;
		}
		else if ((c & 0xF0) == 0xE0 && i + 2 < s.size())
		{
			const unsigned int cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
			out.push_back(static_cast<char16_t>(cp));
			i += 3;
		}
		else
		{
			out.push_back(u'?');
			++i;
		}
	}
	return out;
}

bool parseNonNegDecimal(const std::string &s, int &out)
{
	// PYSAGE 1.9.8.5 .optui 0xF232EA .. 0xF23311: unsigned digits, <= 100000
	if (s.empty())
	{
		return false;
	}
	int acc = 0;
	for (char c : s)
	{
		if (c < '0' || c > '9' || acc > 100000)
		{
			return false;
		}
		acc = acc * 10 + (c - '0');
	}
	out = acc;
	return true;
}

const char *scanCodeName(int keyCode)
{
	// PYSAGE 1.9.8.5 .optfix 0xF3037E .. 0xF304A1 / 0xF304C0 table
	switch (keyCode)
	{
		case KEY_ESC: return "Esc";
		case KEY_1: return "1";
		case KEY_2: return "2";
		case KEY_3: return "3";
		case KEY_4: return "4";
		case KEY_5: return "5";
		case KEY_6: return "6";
		case KEY_7: return "7";
		case KEY_8: return "8";
		case KEY_9: return "9";
		case KEY_0: return "0";
		case KEY_MINUS: return "-";
		case KEY_EQUAL: return "=";
		case KEY_BACKSPACE: return "Backspace";
		case KEY_TAB: return "Tab";
		case KEY_Q: return "Q";
		case KEY_W: return "W";
		case KEY_E: return "E";
		case KEY_R: return "R";
		case KEY_T: return "T";
		case KEY_Y: return "Y";
		case KEY_U: return "U";
		case KEY_I: return "I";
		case KEY_O: return "O";
		case KEY_P: return "P";
		case KEY_LBRACKET: return "[";
		case KEY_RBRACKET: return "]";
		case KEY_ENTER: return "Enter";
		case KEY_LCTRL: return "Ctrl";
		case KEY_A: return "A";
		case KEY_S: return "S";
		case KEY_D: return "D";
		case KEY_F: return "F";
		case KEY_G: return "G";
		case KEY_H: return "H";
		case KEY_J: return "J";
		case KEY_K: return "K";
		case KEY_L: return "L";
		case KEY_SEMICOLON: return ";";
		case KEY_APOSTROPHE: return "'";
		case KEY_TICK: return "`";
		case KEY_LSHIFT: return "Shift";
		case KEY_BACKSLASH: return "\\";
		case KEY_Z: return "Z";
		case KEY_X: return "X";
		case KEY_C: return "C";
		case KEY_V: return "V";
		case KEY_B: return "B";
		case KEY_N: return "N";
		case KEY_M: return "M";
		case KEY_COMMA: return ",";
		case KEY_PERIOD: return ".";
		case KEY_SLASH: return "/";
		case KEY_RSHIFT: return "Shift";
		case KEY_KPSTAR: return "Num *";
		case KEY_LALT: return "Alt";
		case KEY_SPACE: return "Space";
		case KEY_F1: return "F1";
		case KEY_F2: return "F2";
		case KEY_F3: return "F3";
		case KEY_F4: return "F4";
		case KEY_F5: return "F5";
		case KEY_F6: return "F6";
		case KEY_F7: return "F7";
		case KEY_F8: return "F8";
		case KEY_F9: return "F9";
		case KEY_F10: return "F10";
		case KEY_KP7: return "Num 7";
		case KEY_KP8: return "Num 8";
		case KEY_KP9: return "Num 9";
		case KEY_KPMINUS: return "Num -";
		case KEY_KP4: return "Num 4";
		case KEY_KP5: return "Num 5";
		case KEY_KP6: return "Num 6";
		case KEY_KPPLUS: return "Num +";
		case KEY_KP1: return "Num 1";
		case KEY_KP2: return "Num 2";
		case KEY_KP3: return "Num 3";
		case KEY_KP0: return "Num 0";
		case KEY_KPDOT: return "Num .";
		case KEY_F11: return "F11";
		case KEY_F12: return "F12";
		case KEY_KPENTER: return "Num Enter";
		case KEY_KPSLASH: return "Num /";
		case KEY_HOME: return "Home";
		case KEY_UP: return "\xE2\x86\x91";
		case KEY_PGUP: return "Page Up";
		case KEY_LEFT: return "\xE2\x86\x90";
		case KEY_RIGHT: return "\xE2\x86\x92";
		case KEY_END: return "End";
		case KEY_DOWN: return "\xE2\x86\x93";
		case KEY_PGDN: return "Page Down";
		case KEY_INS: return "Insert";
		case KEY_DEL: return "Delete";
		default: return nullptr;
	}
}

} // namespace

const std::array<EnnorathOptionEntry, EnnorathOptions::kEntryCount> &EnnorathOptions::entries()
{
	// PYSAGE 1.9.8.5 .optui 0xF25960 (28 entries, stride 0x24)
	using K = EnnorathOptionKind;
	static const std::array<EnnorathOptionEntry, kEntryCount> kTable = { {
		{ "ETFrameRate",        "ETFrameRate",        1, K::FrameRate,        60,  0, 0 },
		{ "ETMouseSensitivity", "ETMouseSensitivity", 3, K::MouseSensitivity, 100, 0, 0 },
		{ "ETKeyCamLeft",       "ETKeyCamLeft",       3, K::Hotkey,           0,   CMSG_META_BEGIN_CAMERA_ROTATE_LEFT,  CMSG_META_END_CAMERA_ROTATE_LEFT },
		{ "ETKeyCamRight",      "ETKeyCamRight",      3, K::Hotkey,           0,   CMSG_META_BEGIN_CAMERA_ROTATE_RIGHT, CMSG_META_END_CAMERA_ROTATE_RIGHT },
		{ "ETKeyZoomIn",        "ETKeyZoomIn",        3, K::Hotkey,           0,   CMSG_META_BEGIN_CAMERA_ZOOM_IN,      CMSG_META_END_CAMERA_ZOOM_IN },
		{ "ETKeyZoomOut",       "ETKeyZoomOut",       3, K::Hotkey,           0,   CMSG_META_BEGIN_CAMERA_ZOOM_OUT,     CMSG_META_END_CAMERA_ZOOM_OUT },
		{ "ETKeyCameraReset",   "ETKeyCameraReset",   3, K::Hotkey,           0,   CMSG_META_CAMERA_RESET,              0 },
		{ "ETKeyBuilder",       "ETKeyBuilder",       3, K::Hotkey,           0,   CMSG_META_SELECT_NEXT_WORKER,        0 },
		{ "ETKeyStop",          "ETKeyStop",          3, K::Hotkey,           0,   CMSG_META_STOP,                      0 },
		{ "ETKeyAttackMove",    "ETKeyAttackMove",    3, K::Hotkey,           0,   CMSG_META_TOGGLE_ATTACKMOVE,         0 },
		{ "ETKeyBuildPath",     "ETKeyMatchType",     3, K::Hotkey,           0,   CMSG_META_SELECT_MATCHING_UNITS,     0 },
		{ "ETKeySpellbook",     "ETKeySpellbook",     3, K::Hotkey,           0,   CMSG_META_SPELL_STORE,               0 },
		{ "ETKeySell",          "ETKeySell",          3, K::Hotkey,           0,   CMSG_META_SELL,                      0 },
		{ "Detail",             nullptr,              0, K::Retail,           0,   0, 0 },
		{ "Brightness",         nullptr,              0, K::Retail,           0,   0, 0 },
		{ "HealthBars",         nullptr,              0, K::Retail,           0,   0, 0 },
		{ "Resolution",         nullptr,              1, K::Retail,           0,   0, 0 },
		{ "ScrollSpeed",        nullptr,              3, K::Retail,           0,   0, 0 },
		{ "MusicVolume",        nullptr,              2, K::Retail,           0,   0, 0 },
		{ "SoundFxVolume",      nullptr,              2, K::Retail,           0,   0, 0 },
		{ "VoiceVolume",        nullptr,              2, K::Retail,           0,   0, 0 },
		{ "AmbientVolume",      nullptr,              2, K::Retail,           0,   0, 0 },
		{ "MovieVolume",        nullptr,              2, K::Retail,           0,   0, 0 },
		{ "EAX3",               nullptr,              2, K::Retail,           0,   0, 0 },
		{ "HighAudioQuality",   nullptr,              2, K::Retail,           0,   0, 0 },
		{ "OnlineIp",           nullptr,              4, K::Retail,           0,   0, 0 },
		{ "OnlinePortNum",      nullptr,              4, K::Retail,           0,   0, 0 },
		{ "SendDelay",          nullptr,              4, K::Retail,           0,   0, 0 },
		// PYSAGE 1.9.8.7 .wmkeys 0xF3B1D4 / 0xF3B1F8 / 0xF3B21C (active only under ennorath.option-hotkeys-1987)
		{ "ETKeySelectAll",       "ETKeySelectAll",       3, K::Hotkey,       0,   CMSG_META_SELECT_ALL,           0 },
		{ "ETKeySelectAllHeroes", "ETKeySelectAllHeroes", 3, K::Hotkey,       0,   CMSG_META_SELECT_ALL_HEROES,    0 },
		{ "ETKeyTacticalMenu",    "ETKeyTacticalMenu",    3, K::Hotkey,       0,   CMSG_META_TOGGLE_PLANNING_MODE, 0 },
	} };
	return kTable;
}

int EnnorathOptions::activeEntryCount()
{
	return ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys1987) ? kEntryCount : kEntryCount1985;
}

const std::array<EnnorathKeyChoice, EnnorathOptions::kKeyChoiceCount> &EnnorathOptions::keyChoices()
{
	// PYSAGE 1.9.8.5 .optui 0xF24640 (135 entries, stride 0x0C)
	static const std::array<EnnorathKeyChoice, kKeyChoiceCount> kKeys = { {
		{ KEY_NONE, 0, "Default" },
		{ KEY_A, 0, "A" }, { KEY_B, 0, "B" }, { KEY_C, 0, "C" }, { KEY_D, 0, "D" }, { KEY_E, 0, "E" },
		{ KEY_F, 0, "F" }, { KEY_G, 0, "G" }, { KEY_H, 0, "H" }, { KEY_I, 0, "I" }, { KEY_J, 0, "J" },
		{ KEY_K, 0, "K" }, { KEY_L, 0, "L" }, { KEY_M, 0, "M" }, { KEY_N, 0, "N" }, { KEY_O, 0, "O" },
		{ KEY_P, 0, "P" }, { KEY_Q, 0, "Q" }, { KEY_R, 0, "R" }, { KEY_S, 0, "S" }, { KEY_T, 0, "T" },
		{ KEY_U, 0, "U" }, { KEY_V, 0, "V" }, { KEY_W, 0, "W" }, { KEY_X, 0, "X" }, { KEY_Y, 0, "Y" }, { KEY_Z, 0, "Z" },
		{ KEY_F1, 0, "F1" }, { KEY_F2, 0, "F2" }, { KEY_F3, 0, "F3" }, { KEY_F4, 0, "F4" }, { KEY_F5, 0, "F5" }, { KEY_F6, 0, "F6" },
		{ KEY_F7, 0, "F7" }, { KEY_F8, 0, "F8" }, { KEY_F9, 0, "F9" }, { KEY_F10, 0, "F10" }, { KEY_F11, 0, "F11" }, { KEY_F12, 0, "F12" },
		{ KEY_HOME, 0, "Home" }, { KEY_END, 0, "End" }, { KEY_PGUP, 0, "Page Up" }, { KEY_PGDN, 0, "Page Down" },
		{ KEY_UP, 0, "\xE2\x86\x91" }, { KEY_DOWN, 0, "\xE2\x86\x93" }, { KEY_LEFT, 0, "\xE2\x86\x90" }, { KEY_RIGHT, 0, "\xE2\x86\x92" },
		{ KEY_KP0, 0, "Num 0" }, { KEY_KP1, 0, "Num 1" }, { KEY_KP2, 0, "Num 2" }, { KEY_KP3, 0, "Num 3" }, { KEY_KP4, 0, "Num 4" },
		{ KEY_KP5, 0, "Num 5" }, { KEY_KP6, 0, "Num 6" }, { KEY_KP7, 0, "Num 7" }, { KEY_KP8, 0, "Num 8" }, { KEY_KP9, 0, "Num 9" },
		{ KEY_A, MOD_CTRL, "Ctrl+A" }, { KEY_B, MOD_CTRL, "Ctrl+B" }, { KEY_C, MOD_CTRL, "Ctrl+C" }, { KEY_D, MOD_CTRL, "Ctrl+D" }, { KEY_E, MOD_CTRL, "Ctrl+E" },
		{ KEY_F, MOD_CTRL, "Ctrl+F" }, { KEY_G, MOD_CTRL, "Ctrl+G" }, { KEY_H, MOD_CTRL, "Ctrl+H" }, { KEY_I, MOD_CTRL, "Ctrl+I" }, { KEY_J, MOD_CTRL, "Ctrl+J" },
		{ KEY_K, MOD_CTRL, "Ctrl+K" }, { KEY_L, MOD_CTRL, "Ctrl+L" }, { KEY_M, MOD_CTRL, "Ctrl+M" }, { KEY_N, MOD_CTRL, "Ctrl+N" }, { KEY_O, MOD_CTRL, "Ctrl+O" },
		{ KEY_P, MOD_CTRL, "Ctrl+P" }, { KEY_Q, MOD_CTRL, "Ctrl+Q" }, { KEY_R, MOD_CTRL, "Ctrl+R" }, { KEY_S, MOD_CTRL, "Ctrl+S" }, { KEY_T, MOD_CTRL, "Ctrl+T" },
		{ KEY_U, MOD_CTRL, "Ctrl+U" }, { KEY_V, MOD_CTRL, "Ctrl+V" }, { KEY_W, MOD_CTRL, "Ctrl+W" }, { KEY_X, MOD_CTRL, "Ctrl+X" }, { KEY_Y, MOD_CTRL, "Ctrl+Y" }, { KEY_Z, MOD_CTRL, "Ctrl+Z" },
		{ KEY_A, MOD_ALT, "Alt+A" }, { KEY_B, MOD_ALT, "Alt+B" }, { KEY_C, MOD_ALT, "Alt+C" }, { KEY_D, MOD_ALT, "Alt+D" }, { KEY_E, MOD_ALT, "Alt+E" },
		{ KEY_F, MOD_ALT, "Alt+F" }, { KEY_G, MOD_ALT, "Alt+G" }, { KEY_H, MOD_ALT, "Alt+H" }, { KEY_I, MOD_ALT, "Alt+I" }, { KEY_J, MOD_ALT, "Alt+J" },
		{ KEY_K, MOD_ALT, "Alt+K" }, { KEY_L, MOD_ALT, "Alt+L" }, { KEY_M, MOD_ALT, "Alt+M" }, { KEY_N, MOD_ALT, "Alt+N" }, { KEY_O, MOD_ALT, "Alt+O" },
		{ KEY_P, MOD_ALT, "Alt+P" }, { KEY_Q, MOD_ALT, "Alt+Q" }, { KEY_R, MOD_ALT, "Alt+R" }, { KEY_S, MOD_ALT, "Alt+S" }, { KEY_T, MOD_ALT, "Alt+T" },
		{ KEY_U, MOD_ALT, "Alt+U" }, { KEY_V, MOD_ALT, "Alt+V" }, { KEY_W, MOD_ALT, "Alt+W" }, { KEY_X, MOD_ALT, "Alt+X" }, { KEY_Y, MOD_ALT, "Alt+Y" }, { KEY_Z, MOD_ALT, "Alt+Z" },
		{ KEY_A, MOD_SHIFT, "Shift+A" }, { KEY_B, MOD_SHIFT, "Shift+B" }, { KEY_C, MOD_SHIFT, "Shift+C" }, { KEY_D, MOD_SHIFT, "Shift+D" }, { KEY_E, MOD_SHIFT, "Shift+E" },
		{ KEY_F, MOD_SHIFT, "Shift+F" }, { KEY_G, MOD_SHIFT, "Shift+G" }, { KEY_H, MOD_SHIFT, "Shift+H" }, { KEY_I, MOD_SHIFT, "Shift+I" }, { KEY_J, MOD_SHIFT, "Shift+J" },
		{ KEY_K, MOD_SHIFT, "Shift+K" }, { KEY_L, MOD_SHIFT, "Shift+L" }, { KEY_M, MOD_SHIFT, "Shift+M" }, { KEY_N, MOD_SHIFT, "Shift+N" }, { KEY_O, MOD_SHIFT, "Shift+O" },
		{ KEY_P, MOD_SHIFT, "Shift+P" }, { KEY_Q, MOD_SHIFT, "Shift+Q" }, { KEY_R, MOD_SHIFT, "Shift+R" }, { KEY_S, MOD_SHIFT, "Shift+S" }, { KEY_T, MOD_SHIFT, "Shift+T" },
		{ KEY_U, MOD_SHIFT, "Shift+U" }, { KEY_V, MOD_SHIFT, "Shift+V" }, { KEY_W, MOD_SHIFT, "Shift+W" }, { KEY_X, MOD_SHIFT, "Shift+X" }, { KEY_Y, MOD_SHIFT, "Shift+Y" }, { KEY_Z, MOD_SHIFT, "Shift+Z" },
	} };
	return kKeys;
}

const std::array<int, EnnorathOptions::kFpsChoiceCount> &EnnorathOptions::fpsValues()
{
	// PYSAGE 1.9.8.5 .optui 0xF24CD4
	static const std::array<int, kFpsChoiceCount> kFps = { 30, 60 };
	return kFps;
}

const std::array<int, EnnorathOptions::kSensChoiceCount> &EnnorathOptions::sensitivityValues()
{
	// PYSAGE 1.9.8.5 .optui 0xF24CB8 (count 3 at 0xF23679 / 0xF231E3)
	static const std::array<int, kSensChoiceCount> kSens = { 50, 75, 100 };
	return kSens;
}

int EnnorathOptions::findEntryByGadget(const std::string &shortName)
{
	const auto &e = entries();
	const int n = activeEntryCount();
	for (int i = 0; i < n; ++i)
	{
		if (shortName == e[(std::size_t)i].gadgetName)
		{
			return i;
		}
	}
	return -1;
}

int EnnorathOptions::choiceCount(EnnorathOptionKind kind)
{
	switch (kind)
	{
		case EnnorathOptionKind::FrameRate: return kFpsChoiceCount;
		case EnnorathOptionKind::MouseSensitivity: return kSensChoiceCount;
		case EnnorathOptionKind::Hotkey: return kKeyChoiceCount;
		case EnnorathOptionKind::Retail: break;
	}
	return 0;
}

int EnnorathOptions::indexToValue(EnnorathOptionKind kind, int index)
{
	// PYSAGE 1.9.8.5 .optui 0xF231A5
	if (kind == EnnorathOptionKind::FrameRate && index >= 0 && index < kFpsChoiceCount)
	{
		return fpsValues()[(std::size_t)index];
	}
	if (kind == EnnorathOptionKind::MouseSensitivity && index >= 0 && index < kSensChoiceCount)
	{
		return sensitivityValues()[(std::size_t)index];
	}
	return index;
}

int EnnorathOptions::valueToIndex(EnnorathOptionKind kind, int value)
{
	// PYSAGE 1.9.8.5 .optui 0xF231C3
	const int n = choiceCount(kind);
	for (int i = 0; i < n; ++i)
	{
		if (indexToValue(kind, i) == value)
		{
			return i;
		}
	}
	return -1;
}

std::string EnnorathOptions::formatKeyCombo(int keyCode, int modState)
{
	// PYSAGE 1.9.8.5 .optfix 0xF30060 .. 0xF30360
	const char *name = scanCodeName(keyCode);
	if (!name)
	{
		return std::string();
	}
	std::string out;
	if (modState & MOD_CTRL)
	{
		out += "Ctrl+";
	}
	if (modState & MOD_ALT)
	{
		out += "Alt+";
	}
	if (modState & MOD_SHIFT)
	{
		out += "Shift+";
	}
	out += name;
	return out;
}

std::u16string EnnorathOptions::formatDefaultKeyLabel(int entryIndex, const MetaMap *metaMap, const GameTextSource *gameText)
{
	// PYSAGE 1.9.8.5 .optfix 0xF30000 -> .keydef 0xF391E6
	std::u16string suffix;
	if (!gameText || !gameText->fetch("APT:ETKeyDefaultSuffix", suffix) || suffix.empty())
	{
		suffix = u"default";
	}

	std::u16string base;
	if (entryIndex == kEntryBuilder)
	{
		base = u"B"; // PYSAGE 1.9.8.5 .keydef 0xF391FD -> .optui 0xF24660 ("B")
	}
	else if (entryIndex == kEntryMatchType)
	{
		base = u"E"; // PYSAGE 1.9.8.5 .keydef 0xF3920D -> .optui 0xF24684 ("E")
	}
	else if (entryIndex >= 0 && entryIndex < kEntryCount)
	{
		const EnnorathOptionEntry &e = entries()[(std::size_t)entryIndex];
		if (metaMap && e.meta1 != 0)
		{
			if (const MetaMapRec *rec = metaMap->find(e.meta1))
			{
				const std::string combo = formatKeyCombo(rec->key, rec->modState);
				if (!combo.empty())
				{
					base = utf8ToU16(combo);
				}
			}
		}
		if (base.empty() && entryIndex >= 2 && entryIndex <= 6)
		{
			// PYSAGE 1.9.8.5 .keydef 0xF39237 .. 0xF39256: camera rows fall back to APT:ETKeyMiddleButton
			if (!gameText || !gameText->fetch("APT:ETKeyMiddleButton", base) || base.empty())
			{
				base = u"Middle Button";
			}
		}
	}
	if (base.empty())
	{
		if (gameText && gameText->fetch("APT:ETKeyDefault", base) && !base.empty())
		{
			return base;
		}
		return u"Default";
	}
	return base + u" (" + suffix + u")";
}

EnnorathOptionsState::EnnorathOptionsState()
{
	resetToDefaults();
}

void EnnorathOptionsState::resetToDefaults()
{
	const auto &e = EnnorathOptions::entries();
	for (int i = 0; i < EnnorathOptions::kEntryCount; ++i)
	{
		values[(std::size_t)i] = e[(std::size_t)i].defaultValue;
	}
	delayFix = true;
}

void EnnorathOptionsState::loadFromPreferences(const OptionPreferences *prefs)
{
	// PYSAGE 1.9.8.5 .optui 0xF2325C + .keydef 0xF39035 + .dlyfix 0xF3511C
	resetToDefaults();
	if (!prefs)
	{
		return;
	}
	const auto &e = EnnorathOptions::entries();
	const int n = EnnorathOptions::activeEntryCount();
	for (int i = 0; i < n; ++i)
	{
		const EnnorathOptionEntry &entry = e[(std::size_t)i];
		if (entry.kind == EnnorathOptionKind::Retail || !entry.iniKey)
		{
			continue;
		}
		int v = entry.defaultValue;
		if (prefs->has(entry.iniKey))
		{
			int parsed = 0;
			if (parseNonNegDecimal(prefs->get(entry.iniKey), parsed) && EnnorathOptions::valueToIndex(entry.kind, parsed) >= 0)
			{
				v = parsed;
			}
		}
		// PYSAGE 1.9.8.5 .keydef 0xF3903B .. 0xF39057: mod defaults B (2) and E (5) normalize to 0 (Default)
		if (i == EnnorathOptions::kEntryBuilder && v == 2)
		{
			v = 0;
		}
		else if (i == EnnorathOptions::kEntryMatchType && v == 5)
		{
			v = 0;
		}
		values[(std::size_t)i] = v;
	}
	// PYSAGE 1.9.8.5 .dlyfix 0xF3513E .. 0xF35173: NetworkDelayFix ("0" -> false, anything else / missing -> true)
	if (prefs->has("NetworkDelayFix"))
	{
		const std::string raw = prefs->get("NetworkDelayFix");
		delayFix = raw.empty() || raw[0] != '0';
	}
}

void EnnorathOptionsState::resolveDuplicateHotkeys()
{
	// PYSAGE 1.9.8.5 .optui 0xF23C91 .. 0xF23CF5: for each hotkey row i with value != 0,
	// clear any earlier hotkey row j < i holding the same value.
	const auto &e = EnnorathOptions::entries();
	const int n = EnnorathOptions::activeEntryCount();
	for (int i = 0; i < n; ++i)
	{
		if (e[(std::size_t)i].kind != EnnorathOptionKind::Hotkey || values[(std::size_t)i] == 0)
		{
			continue;
		}
		for (int j = 0; j < i; ++j)
		{
			if (e[(std::size_t)j].kind == EnnorathOptionKind::Hotkey && values[(std::size_t)j] == values[(std::size_t)i])
			{
				values[(std::size_t)j] = 0;
			}
		}
	}
}

void EnnorathOptionsState::saveToPreferences(OptionPreferences &prefs, bool writePages, bool writeHotkeys, bool writeDelayFix) const
{
	// PYSAGE 1.9.8.5 .optui 0xF23D0A .. 0xF23DB9 + .dlyfix 0xF351A8 .. 0xF35206
	const auto &e = EnnorathOptions::entries();
	const int n = EnnorathOptions::activeEntryCount();
	for (int i = 0; i < n; ++i)
	{
		const EnnorathOptionEntry &entry = e[(std::size_t)i];
		if (entry.kind == EnnorathOptionKind::Retail || !entry.iniKey)
		{
			continue;
		}
		if (entry.kind == EnnorathOptionKind::Hotkey && !writeHotkeys)
		{
			continue;
		}
		if (entry.kind != EnnorathOptionKind::Hotkey && !writePages)
		{
			continue;
		}
		prefs.set(entry.iniKey, std::to_string(values[(std::size_t)i]));
	}
	if (writeDelayFix)
	{
		prefs.set("NetworkDelayFix", delayFix ? "1" : "0");
	}
}

void applyEnnorathHotkeys(MetaMap &map, const OptionPreferences *prefs, std::vector<MetaMapRec> *originalSnapshot)
{
	if (!ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys))
	{
		return;
	}
	// PYSAGE 1.9.8.5 .optui 0xF239C4 .. 0xF23BEB: snapshot original records once, restore before re-applying
	auto &recs = map.recordsMutable();
	if (originalSnapshot)
	{
		if (originalSnapshot->empty() && !recs.empty())
		{
			*originalSnapshot = recs;
		}
		else if (!originalSnapshot->empty())
		{
			recs = *originalSnapshot;
		}
	}

	EnnorathOptionsState state;
	state.loadFromPreferences(prefs);
	state.resolveDuplicateHotkeys();

	const auto &entries = EnnorathOptions::entries();
	const auto &keys = EnnorathOptions::keyChoices();

	const int activeCount = EnnorathOptions::activeEntryCount();
	auto findHotkeyEntryForMeta = [&](int meta) -> int {
		for (int i = 0; i < activeCount; ++i)
		{
			const EnnorathOptionEntry &e = entries[(std::size_t)i];
			if (e.kind == EnnorathOptionKind::Hotkey && (e.meta1 == meta || (e.meta2 != 0 && e.meta2 == meta)))
			{
				return i;
			}
		}
		return -1;
	};

	// 1. Update existing records matching any custom hotkey, and track which metas exist
	std::array<bool, EnnorathOptions::kEntryCount> hasMeta1{};
	std::array<bool, EnnorathOptions::kEntryCount> hasMeta2{};
	for (MetaMapRec &r : recs)
	{
		const int idx = findHotkeyEntryForMeta(r.meta);
		if (idx < 0)
		{
			continue;
		}
		const EnnorathOptionEntry &e = entries[(std::size_t)idx];
		if (r.meta == e.meta1)
		{
			hasMeta1[(std::size_t)idx] = true;
		}
		if (e.meta2 != 0 && r.meta == e.meta2)
		{
			hasMeta2[(std::size_t)idx] = true;
		}
		const int choiceIdx = state.values[(std::size_t)idx];
		if (choiceIdx > 0 && choiceIdx < EnnorathOptions::kKeyChoiceCount)
		{
			r.key = keys[(std::size_t)choiceIdx].keyCode;
			r.modState = keys[(std::size_t)choiceIdx].modState;
		}
	}

	// 2. PYSAGE 1.9.8.5 .optui 0xF23B13 .. 0xF23B78: suppress colliding non-overridden CommandMap records
	for (MetaMapRec &r : recs)
	{
		const int idx = findHotkeyEntryForMeta(r.meta);
		if (idx >= 0 && state.values[(std::size_t)idx] > 0)
		{
			continue;
		}
		for (int i = 0; i < activeCount; ++i)
		{
			if (entries[(std::size_t)i].kind != EnnorathOptionKind::Hotkey)
			{
				continue;
			}
			const int c = state.values[(std::size_t)i];
			if (c > 0 && c < EnnorathOptions::kKeyChoiceCount && r.key == keys[(std::size_t)c].keyCode && r.modState == keys[(std::size_t)c].modState)
			{
				r.key = KEY_NONE;
				break;
			}
		}
	}

	// 3. PYSAGE 1.9.8.5 .keycmd 0xF31020: synthesize records for custom hotkeys whose meta is absent from CommandMap.ini
	for (int i = 0; i < activeCount; ++i)
	{
		const EnnorathOptionEntry &e = entries[(std::size_t)i];
		if (e.kind != EnnorathOptionKind::Hotkey)
		{
			continue;
		}
		const int c = state.values[(std::size_t)i];
		if (c <= 0 || c >= EnnorathOptions::kKeyChoiceCount)
		{
			continue;
		}
		if (e.meta1 != 0 && !hasMeta1[(std::size_t)i])
		{
			MetaMapRec syn;
			syn.meta = e.meta1;
			syn.key = keys[(std::size_t)c].keyCode;
			syn.transition = TRANSITION_DOWN;
			syn.modState = keys[(std::size_t)c].modState;
			syn.usableIn = COMMANDUSABLE_GAME;
			recs.push_back(syn);
		}
		if (e.meta2 != 0 && !hasMeta2[(std::size_t)i])
		{
			MetaMapRec syn;
			syn.meta = e.meta2;
			syn.key = keys[(std::size_t)c].keyCode;
			syn.transition = TRANSITION_UP;
			syn.modState = keys[(std::size_t)c].modState;
			syn.usableIn = COMMANDUSABLE_GAME;
			recs.push_back(syn);
		}
	}

	// 4. PYSAGE 1.9.8.5 .keydef 0xF39000 / 0xF39106: Ennorath defaults Builder=B (0x5F) and MatchType=E (0x5C) when curVal == 0
	auto applyDefaultKey = [&](int entryIdx, int defaultKeyCode) {
		if (state.values[(std::size_t)entryIdx] != 0)
		{
			return;
		}
		const int meta = entries[(std::size_t)entryIdx].meta1;
		// Suppress other retail bindings for this meta (PYSAGE 1.9.8.5 .keydef 0xF39008 .. 0xF3902A)
		for (MetaMapRec &r : recs)
		{
			if (r.meta == meta)
			{
				r.key = KEY_NONE;
			}
		}
		// Do not bind if another custom hotkey took this key with no modifiers (PYSAGE 1.9.8.5 .keydef 0xF3917D .. 0xF391A2)
		for (int i = 0; i < activeCount; ++i)
		{
			if (entries[(std::size_t)i].kind != EnnorathOptionKind::Hotkey)
			{
				continue;
			}
			const int c = state.values[(std::size_t)i];
			if (c > 0 && c < EnnorathOptions::kKeyChoiceCount && keys[(std::size_t)c].keyCode == defaultKeyCode && keys[(std::size_t)c].modState == 0)
			{
				return;
			}
		}
		MetaMapRec defRec;
		defRec.meta = meta;
		defRec.key = defaultKeyCode;
		defRec.transition = TRANSITION_DOWN;
		defRec.modState = MOD_NONE;
		defRec.usableIn = COMMANDUSABLE_GAME;
		recs.push_back(defRec);
	};
	applyDefaultKey(EnnorathOptions::kEntryBuilder, KEY_B);
	applyDefaultKey(EnnorathOptions::kEntryMatchType, KEY_E);
}

EnnorathDelayFix::FrameCounts EnnorathDelayFix::compute(bool delayFixEnabled, int clientFps, bool isNetworkGame)
{
	FrameCounts out;
	const int fps = clientFps > 0 ? clientFps : 30;
	if (!ModEngineFeatures::has(ModEngineFeature::EnnorathOnlineDelayFix))
	{
		// Retail RotWK 2.01: RW 0x632535 (div 5), RW 0x632A9B (6), RW 0x62ECC8 (10), RW 0x632606 (6)
		out.delayFixActive = false;
		out.logicTickDivisor = 5;
		out.stepLimit = fps / 5;
		out.frameCountCap = 6;
		out.frameCountCapMin10 = 10;
		return out;
	}
	// PYSAGE 1.9.8.5 .optui 0xF238A4 .. 0xF238FE: frame count caps follow clientFps / 5 (6 at 30 FPS, 12 at 60 FPS)
	out.frameCountCap = fps / 5;
	out.frameCountCapMin10 = std::max(out.frameCountCap, 10);
	// PYSAGE 1.9.8.5 .dlyfix 0xF35000 .. 0xF3506B: delay fix is off only when NetworkDelayFix == 0 in a network game
	out.delayFixActive = delayFixEnabled || !isNetworkGame;
	if (out.delayFixActive)
	{
		out.logicTickDivisor = (fps == 30) ? 8 : (fps / 3);
		out.stepLimit = 3; // .dlyfix 0xF35059: mov eax, 2; inc esi -> 3
	}
	else
	{
		out.logicTickDivisor = 5; // .dlyfix 0xF35047: idiv [0xD9F608]
		out.stepLimit = fps / 5;  // .dlyfix 0xF35063: idiv [0xD9F608]
	}
	return out;
}
