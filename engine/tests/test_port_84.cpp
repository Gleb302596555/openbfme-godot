// PORT-84 (issue #232): the unit voice of the combine-with-horde order (RW 0x8DEDBB case 0x8DF2EB, message MSG_COMBINE_HORDES_WITH_OBJECT, 1059).
//
// RW facts (RotWK game.dat; the BFME2 1.06 decomp PickAndPlayUnitVoiceResponse.cpp case 0x423 / CommandXlat.cpp line 697 reads the same):
//   * RW 0x81F4B2 posts the message from evaluateContextCommand's right-click ladder (single selection, target combineable, ActionManager check 16:
//     each selected object's effective horde has a ComboHorde entry for the other side). It writes the target's object id as argument 0.
//   * RW 0x8DF2EB: no pick target (info + 4) -> no voice at all; otherwise, per candidate: the candidate's horde (RW 0x694bf8: the object itself when
//     HORDE-kind, else the horde that contains it) carries the ComboHorde list; the entry whose Target names the TARGET-side horde's template (RW
//     0x87043B on the horde, the compare RW 0x4065AA; the victim's own horde at RW 0x693a1a, else the victim itself) supplies the InitiateVoice; if the
//     slot stays empty, row 0x15 (VoiceCombineWithHorde) fills it (RW 0x8DF5EB).
//   * the port has no writer or consumer of the message yet (CombineHordeSpecialPower / Local 18 is the combine order in the port; the right-click
//     context order, its poster at RW 0x81F4B2 and the group's combine state are not ported), so the branch is exercised here by a synthetic message.
//   * InitiateVoice:EVA:<event> cannot be reached from a ComboHorde line in this port: the parser reads the value with the colon separators, so the value
//     is cut at its colon ("EVA") and the audio-event lookup rejects it (see test_horde_contain_data.cpp, "retail quirk"; the RW branch at RW 0x73AB45
//     stays in the source for shape). Only a plain event name (or NoSound) parses.
//
// Synthetic data only: the expectations are the INI below and the RW branch's shape.

#include "doctest.h"

#include "MoveTestUtil.h"

#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/ContainParseHooks.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <set>
#include <vector>

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

const char kCombineHordeVoiceObjects[] =
	"Object Test84Banner\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	"  Behavior = AIUpdateInterface AITag\n End\n"
	"  VoicePriority = 10\n"
	"  VoiceCombineWithHorde = Test84Join\n"
	"End\n"
	"Object Test84QuietBanner\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	"  Behavior = AIUpdateInterface AITag\n End\n"
	"  VoicePriority = 10\n"
	"End\n"
	"Object Test84ElderBanner\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	"  Behavior = AIUpdateInterface AITag\n End\n"
	"  VoicePriority = 20\n"
	"  VoiceCombineWithHorde = Test84ElderJoin\n"
	"End\n"
	"Object Test84Scout\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	"  VoicePriority = 10\n"
	"  VoiceCombineWithHorde = Test84Join\n"
	"End\n"
	"Object Test84Elder\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	"  VoicePriority = 20\n"
	"  VoiceCombineWithHorde = Test84ElderJoin\n"
	"End\n"
	// the combine target: a horde (the branch reads its template's name, or the name of the horde it rides with)
	"Object Test84TargetHorde\n"
	"  KindOf = HORDE SELECTABLE\n"
	"  Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	"  Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	"  Behavior = HordeAIUpdate AITag\n End\n"
	"  Behavior = HordeContain ContainTag\n"
	"    Slots = 1\n"
	"    RankInfo = RankNumber:1 UnitType:Test84Scout Position:X:20 Y:0\n"
	"    InitialPayload = Test84Scout 1\n"
	"  End\n"
	"  LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// the Result horde (referenced by the entries, never made in the voice tests)
	"Object Test84ResultHorde\n"
	"  KindOf = HORDE COMBO_HORDE SELECTABLE\n"
	"  Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	"  Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	"  Behavior = HordeAIUpdate AITag\n End\n"
	"  Behavior = HordeContain ContainTag\n"
	"    Slots = 4\n"
	"    RankInfo = RankNumber:1 UnitType:Test84Scout Position:X:20 Y:0\n"
	"  End\n"
	"  LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// the candidate horde: its entry for Test84TargetHorde carries the voice the combine starts with
	"Object Test84JoinHorde\n"
	"  KindOf = HORDE SELECTABLE\n"
	"  Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	"  Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	"  Behavior = HordeAIUpdate AITag\n End\n"
	"  Behavior = HordeContain ContainTag\n"
	"    Slots = 1\n"
	"    RankInfo = RankNumber:1 UnitType:Test84Scout Position:X:20 Y:0\n"
	"    InitialPayload = Test84Scout 1\n"
	"    ComboHorde = Target:Test84TargetHorde Result:Test84ResultHorde InitiateVoice:Test84LeaderJoin\n"
	"  End\n"
	"  LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// the candidate horde with InitiateVoice:NoSound: the entry fills nothing, the member's row answers (RW 0x8DF5EB)
	"Object Test84DeafJoinHorde\n"
	"  KindOf = HORDE SELECTABLE\n"
	"  Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	"  Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	"  Behavior = HordeAIUpdate AITag\n End\n"
	"  Behavior = HordeContain ContainTag\n"
	"    Slots = 1\n"
	"    RankInfo = RankNumber:1 UnitType:Test84Scout Position:X:20 Y:0\n"
	"    InitialPayload = Test84Scout 1\n"
	"    ComboHorde = Target:Test84TargetHorde Result:Test84ResultHorde InitiateVoice:NoSound\n"
	"  End\n"
	"  LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// no ComboHorde at all: the members' rows answer (RW 0x8DF5EB)
	"Object Test84PlainHorde\n"
	"  KindOf = HORDE SELECTABLE\n"
	"  Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	"  Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	"  Behavior = HordeAIUpdate AITag\n End\n"
	"  Behavior = HordeContain ContainTag\n"
	"    Slots = 2\n"
	"    RankInfo = RankNumber:1 UnitType:Test84Elder Position:X:20 Y:0\n"
	"    RankInfo = RankNumber:2 UnitType:Test84Scout Position:X:-20 Y:0\n"
	"    InitialPayload = Test84Elder 1\n"
	"    InitialPayload = Test84Scout 1\n"
	"  End\n"
	"  LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n";

struct VoiceRig
{
	movetest::MoveWorld f;
	RecordingSink sink;
	std::unique_ptr<UnitVoiceResponse> voice;
	ContainParseHooks saved;
	VoiceRig()
		: f(100, 100, "")
	{
		// the ComboHorde InitiateVoice names an audio event the parser must resolve (S-083): install the lookup before the objects' INI is read
		saved = TheContainParseHooks();
		TheContainParseHooks().audioEventExists = [](const std::string &n) { return n == "Test84LeaderJoin"; };
		const std::string err = f.w.load(kCombineHordeVoiceObjects, INI_LOAD_OVERWRITE, "port84.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		for (const char *s : { "Test84Join", "Test84ElderJoin", "Test84LeaderJoin" })
		{
			sink.known.insert(s);
		}
		voice = std::make_unique<UnitVoiceResponse>(*f.logic, sink);
		voice->setLocalPlayerIndex(f.players.findPlayerWithName("Alice")->getPlayerIndex());
	}
	~VoiceRig() { TheContainParseHooks() = saved; }
	ObjectID make(const char *name, const char *player = "Alice")
	{
		Object *o = f.make(name, f.teamOf(player));
		return o->getID();
	}
	Object *horde(const char *name) { return f.logic->findObjectByID(make(name)); }

	static ObjectID memberWithTemplate(movetest::MoveWorld &f, Object *horde, const char *templateName)
	{
		if (ContainModuleInterface *contain = horde ? horde->getContain() : nullptr)
		{
			if (const ContainModuleInterface::ContainedItemsList *items = contain->getContainedItemsList())
			{
				for (Object *member : *items)
				{
					if (member && member->getTemplate() && member->getTemplate()->getName() == templateName)
					{
						return member->getID();
					}
				}
			}
		}
		return INVALID_ID;
	}
};
} // namespace

TEST_CASE("PORT-84: the combine-hordes message is answered (RW 0x8DF2EB) and the target is its argument 0")
{
	CHECK(UnitVoiceResponse::handles(MSG_COMBINE_HORDES_WITH_OBJECT));
	// the RW producer (RW 0x81F4B2) writes the target object id alone
	CHECK(UnitVoiceResponse::voiceTargetArgument(MSG_COMBINE_HORDES_WITH_OBJECT) == 0);
	// the message reaches the HUD and the info is parsed from it (RW 0x8DF2EB reads the pick info's target)
	VoiceRig r;
	const ObjectID banner = r.make("Test84Banner");
	const ObjectID target = r.make("Test84TargetHorde");
	GameMessage msg(MSG_COMBINE_HORDES_WITH_OBJECT, 0);
	msg.appendObjectIDArgument(target);
	PlayUnitVoiceForMessage(*r.voice, { banner }, msg);
	REQUIRE(r.sink.sounds.size() == 1);
	CHECK(r.sink.sounds[0] == "Test84Join");
	CHECK(r.sink.owners[0] == banner);
}

TEST_CASE("PORT-84: a banner carrier with VoiceCombineWithHorde says its join line; the unit without the row is silent (RW 0x8DF5EB)")
{
	VoiceRig r;
	const ObjectID banner = r.make("Test84Banner");
	const ObjectID quiet = r.make("Test84QuietBanner");
	const ObjectID target = r.make("Test84TargetHorde");
	const ObjectID elder = r.make("Test84ElderBanner");
	UnitVoiceResponse::Info info;
	info.target = target;
	// the speaker RW picks: two carriers, the higher VoicePriority wins (the rank of RW 0x8DD663)
	CHECK(r.voice->pickAndPlay({ banner, elder }, MSG_COMBINE_HORDES_WITH_OBJECT, &info));
	CHECK(r.sink.sounds == std::vector<std::string>{ "Test84ElderJoin" });
	CHECK(r.sink.owners == std::vector<ObjectID>{ elder });
	CHECK(r.voice->last().count == 1);
	// a unit without the row: nothing (RW 0x8DF5EB finds no row voice)
	r.sink.clear();
	CHECK_FALSE(r.voice->pickAndPlay({ quiet }, MSG_COMBINE_HORDES_WITH_OBJECT, &info));
	CHECK(r.sink.sounds.empty());
	CHECK(r.sink.eva.empty());
	CHECK(r.voice->stats().silent >= 1);
}

TEST_CASE("PORT-84: without the picked target there is no voice at all (RW 0x8DF2F2)")
{
	VoiceRig r;
	const ObjectID banner = r.make("Test84Banner");
	const ObjectID target = r.make("Test84TargetHorde");
	CHECK_FALSE(r.voice->pickAndPlay({ banner }, MSG_COMBINE_HORDES_WITH_OBJECT, nullptr));
	UnitVoiceResponse::Info empty; // target = INVALID_ID
	CHECK_FALSE(r.voice->pickAndPlay({ banner }, MSG_COMBINE_HORDES_WITH_OBJECT, &empty));
	UnitVoiceResponse::Info gone;
	gone.target = target + 1000; // no such object
	CHECK_FALSE(r.voice->pickAndPlay({ banner }, MSG_COMBINE_HORDES_WITH_OBJECT, &gone));
	CHECK(r.sink.sounds.empty());
	CHECK(r.sink.eva.empty());
}

TEST_CASE("PORT-84: the candidate horde's ComboHorde entry for the target's template supplies InitiateVoice (RW 0x87043B)")
{
	VoiceRig r;
	const ObjectID target = r.make("Test84TargetHorde");
	Object *horde = r.horde("Test84JoinHorde");
	REQUIRE(horde);
	const ObjectID scout = VoiceRig::memberWithTemplate(r.f, horde, "Test84Scout");
	REQUIRE(scout != INVALID_ID);
	UnitVoiceResponse::Info info;
	info.target = target;
	// the horde's selection speaks with the entry's InitiateVoice, not with the row (RW 0x8DF2EB: the entry fills slot 0 first)
	CHECK(r.voice->pickAndPlay({ horde->getID() }, MSG_COMBINE_HORDES_WITH_OBJECT, &info));
	CHECK(r.sink.sounds == std::vector<std::string>{ "Test84LeaderJoin" });
	CHECK(r.sink.owners == std::vector<ObjectID>{ scout });
	// the individually selected member speaks the same voice (its horde carries the list)
	r.sink.clear();
	CHECK(r.voice->pickAndPlay({ scout }, MSG_COMBINE_HORDES_WITH_OBJECT, &info));
	CHECK(r.sink.sounds == std::vector<std::string>{ "Test84LeaderJoin" });
	CHECK(r.sink.owners == std::vector<ObjectID>{ scout });
}

TEST_CASE("PORT-84: InitiateVoice:NoSound fills nothing, the member's row of VoiceCombineWithHorde answers (RW 0x8DF5EB)")
{
	VoiceRig r;
	const ObjectID target = r.make("Test84TargetHorde");
	Object *horde = r.horde("Test84DeafJoinHorde");
	REQUIRE(horde);
	const ObjectID scout = VoiceRig::memberWithTemplate(r.f, horde, "Test84Scout");
	REQUIRE(scout != INVALID_ID);
	UnitVoiceResponse::Info info;
	info.target = target;
	CHECK(r.voice->pickAndPlay({ horde->getID() }, MSG_COMBINE_HORDES_WITH_OBJECT, &info));
	CHECK(r.sink.sounds == std::vector<std::string>{ "Test84Join" });
	CHECK(r.sink.owners == std::vector<ObjectID>{ scout });
}

TEST_CASE("PORT-84: a horde without a ComboHorde entry answers with the members' rows, the better VoicePriority speaks (RW 0x8DD663 / 0x8DF5EB)")
{
	VoiceRig r;
	const ObjectID target = r.make("Test84TargetHorde");
	Object *horde = r.horde("Test84PlainHorde");
	REQUIRE(horde);
	const ObjectID elder = VoiceRig::memberWithTemplate(r.f, horde, "Test84Elder");
	const ObjectID scout = VoiceRig::memberWithTemplate(r.f, horde, "Test84Scout");
	REQUIRE(elder != INVALID_ID);
	REQUIRE(scout != INVALID_ID);
	REQUIRE(elder != scout);
	UnitVoiceResponse::Info info;
	info.target = target;
	CHECK(r.voice->pickAndPlay({ horde->getID() }, MSG_COMBINE_HORDES_WITH_OBJECT, &info));
	// both members fill their row; the higher VoicePriority (the elder, 20 > 10) wins, as for every bucket
	CHECK(r.sink.sounds == std::vector<std::string>{ "Test84ElderJoin" });
	CHECK(r.sink.owners == std::vector<ObjectID>{ elder });
}
