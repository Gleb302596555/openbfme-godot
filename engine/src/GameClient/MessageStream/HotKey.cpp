// OpenBFME. GPL-3.0.
// See HotKey.h.

#include "GameClient/MessageStream/HotKey.h"

#include "GameClient/MessageStream/MetaEvent.h"

#include <cwctype>

char32_t HotKeyTranslator::hotkeyOf(const std::u16string &text)
{
	// RW 0x75A67F: the first '&' gives the character after it (an '&' at the end gives none)
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == u'&')
		{
			return i + 1 < text.size() ? (char32_t)text[i + 1] : 0;
		}
	}
	return 0;
}

char32_t HotKeyTranslator::usCharOf(int scanCode)
{
	static const struct
	{
		int code;
		char c;
	} kChars[] = { { KEY_A, 'a' }, { KEY_B, 'b' }, { KEY_C, 'c' }, { KEY_D, 'd' }, { KEY_E, 'e' }, { KEY_F, 'f' }, { KEY_G, 'g' }, { KEY_H, 'h' }, { KEY_I, 'i' },
		{ KEY_J, 'j' }, { KEY_K, 'k' }, { KEY_L, 'l' }, { KEY_M, 'm' }, { KEY_N, 'n' }, { KEY_O, 'o' }, { KEY_P, 'p' }, { KEY_Q, 'q' }, { KEY_R, 'r' }, { KEY_S, 's' },
		{ KEY_T, 't' }, { KEY_U, 'u' }, { KEY_V, 'v' }, { KEY_W, 'w' }, { KEY_X, 'x' }, { KEY_Y, 'y' }, { KEY_Z, 'z' }, { KEY_1, '1' }, { KEY_2, '2' }, { KEY_3, '3' },
		{ KEY_4, '4' }, { KEY_5, '5' }, { KEY_6, '6' }, { KEY_7, '7' }, { KEY_8, '8' }, { KEY_9, '9' }, { KEY_0, '0' } };
	for (const auto &k : kChars)
	{
		if (k.code == scanCode)
		{
			return (char32_t)k.c;
		}
	}
	return 0;
}

char32_t HotKeyTranslator::cyrillicCharOf(int scanCode, bool ukrainian)
{
	static const struct
	{
		int code;
		char16_t ru;
		char16_t uk;
	} kChars[] = { { KEY_Q, u'\u0439', u'\u0439' }, { KEY_W, u'\u0446', u'\u0446' }, { KEY_E, u'\u0443', u'\u0443' }, { KEY_R, u'\u043A', u'\u043A' }, { KEY_T, u'\u0435', u'\u0435' }, { KEY_Y, u'\u043D', u'\u043D' },
		{ KEY_U, u'\u0433', u'\u0433' }, { KEY_I, u'\u0448', u'\u0448' }, { KEY_O, u'\u0449', u'\u0449' }, { KEY_P, u'\u0437', u'\u0437' }, { KEY_LBRACKET, u'\u0445', u'\u0445' }, { KEY_RBRACKET, u'\u044A', u'\u0457' },
		{ KEY_A, u'\u0444', u'\u0444' }, { KEY_S, u'\u044B', u'\u0456' }, { KEY_D, u'\u0432', u'\u0432' }, { KEY_F, u'\u0430', u'\u0430' }, { KEY_G, u'\u043F', u'\u043F' }, { KEY_H, u'\u0440', u'\u0440' },
		{ KEY_J, u'\u043E', u'\u043E' }, { KEY_K, u'\u043B', u'\u043B' }, { KEY_L, u'\u0434', u'\u0434' }, { KEY_SEMICOLON, u'\u0436', u'\u0436' }, { KEY_APOSTROPHE, u'\u044D', u'\u0454' },
		{ KEY_Z, u'\u044F', u'\u044F' }, { KEY_X, u'\u0447', u'\u0447' }, { KEY_C, u'\u0441', u'\u0441' }, { KEY_V, u'\u043C', u'\u043C' }, { KEY_B, u'\u0438', u'\u0438' }, { KEY_N, u'\u0442', u'\u0442' },
		{ KEY_M, u'\u044C', u'\u044C' }, { KEY_COMMA, u'\u0431', u'\u0431' }, { KEY_PERIOD, u'\u044E', u'\u044E' }, { KEY_TICK, u'\u0451', 0 } };
	for (const auto &k : kChars)
	{
		if (k.code == scanCode)
		{
			return (char32_t)(ukrainian ? k.uk : k.ru);
		}
	}
	return 0;
}

namespace
{
// the C locale's towlower / towupper leave Cyrillic alone: U+0430..U+044F, U+0450..U+045F <-> U+0410..U+042F, U+0400..U+040F
char32_t cyrillicLower(char32_t c)
{
	return c >= 0x410 && c <= 0x42F ? c + 0x20 : c >= 0x400 && c <= 0x40F ? c + 0x50 : c;
}
char32_t cyrillicUpper(char32_t c)
{
	return c >= 0x430 && c <= 0x44F ? c - 0x20 : c >= 0x450 && c <= 0x45F ? c - 0x50 : c;
}
} // namespace

const HotKeyTranslator::Entry *HotKeyTranslator::findAnyCase(char32_t ch, bool &disabledSeen) const
{
	const char32_t lower = cyrillicLower((char32_t)std::towlower((wint_t)ch)), upper = cyrillicUpper((char32_t)std::towupper((wint_t)ch));
	const Entry *e = find(lower, disabledSeen);
	if (!e && upper != lower)
	{
		e = find(upper, disabledSeen);
	}
	return e;
}

const HotKeyTranslator::Entry *HotKeyTranslator::find(char32_t ch, bool &disabledSeen) const
{
	// the manager's maps hold one action per key: the first registration of the key stands for it (INFERENCE: the registration order is the command
	// bar's window order)
	for (const Entry &e : m_entries)
	{
		if (e.key != ch)
		{
			continue;
		}
		if (e.availability == Availability::Hidden)
		{
			return nullptr; // vslot 4: passed over
		}
		if (e.availability == Availability::Disabled)
		{
			disabledSeen = true;
			return nullptr;
		}
		return &e;
	}
	return nullptr;
}

MessageDisposition HotKeyTranslator::translate(const ClientMessage &msg)
{
	if (msg.type() != CMSG_RAW_KEY_UP)
	{
		return MessageDisposition::Keep;
	}
	// RW 0x75B0E1: Shift alone or no modifier; Ctrl or Alt refuse the key
	const int state = msg.arg(1).integer;
	if ((state & KEY_STATE_CONTROL) || (state & KEY_STATE_ALT))
	{
		return MessageDisposition::Keep;
	}
	if (!m_ctx.ui.getInputEnabled() || m_entries.empty())
	{
		return MessageDisposition::Keep;
	}
	const int scanCode = msg.arg(0).integer;
	char32_t ch = msg.argumentCount() > 2 ? (char32_t)msg.arg(2).integer : 0;
	if (ch == 0)
	{
		ch = usCharOf(scanCode);
	}
	bool disabled = false;
	const Entry *e = ch != 0 ? findAnyCase(ch, disabled) : nullptr;
	// NOT RETAIL (see the header): the key's character in the other layouts, when the active layout's finds nothing (a disabled match stands)
	if (!e && !disabled)
	{
		for (const char32_t other : { usCharOf(scanCode), cyrillicCharOf(scanCode, false), cyrillicCharOf(scanCode, true) })
		{
			if (other != 0 && other != ch && (e = findAnyCase(other, disabled)) != nullptr)
			{
				break;
			}
			if (disabled)
			{
				break;
			}
		}
	}
	if (ch == 0 && !e && !disabled)
	{
		return MessageDisposition::Keep;
	}
	if (!e)
	{
		++m_outcomes[disabled ? "disabled" : "none"]; // RW 0x75A5BC: DisabledHotKeyPressed (not played)
		return MessageDisposition::Keep;
	}
	if (e->run ? e->run() : (m_press && m_press(e->slot, e->inPalantir)))
	{
		++m_outcomes["enabled"]; // RW 0x75A544: EnabledHotKeyPressed (not played)
	}
	else
	{
		++m_outcomes["press refused"];
	}
	return MessageDisposition::Destroy;
}
