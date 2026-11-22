// OpenBFME unit tests. GPL-3.0.
// LOCAL-127 (fork issue 811): Ennorath 1.9.8.5 CommandMap hotkey overrides and defaults.
// Sources of expectations:
// - PYSAGE 1.9.8.5 .optui 0xF239C4 (hook RW 0x5DA8F0), 0xF23B13 (collision suppression)
// - PYSAGE 1.9.8.5 .keydef 0xF39000 / 0xF39106 (hook RW 0x5DA910; Builder=B, MatchType=E defaults)
// - PYSAGE 1.9.8.5 .keycmd 0xF31020 (synthetic MetaMapRec dispatch for custom hotkeys)

#include "doctest.h"

#include "Common/ModEngineFeatures.h"
#include "Common/NameKeyGenerator.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "Common/Thing/RwGrammar.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "GameClient/EnnorathOptions.h"
#include "GameClient/HudContext.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/MessageStream/MetaEvent.h"
#include "GameClient/OptionPreferences.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/GameLogic.h"

#include <string>
#include <vector>

namespace
{

struct ScopedModEngineReset
{
	~ScopedModEngineReset() { ModEngineFeatures::reset(); }
};

void activateEnnorath1985()
{
	std::string err;
	REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &err), err);
}

} // namespace

TEST_CASE("LOCAL-127 hotkeys: CommandMap overrides, collision suppression, and Builder=B / MatchType=E defaults (profile ON vs OFF)")
{
	// Sources: PYSAGE 1.9.8.5 .optui 0xF239C4 (hook RW 0x5DA8F0), 0xF23B13; .keydef 0xF39000 / 0xF39106 (hook RW 0x5DA910); .keycmd 0xF31020
	ScopedModEngineReset guard;

	auto buildBaseMap = []() {
		MetaMap map;
		MetaMapRec stopRec;
		stopRec.meta = CMSG_META_STOP;
		stopRec.key = KEY_S;
		stopRec.transition = TRANSITION_DOWN;
		stopRec.modState = MOD_NONE;
		stopRec.usableIn = COMMANDUSABLE_GAME;
		map.recordsMutable().push_back(stopRec);

		MetaMapRec collideRec;
		collideRec.meta = CMSG_META_SCATTER;
		collideRec.key = KEY_G;
		collideRec.transition = TRANSITION_DOWN;
		collideRec.modState = MOD_NONE;
		collideRec.usableIn = COMMANDUSABLE_GAME;
		map.recordsMutable().push_back(collideRec);
		return map;
	};

	OptionPreferences prefs;
	prefs.set("ETKeyCameraReset", "7"); // index 7 = G (KEY_G, MOD_NONE)

	// Profile OFF: retail 2.01 CommandMap stays untouched
	ModEngineFeatures::reset();
	{
		MetaMap map = buildBaseMap();
		applyEnnorathHotkeys(map, &prefs);
		CHECK(map.find(CMSG_META_CAMERA_RESET) == nullptr);
		CHECK(map.find(CMSG_META_SELECT_NEXT_WORKER) == nullptr);
		REQUIRE(map.find(CMSG_META_SCATTER) != nullptr);
		CHECK(map.find(CMSG_META_SCATTER)->key == KEY_G);
	}

	// Profile ON: CameraReset -> G (synthesized), Scatter (G) suppressed, Builder -> B, MatchType -> E
	activateEnnorath1985();
	{
		MetaMap map = buildBaseMap();
		std::vector<MetaMapRec> snapshot;
		applyEnnorathHotkeys(map, &prefs, &snapshot);

		REQUIRE(map.find(CMSG_META_CAMERA_RESET) != nullptr);
		CHECK(map.find(CMSG_META_CAMERA_RESET)->key == KEY_G);
		REQUIRE(map.find(CMSG_META_SCATTER) != nullptr);
		CHECK(map.find(CMSG_META_SCATTER)->key == KEY_NONE); // suppressed on collision (.optui 0xF23B67)
		REQUIRE(map.find(CMSG_META_SELECT_NEXT_WORKER) != nullptr);
		CHECK(map.find(CMSG_META_SELECT_NEXT_WORKER)->key == KEY_B); // .keydef 0xF3915B
		REQUIRE(map.find(CMSG_META_SELECT_MATCHING_UNITS) != nullptr);
		CHECK(map.find(CMSG_META_SELECT_MATCHING_UNITS)->key == KEY_E); // .keydef 0xF39168

		// Verify MetaEventTranslator actually emits CMSG_META_CAMERA_RESET on KEY_G and CMSG_META_SELECT_NEXT_WORKER on KEY_B
		NameKeyGenerator keys;
		RwGrammar grammar(RwBinaryData::embedded());
		ModuleFactory modules(keys, grammar);
		ThingFactory things(keys, modules, grammar);
		PlayerTemplateStore templates(keys);
		TeamFactory teams;
		PlayerList players(keys, templates, teams);
		GameLogic logic(things, modules, players, RandomAlgorithm::RotWK_GameDat_LCG);
		PinholeView view;
		InGameUI ui;
		MessageStream stream;
		MouseSettings mouse;
		HudContext ctx{ logic, nullptr, view, ui, stream, nullptr, mouse };
		MetaEventTranslator xlat(ctx, map);
		stream.attachTranslator(&xlat, 20);

		ClientMessage &downG = stream.append(CMSG_RAW_KEY_DOWN);
		downG.appendInteger(KEY_G);
		downG.appendInteger(KEY_STATE_DOWN);
		ClientMessage &downB = stream.append(CMSG_RAW_KEY_DOWN);
		downB.appendInteger(KEY_B);
		downB.appendInteger(KEY_STATE_DOWN);
		const std::vector<ClientMessage> out = stream.propagate();
		bool sawReset = false;
		bool sawBuilder = false;
		for (const ClientMessage &m : out)
		{
			sawReset = sawReset || (m.type() == CMSG_META_CAMERA_RESET);
			sawBuilder = sawBuilder || (m.type() == CMSG_META_SELECT_NEXT_WORKER);
		}
		CHECK(sawReset);
		CHECK(sawBuilder);

		// Re-applying with ETKeyCameraReset = 0 restores Scatter -> G via snapshot
		OptionPreferences prefsReset;
		prefsReset.set("ETKeyCameraReset", "0");
		applyEnnorathHotkeys(map, &prefsReset, &snapshot);
		REQUIRE(map.find(CMSG_META_SCATTER) != nullptr);
		CHECK(map.find(CMSG_META_SCATTER)->key == KEY_G);
	}
}
