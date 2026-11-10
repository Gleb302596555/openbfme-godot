// OpenBFME. PORT-85 (issue #233): UnitSpecificSounds VoicePrimaryWeaponMode / VoiceSecondaryWeaponMode — RotWK 2.01 verdict, pinned.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001):
//   * "VoicePrimaryWeaponMode" (RW 0xC77B14), "VoiceSecondaryWeaponMode" (RW 0xC77B2C) and "VoiceTertiaryWeaponMode" (RW 0xC77B48)
//     appear once each in .rdata and have a single .text reference each: RW 0x8DF7C9 (slot 0), RW 0x8DF7A4 (slot 1) and RW 0x8DF776
//     (slot 2), all inside RW 0x8DF74F — the MSG_SWITCH_WEAPONS (0x43A) arm of PickAndPlayUnitVoiceResponse (RW 0x8DEDBB, jump tables
//     RW 0x8E01CD index 0x51 -> RW 0x8E0191 selector 0xD, UnitSpecificSounds lookup RW 0x8DDB2A -> RW 0x674FFE -> RW 0x73EDD0).
//   * Only two commands post 0x43A and call RW 0x8DEDBB(0x43A): GUI_COMMAND_SWITCH_WEAPON (0x1B, RW 0x941204, voice call RW 0x941242)
//     and GUI_COMMAND_TOGGLE_WEAPON (0x23, RW 0x941257, voice call RW 0x94129E), whose ControlBar availability rules (RW 0x94375C /
//     RW 0x9437E7) belong to S-290.
//   * Infantry / cavalry bow <-> sword toggling uses GUI_COMMAND_TOGGLE_WEAPONSET (0x2D, RW 0x9412C8), which posts MSG_WEAPONSET_TOGGLE
//     (0x457 = 1111, RW 0x9412D0 — sole push 0x457 in game.dat) with the context ObjectID (RW 0x71111A) and jumps to RW 0x941A22 ->
//     RW 0x75D22E(sourceObj, 1), updating only CommandButton frame index +0xFC and exiting at RW 0x941ABB with zero audio calls.
//   * RW 0x8DEDBB does not handle 0x457: none of its 56 callers passes 0x457, and at RW 0x8DF28D..0x8DF2A9 (0x457 < 0x461, 0x457 - 0x3E9
//     = 0x6E > 0x6D) it jumps straight to default no-voice exit RW 0x8DFEA4. Logic execution of 0x457 (RW 0x77B529..0x77B6AC ->
//     RW 0x691059 / 0x691106 / 0x6945DA / 0x694636 -> RW 0x6C99E2) toggles WEAPONSET_TOGGLE_1 (24) with zero voice calls.
//
// Full map: docs/ennorath/port85-weapon-mode-voice-rw.md; project stop S-7285 in docs/STOPS.md. No engine behaviour changed.
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "GameClient/ControlBar.h"
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/WeaponSetToggle.h"

#include <set>
#include <string>
#include <vector>

namespace
{
struct RecordingVoiceSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds;
	std::vector<std::string> eva;
	std::vector<ObjectID> speakers;

	bool soundExists(const std::string &name) override { return known.count(name) == 1; }
	void playSound(const std::string &name, ObjectID object, int) override
	{
		sounds.push_back(name);
		speakers.push_back(object);
	}
	void reportEva(const std::string &eventName, const Coord3D *) override { eva.push_back(eventName); }
	void clear()
	{
		sounds.clear();
		eva.clear();
		speakers.clear();
	}
};

std::string makePort85ObjectsIni(bool includeModeVoices)
{
	const std::string modeVoicesBlock = includeModeVoices
		? "  UnitSpecificSounds\n"
		  "    VoicePrimaryWeaponMode = SynthGaladhrimBowMode\n"
		  "    VoiceSecondaryWeaponMode = SynthGaladhrimSwordMode\n"
		  "    VoiceTertiaryWeaponMode = SynthGaladhrimAltMode\n"
		  "  End\n"
		: "";
	return
		"Object SynthGaladhrim\n"
		"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
		"  VisionRange = 200\n"
		"  CommandSet = SynthToggleWeaponSetBar\n"
		"  VoicePriority = 30\n"
		"  VoiceSelect = SynthGaladhrimSelect\n"
		"  VoiceMove = SynthGaladhrimMove\n"
		+ modeVoicesBlock +
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryMinorRadius = 8\n"
		"  GeometryHeight = 20\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY BowWeapon\n"
		"  End\n"
		"  WeaponSet\n"
		"    Conditions = WEAPONSET_TOGGLE_1\n"
		"    Weapon = PRIMARY SwordWeapon\n"
		"  End\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = PlainArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"    InitialHealth = 100\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  LocomotorSet\n"
		"    Locomotor = WalkerLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 55\n"
		"  End\n"
		"End\n"
		"Object SynthGaladhrimHorde\n"
		"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
		"  VisionRange = 200\n"
		"  CommandSet = SynthToggleWeaponSetBar\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 30\n"
		"  GeometryMinorRadius = 45\n"
		"  GeometryHeight = 20\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY HordeRangefinder\n"
		"  End\n"
		"  Body = ImmortalBody ModuleTag_Body\n"
		"    MaxHealth = 1\n"
		"  End\n"
		"  Behavior = HordeAIUpdate ModuleTag_AI\n"
		"  End\n"
		"  Behavior = HordeContain ModuleTag_Contain\n"
		"    RankInfo = RankNumber:1 UnitType:SynthGaladhrim Position:X:30 Y:0 Position:X:30 Y:20\n"
		"    InitialPayload = SynthGaladhrim 2\n"
		"  End\n"
		"  LocomotorSet\n"
		"    Locomotor = HordeLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 50\n"
		"  End\n"
		"End\n";
}

struct Port85Rig
{
	combattest::CombatWorld world;
	CommandStore commandStore;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 0.0f, -300.0f, 400.0f }, { 100.0f, 100.0f, 0.0f }, 1024, 768, 0.7f };
	HudInput input;
	ControlBar bar;
	RecordingVoiceSink sink;
	UnitVoiceResponse voice;
	CommandButton toggleWeaponSetBtn;
	CommandButton toggleWeaponBtn;
	CommandButton switchWeaponBtn;
	std::vector<int> observedMessageTypes;

	explicit Port85Rig(bool includeModeVoices = true)
		: world(makePort85ObjectsIni(includeModeVoices).c_str(), 40)
		, input(*world.logic, world.ai.get(), view, world.commands, mouse, metaMap)
		, bar(input.context())
		, voice(*world.logic, sink)
	{
		WeaponSetToggle::registerHandlers(world.dispatcher);
		sink.known = {
			"SynthGaladhrimSelect",
			"SynthGaladhrimMove",
			"SynthGaladhrimBowMode",
			"SynthGaladhrimSwordMode",
			"SynthGaladhrimAltMode",
			"SynthStanceLine",
		};
		Player *alice = world.players.findPlayerWithName("Alice");
		REQUIRE(alice);
		world.players.setLocalPlayer(alice);
		voice.setLocalPlayerIndex(alice->getPlayerIndex());

		toggleWeaponSetBtn.m_name = "Command_SynthToggleWeaponSet";
		toggleWeaponSetBtn.m_command = GUI_COMMAND_TOGGLE_WEAPONSET;
		toggleWeaponSetBtn.m_inPalantir = true;

		toggleWeaponBtn.m_name = "Command_SynthToggleCatapultWeapon";
		toggleWeaponBtn.m_command = GUI_COMMAND_TOGGLE_WEAPON;
		toggleWeaponBtn.m_weaponSlot = SECONDARY_WEAPON;
		toggleWeaponBtn.m_inPalantir = true;

		switchWeaponBtn.m_name = "Command_SynthSwitchWeapon";
		switchWeaponBtn.m_command = GUI_COMMAND_SWITCH_WEAPON;
		switchWeaponBtn.m_weaponSlot = PRIMARY_WEAPON;
		switchWeaponBtn.m_inPalantir = true;

		CommandSet *cs = commandStore.newDynamicCommandSet("SynthToggleWeaponSetBar");
		cs->setDynamicButton(0, &toggleWeaponSetBtn);
		cs->setDynamicButton(1, &toggleWeaponBtn);
		cs->setDynamicButton(2, &switchWeaponBtn);
		input.context().commands = &commandStore;

		input.setLogicMessageObserver([this](const GameMessage &msg) {
			observedMessageTypes.push_back(msg.getType());
			PlayUnitVoiceForMessage(voice, input.ui().selected(), msg);
		});
		AudioApi::installUnitVoiceHandler([this](const AudioApi::UnitVoiceParams &p) {
			UnitVoiceResponse::Info info;
			info.producer = (ObjectID)p.producerId;
			info.powerTemplate = p.powerTemplate;
			info.target = (ObjectID)p.targetId;
			info.hasPosition = p.hasPosition;
			info.position = Coord3D{ p.posX, p.posY, p.posZ };
			voice.pickAndPlay({ (ObjectID)p.objectId }, p.voiceEvent, &info);
		}, this);
	}

	~Port85Rig()
	{
		AudioApi::uninstallUnitVoiceHandler(this);
	}

	void selectUnit(Object &obj)
	{
		input.ui().deselectAll(true);
		input.ui().selectObject(obj.getID());
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, world.playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		sel.appendObjectIDArgument(obj.getID());
		input.submit(sel);
		input.flush();
		world.frames(1);
		bar.update();
	}

	bool hasToggleFlag(const Object &obj) const
	{
		const int bit = WeaponSetToggle::WEAPONSET_TOGGLE_1;
		return (obj.getWeaponSetFlags()[(size_t)bit / 32] & (1u << (bit % 32))) == (1u << (bit % 32));
	}

	std::string primaryWeaponName(const Object &obj) const
	{
		const ObjectWeapons *weapons = obj.getWeapons();
		if (!weapons)
		{
			return std::string();
		}
		const Weapon *w = weapons->weaponInSlot(PRIMARY_WEAPON);
		if (!w || !w->getTemplate())
		{
			return std::string();
		}
		return w->getTemplate()->getName();
	}
};
} // namespace

TEST_CASE("PORT-85 verdict: UnitSpecificSounds stores VoicePrimaryWeaponMode and VoiceSecondaryWeaponMode, while UnitVoiceResponse rejects 0x457 and leaves 0x43A in S-703")
{
	Port85Rig voicedRig(true);
	Port85Rig plainRig(false);
	Object *voiced = voicedRig.world.unit("SynthGaladhrim", 'A', 100.0f, 100.0f);
	Object *plain = plainRig.world.unit("SynthGaladhrim", 'A', 100.0f, 100.0f);
	REQUIRE(voiced);
	REQUIRE(plain);
	voicedRig.world.frames(2);
	plainRig.world.frames(2);

	const ThingTemplate *voicedTmpl = voiced->getTemplate();
	const ThingTemplate *plainTmpl = plain->getTemplate();
	REQUIRE(voicedTmpl);
	REQUIRE(plainTmpl);

	// ThingTemplate's UnitSpecificSounds map (RW 0x73EDD0) parses and stores all three weapon-mode keys
	CHECK(UnitVoiceResponse::unitSpecificVoice(*voicedTmpl, "VoicePrimaryWeaponMode").sound == "SynthGaladhrimBowMode");
	CHECK(UnitVoiceResponse::unitSpecificVoice(*voicedTmpl, "VoiceSecondaryWeaponMode").sound == "SynthGaladhrimSwordMode");
	CHECK(UnitVoiceResponse::unitSpecificVoice(*voicedTmpl, "VoiceTertiaryWeaponMode").sound == "SynthGaladhrimAltMode");
	CHECK(voicedRig.voice.objectNameVoice(*voiced, "VoicePrimaryWeaponMode").sound == "SynthGaladhrimBowMode");
	CHECK(voicedRig.voice.objectNameVoice(*voiced, "VoiceSecondaryWeaponMode").sound == "SynthGaladhrimSwordMode");

	CHECK_FALSE(UnitVoiceResponse::unitSpecificVoice(*plainTmpl, "VoicePrimaryWeaponMode").any());
	CHECK_FALSE(UnitVoiceResponse::unitSpecificVoice(*plainTmpl, "VoiceSecondaryWeaponMode").any());
	CHECK_FALSE(UnitVoiceResponse::unitSpecificVoice(*plainTmpl, "VoiceTertiaryWeaponMode").any());

	// RW 0x8DF28D..0x8DF2A9: 0x457 (MSG_WEAPONSET_TOGGLE) -> 0x457 - 0x3E9 = 0x6E > 0x6D -> ja 0x8DFEA4 (not handled)
	CHECK_FALSE(UnitVoiceResponse::handles(MSG_WEAPONSET_TOGGLE));
	GameMessage toggleMsg(MSG_WEAPONSET_TOGGLE, voicedRig.world.playerIndex("Alice"));
	toggleMsg.appendObjectIDArgument(voiced->getID());
	PlayUnitVoiceForMessage(voicedRig.voice, { voiced->getID() }, toggleMsg);
	CHECK(voicedRig.voice.stats().calls == 0);
	CHECK(voicedRig.sink.sounds.empty());
	CHECK(voicedRig.sink.eva.empty());

	CHECK_FALSE(voicedRig.voice.pickAndPlay({ voiced->getID() }, MSG_WEAPONSET_TOGGLE));
	CHECK(voicedRig.sink.sounds.empty());
	CHECK(voicedRig.sink.eva.empty());

	// RW 0x8DF74F: 0x43A (MSG_SWITCH_WEAPONS) is the sole RW reader of Voice*WeaponMode and remains unported under S-703
	CHECK_FALSE(UnitVoiceResponse::handles(MSG_SWITCH_WEAPONS));
	const std::uint64_t unportedBefore = voicedRig.voice.stats().unportedMessages;
	CHECK_FALSE(voicedRig.voice.pickAndPlay({ voiced->getID() }, MSG_SWITCH_WEAPONS));
	CHECK(voicedRig.voice.stats().unportedMessages == unportedBefore + 1);
	CHECK(voicedRig.sink.sounds.empty());

	bool foundS703WeaponModes = false;
	for (const std::string &line : UnitVoiceResponse::acceptanceStops())
	{
		if (line.rfind("[S-703]", 0) == 0 && line.find("weapon modes (0x43A)") != std::string::npos)
		{
			foundS703WeaponModes = true;
		}
	}
	CHECK(foundS703WeaponModes);
}

TEST_CASE("PORT-85 verdict: GUI_COMMAND_TOGGLE_WEAPONSET switches a unit and a horde between bow and sword without playing VoicePrimaryWeaponMode or VoiceSecondaryWeaponMode (RW 0x9412C8 -> 0x941A22, RW 0x77B529)")
{
	SUBCASE("single unit with and without Voice*WeaponMode")
	{
		Port85Rig voicedRig(true);
		Port85Rig plainRig(false);
		Object *voiced = voicedRig.world.unit("SynthGaladhrim", 'A', 100.0f, 100.0f);
		Object *plain = plainRig.world.unit("SynthGaladhrim", 'A', 100.0f, 100.0f);
		REQUIRE(voiced);
		REQUIRE(plain);
		voicedRig.world.frames(2);
		plainRig.world.frames(2);

		voicedRig.selectUnit(*voiced);
		plainRig.selectUnit(*plain);
		CHECK(voicedRig.sink.sounds == std::vector<std::string>{ "SynthGaladhrimSelect" });
		voicedRig.sink.clear();
		plainRig.sink.clear();
		voicedRig.observedMessageTypes.clear();
		plainRig.observedMessageTypes.clear();

		CHECK_FALSE(voicedRig.hasToggleFlag(*voiced));
		CHECK(voicedRig.primaryWeaponName(*voiced) == "BowWeapon");
		CHECK(voicedRig.world.logic->computeStateHash() == plainRig.world.logic->computeStateHash());

		// First press: bow -> sword (WEAPONSET_TOGGLE_1 set via MSG_WEAPONSET_TOGGLE 0x457); client RW 0x941A22 and logic RW 0x77B529 play no voice
		REQUIRE(voicedRig.bar.palantirButtons().size() == 3);
		CHECK(voicedRig.bar.palantirButtons()[0].state == ButtonState::Enabled);
		REQUIRE(voicedRig.bar.pressButton(0, true));
		REQUIRE(plainRig.bar.pressButton(0, true));
		CHECK(voicedRig.input.update() == 1);
		CHECK(plainRig.input.update() == 1);
		CHECK(voicedRig.observedMessageTypes == std::vector<int>{ MSG_WEAPONSET_TOGGLE });
		CHECK(voicedRig.sink.sounds.empty());
		CHECK(voicedRig.sink.eva.empty());

		voicedRig.world.frames(1);
		plainRig.world.frames(1);
		AudioApi::drainDeferred();
		CHECK(voicedRig.hasToggleFlag(*voiced));
		CHECK(voicedRig.primaryWeaponName(*voiced) == "SwordWeapon");
		CHECK(voicedRig.sink.sounds.empty());
		CHECK(voicedRig.sink.eva.empty());
		CHECK(voicedRig.world.logic->computeStateHash() == plainRig.world.logic->computeStateHash());

		// Second press: sword -> bow (WEAPONSET_TOGGLE_1 cleared via RW 0x691106); still silent
		voicedRig.bar.update();
		plainRig.bar.update();
		REQUIRE(voicedRig.bar.pressButton(0, true));
		REQUIRE(plainRig.bar.pressButton(0, true));
		CHECK(voicedRig.input.update() == 1);
		CHECK(plainRig.input.update() == 1);
		voicedRig.world.frames(1);
		plainRig.world.frames(1);
		AudioApi::drainDeferred();
		CHECK_FALSE(voicedRig.hasToggleFlag(*voiced));
		CHECK(voicedRig.primaryWeaponName(*voiced) == "BowWeapon");
		CHECK(voicedRig.sink.sounds.empty());
		CHECK(voicedRig.sink.eva.empty());
		CHECK(voicedRig.world.logic->computeStateHash() == plainRig.world.logic->computeStateHash());
	}

	SUBCASE("horde whose members carry VoicePrimaryWeaponMode and VoiceSecondaryWeaponMode")
	{
		Port85Rig voicedRig(true);
		Port85Rig plainRig(false);
		Object *voicedHorde = voicedRig.world.unit("SynthGaladhrimHorde", 'A', 120.0f, 120.0f);
		Object *plainHorde = plainRig.world.unit("SynthGaladhrimHorde", 'A', 120.0f, 120.0f);
		REQUIRE(voicedHorde);
		REQUIRE(plainHorde);
		voicedRig.world.frames(2);
		plainRig.world.frames(2);

		const std::vector<Object *> members = voicedRig.world.membersOf(voicedHorde);
		REQUIRE(members.size() == 2);

		voicedRig.selectUnit(*voicedHorde);
		plainRig.selectUnit(*plainHorde);
		// Horde selection picks the members' VoiceSelect (RW 0x8DEE5B..0x8DEF45), proving horde member voice routing is live
		CHECK(voicedRig.sink.sounds == std::vector<std::string>{ "SynthGaladhrimSelect" });
		voicedRig.sink.clear();
		plainRig.sink.clear();

		REQUIRE(voicedRig.bar.pressButton(0, true));
		REQUIRE(plainRig.bar.pressButton(0, true));
		CHECK(voicedRig.input.update() == 1);
		CHECK(plainRig.input.update() == 1);
		voicedRig.world.frames(1);
		plainRig.world.frames(1);
		AudioApi::drainDeferred();

		CHECK(voicedRig.hasToggleFlag(*voicedHorde));
		for (const Object *m : members)
		{
			CHECK(voicedRig.hasToggleFlag(*m));
			CHECK(voicedRig.primaryWeaponName(*m) == "SwordWeapon");
		}
		CHECK(voicedRig.sink.sounds.empty());
		CHECK(voicedRig.sink.eva.empty());
		CHECK(voicedRig.world.logic->computeStateHash() == plainRig.world.logic->computeStateHash());

		voicedRig.bar.update();
		plainRig.bar.update();
		REQUIRE(voicedRig.bar.pressButton(0, true));
		REQUIRE(plainRig.bar.pressButton(0, true));
		CHECK(voicedRig.input.update() == 1);
		CHECK(plainRig.input.update() == 1);
		voicedRig.world.frames(1);
		plainRig.world.frames(1);
		AudioApi::drainDeferred();

		CHECK_FALSE(voicedRig.hasToggleFlag(*voicedHorde));
		for (const Object *m : members)
		{
			CHECK_FALSE(voicedRig.hasToggleFlag(*m));
			CHECK(voicedRig.primaryWeaponName(*m) == "BowWeapon");
		}
		CHECK(voicedRig.sink.sounds.empty());
		CHECK(voicedRig.sink.eva.empty());
		CHECK(voicedRig.world.logic->computeStateHash() == plainRig.world.logic->computeStateHash());
	}
}

TEST_CASE("PORT-85 S-290 boundary and neighbour voices: SWITCH_WEAPON and TOGGLE_WEAPON stay restricted (S-290), while select, move and stance voices on the same unit still play")
{
	Port85Rig rig(true);
	Object *voiced = rig.world.unit("SynthGaladhrim", 'A', 100.0f, 100.0f);
	REQUIRE(voiced);
	rig.world.frames(2);

	rig.selectUnit(*voiced);
	CHECK(rig.sink.sounds == std::vector<std::string>{ "SynthGaladhrimSelect" });
	rig.sink.clear();
	rig.observedMessageTypes.clear();

	// S-290: ControlBar restricts GUI_COMMAND_TOGGLE_WEAPON (RW 0x9437E7) and GUI_COMMAND_SWITCH_WEAPON (RW 0x94375C); pressing emits no 0x43A and no voice
	REQUIRE(rig.bar.palantirButtons().size() == 3);
	CHECK(rig.bar.palantirButtons()[1].state == ButtonState::Restricted);
	CHECK(rig.bar.palantirButtons()[2].state == ButtonState::Restricted);
	CHECK_FALSE(rig.bar.pressButton(1, true));
	CHECK_FALSE(rig.bar.pressButton(2, true));
	CHECK(rig.input.update() == 0);
	CHECK(rig.observedMessageTypes.empty());
	CHECK(rig.sink.sounds.empty());

	// Neighbouring unit voices on the same template still play untouched
	GameMessage moveMsg(MSG_DO_MOVETO, rig.world.playerIndex("Alice"));
	moveMsg.appendLocationArgument(Coord3D{ 180.0f, 100.0f, 0.0f });
	rig.input.submit(moveMsg);
	rig.input.flush();
	CHECK(rig.sink.sounds == std::vector<std::string>{ "SynthGaladhrimMove" });
	rig.sink.clear();

	UnitVoiceResponse::Info stanceInfo;
	stanceInfo.commandSound = "SynthStanceLine";
	CHECK(rig.voice.pickAndPlay({ voiced->getID() }, UnitVoiceResponse::VOICE_EVENT_STANCE, &stanceInfo));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "SynthStanceLine" });
}
