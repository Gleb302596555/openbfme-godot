// OpenBFME. GPL-3.0.
// See Common/ModEngineFeatures.h (lane MODS-3).

#include "Common/ModEngineFeatures.h"

#include "Common/AsciiString.h"

#include <algorithm>
#include <array>
#include <atomic>

namespace
{

using F = ModEngineFeature;

// set once at mount, before any data is parsed or a game starts; read by parsers and modules
std::array<std::atomic<bool>, (size_t)F::Count> g_on{};
std::vector<std::string> g_profiles;

} // namespace

const char *ModEngineFeatures::name(ModEngineFeature f)
{
	switch (f)
	{
	case F::EdainUnchainedHerobarKindOf: return "edain-unchained.herobar-kindof";
	case F::EdainUnchainedQueueIgnoreCP: return "edain-unchained.queue-ignore-cp";
	case F::EdainUnchainedCommandSetLimit: return "edain-unchained.commandset-limit";
	case F::EdainUnchainedCommandSetButtons: return "edain-unchained.commandset-button-upgrade";
	case F::EdainUnchainedObjectImageUpgrade: return "edain-unchained.object-image-upgrade";
	case F::EdainUnchainedPurchaseScienceSet: return "edain-unchained.spell-store-upgrade";
	case F::EdainUnchainedFireAtAttacker: return "edain-unchained.fire-at-attacker";
	case F::EdainUnchainedLargeGroupBonus: return "edain-unchained.large-group-bonus";
	case F::EdainUnchainedSciencePrereqs: return "edain-unchained.science-prereqs";
	case F::EdainUnchainedSpawnUnion: return "edain-unchained.spawn-union";
	case F::EdainUnchainedShareExperienceAll: return "edain-unchained.share-experience-all";
	case F::EdainUnchainedTriggerRechargeList: return "edain-unchained.trigger-recharge-list";
	case F::EdainUnchainedUniqueProductionId: return "edain-unchained.unique-production-id";
	case F::EdainUnchainedHeroRecruitParallel: return "edain-unchained.hero-recruit-parallel";
	case F::EdainUnchainedHordeExitAbsorption: return "edain-unchained.horde-exit-absorption";
	case F::EdainUnchainedMultiExecuteGate: return "edain-unchained.multi-execute-gate";
	case F::EdainUnchainedFoundationRebind: return "edain-unchained.foundation-rebind";
	case F::EdainUnchainedHerobar: return "edain-unchained.herobar";
	case F::EdainUnchainedDescriptionTimers: return "edain-unchained.description-timers";
	case F::EdainUnchainedUpgradeDescription: return "edain-unchained.upgrade-description";
	case F::EdainUnchainedObserverCommandRange: return "edain-unchained.observer-command-range";
	case F::EdainUnchainedProductionSplit: return "edain-unchained.production-split";
	case F::EnnorathStanceKeepsMoving: return "ennorath.stance-keeps-moving";
	case F::EnnorathMiddleClickCamera: return "ennorath.middle-click-camera";
	case F::EnnorathEvacuateContainers: return "ennorath.evacuate-containers";
	case F::EnnorathOptionsPages: return "ennorath.options-pages";
	case F::EnnorathOptionHotkeys: return "ennorath.option-hotkeys";
	case F::EnnorathOnlineDelayFix: return "ennorath.online-delay-fix";
	case F::EnnorathLobbyMapSearch: return "ennorath.lobby-map-search";
	case F::EnnorathChatMute: return "ennorath.chat-mute";
	case F::EnnorathObserverCommandRange: return "ennorath.observer-command-range";
	case F::EnnorathCommandSet64: return "ennorath.commandset-64";
	case F::EnnorathUpgrades5120: return "ennorath.upgrades-5120";
	case F::EnnorathBuildingLimitFix: return "ennorath.building-limit-fix";
	case F::EnnorathSpellStoreDelay: return "ennorath.spell-store-delay";
	case F::EnnorathSelectAllBuildings: return "ennorath.select-all-buildings";
	case F::EnnorathReplayControls: return "ennorath.replay-tabs-chat-pause";
	case F::EnnorathWotr8Players: return "ennorath.wotr-8-players";
	case F::EnnorathChatClipboard: return "ennorath.chat-clipboard";
	case F::Count: break;
	}
	return "?";
}

const std::vector<ModEngineFeatures::Profile> &ModEngineFeatures::knownBinaries()
{
	// the MD5s of the modified binaries of the All In One launcher's RotWK packages (logs/mods-engine-edits-catalogue.md, lane MODS-3)
	static const std::vector<Profile> profiles = {
		{ "5c31f5036cacb7690a6161b104a3efdb", "Edain Unchained 4.4.3 game.dat",
			{ F::EdainUnchainedHerobarKindOf, F::EdainUnchainedQueueIgnoreCP, F::EdainUnchainedCommandSetLimit, F::EdainUnchainedCommandSetButtons,
				F::EdainUnchainedObjectImageUpgrade, F::EdainUnchainedPurchaseScienceSet, F::EdainUnchainedFireAtAttacker,
				F::EdainUnchainedLargeGroupBonus, F::EdainUnchainedSciencePrereqs, F::EdainUnchainedSpawnUnion, F::EdainUnchainedShareExperienceAll,
				F::EdainUnchainedTriggerRechargeList, F::EdainUnchainedUniqueProductionId, F::EdainUnchainedHeroRecruitParallel,
				F::EdainUnchainedHordeExitAbsorption, F::EdainUnchainedMultiExecuteGate,
				F::EdainUnchainedFoundationRebind, F::EdainUnchainedHerobar,
				F::EdainUnchainedDescriptionTimers, F::EdainUnchainedUpgradeDescription, F::EdainUnchainedObserverCommandRange,
				F::EdainUnchainedProductionSplit },
			"not yet re-implemented: the production rate of PRODUCTION_UNIT / _UPGRADE (the port has no PRODUCTION rate yet, S-203; no Edain data names "
			"them), desert weather (SAND, DESERT: no Edain data or map names them), the per-frame CRC, the hero bar's 32 slots (inert: Edain keeps the "
			"retail 16-slot movie) (logs/mods-engine-edits-catalogue.md)" },
		{ "4b7718e9548e6ceac7252978346a0266", "Edain Volker Sub Mod 1.7 game.dat", {},
			"not yet re-implemented: 2.02 frame pacing, desync reported in chat instead of ending the game, per-frame CRC, 31 factions in the "
			"LAN game info (OpenBFME's lobby already carries them)" },
		{ "7a4068f87ea5c3b98e03ae4e83e2b806", "Patch 2.02 lotrbfme2ep1.exe (a whole game binary)", {},
			"install / copy-protection checks (none in OpenBFME); EA's frame pacing restored (presentation timing, not lockstep state)" },
		{ "a6ce9397ee16376e7c84a72218ffe03a", "2.02 Delayfix Addon lotrbfme2ep1.exe", {},
			"install checks (none in OpenBFME); the 2.01 frame pacing (OpenBFME's)" },
		// Ennorath 1.9.8.5's pySAGE-patched game.dat: the MD5 of the mod's RotWK\game.dat and the All In One service's Ennorath 1.9.8.5 package
		// ("engine_edits": "Ennorath: game.dat md5 fc32e4c4..."). Its sections are named by patch; docs/ennorath/ENNORATH_EXE.md maps each to its hooks
		// in the retail code. The features below rebuild the edits the owner took; the line below names what is left out.
		{ "fc32e4c4142b5c607b90972eaf448400", "Ennorath 1.9.8.5 game.dat",
			{ F::EnnorathStanceKeepsMoving, F::EnnorathMiddleClickCamera, F::EnnorathEvacuateContainers,
			  F::EnnorathOptionsPages, F::EnnorathOptionHotkeys, F::EnnorathOnlineDelayFix, F::EnnorathLobbyMapSearch, F::EnnorathChatMute,
			  F::EnnorathObserverCommandRange, F::EnnorathCommandSet64, F::EnnorathUpgrades5120, F::EnnorathBuildingLimitFix,
			  F::EnnorathSpellStoreDelay, F::EnnorathSelectAllBuildings,
			  F::EnnorathReplayControls, F::EnnorathWotr8Players, F::EnnorathChatClipboard },
			"rebuilt: the defensive stance that keeps a moving unit going (.stancem), the middle click camera reset by time (.camfix), war wagons and "
			"the Witch-king leaving a tunnel by evacuate all (.keydef 0xF39580), the paged options menu (.optui / .optfix), the option hotkeys and mod "
			"defaults (.hotkeys / .keydef / .keycmd), the online lag fix option (.dlyfix), the map search in the lobby (.mapfind / .mapsvar), per-player chat mute on the tribute screen (.chatmut 0xEDB000), "
			"the observer's command range (.obscmd), 64 CommandSet slots (.cmdext), 5120 upgrades (.upg5k), the building limit fix (.bldcap), spell store open delay (.spstore), "
			"select all buildings (.bldgrp / .padsel / .bldext / .bldcan / .bldlvl), "
			"replays (tabs, chat, Ctrl+] pause: .rpchat / .rppause / .rppvar), War of the Ring for 8 players (.wotrui / .wotrobs / .wtrview / .wtrdisp / .lobfac), the text entry clipboard Ctrl+A / C / X / V (.txtclip); not yet "
			"(docs/ennorath/ENNORATH_EXE.md): the DirectX resolution sort (.ressort) has no display mode list in OpenBFME (S-1913 / S-3401); "
			"the 30 / 60 FPS choice (.fxrate / .fpsvar / .fpsdraw; ETFrameRate shown in Options) is left out by decision: OpenBFME keeps its own frame rate" },
		// Ennorath 1.9.8.7 (2026-11-22; the patch note's 1.9.8.6 and 1.9.8.7 are one game.dat, md5 f222b925...): the 1.9.8.5 sections stay (some edited:
		// .camfix, .hotkeys, .keycmd, .keydef, .optui, .rpchat, .chatmut), plus .wmfeat / .wmkeys. Until their features are rebuilt it turns on what
		// 1.9.8.5 has, so the mod's package keeps its stance, camera, evacuate, options, map search and chat mute with the new game.dat.
		{ "f222b925edf2a9553e574ad8445f2841", "Ennorath 1.9.8.7 game.dat",
			{ F::EnnorathStanceKeepsMoving, F::EnnorathMiddleClickCamera, F::EnnorathEvacuateContainers,
			  F::EnnorathOptionsPages, F::EnnorathOptionHotkeys, F::EnnorathOnlineDelayFix, F::EnnorathLobbyMapSearch, F::EnnorathChatMute,
			  F::EnnorathObserverCommandRange, F::EnnorathCommandSet64, F::EnnorathUpgrades5120, F::EnnorathBuildingLimitFix,
			  F::EnnorathSpellStoreDelay, F::EnnorathSelectAllBuildings,
			  F::EnnorathReplayControls, F::EnnorathWotr8Players, F::EnnorathChatClipboard },
			"rebuilt: as Ennorath 1.9.8.5 (stance .stancem, middle click camera .camfix, evacuate all .keydef, paged options .optui / .optfix, option "
			"hotkeys .hotkeys / .keydef / .keycmd, online lag fix .dlyfix, lobby map search .mapfind, chat mute .chatmut, replays .rpchat / .rppause, War of the Ring 8p .wotrui / .wotrobs / .wtrview / .wtrdisp, the text entry clipboard .txtclip); not yet "
			"(docs/ennorath/ENNORATH_EXE.md): the 1.9.8.6 / 1.9.8.7 edits (.wmfeat / .wmkeys: the builder camera fix, select all units / all heroes / "
			"planning mode hotkeys, workers out of the builder hotkey, more than two builders, the builder click sound, 127 factions, the resurrection "
			"filter, portrait / icon change, CANT_EARN_POWERPOINTS, ToggleMounted at speed 0), and the 1.9.8.5 edits still open there" },
		{ "f7ce90438eb50da4ce63804ba2688b6b", "RJ-RotWK launcher (a Qt program that starts the game, not a game binary)", {}, "" },
		{ "243edd56bb09405626bb455a67b5aa46", "RJ-RotWK file.dat (the launcher's settings)", {}, "" },
	};
	return profiles;
}

bool ModEngineFeatures::activate(const std::vector<std::string> &md5s, std::string *error)
{
	std::vector<const Profile *> chosen;
	std::string unknown;
	for (const std::string &m : md5s)
	{
		const std::string low = AsciiStringUtil::lowered(m);
		const Profile *found = nullptr;
		for (const Profile &p : knownBinaries())
		{
			if (low == p.md5)
			{
				found = &p;
			}
		}
		if (!found)
		{
			unknown += (unknown.empty() ? "" : ", ") + m;
			continue;
		}
		chosen.push_back(found);
	}
	if (!unknown.empty())
	{
		reset(); // nothing of a half-known engine: retail
		if (error)
		{
			*error = "this mod changes the game program in a way OpenBFME does not know yet (unknown game binary " + unknown +
				"): its engine changes would be missing";
		}
		return false;
	}
	reset();
	for (const Profile *p : chosen)
	{
		g_profiles.push_back(p->what);
		for (ModEngineFeature f : p->features)
		{
			g_on[(size_t)f].store(true);
		}
	}
	return true;
}

void ModEngineFeatures::reset()
{
	for (auto &b : g_on)
	{
		b.store(false);
	}
	g_profiles.clear();
}

bool ModEngineFeatures::has(ModEngineFeature f)
{
	return g_on[(size_t)f].load(std::memory_order_relaxed);
}

std::vector<std::string> ModEngineFeatures::identityFeatures()
{
	std::vector<std::string> out;
	for (size_t i = 0; i < (size_t)F::Count; ++i)
	{
		if (g_on[i].load())
		{
			out.push_back(std::string("mod-engine:") + name((F)i));
		}
	}
	return out;
}

std::vector<std::string> ModEngineFeatures::stopLines()
{
	std::vector<std::string> out;
	for (const Profile &p : knownBinaries())
	{
		if (*p.notes && std::find(g_profiles.begin(), g_profiles.end(), std::string(p.what)) != g_profiles.end())
		{
			out.push_back(std::string("S-5304: ") + p.what + ": " + p.notes);
		}
	}
	return out;
}

std::vector<std::string> ModEngineFeatures::activeProfiles()
{
	return g_profiles;
}
