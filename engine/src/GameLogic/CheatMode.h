// OpenBFME. GPL-3.0.
//
// CheatMode (lane CHEAT-1): OpenBFME's open "Cheat Mode" for custom games (the owner, 2026-10-23: "an online cheat mode ... exposed in the lobby menu under
// Open Play but not for ranked ... the host can balance the game or get money to test out heroes"). NOT RETAIL: RotWK 2.01 compiles every cheat out (no
// MSG_CHEAT_* or MSG_META_DEMO_* type in its name table; logicMessageDispatcher RW 0x779A3D has no cheat case we can read).
//
// An OPEN mode, agreed to by everyone in the game: the host ticks "Cheat Mode (OpenBFME)" on the Rules tab (SkirmishGameInfo::cheatMode, the host's slot);
// the Open Play / LAN game lists tag the game before anyone joins, the load screen, the in-game marker and the score screen say so, and every command below
// (run or refused) is announced to every player as a message line. Quick Match and tournament rooms never offer it (the lobby refuses it with a room code).
// A cheat-mode game is never counted: its game record is marked and kept out of the player's statistics and the platform's results.
//
// The command: MSG_OPENBFME_CHEAT (GameMessage.h), an ordinary frame command, so it travels, is recorded in the replay and runs on EVERY peer on the same
// frame. The one check (execute): the game is a cheat-mode game and the message's player (stamped by the network from the sending slot,
// Network::relayCommands / Network.cpp: a peer cannot name another slot's player) is the cheat seat's player. Anything else is refused, logged and
// announced as refused, identically on every peer (no desync); a refused or malformed command never reaches the dispatcher's unhandled count (any peer can
// send the type: it must not be able to stop a game).
//
// Arguments (all but the last integers; the full table with limits is docs/CHEAT_MODE.md, the API a stream-voting plugin would call):
//   0 command (Command), 1 target player index, 2 value, then per command:
//   Spawn:    3 template id (ThingFactory::findByTemplateID, the final override), 4 location (the first object's point)
//   LevelUp:  3 object id (an object of the target player)
//   the object commands (Freeze .. GiveTo): 3 the number of objects n (1 .. kMaxObjects), 4 .. 3 + n their ids (the host's selection, any player's objects)
// The effects are ZH's own cheat code (DONOR, GeneralsMD file:line) calling the ported RotWK functions (an earlier, unmerged prototype's reviewed effects, without its signatures):
//   AddResources     Money::deposit (ZH CommandXlat.cpp:3370 MSG_CHEAT_ADD_CASH; RW 0x7B18B8), no score, no sound; 1 .. 100000
//   AddCommandPoints CommandPoints::addBonus (RW 0x6A7C3E: the bonus the limit adds; no ZH cheat: OpenBFME's); 1 .. 1000
//   AddPowerPoints   PlayerScience::addSciencePurchasePoints (ZH CommandXlat.cpp:3403 MSG_CHEAT_GIVE_SCIENCEPURCHASEPOINTS; RW 0x782764); 1 .. 100
//   Spawn            newObject + ground height + setPosition + setOrientation + the pathfinder footprint + onBuildComplete (RW 0x629DD9 placeObjectAtPosition's
//                    order, GameLogic/NewGame/StartingBase.cpp, which places the starting structures too; ZH has no spawn cheat: OpenBFME's); 1 .. 10 objects of
//                    the target player's default team, in rows of 4 at 15 world units; units, heroes and structures (the owner's test sandbox, 2026-11-03),
//                    no projectile
//   LevelUp          ExperienceTracker::gainLevels(n, true) RW 0x79D7E5 on a trainable object of the target player (ZH CommandXlat.cpp:4212
//                    MSG_META_DEBUG_GIVE_VETERANCY); 1 .. 10
//   RevealMap        ShroudManager::revealMapForPlayerPermanently RW 0xB4F5D0 / undoRevealMapForPlayerPermanently RW 0xB519B0 (ZH LookAtXlat.cpp:531
//                    MSG_CHEAT_DESHROUD); value 1 reveal, 0 undo; idempotent per player (the shroud counts its viewers)
//   EndGame          every player that is not the target nor its ally is killed (VictoryConditions::killPlayer RW 0x6ABE7C); the victory conditions then end
//                    the game for the target's alliance as any game ends
//   VoidGame         the game is marked void and every player is killed: the game ends with no winner (the game record says void)
// The test sandbox (the owner, 2026-11-03: spawn anything for anyone, make units stand still or leave them to the AI, quick actions on the selection). Every
// one acts on the objects the message names (a horde: the horde and its members) and is OpenBFME's (no RotWK cheat exists to port):
//   Freeze           value 1: DISABLED_HELD until UPDATE_SLEEP_FOREVER (Object::setDisabled RW 0x6907F1 / 0x692432: what a garrison holds its riders with:
//                    the object does not move, it still fights where it stands) and the AI idles (CMD_FROM_PLAYER); value 0: clearDisabled (RW 0x692443)
//   HoldFire         value 1 / 0: the status NO_AUTO_ACQUIRE, as the hold-fire button sets it (WeaponCommands::setNoAutoAcquire, RW 0x77B6EF), an attacking
//                    object idles
//   Heal             Object::attemptHealing(max health, no source) (RW 0x690532)
//   Kill             Object::kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL) (RW 0x698E7D)
//   LevelUpObjects   ExperienceTracker::gainLevels(value, true) on each trainable object (RW 0x79D7E5); 1 .. 10
//   ResetCooldowns   every special power module's setReadyFrame(now) (slot 0x20, RW 0x99344A): every power is ready
//   GiveTo           Object::setTeam(the target player's default team) (RW 0x697C09; a horde's members follow, RW 0x69962F)
//   PauseAI          value 1 / 0: the target player's skirmish AI skips its update (SkirmishAIManager::updateAI); State's paused mask, hashed and saved
//   InstantBuild     value 1 / 0: the target player's production (units, heroes, upgrades: ProductionUpdate::totalProductionFrames) and construction (a
//                    dozer's build step, GettingBuiltBehavior's frames) take one frame. On for the cheat seat's player when the game starts (the owner,
//                    2026-11-04: "units, upgrades and buildings at once with Cheat Mode on"); State's instant mask, hashed and saved
// The announcements (take*): one per command, display data only, never hashed or saved (a replay announces the same lines again as it plays).
#pragma once

#include "Common/StateHash.h"
#include "GameLogic/ObjectTypes.h"

#include <cstdint>
#include <string>
#include <vector>

class GameLogic;
class GameLogicDispatch;
class GameMessage;
class Xfer;
struct Coord3D;

namespace CheatMode
{
enum class Command : int
{
	AddResources = 1,
	AddCommandPoints = 2,
	AddPowerPoints = 3,
	Spawn = 4,
	LevelUp = 5,
	RevealMap = 6,
	EndGame = 7,
	VoidGame = 8,
	Freeze = 9,
	HoldFire = 10,
	Heal = 11,
	Kill = 12,
	LevelUpObjects = 13,
	ResetCooldowns = 14,
	GiveTo = 15,
	PauseAI = 16,
	InstantBuild = 17
};
constexpr int kFirstCommand = 1;
constexpr int kLastCommand = 17;
// the commands whose arguments are a list of objects (argument 3 the count, then the ids)
constexpr bool takesObjects(Command c) { return c >= Command::Freeze && c <= Command::GiveTo; }

// the limits (docs/CHEAT_MODE.md)
constexpr int kMaxResources = 100000;
constexpr int kMaxCommandPoints = 1000;
constexpr int kMaxPowerPoints = 100;
constexpr int kMaxSpawn = 10;
constexpr int kMaxLevels = 10;
constexpr int kMaxObjects = 100;
constexpr float kStructureGap = 20.0f; // world units between two spawned structures
constexpr std::size_t kLogCap = 256;

struct LogEntry
{
	UnsignedInt frame = 0;
	int player = -1;  ///< the message's (network-stamped) player
	int command = 0;  ///< Command (0: undecodable)
	int target = -1;
	int value = 0;
	int extra = 0;    ///< the template id (Spawn), the object id (LevelUp) or the number of objects (the object commands)
	bool executed = false;
	std::string text; ///< the announced line (UTF-8): what it did, or why it was refused
};

class State
{
public:
	// the game's cheat seat: its game slot and the logic player index of that slot (LiveGame::load); slot -1 = not a cheat-mode game
	void configure(int slot, int playerIndex)
	{
		m_slot = slot;
		m_player = slot >= 0 ? playerIndex : -1;
		m_instantMask = m_player >= 0 && m_player < 32 ? (1u << m_player) : 0u; // InstantBuild: on for the cheat seat's player
	}
	bool enabled() const { return m_slot >= 0 && m_player >= 0; }
	int slot() const { return m_slot; }
	int seatPlayer() const { return m_player; }
	bool voided() const { return m_voided; }
	void setVoided() { m_voided = true; }
	bool revealed(int playerIndex) const { return playerIndex >= 0 && playerIndex < 32 && ((m_revealedMask >> playerIndex) & 1u); }
	void setRevealed(int playerIndex, bool on) { m_revealedMask = on ? (m_revealedMask | (1u << playerIndex)) : (m_revealedMask & ~(1u << playerIndex)); }
	// PauseAI: the players whose skirmish AI does not update
	bool aiPaused(int playerIndex) const { return playerIndex >= 0 && playerIndex < 32 && ((m_aiPausedMask >> playerIndex) & 1u); }
	void setAIPaused(int playerIndex, bool on) { m_aiPausedMask = on ? (m_aiPausedMask | (1u << playerIndex)) : (m_aiPausedMask & ~(1u << playerIndex)); }
	std::uint32_t aiPausedMask() const { return m_aiPausedMask; }
	// InstantBuild: the players whose production and construction take one frame
	bool instantBuild(int playerIndex) const { return enabled() && playerIndex >= 0 && playerIndex < 32 && ((m_instantMask >> playerIndex) & 1u); }
	void setInstantBuild(int playerIndex, bool on) { m_instantMask = on ? (m_instantMask | (1u << playerIndex)) : (m_instantMask & ~(1u << playerIndex)); }
	std::uint32_t instantMask() const { return m_instantMask; }
	// the sandbox block (GameLogic::xferCore version 5) is written while one of its masks is set
	bool sandboxInUse() const { return m_aiPausedMask != 0 || m_instantMask != 0; }
	void clearSandbox()
	{
		m_aiPausedMask = 0;
		m_instantMask = 0;
	}
	std::uint32_t executedCount() const { return m_executed; }
	std::uint32_t refusedCount() const { return m_refused; }
	const std::vector<LogEntry> &log() const { return m_log; }

	// one entry: counted, kept (the last kLogCap), and announced
	void record(LogEntry e);
	// the announcements since the last take (LiveGame publishes them with the frame's snapshot)
	std::vector<LogEntry> takeAnnouncements();

	// hashed only in a cheat-mode game (every other game's hash is unchanged)
	void crc(StateHasher &h) const;
	// GameLogic::xferCore (version 2): the seat, the reveals, void and the counts (the log and the announcements are display data)
	void xfer(Xfer &x);
	// GameLogic::xferCore (version 5): the paused AIs and the instant builders
	void xferSandbox(Xfer &x);

private:
	int m_slot = -1;
	int m_player = -1;
	std::uint32_t m_revealedMask = 0;
	std::uint32_t m_aiPausedMask = 0;
	std::uint32_t m_instantMask = 0;
	bool m_voided = false;
	std::uint32_t m_executed = 0, m_refused = 0;
	std::vector<LogEntry> m_log;
	std::vector<LogEntry> m_announce;
};

void registerHandlers(GameLogicDispatch &dispatch);
// a player a command may name: a playable faction's player (not the neutral player, an observer or a map side such as the civilians)
bool isGamePlayer(const GameLogic &logic, int index);
// the dispatcher's MSG_OPENBFME_CHEAT: check, then run; always true (see the header comment)
bool execute(GameLogic &logic, const GameMessage &msg);

// the client side: the message of a command (`player` the issuing player; the network stamps it again)
GameMessage makeMessage(int player, Command command, int target, int value, int extra = 0, const Coord3D *location = nullptr);
// an object command (takesObjects): the ids follow their count
GameMessage makeObjectsMessage(int player, Command command, int target, int value, const std::vector<ObjectID> &objects);
const char *commandName(int command);
// the command's id from its name ("add_resources" ...; docs/CHEAT_MODE.md); 0 when unknown
int commandFromName(const std::string &name);
} // namespace CheatMode
