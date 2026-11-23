// OpenBFME unit tests. GPL-3.0.
// LOCAL-129 (fork issue 813): Ennorath 1.9.8.5 chat mute on the PlayerTribute screen (.chatmut 0xEDB000) and in the network chat path.
// Expectations: PYSAGE 1.9.8.5 .chatmut 0x00EDB01C / 0x00EDB037 -> 0x00EDB058 (StatusPage RW 0x915E00, TributePage RW 0x9162F5),
// PYSAGE 1.9.8.5 .chatmut 0x00EDB22C / 0x00EDB2B0 (Network::processChat RW 0x8D54D1, hooks RW 0x8D55ED / 0x8D560B),
// PYSAGE 1.9.8.5 .chatmut 0x00EDB257 (reset hook RW 0x62D232), and Ennorath 1.9.8.5 apt/PlayerTribute.big (31000-byte PlayerTribute.apt).

#include "StartShellFx.h"

#include "Common/ModEngineFeatures.h"
#include "GameClient/GUI/PlayerStatusInfo.h"
#include "GameClient/GUI/TributeInfo.h"
#include "GameNetwork/Network.h"
#include "Libraries/Source/Apt/AptCharacterInst.h"

#include <cstdlib>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace starttest;

struct RetailGuard
{
	~RetailGuard() { ModEngineFeatures::reset(); }
};

// Mounts Ennorath's apt/PlayerTribute.big ahead of the retail RotWK archive set so PlayerTribute.apt is the 1.9.8.5 movie with ChatMute.
class EnnorathOverlaySource : public AptFileSource
{
public:
	EnnorathOverlaySource(ArchiveFileSystem &baseFs, const std::string &bigPath)
		: m_base(baseFs), m_overlayFs(std::make_unique<Win32BIGFileSystem>())
	{
		std::string err;
		const std::filesystem::path bp(bigPath);
		m_hasOverlay = m_overlayFs->loadBigFilesFromDirectory(bp.parent_path().string(), bp.filename().string(), true, &err);
		if (m_hasOverlay)
		{
			m_overlay = std::make_unique<AptArchiveFileSource>(*m_overlayFs);
		}
	}
	bool hasOverlay() const { return m_hasOverlay; }
	bool readFile(const std::string &virtualPath, std::vector<std::uint8_t> &out, std::string *error) override
	{
		if (m_overlay && m_overlay->fileExists(virtualPath))
		{
			return m_overlay->readFile(virtualPath, out, error);
		}
		return m_base.readFile(virtualPath, out, error);
	}
	bool fileExists(const std::string &virtualPath) override
	{
		return (m_overlay && m_overlay->fileExists(virtualPath)) || m_base.fileExists(virtualPath);
	}
	std::vector<std::string> listMovies() override { return m_base.listMovies(); }

private:
	AptArchiveFileSource m_base;
	std::unique_ptr<Win32BIGFileSystem> m_overlayFs;
	std::unique_ptr<AptArchiveFileSource> m_overlay;
	bool m_hasOverlay = false;
};

struct EnnorathShellFx
{
	EnnorathOverlaySource source;
	RecordingShellServices services;
	MapGameText text;
	WorldSkirmishSetupSource setup;
	RecordingNewGameSink sink;
	ShellEnvironment environment;
	AptScreenFactoryTable factories;
	GadgetSkinData skins;
	LoadScreenInfo loadScreen;
	std::unique_ptr<WindowManager> wm;
	std::unique_ptr<AptGadgetLayer> layer;
	std::unique_ptr<Shell> shell;

	EnnorathShellFx(Shared &s, const std::string &bigPath) : source(*s.mount->fs, bigPath)
	{
		REQUIRE_MESSAGE(source.hasOverlay(), "failed to open " << bigPath);
		std::string error;
		REQUIRE_MESSAGE(loadGadgetSkinData(*s.mount->fs, skins, &error), error);
		REQUIRE_MESSAGE(setup.load(*s.world, s.settings, *s.mount->fs, &error), error);
		lobbyLabels(text);
		environment.gameText = &text;
		environment.skirmish = &setup;
		environment.newGame = &sink;
		environment.loadScreen = &loadScreen;
		registerAptScreenFactories(factories);
		wm = std::make_unique<WindowManager>(source, services);
		layer = std::make_unique<AptGadgetLayer>(*wm, source, skins);
		layer->registerComponents();
		shell = std::make_unique<Shell>(*wm, factories, services, environment);
		wm->setShell(shell.get());
		wm->init();
	}
	~EnnorathShellFx()
	{
		shell.reset();
		layer.reset();
	}
	void tick(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			wm->update(33);
		}
	}
	AptCharacterInst *at(int level, const std::string &path) { return wm->apt().resolvePath(wm->apt().level(level), path); }
};

struct MuteTributeSource : public TributeSource
{
	std::vector<TributePlayerRow> players;
	std::uint32_t mutedMask = 0;
	Network *net = nullptr;

	bool localActive() const override { return true; }
	bool transferAllowed() const override { return true; }
	std::vector<TributePlayerRow> tributePlayers() override { return players; }
	void send(int, int, std::uint32_t) override {}
	bool chatMuted(int slot) const override
	{
		if (slot < 0 || slot > 7)
		{
			return false;
		}
		return net ? net->chatMuted(slot) : ((mutedMask >> slot & 1u) != 0);
	}
	void setChatMuted(int slot, bool muted) override
	{
		if (slot < 0 || slot > 7)
		{
			return;
		}
		if (net)
		{
			net->setChatMuted(slot, muted);
			return;
		}
		if (muted)
		{
			mutedMask |= 1u << slot;
		}
		else
		{
			mutedMask &= ~(1u << slot);
		}
	}
};

std::string ennorathPlayerTributeBig()
{
	const char *env = std::getenv("OPENBFME_ENNORATH_INSTALL");
	if (!env || !*env)
	{
		return {};
	}
	const std::filesystem::path p = std::filesystem::path(env) / "apt" / "PlayerTribute.big";
	return std::filesystem::exists(p) ? p.string() : std::string();
}

// Returns the frame label currently active on `<rowPath>.ChatMute.State` (e.g. "_pending", "_audible", "_muted", "_disabled").
std::string muteStateLabel(EnnorathShellFx &fx, int level, const std::string &rowPath)
{
	AptCharacterInst *stateInst = fx.at(level, rowPath + ".ChatMute.State");
	const AptSpriteInst *sp = stateInst ? stateInst->asSprite() : nullptr;
	if (!sp)
	{
		return "<missing>";
	}
	for (const char *lbl : { "_pending", "_audible", "_muted", "_disabled" })
	{
		const int f = sp->labelFrame(lbl);
		if (f >= 0 && sp->frame == f)
		{
			return lbl;
		}
	}
	return "frame:" + std::to_string(sp->frame);
}

PlayerStatusInfo makeThreePlayerStatus()
{
	PlayerStatusInfo st;
	st.inSkirmish = false;
	st.localSlot = 0;
	const char *names[3] = { "LocalPlayer", "AllyPlayer", "EnemyPlayer" };
	for (int i = 0; i < 3; ++i)
	{
		PlayerStatusRow r;
		r.slot = i;
		r.fields[0] = names[i];
		r.fields[1] = "Gondor";
		r.fields[2] = i < 2 ? "Team 1" : "Team 2";
		r.fields[3] = "Alive";
		st.rows.push_back(r);
	}
	return st;
}

std::vector<TributePlayerRow> makeTwoAllyTributeRows()
{
	TributePlayerRow l;
	l.playerIndex = 3;
	l.slot = 0;
	l.name = "LocalPlayer";
	l.cash = 1000;
	l.local = true;

	TributePlayerRow al;
	al.playerIndex = 4;
	al.slot = 1;
	al.name = "AllyPlayer";
	al.cash = 500;
	al.local = false;
	return { l, al };
}

class LocalChatHub
{
public:
	struct Endpoint : NetTransport
	{
		LocalChatHub *hub = nullptr;
		int slot = 0;
		std::deque<std::pair<int, std::vector<std::uint8_t>>> inbox;
		bool sendTo(int to, const std::vector<std::uint8_t> &bytes, std::string *) override
		{
			hub->m_flight.push_back({ slot, to, bytes });
			return true;
		}
		bool receive(int &from, std::vector<std::uint8_t> &bytes) override
		{
			if (inbox.empty())
			{
				return false;
			}
			from = inbox.front().first;
			bytes = inbox.front().second;
			inbox.pop_front();
			return true;
		}
	};
	explicit LocalChatHub(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			auto e = std::make_unique<Endpoint>();
			e->hub = this;
			e->slot = i;
			m_endpoints.push_back(std::move(e));
		}
	}
	Endpoint &endpoint(int slot) { return *m_endpoints[(size_t)slot]; }
	void tick()
	{
		std::vector<Flight> batch;
		batch.swap(m_flight);
		for (Flight &f : batch)
		{
			m_endpoints[(size_t)f.to]->inbox.push_back({ f.from, std::move(f.bytes) });
		}
	}

private:
	struct Flight
	{
		int from, to;
		std::vector<std::uint8_t> bytes;
	};
	std::vector<std::unique_ptr<Endpoint>> m_endpoints;
	std::vector<Flight> m_flight;
};
} // namespace

// Expectations: PYSAGE 1.9.8.5 .chatmut 0x00EDB01C / 0x00EDB037 -> 0x00EDB058 (hooks RW 0x915E00 / RW 0x9162F5) and Ennorath 1.9.8.5 PlayerTribute.apt.
TEST_CASE("LOCAL-129: ennorath.chat-mute: PlayerTribute StatusPage and TributePage mute state per player, profile on and off")
{
	OPENBFME_REQUIRE_START(s);
	const std::string bigPath = ennorathPlayerTributeBig();
	if (bigPath.empty())
	{
		MESSAGE("SKIP: OPENBFME_ENNORATH_INSTALL/apt/PlayerTribute.big not set");
		return;
	}

	for (const bool profileOn : { false, true })
	{
		CAPTURE(profileOn);
		RetailGuard guard;
		std::string error;
		if (profileOn)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &error), error);
		}

		EnnorathShellFx fx(*s, bigPath);
		MuteTributeSource game;
		game.players = makeTwoAllyTributeRows();
		PlayerStatusInfo status = makeThreePlayerStatus();
		fx.environment.tribute = &game;
		fx.environment.playerStatus = &status;

		fx.shell->push("PlayerTribute.apt");
		REQUIRE(fx.shell->top() != nullptr);
		const int level = fx.shell->top()->level();
		fx.tick(15);

		const std::string sRow0 = "OuterFrame.Pages.StatusPage.Table.0";
		const std::string sRow1 = "OuterFrame.Pages.StatusPage.Table.1";
		const std::string sRow2 = "OuterFrame.Pages.StatusPage.Table.2";
		REQUIRE(fx.at(level, sRow0 + ".ChatMute") != nullptr);
		REQUIRE(fx.at(level, sRow1 + ".ChatMute") != nullptr);
		REQUIRE(fx.at(level, sRow2 + ".ChatMute") != nullptr);

		if (!profileOn)
		{
			// Retail 2.01 (profile off): cmq= / cmt= are not intercepted by RW 0x915E00, so ChatMute stays on its initial "_pending" frame and never mutates mute state.
			CHECK(muteStateLabel(fx, level, sRow0) == "_pending");
			CHECK(muteStateLabel(fx, level, sRow1) == "_pending");
			CHECK(muteStateLabel(fx, level, sRow2) == "_pending");
			REQUIRE_MESSAGE(fx.wm->invokeASAt(level, sRow1 + ".ChatMute", "ToggleChat", {}, nullptr, &error), error);
			fx.tick(3);
			CHECK_FALSE(game.chatMuted(1));
			CHECK(muteStateLabel(fx, level, sRow1) == "_pending");
			continue;
		}

		// Profile on (.chatmut 0x00EDB058): local player (slot 0) -> "_disabled"; remote slots 1 & 2 -> "_audible".
		CHECK(muteStateLabel(fx, level, sRow0) == "_disabled");
		CHECK(muteStateLabel(fx, level, sRow1) == "_audible");
		CHECK(muteStateLabel(fx, level, sRow2) == "_audible");

		// Clicking local player's mute button does not mute slot 0 (.chatmut 0x00EDB167 .. 0x00EDB16F).
		REQUIRE_MESSAGE(fx.wm->invokeASAt(level, sRow0 + ".ChatMute", "ToggleChat", {}, nullptr, &error), error);
		fx.tick(2);
		CHECK_FALSE(game.chatMuted(0));
		CHECK(muteStateLabel(fx, level, sRow0) == "_disabled");

		// Clicking slot 1's mute button toggles slot 1 to "_muted" while slot 2 stays "_audible".
		REQUIRE_MESSAGE(fx.wm->invokeASAt(level, sRow1 + ".ChatMute", "ToggleChat", {}, nullptr, &error), error);
		fx.tick(2);
		CHECK(game.chatMuted(1));
		CHECK_FALSE(game.chatMuted(2));
		CHECK(muteStateLabel(fx, level, sRow1) == "_muted");
		CHECK(muteStateLabel(fx, level, sRow2) == "_audible");

		// Switch to the Tribute tab and check that TributePage reflects slot 1's muted state and toggles it back (.chatmut 0x00EDB037, RW 0x9162F5).
		AptButtonInst *tab = firstButtonIn(fx.at(level, "OuterFrame.Pages.tributeTab"));
		REQUIRE(tab != nullptr);
		float x0 = 0, y0 = 0, x1 = 0, y1 = 0, tx = 0, ty = 0;
		REQUIRE(tab->contentBounds(x0, y0, x1, y1));
		tab->globalMatrix().apply((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, tx, ty);
		fx.wm->postMouseMove(tx, ty);
		fx.tick(1);
		fx.wm->postMouseButton(true);
		fx.tick(1);
		fx.wm->postMouseButton(false);
		fx.tick(90);
		const std::string tRow0 = "OuterFrame.Pages.TributePage.enabledContent.Table.0";
		const std::string tRow1 = "OuterFrame.Pages.TributePage.enabledContent.Table.1";
		REQUIRE(fx.at(level, tRow0 + ".ChatMute") != nullptr);
		REQUIRE(fx.at(level, tRow1 + ".ChatMute") != nullptr);
		CHECK(muteStateLabel(fx, level, tRow0) == "_disabled");
		CHECK(muteStateLabel(fx, level, tRow1) == "_muted");

		// Toggle slot 1 off from TributePage -> "_audible" and ChatMuteRevision refreshes StatusPage on next enterFrame.
		REQUIRE_MESSAGE(fx.wm->invokeASAt(level, tRow1 + ".ChatMute", "ToggleChat", {}, nullptr, &error), error);
		fx.tick(2);
		CHECK_FALSE(game.chatMuted(1));
		CHECK(muteStateLabel(fx, level, tRow1) == "_audible");
	}
}

// Expectations: PYSAGE 1.9.8.5 .chatmut 0x00EDB22C / 0x00EDB2B0 (hooks RW 0x8D55ED / RW 0x8D560B in Network::processChat RW 0x8D54D1)
// and .chatmut 0x00EDB058 / 0x00EDB26C (local-only mute mask per slot).
TEST_CASE("LOCAL-129: ennorath.chat-mute: muted player's network chat lines are dropped while others' arrive, and unmute restores them")
{
	OPENBFME_REQUIRE_START(s);
	const std::string bigPath = ennorathPlayerTributeBig();
	if (bigPath.empty())
	{
		MESSAGE("SKIP: OPENBFME_ENNORATH_INSTALL/apt/PlayerTribute.big not set");
		return;
	}

	for (const bool profileOn : { false, true })
	{
		CAPTURE(profileOn);
		RetailGuard guard;
		std::string error;
		if (profileOn)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &error), error);
		}

		// 3-peer in-memory network: slot 0 (local), slot 1 (ally), slot 2 (enemy).
		LocalChatHub hub(3);
		std::vector<std::unique_ptr<Network>> nets;
		const char16_t *names[3] = { u"LocalPlayer", u"AllyPlayer", u"EnemyPlayer" };
		for (int i = 0; i < 3; ++i)
		{
			NetworkConfig cfg;
			cfg.localSlot = i;
			for (int k = 0; k < 3; ++k)
			{
				cfg.slotPlayerIndex[(size_t)k] = 10 + k;
				cfg.slotNames[(size_t)k] = names[k];
			}
			cfg.runAhead = 2;
			cfg.crcInterval = 0;
			cfg.disconnectPath = false;
			nets.push_back(std::make_unique<Network>(cfg, hub.endpoint(i)));
		}
		auto pump = [&]() {
			hub.tick();
			CommandList empty;
			for (auto &n : nets)
			{
				n->update(0, empty);
			}
		};

		EnnorathShellFx fx(*s, bigPath);
		MuteTributeSource game;
		game.players = makeTwoAllyTributeRows();
		game.net = nets[0].get();
		PlayerStatusInfo status = makeThreePlayerStatus();
		fx.environment.tribute = &game;
		fx.environment.playerStatus = &status;

		fx.shell->push("PlayerTribute.apt");
		REQUIRE(fx.shell->top() != nullptr);
		const int level = fx.shell->top()->level();
		fx.tick(15);

		const std::string sRow1 = "OuterFrame.Pages.StatusPage.Table.1";
		REQUIRE(fx.at(level, sRow1 + ".ChatMute") != nullptr);

		// Click mute on slot 1 in PlayerTribute.
		REQUIRE_MESSAGE(fx.wm->invokeASAt(level, sRow1 + ".ChatMute", "ToggleChat", {}, nullptr, &error), error);
		fx.tick(2);

		// Slot 1 and slot 2 both send chat to everyone (mask 0x7).
		REQUIRE_MESSAGE(nets[1]->sendChat(u"from slot 1 muted", 0x7u, &error), error);
		REQUIRE_MESSAGE(nets[2]->sendChat(u"from slot 2 audible", 0x7u, &error), error);
		pump();

		const std::vector<Network::ChatLine> got0 = nets[0]->takeChat();
		if (!profileOn)
		{
			// Profile off: ToggleChat did nothing, both lines arrive.
			REQUIRE(got0.size() == 2);
			CHECK(got0[0].slot == 1);
			CHECK(got0[1].slot == 2);
			CHECK(nets[0]->chatDroppedMuted() == 0);
			continue;
		}

		// Profile on: slot 1's line is dropped on slot 0, slot 2's line arrives; mute is local only (slot 1 still receives slot 2 and its own echo).
		REQUIRE(got0.size() == 1);
		CHECK(got0[0].slot == 2);
		CHECK(got0[0].text == u"from slot 2 audible");
		CHECK(nets[0]->chatDroppedMuted() == 1);
		CHECK(nets[1]->chatMutedMask() == 0u);
		CHECK(nets[1]->takeChat().size() == 2);

		// Unmute slot 1 from PlayerTribute -> slot 1's next line arrives again.
		REQUIRE_MESSAGE(fx.wm->invokeASAt(level, sRow1 + ".ChatMute", "ToggleChat", {}, nullptr, &error), error);
		fx.tick(2);
		CHECK_FALSE(nets[0]->chatMuted(1));
		REQUIRE_MESSAGE(nets[1]->sendChat(u"from slot 1 unmuted", 0x7u, &error), error);
		pump();
		const std::vector<Network::ChatLine> after = nets[0]->takeChat();
		REQUIRE(after.size() == 1);
		CHECK(after[0].slot == 1);
		CHECK(after[0].text == u"from slot 1 unmuted");
	}
}
