// OpenBFME. GPL-3.0.
// See InGameHelpBox.h (lane HUD-6). Client-only.

#include "GameClient/InGameHelpBox.h"

#include <cstdio>
#include <cstdlib>

void InGameHelpBox::loaded(const std::string &clip, CallFn call)
{
	// RW 0x6D4F28: level = the "_level<N>" of the clip (RW 0x8155D9), name = the clip without it (RW 0x815563); RW 0x92E689 clears the state (0, -1 width)
	int level = 0;
	std::string name = clip;
	if (clip.rfind("_level", 0) == 0)
	{
		const size_t dot = clip.find('.');
		level = std::atoi(clip.substr(6, dot == std::string::npos ? std::string::npos : dot - 6).c_str());
		name = dot == std::string::npos ? std::string() : clip.substr(dot + 1);
	}
	m_call = std::move(call);
	m_loaded = true;
	m_name = name;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "_level%d.", level); // RW 0x92E588 (string RW 0xC7C4A4)
	m_renderName = std::string(buf) + name + "_Content";  // RW 0x92E6DC (string RW 0xC7EFF0)
	m_state = 0;
	m_countdown = 0;
	m_width = -1;
	setHelp(nullptr);
}

void InGameHelpBox::unloaded()
{
	m_loaded = false;
	m_call = nullptr;
	m_state = 0;
	setHelp(nullptr);
}

bool InGameHelpBox::call(const std::string &fn, const std::vector<std::string> &args)
{
	std::string line = fn + "(";
	for (size_t i = 0; i < args.size(); ++i)
	{
		line += (i ? "," : "") + args[i];
	}
	line += ")";
	if (m_calls.size() >= 64)
	{
		m_calls.erase(m_calls.begin());
	}
	m_calls.push_back(line);
	return m_call && m_call(m_name, fn, args);
}

void InGameHelpBox::setHelp(std::shared_ptr<CommandButtonHelp> help)
{
	// RW 0x92E570 -> 0x92E362: a shown box hides (state 2) and the help goes, then the new one is kept
	if (m_state == 3 || m_state == 4)
	{
		call("Hide", {});
		m_state = 2;
	}
	m_displayedCommand.reset();
	m_help = std::move(help);
}

void InGameHelpBox::hideSoon()
{
	if (m_state == 3) // RW 0x92E229
	{
		m_countdown = 5;
		m_state = 4;
	}
}

void InGameHelpBox::update(const std::function<int(CommandButtonHelp &, int)> &measure, float stageScaleY)
{
	if (!m_loaded)
	{
		return;
	}
	if (!m_marked) // nobody offered this command window this frame: re-entry must pay the ordinary hover delay
	{
		m_displayedCommand.reset();
	}
	switch (m_state) // RW 0x92E462
	{
		case 0:
			call("SampleContentWidth", {});
			m_width = -1;
			m_state = 1;
			break;
		case 1:
			if (m_width > 0)
			{
				call("Hide", {});
				m_state = 2;
			}
			break;
		case 2:
			if (m_help)
			{
				const int height = measure(*m_help, m_width);
				char buf[64];
				std::snprintf(buf, sizeof(buf), "%f", (double)((float)height * stageScaleY)); // RW 0x92E3AA: the float as "%f" (RW 0x6228E8)
				call("Show", { buf });
				m_state = 3;
			}
			break;
		case 4:
			if (--m_countdown <= 0)
			{
				// RW 0x92E362: Hide, state 2, the help released
				setHelp(nullptr);
			}
			break;
		default:
			break;
	}
}

void InGameHelpBox::render(float x, float y, float w, float h, std::vector<HelpDrawOp> &out)
{
	if (m_state == 1)
	{
		const int sampled = (int)(w + 0.5f); // RW 0x92E2B9: cvttss2si(width + 0.5f), at least 1
		m_width = sampled < 1 ? 1 : sampled;
	}
	else if ((m_state == 3 || m_state == 4) && m_help)
	{
		m_help->render(x, y, w, h, out);
	}
}

void InGameHelpBox::beginFrame()
{
	// ControlBar::update RW 0x71FC08: no hover marked in the last frame -> the box hides soon (when shown), the provider goes
	if (!m_marked)
	{
		if (m_loaded && isShown())
		{
			hideSoon();
		}
		m_haveProvider = false;
	}
	m_marked = false;
}

// lane OWNERFIX-1 / PORT-189: RW 0x8076CD (thunk RW 0x8079BB; InGameHelpBox.h)
void InGameHelpBox::dropProvider()
{
	m_marked = false;
	m_haveProvider = false;
	m_provider = Provider();
	m_shownForProvider = false;
	if (m_loaded && isShown())
	{
		hideSoon(); // RW 0x6D473D -> the help box's vslot 1 (RW 0x92E229): a shown help counts down to its hide
	}
}

void InGameHelpBox::showHelp(const Provider &p, std::uint32_t nowMs, int tooltipDelayMs, const std::function<std::shared_ptr<CommandButtonHelp>()> &compose,
                             const ControlBarButton *commandWindow)
{
	if (m_endGameTimer >= 0) // RW 0x807848, early timer gate 0x807874
	{
		return;
	}
	if (!m_hoverStartSet) // RW 0x807886: the static's first use
	{
		m_hoverStartSet = true;
		m_hoverStart = nowMs;
	}
	if (m_displayedCommand && !(m_displayedCommand->owner == p))
	{
		m_displayedCommand.reset(); // an offer for a different window cannot refresh the outgoing content
	}
	// RW 0x807A81 selects both labels at 0x807D9C / 0x807EB3 using the current index; RW 0x92E2AC draws the retained help in 3 AND 4.
	// INFERENCE (PORT-272): in the split HUD/logic updates, refresh an already visible command's changed text this update. Compare the two
	// effective label keys used by compose, not the image or the provider identity (RW 0x8075F5 only compares the window and button).
	const bool refresh = commandWindow && m_displayedCommand && (m_state == 3 || m_state == 4) &&
		(commandWindow->textLabel != m_displayedCommand->textLabel || commandWindow->descriptLabel != m_displayedCommand->descriptLabel);
	if (!m_haveProvider || !(m_provider == p))
	{
		if (!m_loaded || isShown())
		{
			m_haveProvider = false; // RW 0xA0CB16: release the old provider before requesting the hide.
			m_provider = Provider();
			if (m_loaded)
			{
				hideSoon();
			}
			return;
		}
		m_provider = p; // RW 0x807848: clone, mark, restart the clock (0x807918..0x807960)
		m_haveProvider = true;
		m_marked = true;
		m_hoverStart = nowMs;
		m_shownForProvider = false;
		if (!refresh)
		{
			return;
		}
	}
	m_marked = true;
	if (!refresh && (m_shownForProvider || nowMs <= m_hoverStart + (std::uint32_t)tooltipDelayMs)) // RW 0x8078CA..0x8078ED: unsigned
	{
		return;
	}
	m_shownForProvider = true;
	m_displayedCommand.reset(); // even a suppressed request is consumed; do not retry it when the menu goes away
	if (m_disconnectMenuPresent) // RW 0x807982: after consumption, before composition
	{
		return;
	}
	std::shared_ptr<CommandButtonHelp> help = compose(); // RW 0x807676: the provider's vslot 3, then the Palantir's help box
	if (help && m_loaded)
	{
		setHelp(std::move(help)); // RW 0x92E570: replace; update remeasures for the movie's sampled width before the next draw
		if (commandWindow)
		{
			m_displayedCommand = DisplayedCommand{ p, commandWindow->textLabel, commandWindow->descriptLabel };
		}
		++m_helpsShown;
	}
}

std::vector<std::string> InGameHelpBox::acceptanceStops()
{
	return {
		"[S-2703] help box (HUD-6): ported the help box movie clip (RW 0x92E462 / 0x92E2AC / 0x92E570 / 0x92E229), the control bar's provider (RW 0x807848, "
		"ControlBar::update RW 0x71FC08); INFERENCE: the hovered Palantir command button is the Apt player's current button inside a content the control bar "
		"filled (the content updater's rollover route, RW 0xC8C3D4 vslot 2, was not traced to its caller), the radial bubbles' hover is the ring's hit test, "
		"the Apt player's y scale is the stage height over the window height; ported (lane ROTWK-C): showHelp's end-game timer guard (RW 0xDE3BAC + "
		"0x1A204), the provider release RW 0xA0CB16 and the late TheDisconnectMenu guard (RW 0xDEA328); not ported: showHelp's guards InGameUI vslot 0x1BC / "
		"0x17C (lane HOVER-1 ports the hero bar's and the spell book's providers, S-4612)"
	};
}
