// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
// Port of ZH GameClient/GUI/Gadget/GadgetTextEntry.cpp and GameEngineDevice W3DTextEntry.cpp.
//
// Text arrives as GWM_IME_CHAR (ZH's IMEManager translates the keyboard into it); GWM_CHAR carries the editing keys. The editing is RotWK's
// (P-128, read in the RW bytes against the BFME2 decomp): a cursor and a selection (Home / End / arrows, Shift to select, Ctrl+Left / Right
// words, Del, Backspace, click and drag), plus Ennorath 1.9.8.3's Ctrl+A / C / X / V. Not ported (stop S-177): the IME composition
// (GWM_IME_STRING, the candidate list).

#include "GameClient/GUI/Gadgets.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace
{
EntryData *entryOf(GameWindow *w) { return static_cast<EntryData *>(w->winGetUserData()); }

bool winIsDigit(char16_t c) { return c < 0x80 && std::isdigit((int)c); }
bool winIsAlNum(char16_t c) { return c < 0x80 && std::isalnum((int)c); }
bool winIsAscii(char16_t c) { return c < 0x80; }
} // namespace

std::uint32_t GadgetTextEntryValidationFlags(const EntryData &e)
{
	// the EntryData flags as the validator's bits (RW 0x75E4DF: 0x10 ASCII, 0x20 digits, 0x40 letters and digits). INFERENCE (stop S-1409): the
	// other bits (0x01 no space, 0x02 no '%', 0x04 printable ASCII without '\\', 0x08 no file-name characters, 0x80 space allowed) have no setter
	// ported here, so they are never set
	return (e.aSCIIOnly ? 0x10u : 0u) | (e.numericalOnly ? 0x20u : 0u) | (e.alphaNumericalOnly ? 0x40u : 0u);
}

bool GadgetTextEntryValidateCharacter(char16_t character, std::uint32_t flags)
{
	// RW 0x75E4DF (BFME2 decomp GadgetTextEntryValidateCharacter.cpp, tier A byte-matched): the Thai blocks U+0E01..U+0E3A and U+0E3F..U+0E5B are
	// refused whatever the flags, then the flag tests in retail order (the low byte of the flags only)
	const std::uint32_t c = character;
	const std::uint8_t f = (std::uint8_t)flags;
	if ((c >= 0xE01 && c <= 0xE3A) || (c >= 0xE3F && c <= 0xE5B))
	{
		return false;
	}
	if ((f & 0x80) && c == 0x20)
	{
		return true;
	}
	if ((f & 0x01) && c == 0x20)
	{
		return false;
	}
	if ((f & 0x02) && c == 0x25)
	{
		return false;
	}
	if ((f & 0x04) && (c < 0x22 || c > 0x7E || c == 0x5C))
	{
		return false;
	}
	if ((f & 0x08) && (c == 0x2A || c == 0x3F || c == 0x3A || c == 0x5C || c == 0x2F || c == 0x22 || c == 0x3C || c == 0x3E || c == 0x7C))
	{
		return false;
	}
	// iswascii / iswdigit / iswalnum of MSVCRT: INFERENCE (S-1409) for characters above 0x7F, which the port treats as neither digit nor letter
	if ((f & 0x10) && !winIsAscii(character))
	{
		return false;
	}
	if ((f & 0x20) && !winIsDigit(character))
	{
		return false;
	}
	if ((f & 0x40) && !winIsAlNum(character))
	{
		return false;
	}
	return true;
}

namespace
{
// DisplayString::getWidth(n) (RotWK DisplayString slot 0x40): the width of the first n characters in the window's font
int entryWidth(GameWindow *window, const UnicodeString &text, int count)
{
	const int n = std::max(0, std::min<int>(count, (int)text.size()));
	return window->manager().winTextWidth(window->winGetFont(), text.substr(0, (std::size_t)n));
}

TextEntryClipboard *g_clipboard = nullptr;

// DirectInput scan codes of the clipboard keys (ZH KeyDefs.h KEY_A / C / X / V; the patch's 0x1E / 0x2E / 0x2D / 0x2F)
constexpr int kKeyA = 0x1E, kKeyC = 0x2E, kKeyX = 0x2D, kKeyV = 0x2F;
} // namespace

void GadgetTextEntrySetClipboard(TextEntryClipboard *clipboard) { g_clipboard = clipboard; }
TextEntryClipboard *GadgetTextEntryGetClipboard() { return g_clipboard; }

void GadgetTextEntrySetCursorPosition(GameWindow *window, int position)
{
	// RW 0x7220D3 (BFME2 decomp GadgetTextEntryInsertCharacter.cpp, GadgetTextEntrySetCursorPosition 0x00320737; the RotWK bytes read the same way):
	// the cursor at +0x1C; a cursor before the first character in view backs the view up to five characters before it, else the view advances
	// while the text from the first character in view to five characters past the cursor is wider than the window less 8. The widths are of the
	// plain text, even in a secret entry (retail)
	EntryData *e = entryOf(window);
	if (!e)
	{
		return;
	}
	e->charPos = (short)position;
	if ((unsigned short)e->charPos < (unsigned)e->scrollPos)
	{
		e->scrollPos = (unsigned short)e->charPos < 5 ? 0 : (unsigned short)e->charPos - 5;
		return;
	}
	int width = 0, height = 0;
	window->winGetSize(&width, &height);
	if (width > 8)
	{
		width -= 8;
	}
	if (entryWidth(window, e->text, e->charPos) - entryWidth(window, e->text, e->scrollPos) > width)
	{
		const int end = std::min<int>((int)e->text.size(), e->charPos + 5);
		const int endX = entryWidth(window, e->text, end);
		while (endX - entryWidth(window, e->text, e->scrollPos) > width && e->scrollPos < (int)e->text.size())
		{
			++e->scrollPos;
		}
	}
}

bool GadgetTextEntryUpdateComposition(GameWindow *window)
{
	// RW 0x7224D8 (BFME2 decomp GadgetTextEntryUpdateComposition.cpp, 0x00320B3C): the span between the cursor and the other end of the selection,
	// clamped to the text, is removed; the cursor goes to its start (through RW 0x7220D3) and the other end follows; one '*' of the secret text goes
	// per removed character. The result is ORed into drawTextFromStart (+0x13)
	EntryData *e = entryOf(window);
	if (!e)
	{
		return false;
	}
	const int lower = std::min<int>((unsigned short)e->charPos, (unsigned short)e->selectPos);
	int upper = std::max<int>((unsigned short)e->charPos, (unsigned short)e->selectPos);
	upper = std::min<int>(upper, (int)e->text.size());
	bool changed = false;
	if (upper > 0 && upper != lower && lower < upper)
	{
		e->text.erase((std::size_t)lower, (std::size_t)(upper - lower));
		changed = true;
		GadgetTextEntrySetCursorPosition(window, lower);
		e->selectPos = (short)lower;
		for (int i = 0; i < upper - lower && !e->sText.empty(); ++i)
		{
			e->sText.pop_back();
		}
	}
	e->drawTextFromStart = e->drawTextFromStart || changed;
	return changed;
}

bool GadgetTextEntryInsertCharacter(GameWindow *window, char16_t character)
{
	// RW 0x72260B (BFME2 decomp GadgetTextEntryInsertCharacter.cpp, tier A byte-matched): validated; a selection is deleted first (RW 0x7224D8);
	// refused when the text already holds maxTextLen characters, else inserted at the cursor, the cursor one further (RW 0x7220D3) with the other
	// end of the selection on it, a '*' appended to the secret text and drawTextFromStart set
	EntryData *e = entryOf(window);
	if (!e || !GadgetTextEntryValidateCharacter(character, GadgetTextEntryValidationFlags(*e)))
	{
		return false;
	}
	if (e->selectPos != e->charPos)
	{
		GadgetTextEntryUpdateComposition(window);
	}
	if ((int)e->text.size() >= (int)e->maxTextLen)
	{
		return false;
	}
	const std::size_t at = std::min<std::size_t>((unsigned short)e->charPos, e->text.size());
	e->text.insert(e->text.begin() + (std::ptrdiff_t)at, character);
	GadgetTextEntrySetCursorPosition(window, (int)at + 1);
	e->selectPos = e->charPos;
	e->sText.push_back(u'*');
	e->drawTextFromStart = true;
	return true;
}

namespace
{
// the character under a screen x (RW 0x722886 .. 0x7228EE, the same loop at RW 0x7229DD): the x within the window plus the width of the text before
// the first character in view, then the last character boundary at or left of it (the secret text's widths in a secret entry)
int entryCharAt(GameWindow *window, EntryData *e, WindowMsgData packedMouse)
{
	int x = 0, y = 0, w = 0, h = 0;
	window->winGetScreenPosition(&x, &y);
	window->winGetSize(&w, &h);
	const int clickX = (int)(packedMouse & 0xFFFF) - x + entryWidth(window, e->text, e->scrollPos);
	const UnicodeString &shown = e->secretText ? e->sText : e->text;
	int pos = (int)shown.size();
	while (pos > 0 && clickX < entryWidth(window, shown, pos))
	{
		--pos;
	}
	return pos;
}

// P-128: Ennorath 1.9.8.3's pySAGE patch `text-entry-clipboard` (section .txtclip, hooked at RW 0x722A8A: the Ctrl branch of GWM_CHAR after
// Ctrl+Left / Right). Down with Ctrl and without Alt / AltGr (Shift does not matter): A selects all, C copies the selection, X copies it and on a
// successful copy deletes it as Del does, V pastes. A recognised key is handled even when it does nothing (an empty selection, a secret entry, no
// clipboard). Pasting sends each UTF-16 unit, up to 4096 or the first NUL, as GWM_IME_CHAR to the entry's own input, so validation, maxTextLen and
// the selection's replacement stay the entry's; tab, CR, LF and every surrogate unit (0xD800 .. 0xDFFF) are skipped.
bool textEntryClipboardKey(GameWindow *window, EntryData *e, int key, std::uint32_t state)
{
	if (!BitTest(state, KEY_STATE_DOWN) || !BitTest(state, KEY_STATE_CONTROL) || BitTest(state, KEY_STATE_ALT))
	{
		return false;
	}
	if (key != kKeyA && key != kKeyC && key != kKeyX && key != kKeyV)
	{
		return false;
	}
	if (key == kKeyA)
	{
		// the cursor to the end (RW 0x7220D3), the other end of the selection to the start
		GadgetTextEntrySetCursorPosition(window, (int)e->text.size());
		e->selectPos = 0;
		return true;
	}
	TextEntryClipboard *clipboard = GadgetTextEntryGetClipboard();
	if (!clipboard)
	{
		return true;
	}
	if (key == kKeyV)
	{
		UnicodeString pasted;
		if (!clipboard->read(pasted))
		{
			return true;
		}
		const std::size_t bound = std::min<std::size_t>(pasted.size(), 4096);
		for (std::size_t i = 0; i < bound; ++i)
		{
			const char16_t c = pasted[i];
			if (c == 0)
			{
				break;
			}
			if (c == 9 || c == 10 || c == 13 || (c >= 0xD800 && c <= 0xDFFF))
			{
				continue;
			}
			GadgetTextEntryInput(window, GWM_IME_CHAR, (WindowMsgData)c, 0);
		}
		return true;
	}
	// copy and cut: never out of a secret entry; the selection's ends ordered and clamped to the text
	if (e->secretText)
	{
		return true;
	}
	int start = (unsigned short)e->charPos, end = (unsigned short)e->selectPos;
	if (start > end)
	{
		std::swap(start, end);
	}
	start = std::min<int>(start, (int)e->text.size());
	end = std::min<int>(end, (int)e->text.size());
	if (end <= start)
	{
		return true;
	}
	if (clipboard->write(e->text.substr((std::size_t)start, (std::size_t)(end - start))) && key == kKeyX)
	{
		GadgetTextEntryInput(window, GWM_CHAR, KEY_DEL, KEY_STATE_DOWN);
	}
	return true;
}

// Home / End / Left / Right without Shift collapse the selection onto the cursor (RW 0x722D74, 0x722B77)
void collapseUnlessShift(EntryData *e, std::uint32_t state)
{
	if (!BitTest(state, KEY_STATE_SHIFT))
	{
		e->selectPos = e->charPos;
	}
}
} // namespace

WindowMsgHandledType GadgetTextEntryInput(GameWindow *window, std::uint32_t msg, WindowMsgData mData1, WindowMsgData mData2)
{
	// RW 0x722745 (BFME2 decomp GadgetTextEntryInput.cpp, 0x00320DA9; the RotWK bytes read the same way). Not ported (stop S-177): the IME guard at
	// the top and the composition reset of a click; the mouse-cursor shape the hover sets (RW 0x83EDE7 with 0x2C / 2) and its +0x14 byte
	EntryData *e = entryOf(window);
	WinInstanceData *instData = window->winGetInstanceData();
	GameWindowManager &mgr = window->manager();
	const std::uint32_t state = (std::uint32_t)mData2;
	const bool down = BitTest(state, KEY_STATE_DOWN);
	switch (msg)
	{
		case GWM_IME_CHAR:
		{
			const char16_t ch = (char16_t)mData1;
			if (ch == u'\r')
			{
				// RW 0x722984: drawTextFromStart cleared, then EDIT_DONE with 0
				e->drawTextFromStart = false;
				mgr.winSendSystemMsg(window->winGetOwner(), GEM_EDIT_DONE, msgData(window), 0);
				return MSG_HANDLED;
			}
			if (ch)
			{
				// lane CAH-2 r2: RotWK's insert (RW 0x72260B) and validator (RW 0x75E4DF) instead of ZH's "charPos < maxTextLen - 1", which kept an
				// entry one character short of its limit (the Create-a-Hero name: 21 of RW 0x9C3F2A's 22)
				if (GadgetTextEntryInsertCharacter(window, ch))
				{
					mgr.winSendSystemMsg(window->winGetOwner(), GEM_UPDATE_TEXT, msgData(window), 0);
				}
			}
			break;
		}
		case GWM_CHAR:
			// RW 0x722A4C: a key down with Ctrl or Alt: only the Ctrl keys are the entry's (Ctrl+Left / Right jump words, then the clipboard keys of
			// P-128); everything else is offered to the parents (ignored)
			if (down && BitTest(state, KEY_STATE_ALT | KEY_STATE_CONTROL))
			{
				if (!BitTest(state, KEY_STATE_CONTROL))
				{
					return MSG_IGNORED;
				}
				if (mData1 == KEY_RIGHT)
				{
					// RW 0x722A92: to the start of the next word (spaces, a word, spaces)
					if (e->conCharPos == 0)
					{
						const int len = (int)e->text.size();
						int pos = (unsigned short)e->charPos;
						if (pos < len)
						{
							while (pos < len && e->text[(std::size_t)pos] == u' ')
							{
								++pos;
							}
							while (pos < len && e->text[(std::size_t)pos] != u' ')
							{
								++pos;
							}
							while (pos < len && e->text[(std::size_t)pos] == u' ')
							{
								++pos;
							}
							GadgetTextEntrySetCursorPosition(window, pos);
						}
						collapseUnlessShift(e, state);
					}
					return MSG_HANDLED;
				}
				if (mData1 == KEY_LEFT)
				{
					// RW 0x722B11: to the start of this or the previous word
					if (e->conCharPos == 0)
					{
						int pos = (unsigned short)e->charPos;
						if (pos > 0)
						{
							pos = std::min<int>(pos, (int)e->text.size());
							while (pos > 0 && e->text[(std::size_t)pos - 1] == u' ')
							{
								--pos;
							}
							while (pos > 0 && e->text[(std::size_t)pos - 1] != u' ')
							{
								--pos;
							}
							GadgetTextEntrySetCursorPosition(window, pos);
						}
						collapseUnlessShift(e, state);
					}
					return MSG_HANDLED;
				}
				return textEntryClipboardKey(window, e, (int)mData1, state) ? MSG_HANDLED : MSG_IGNORED;
			}
			switch (mData1)
			{
				// RW 0x722B8F: these are not the entry's
				case KEY_ESC:
				case KEY_PGUP:
				case KEY_PGDN:
				case KEY_F1:
				case KEY_F2:
				case KEY_F3:
				case KEY_F4:
				case KEY_F5:
				case KEY_F6:
				case KEY_F7:
				case KEY_F8:
				case KEY_F9:
				case KEY_F10:
				case KEY_F11:
				case KEY_F12:
				case KEY_CAPS:
					return MSG_IGNORED;
				case KEY_ENTER:
				case KEY_KPENTER:
					if (BitTest(state, KEY_STATE_CONTROL))
					{
						return MSG_IGNORED;
					}
					break;
				case KEY_TAB:
					// RW 0x722BC7: Shift+Tab tabs backwards (to the combo box holding the entry, else from the entry)
					if (down)
					{
						GameWindow *parent = window->winGetParent();
						if (parent && !BitTest(parent->winGetStyle(), GWS_COMBO_BOX))
						{
							parent = nullptr;
						}
						if (BitTest(state, KEY_STATE_SHIFT))
						{
							mgr.winPrevTab(parent ? parent : window);
						}
						else
						{
							mgr.winNextTab(parent ? parent : window);
						}
					}
					break;
				case KEY_DEL:
					// RW 0x722CE2: an empty selection takes the character after the cursor
					if (down && e->conCharPos == 0)
					{
						if (e->selectPos == e->charPos)
						{
							e->selectPos = (short)(e->charPos + 1);
						}
						if (GadgetTextEntryUpdateComposition(window))
						{
							mgr.winSendSystemMsg(window->winGetOwner(), GEM_UPDATE_TEXT, msgData(window), 0);
						}
					}
					break;
				case KEY_BACKSPACE:
					// RW 0x722C3D: an empty selection takes the character before the cursor
					if (down && e->conCharPos == 0)
					{
						if ((unsigned short)e->selectPos > 0 && e->selectPos == e->charPos)
						{
							e->selectPos = (short)(e->charPos - 1);
						}
						if (GadgetTextEntryUpdateComposition(window))
						{
							mgr.winSendSystemMsg(window->winGetOwner(), GEM_UPDATE_TEXT, msgData(window), 0);
						}
					}
					break;
				case KEY_HOME:
					if (e->conCharPos == 0)
					{
						if (down)
						{
							GadgetTextEntrySetCursorPosition(window, 0);
						}
						collapseUnlessShift(e, state);
					}
					break;
				case KEY_END:
					if (e->conCharPos == 0)
					{
						if (down)
						{
							GadgetTextEntrySetCursorPosition(window, (int)e->text.size());
						}
						collapseUnlessShift(e, state);
					}
					break;
				case KEY_RIGHT:
					if (down && e->conCharPos == 0)
					{
						if ((unsigned short)e->charPos < e->text.size())
						{
							GadgetTextEntrySetCursorPosition(window, e->charPos + 1);
						}
						collapseUnlessShift(e, state);
					}
					break;
				case KEY_LEFT:
					if (down && e->conCharPos == 0)
					{
						if ((unsigned short)e->charPos > 0)
						{
							GadgetTextEntrySetCursorPosition(window, e->charPos - 1);
						}
						collapseUnlessShift(e, state);
					}
					break;
				default:
					break; // RW: Up, Down and the other keys are taken and do nothing (ZH's Up / Down tabbing is not RotWK's)
			}
			break;
		case GWM_LEFT_DOWN:
			// RW 0x722854: the focus, then the cursor and the other end of the selection at the clicked character; the entry captures the mouse
			mgr.winSetFocus(window);
			{
				const int pos = entryCharAt(window, e, mData1);
				e->selectPos = (short)pos;
				GadgetTextEntrySetCursorPosition(window, pos);
			}
			mgr.winCapture(window);
			break;
		case GWM_LEFT_UP:
			// RW 0x722827
			if (mgr.winGetCapture() == window)
			{
				mgr.winRelease(window);
			}
			break;
		case GWM_MOUSE_POS:
			// RW 0x7229C6: a drag while captured moves the cursor (the selection grows from the click)
			if (mgr.winGetCapture() != window)
			{
				return MSG_IGNORED;
			}
			GadgetTextEntrySetCursorPosition(window, entryCharAt(window, e, mData1));
			break;
		case GWM_MOUSE_ENTERING:
			if (BitTest(instData->getStyle(), GWS_MOUSE_TRACK))
			{
				BitSet(instData->m_state, WIN_STATE_HILITED);
				mgr.winSendSystemMsg(window->winGetOwner(), GBM_MOUSE_ENTERING, msgData(window), 0);
			}
			break;
		case GWM_MOUSE_LEAVING:
			if (BitTest(instData->getStyle(), GWS_MOUSE_TRACK))
			{
				BitClear(instData->m_state, WIN_STATE_HILITED);
				mgr.winSendSystemMsg(window->winGetOwner(), GBM_MOUSE_LEAVING, msgData(window), 0);
			}
			break;
		case GWM_LEFT_DRAG:
			if (BitTest(instData->getStyle(), GWS_MOUSE_TRACK))
			{
				mgr.winSendSystemMsg(window->winGetOwner(), GGM_LEFT_DRAG, msgData(window), 0);
			}
			break;
		default:
			return MSG_IGNORED;
	}
	return MSG_HANDLED;
}

WindowMsgHandledType GadgetTextEntrySystem(GameWindow *window, std::uint32_t msg, WindowMsgData mData1, WindowMsgData mData2)
{
	// RW 0x722191 (BFME2 decomp GadgetTextEntrySystem.cpp, 0x003207F5). Not ported: the IME attach (stop S-177); on losing the focus RotWK also sends
	// GEM_EDIT_DONE with 1 when drawTextFromStart is set (RW 0x7222C7): several of the port's screens act on EDIT_DONE without reading that 1
	// (stop S-7386)
	EntryData *e = entryOf(window);
	WinInstanceData *instData = window->winGetInstanceData();
	GameWindowManager &mgr = window->manager();
	switch (msg)
	{
		case GEM_GET_TEXT:
			*msgPtr<UnicodeString>(mData2) = e->text;
			break;
		case GEM_SET_TEXT:
		{
			// RW 0x7221E2: the cursor at the end when the entry has the focus, else at the start; no selection
			const UnicodeString &ustr = *msgPtr<const UnicodeString>(mData1);
			e->text = ustr;
			GadgetTextEntrySetCursorPosition(window, mgr.winGetFocus() == window ? (int)ustr.size() : 0);
			e->constructText.clear();
			e->selectPos = e->charPos;
			e->conCharPos = 0;
			e->sText.assign(ustr.size(), u'*');
			break;
		}
		case GWM_CREATE:
			break;
		case GWM_DESTROY:
			delete e;
			window->winSetUserData(nullptr);
			break;
		case GWM_INPUT_FOCUS:
			if (mData1 == 0)
			{
				// RW 0x7222B7: the cursor to the end, no selection
				GadgetTextEntrySetCursorPosition(window, (int)e->text.size());
				e->selectPos = e->charPos;
				BitClear(instData->m_state, WIN_STATE_SELECTED);
				BitClear(instData->m_state, WIN_STATE_HILITED);
				e->constructText.clear();
				e->conCharPos = 0;
			}
			else
			{
				// RW 0x722362: the cursor to the end and the whole text selected (typing replaces it)
				GadgetTextEntrySetCursorPosition(window, (int)e->text.size());
				e->selectPos = 0;
				BitSet(instData->m_state, WIN_STATE_SELECTED);
				BitSet(instData->m_state, WIN_STATE_HILITED);
				e->drawTextFromStart = false;
			}
			mgr.winSendSystemMsg(window->winGetOwner(), GGM_FOCUS_CHANGE, mData1, (WindowMsgData)window->winGetWindowId());
			*msgPtr<bool>(mData2) = true;
			break;
		default:
			return MSG_IGNORED;
	}
	return MSG_HANDLED;
}

UnicodeString GadgetTextEntryGetText(GameWindow *textentry)
{
	if (textentry == nullptr || !BitTest(textentry->winGetStyle(), GWS_ENTRY_FIELD))
	{
		return UnicodeString();
	}
	UnicodeString result;
	textentry->manager().winSendSystemMsg(textentry, GEM_GET_TEXT, 0, msgData(&result));
	return result;
}

void GadgetTextEntrySetText(GameWindow *g, const UnicodeString &text)
{
	if (g == nullptr)
	{
		return;
	}
	UnicodeString copy = text;
	g->manager().winSendSystemMsg(g, GEM_SET_TEXT, msgData(&copy), 0);
}

static Color darkenColor(Color color, int percent)
{
	// ZH GameDarkenColor
	if (percent >= 90 || percent <= 0)
	{
		return color;
	}
	int r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
	const int a = (color >> 24) & 0xFF;
	r -= (r * percent / 100);
	g -= (g * percent / 100);
	b -= (b * percent / 100);
	return GameMakeColor(r, g, b, a);
}

void GadgetTextEntrySetTextColor(GameWindow *g, Color color)
{
	const Color back = g->winGetEnabledTextBorderColor();
	g->winSetEnabledTextColors(color, back);
	g->winSetDisabledTextColors(darkenColor(color, 25), back);
}

// ---- draw (W3DTextEntry.cpp) -------------------------------------------------------------------------------------------------------------------

static void drawTextEntryText(GameWindow *window, WinInstanceData *instData, Color textColor, Color textDropColor, int x, int y, int width, int fontHeight)
{
	// RW 0x49F7BC (BFME2 decomp W3DTextEntry.cpp drawTextEntryText, RVA 0x9FE49, tier A; the RotWK bytes read the same way): the text scrolled to the
	// first character in view (+0x24), the part before the selection, the selection in black (the drop colour inverted) over a box in the text colour
	// while the entry (or its combo box) has the focus, the cursor blinking on timeGetTime & 0x200, the rest of the text. Each part is clipped to the
	// window. Not ported: the IME composition drawn at the cursor (stop S-177)
	EntryData *e = entryOf(window);
	GameWindowManager &mgr = window->manager();
	(void)instData;
	(void)width;
	if (textColor == WIN_COLOR_UNDEFINED)
	{
		return;
	}
	const UnicodeString &text = e->secretText ? e->sText : e->text;
	GameFont *font = window->winGetFont();
	ICoord2D origin, size;
	window->winGetScreenPosition(&origin.x, &origin.y);
	window->winGetSize(&size.x, &size.y);
	IRegion2D clipRegion;
	clipRegion.lo.x = origin.x;
	clipRegion.hi.x = origin.x + size.x;
	clipRegion.lo.y = origin.y;
	clipRegion.hi.y = origin.y + size.y;
	IRegion2D region = clipRegion;
	const int len = (int)text.size();
	auto widthOf = [&](int n) { return entryWidth(window, text, n); };

	x += 2 - widthOf(e->scrollPos);
	int cursorPos = x;
	const int charPos = std::min<int>((unsigned short)e->charPos, len);
	const int selectPos = std::min<int>((unsigned short)e->selectPos, len);
	const int selStart = std::min(selectPos, charPos);
	const int selEnd = std::max(selectPos, charPos);
	const int height = std::max(size.y - 4, fontHeight);

	GameWindow *parent = window->winGetParent();
	if (parent && !BitTest(parent->winGetStyle(), GWS_COMBO_BOX))
	{
		parent = nullptr;
	}
	const bool focused = window == mgr.winGetFocus() || (parent && parent == mgr.winGetFocus());

	if (selStart > 0)
	{
		const int w = widthOf(selStart);
		region.hi.x = std::min(clipRegion.hi.x, x + w);
		mgr.setClipRegion(region);
		mgr.winDrawText(text, font, x, y, textColor, textDropColor);
		x += w;
		cursorPos += w;
	}
	if (selEnd > selStart)
	{
		const int w = widthOf(selEnd) - widthOf(selStart);
		if (focused)
		{
			const int lo = std::max(x, origin.x);
			const int hi = std::min(x + w, origin.x + size.x);
			if (lo < hi)
			{
				mgr.enableClipping(false);
				mgr.winFillRect(textColor, WIN_DRAW_LINE_WIDTH, lo, origin.y + 2, hi, origin.y + height + 2);
			}
		}
		region.lo.x = std::max(clipRegion.lo.x, x);
		region.hi.x = std::min(clipRegion.hi.x, x + w);
		mgr.setClipRegion(region);
		// RW 0x49F774: the drop colour's RGB inverted, its alpha kept
		const Color inverted = (Color)((textDropColor & 0xFF000000u) | (~textDropColor & 0x00FFFFFFu));
		mgr.winDrawText(text, font, x - widthOf(selStart), y, (Color)0xFF000000u, inverted);
		x += w;
		if (selEnd == charPos)
		{
			cursorPos += w;
		}
	}
	mgr.enableClipping(false);
	if (focused && (mgr.timeMs() & 0x200))
	{
		mgr.winFillRect(textColor, WIN_DRAW_LINE_WIDTH, cursorPos, origin.y + 2, cursorPos + 3, origin.y + height + 2);
	}
	window->winSetCursorPosition(cursorPos + 2 - origin.x, 0);
	if (selEnd < len)
	{
		region.lo.x = std::max(clipRegion.lo.x, x);
		region.hi.x = clipRegion.hi.x;
		mgr.setClipRegion(region);
		mgr.winDrawText(text, font, x - widthOf(selEnd), y, textColor, textDropColor);
		mgr.enableClipping(false);
	}
}

static void drawTextEntryBody(GameWindow *window, WinInstanceData *instData, Color textColor, Color textBorder)
{
	GameWindowManager &mgr = window->manager();
	ICoord2D origin, size;
	window->winGetScreenPosition(&origin.x, &origin.y);
	window->winGetSize(&size.x, &size.y);
	const int fontHeight = mgr.winFontHeight(instData->getFont());
	const int startOffset = 5;
	const int width = size.x - (2 * startOffset);
	const int startX = origin.x + startOffset;
	int startY;
	if (BitTest(window->winGetStatus(), WIN_STATUS_ONE_LINE))
	{
		startY = size.y / 2 - fontHeight / 2; // ZH: no origin.y added (kept)
	}
	else
	{
		startY = origin.y + startOffset;
	}
	drawTextEntryText(window, instData, textColor, textBorder, startX, startY, width, fontHeight);
}

void W3DGadgetTextEntryDraw(GameWindow *window, WinInstanceData *instData)
{
	EntryData *e = entryOf(window);
	e->receivedUnichar = false;
	GameWindowManager &mgr = window->manager();
	ICoord2D origin, size;
	window->winGetScreenPosition(&origin.x, &origin.y);
	window->winGetSize(&size.x, &size.y);
	Color backBorder, backColor, textColor, textBorder;
	if (!BitTest(window->winGetStatus(), WIN_STATUS_ENABLED))
	{
		textColor = window->winGetDisabledTextColor();
		textBorder = window->winGetDisabledTextBorderColor();
		backColor = window->winGetDisabledColor(0);
		backBorder = window->winGetDisabledBorderColor(0);
	}
	else if (BitTest(instData->getState(), WIN_STATE_HILITED))
	{
		textColor = window->winGetHiliteTextColor();
		textBorder = window->winGetHiliteTextBorderColor();
		backColor = window->winGetHiliteColor(0);
		backBorder = window->winGetHiliteBorderColor(0);
	}
	else
	{
		textColor = window->winGetEnabledTextColor();
		textBorder = window->winGetEnabledTextBorderColor();
		backColor = window->winGetEnabledColor(0);
		backBorder = window->winGetEnabledBorderColor(0);
	}
	if (backBorder != WIN_COLOR_UNDEFINED)
	{
		mgr.winOpenRect(backBorder, WIN_DRAW_LINE_WIDTH, origin.x, origin.y, origin.x + size.x, origin.y + size.y);
	}
	if (backColor != WIN_COLOR_UNDEFINED)
	{
		mgr.winFillRect(backColor, WIN_DRAW_LINE_WIDTH, origin.x + 1, origin.y + 1, origin.x + 1 + size.x - 2, origin.y + 1 + size.y - 2);
	}
	drawTextEntryBody(window, instData, textColor, textBorder);
}

void W3DGadgetTextEntryImageDraw(GameWindow *window, WinInstanceData *instData)
{
	EntryData *e = entryOf(window);
	e->receivedUnichar = false;
	GameWindowManager &mgr = window->manager();
	ICoord2D origin, size, start, end;
	window->winGetScreenPosition(&origin.x, &origin.y);
	window->winGetSize(&size.x, &size.y);
	const int xOffset = instData->m_imageOffset.x;
	const int yOffset = instData->m_imageOffset.y;
	Color textColor, textBorder;
	const std::string *leftImage, *rightImage, *centerImage, *smallCenterImage;
	if (!BitTest(window->winGetStatus(), WIN_STATUS_ENABLED))
	{
		textColor = window->winGetDisabledTextColor();
		textBorder = window->winGetDisabledTextBorderColor();
		leftImage = &window->winGetDisabledImage(0);
		rightImage = &window->winGetDisabledImage(1);
		centerImage = &window->winGetDisabledImage(2);
		smallCenterImage = &window->winGetDisabledImage(3);
	}
	else if (BitTest(instData->getState(), WIN_STATE_HILITED))
	{
		textColor = window->winGetHiliteTextColor();
		textBorder = window->winGetHiliteTextBorderColor();
		leftImage = &window->winGetHiliteImage(0);
		rightImage = &window->winGetHiliteImage(1);
		centerImage = &window->winGetHiliteImage(2);
		smallCenterImage = &window->winGetHiliteImage(3);
	}
	else
	{
		textColor = window->winGetEnabledTextColor();
		textBorder = window->winGetEnabledTextBorderColor();
		leftImage = &window->winGetEnabledImage(0);
		rightImage = &window->winGetEnabledImage(1);
		centerImage = &window->winGetEnabledImage(2);
		smallCenterImage = &window->winGetEnabledImage(3);
	}
	int lw, lh, rw, rh, cw, ch, sw, sh;
	const bool haveImages = !leftImage->empty() && !rightImage->empty() && !centerImage->empty() && !smallCenterImage->empty() && mgr.imageSize(*leftImage, lw, lh) && mgr.imageSize(*rightImage, rw, rh) && mgr.imageSize(*centerImage, cw, ch) && mgr.imageSize(*smallCenterImage, sw, sh);
	if (haveImages && cw > 0 && sw > 0)
	{
		ICoord2D leftEnd, rightStart;
		leftEnd.x = origin.x + lw + xOffset;
		leftEnd.y = origin.y + size.y + yOffset;
		rightStart.x = origin.x + size.x - rw + xOffset;
		rightStart.y = origin.y + yOffset;
		int centerWidth = rightStart.x - leftEnd.x;
		int pieces = centerWidth / cw;
		start.x = leftEnd.x;
		start.y = origin.y + yOffset;
		end.y = start.y + size.y;
		for (int i = 0; i < pieces; ++i)
		{
			end.x = start.x + cw;
			mgr.winDrawImage(*centerImage, start.x, start.y, end.x, end.y);
			start.x += cw;
		}
		centerWidth = rightStart.x - start.x;
		pieces = centerWidth / sw + 1;
		end.y = start.y + size.y;
		for (int i = 0; i < pieces; ++i)
		{
			end.x = start.x + sw;
			mgr.winDrawImage(*smallCenterImage, start.x, start.y, end.x, end.y);
			start.x += sw;
		}
		start.x = origin.x + xOffset;
		start.y = origin.y + yOffset;
		end = leftEnd;
		mgr.winDrawImage(*leftImage, start.x, start.y, end.x, end.y);
		start = rightStart;
		end.x = start.x + rw;
		end.y = start.y + size.y;
		mgr.winDrawImage(*rightImage, start.x, start.y, end.x, end.y);
	}
	drawTextEntryBody(window, instData, textColor, textBorder);
}
