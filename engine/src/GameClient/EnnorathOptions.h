// OpenBFME. GPL-3.0.
//
// Ennorath 1.9.8.5 options menu, hotkeys and online delay-fix support (fork issue 811, lane MODS-3).
// Rebuilt from Ennorath 1.9.8.5's pySAGE-patched game.dat (md5 fc32e4c4142b5c607b90972eaf448400):
//   .optui   0xF23000 (hooks RW 0x5DA8F0, 0x5ED37A, 0x639FEF, 0x6E573C, 0x9204D9, 0x920602, 0x9212BA; .rdata RW 0xC1B4B4)
//   .optfix  0xF30000 (key display names and default label formatting)
//   .keycmd  0xF31000 (synthetic MetaMapRec dispatch for custom hotkeys)
//   .dlyfix  0xF35000 (hooks RW 0x632535, 0x632A9B; in-place RW 0x62ECC8, 0x632606, 0x632633, 0x63264C, 0x632AD1)
//   .hotkeys 0xF38000 (hooks RW 0x5DA8F5, 0x812C89, 0x812EDA, 0x81FAAB)
//   .keydef  0xF39000 (hook RW 0x5DA910; .rdata RW 0xC80314)
//
// Active only when the corresponding ModEngineFeature is enabled; off, the port stays retail RotWK 2.01.

#pragma once

#include <array>
#include <string>
#include <vector>

class GameTextSource;
class MetaMap;
struct MetaMapRec;
class OptionPreferences;

struct EnnorathKeyChoice
{
	int keyCode;      // DirectInput scan code (KeyCode, 0 = default)
	int modState;     // ModifierMask (0, MOD_CTRL 0x04, MOD_SHIFT 0x10, MOD_ALT 0x40)
	const char *label; // ASCII / UTF-8 label (index 0 is replaced by formatted default label)
};

enum class EnnorathOptionKind : int
{
	Retail = 0,          // retail gadget managed by AptOptions::InitGadgets (RW 0x9205C4), hidden/shown per page
	FrameRate = 1,       // Options::ETFrameRate (30 / 60; display-only in OpenBFME, owner decision 2026-11-15)
	MouseSensitivity = 2, // Options::ETMouseSensitivity (50% / 75% / 100%)
	Hotkey = 3           // Options::ETKey* (11 CommandMap hotkey rows)
};

struct EnnorathOptionEntry
{
	const char *gadgetName; // name after "Options::" (e.g. "ETFrameRate", "ETKeyBuildPath")
	const char *iniKey;     // Options.ini key (nullptr for Retail entries; note ETKeyBuildPath -> "ETKeyMatchType")
	int page;               // 0 Graphics, 1 Screen, 2 Sound, 3 Hotkeys, 4 Network
	EnnorathOptionKind kind;
	int defaultValue;       // 60 for FPS, 100 for sensitivity, 0 for hotkeys
	int meta1;              // ClientMessageType for primary/DOWN command (0 for non-hotkeys)
	int meta2;              // ClientMessageType for UP/END command (0 when single-shot)
};

class EnnorathOptions
{
public:
	static constexpr int kEntryCount = 28;      // PYSAGE 1.9.8.5 .optui 0xF25960 (0x1C entries, stride 0x24)
	static constexpr int kKeyChoiceCount = 135; // PYSAGE 1.9.8.5 .optui 0xF24640 (0x87 entries, stride 0x0C)
	static constexpr int kFpsChoiceCount = 2;   // PYSAGE 1.9.8.5 .optui 0xF24CD4 (30, 60)
	static constexpr int kSensChoiceCount = 3;  // PYSAGE 1.9.8.5 .optui 0xF23679 (50, 75, 100)
	static constexpr int kEntryBuilder = 7;     // Options::ETKeyBuilder (default B = index 2)
	static constexpr int kEntryMatchType = 10;  // Options::ETKeyBuildPath / ETKeyMatchType (default E = index 5)
	static constexpr int kEntrySpellbook = 11;  // Options::ETKeySpellbook (meta 0x92)
	static constexpr int kEntrySell = 12;       // Options::ETKeySell (meta 0x9A)

	static const std::array<EnnorathOptionEntry, kEntryCount> &entries();
	static const std::array<EnnorathKeyChoice, kKeyChoiceCount> &keyChoices();
	static const std::array<int, kFpsChoiceCount> &fpsValues();
	static const std::array<int, kSensChoiceCount> &sensitivityValues();

	static int findEntryByGadget(const std::string &shortName);
	static int valueToIndex(EnnorathOptionKind kind, int value);
	static int indexToValue(EnnorathOptionKind kind, int index);
	static int choiceCount(EnnorathOptionKind kind);

	// PYSAGE 1.9.8.5 .optfix 0xF30000 / 0xF30060 + .keydef 0xF391E6: label for index 0 of a hotkey ComboBox
	static std::u16string formatDefaultKeyLabel(int entryIndex, const MetaMap *metaMap, const GameTextSource *gameText);
	static std::string formatKeyCombo(int keyCode, int modState);
};

struct EnnorathOptionsState
{
	std::array<int, EnnorathOptions::kEntryCount> values{};
	bool delayFix = true; // PYSAGE 1.9.8.5 .dlyfix 0xF352E8: default 1 (enabled)

	EnnorathOptionsState();
	void resetToDefaults();
	// PYSAGE 1.9.8.5 .optui 0xF2325C + .keydef 0xF39035 + .dlyfix 0xF3511C
	void loadFromPreferences(const OptionPreferences *prefs);
	// PYSAGE 1.9.8.5 .optui 0xF23C91: later hotkey row wins on collision, earlier resets to 0
	void resolveDuplicateHotkeys();
	// PYSAGE 1.9.8.5 .optui 0xF23D0A + .dlyfix 0xF35189
	void saveToPreferences(OptionPreferences &prefs, bool writePages, bool writeHotkeys, bool writeDelayFix) const;
};

// PYSAGE 1.9.8.5 .optui 0xF239C4 + .hotkeys 0xF38000 / 0xF38099 + .keydef 0xF39000 / 0xF39106 + .keycmd 0xF31020:
// Applies Ennorath 1.9.8.5's hotkey overrides and mod defaults (Builder=B, MatchType=E) to a MetaMap when
// ModEngineFeature::EnnorathOptionHotkeys is active. Passing originalSnapshot preserves the pre-override
// CommandMap so repeated calls after saving options are idempotent.
void applyEnnorathHotkeys(MetaMap &map, const OptionPreferences *prefs, std::vector<MetaMapRec> *originalSnapshot = nullptr);

struct EnnorathDelayFix
{
	struct FrameCounts
	{
		bool delayFixActive = false;
		int logicTickDivisor = 5;   // divisor at RW 0x632535 (.dlyfix 0xF35035): 8 (30 FPS) / 20 (60 FPS) when ON, 5 when OFF
		int stepLimit = 6;          // limit at RW 0x632A9B (.dlyfix 0xF35052): 3 when ON, clientFps/5 when OFF
		int frameCountCap = 6;      // in-place RW 0x632606 / 0x63264C (.optui 0xF238A4): clientFps/5 (6 at 30 FPS, 12 at 60 FPS)
		int frameCountCapMin10 = 10; // in-place RW 0x62ECC8 / 0x632633 / 0x632AD1 (.optui 0xF238BF): max(clientFps/5, 10)
	};

	// PYSAGE 1.9.8.5 .dlyfix 0xF35000 .. 0xF3506B + .optui 0xF238A4 .. 0xF238FE (hooks RW 0x632535, 0x632A9B).
	// When ModEngineFeature::EnnorathOnlineDelayFix is off, returns retail 2.01 values (5, 6, 6, 10).
	static FrameCounts compute(bool delayFixEnabled, int clientFps, bool isNetworkGame);
};
