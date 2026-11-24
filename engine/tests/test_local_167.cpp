// OpenBFME. GPL-3.0.
// LOCAL-167 (fork issue 934): War of the Ring strategic UI — empty build plot rollover help
// (LivingWorldBuildPlotIcon::GetHelp RW 0x9A8025 -> StrategicInGameUI::CreateHelpFor RW 0x973751)
// and radial recruit button right-click dequeue (QueueUnitButton::OnRightClicked RW 0xA0C74F).

#include "doctest.h"
#include "AptPlayerTestUtil.h"
#include "AptRetail.h"
#include "GameClient/GUI/AptScreens/AptScreenFactories.h"
#include "GameClient/GUI/AptScreens/AptStrategicHUD.h"
#include "GameClient/GUI/GameTextSource.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"
#include "Libraries/Source/Apt/AptCharacterInst.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{

class MapText167 : public GameTextSource
{
public:
	void set(const std::string &label, const std::string &utf8) { m_values[label] = utf8; }
	bool fetch(const std::string &label, std::u16string &out) const override
	{
		auto it = m_values.find(label);
		if (it == m_values.end())
		{
			return false;
		}
		out.clear();
		for (unsigned char c : it->second)
		{
			out.push_back((char16_t)c);
		}
		return true;
	}

private:
	std::map<std::string, std::string> m_values;
};

struct StrategicHudFx167
{
	AptArchiveFileSource source;
	RecordingShellServices services;
	MapText167 text;
	ShellEnvironment env;
	AptScreenFactoryTable factories;
	std::unique_ptr<WindowManager> wm;
	std::unique_ptr<Shell> shell;
	std::unique_ptr<AptStrategicHUD> hud;

	explicit StrategicHudFx167(AptRetail &mount) : source(mount.fs)
	{
		env.gameText = &text;
		registerAptScreenFactories(factories);
		wm = std::make_unique<WindowManager>(source, services);
		shell = std::make_unique<Shell>(*wm, factories, services, env);
		wm->init();
		hud = std::make_unique<AptStrategicHUD>(*wm, *shell, env);
		tick(90);
	}

	~StrategicHudFx167()
	{
		hud.reset();
		shell.reset();
	}

	void tick(int frames)
	{
		for (int i = 0; i < frames; ++i)
		{
			wm->update(33);
		}
	}
};

AptButtonInst *findFirstButton167(AptCharacterInst *root)
{
	if (!root)
	{
		return nullptr;
	}
	if (AptButtonInst *b = root->asButton())
	{
		return b;
	}
	if (AptSpriteInst *sp = root->asSprite())
	{
		for (AptCharacterInst *k : sp->children())
		{
			if (AptButtonInst *b = findFirstButton167(k))
			{
				return b;
			}
		}
	}
	return nullptr;
}

void clickAt167(StrategicHudFx167 &fx, AptButtonInst &b)
{
	float x0, y0, x1, y1;
	REQUIRE(b.contentBounds(x0, y0, x1, y1));
	float x, y;
	b.globalMatrix().apply((x0 + x1) / 2, (y0 + y1) / 2, x, y);
	fx.wm->postMouseMove(x, y);
	fx.tick(1);
	fx.wm->postMouseButton(true);
	fx.tick(1);
	fx.wm->postMouseButton(false);
	fx.tick(1);
}

} // namespace

// Expectations from RW 0x9A8025 (LivingWorldBuildPlotIcon::GetHelp) -> RW 0x973751 (StrategicInGameUI::CreateHelpFor):
// fetches RW 0x9D8BA7 ("STRATEGICHUD:BuildPlotName") as title and "STRATEGICHUD:BuildPlotHelp" as body.
TEST_CASE("LOCAL-167 LivingWorldBuildPlotIcon: empty build plot rollover help (RW 0x9A8025 / RW 0x973751)")
{
	OPENBFME_REQUIRE_RETAIL(mount);
	StrategicHudFx167 fx(mount);
	REQUIRE(fx.hud->helpBox().isLoaded());

	fx.text.set("STRATEGICHUD:BuildPlotName", "Empty Build Plot");
	fx.text.set("STRATEGICHUD:BuildPlotHelp", "Click to construct a building here.");
	fx.text.set("OBJECT:Farm", "Farm");
	fx.text.set("OBJECT:FarmDesc", "Produces resources.");
	fx.text.set("CONTROLBAR:LW_FarmTreasuryBonus", " +%d");

	AptStrategicHUD::MapHelp plotHelp;
	plotHelp.kind = AptStrategicHUD::MapHelp::BUILD_PLOT;
	fx.hud->showMapHelp(plotHelp);
	REQUIRE(fx.hud->helpBox().help() != nullptr);
	CHECK(fx.hud->helpBox().help()->name() == u"Empty Build Plot");
	CHECK(fx.hud->helpBox().help()->longText() == u"Click to construct a building here.");
	CHECK(fx.hud->mapHelpShown());

	// Neighbor check: BUILDING map help still uses its own titleLabel / textLabel + farm treasury line (RW 0x9736A0)
	AptStrategicHUD::MapHelp bldHelp;
	bldHelp.kind = AptStrategicHUD::MapHelp::BUILDING;
	bldHelp.titleLabel = "OBJECT:Farm";
	bldHelp.textLabel = "OBJECT:FarmDesc";
	bldHelp.farmTreasury = 25;
	fx.hud->showMapHelp(bldHelp);
	REQUIRE(fx.hud->helpBox().help() != nullptr);
	CHECK(fx.hud->helpBox().help()->name() == u"Farm");
	CHECK(fx.hud->helpBox().help()->longText() == u"Produces resources. +25");
}

// Expectations from RW 0xA0C74F (QueueUnitButton::OnRightClicked, 0xC927B4[3]) and RW 0x9F3C24 (OnClicked):
// left click (pressButton == 0) emits "radial:<id>" only when enabled;
// right click (pressButton == 2) emits "radial_right:<id>" even when disabled; middle click (1) emits nothing.
TEST_CASE("LOCAL-167 StrategicInGameUI: radial button left vs right click (RW 0xA0C74F)")
{
	OPENBFME_REQUIRE_RETAIL(mount);
	StrategicHudFx167 fx(mount);

	std::vector<AptStrategicHUD::RadialButton> buttons(2);
	buttons[0].image = "BGFarm";
	buttons[0].enabled = true;
	buttons[1].image = "BGBarracks";
	buttons[1].enabled = false;
	fx.hud->showRadialMenu(512, 384, 1024, 768, 1.0f, 1.0f, buttons);
	fx.tick(30);
	REQUIRE(fx.hud->radialMenuShown());
	REQUIRE(fx.hud->radialButtonsReady() == 2);

	auto stageIt = fx.hud->loadedParts().find("RadialMenuStage");
	REQUIRE(stageIt != fx.hud->loadedParts().end());
	const std::string stageRel = stageIt->second.substr(stageIt->second.find('.') + 1);
	AptCharacterInst *c0 = fx.wm->apt().resolvePath(fx.wm->apt().level(fx.hud->level()), stageRel + ".0.bttnFrame.bttn");
	AptCharacterInst *c1 = fx.wm->apt().resolvePath(fx.wm->apt().level(fx.hud->level()), stageRel + ".1.bttnFrame.bttn");
	REQUIRE(c0 != nullptr);
	REQUIRE(c1 != nullptr);
	AptButtonInst *b0 = findFirstButton167(c0);
	AptButtonInst *b1 = findFirstButton167(c1);
	REQUIRE(b0 != nullptr);
	REQUIRE(b1 != nullptr);

	// Left click on enabled button 0 -> "radial:0"
	fx.hud->takeRequests();
	fx.wm->setPressButton(0);
	clickAt167(fx, *b0);
	fx.tick(5);
	auto req = fx.hud->takeRequests();
	CHECK(std::count(req.begin(), req.end(), "radial:0") == 1);
	CHECK(std::count(req.begin(), req.end(), "radial_right:0") == 0);

	// Left click on disabled button 1 -> nothing
	fx.wm->setPressButton(0);
	clickAt167(fx, *b1);
	fx.tick(5);
	req = fx.hud->takeRequests();
	CHECK(req.empty());

	// Right click on enabled button 0 -> "radial_right:0" (not "radial:0")
	fx.wm->setPressButton(2);
	clickAt167(fx, *b0);
	fx.tick(5);
	req = fx.hud->takeRequests();
	CHECK(std::count(req.begin(), req.end(), "radial_right:0") == 1);
	CHECK(std::count(req.begin(), req.end(), "radial:0") == 0);

	// Right click on disabled button 1 -> "radial_right:1" (RW 0xA0C74F dequeues regardless of enabled state)
	fx.wm->setPressButton(2);
	clickAt167(fx, *b1);
	fx.tick(5);
	req = fx.hud->takeRequests();
	CHECK(std::count(req.begin(), req.end(), "radial_right:1") == 1);

	// Neighbor check: middle click (button 1) emits nothing
	fx.wm->setPressButton(1);
	clickAt167(fx, *b0);
	fx.tick(5);
	req = fx.hud->takeRequests();
	CHECK(req.empty());
}
