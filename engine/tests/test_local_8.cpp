// OpenBFME unit tests: LOCAL-8 hero respawn (issue #113, TOP20 #25).
//
// Editions: both (BFME2 1.06 and RotWK 2.01 are byte-for-byte identical across RespawnUpdate, delta RW - 0x4106 = BFME2).
//
// RotWK / BFME2 addresses:
//   RespawnUpdateModuleData::buildFieldParse: RW 0x8B3DD7 / BFME2 0x8AFCD1 (table RW 0xC6CE80 / BFME2 0xC561D0)
//     DeathAnimationTime (+0xFC, RW 0xC6CEA0 / BFME2 0xC561F0, parseDurationUnsignedInt RW 0x73A429; parsed-unused in both binaries)
//     AutoRespawnAtObjectFilter (+0x08, RW 0xC6CF10 / BFME2 0xC56260, parseObjectFilter RW 0x76392F)
//     ButtonImage (+0x118, RW 0xC6CF40 / BFME2 0xC56290, parseAsciiString RW 0x42EE5E)
//   RespawnUpdate::getDeadImage: RW 0x8B33E4 .. 0x8B34BC / BFME2 0x8AF2DE (primary vtable 0xC6C360 slot 14 / +0x38)
//   HeroRecord::HeroRecord(const Object*): RW 0x780DD4 (at RW 0x780EEE .. 0x780F06 stores getDeadImage in HeroRecord + 0x00)
//   HeroRecord::xfer: RW 0x7808FD (at RW 0x780A29 .. 0x780A79 transfers HeroRecord + 0x00 as AsciiString)
//   HeroRecord::getButtonImage: RW 0x7810BC / BFME2 0x77DDAC (at RW 0x7810CA .. 0x7810D5 returns HeroRecord + 0x00 when dead + 0xB0 is set)
//   Living World dead hero record setup: RW 0x6E2E18 (at RW 0x6E2F59 .. 0x6E2FB3 reads RespawnUpdateModuleData + 0x118 into HeroRecord + 0x00)

#include "doctest.h"

#include "ProdTestUtil.h"

#include "Common/PlayerHeroList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GUI/Image.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/HeroSystem.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/HeroModules.h"

namespace
{
const char kLocal8ButtonObjects[] =
	"Object Local8RecallHero\n"
	"  ButtonImage = HIHeroAlive\n"
	"  SelectPortrait = HPHeroPortrait\n"
	"  BuildCost = 1100\n"
	"  BuildTime = 20.0\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = RespawnBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"  End\n"
	"  Behavior = RespawnUpdate ModuleTag_Respawn\n"
	"    DeathAnim = DYING\n"
	"    DeathFX = None\n"
	"    DeathAnimationTime = 1133\n"
	"    RespawnAnim = LEVELED\n"
	"    RespawnAnimationTime = 2000\n"
	"    ButtonImage = HIHeroRecall\n"
	"    RespawnRules = AutoSpawn:No Cost:550 Time:40000 Health:100%\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Local8NoObjButtonHero\n"
	"  BuildCost = 900\n"
	"  BuildTime = 15.0\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = RespawnBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = RespawnUpdate ModuleTag_Respawn\n"
	"    DeathAnim = DYING\n"
	"    DeathFX = None\n"
	"    RespawnAnim = LEVELED\n"
	"    RespawnAnimationTime = 1500\n"
	"    ButtonImage = HIWormtongueOnly\n"
	"    RespawnRules = AutoSpawn:No Cost:450 Time:30000 Health:100%\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Local8Fortress\n"
	"  KindOf = STRUCTURE SELECTABLE CASTLE_KEEP\n"
	"  CommandSet = Local8FortressSet\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    MaxQueueEntries = 5\n"
	"  End\n"
	"  Behavior = QueueProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:-20.0 Z:0.0\n"
	"    NaturalRallyPoint = X:30.0 Y:-50.0 Z:0.0\n"
	"  End\n"
	"End\n";

const char kLocal8ButtonCommands[] =
	"CommandButton Command_Local8Revive0\n"
	"  Command = REVIVE\n"
	"  Options = CANCELABLE\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_Local8Revive1\n"
	"  Command = REVIVE\n"
	"  Options = CANCELABLE\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandSet Local8FortressSet\n"
	"  InitialVisible = 6\n"
	"  1 = Command_Local8Revive0\n"
	"  2 = Command_Local8Revive1\n"
	"End\n";

void killHero(Object *victim)
{
	DamageInfo d;
	d.m_input.m_sourceID = 0;
	d.m_input.m_amount = 100000.0f;
	d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	victim->attemptDamage(d);
}
} // namespace

TEST_CASE("LOCAL-8 respawn ButtonImage: dead hero record stores RespawnUpdate ButtonImage, ControlBar shows it on REVIVE button and falls back when unmapped, xfer and crc preserve it (RW 0x8B33E4 / 0x780F00 / 0x7810BC / 0x780A29)")
{
	prodtest::ProdWorld f;
	f.logic->settings().bodyThresholdsLoaded = true;
	f.logic->settings().unitDamagedThreshold = 0.7f;
	f.logic->settings().unitReallyDamagedThreshold = 0.35f;
	f.load(kLocal8ButtonObjects);
	f.load(kLocal8ButtonCommands);

	Player *alice = f.alice();
	REQUIRE(alice != nullptr);
	f.players.setLocalPlayer(alice);
	PlayerTemplate *pt = const_cast<PlayerTemplate *>(alice->getPlayerTemplate());
	REQUIRE(pt != nullptr);
	pt->m_buildableHeroesMP = { "Local8RecallHero", "Local8NoObjButtonHero" };

	Object *fortress = f.make("Local8Fortress", f.teamOf("Alice"));
	REQUIRE(fortress != nullptr);

	// Purchase records before any hero has spawned or died: !r.dead uses Object/ButtonImage
	alice->heroes().addPurchase(*f.w.get("Local8RecallHero"));
	alice->heroes().addPurchase(*f.w.get("Local8NoObjButtonHero"));
	REQUIRE(alice->heroes().size() == 2u);
	CHECK_FALSE(alice->heroes().at(0)->dead);
	CHECK(alice->heroes().at(0)->deadImage.empty());

	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *f.logic, nullptr, view, ui, stream, &f.commands, MouseSettings{} };
	ControlBar bar(ctx);
	ui.selectObject(fortress->getID());
	bar.update();

	REQUIRE(bar.palantirButtons().size() == 2u);
	CHECK(bar.palantirButtons()[0].image == "HIHeroAlive");
	CHECK(bar.palantirButtons()[1].image == "");

	// Spawn both heroes and kill them: onDeath (RW 0x8B3744) -> addDeadHero (RW 0x780DD4) -> getDeadImage (RW 0x8B33E4)
	alice->heroes().clear();
	Object *hero0 = f.make("Local8RecallHero", f.teamOf("Alice"));
	Object *hero1 = f.make("Local8NoObjButtonHero", f.teamOf("Alice"));
	REQUIRE(hero0 != nullptr);
	REQUIRE(hero1 != nullptr);
	killHero(hero0);
	killHero(hero1);

	REQUIRE(alice->heroes().size() == 2u);
	CHECK(alice->heroes().at(0)->dead);
	CHECK(alice->heroes().at(0)->deadImage == "HIHeroRecall");
	CHECK(alice->heroes().at(1)->dead);
	CHECK(alice->heroes().at(1)->deadImage == "HIWormtongueOnly");

	// ControlBar::heroRecordImage (RW 0x7810BC at RW 0x7810CA..0x7810D5) returns r.deadImage for dead records
	bar.update();
	REQUIRE(bar.palantirButtons().size() == 2u);
	CHECK(bar.palantirButtons()[0].image == "HIHeroRecall");
	CHECK(bar.palantirButtons()[1].image == "HIWormtongueOnly");

	// RW 0x8B348F .. 0x8B34B1: when MappedImageCollection is attached and RespawnUpdate/ButtonImage is absent from it (e.g. EvilMenBlackRider's HIBlackRider_res),
	// falls back to the template's ButtonImage ("HIHeroAlive"); once the mapped image exists, returns "HIHeroRecall"
	MappedImageCollection images;
	{
		Image img;
		img.name = "HIHeroAlive";
		images.addImage(img);
		img.name = "HIWormtongueOnly";
		images.addImage(img);
	}
	ctx.images = &images;
	bar.update();
	REQUIRE(bar.palantirButtons().size() == 2u);
	CHECK(bar.palantirButtons()[0].image == "HIHeroAlive");
	CHECK(bar.palantirButtons()[1].image == "HIWormtongueOnly");

	{
		Image img;
		img.name = "HIHeroRecall";
		images.addImage(img);
	}
	bar.update();
	CHECK(bar.palantirButtons()[0].image == "HIHeroRecall");

	// State hash and xfer round-trip (RW 0x780A29 .. 0x780A79)
	const std::uint32_t hashBefore = f.logic->computeStateHash();
	alice->heroes().at(0)->deadImage = "HIHeroAlt";
	CHECK(f.logic->computeStateHash() != hashBefore);
	alice->heroes().at(0)->deadImage = "HIHeroRecall";
	CHECK(f.logic->computeStateHash() == hashBefore);

	XferSave saver;
	alice->heroes().xfer(saver);
	PlayerHeroList loaded;
	XferLoad loader(saver.data());
	loaded.xfer(loader);
	REQUIRE(loaded.size() == 2u);
	CHECK(loaded.at(0)->dead);
	CHECK(loaded.at(0)->deadImage == "HIHeroRecall");
	CHECK(loaded.at(1)->dead);
	CHECK(loaded.at(1)->deadImage == "HIWormtongueOnly");
}

TEST_CASE("LOCAL-8 respawn ButtonImage: Living World dead hero record populates deadImage from RespawnUpdateModuleData +0x118 (RW 0x6E2F82 .. 0x6E2FB3)")
{
	prodtest::ProdWorld f;
	f.load(kLocal8ButtonObjects);
	Player *alice = f.alice();
	REQUIRE(alice != nullptr);

	std::vector<HeroSystem::LivingWorldHero> wotrHeroes = {
		{ "Local8RecallHero", "hero_1", 50.0f, true },
		{ "Local8NoObjButtonHero", "hero_2", 0.0f, true },
	};
	std::vector<std::string> errors;
	HeroSystem::initLivingWorldPlayer(*f.logic, *alice, wotrHeroes, 1, errors);
	CHECK(errors.empty());
	REQUIRE(alice->heroes().size() == 2u);
	CHECK(alice->heroes().at(0)->dead);
	CHECK(alice->heroes().at(0)->deadImage == "HIHeroRecall");
	CHECK(alice->heroes().at(1)->dead);
	CHECK(alice->heroes().at(1)->deadImage == "HIWormtongueOnly");
}
