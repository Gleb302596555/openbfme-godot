// OpenBFME unit tests, LOCAL-165 (fork issue 932):
// Expectations come from RotWK 2.01 disassembly:
//   - RW 0x6C77B2 (SkirmishAI::UnRegister(Team*)) -> RW 0x6C6C71 -> RW 0x90B9F9 -> RW 0x8F2727 (AITactic::NotifyTeamCancelled):
//     when a handed-over team is removed and m_teams (+0x14) becomes empty, if m_field34 (+0x34) != 0 or !m_started (+0x10 == 0)
//     (0x8F275F..0x8F277A), the tactic ends with end(false, false) (RW 0x8F2006).

#include "doctest.h"

#include "Common/NameKeyGenerator.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Thing/RwGrammar.h"
#include "Common/Team.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/SkirmishAI/AITacticalAI.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"

TEST_CASE("LOCAL-165 AITactic: NotifyTeamCancelled (RW 0x8F2727) ends unstarted tactic when last handed-over team dies")
{
	// Expected from RW 0x8F275F..0x8F277A: !m_started (+0x10 == 0) and m_teams (+0x14) becomes empty -> end(false, false).
	NameKeyGenerator keys;
	RwGrammar grammar(RwBinaryData::embedded());
	ModuleFactory modules(keys, grammar);
	ThingFactory things(keys, modules, grammar);
	PlayerTemplateStore templates(keys);
	TeamFactory teams;
	PlayerList players(keys, templates, teams);
	GameLogic logic(things, modules, players, RandomAlgorithm::RotWK_GameDat_LCG);

	AISkirmishPlayer ai;
	ai.playerIndex = 1;
	AIBrainState &brain = ai.brain;
	AITarget target;
	target.type = 0; // ENEMY_STRUCTURE
	target.teams = 1;
	brain.targets.push_back(target);

	AITactic tac;
	tac.kind = "SimpleAttack";
	tac.kindIndex = 0;
	tac.id = 1;
	tac.targetIndex = 0;
	tac.targetOwner = 1;
	tac.started = false;
	tac.ended = false;

	AITacticTeam team;
	team.index = 0;
	team.handedOver = true;
	team.wantsMembers = false;
	team.members.push_back((ObjectID)9999); // destroyed / missing object -> !teamAlive
	tac.teams.push_back(team);

	AITacticalAI::updateTacticForTest(logic, ai, brain, tac, brain);

	CHECK(tac.teams.empty());
	CHECK(tac.ended);
	CHECK_FALSE(tac.success);
	CHECK(brain.targets[0].teams == 0);
}

TEST_CASE("LOCAL-165 AITactic: unstarted tactic keeps waiting when a pending team remains or was not handed over (RW 0x8F28DD / RW 0x8F2727)")
{
	// Expected from RW 0x8F28DD + RW 0x8F2727: pending prototypes (+0x04) are not pruned, and when a pending team remains,
	// m_teams cancellation does not prematurely drop a still-recruiting tactic.
	NameKeyGenerator keys;
	RwGrammar grammar(RwBinaryData::embedded());
	ModuleFactory modules(keys, grammar);
	ThingFactory things(keys, modules, grammar);
	PlayerTemplateStore templates(keys);
	TeamFactory teams;
	PlayerList players(keys, templates, teams);
	GameLogic logic(things, modules, players, RandomAlgorithm::RotWK_GameDat_LCG);

	AISkirmishPlayer ai;
	ai.playerIndex = 1;
	AIBrainState &brain = ai.brain;
	AITarget target;
	target.type = 0;
	target.teams = 1;
	brain.targets.push_back(target);

	// Case 1: single team still pending recruitment (!handedOver) with no members yet.
	AITactic pendingOnly;
	pendingOnly.kind = "SimpleAttack";
	pendingOnly.kindIndex = 0;
	pendingOnly.id = 2;
	pendingOnly.targetIndex = 0;
	pendingOnly.targetOwner = 1;
	pendingOnly.started = false;
	AITacticTeam pendingTeam;
	pendingTeam.index = 0;
	pendingTeam.handedOver = false;
	pendingTeam.wantsMembers = true;
	pendingOnly.teams.push_back(pendingTeam);

	AITacticalAI::updateTacticForTest(logic, ai, brain, pendingOnly, brain);
	CHECK_FALSE(pendingOnly.ended);
	CHECK(pendingOnly.teams.size() == 1);
	CHECK(brain.targets[0].teams == 1);

	// Case 2: two-team tactic where team 0 (handedOver) dies while team 1 is still pending (!handedOver).
	AITactic twoTeams;
	twoTeams.kind = "FlankAttack";
	twoTeams.kindIndex = 2;
	twoTeams.id = 3;
	twoTeams.targetIndex = 0;
	twoTeams.targetOwner = 1;
	twoTeams.started = false;
	AITacticTeam deadHanded;
	deadHanded.index = 0;
	deadHanded.handedOver = true;
	deadHanded.wantsMembers = false;
	deadHanded.members.push_back((ObjectID)9999);
	AITacticTeam stillPending;
	stillPending.index = 1;
	stillPending.handedOver = false;
	stillPending.wantsMembers = true;
	twoTeams.teams.push_back(deadHanded);
	twoTeams.teams.push_back(stillPending);

	AITacticalAI::updateTacticForTest(logic, ai, brain, twoTeams, brain);
	CHECK_FALSE(twoTeams.ended);
	CHECK(twoTeams.teams.size() == 1);
	CHECK(twoTeams.teams[0].index == 1);
}
