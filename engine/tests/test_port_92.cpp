// OpenBFME. PORT-92 tests: GiveUpgradeUpdate's SpawnOutFX, the porter's delivery effect.
//
// TARGET (RotWK game.dat): the delivery function RW 0x8A01B2 (triggerAbilityEffect) ends at RW 0x8A02BB with
// `push ebx (= m_object, the porter); call RW 0x89FE01`. RW 0x89FE01 is the closing part: RW 0x89FE12 reads
// ModuleData +0x04 (the module data), RW 0x89FE1C reads +0xE0 (SpawnOutFX, field table at RW 0xC67AE8:
// parse 0x73A302, zero default RW 0x89FC84; the INI name "SpawnOutFX" itself is at RW 0xC67AB4), RW 0x89FE22
// tests it, and when set RW 0x89FE26..RW 0x89FE35 pushes (fx, &owner->m_pos, 0, 0.0f, 0) and calls
// RW 0x494615 (FXList::doFXPos at the porter's position, no matrix). DONOR BFME2 0x0049C67E
// (Rva0049C67E::rva0049C67E, GiveUpgradeUpdate.cpp; the caller RW 0x8A02BB sits in RW 0x8A01B2).
// Tests are synthetic, no game files. Field table offsets were read from the image (RW 0xC67AD8..0xC67B18).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/PartitionManager.h"

#include <string>
#include <vector>

namespace
{
// one recorder per world: every effect call with the frame it was made in
struct Recorder : FXEventSink
{
	std::vector<FXEvent> events;
	void onFXEvent(const FXEvent &e) override { events.push_back(e); }
	std::vector<FXEvent> site(const char *name) const
	{
		std::vector<FXEvent> out;
		for (const FXEvent &e : events)
		{
			if (std::string(e.site) == name)
			{
				out.push_back(e);
			}
		}
		return out;
	}
};

const char kObjects[] =
	"SpecialPower TestPort92GivePower\n Enum = SPECIAL_GIVE_UPGRADE\n ReloadTime = 100\nEnd\n"
	"Object Port92Porter\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule PowerTag\n SpecialPowerTemplate = TestPort92GivePower\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = GiveUpgradeUpdate GiveTag\n SpecialPowerTemplate = TestPort92GivePower\n StartAbilityRange = 100\n"
	" IgnoreFacingCheck = Yes\n ApproachRequiresLOS = No\n UnpackTime = 100\n PreparationTime = 100\n PackTime = 100\n DeliverUpgrade = Yes\n"
	" SpawnOutFX = FX_Port92PorterSpawn\n End\nEnd\n"
	"Object Port92PorterNoField\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule PowerTag\n SpecialPowerTemplate = TestPort92GivePower\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = GiveUpgradeUpdate GiveTag\n SpecialPowerTemplate = TestPort92GivePower\n StartAbilityRange = 100\n"
	" IgnoreFacingCheck = Yes\n ApproachRequiresLOS = No\n UnpackTime = 100\n PreparationTime = 100\n PackTime = 100\n DeliverUpgrade = Yes\n End\nEnd\n"
	"Object Port92PorterNoneFX\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule PowerTag\n SpecialPowerTemplate = TestPort92GivePower\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = GiveUpgradeUpdate GiveTag\n SpecialPowerTemplate = TestPort92GivePower\n StartAbilityRange = 100\n"
	" IgnoreFacingCheck = Yes\n ApproachRequiresLOS = No\n UnpackTime = 100\n PreparationTime = 100\n PackTime = 100\n DeliverUpgrade = Yes\n"
	" SpawnOutFX = None\n End\nEnd\n"
	"Object Port92PorterManual\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule PowerTag\n SpecialPowerTemplate = TestPort92GivePower\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = GiveUpgradeUpdate GiveTag\n SpecialPowerTemplate = TestPort92GivePower\n StartAbilityRange = 100\n"
	" IgnoreFacingCheck = Yes\n ApproachRequiresLOS = No\n UnpackTime = 100\n PreparationTime = 100\n PackTime = 100\n DeliverUpgrade = No\n"
	" SpawnOutFX = FX_Port92PorterSpawn\n End\nEnd\n"
	"Object Port92Recipient\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = LocomotorSetUpgrade UpgradeTag\n TriggeredBy = Upgrade_Barracks2\n End\nEnd\n";

struct Port92World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previous = TheSpecialPowerStore;
	Port92World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string error = w.load(kObjects);
		REQUIRE_MESSAGE(error.empty(), error);
	}
	~Port92World() { TheSpecialPowerStore = previous; }
	// the porter delivers, fades and is gone; the events are in the recorder
	// the porter fades away during the frames (the delivery destroys it), so the caller gets its ID, taken before the frames, never the pointer
	Recorder run(const char *porterName, ObjectID *carrierIdOut = nullptr, Object **recipientOut = nullptr)
	{
		Object *carrier = unit(porterName, 'A', 300, 300);
		Object *recipient = unit("Port92Recipient", 'A', 305, 300);
		REQUIRE(carrier);
		REQUIRE(recipient);
		const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
		REQUIRE(upgrade);
		requireAffected(recipient, upgrade);
		const ObjectID carrierId = carrier->getID();
		carrier->giveUpgrade(upgrade);
		logic->partition().update();
		Recorder rec;
		logic->fxEvents().setSink(&rec);
		frames(120);
		logic->fxEvents().setSink(nullptr);
		if (carrierIdOut)
		{
			*carrierIdOut = carrierId;
		}
		if (recipientOut)
		{
			*recipientOut = recipient;
		}
		return rec;
	}
	void requireAffected(Object *recipient, const UpgradeTemplate *upgrade)
	{
		REQUIRE(recipient->affectedByUpgrade(upgrade));
	}
};
} // namespace

TEST_CASE("PORT-92 GiveUpgradeUpdate SpawnOutFX: an automatically delivering porter requests it once, at the porter, with no matrix (RW 0x8A02BB -> 0x89FE01, FX call RW 0x89FE1C..RW 0x89FE35 / RW 0x494615)")
{
	Port92World w;
	ObjectID carrierId = INVALID_ID;
	Object *recipient = nullptr;
	const Recorder rec = w.run("Port92Porter", &carrierId, &recipient);
	const std::vector<FXEvent> events = rec.site("GiveUpgradeUpdate SpawnOutFX");
	CAPTURE(events.size());
	// RW 0x89FE22 tests the FXList for 0 and calls it at most once per completed delivery; the porter fades once
	REQUIRE(events.size() == 1);
	const FXEvent &e = events[0];
	CHECK(e.kind == FXEvent::POSITION_FX); // FXList::doFXPos (RW 0x494615), not doFXObj (RW 0x4B1B5A)
	CHECK(*e.fxList == "FX_Port92PorterSpawn");
	CHECK(e.primary == carrierId);
	CHECK(e.position.x == 300.0f); // the porter's position (RW 0x89FE30: lea ecx,[edi+0x38], edi = owner), not the recipient's (305, 300)
	CHECK(e.position.y == 300.0f);
	CHECK_FALSE(e.hasTransform);           // RW 0x89FE2E: the matrix argument is 0
	CHECK(e.weaponSpeed == 0.0f); // fldz at RW 0x89FE28 (RW 0x494615 arg 4)
	CHECK_FALSE(e.hasSecondaryPosition);   // RW 0x89FE26: push 0 (RW 0x494615 arg 5)
	// the delivery itself happened on the same path (RW 0x8A01B2): the recipient got the upgrade and the porter faded away
	const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
	REQUIRE(upgrade);
	CHECK(recipient->hasUpgrade(upgrade));
	CHECK(w.logic->findObjectByID(carrierId) == nullptr);
}

TEST_CASE("PORT-92 GiveUpgradeUpdate SpawnOutFX: the explicit special power call requests it on the same path")
{
	Port92World w;
	Object *carrier = w.unit("Port92PorterManual", 'A', 300, 300);
	Object *recipient = w.unit("Port92Recipient", 'A', 305, 300);
	REQUIRE(carrier);
	REQUIRE(recipient);
	const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
	REQUIRE(upgrade);
	REQUIRE(recipient->affectedByUpgrade(upgrade));
	carrier->giveUpgrade(upgrade);
	Recorder rec;
	w.logic->fxEvents().setSink(&rec);
	auto *ability = dynamic_cast<SpecialAbilityUpdate *>(carrier->findModule("GiveUpgradeUpdate"));
	REQUIRE(ability);
	ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, recipient, nullptr, 0, 0);
	const ObjectID carrierId = carrier->getID(); // the porter may fade during the frames
	w.frames(20);
	w.logic->fxEvents().setSink(nullptr);
	const std::vector<FXEvent> events = rec.site("GiveUpgradeUpdate SpawnOutFX");
	REQUIRE(events.size() == 1);
	CHECK(events[0].kind == FXEvent::POSITION_FX);
	CHECK(events[0].primary == carrierId);
	CHECK(events[0].position.x == 300.0f);
	CHECK(*events[0].fxList == "FX_Port92PorterSpawn");
	CHECK(recipient->hasUpgrade(upgrade));
}

TEST_CASE("PORT-92 GiveUpgradeUpdate SpawnOutFX: no SpawnOutFX field, the delivery still completes and nothing is requested")
{
	Port92World w;
	ObjectID carrierId = INVALID_ID;
	Object *recipient = nullptr;
	const Recorder rec = w.run("Port92PorterNoField", &carrierId, &recipient);
	CHECK(rec.site("GiveUpgradeUpdate SpawnOutFX").empty()); // no field: no call (RW 0x89FE22 on a zero +0xE0)
	const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
	REQUIRE(upgrade);
	CHECK(recipient->hasUpgrade(upgrade));                   // the delivery itself is unaffected
	CHECK(w.logic->findObjectByID(carrierId) == nullptr);
}

TEST_CASE("PORT-92 GiveUpgradeUpdate SpawnOutFX: \"SpawnOutFX = None\" parses to nothing and nothing is requested")
{
	Port92World w;
	ObjectID carrierId = INVALID_ID;
	Object *recipient = nullptr;
	const Recorder rec = w.run("Port92PorterNoneFX", &carrierId, &recipient);
	CHECK(rec.site("GiveUpgradeUpdate SpawnOutFX").empty()); // "None" stores no FXList (RW 0x73A302; module data +0xE0 stays 0)
	const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
	REQUIRE(upgrade);
	CHECK(recipient->hasUpgrade(upgrade));
	CHECK(w.logic->findObjectByID(carrierId) == nullptr);
}
