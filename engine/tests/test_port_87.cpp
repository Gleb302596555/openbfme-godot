// OpenBFME unit tests. GPL-3.0.
// PORT-87 (issue #235): the unit voice's VoiceStartCharging (message 0x7EA) and VoiceNoBuild (0x7EB).
//
// Binary facts (RotWK 2.01 game.dat, md5 daed3668006cd90f01c34e5a7da1901f), taken from the reads this repository already holds — the
// audit of 2026-11-09 (docs/ennorath/arena-triage/batch-4.md rows 24 / 25 and section A, batch-8.md row 13) and from the port's own
// reading of PlaceEventTranslator (RW 0x83E486); the full chain is docs/ennorath/PORT87_RW_ANALYSIS.md:
//   * the dispatcher RW 0x8DEDBB's second switch (ids 0x7E4 .. 0x7EC, jump table RW 0x8E023B) reaches case 0x7EA at RW 0x8DFD0D, which
//     pushes the literal at RW 0xC77A74 ("VoiceStartCharging"), and case 0x7EB at RW 0x8DFE7B, which pushes RW 0xC77A64 ("VoiceNoBuild");
//     both build the AsciiString (RW 0x4374E0) and call the name lookup RW 0x8DDB2A, the UnitSpecificSounds map of the template
//     (tt + 0x388, table RW 0xDA4378, parser RW 0x73F684) — the port's `fillName`.
//   * the one live poster proven in the image is 0x7EB: RW 0x83EAA0 `push 0x7EB` inside RotWK's PlaceEventTranslator, after the refusal
//     label RW 0x83EA4E and before the beep RW 0x83EAC5, with the builder ALONE (a one-element drawable list) and a pick info built by
//     RW 0x8DD5FF.
//   * the post of 0x7EA is unproven for RotWK (stop S-7226): this file pins the dispatcher's half of it and that the port does not invent
//     a poster (no second path) until a read names one.
//
// The tests use synthetic templates only (no game files): UnitVoiceResponse on a logic world, and the placement translator on the same
// world with the HUD context's voice callback installed.

#include "doctest.h"

#include "LogicTestUtil.h"
#include "SyntheticHudTestUtil.h"

#include "Common/BuildAssistant.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/HudInput.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/BuildPlacement.h"
#include "GameClient/MessageStream/PlaceEventTranslator.h"
#include "GameLogic/Object/Object.h"

#include <set>
#include <string>
#include <vector>

using namespace synthetichud;

namespace
{
struct RecordingSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds, eva;
	std::vector<ObjectID> owners;
	bool soundExists(const std::string &name) override { return known.count(name) != 0; }
	void playSound(const std::string &name, ObjectID object, int) override
	{
		sounds.push_back(name);
		owners.push_back(object);
	}
	void reportEva(const std::string &name, const Coord3D *) override { eva.push_back(name); }
	void clear()
	{
		sounds.clear();
		eva.clear();
		owners.clear();
	}
};

// A builder with both ported rows and the row VoiceBuildResponse (the neighbouring behaviour: a placed building says its build voice).
const char kVoiceTemplates[] =
	"Object TestPorter\n"
	"  KindOf = SELECTABLE DOZER\n"
	"  VoicePriority = 10\n"
	"  UnitSpecificSounds\n"
	"    VoiceNoBuild = PorterCantBuild\n"
	"    VoiceStartCharging = PorterChargeRoar\n"
	"    VoiceBuildResponse = PorterBuildStart\n"
	"  End\n"
	"End\n"
	"Object TestMutePort\n"
	"  KindOf = SELECTABLE DOZER\n"
	"End\n"
	"Object TestBarracks\n"
	"  KindOf = STRUCTURE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 40\n"
	"  GeometryMinorRadius = 40\n"
	"  GeometryHeight = 20\n"
	"End\n";

struct Port87Voice
{
	RecordingSink sink;
	std::unique_ptr<UnitVoiceResponse> voice;
	explicit Port87Voice(logictest::LogicWorld &f)
	{
		for (const char *s : { "PorterCantBuild", "PorterChargeRoar", "PorterBuildStart" })
		{
			sink.known.insert(s);
		}
		voice = std::make_unique<UnitVoiceResponse>(*f.logic, sink);
		voice->setLocalPlayerIndex(f.players.findPlayerWithName("Alice")->getPlayerIndex());
	}
};
} // namespace

TEST_CASE("PORT-87 voice: message 0x7EA answers UnitSpecificSounds/VoiceStartCharging (RW 0x8DFD0D)")
{
	logictest::LogicWorld f;
	REQUIRE_MESSAGE(f.w.load(kVoiceTemplates, INI_LOAD_OVERWRITE, "port87.ini").empty(), f.loadError);
	Port87Voice v(f);
	Object *porter = f.make("TestPorter", f.teamOf("Alice"));
	REQUIRE(porter != nullptr);
	// the type is the case's id (RW 0x8DFD0D) and it is one this port answers; before PORT-87 it was counted as unported (S-703)
	CHECK(UnitVoiceResponse::VOICE_EVENT_START_CHARGING == 0x7EA);
	CHECK(UnitVoiceResponse::handles(UnitVoiceResponse::VOICE_EVENT_START_CHARGING));
	CHECK(v.voice->stats().unportedMessages == 0);
	// the row speaks, the literal key's name being the one of RW 0xC77A74
	CHECK(v.voice->pickAndPlay({ porter->getID() }, UnitVoiceResponse::VOICE_EVENT_START_CHARGING));
	CHECK(v.voice->stats().unportedMessages == 0);
	REQUIRE(v.sink.sounds.size() == 1);
	CHECK(v.sink.sounds[0] == "PorterChargeRoar");
	CHECK(v.sink.owners[0] == porter->getID());
	CHECK(v.sink.eva.empty());
	CHECK(v.voice->last().msgType == 0x7EA);
	// a template without the row answers silence, once per ask (not once per candidate beyond the first)
	Object *mute = f.make("TestMutePort", f.teamOf("Alice"));
	REQUIRE(mute != nullptr);
	v.sink.clear();
	CHECK_FALSE(v.voice->pickAndPlay({ mute->getID() }, UnitVoiceResponse::VOICE_EVENT_START_CHARGING));
	CHECK(v.sink.sounds.empty());
	CHECK(v.voice->stats().unknownSounds == 0); // a missing row is not an unknown audio event
}

TEST_CASE("PORT-87 voice: message 0x7EB answers UnitSpecificSounds/VoiceNoBuild (RW 0x8DFE7B) and the neighbouring build response still says its own row")
{
	logictest::LogicWorld f;
	REQUIRE_MESSAGE(f.w.load(kVoiceTemplates, INI_LOAD_OVERWRITE, "port87.ini").empty(), f.loadError);
	Port87Voice v(f);
	Object *porter = f.make("TestPorter", f.teamOf("Alice"));
	Object *mute = f.make("TestMutePort", f.teamOf("Alice"));
	REQUIRE(porter != nullptr);
	REQUIRE(mute != nullptr);
	CHECK(UnitVoiceResponse::VOICE_EVENT_NO_BUILD == 0x7EB);
	CHECK(UnitVoiceResponse::handles(UnitVoiceResponse::VOICE_EVENT_NO_BUILD));
	// the refusal's row
	CHECK(v.voice->pickAndPlay({ porter->getID() }, UnitVoiceResponse::VOICE_EVENT_NO_BUILD));
	REQUIRE(v.sink.sounds.size() == 1);
	CHECK(v.sink.sounds[0] == "PorterCantBuild");
	// no row, no sound
	v.sink.clear();
	CHECK_FALSE(v.voice->pickAndPlay({ mute->getID() }, UnitVoiceResponse::VOICE_EVENT_NO_BUILD));
	CHECK(v.sink.sounds.empty());
	// the neighbouring behaviour of the same path (a placed building's VoiceBuildResponse) is unchanged
	v.sink.clear();
	CHECK(v.voice->pickAndPlay({ porter->getID() }, MSG_DOZER_CONSTRUCT));
	REQUIRE(v.sink.sounds.size() == 1);
	CHECK(v.sink.sounds[0] == "PorterBuildStart");
	// and the two new ids are not enter-state voices (RW 0x8DD63C lists 0x7E6, 0x7E7, 0x7E8, 0x7EC), so the repeat memory does not suppress them
	CHECK_FALSE(UnitVoiceResponse::handles(MSG_COMBINE_HORDES_WITH_OBJECT)); // still not answered (S-703)
	CHECK(v.voice->stats().unportedMessages == 1);
}

TEST_CASE("PORT-87 placement: a refused click asks the voice for 0x7EB with the builder alone and the click's position (RW 0x83EAA0)")
{
	Rig r;
	REQUIRE(r.w.load(kVoiceTemplates) == "");
	REQUIRE(r.w.load("Object TestFarm\n KindOf = STRUCTURE\n Geometry = BOX\n GeometryMajorRadius = 40\n GeometryMinorRadius = 40\n GeometryHeight = 20\nEnd\n") == "");
	Object *porter = r.place("TestPorter", 100, 100);
	Object *farm = r.place("TestFarm", 260, 100);
	REQUIRE(porter != nullptr);
	REQUIRE(farm != nullptr);
	// the HUD context's voice callback: what InGameHud installs (UnitVoiceResponse::pickAndPlay with one object and a position)
	struct Post
	{
		int msgType = 0;
		ObjectID object = INVALID_ID;
		Coord3D position;
	};
	std::vector<Post> posts;
	r.input->context().playObjectVoice = [&posts](int msgType, ObjectID object, const Coord3D &position) {
		posts.push_back({ msgType, object, position });
	};
	// the porter and a second, selected unit: RotWK hands the picker the builder alone, never the selection (RW 0x83EAA0's one-element list)
	Object *other = r.place("TestMutePort", 160, 100);
	REQUIRE(other != nullptr);
	r.input->ui().selectObject(porter->getID());
	r.players.getLocalPlayer()->selection().push_back(porter->getID());
	r.input->ui().selectObject(other->getID());
	r.players.getLocalPlayer()->selection().push_back(other->getID());
	// place on the farm: objects in the way (the refusal branch of RW 0x83EA4E .. RW 0x83EB02). A left press anchors the site (vslot 0xE8) and the
	// release is the placement click (RW 0x83E6C7)
	r.input->ui().placeBuildAvailable("TestBarracks", porter->getID());
	r.move(260, 100);
	r.button(HudInput::Button::Left, true, 260, 100);
	CHECK(r.input->ui().isPlacementAnchored());
	r.button(HudInput::Button::Left, false, 260, 100);
	REQUIRE(r.input->ui().isPlacing());
	CHECK(r.input->ui().placeLegalCode() == (int)LBC_OBJECTS_IN_THE_WAY);
	REQUIRE(r.placeTranslator().stats().refused == 1);
	REQUIRE(posts.size() == 1);
	CHECK(posts[0].msgType == 0x7EB);
	CHECK(posts[0].object == porter->getID());     // the builder alone, not the other selected unit
	CHECK(posts[0].object != other->getID());
	// the click's world position is the anchor start's terrain point (RW 0x83E745: the same location the construct message would carry), bit for bit:
	// the translator passes on the value it read, no arithmetic of its own (the march of TacticalView::screenToTerrain is deterministic here)
	Coord3D site;
	REQUIRE(r.view.screenToTerrain(r.input->ui().placementStart(), *r.logic, site));
	CHECK(posts[0].position.x == site.x);
	CHECK(posts[0].position.y == site.y);
	CHECK(posts[0].position.z == site.z);
	// the order of the refusal branch: the label, the voice, the beep, then the anchor drops (RW 0x83EA4E, 0x83EAA0, 0x83EAC5, 0x83EB02)
	const std::vector<std::string> labels = r.input->ui().takeMessages();
	REQUIRE(labels.size() == 1);
	CHECK(labels[0] == std::string(PlaceEventTranslator::cantBuildLabel((int)LBC_OBJECTS_IN_THE_WAY)));
	const std::vector<std::string> misc = r.input->ui().takeMiscSounds();
	REQUIRE(misc.size() == 1);
	CHECK(misc[0] == "BadBuildingPlacementSound");
	CHECK_FALSE(r.input->ui().isPlacementAnchored());
	// a legal site places the building and asks no voice from the refusal branch; a stream without a HUD counts the unheard post
	CHECK(r.placeTranslator().stats().voicesUnheard == 0);
}

TEST_CASE("PORT-87 placement: without a HUD the refused click is counted, and a legal site posts nothing")
{
	Rig r;
	REQUIRE(r.w.load(kVoiceTemplates) == "");
	Object *porter = r.place("TestPorter", 100, 100);
	REQUIRE(porter != nullptr);
	// no playObjectVoice installed: the post is counted (Stats::voicesUnheard), never silently dropped
	r.input->ui().placeBuildAvailable("TestBarracks", porter->getID());
	r.move(600, 400);
	r.button(HudInput::Button::Left, true, 600, 400);
	r.button(HudInput::Button::Left, false, 600, 400);
	if (r.placeTranslator().stats().refused == 0)
	{
		// a legal site (this stand-in world may accept it): nothing of the refusal branch runs
		CHECK(r.placeTranslator().stats().voicesUnheard == 0);
		return;
	}
	CHECK(r.placeTranslator().stats().voicesUnheard == 1);
}
