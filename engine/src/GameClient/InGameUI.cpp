// OpenBFME. GPL-3.0.
// See InGameUI.h.

#include "GameClient/InGameUI.h"

#include "GameClient/ControlBarCommands.h"

#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/HudContext.h"
#include "GameClient/HudObjects.h"
#include "GameLogic/Object/Object.h"
#include "Common/BuildAssistant.h"
#include "Common/ModEngineFeatures.h"
#include "Common/Thing/KindOfTokens.h"
#include "GameLogic/GameLogic.h"

#include <algorithm>
#include <set>

// RW 0x69D90C selects across the screen from the current selection, not the object that passed the ControlBar gate.
// The region scan retains the ZH-based port (S-1202): contained-template matching is not yet reproduced.
// PORT-271 (issue #638, P-151): the traversal order is RotWK's. RW 0x6A43FE walks TheGameClient's drawables, which are newest-first, and adds
// each match through selectDrawable (RW 0x6A4109), a push_front, so the selection ends oldest-first (ascending ObjectID) and the hero the
// panel picks (the smallest id of the senior rank, RW 0x94475A) is the first of the selection, the one the voice answers for (RW 0x8DEDBB).
// The object list here runs oldest-first, so the walk takes it from the end.
namespace
{
// PYSAGE 1.9.8.5 .padsel 0xEDE128: returns the "<Prefix>ExpansionPad" stem when name ends with "ExpansionPadSide" or "ExpansionPadCorner".
bool expansionPadStem(const std::string &name, std::string &stem)
{
	const size_t n = name.size();
	size_t cut = 0;
	if (n >= 16 && name.compare(n - 4, 4, "Side") == 0)
	{
		cut = n - 4;
	}
	else if (n >= 18 && name.compare(n - 6, 6, "Corner") == 0)
	{
		cut = n - 6;
	}
	else
	{
		return false;
	}
	if (cut < 12 || name.compare(cut - 12, 12, "ExpansionPad") != 0)
	{
		return false;
	}
	stem.assign(name, 0, cut);
	return true;
}

bool templateHasKindOf(GameLogic &logic, const ThingTemplate &t, unsigned bit)
{
	return MaskTest(logic.templateInfo(t.getFinalOverride()).kindOf, bit);
}
} // namespace

bool InGameUI::isSameExpansionPad(GameLogic &logic, const ThingTemplate *a, const ThingTemplate *b)
{
	if (!a || !b)
	{
		return false;
	}
	// PYSAGE 1.9.8.5 .padsel 0xEDE0A9..0xEDE0DD: STRUCTURE (7), BASE_FOUNDATION (104), EXPANSION_PAD (216)
	static const int kStructure = ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE");
	static const int kBaseFoundation = ObjectTemplateInfoBuilder::kindOfIndex("BASE_FOUNDATION");
	static const int kExpansionPad = ObjectTemplateInfoBuilder::kindOfIndex("EXPANSION_PAD");
	if (kStructure < 0 || kBaseFoundation < 0 || kExpansionPad < 0)
	{
		return false;
	}
	for (const ThingTemplate *t : { a, b })
	{
		if (!templateHasKindOf(logic, *t, (unsigned)kStructure)
			|| !templateHasKindOf(logic, *t, (unsigned)kBaseFoundation)
			|| !templateHasKindOf(logic, *t, (unsigned)kExpansionPad))
		{
			return false;
		}
	}
	if (BuildAssistant::isEquivalentTo(a, b))
	{
		return true;
	}
	std::string stemA, stemB;
	return expansionPadStem(a->getName(), stemA) && expansionPadStem(b->getName(), stemB) && stemA == stemB;
}

int InGameUI::selectMatching(HudContext &ctx, bool acrossMap)
{
	std::set<const ThingTemplate *> templates;
	for (ObjectID id : selected())
	{
		if (Object *o = ctx.logic.findObjectByID(id))
		{
			if (HudObjects::isLocallyControlled(ctx, *o) && o->getTemplate())
			{
				templates.insert(o->getTemplate());
			}
		}
	}
	if (templates.empty())
	{
		return 0;
	}
	const bool selectAllBuildings = ModEngineFeatures::has(ModEngineFeature::EnnorathSelectAllBuildings);
	const ICoord2D size = ctx.view.size();
	std::vector<Object *> found;
	for (Object *o = ctx.logic.getLastObject(); o; o = o->getPrevObject())
	{
		if (o->isDestroyed() || o->getContainedBy() || !HudObjects::isLocallyControlled(ctx, *o) || !HudObjects::isSelectable(*o, getGUICommand()))
		{
			continue;
		}
		if (isSelected(o->getID()))
		{
			continue;
		}
		const bool isStructure = o->isKindOfName("STRUCTURE");
		if (isStructure)
		{
			if (!selectAllBuildings)
			{
				continue;
			}
			// PYSAGE 1.9.8.5 .bldgrp 0xEDA020 (hook RW 0x625696): skip if producer is an UNSELECTABLE HORDE.
			if (o->getProducerID() != INVALID_ID)
			{
				if (const Object *prod = ctx.logic.findObjectByID(o->getProducerID());
					prod && prod->isKindOfName("HORDE") && prod->testStatus(3 /* OBJECT_STATUS_UNSELECTABLE */))
				{
					continue;
				}
			}
			// PYSAGE 1.9.8.5 .padsel 0xEDE018 / 0xEDE040 (hooks RW 0x6A286A, RW 0x6A2955)
			bool matched = false;
			for (const ThingTemplate *t : templates)
			{
				if (t == o->getTemplate() || BuildAssistant::isEquivalentTo(o->getTemplate(), t) || isSameExpansionPad(ctx.logic, o->getTemplate(), t))
				{
					matched = true;
					break;
				}
			}
			if (!matched)
			{
				continue;
			}
		}
		else if (!templates.count(o->getTemplate()))
		{
			continue;
		}
		if (!acrossMap)
		{
			ICoord2D px;
			if (!ctx.view.worldToScreen(*o->getPosition(), px) || px.x < 0 || px.y < 0 || px.x > size.x || px.y > size.y)
			{
				continue;
			}
		}
		if (getMaxSelectCount() > 0 && getSelectCount() + (int)found.size() >= getMaxSelectCount())
		{
			break;
		}
		found.push_back(o);
	}
	if (found.empty())
	{
		return 0;
	}
	ClientMessage &m = ctx.stream.append(MSG_CREATE_SELECTED_GROUP_NO_SOUND);
	m.appendBoolean(false);
	for (Object *o : found)
	{
		selectObject(o->getID());
		m.appendObjectID(o->getID());
	}
	return (int)found.size();
}

bool InGameUI::isSelected(ObjectID id) const
{
	return std::find(m_selected.begin(), m_selected.end(), id) != m_selected.end();
}

void InGameUI::selectObject(ObjectID id)
{
	if (id != INVALID_ID && !isSelected(id))
	{
		markSelectionChanged();
		m_selected.insert(m_selected.begin(), id); // ZH m_selectedDrawables.push_front: the newest selection is the first one
		if (m_selectHandler)
		{
			m_selectHandler(id); // LOCAL-3: flashAsSelected RW 0x81DC79 / 0x81E0A7
		}
	}
}

void InGameUI::deselectObject(ObjectID id)
{
	if (isSelected(id))
	{
		markSelectionChanged();
	}
	m_selected.erase(std::remove(m_selected.begin(), m_selected.end(), id), m_selected.end());
}

void InGameUI::markSelectionChanged()
{
	if (m_logic)
	{
		m_frameSelectionChanged = m_logic->getFrame();
	}
}

void InGameUI::deselectAll(bool postMessage)
{
	if (!m_selected.empty())
	{
		markSelectionChanged(); // ZH / RotWK: deselectDrawable of each selected drawable
	}
	m_selected.clear();
	// ZH deselectAllDrawables (InGameUI.cpp:3260): with postMsg the logic's group is destroyed by a message even when nothing was selected
	// ("@todo don't we want to not emit this message if there wasn't a group at all"); its argument is TRUE (deletes the entire group)
	if (postMessage && m_stream)
	{
		m_stream->append(MSG_DESTROY_SELECTED_GROUP).appendBoolean(true);
	}
}

// lane OWNERFIX-1: RW 0x69B5ED (InGameUI.h)
void InGameUI::setInputEnabled(bool on)
{
	const bool was = m_inputEnabled;
	m_inputEnabled = on;
	if (!was || on)
	{
		return;
	}
	setSelecting(false); // RW 0x69B628: vslot 0xAC
	noteStep("setSelecting");
	if (m_radiusCursorNone)
	{
		m_radiusCursorNone(); // RW 0x69B632: vslot 0x144, before the mode flags
	}
	noteStep("setRadiusCursorNone");
	m_forceAttack = false; // + 0x8B8
	m_forceMove = false;   // + 0x8B9
	m_waypoint = false;    // + 0x8B0
	m_preferSelection = false; // + 0x8BA (RW 0x820106 / 0x820112: the prefer-selection mode, as lane INPUT-1 found; CommandXlat.cpp)
	noteStep("modeFlags");
	if (m_stream)
	{
		ClientMessage &m = m_stream->append(MSG_CHANGE_ORDERMODE); // 0x469 (0, 1): decomp InGameUIInputModes.cpp, last statement of the edge
		m.appendInteger(0);
		m.appendInteger(1);
	}
	noteStep("MSG_CHANGE_ORDERMODE");
}

// lane OWNERFIX-1: RW 0x7BC586 / RW 0x7BC5E9 (InGameUI.h)
void InGameUI::applyScriptInput(bool enable)
{
	setInputEnabled(enable); // RW 0x69B5DA
	m_scriptMouseHidden = !enable; // RW 0x5EDD0F
	noteStep("mouseVisibility");
	if (enable)
	{
		return;
	}
	deselectAll(true); // the stream's MSG_DESTROY_SELECTED_GROUP (true), then RW 0x69C817: ZH deselectAllDrawables(true)
	noteStep("MSG_DESTROY_SELECTED_GROUP");
	m_waypoint = false; // + 0x8B0 = 0
	noteStep("waypointMode");
}

std::string InGameUI::scriptInputStopLine()
{
	return "[S-4964] cutscene input (lane OWNERFIX-1): DISABLE_INPUT (RW 0x7BC586) turns the input off, hides the pointer, drops the selection and the pending "
		   "radius cursor, clears the force attack / force move / waypoint modes, the help provider (RW 0x8076CD: its flag +0x21C and provider are ported, "
		   "its call on the radar window override's inner object +0xC8 is not) and the LookAt flags (RW 0x83AB5D: +0x38 / +0x39 / +0x154 .. +0x157; +0x3B / "
		   "+0x3C are not mapped); +0x8BA is the prefer-selection mode (RW 0x820106), the other eight mode flags +0x8BB .. +0x8C2 are not mapped; a pending GUI command (the attack move button's) is not cleared, as in retail; the CommandTranslator's input-off pass list (RW 0x81F8F2) keeps "
		   "its logic messages, its client message numbers (0x1D, 0x70, 0x6A5, 0x6A6, 0x6B8, 0x6BA, 0x6BB, 0x7D9, 0x7ED) are not mapped; INFERENCE: the HUD's "
		   "own entry paths (the spell book bar and its target click, the hero bar, the radar's look-at) do nothing while the input is off (retail's windows "
		   "still run their callbacks, its translators drop the result); the pointer over a menu opened during a cutscene shows (not read)";
}

void InGameUI::mirrorSelection(const std::vector<ObjectID> &ids)
{
	if (m_selected != ids)
	{
		markSelectionChanged();
		m_selected = ids;
	}
}

void InGameUI::setGUICommand(const CommandButton *command)
{
	// RW 0x69B34A / BFME2 0x0029A889: InGameUI::setGUICommand (0x0029A889, InGameUIInputModes.cpp, matched): the command is kept.
	// RW 0x6A3B77 / BFME2 0x002A3273: GUI command mode with COMMAND_OPTION_NEED_TARGET (RW 0x69B34A) and no radius cursor immediately initializes
	// its mode cursor to the command's CursorName (e.g. AttackMove), avoiding a one-frame delay waiting for mouse move.
	m_guiCommand = command;
	if (command && (command->m_options & COMMAND_OPTION_NEED_TARGET) != 0 && command->m_radiusCursor.empty())
	{
		const std::string &cur = command->m_cursorName.empty() ? std::string(MouseCursorName::Target) : command->m_cursorName;
		setCursor(cur);
		m_guiModeCursor = cur;
	}
	else
	{
		setCursor(MouseCursorName::Arrow);
		m_guiModeCursor = MouseCursorName::Arrow;
	}
}

int InGameUI::mouseMode() const
{
	if (isPlacing())
	{
		return 1;
	}
	// RW 0x69B34A / BFME2 0x0029A889: MOUSEMODE_GUI_COMMAND (2) for a command other than DOZER_CONSTRUCT (0x35) with COMMAND_OPTION_NEED_TARGET (0x227)
	if (m_guiCommand && m_guiCommand->m_command != GUI_COMMAND_DOZER_CONSTRUCT && (m_guiCommand->m_options & COMMAND_OPTION_NEED_TARGET) != 0)
	{
		return 2;
	}
	return 0;
}

const std::string &InGameUI::mouseModeCursor() const
{
	static const std::string build = MouseCursorName::Build, arrow = MouseCursorName::Arrow;
	switch (mouseMode())
	{
		case 1:
			// RW 0x6A3E1E / BFME2 0x002A3273: mode 1 placement hint cursor is Build (10, hidden)
			return build;
		case 2:
			return m_guiModeCursor;
		default:
			return arrow;
	}
}

void InGameUI::setCursor(const std::string &name)
{
	// BFME2 InGameUI::setMouseCursor (0x0029A5D6, InGameUIInputModes.cpp, matched): TheMouse->setCursor, then in the GUI command mode (2) a cursor other than
	// Arrow / Scroll becomes the mode's cursor (+ 0x800)
	m_cursor = name;
	if (mouseMode() == 2 && name != MouseCursorName::Arrow && name != MouseCursorName::Scroll)
	{
		m_guiModeCursor = name;
	}
}

void InGameUI::setScrolling(bool on)
{
	// lane BUILD-5: RW 0x69B28F (InGameUIInputModes.cpp:330)
	if (m_scrolling == on)
	{
		return;
	}
	setCursor(on ? MouseCursorName::Scroll : MouseCursorName::Arrow);
	m_scrolling = on;
}

void InGameUI::placeBuildAvailable(const std::string &templateName, ObjectID sourceObject, const CommandButton *command)
{
	m_placeTemplate = templateName;
	m_placeSource = sourceObject;
	m_placeCommand = templateName.empty() ? nullptr : command;
	m_placeHasGhost = false;
	m_placeAngle = 0.0f; // the first placement update turns the ghost to the template's PlacementViewAngle (template + 0x4E0, RW 0x69C755; PlaceEventTranslator)
	m_placeSlotAngle = 0.0f;
	m_placeLegal = 0;
	m_placeAnchored = false; // RW 0x69C7C3 / the end path: setPlacementStart(null)
	// lane BUILD-5: RW 0x69C69B setMouseCursor(Target) with a template, RW 0x69C7B0 / 0x69C7EE setMouseCursor(Arrow) without one
	setCursor(templateName.empty() ? MouseCursorName::Arrow : MouseCursorName::Target);
}

// RW 0x69F54F (its selection and location tests need the logic: HudInput::hintSpy)
void InGameUI::createMoveHint(const Coord3D &pos)
{
	expireMoveHints();
	MoveHint &h = m_moveHints[m_nextMoveHint];
	h.frame = m_clientFrame;
	h.pos = pos;
	h.expired = false;
	++m_moveHintsMade;
	if (++m_nextMoveHint == MAX_MOVE_HINTS)
	{
		m_nextMoveHint = 0;
	}
}

// RW 0x69B76C(0) -> RW 0x69B745 for each slot
void InGameUI::expireMoveHints()
{
	for (MoveHint &h : m_moveHints)
	{
		h.frame = 0;
		h.expired = true;
	}
}

bool InGameUI::moveHintDrawn(int i) const
{
	if (i < 0 || i >= MAX_MOVE_HINTS)
	{
		return false;
	}
	const MoveHint &h = m_moveHints[i];
	// RW 0x48EDFF .. 0x48EE24: elapsed = frame - hint frame, taken as 41 while the client frame is below 41; drawn when not expired and elapsed < 41
	const unsigned elapsed = m_clientFrame < MOVE_HINT_FRAMES + 1 ? MOVE_HINT_FRAMES + 1 : m_clientFrame - h.frame;
	return !h.expired && elapsed <= MOVE_HINT_FRAMES;
}

int InGameUI::liveMoveHintCount() const
{
	int n = 0;
	for (int i = 0; i < MAX_MOVE_HINTS; ++i)
	{
		n += moveHintDrawn(i) ? 1 : 0;
	}
	return n;
}

std::vector<std::string> InGameUI::takeMessages()
{
	std::vector<std::string> out;
	out.swap(m_messages);
	return out;
}
