// Both editions: RW 0x8B80D3 / 0x8B80F6 and BFME2 0x8B3F58 / 0x8B3F7B.
#include "doctest.h"
#include "MoveTestUtil.h"
#include "Common/Upgrade.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/LocomotorSetUpgrade.h"

namespace {
const char kObjects[] =
    "Object TestLocomotorUpgrade\n KindOf = INFANTRY SELECTABLE\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " Behavior = LocomotorSetUpgrade Upgrade\n TriggeredBy = Upgrade_Barracks2\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL_UPGRADED\n Speed = 80\n End\nEnd\n"
    "Object TestKillLocomotorUpgrade\n KindOf = INFANTRY SELECTABLE\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " Behavior = LocomotorSetUpgrade Upgrade\n TriggeredBy = Upgrade_Barracks2\n KillLocomotorUpgrade = Yes\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL_UPGRADED\n Speed = 80\n End\nEnd\n";
}

TEST_CASE("modgap2 locomotor upgrade: grant and removal switch the normal set in both editions")
{
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        movetest::MoveWorld w;
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        const std::string error = w.w.load(kObjects);
        REQUIRE_MESSAGE(error.empty(), error);
        w.buildMap();
        Object *obj = w.spawn("TestLocomotorUpgrade", 300, 300, 0, w.teamOf("Alice"));
        REQUIRE(obj);
        AIUpdateInterface *ai = obj->getAIUpdateInterface();
        REQUIRE(ai);
        REQUIRE(dynamic_cast<LocomotorSetUpgrade *>(obj->findModule("LocomotorSetUpgrade")));
        const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_Barracks2");
        REQUIRE(upgrade);
        CHECK(ai->curLocomotorSet() == LOCOMOTORSET_NORMAL);
        obj->giveUpgrade(upgrade);
        CHECK(ai->curLocomotorSet() == LOCOMOTORSET_NORMAL_UPGRADED);
        obj->removeUpgrade(upgrade);
        CHECK(ai->curLocomotorSet() == LOCOMOTORSET_NORMAL);

        Object *killer = w.spawn("TestKillLocomotorUpgrade", 400, 300, 0, w.teamOf("Alice"));
        REQUIRE(killer);
        AIUpdateInterface *killAI = killer->getAIUpdateInterface();
        REQUIRE(killAI);
        killAI->setLocomotorUpgrade(true);
        CHECK(killAI->curLocomotorSet() == LOCOMOTORSET_NORMAL_UPGRADED);
        killer->giveUpgrade(upgrade);
        CHECK(killAI->curLocomotorSet() == LOCOMOTORSET_NORMAL);
        killer->removeUpgrade(upgrade);
        CHECK(killAI->curLocomotorSet() == LOCOMOTORSET_NORMAL);
    }
}
